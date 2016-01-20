/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Wang Jian
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

#include <st_log.h>
#include <st_utils.h>

#include "sigmoid_layer.h"

static int sigmoid_forward(layer_t *layer)
{
    ST_CHECK_PARAM(layer == NULL, -1);

    return 0;
}

static int sigmoid_backprop(layer_t *layer)
{
    ST_CHECK_PARAM(layer == NULL, -1);

    return 0;
}

void sigmoid_destroy(layer_t *layer)
{
    if (layer == NULL) {
        return;
    }

    layer->forward = NULL;
    layer->backprop = NULL;
    layer->extra = NULL;
}

int sigmoid_init(layer_t *layer)
{
    ST_CHECK_PARAM(layer == NULL, -1);

    if (strcasecmp(layer->type, SIGMOID_NAME) != 0) {
        ST_WARNING("Not a sigmoid layer. [%s]", layer->type);
        return -1;
    }

    layer->forward = sigmoid_forward;
    layer->backprop = sigmoid_backprop;
    layer->extra = NULL;

    return 0;
}

int sigmoid_dup(layer_t *dst, layer_t *src)
{
    ST_CHECK_PARAM(dst == NULL || src == NULL, -1);

    if (strcasecmp(dst->type, SIGMOID_NAME) != 0) {
        ST_WARNING("dst is Not a sigmoid layer. [%s]", dst->type);
        return -1;
    }

    if (strcasecmp(src->type, SIGMOID_NAME) != 0) {
        ST_WARNING("src is Not a sigmoid layer. [%s]", src->type);
        return -1;
    }

    dst->forward = src->forward;
    dst->backprop = src->forward;
    dst->extra = src->extra;

    return 0;
}

int sigmoid_parse_topo(layer_t *layer, const char *line)
{
    char *p;

    ST_CHECK_PARAM(layer == NULL || line == NULL, -1);

    p = (char *)line;
    while(*p != '\0') {
        if (*p != ' ' || *p != '\t') {
            ST_WARNING("Sigmoid topo should be empty. [%s]", line);
            return -1;
        }
    }

    return 0;
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
