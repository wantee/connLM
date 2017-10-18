/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Wang Jian
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <string.h>
#include <assert.h>

#include <stutils/st_macro.h>
#include <stutils/st_log.h>
#include <stutils/st_mem.h>

#include "component_updater.h"

void comp_updater_destroy(comp_updater_t *comp_updater)
{
    int i;

    if (comp_updater == NULL) {
        return;
    }

    if (comp_updater->layer_updaters != NULL) {
        for (i = 2; i < comp_updater->comp->num_layer; i++) {
            safe_layer_updater_destroy(comp_updater->layer_updaters[i]);
        }
        safe_st_free(comp_updater->layer_updaters);
    }

    if (comp_updater->glue_updaters != NULL) {
        for (i = 0; i < comp_updater->comp->num_glue; i++) {
            safe_glue_updater_destroy(comp_updater->glue_updaters[i]);
        }
        safe_st_free(comp_updater->glue_updaters);
    }

    if (comp_updater->bptt_updaters != NULL) {
        for (i = 0; i < comp_updater->comp->num_glue_cycle; i++) {
            safe_bptt_updater_destroy(comp_updater->bptt_updaters[i]);
        }
        safe_st_free(comp_updater->bptt_updaters);
    }

    mat_destroy(&comp_updater->bptt_er0);
    mat_destroy(&comp_updater->bptt_er1);

    comp_updater->comp = NULL;
    comp_updater->out_updater = NULL;
}

comp_updater_t* comp_updater_create(component_t *comp,
        out_updater_t *out_updater)
{
    comp_updater_t *comp_updater = NULL;

    size_t sz;
    int i;

    ST_CHECK_PARAM(comp == NULL, NULL);

    comp_updater = (comp_updater_t *)st_malloc(sizeof(comp_updater_t));
    if (comp_updater == NULL) {
        ST_WARNING("Failed to st_malloc comp_updater.");
        goto ERR;
    }
    memset(comp_updater, 0, sizeof(comp_updater_t));

    comp_updater->comp = comp;
    comp_updater->out_updater = out_updater;

    if (comp->num_layer > 0) {
        sz = sizeof(layer_updater_t*)*comp->num_layer;
        comp_updater->layer_updaters = (layer_updater_t **)st_malloc(sz);
        if (comp_updater->layer_updaters == NULL) {
            ST_WARNING("Failed to st_malloc layer_updaters.");
            goto ERR;
        }
        memset(comp_updater->layer_updaters, 0, sz);

        for (i = 2; i < comp->num_layer; i++) {
            comp_updater->layer_updaters[i] = layer_updater_create(comp->layers[i]);
            if (comp_updater->layer_updaters[i] == NULL) {
                ST_WARNING("Failed to layer_updater_create[%s].",
                        comp->layers[i]->name);
                goto ERR;
            }
        }
    }

    if (comp->num_glue > 0) {
        sz = sizeof(glue_updater_t*)*comp->num_glue;
        comp_updater->glue_updaters = (glue_updater_t **)st_malloc(sz);
        if (comp_updater->glue_updaters == NULL) {
            ST_WARNING("Failed to st_malloc glue_updaters.");
            goto ERR;
        }

        for (i = 0; i < comp->num_glue; i++) {
            comp_updater->glue_updaters[i] = glue_updater_create(comp->glues[i]);
            if (comp_updater->glue_updaters[i] == NULL) {
                ST_WARNING("Failed to glue_updater_create[%s].",
                        comp->glues[i]->name);
                goto ERR;
            }
        }
    }
    return comp_updater;

ERR:
    safe_comp_updater_destroy(comp_updater);
    return NULL;
}

int comp_updater_set_rand_seed(comp_updater_t *comp_updater, unsigned int *seed)
{
    int g;

    ST_CHECK_PARAM(comp_updater == NULL, -1);

    comp_updater->rand_seed = seed;

    for (g = 0; g < comp_updater->comp->num_glue; g++) {
        if (glue_updater_set_rand_seed(comp_updater->glue_updaters[g],
                    seed) < 0) {
            ST_WARNING("Failed to glue_updater_set_rand_seed for glue[%s].",
                    comp_updater->glue_updaters[g]->glue->name);
            return -1;
        }
    }

    return 0;
}

