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

#include <stutils/st_macro.h>
#include <stutils/st_log.h>
#include <stutils/st_mem.h>
#include <stutils/st_rand.h>

#include "../../glues/direct_glue.h"
#include "../../glues/fc_glue.h"
#include "../../glues/emb_glue.h"
#include "../../glues/out_glue.h"

#include "../component_updater.h"
#include "../layer_updater.h"
#include "direct_glue_updater.h"
#include "fc_glue_updater.h"
#include "emb_glue_updater.h"
#include "out_glue_updater.h"

#include "glue_updater.h"

static glue_updater_impl_t GLUE_UPDATER_IMPL[] = {
    {DIRECT_GLUE_NAME, direct_glue_updater_init, direct_glue_updater_destroy,
        direct_glue_updater_setup, direct_glue_updater_prepare,
        direct_glue_updater_forward, direct_glue_updater_backprop,
        direct_glue_updater_forward_util_out, direct_glue_updater_forward_out,
        direct_glue_updater_forward_out_words,
        direct_glue_updater_gen_keep_mask},
    {FC_GLUE_NAME, NULL, NULL, NULL,
        NULL, fc_glue_updater_forward, fc_glue_updater_backprop,
        fc_glue_updater_forward, NULL, NULL},
    {EMB_GLUE_NAME, emb_glue_updater_init, emb_glue_updater_destroy,
        emb_glue_updater_setup, emb_glue_updater_prepare,
        emb_glue_updater_forward, emb_glue_updater_backprop,
        emb_glue_updater_forward, NULL, NULL},
    {OUT_GLUE_NAME, out_glue_updater_init, out_glue_updater_destroy,
        out_glue_updater_setup, out_glue_updater_prepare,
        out_glue_updater_forward, out_glue_updater_backprop,
        NULL, out_glue_updater_forward_out, out_glue_updater_forward_out_words,
        NULL},
};

static glue_updater_impl_t* glue_updater_get_impl(const char *type)
{
    int t;

    for (t = 0; t < sizeof(GLUE_UPDATER_IMPL) / sizeof(GLUE_UPDATER_IMPL[0]); t++) {
        if (strcasecmp(type, GLUE_UPDATER_IMPL[t].type) == 0) {
            return GLUE_UPDATER_IMPL + t;
        }
    }

    return NULL;
}

void glue_updater_destroy(glue_updater_t *glue_updater)
{
    int i;

    if (glue_updater == NULL) {
        return;
    }

    if (glue_updater->impl != NULL) {
        if (glue_updater->impl->destroy != NULL) {
            glue_updater->impl->destroy(glue_updater);
        }
    }

    if (glue_updater->glue->recur_type != RECUR_BODY) {
        mat_destroy(&glue_updater->keep_mask);
    }
    mat_destroy(&glue_updater->dropout_val);

    if (glue_updater->wt_updaters != NULL) {
        for (i = 0; i < glue_updater->num_wt_updaters; i++) {
            safe_wt_updater_destroy(glue_updater->wt_updaters[i]);
        }
        safe_st_free(glue_updater->wt_updaters);
    }

    glue_updater->glue = NULL;
}

glue_updater_t* glue_updater_create(glue_t *glue)
{
    wt_update_type_t wu_type;
    glue_updater_t *glue_updater = NULL;

    ST_CHECK_PARAM(glue == NULL, NULL);

    glue_updater = (glue_updater_t *)st_malloc(sizeof(glue_updater_t));
    if (glue_updater == NULL) {
        ST_ERROR("Failed to st_malloc glue_updater.");
        goto ERR;
    }
    memset(glue_updater, 0, sizeof(glue_updater_t));

    glue_updater->glue = glue;

    if (strcasecmp(glue->type, DIRECT_GLUE_NAME) == 0) {
        wu_type = WT_UT_PART;
    } else if (strcasecmp(glue->type, EMB_GLUE_NAME) == 0) {
        wu_type = WT_UT_ONE_SHOT;
    } else if (strcasecmp(glue->type, FC_GLUE_NAME) == 0) {
        wu_type = WT_UT_FULL;
    } else if (strcasecmp(glue->type, OUT_GLUE_NAME) == 0) {
        wu_type = WT_UT_FULL;
    } else {
        ST_ERROR("Unknown glue type[%s]", glue->type);
        goto ERR;
    }

    glue_updater->keep_prob = 1.0; // no dropout by default

    glue_updater->impl = glue_updater_get_impl(glue->type);
    if (glue_updater->impl == NULL) {
        ST_ERROR("Unknown type of glue [%s].", glue->type);
        goto ERR;
    }
    if (glue_updater->impl->init != NULL) {
        if (glue_updater->impl->init(glue_updater) < 0) {
            ST_ERROR("Failed to init impl glue updater.");
            goto ERR;
        }
    }

    glue_updater->wt_updaters = glue_init_wt_updaters(glue, NULL,
            wu_type, &glue_updater->num_wt_updaters);
    if (glue_updater->wt_updaters == NULL) {
        ST_ERROR("Failed to init_wt_updaters for glue updater.");
        goto ERR;
    }

    return glue_updater;

ERR:
    safe_glue_updater_destroy(glue_updater);
    return NULL;
}

