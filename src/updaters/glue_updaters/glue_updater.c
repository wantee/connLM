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
        direct_glue_updater_setup,
        direct_glue_updater_forward, direct_glue_updater_backprop,
        direct_glue_updater_forward_util_out, direct_glue_updater_forward_out,
        direct_glue_updater_forward_out_word,
        true},
    {FC_GLUE_NAME, NULL, NULL,
        NULL, fc_glue_updater_forward, fc_glue_updater_backprop,
        fc_glue_updater_forward, NULL, false},
    {EMB_GLUE_NAME, NULL, NULL,
        NULL, emb_glue_updater_forward, emb_glue_updater_backprop,
        emb_glue_updater_forward, NULL, false},
    {OUT_GLUE_NAME, NULL, NULL,
        NULL, out_glue_updater_forward, out_glue_updater_backprop,
        NULL, out_glue_updater_forward_out, out_glue_updater_forward_out_word,
        true},
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
    if (glue_updater == NULL) {
        return;
    }

    if (glue_updater->impl != NULL) {
        if (glue_updater->impl->destroy != NULL) {
            glue_updater->impl->destroy(glue_updater);
        }
    }

    safe_st_free(glue_updater->forwarded);
    if (glue_updater->glue->recur_type != RECUR_BODY) {
        safe_st_free(glue_updater->keep_mask);
    } else {
        glue_updater->keep_mask = NULL;
    }
    safe_st_free(glue_updater->dropout_ac);

    safe_wt_updater_destroy(glue_updater->wt_updater);
    glue_updater->glue = NULL;
}