static int comp_updater_setup_dropout(comp_updater_t *comp_updater)
{
    component_t *comp;
    glue_t *glue;
    glue_updater_t *head, *glue_updater;
    int i, j, g, u;

    ST_CHECK_PARAM(comp_updater == NULL, -1);

    comp = comp_updater->comp;

    for (i = 0; i < comp->num_glue_cycle; i++) {
        g = comp->glue_cycles[i][1];
        glue = comp->glues[g];
        head = comp_updater->glue_updaters[g];

        // following check is necessary for the glues that may be
        // contained in multiple cycles
        if (head->keep_mask.num_rows > 0) {
            // set effective learning rate
            for (u = 0; u < head->num_wt_updaters; u++) {
                head->wt_updaters[u]->param.learn_rate *= (1 - glue->param.dropout);
            }
        }

        if (glue_updater_setup_dropout(head, glue->param.dropout) < 0) {
            ST_WARNING("Failed to glue_updater_setup_dropout[%s] of recur_head.",
                    glue->name);
            return -1;
        }

        for (j = 2; j <= comp->glue_cycles[i][0]; j++) {
            g = comp->glue_cycles[i][j];
            glue_updater = comp_updater->glue_updaters[g];

            // following check is necessary for the glues that may be
            // contained in multiple cycles
            if (glue_updater->keep_mask.num_rows > 0) {
                // set effective learning rate
                for (u = 0; u < glue_updater->num_wt_updaters; u++) {
                    glue_updater->wt_updaters[u]->param.learn_rate *= (1 - glue->param.dropout);
                }
            }

            if (glue_updater_setup_dropout(glue_updater, glue->param.dropout) < 0) {
                ST_WARNING("Failed to glue_updater_setup_dropout[%s] of recur_body.",
                        glue->name);
                return -1;
            }
        }
    }

    for (i = 0; i < comp->num_glue; i++) {
        glue = comp->glues[i];
        glue_updater = comp_updater->glue_updaters[i];
        if (glue->recur_type == RECUR_NON && glue->param.dropout > 0.0) {
            if (glue_updater_setup_dropout(glue_updater, glue->param.dropout) < 0) {
                ST_WARNING("Failed to glue_updater_setup_dropout[%s] of recur_non.",
                        glue->name);
                return -1;
            }
            // set effective learning rate
            for (u = 0; u < glue_updater->num_wt_updaters; u++) {
                glue_updater->wt_updaters[u]->param.learn_rate *= (1 - glue->param.dropout);
            }
        }
    }

    return 0;
}

int comp_updater_setup(comp_updater_t *comp_updater, bool backprop)
{
    component_t *comp;
    layer_updater_t **layer_updaters;
    int i;

    ST_CHECK_PARAM(comp_updater == NULL, -1);

    comp = comp_updater->comp;
    layer_updaters = comp_updater->layer_updaters;

    for (i = 2; i < comp->num_layer; i++) {
        if (layer_updater_setup(layer_updaters[i], backprop) < 0) {
            ST_WARNING("Failed to layer_updater_setup[%s].",
                    comp->layers[i]->name);
            goto ERR;
        }
    }

    for (i = 0; i < comp->num_glue; i++) {
        if (glue_updater_setup(comp_updater->glue_updaters[i],
                    comp_updater, backprop) < 0) {
            ST_WARNING("Failed to glue_updater_setup[%s].",
                    comp->glues[i]->name);
            goto ERR;
        }
    }

    if (backprop) {
        if (comp->num_glue_cycle > 0) {
            if (! comp_check_glue_cycles(comp)) {
                ST_WARNING("Failed to comp_check_glue_cycles.");
                goto ERR;
            }

            comp_updater->bptt_updaters = (bptt_updater_t **)st_malloc(
                    sizeof(bptt_updater_t*) * comp->num_glue_cycle);
            if (comp_updater->bptt_updaters == NULL) {
                ST_WARNING("Failed to st_malloc bptt_updaters.");
                goto ERR;
            }
            for (i = 0; i < comp->num_glue_cycle; i++) {
                comp_updater->bptt_updaters[i] = bptt_updater_create(comp, i,
                        comp_updater->glue_updaters);
                if (comp_updater->bptt_updaters[i] == NULL) {
                    ST_WARNING("Failed to bptt_updater_create.[%d][%s]", i,
                            comp->glues[comp->glue_cycles[i][1]]->name);
                    goto ERR;
                }
            }
        }

        if (comp_updater_setup_dropout(comp_updater) < 0) {
            ST_WARNING("Failed to comp_updater_setup_dropout.");
            goto ERR;
        }
    }

    return 0;

ERR:
    if (comp_updater->bptt_updaters != NULL) {
        for (i = 0; i < comp_updater->comp->num_glue_cycle; i++) {
            safe_bptt_updater_destroy(comp_updater->bptt_updaters[i]);
        }
        safe_st_free(comp_updater->bptt_updaters);
    }

    return -1;
}

