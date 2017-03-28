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

    safe_st_aligned_free(comp_updater->bptt_er0);
    safe_st_aligned_free(comp_updater->bptt_er1);

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

int comp_updater_setup(comp_updater_t *comp_updater, bool backprop)
{
    component_t *comp;
    layer_updater_t **layer_updaters;
    glue_t *glue;
    int i, j, g, sz;

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

            sz = 0;
            for (i = 0; i < comp->num_glue_cycle; i++) {
                for (j = 1; j <= comp->glue_cycles[i][0]; j++) {
                    g = comp->glue_cycles[i][j];
                    glue = comp->glues[g];
                    if (comp->layers[glue->in_layer]->size > sz) {
                        sz = comp->layers[glue->in_layer]->size;
                    }
                    if (comp->layers[glue->out_layer]->size > sz) {
                        sz = comp->layers[glue->out_layer]->size;
                    }
                }
            }
            sz *= sizeof(real_t);
            comp_updater->bptt_er0 = st_aligned_malloc(sz, ALIGN_SIZE);
            if (comp_updater->bptt_er0 == NULL) {
                ST_WARNING("Failed to st_aligned_malloc bptt_er0.");
                goto ERR;
            }
            comp_updater->bptt_er1 = st_aligned_malloc(sz, ALIGN_SIZE);
            if (comp_updater->bptt_er1 == NULL) {
                ST_WARNING("Failed to st_aligned_malloc bptt_er1.");
                goto ERR;
            }
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
    safe_st_aligned_free(comp_updater->bptt_er0);
    safe_st_aligned_free(comp_updater->bptt_er1);

    return -1;
}

static int comp_updater_bptt(comp_updater_t *comp_updater, bool clear)
{
    component_t *comp;
    layer_updater_t **layer_updaters;
    layer_updater_t *out_layer_updater;
    wt_updater_t *wt_updater;
    glue_t *glue;
    bptt_updater_t *bptt_updater;

    real_t *in_er = NULL, *out_er = NULL, *in_ac = NULL, *out_ac, *tmp;
    int bptt, bptt_delay;
    int i, j, g, t, ii, er_t, in_sz, out_sz;

    ST_CHECK_PARAM(comp_updater == NULL, -1);

    comp = comp_updater->comp;
    layer_updaters = comp_updater->layer_updaters;

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Backprop: BPTT comp[%s](clear[%s])", comp->name,
            bool2str(clear));