glue_updater_t* glue_updater_create(glue_t *glue)
{
    glue_updater_t *glue_updater = NULL;

    ST_CHECK_PARAM(glue == NULL, NULL);

    glue_updater = (glue_updater_t *)st_malloc(sizeof(glue_updater_t));
    if (glue_updater == NULL) {
        ST_WARNING("Failed to st_malloc glue_updater.");
        goto ERR;
    }
    memset(glue_updater, 0, sizeof(glue_updater_t));

    glue_updater->glue = glue;

    glue_updater->keep_prob = 1.0; // no dropout by default

    glue_updater->impl = glue_updater_get_impl(glue->type);
    if (glue_updater->impl == NULL) {
        ST_WARNING("Unknown type of glue [%s].", glue->type);
        goto ERR;
    }
    if (glue_updater->impl->init != NULL) {
        if (glue_updater->impl->init(glue_updater) < 0) {
            ST_WARNING("Failed to init impl glue updater.");
            goto ERR;
        }
    }

    glue_updater->wt_updater = glue_init_wt_updater(glue, NULL);
    if (glue_updater->wt_updater == NULL) {
        ST_WARNING("Failed to init_wt_updater for glue updater.");
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

    wt_updater_clear(glue_updater->wt_updater);

    if (glue_updater->impl != NULL && glue_updater->impl->setup != NULL) {
        if (glue_updater->impl->setup(glue_updater,
                    comp_updater, backprop) < 0) {
            ST_WARNING("Failed to glue_updater->impl->setup.[%s]",
                    glue_updater->glue->name);
            goto ERR;
        }
    }

    glue = glue_updater->glue;
    if (glue->recur_type != RECUR_NON) {
        layer_updater = comp_updater->layer_updaters[glue->out_layer];
        if (layer_updater_setup_er_raw(layer_updater) < 0) {
            ST_WARNING("Failed to layer_updater_setup_er_raw.");
            goto ERR;
        }

        if (glue->recur_type == RECUR_HEAD) {
            layer_updater = comp_updater->layer_updaters[glue->in_layer];
            if (layer_updater_setup_state(layer_updater, backprop) < 0) {
                ST_WARNING("Failed to layer_updater_setup_state.");
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
            ST_WARNING("Failed to layer_updater_setup_pre_ac_state.");
            return -1;
        }
    }

    return 0;
}

int glue_updater_reset(glue_updater_t *glue_updater)
{
    ST_CHECK_PARAM(glue_updater == NULL, -1);

    return 0;
}

int glue_updater_setup_dropout(glue_updater_t *glue_updater, real_t dropout)
{
    ST_CHECK_PARAM(glue_updater == NULL, -1);

    glue_updater->keep_prob = 1.0 - dropout;

    if (glue_updater->keep_prob < 1.0) {
        glue_updater->keep_mask = (bool *)st_malloc(sizeof(bool)
                * glue_updater->glue->in_length);
        if (glue_updater->keep_mask == NULL) {
            ST_WARNING("Failed to st_malloc keep_mask");
            return -1;
        }

        if (glue_updater->glue->recur_type == RECUR_NON) {
            glue_updater->dropout_ac = (real_t *)st_malloc(sizeof(real_t)
                    * glue_updater->glue->in_length);
            if (glue_updater->dropout_ac == NULL) {
                ST_WARNING("Failed to st_malloc dropout_ac");
                return -1;
            }
        }
    }

    return 0;
}

int glue_updater_gen_keep_mask(glue_updater_t *glue_updater, unsigned int *seed)
{
    double p;
    int i;

    ST_CHECK_PARAM(glue_updater == NULL, -1);

    if (glue_updater->keep_prob >= 1.0) {
        return 0;
    }

    ST_CHECK_PARAM(seed == NULL, -1);

    for (i = 0; i < glue_updater->glue->in_length; i++) {
        p = st_random_r(0, 1, seed);
        if (p < glue_updater->keep_prob) {
            glue_updater->keep_mask[i] = 1;
        } else {
            glue_updater->keep_mask[i] = 0;
        }
    }

    return 0;
}

int glue_updater_forward(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, sent_t *input_sent)
{
    glue_t *glue;
    layer_updater_t **layer_updaters;
    real_t *in_ac;
    real_t *out_ac;
    int lid, i;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL, -1);

    glue = glue_updater->glue;

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Forward: glue[%s]", glue->name);
#endif

    layer_updaters = comp_updater->layer_updaters;

    in_ac = NULL;
    lid = glue->in_layer;
    if (lid >= 2) { // Ignore input & output layer
        if (glue->recur_type == RECUR_HEAD) {
            in_ac = layer_updaters[lid]->ac_state + glue->in_offset;
        } else {
            if (layer_updater_activate(layer_updaters[lid]) < 0) {
                ST_WARNING("Failed to layer_activate.[%s]",
                        comp_updater->comp->layers[lid]->name);
                return -1;
            }
            in_ac = layer_updaters[lid]->ac + glue->in_offset;

            if (glue_updater->keep_prob < 1.0) {
                for (i = 0; i < glue->in_length; i++) {
                    if(glue_updater->keep_mask[i] == 1) {
                        glue_updater->dropout_ac[i] = in_ac[i] / glue_updater->keep_prob; // scale
                    } else {
                        glue_updater->dropout_ac[i] = 0.0;
                    }
                }
                in_ac = glue_updater->dropout_ac;
            }
        }
    }
    if (glue->out_layer == 0) { // output layer
        out_ac = comp_updater->out_updater->ac + glue->out_offset;
    } else {
        out_ac = layer_updaters[glue->out_layer]->ac + glue->out_offset;
    }

    if (glue_updater->impl != NULL && glue_updater->impl->forward != NULL) {
        if (glue_updater->impl->forward(glue_updater, comp_updater,
                    input_sent, in_ac, out_ac) < 0) {
            ST_WARNING("Failed to glue_updater->impl->forward.[%s]",
                    glue->name);
            return -1;
        }
    }

    return 0;
}

int glue_updater_backprop(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, sent_t *input_sent)
{
    glue_t *glue;
    layer_updater_t **layer_updaters;
    real_t *in_ac = NULL;
    real_t *out_er = NULL;
    real_t *in_er = NULL;
    int out_lid;
    int i;

    ST_CHECK_PARAM(glue_updater == NULL, -1);

    glue = glue_updater->glue;

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Backprop: glue[%s]", glue->name);
#endif

    layer_updaters = comp_updater->layer_updaters;

    out_lid = glue->out_layer;
    if (out_lid >= 2) { // Ignore input & output layer
        if (layer_updater_deriv(layer_updaters[out_lid]) < 0) {
            ST_WARNING("Failed to layer_deriv.[%s]",
                    comp_updater->comp->layers[out_lid]->name);
            return -1;
        }
    }

    if (glue_updater->impl != NULL && glue_updater->impl->backprop != NULL) {
        if (glue->in_layer >= 2) { // Ignore input layer
            if (glue->recur_type != RECUR_HEAD) {
                in_er = layer_updaters[glue->in_layer]->er + glue->in_offset;
            }
            if (glue->recur_type == RECUR_NON) {
                // recur glues will be updated in bptt_updater
                if (glue_updater->keep_prob < 1.0) {
                    // dropout_ac should be filled during forward pass
                    in_ac = glue_updater->dropout_ac;
                } else {
                    in_ac = layer_updaters[glue->in_layer]->ac + glue->in_offset;
                }
            }
        }
        if (out_lid == 0) { // output layer
            out_er = comp_updater->out_updater->er + glue->out_offset;
        } else {
            out_er = layer_updaters[out_lid]->er + glue->out_offset;
        }
        if (glue_updater->impl->backprop(glue_updater, comp_updater,
                    input_sent, in_ac, out_er, in_er) < 0) {
            ST_WARNING("Failed to glue_updater->impl->backprop.[%s]",
                    glue->name);
            return -1;
        }

        if (glue->recur_type == RECUR_NON) {
            if (wt_flush(glue_updater->wt_updater, false) < 0) {
                ST_WARNING("Failed to wt_flush.");
                return -1;
            }
        }

        if (glue_updater->keep_prob < 1.0 && in_er != NULL) {
            for (i = 0; i < glue->in_length; i++) {
                if (glue_updater->keep_mask[i] == 0) {
                    in_er[i] = 0.0;
                }
            }
        }
    }

    return 0;
}

int glue_updater_finish(glue_updater_t *glue_updater)
{
    ST_CHECK_PARAM(glue_updater == NULL, -1);

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Finish: glue[%s]", glue_updater->glue->name);
#endif

    if (wt_flush(glue_updater->wt_updater, true) < 0) {
        ST_WARNING("Failed to wt_flush.");
        return -1;
    }

    return 0;
}

int glue_updater_forward_util_out(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, sent_t *input_sent)
{
    glue_t *glue;
    layer_updater_t **layer_updaters;
    real_t *in_ac;
    real_t *out_ac;
    int lid;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL, -1);

    glue = glue_updater->glue;

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Forward-util-out: glue[%s]", glue->name);
#endif

    layer_updaters = comp_updater->layer_updaters;

    lid = glue->in_layer;
    in_ac = NULL;
    if (lid >= 2) { // Ignore input & output layer
        if (glue->recur_type == RECUR_HEAD) {
            in_ac = layer_updaters[lid]->ac_state + glue->in_offset;
        } else {
            if (layer_updater_activate(layer_updaters[lid]) < 0) {
                ST_WARNING("Failed to layer_activate.[%s]",
                        comp_updater->comp->layers[lid]->name);
                return -1;
            }
            in_ac = layer_updaters[lid]->ac + glue->in_offset;
        }
    }
    if (glue->out_layer == 0) { // output layer
        out_ac = comp_updater->out_updater->ac + glue->out_offset;
    } else {
        out_ac = layer_updaters[glue->out_layer]->ac + glue->out_offset;
    }

    if (glue_updater->impl != NULL
            && glue_updater->impl->forward_util_out != NULL) {
        if (glue_updater->impl->forward_util_out(glue_updater, comp_updater,
                    input_sent, in_ac, out_ac) < 0) {
            ST_WARNING("Failed to glue_updater->impl->forward_util_out.[%s]",
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

    real_t *in_ac;
    real_t *out_ac;
    int lid;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL, -1);

    glue = glue_updater->glue;
    layer_updaters = comp_updater->layer_updaters;

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Forward-out: glue[%s]", glue->name);
#endif

    if (glue_updater->impl != NULL && glue_updater->impl->forward_out != NULL) {
        in_ac = NULL;
        lid = glue->in_layer;
        if (lid >= 2) { // Ignore input & output layer
            if (glue->recur_type == RECUR_HEAD) {
                in_ac = layer_updaters[lid]->ac_state + glue->in_offset;
            } else {
                in_ac = layer_updaters[lid]->ac + glue->in_offset;
            }
        }
        if (glue->out_layer == 0) { // output layer
            out_ac = comp_updater->out_updater->ac + glue->out_offset;
        } else {
            out_ac = layer_updaters[glue->out_layer]->ac + glue->out_offset;
        }
        if (glue_updater->impl->forward_out(glue_updater, comp_updater,
                    node, in_ac, out_ac) < 0) {
            ST_WARNING("Failed to glue_updater->impl->forward_out.[%s]",
                    glue->name);
            return -1;
        }
    }

    return 0;
}

int glue_updater_forward_out_word(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, int word)
{
    glue_t *glue;
    layer_updater_t **layer_updaters;

    real_t *in_ac;
    real_t *out_ac;
    int lid;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL, -1);

    glue = glue_updater->glue;
    layer_updaters = comp_updater->layer_updaters;

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Forward-out-word: glue[%s]", glue->name);
#endif

    if (glue_updater->impl != NULL && glue_updater->impl->forward_out_word != NULL) {
        in_ac = NULL;
        lid = glue->in_layer;
        if (lid >= 2) { // Ignore input & output layer
            if (glue->recur_type == RECUR_HEAD) {
                in_ac = layer_updaters[lid]->ac_state + glue->in_offset;
            } else {
                in_ac = layer_updaters[lid]->ac + glue->in_offset;
            }
        }
        if (glue->out_layer == 0) { // output layer
            out_ac = comp_updater->out_updater->ac + glue->out_offset;
        } else {
            out_ac = layer_updaters[glue->out_layer]->ac + glue->out_offset;
        }
        if (glue_updater->impl->forward_out_word(glue_updater, comp_updater,
                    word, in_ac, out_ac) < 0) {
            ST_WARNING("Failed to glue_updater->impl->forward_out_word.[%s]",
                    glue->name);
            return -1;
        }
    }

    return 0;
}

int glue_updater_init_multicall(glue_updater_t *glue_updater, output_t *output)
{
    ST_CHECK_PARAM(glue_updater == NULL, -1);

    if (glue_updater->impl == NULL || ! glue_updater->impl->multicall) {
        glue_updater->forwarded = NULL;
        return 0;
    }

    glue_updater->forwarded = st_malloc(sizeof(bool) * output->tree->num_node);
    if (glue_updater->forwarded == NULL) {
        ST_WARNING("Failed to st_malloc forwarded.");
        return -1;
    }
    memset(glue_updater->forwarded, 0, sizeof(bool) * output->tree->num_node);

    return 0;
}

int glue_updater_clear_multicall(glue_updater_t *glue_updater,
        output_t *output)
{
    ST_CHECK_PARAM(glue_updater == NULL, -1);

    if (glue_updater->forwarded == NULL) {
        return 0;
    }

    memset(glue_updater->forwarded, 0,
            sizeof(bool) * (output->tree->num_node));

    return 0;
}