int comp_updater_setup_pre_ac_state(comp_updater_t *comp_updater)
{
    component_t *comp;
    int i;

    ST_CHECK_PARAM(comp_updater == NULL, -1);

    comp = comp_updater->comp;

    for (i = 0; i < comp->num_glue; i++) {
        if (glue_updater_setup_pre_ac_state(
                    comp_updater->glue_updaters[i], comp_updater) < 0) {
            ST_WARNING("Failed to glue_updater_setup_pre_ac_state[%s].",
                    comp->glues[i]->name);
            return -1;
        }
    }

    return 0;
}

static int comp_updater_bptt(comp_updater_t *comp_updater, bool clear)
{
    component_t *comp;
    layer_updater_t **layer_updaters;
    layer_updater_t *out_layer_updater;
    wt_updater_t *wt_updater;
    layer_t *layer;
    glue_t *glue;
    bptt_updater_t *bptt_updater;
    real_t keep_prob;

    mat_t *in_er = NULL;
    mat_t *out_er = NULL;
    mat_t *tmp;
    mat_t in_ac = {0};
    mat_t out_ac = {0};
    mat_t sub_er = {0};
    int bptt, bptt_delay, batch_size;
    int i, j, g, t, er_t;

    ST_CHECK_PARAM(comp_updater == NULL, -1);

    comp = comp_updater->comp;
    layer_updaters = comp_updater->layer_updaters;

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Backprop: BPTT comp[%s](clear[%s])", comp->name,
            bool2str(clear));
