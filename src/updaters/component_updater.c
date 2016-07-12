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

#include "component_updater.h"

void comp_updater_destroy(comp_updater_t *comp_updater)
{
    int i;

    if (comp_updater == NULL) {
        return;
    }

    if (comp_updater->layer_updaters != NULL) {
        for (i = 0; i < comp_updater->comp->num_layer; i++) {
            safe_layer_updater_destroy(comp_updater->layer_updaters[i]);
        }
        safe_free(comp_updater->layer_updaters);
    }

    if (comp_updater->glue_updaters != NULL) {
        for (i = 0; i < comp_updater->comp->num_glue; i++) {
            safe_glue_updater_destroy(comp_updater->glue_updaters[i]);
        }
        safe_free(comp_updater->glue_updaters);
    }

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

    comp_updater = (comp_updater_t *)malloc(sizeof(comp_updater_t));
    if (comp_updater == NULL) {
        ST_WARNING("Failed to malloc comp_updater.");
        goto ERR;
    }
    memset(comp_updater, 0, sizeof(comp_updater_t));

    comp_updater->comp = comp;
    comp_updater->out_updater = out_updater;

    if (comp->num_layer > 0) {
        sz = sizeof(layer_updater_t*)*comp->num_layer;
        comp_updater->layer_updaters = (layer_updater_t **)malloc(sz);
        if (comp_updater->layer_updaters == NULL) {
            ST_WARNING("Failed to malloc layer_updaters.");
            goto ERR;
        }

        for (i = 0; i < comp->num_layer; i++) {
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
        comp_updater->glue_updaters = (glue_updater_t **)malloc(sz);
        if (comp_updater->glue_updaters == NULL) {
            ST_WARNING("Failed to malloc glue_updaters.");
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

int comp_updater_setup(comp_updater_t *comp_updater, bool backprob)
{
    int i;

    ST_CHECK_PARAM(comp_updater == NULL, -1);

    for (i = 0; i < comp_updater->comp->num_layer; i++) {
        if (layer_updater_setup(comp_updater->layer_updaters[i],
                    backprob) < 0) {
            ST_WARNING("Failed to layer_updater_setup[%s].",
                    comp_updater->comp->layers[i]->name);
            return -1;
        }
    }

    for (i = 0; i < comp_updater->comp->num_glue; i++) {
        if (glue_updater_setup(comp_updater->glue_updaters[i],
                    comp_updater, backprob) < 0) {
            ST_WARNING("Failed to glue_updater_setup[%s].",
                    comp_updater->comp->glues[i]->name);
            return -1;
        }
    }
    return 0;
}

int comp_updater_reset(comp_updater_t *comp_updater)
{
    ST_CHECK_PARAM(comp_updater == NULL, -1);

    return 0;
}

int comp_updater_start(comp_updater_t *comp_updater)
{
    ST_CHECK_PARAM(comp_updater == NULL, -1);

    return 0;
}

int comp_updater_forward(comp_updater_t *comp_updater, int *words,
        int n_word, int tgt_pos)
{
    component_t *comp;
    glue_updater_t *glue_updater;
    int g;

    ST_CHECK_PARAM(comp_updater == NULL, -1);

    comp = comp_updater->comp;

#if _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Forward: comp[%s]", comp->name);
#endif

    for (g = 0; g < comp->num_glue; g++) {
        glue_updater = comp_updater->glue_updaters[comp->fwd_order[g]];
        if (glue_updater_forward(glue_updater, comp_updater,
                    words, n_word, tgt_pos) < 0) {
            ST_WARNING("Failed to forward glue[%s].",
                    glue_updater->glue->name);
            return -1;
        }
    }

    return 0;
}

int comp_updater_backprop(comp_updater_t *comp_updater, int *words,
        int n_word, int tgt_pos)
{
    component_t *comp;
    glue_updater_t *glue_updater;
    int g;

    ST_CHECK_PARAM(comp_updater == NULL, -1);

    comp = comp_updater->comp;

#if _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Backprop: comp[%s]", comp->name);
#endif

    for (g = comp->num_glue - 1; g >= 0; g--) {
        glue_updater = comp_updater->glue_updaters[comp->fwd_order[g]];
        if (glue_updater_backprop(glue_updater, comp_updater,
                    words, n_word, tgt_pos) < 0) {
            ST_WARNING("Failed to backprop glue[%s].",
                    glue_updater->glue->name);
            return -1;
        }
    }

    return 0;
}

int comp_updater_end(comp_updater_t *comp_updater)
{
    ST_CHECK_PARAM(comp_updater == NULL, -1);

    return 0;
}

int comp_updater_finish(comp_updater_t *comp_updater)
{
    ST_CHECK_PARAM(comp_updater == NULL, -1);

    return 0;
}