int glue_updater_setup(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, bool backprop)
{
    glue_t *glue;
    layer_updater_t *layer_updater;

    ST_CHECK_PARAM(glue_updater == NULL, -1);

    if (glue_updater->impl != NULL && glue_updater->impl->setup != NULL) {
        if (glue_updater->impl->setup(glue_updater,
                    comp_updater, backprop) < 0) {
            ST_ERROR("Failed to glue_updater->impl->setup.[%s]",
                    glue_updater->glue->name);
            goto ERR;
        }
    }

    glue = glue_updater->glue;
    if (glue->recur_type != RECUR_NON) {
        layer_updater = comp_updater->layer_updaters[glue->out_layer];
        if (layer_updater_setup_er_raw(layer_updater) < 0) {
            ST_ERROR("Failed to layer_updater_setup_er_raw.");
            goto ERR;
        }

        if (glue->recur_type == RECUR_HEAD) {
            layer_updater = comp_updater->layer_updaters[glue->in_layer];
            if (layer_updater_setup_state(layer_updater, backprop) < 0) {
                ST_ERROR("Failed to layer_updater_setup_state.");
                goto ERR;
            }
        }
    }

    return 0;

ERR:
    return -1;
}

int glue_updater_setup_pre_ac_state(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater)
{
    glue_t *glue;
    layer_updater_t *layer_updater;

    ST_CHECK_PARAM(glue_updater == NULL, -1);

    glue = glue_updater->glue;
    if (glue->recur_type == RECUR_HEAD) {
        layer_updater = comp_updater->layer_updaters[glue->in_layer];
        if (layer_updater_setup_pre_ac_state(layer_updater) < 0) {
            ST_ERROR("Failed to layer_updater_setup_pre_ac_state.");
            return -1;
        }
    }

    return 0;
}

int glue_updater_set_rand_seed(glue_updater_t *glue_updater, unsigned int *seed)
{
    ST_CHECK_PARAM(glue_updater == NULL, -1);

    glue_updater->rand_seed = seed;

    return 0;
}

int glue_updater_setup_dropout(glue_updater_t *glue_updater, real_t dropout)
{
    int keep_mask_len;

    ST_CHECK_PARAM(glue_updater == NULL, -1);

    glue_updater->keep_prob = 1.0 - dropout;

    if (glue_updater->keep_prob < 1.0) {
        if (glue_updater->glue->in_layer == INPUT_LAYER_ID) {
            keep_mask_len = glue_updater->glue->out_length
                - glue_updater->glue->out_offset;
        } else {
            keep_mask_len = glue_updater->glue->in_length
                - glue_updater->glue->in_offset;
        }

        if (mat_resize(&glue_updater->keep_mask, 1, keep_mask_len, NAN) < 0) {
            ST_ERROR("Failed to mat_resize keep_mask");
            return -1;
        }
    }

    return 0;
}

int mat_gen_keep_mask(unsigned int *rand_seed, real_t keep_prob, mat_t *mask)
{
    double p;
    int r, c;

    ST_CHECK_PARAM(mask == NULL || rand_seed == NULL, -1);

    for (r = 0; r < mask->num_rows; r++) {
        for (c = 0; c < mask->num_cols; c++) {
            p = st_random_r(0, 1, rand_seed);
            if (p < keep_prob) {
                MAT_VAL(mask, r, c) = 1.0;
            } else {
                MAT_VAL(mask, r, c) = 0.0;
            }
        }
    }

    return 0;
}