#endif

    g = comp->glue_cycles[0][1];
    glue = comp->glues[g];
    batch_size = comp_updater->batch_size;

    if (comp_updater->bptt_step == 0) {
        for (i = 0; i < comp->num_glue_cycle; i++) {
            for (j = 1; j <= comp->glue_cycles[i][0]; j++) {
                g = comp->glue_cycles[i][j];
                if (glue_updater_gen_keep_mask(comp_updater->glue_updaters[g],
                            batch_size) < 0) {
                    ST_WARNING("Failed to glue_updater_gen_keep_mask.");
                    return -1;
                }
            }
        }
    }
    if (!clear) {
        comp_updater->bptt_step += 1;
    }

    for (i = 0; i < comp->num_glue_cycle; i++) {
        g = comp->glue_cycles[i][1];
        bptt = comp->glues[g]->bptt_opt.bptt;
        bptt_delay = comp->glues[g]->bptt_opt.bptt_delay;
        bptt_updater = comp_updater->bptt_updaters[i];

        if (clear) {
            if (comp_updater->bptt_step > 0
                    && (comp_updater->bptt_step % bptt_delay == 0)) {
                // we already do BPTT in last call to this function, so skip
                continue;
            }
        } else {
            // clear == true must only called without step forward
            // store the ac and er for bptt.
            for (j = 1; j <= comp->glue_cycles[i][0]; j++) {
                g = comp->glue_cycles[i][j];
                glue = comp->glues[g];
                keep_prob = comp_updater->glue_updaters[g]->keep_prob;

                if (keep_prob < 1.0) {
                    // dropout_val should be filled during forward pass
                    mat_assign(&in_ac, &comp_updater->glue_updaters[g]->dropout_val);
                } else {
                    if (j == 1) {
                        if (mat_submat(&layer_updaters[glue->in_layer]->ac_state,
                                    0, 0, glue->in_offset, 0, &in_ac) < 0) {
                            ST_WARNING("Failed to mat_submat in_ac.");
                            return -1;
                        }
                    } else {
                        if (mat_submat(&layer_updaters[glue->in_layer]->ac,
                                    0, 0, glue->in_offset, 0, &in_ac) < 0) {
                            ST_WARNING("Failed to mat_submat in_ac.");
                            return -1;
                        }
                    }
                }

                layer = layer_updaters[glue->in_layer]->layer;
                if (mat_resize(bptt_updater->ac_bptts + j,
                            batch_size * (bptt + bptt_delay),
                            layer->size - glue->in_offset, NAN) < 0) {
                    ST_WARNING("Failed to mat_resize ac_bptts");
                    return -1;
                }

                layer = layer_updaters[glue->out_layer]->layer;
                if (mat_resize(bptt_updater->er_bptts + j,
                            batch_size * (bptt + bptt_delay),
                            layer->size - glue->out_offset, NAN) < 0) {
                    ST_WARNING("Failed to mat_resize er_bptts");
                    return -1;
                }

                if (bptt_updater->num_ac_bptts >= bptt + bptt_delay) {
                    if (mat_move_up(bptt_updater->ac_bptts + j,
                                0, batch_size) < 0) {
                        ST_WARNING("Failed to mat_move_up for ac_bptts.");
                        return -1;
                    }

                    if (mat_fill(bptt_updater->ac_bptts + j,
                                (bptt_updater->num_ac_bptts - 1) * batch_size,
                                batch_size, 0, 0, &in_ac) < 0) {
                        ST_WARNING("Failed to mat_fill for ac_bptts.");
                        return -1;
                    }
                } else {
                    if (mat_fill(bptt_updater->ac_bptts + j,
                                bptt_updater->num_ac_bptts * batch_size,
                                batch_size, 0, 0, &in_ac) < 0) {
                        ST_WARNING("Failed to mat_fill for ac_bptts.");
                        return -1;
                    }
                }

                if (bptt_updater->num_er_bptts >= bptt_delay) {
                    if (mat_move_up(bptt_updater->er_bptts + j,
                                0, batch_size) < 0) {
                        ST_WARNING("Failed to mat_move_up for er_bptts.");
                        return -1;
                    }

                    if (mat_fill(bptt_updater->er_bptts + j,
                                (bptt_updater->num_er_bptts - 1) * batch_size,
                                batch_size, 0, 0,
                                &layer_updaters[glue->out_layer]->er_raw) < 0) {
                        ST_WARNING("Failed to mat_fill for er_bptts.");
                        return -1;
                    }
                } else {
                    if (mat_fill(bptt_updater->er_bptts + j,
                                bptt_updater->num_er_bptts * batch_size,
                                batch_size, 0, 0,
                                &layer_updaters[glue->out_layer]->er_raw) < 0) {
                        ST_WARNING("Failed to mat_fill for er_bptts.");
                        return -1;
                    }
                }
            }
            if (bptt_updater->num_ac_bptts < bptt + bptt_delay) {
                bptt_updater->num_ac_bptts++;
            }
            if (bptt_updater->num_er_bptts < bptt_delay) {
                bptt_updater->num_er_bptts++;
            }
        }

        // we can not use in_ or out_layer_updater->er as
        // buffer, since it could be the case that
        // glue->in_layer == glue->out_layer, then
        // in_er and out_er would be the same.
        //
        // we directly assign bptt_er0/1, so that they can do resize
        // we will assign them back in order to properly used then outside
        in_er = &comp_updater->bptt_er0;
        out_er = &comp_updater->bptt_er1;

        if (comp_updater->bptt_step % bptt_delay == 0 || clear) {
            // backprop through time
            for (t = bptt_updater->num_ac_bptts - 1; t >= 0; t--) {
                for (j = 1; j <= comp->glue_cycles[i][0]; j++) {
                    g = comp->glue_cycles[i][j];
                    glue = comp->glues[g];
                    keep_prob = comp_updater->glue_updaters[g]->keep_prob;

                    if (t == bptt_updater->num_ac_bptts - 1 && j == 1) {
                        // using values in current timestep
                        if (mat_submat(&layer_updaters[glue->out_layer]->ac,
                                    0, 0, glue->out_offset, 0, &out_ac) < 0) {
                            ST_WARNING("Failed to mat_submat out_ac.");
                            return -1;
                        }

                        layer = layer_updaters[glue->out_layer]->layer;
                        if (mat_resize(out_er, batch_size,
                                    layer->size - glue->out_offset, NAN) < 0) {
                            ST_WARNING("Failed to mat_resize out_er");
                            return -1;
                        }
                        mat_set(out_er, 0.0);
                    } else {
                        // the glues in a cycle is joined one-by-one,
                        // the output of one glue is the input of the prev glue
                        mat_assign(&out_ac, &in_ac);

                        // exchange in_er and out_er
                        tmp = out_er;
                        out_er = in_er;
                        in_er = tmp;
                    }
                    if (mat_submat(bptt_updater->ac_bptts + j,
                                t * batch_size, batch_size,
                                0, 0, &in_ac) < 0) {
                        ST_WARNING("Failed to mat_submat in_ac.");
                        return -1;
                    }

                    er_t = t - bptt_updater->num_ac_bptts
                             + bptt_updater->num_er_bptts;
                    if (er_t >= 0) {
#ifdef _CONNLM_TRACE_PROCEDURE_
                        ST_TRACE("Backprop: BPTT timestep(delayed)[%d], "
                                "glue[%s]", t, comp->glues[g]->name);
#endif
                        if (mat_submat(bptt_updater->er_bptts + j,
                                    er_t * batch_size, batch_size,
                                    0, 0, &sub_er) < 0) {
                            ST_WARNING("Failed to mat_submat er_bptts.");
                            return -1;
                        }
                        if (mat_add_elems(out_er, 1.0, &sub_er, 1.0, out_er) < 0) {
                            ST_WARNING("Failed to mat_add_elems for out_er");
                            return -1;
                        }
                    } else {
#ifdef _CONNLM_TRACE_PROCEDURE_
                        ST_TRACE("Backprop: BPTT timestep[%d], glue[%s]",
                                t, comp->glues[g]->name);
#endif
                    }

                    // deriv
                    out_layer_updater = layer_updaters[glue->out_layer];
                    assert (out_layer_updater->deriv != NULL);
                    if (out_layer_updater->deriv(out_layer_updater->layer,
                                out_er, &out_ac) < 0) {
                        ST_WARNING("Failed to deriv.[%s]",
                                out_layer_updater->layer->name);
                        return -1;
                    }

                    wt_updater = bptt_updater->wt_updaters[j];
                    // propagate error to next glue
                    if (j < comp->glue_cycles[i][0] || t - 1 >= 0) {
                        layer = layer_updaters[glue->in_layer]->layer;
                        if (mat_resize(in_er, batch_size,
                                    layer->size - glue->in_offset, NAN) < 0) {
                            ST_WARNING("Failed to mat_resize in_er");
                            return -1;
                        }
                        mat_set(in_er, 0.0);

                        if (propagate_error(&wt_updater->wt, out_er, 1.0,
                                    wt_updater->param.er_cutoff, in_er) < 0) {
                            ST_WARNING("Failed to propagate_error.");
                            return -1;
                        }

                        // dropout in_er
                        if (keep_prob < 1.0) {
                            if (mat_mul_elems(in_er,
                                    &comp_updater->glue_updaters[g]->keep_mask,
                                    in_er) < 0) {
                                ST_WARNING("Failed to mat_mul_elems.");
                                return -1;
                            }
                        }
                    }

                    // store in_ac and out_er
                    if (mat_append(bptt_updater->in_acs + j, &in_ac) < 0) {
                        ST_WARNING("Failed to mat_append in_ac");
                        return -1;
                    }
                    if (mat_append(bptt_updater->out_ers + j, out_er) < 0) {
                        ST_WARNING("Failed to mat_append out_er");
                        return -1;
                    }
                }
            }

            bptt_updater->num_er_bptts = 0;
            if (bptt_updater->num_ac_bptts > bptt) {
                for (j = 1; j <= comp->glue_cycles[i][0]; j++) {
                    g = comp->glue_cycles[i][j];
                    glue = comp->glues[g];

                    if (mat_move_up(bptt_updater->ac_bptts + j, 0,
                                (bptt_updater->num_ac_bptts - bptt) * batch_size) < 0) {
                        ST_WARNING("Failed to mat_move_up for ac_bptts.");
                        return -1;
                    }
                }
                bptt_updater->num_ac_bptts = bptt;
            }
        }
    }

    for (i = 0; i < comp->num_glue_cycle; i++) {
        g = comp->glue_cycles[i][1];
        bptt = comp->glues[g]->bptt_opt.bptt;
        bptt_delay = comp->glues[g]->bptt_opt.bptt_delay;
        bptt_updater = comp_updater->bptt_updaters[i];

        if (comp_updater->bptt_step % bptt_delay == 0 || clear) {
            for (j = 1; j <= comp->glue_cycles[i][0]; j++) {
                if (bptt_updater->out_ers[j].num_rows == 0) {
                    continue;
                }

                wt_updater = bptt_updater->wt_updaters[j];

                if (wt_update(wt_updater, bptt_updater->out_ers + j, 1.0,
                            bptt_updater->in_acs + j, 1.0,
                            NULL, NULL) < 0) {
                    ST_WARNING("Failed to wt_update.");
                    return -1;
                }
                mat_clear(bptt_updater->out_ers + j);
                mat_clear(bptt_updater->in_acs + j);

                g = comp->glue_cycles[i][j];
                if (glue_updater_gen_keep_mask(comp_updater->glue_updaters[g],
                            batch_size) < 0) {
                    ST_WARNING("Failed to glue_updater_gen_keep_mask.");
                    return -1;
                }
            }
        }
    }

    return 0;
}