#endif

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
                in_sz = layer_updaters[glue->in_layer]->layer->size;
                out_sz = layer_updaters[glue->out_layer]->layer->size;

                out_er = layer_updaters[glue->out_layer]->er_raw;
                if (j == 1) {
                    in_ac = layer_updaters[glue->in_layer]->ac_state;
                } else {
                    in_ac = layer_updaters[glue->in_layer]->ac;
                }

                if (bptt_updater->num_ac_bptt >= bptt + bptt_delay) {
                    memmove(bptt_updater->ac_bptt[j],
                            bptt_updater->ac_bptt[j] + in_sz,
                            sizeof(real_t)*in_sz*(bptt_updater->num_ac_bptt-1));
                    memcpy(bptt_updater->ac_bptt[j]
                            + (bptt_updater->num_ac_bptt - 1) * in_sz,
                            in_ac, sizeof(real_t) * in_sz);
                } else {
                    memcpy(bptt_updater->ac_bptt[j]
                            + bptt_updater->num_ac_bptt * in_sz,
                            in_ac, sizeof(real_t) * in_sz);
                }

                if (bptt_updater->num_er_bptt >= bptt_delay) {
                    memmove(bptt_updater->er_bptt[j],
                            bptt_updater->er_bptt[j] + out_sz,
                            sizeof(real_t)*out_sz*(bptt_updater->num_er_bptt-1));
                    memcpy(bptt_updater->er_bptt[j]
                            + (bptt_updater->num_er_bptt - 1) * out_sz,
                            out_er, sizeof(real_t) * out_sz);
                } else {
                    memcpy(bptt_updater->er_bptt[j]
                            + bptt_updater->num_er_bptt * out_sz,
                            out_er, sizeof(real_t) * out_sz);
                }
            }
            if (bptt_updater->num_ac_bptt < bptt + bptt_delay) {
                bptt_updater->num_ac_bptt++;
            }
            if (bptt_updater->num_er_bptt < bptt_delay) {
                bptt_updater->num_er_bptt++;
            }
        }

        if (comp_updater->bptt_step % bptt_delay == 0 || clear) {
            // backprop through time
            for (t = bptt_updater->num_ac_bptt - 1; t >= 0; t--) {
                for (j = 1; j <= comp->glue_cycles[i][0]; j++) {
                    g = comp->glue_cycles[i][j];
                    glue = comp->glues[g];

                    in_sz = layer_updaters[glue->in_layer]->layer->size;
                    out_sz = layer_updaters[glue->out_layer]->layer->size;

                    if (t == bptt_updater->num_ac_bptt - 1 && j == 1) {
                        // using values in current timestep
                        out_ac = layer_updaters[glue->out_layer]->ac;

                        // we can not use in_ or out_layer_updater->er as
                        // buffer, since it could be the case that
                        // glue->in_layer == glue->out_layer, then
                        // in_er and out_er would be the same.
                        in_er = comp_updater->bptt_er0;
                        out_er = comp_updater->bptt_er1;
                        memset(out_er, 0, out_sz * sizeof(real_t));
                    } else {
                        // the glues in a cycle is joined one-by-one,
                        // the output of one glue is the input of the prev glue
                        out_ac = in_ac;

                        // exchange in_er and out_er
                        tmp = out_er;
                        out_er = in_er;
                        in_er = tmp;
                    }
                    in_ac = bptt_updater->ac_bptt[j] + (t * in_sz);

                    er_t = t - bptt_updater->num_ac_bptt
                             + bptt_updater->num_er_bptt;
                    if (er_t >= 0) {
#ifdef _CONNLM_TRACE_PROCEDURE_
                        ST_TRACE("Backprop: BPTT timestep(delayed)[%d], "
                                "glue[%s]", t, comp->glues[g]->name);
#endif
                        for (ii = 0; ii < out_sz; ii++) {
                            out_er[ii] +=
                                bptt_updater->er_bptt[j][er_t * out_sz + ii];
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
                                out_er, out_ac,
                                out_layer_updater->layer->size) < 0) {
                        ST_WARNING("Failed to deriv.[%s]",
                                out_layer_updater->layer->name);
                        return -1;
                    }

                    wt_updater = bptt_updater->wt_updaters[j];
                    // propagate error to next glue
                    if (j < comp->glue_cycles[i][0] || t - 1 >= 0) {
                        memset(in_er, 0, sizeof(real_t) * in_sz);
                        propagate_error(in_er, out_er,
                                wt_updater->wt,
                                wt_updater->col, wt_updater->row,
                                wt_updater->param.er_cutoff, 1.0);
                    }

                    // update weight
                    // the recur glue should not be embedding or output or
                    // direct, so the parameters to wt_update only involves
                    // in_ac and out_er
                    if (wt_update(wt_updater, NULL, -1, out_er, 1.0,
                                in_ac, 1.0, NULL) < 0) {
                        ST_WARNING("Failed to wt_update.");
                        return -1;
                    }
                }
            }

            bptt_updater->num_er_bptt = 0;
        }
    }

    for (i = 0; i < comp->num_glue_cycle; i++) {
        g = comp->glue_cycles[i][1];
        bptt = comp->glues[g]->bptt_opt.bptt;
        bptt_delay = comp->glues[g]->bptt_opt.bptt_delay;
        bptt_updater = comp_updater->bptt_updaters[i];

        if (comp_updater->bptt_step % bptt_delay == 0 || clear) {
            for (j = 1; j <= comp->glue_cycles[i][0]; j++) {
                wt_updater = bptt_updater->wt_updaters[j];
                if (wt_flush(wt_updater, false) < 0) {
                    ST_WARNING("Failed to wt_flush.");
                    return -1;
                }
            }
        }
    }

    return 0;
}

