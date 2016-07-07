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
    int l;

    if (comp_updater == NULL) {
        return;
    }

    if (comp_updater->layer_updaters != NULL) {
        for (l = 0; l < comp_updater->comp->num_layer; l++) {
            safe_layer_updater_destroy(comp_updater->layer_updaters[l]);
        }
        safe_free(comp_updater->layer_updaters);
    }

    comp_updater->comp = NULL;
}

comp_updater_t* comp_updater_create(component_t *comp)
{
    comp_updater_t *comp_updater = NULL;

    size_t sz;
    int l;

    ST_CHECK_PARAM(comp == NULL, NULL);

    comp_updater = (comp_updater_t *)malloc(sizeof(comp_updater_t));
    if (comp_updater == NULL) {
        ST_WARNING("Failed to malloc comp_updater.");
        goto ERR;
    }
    memset(comp_updater, 0, sizeof(comp_updater_t));

    comp_updater->comp = comp;

    if (comp->num_layer > 0) {
        sz = sizeof(layer_updater_t*)*comp->num_layer;
        comp_updater->layer_updaters = (layer_updater_t **)malloc(sz);
        if (comp_updater->layer_updaters == NULL) {
            ST_WARNING("Failed to malloc layer_updaters.");
            goto ERR;
        }

        for (l = 0; l < comp->num_layer; l++) {
            comp_updater->layer_updaters[l] = layer_updater_create(comp->layers[l]);
            if (comp_updater->layer_updaters[l] == NULL) {
                ST_WARNING("Failed to layer_updater_create[%s].",
                        comp->layers[l]->name);
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
    int l;

    ST_CHECK_PARAM(comp_updater == NULL, -1);

    for (l = 0; l < comp_updater->comp->num_layer; l++) {
        if (layer_updater_setup(comp_updater->layer_updaters[l], backprob) < 0) {
            ST_WARNING("Failed to layer_updater_setup[%s].",
                    comp_updater->comp->layers[l]->name);
            return -1;
        }
    }

    return 0;
}

int comp_updater_step(comp_updater_t *comp_updater)
{
    ST_CHECK_PARAM(comp_updater == NULL, -1);

    return 0;
}