int comp_updater_reset(comp_updater_t *comp_updater, int batch_i)
{
    bptt_updater_t *bptt_updater;
    int i, j, g;

    ST_CHECK_PARAM(comp_updater == NULL, -1);

    if (comp_updater->bptt_updaters == NULL) {
        return 0;
    }

    for (g = 0; g < comp_updater->comp->num_glue_cycle; g++) {
        bptt_updater = comp_updater->bptt_updaters[g];

        for (j = 1; j <= comp_updater->comp->glue_cycles[g][0]; j++) {
            // </s> should be the last member on ac_bptts and er_bptts
            // so we reset all time steps on the buffer
            for (i = 0; i < bptt_updater->num_ac_bptts; i++) {
                if (mat_set_row(bptt_updater->ac_bptts + j,
                            i * comp_updater->batch_size + batch_i, 0.0) < 0) {
                    ST_WARNING("Failed to mat_set_row ac_bptts.");
                    return -1;
                }
            }
            // do we need to reset er_bptts?
            for (i = 0; i < bptt_updater->num_er_bptts; i++) {
                if (mat_set_row(bptt_updater->er_bptts + j,
                            i * comp_updater->batch_size + batch_i, 0.0) < 0) {
                    ST_WARNING("Failed to mat_set_row er_bptts.");
                    return -1;
                }
            }
        }
    }

    for (i = 2; i < comp_updater->comp->num_layer; i++) {
        if (layer_updater_reset(comp_updater->layer_updaters[i], batch_i) < 0) {
            ST_WARNING("Failed to layer_updater_reset.[%s]",
                    comp_updater->comp->layers[i]->name);
            return -1;
        }
    }

    return 0;
}

