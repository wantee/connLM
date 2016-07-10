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

#include "../../glues/direct_glue.h"
#include "../../glues/wt_glue.h"
#include "../../glues/emb_wt_glue.h"
#include "../../glues/out_wt_glue.h"

#include "glue_updater.h"
#include "direct_glue_updater.h"
#include "wt_glue_updater.h"
#include "emb_wt_glue_updater.h"
#include "out_wt_glue_updater.h"

static glue_updater_impl_t GLUE_UPDATER_IMPL[] = {
    {DIRECT_GLUE_NAME, direct_glue_updater_init,
        direct_glue_updater_destroy,
        direct_glue_updater_forward},
    {WT_GLUE_NAME, wt_glue_updater_init,
        wt_glue_updater_destroy,
        wt_glue_updater_forward},
    {EMB_WT_GLUE_NAME, emb_wt_glue_updater_init,
        emb_wt_glue_updater_destroy,
        emb_wt_glue_updater_forward},
    {OUT_WT_GLUE_NAME, out_wt_glue_updater_init,
        out_wt_glue_updater_destroy,
        out_wt_glue_updater_forward},
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

    glue_updater->glue = NULL;
}

glue_updater_t* glue_updater_create(glue_t *glue)
{
    glue_updater_t *glue_updater = NULL;

    ST_CHECK_PARAM(glue == NULL, NULL);

    glue_updater = (glue_updater_t *)malloc(sizeof(glue_updater_t));
    if (glue_updater == NULL) {
        ST_WARNING("Failed to malloc glue_updater.");
        goto ERR;
    }
    memset(glue_updater, 0, sizeof(glue_updater_t));

    glue_updater->glue = glue;

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

    return glue_updater;

ERR:
    safe_glue_updater_destroy(glue_updater);
    return NULL;
}

int glue_updater_setup(glue_updater_t *glue_updater, bool backprob)
{
    ST_CHECK_PARAM(glue_updater == NULL, -1);

    return 0;
}

int glue_updater_forward(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, out_updater_t *out_updater)
{
    glue_t *glue;
    layer_updater_t **layer_updaters;
    int l;
    int lid;
    int off;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL, -1);

    glue = glue_updater->glue;

#if _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Forward: glue[%s]", glue->name);
#endif

    layer_updaters = comp_updater->layer_updaters;

    for (l = 0; l < glue->num_in_layer; l++) {
        lid = glue->in_layers[l];
        off = glue->in_offsets[l];
        if (!layer_updaters[lid]->activated) {
            if (layer_updater_activate(layer_updaters[lid], off) < 0) {
                ST_WARNING("Failed to layer_activate.[%s]",
                        comp_updater->comp->layers[lid]->name);
                return -1;
            }

            layer_updaters[lid]->activated = true;
        }
    }

    for (l = 0; l < glue->num_out_layer; l++) {
        lid = glue->out_layers[l];
        off = glue->out_offsets[l];
        if (!layer_updaters[lid]->cleared) {
            if (layer_updater_clear(layer_updaters[lid], off) < 0) {
                ST_WARNING("Failed to layer_clear.[%s]",
                        comp_updater->comp->layers[lid]->name);
                return -1;
            }

            layer_updaters[lid]->cleared = true;
        }
    }

    if (glue_updater->impl != NULL && glue_updater->impl->forward != NULL) {
        if (glue_updater->impl->forward(glue_updater,
                    comp_updater, out_updater) < 0) {
            ST_WARNING("Failed to glue_updater->impl->forward.[%s]",
                    glue->name);
            return -1;
        }
    }

    return 0;
}

int glue_updater_backprop(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, out_updater_t *out_updater)
{
    glue_t *glue;

    ST_CHECK_PARAM(glue_updater == NULL, -1);

    glue = glue_updater->glue;

#if _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Backprop: glue[%s]", glue->name);
#endif

    return 0;
}