int mat_dropout(mat_t *in, mat_t *mask, real_t keep_prob, mat_t *out)
{
    ST_CHECK_PARAM(in == NULL || mask == NULL || out == NULL, -1);

    if (mat_resize(out, in->num_rows, in->num_cols, NAN) < 0) {
        ST_ERROR("Failed to mat_resize out.");
        return -1;
    }

    mat_scale(in, 1.0 / keep_prob);
    if (mat_mul_elems(in, mask, out) < 0) {
        ST_ERROR("Failed to mat_mul_elems.");
        return -1;
    }

    return 0;
}

int glue_updater_gen_keep_mask(glue_updater_t *glue_updater, int batch_size)
{
    ST_CHECK_PARAM(glue_updater == NULL || batch_size < 0, -1);

    if (glue_updater->keep_prob >= 1.0) {
        return 0;
    }

    if (glue_updater->impl != NULL
            && glue_updater->impl->gen_keep_mask != NULL) {
        if (glue_updater->impl->gen_keep_mask(glue_updater, batch_size) < 0) {
            ST_ERROR("Failed to glue_updater->impl->gen_keep_mask.[%s]",
                    glue_updater->glue->name);
            return -1;
        }

        return 0;
    }

    if(glue_updater->rand_seed == NULL) {
        ST_ERROR("rand_seed not set for glue[%s]", glue_updater->glue->name);
        return -1;
    }

    if (mat_resize_row(&glue_updater->keep_mask, batch_size, NAN) < 0) {
        ST_ERROR("Failed to mat_resize_row for keep_mask.");
        return -1;
    }

    if (mat_gen_keep_mask(glue_updater->rand_seed, glue_updater->keep_prob,
                &glue_updater->keep_mask) < 0) {
        ST_ERROR("Failed to mat_gen_keep_mask.");
        return -1;
    }

    return 0;
}

int glue_updater_prepare(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, egs_batch_t *batch)
{
    ST_CHECK_PARAM(glue_updater == NULL, -1);

    if (glue_updater->impl != NULL && glue_updater->impl->prepare != NULL) {
        if (glue_updater->impl->prepare(glue_updater, comp_updater,
                    batch) < 0) {
            ST_ERROR("Failed to glue_updater->impl->prepare.[%s]",
                    glue_updater->glue->name);
            return -1;
        }
    }

    return 0;
}

int glue_updater_forward(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, egs_batch_t *batch)
{
    glue_t *glue;
    layer_updater_t **layer_updaters;
    mat_t in_ac = {0};
    mat_t out_ac = {0};
    int lid;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL, -1);

    glue = glue_updater->glue;

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Forward: glue[%s]", glue->name);
#endif

    layer_updaters = comp_updater->layer_updaters;

    lid = glue->in_layer;
    if (lid >= 2) { // Ignore input & output layer
        if (glue->recur_type == RECUR_HEAD) {
            if (mat_submat(&layer_updaters[lid]->ac_state, 0, 0,
                        glue->in_offset, 0, &in_ac) < 0) {
                ST_ERROR("Failed to mat_submat ac_state.");
                return -1;
            }
        } else {
            if (layer_updater_activate(layer_updaters[lid]) < 0) {
                ST_ERROR("Failed to layer_activate.[%s]",
                        comp_updater->comp->layers[lid]->name);
                return -1;
            }
            if (mat_submat(&layer_updaters[lid]->ac, 0, 0,
                        glue->in_offset, 0, &in_ac) < 0) {
                ST_ERROR("Failed to mat_submat ac.");
                return -1;
            }
        }

        if (glue_updater->keep_prob < 1.0) {
            if (mat_dropout(&in_ac, &glue_updater->keep_mask,
                        glue_updater->keep_prob,
                        &glue_updater->dropout_val) < 0) {
                ST_ERROR("Failed to mat_dropout.");
                return -1;
            }

            mat_assign(&in_ac, &glue_updater->dropout_val);
        }
    }
    if (glue->out_layer == 0) {
        // output layer don't need out_ac
    } else {
        if (mat_submat(&layer_updaters[glue->out_layer]->ac, 0, 0,
                    glue->out_offset, 0, &out_ac) < 0) {
            ST_ERROR("Failed to mat_submat out_ac.");
            return -1;
        }
    }

    if (glue_updater->impl != NULL && glue_updater->impl->forward != NULL) {
        if (glue_updater->impl->forward(glue_updater, comp_updater,
                    batch, &in_ac, &out_ac) < 0) {
            ST_ERROR("Failed to glue_updater->impl->forward.[%s]",
                    glue->name);
            return -1;
        }
    }

    return 0;
}