int comp_updater_forward(comp_updater_t *comp_updater, egs_batch_t *batch)
{
    component_t *comp;
    glue_updater_t *glue_updater;
    int g;

    ST_CHECK_PARAM(comp_updater == NULL, -1);

    comp = comp_updater->comp;

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Forward: comp[%s]", comp->name);
#endif

    if (comp_updater_state_size(comp_updater) > 0) {
        if (comp_updater->batch_size > 0) {
            if (comp_updater->batch_size != batch->num_egs) {
                ST_WARNING("Can not change batch_size for stateful model");
                return -1;
            }
        } else {
            comp_updater->batch_size = batch->num_egs;
        }
    } else {
        comp_updater->batch_size = batch->num_egs;
    }

    for (g = 0; g < comp->num_glue; g++) {
        glue_updater = comp_updater->glue_updaters[comp->fwd_order[g]];

        if (glue_updater->glue->recur_type == RECUR_NON) {
            if (glue_updater_gen_keep_mask(glue_updater, batch->num_egs) < 0) {
                ST_WARNING("Failed to glue_updater_gen_keep_mask.");
                return -1;
            }
        }

        if (glue_updater_forward(glue_updater, comp_updater, batch) < 0) {
            ST_WARNING("Failed to forward glue[%s].",
                    glue_updater->glue->name);
            return -1;
        }
    }

    return 0;
}

int comp_updater_backprop(comp_updater_t *comp_updater, egs_batch_t *batch)
{
    component_t *comp;
    glue_updater_t *glue_updater;
    int g;

    ST_CHECK_PARAM(comp_updater == NULL, -1);

    comp = comp_updater->comp;

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Backprop: comp[%s]", comp->name);
#endif

    for (g = comp->num_glue - 1; g >= 0; g--) {
        glue_updater = comp_updater->glue_updaters[comp->fwd_order[g]];
        if (glue_updater_backprop(glue_updater, comp_updater, batch) < 0) {
            ST_WARNING("Failed to backprop glue[%s].",
                    glue_updater->glue->name);
            return -1;
        }
    }

    if (comp->num_glue_cycle > 0) {
        // backprop recur glues
        if (comp_updater_bptt(comp_updater, false) < 0) {
            ST_WARNING("Failed to bptt comp[%s].", comp->name);
            return -1;
        }
    }

    return 0;
}

int comp_updater_save_state(comp_updater_t *comp_updater)
{
    int i;

    ST_CHECK_PARAM(comp_updater == NULL, -1);

    for (i = 2; i < comp_updater->comp->num_layer; i++) {
        if (layer_updater_save_state(comp_updater->layer_updaters[i]) < 0) {
            ST_WARNING("Failed to layer_updater_save_state.[%s]",
                    comp_updater->comp->layers[i]->name);
            return -1;
        }
    }

    return 0;
}

int comp_updater_prepare(comp_updater_t *comp_updater, egs_batch_t *batch)
{
    int i;

    ST_CHECK_PARAM(comp_updater == NULL, -1);

    for (i = 2; i < comp_updater->comp->num_layer; i++) {
        if (layer_updater_prepare(comp_updater->layer_updaters[i],
                    batch->num_egs) < 0) {
            ST_WARNING("Failed to layer_updater_prepare.[%s]",
                    comp_updater->comp->layers[i]->name);
            return -1;
        }
    }

    for (i = 0; i < comp_updater->comp->num_glue; i++) {
        if (glue_updater_prepare(comp_updater->glue_updaters[i],
                    comp_updater, batch) < 0) {
            ST_WARNING("Failed to glue_updater_prepare.[%s]",
                    comp_updater->comp->glues[i]->name);
            return -1;
        }
    }
    return 0;
}