int comp_updater_reset(comp_updater_t *comp_updater)
{
    int i;

    ST_CHECK_PARAM(comp_updater == NULL, -1);

    // bptt_reset must before layer_reset, since it will use the ac_state.
    if (comp_updater->bptt_updaters != NULL) {
        if (comp_updater_bptt(comp_updater, true) < 0) {
            ST_WARNING("Failed to reset bptt comp[%s].",
                    comp_updater->comp->name);
            return -1;
        }
        comp_updater->bptt_step = 0;
        for (i = 0; i < comp_updater->comp->num_glue_cycle; i++) {
            if (bptt_updater_reset(comp_updater->bptt_updaters[i]) < 0) {
                ST_WARNING("Failed to bptt_updater_reset.[%d][%s]", i,
                        comp_updater->comp->glues[
                        comp_updater->comp->glue_cycles[i][1]]->name);
                return -1;
            }
        }
    }

    for (i = 2; i < comp_updater->comp->num_layer; i++) {
        if (layer_updater_reset(comp_updater->layer_updaters[i]) < 0) {
            ST_WARNING("Failed to layer_updater_reset.[%s]",
                    comp_updater->comp->layers[i]->name);
            return -1;
        }
    }

    for (i = 2; i < comp_updater->comp->num_glue; i++) {
        if (glue_updater_reset(comp_updater->glue_updaters[i]) < 0) {
            ST_WARNING("Failed to glue_updater_reset.[%s]",
                    comp_updater->comp->glues[i]->name);
            return -1;
        }
    }

    return 0;
}

int comp_updater_forward(comp_updater_t *comp_updater, sent_t *input_sent)
{
    component_t *comp;
    glue_updater_t *glue_updater;
    int g;

    ST_CHECK_PARAM(comp_updater == NULL, -1);

    comp = comp_updater->comp;

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Forward: comp[%s]", comp->name);
#endif

    for (g = 0; g < comp->num_glue; g++) {
        glue_updater = comp_updater->glue_updaters[comp->fwd_order[g]];
        if (glue_updater_forward(glue_updater, comp_updater, input_sent) < 0) {
            ST_WARNING("Failed to forward glue[%s].",
                    glue_updater->glue->name);
            return -1;
        }
    }

    return 0;
}

int comp_updater_backprop(comp_updater_t *comp_updater, sent_t *input_sent)
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
        if (glue_updater_backprop(glue_updater, comp_updater, input_sent) < 0) {
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

int comp_updater_clear(comp_updater_t *comp_updater)
{
    int i;

    ST_CHECK_PARAM(comp_updater == NULL, -1);

    for (i = 2; i < comp_updater->comp->num_layer; i++) {
        if (layer_updater_clear(comp_updater->layer_updaters[i]) < 0) {
            ST_WARNING("Failed to layer_updater_clear.[%s]",
                    comp_updater->comp->layers[i]->name);
            return -1;
        }
    }

    return 0;
}

int comp_updater_finish(comp_updater_t *comp_updater)
{
    component_t *comp;
    glue_updater_t *glue_updater;
    int g;

    ST_CHECK_PARAM(comp_updater == NULL, -1);

    comp = comp_updater->comp;

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Finish: comp[%s]", comp->name);
#endif

    if (comp_updater_bptt(comp_updater, true) < 0) {
        ST_WARNING("Failed to clear bptt comp[%s].", comp->name);
        return -1;
    }

    for (g = comp->num_glue - 1; g >= 0; g--) {
        glue_updater = comp_updater->glue_updaters[comp->fwd_order[g]];
        if (glue_updater_finish(glue_updater) < 0) {
            ST_WARNING("Failed to finish glue[%s].", glue_updater->glue->name);
            return -1;
        }
    }

    return 0;
}

int comp_updater_forward_util_out(comp_updater_t *comp_updater,
        sent_t *input_sent)
{
    component_t *comp;
    glue_updater_t *glue_updater;
    int g;

    ST_CHECK_PARAM(comp_updater == NULL, -1);

    comp = comp_updater->comp;

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Forward-util-out: comp[%s]", comp->name);
#endif

    for (g = 0; g < comp->num_glue; g++) {
        glue_updater = comp_updater->glue_updaters[comp->fwd_order[g]];
        if (glue_updater_forward_util_out(glue_updater, comp_updater,
                    input_sent) < 0) {
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

int comp_updater_dump_state(comp_updater_t *comp_updater, real_t *state)
{
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
        if (layer_updater_dump_state(comp_updater->layer_updaters[i],
                    state + total_size) < 0) {
            ST_WARNING("Failed to layer_updater_dump_state.[%s]",
                    comp_updater->comp->layers[i]->name);
            return -1;
        }
        total_size += size;
    }

    return 0;
}

int comp_updater_feed_state(comp_updater_t *comp_updater, real_t *state)
{
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
        if (layer_updater_feed_state(comp_updater->layer_updaters[i],
                    state + total_size) < 0) {
            ST_WARNING("Failed to layer_updater_feed_state.[%s]",
                    comp_updater->comp->layers[i]->name);
            return -1;
        }
        total_size += size;
    }

    return 0;
}