int glue_updater_backprop(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, egs_batch_t *batch)
{
    glue_t *glue;
    layer_updater_t **layer_updaters;
    mat_t in_ac = {0};
    mat_t out_er = {0};
    mat_t in_er = {0};
    int out_lid;

    ST_CHECK_PARAM(glue_updater == NULL, -1);

    glue = glue_updater->glue;

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Backprop: glue[%s]", glue->name);
#endif

    layer_updaters = comp_updater->layer_updaters;

    out_lid = glue->out_layer;
    if (out_lid >= 2) { // Ignore input & output layer
        if (layer_updater_deriv(layer_updaters[out_lid]) < 0) {
            ST_ERROR("Failed to layer_deriv.[%s]",
                    comp_updater->comp->layers[out_lid]->name);
            return -1;
        }
    }

    if (glue_updater->impl != NULL && glue_updater->impl->backprop != NULL) {
        if (glue->in_layer >= 2) { // Ignore input layer
            if (glue->recur_type != RECUR_HEAD) {
                if (mat_submat(&layer_updaters[glue->in_layer]->er, 0, 0,
                            glue->in_offset, 0, &in_er) < 0) {
                    ST_ERROR("Failed to mat_submat in_er.");
                    return -1;
                }
            }
            if (glue->recur_type == RECUR_NON) {
                // recur glues will be updated in bptt_updater
                if (glue_updater->keep_prob < 1.0) {
                    // dropout_val should be filled during forward pass
                    mat_assign(&in_ac, &glue_updater->dropout_val);
                } else {
                    if (mat_submat(&layer_updaters[glue->in_layer]->ac, 0, 0,
                            glue->in_offset, 0, &in_ac) < 0) {
                        ST_ERROR("Failed to mat_submat in_ac.");
                        return -1;
                    }
                }
            }
        }
        if (out_lid == 0) {
            // output layer don't need out_er
        } else {
            if (mat_submat(&layer_updaters[out_lid]->er, 0, 0,
                    glue->out_offset, 0, &out_er) < 0) {
                ST_ERROR("Failed to mat_submat out_er.");
                return -1;
            }
        }
        if (glue_updater->impl->backprop(glue_updater, comp_updater,
                    batch, &in_ac, &out_er, &in_er) < 0) {
            ST_ERROR("Failed to glue_updater->impl->backprop.[%s]",
                    glue->name);
            return -1;
        }

        if (glue_updater->keep_prob < 1.0 && in_er.num_rows > 0) {
            if (mat_mul_elems(&in_er, &glue_updater->keep_mask, &in_er) < 0) {
                ST_ERROR("Failed to mat_mul_elems.");
                return -1;
            }
        }
    }

    return 0;
}

int glue_updater_forward_util_out(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, egs_batch_t *batch)
{
    glue_t *glue;
    layer_updater_t **layer_updaters;
    mat_t in_ac = {0};
    mat_t out_ac = {0};
    int lid;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL, -1);

    glue = glue_updater->glue;

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Forward-util-out: glue[%s]", glue->name);
#endif

    layer_updaters = comp_updater->layer_updaters;

    lid = glue->in_layer;
    if (lid >= 2) { // Ignore input & output layer
        if (glue->recur_type == RECUR_HEAD) {
            if (mat_submat(&layer_updaters[lid]->ac_state, 0, 0,
                        glue->in_offset, 0, &in_ac) < 0) {
                ST_ERROR("Failed to mat_submat ac_state.");
                return -1;
            }
        } else {
            if (layer_updater_activate(layer_updaters[lid]) < 0) {
                ST_ERROR("Failed to layer_activate.[%s]",
                        comp_updater->comp->layers[lid]->name);
                return -1;
            }
            if (mat_submat(&layer_updaters[lid]->ac, 0, 0,
                        glue->in_offset, 0, &in_ac) < 0) {
                ST_ERROR("Failed to mat_submat ac.");
                return -1;
            }
        }
    }
    if (glue->out_layer == 0) {
        // output layer don't need out_ac
    } else {
        if (mat_submat(&layer_updaters[glue->out_layer]->ac, 0, 0,
                    glue->out_offset, 0, &out_ac) < 0) {
            ST_ERROR("Failed to mat_submat out_ac.");
            return -1;
        }
    }

    if (glue_updater->impl != NULL
            && glue_updater->impl->forward_util_out != NULL) {
        if (glue_updater->impl->forward_util_out(glue_updater, comp_updater,
                    batch, &in_ac, &out_ac) < 0) {
            ST_ERROR("Failed to glue_updater->impl->forward_util_out.[%s]",
                    glue->name);
            return -1;
        }
    }

    return 0;
}