int comp_updater_finish(comp_updater_t *comp_updater)
{
    component_t *comp;

    ST_CHECK_PARAM(comp_updater == NULL, -1);

    comp = comp_updater->comp;

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Finish: comp[%s]", comp->name);
#endif

    if (comp->num_glue_cycle > 0) {
        if (comp_updater_bptt(comp_updater, true) < 0) {
            ST_WARNING("Failed to clear bptt comp[%s].", comp->name);
            return -1;
        }
    }

    return 0;
}

int comp_updater_forward_util_out(comp_updater_t *comp_updater,
        egs_batch_t *batch)
{
    component_t *comp;
    glue_updater_t *glue_updater;
    int g;

    ST_CHECK_PARAM(comp_updater == NULL, -1);

    comp = comp_updater->comp;

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Forward-util-out: comp[%s]", comp->name);
#endif

    if (comp_updater_state_size(comp_updater) > 0) {
        if (comp_updater->batch_size > 0) {
            if (comp_updater->batch_size != batch->num_egs) {
                ST_WARNING("Can not change batch_size for stateful model");
                return -1;
            }
        } else {
            comp_updater->batch_size = batch->num_egs;
        }
    } else {
        comp_updater->batch_size = batch->num_egs;
    }

    for (g = 0; g < comp->num_glue; g++) {
        glue_updater = comp_updater->glue_updaters[comp->fwd_order[g]];
        if (glue_updater_forward_util_out(glue_updater, comp_updater,
                    batch) < 0) {
            ST_WARNING("Failed to forward_util_out glue[%s].",
                    glue_updater->glue->name);
            return -1;
        }
    }

    return 0;
}

int comp_updater_forward_out(comp_updater_t *comp_updater,
        output_node_id_t node)
{
    component_t *comp;
    glue_updater_t *glue_updater;

    int g;

    ST_CHECK_PARAM(comp_updater == NULL, -1);

    comp = comp_updater->comp;

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Forward-out: comp[%s], node["OUTPUT_NODE_FMT"]", comp->name,
            node);
#endif

    for (g = 0; g < comp->num_glue; g++) {
        glue_updater = comp_updater->glue_updaters[comp->fwd_order[g]];
        if (glue_updater_forward_out(glue_updater, comp_updater, node) < 0) {
            ST_WARNING("Failed to forward_out glue[%s].",
                    glue_updater->glue->name);
            return -1;
        }
    }

    return 0;
}

int comp_updater_forward_out_words(comp_updater_t *comp_updater, ivec_t *words)
{
    component_t *comp;
    glue_updater_t *glue_updater;

    int g;

    ST_CHECK_PARAM(comp_updater == NULL, -1);

    comp = comp_updater->comp;

#ifdef _CONNLM_TRACE_PROCEDURE_
    char buf[MAX_LINE_LEN];
    ST_TRACE("Forward-out-word: comp[%s], words[%s]", comp->name,
            ivec_dump(words, buf, MAX_LINE_LEN));
#endif

    for (g = 0; g < comp->num_glue; g++) {
        glue_updater = comp_updater->glue_updaters[comp->fwd_order[g]];
        if (glue_updater_forward_out_words(glue_updater,
                    comp_updater, words) < 0) {
            ST_WARNING("Failed to forward_out_word glue[%s].",
                    glue_updater->glue->name);
            return -1;
        }
    }

    return 0;
}

int comp_updater_state_size(comp_updater_t *comp_updater)
{
    int i;
    int size;
    int total_size;

    ST_CHECK_PARAM(comp_updater == NULL, -1);

    total_size = 0;
    for (i = 2; i < comp_updater->comp->num_layer; i++) {
        size = layer_updater_state_size(comp_updater->layer_updaters[i]);
        if (size < 0) {
            ST_WARNING("Failed to layer_updater_state_size.[%s]",
                    comp_updater->comp->layers[i]->name);
            return -1;
        }
        total_size += size;
    }

    return total_size;
}

int comp_updater_dump_state(comp_updater_t *comp_updater, mat_t *state)
{
    mat_t sub_state;

    int i;
    int size;
    int total_size;

    ST_CHECK_PARAM(comp_updater == NULL || state == NULL, -1);

    total_size = 0;
    for (i = 2; i < comp_updater->comp->num_layer; i++) {
        size = layer_updater_state_size(comp_updater->layer_updaters[i]);
        if (size < 0) {
            ST_WARNING("Failed to layer_updater_state_size.[%s]",
                    comp_updater->comp->layers[i]->name);
            return -1;
        }
        if (mat_submat(state, 0, 0, total_size, 0, &sub_state) < 0) {
            ST_WARNING("Failed to mat_submat state.");
            return -1;
        }
        if (layer_updater_dump_state(comp_updater->layer_updaters[i],
                    &sub_state) < 0) {
            ST_WARNING("Failed to layer_updater_dump_state.[%s]",
                    comp_updater->comp->layers[i]->name);
            return -1;
        }
        total_size += size;
    }

    return 0;
}

