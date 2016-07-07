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

#include "layer_updater.h"

void layer_updater_destroy(layer_updater_t *layer_updater)
{
    if (layer_updater == NULL) {
        return;
    }

    safe_free(layer_updater->ac);
    safe_free(layer_updater->er);

    layer_updater->layer = NULL;
}

layer_updater_t* layer_updater_create(layer_t *layer)
{
    layer_updater_t *layer_updater = NULL;

    ST_CHECK_PARAM(layer == NULL, NULL);

    layer_updater = (layer_updater_t *)malloc(sizeof(layer_updater_t));
    if (layer_updater == NULL) {
        ST_WARNING("Failed to malloc layer_updater.");
        goto ERR;
    }
    memset(layer_updater, 0, sizeof(layer_updater_t));

    layer_updater->layer = layer;

    return layer_updater;

ERR:
    safe_layer_updater_destroy(layer_updater);
    return NULL;
}

int layer_updater_setup(layer_updater_t *layer_updater, bool backprob)
{
    ST_CHECK_PARAM(layer_updater == NULL, -1);

    return 0;
}