int glue_updater_forward_out(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, output_node_id_t node)
{
    glue_t *glue;
    layer_updater_t **layer_updaters;

    mat_t in_ac = {0};
    mat_t out_ac = {0};
    int lid;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL, -1);

    glue = glue_updater->glue;
    layer_updaters = comp_updater->layer_updaters;

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Forward-out: glue[%s]", glue->name);
#endif

    if (glue_updater->impl != NULL && glue_updater->impl->forward_out != NULL) {
        lid = glue->in_layer;
        if (lid >= 2) { // Ignore input & output layer
            if (glue->recur_type == RECUR_HEAD) {
                if (mat_submat(&layer_updaters[lid]->ac_state, 0, 0,
                            glue->in_offset, 0, &in_ac) < 0) {
                    ST_ERROR("Failed to mat_submat ac_state.");
                    return -1;
                }
            } else {
                if (mat_submat(&layer_updaters[lid]->ac, 0, 0,
                            glue->in_offset, 0, &in_ac) < 0) {
                    ST_ERROR("Failed to mat_submat ac.");
                    return -1;
                }
            }
        }
        if (glue->out_layer == 0) {
            // output layer don't need out_ac
        } else {
            if (mat_submat(&layer_updaters[glue->out_layer]->ac, 0, 0,
                        glue->out_offset, 0, &out_ac) < 0) {
                ST_ERROR("Failed to mat_submat out_ac.");
                return -1;
            }
        }
        if (glue_updater->impl->forward_out(glue_updater, comp_updater,
                    node, &in_ac, &out_ac) < 0) {
            ST_ERROR("Failed to glue_updater->impl->forward_out.[%s]",
                    glue->name);
            return -1;
        }
    }

    return 0;
}

int glue_updater_forward_out_words(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, ivec_t *words)
{
    glue_t *glue;
    layer_updater_t **layer_updaters;

    mat_t in_ac = {0};
    mat_t out_ac = {0};
    int lid;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL, -1);

    glue = glue_updater->glue;
    layer_updaters = comp_updater->layer_updaters;

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Forward-out-word: glue[%s]", glue->name);
#endif

    if (glue_updater->impl != NULL && glue_updater->impl->forward_out_words != NULL) {
        lid = glue->in_layer;
        if (layer_updaters[lid]->ac.num_rows != words->size) {
            ST_ERROR("words->size not match with batch_size.");
            return -1;
        }

        if (lid >= 2) { // Ignore input & output layer
            if (glue->recur_type == RECUR_HEAD) {
                if (mat_submat(&layer_updaters[lid]->ac_state, 0, 0,
                            glue->in_offset, 0, &in_ac) < 0) {
                    ST_ERROR("Failed to mat_submat ac_state.");
                    return -1;
                }
            } else {
                if (mat_submat(&layer_updaters[lid]->ac, 0, 0,
                            glue->in_offset, 0, &in_ac) < 0) {
                    ST_ERROR("Failed to mat_submat ac.");
                    return -1;
                }
            }
        }
        if (glue->out_layer == 0) {
            // output layer don't need out_ac
        } else {
            if (mat_submat(&layer_updaters[glue->out_layer]->ac, 0, 0,
                        glue->out_offset, 0, &out_ac) < 0) {
                ST_ERROR("Failed to mat_submat out_ac.");
                return -1;
            }
        }
        if (glue_updater->impl->forward_out_words(glue_updater, comp_updater,
                    words, &in_ac, &out_ac) < 0) {
            ST_ERROR("Failed to glue_updater->impl->forward_out_words.[%s]",
                    glue->name);
            return -1;
        }
    }

    return 0;
}

int propagate_error(mat_t *wt, mat_t *src_er,
        real_t scale, real_t er_cutoff, mat_t *dst_er)
{
    if (add_mat_mat(scale, src_er, MT_NoTrans,
                wt, MT_NoTrans, 1.0, dst_er) < 0) {
        ST_ERROR("Failed to add_mat_mat.");
        return -1;
    }

    if (er_cutoff > 0) {
        mat_cutoff(dst_er, er_cutoff);
    }

    return 0;
}