int comp_updater_dump_pre_ac_state(comp_updater_t *comp_updater,
        mat_t *state)
{
    mat_t sub_state;

    int i;
    int size;
    int total_size;

    ST_CHECK_PARAM(comp_updater == NULL || state == NULL, -1);

    total_size = 0;
    for (i = 2; i < comp_updater->comp->num_layer; i++) {
        size = layer_updater_state_size(comp_updater->layer_updaters[i]);
        if (size < 0) {
            ST_WARNING("Failed to layer_updater_state_size.[%s]",
                    comp_updater->comp->layers[i]->name);
            return -1;
        }
        if (mat_submat(state, 0, 0, total_size, 0, &sub_state) < 0) {
            ST_WARNING("Failed to mat_submat state.");
            return -1;
        }
        if (layer_updater_dump_pre_ac_state(
                    comp_updater->layer_updaters[i],
                    &sub_state) < 0) {
            ST_WARNING("Failed to layer_updater_dump_pre_ac_state.[%s]",
                    comp_updater->comp->layers[i]->name);
            return -1;
        }
        total_size += size;
    }

    return 0;
}

int comp_updater_feed_state(comp_updater_t *comp_updater, mat_t *state)
{
    mat_t sub_state;
    int i;
    int size;
    int total_size;

    ST_CHECK_PARAM(comp_updater == NULL || state == NULL, -1);

    total_size = 0;
    for (i = 2; i < comp_updater->comp->num_layer; i++) {
        size = layer_updater_state_size(comp_updater->layer_updaters[i]);
        if (size < 0) {
            ST_WARNING("Failed to layer_updater_state_size.[%s]",
                    comp_updater->comp->layers[i]->name);
            return -1;
        }
        if (mat_submat(state, 0, 0, total_size, 0, &sub_state) < 0) {
            ST_WARNING("Failed to mat_submat state.");
            return -1;
        }
        if (layer_updater_feed_state(comp_updater->layer_updaters[i],
                    &sub_state) < 0) {
            ST_WARNING("Failed to layer_updater_feed_state.[%s]",
                    comp_updater->comp->layers[i]->name);
            return -1;
        }
        total_size += size;
    }

    return 0;
}

int comp_updater_random_state(comp_updater_t *comp_updater, mat_t *state)
{
    mat_t sub_state;
    int i;
    int size;
    int total_size;

    ST_CHECK_PARAM(comp_updater == NULL || state == NULL, -1);

    total_size = 0;
    for (i = 2; i < comp_updater->comp->num_layer; i++) {
        size = layer_updater_state_size(comp_updater->layer_updaters[i]);
        if (size < 0) {
            ST_WARNING("Failed to layer_updater_state_size.[%s]",
                    comp_updater->comp->layers[i]->name);
            return -1;
        }
        if (mat_submat(state, 0, 0, total_size, 0, &sub_state) < 0) {
            ST_WARNING("Failed to mat_submat state.");
            return -1;
        }
        if (layer_updater_random_state(comp_updater->layer_updaters[i],
                    &sub_state) < 0) {
            ST_WARNING("Failed to layer_updater_random_state.[%s]",
                    comp_updater->comp->layers[i]->name);
            return -1;
        }
        total_size += size;
    }

    return 0;
}

int comp_updater_activate_state(comp_updater_t *comp_updater, mat_t *state)
{
    mat_t sub_state;

    int i;
    int size;
    int total_size;

    ST_CHECK_PARAM(comp_updater == NULL || state == NULL, -1);

    total_size = 0;
    for (i = 2; i < comp_updater->comp->num_layer; i++) {
        size = layer_updater_state_size(comp_updater->layer_updaters[i]);
        if (size < 0) {
            ST_WARNING("Failed to layer_updater_state_size.[%s]",
                    comp_updater->comp->layers[i]->name);
            return -1;
        }
        if (mat_submat(state, 0, 0, total_size, 0, &sub_state) < 0) {
            ST_WARNING("Failed to mat_submat state.");
            return -1;
        }
        if (layer_updater_activate_state(comp_updater->layer_updaters[i],
                    &sub_state) < 0) {
            ST_WARNING("Failed to layer_updater_activate.[%s]",
                    comp_updater->comp->layers[i]->name);
            return -1;
        }
        total_size += size;
    }

    return 0;
}
