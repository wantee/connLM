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

#include "layer.h"

void layer_destroy(layer_t *layer)
{
    if (layer == NULL) {
        return;
    }

    layer->name[0] = '\0';
    layer->id = -1;
}

layer_t* layer_parse_topo(const char *line)
{
    layer_t *layer = NULL;

    char keyvalue[2][MAX_LINE_LEN];
    char token[MAX_LINE_LEN];
    char *p;

    ST_CHECK_PARAM(line == NULL, NULL);

    layer = (layer_t *)malloc(sizeof(layer_t));
    if (layer == NULL) {
        ST_WARNING("Failed to malloc layer_t.");
        goto ERR;
    }
    memset(layer, 0, sizeof(layer_t));

    p = (char *)line;

    p = get_next_token(p, token);
    if (strcasecmp("layer", token) != 0) {
        ST_WARNING("Not layer line.");
        goto ERR;
    }

    while (p != NULL) {
        p = get_next_token(p, token);
        if (split_line(token, keyvalue, 2, "=") < 0) {
            ST_WARNING("Failed to split key/value. [%s]", token);
            goto ERR;
        }

        if (strcasecmp("name", keyvalue[0]) == 0) {
            strncpy(layer->name, keyvalue[1], MAX_NAME_LEN);
            layer->name[MAX_NAME_LEN - 1] = '\0';
        } else if (strcasecmp("type", keyvalue[0]) == 0) {
            strncpy(layer->type, keyvalue[1], MAX_NAME_LEN);
            layer->type[MAX_NAME_LEN - 1] = '\0';
        } else if (strcasecmp("size", keyvalue[0]) == 0) {
            layer->size = atoi(keyvalue[1]);
            if (layer->size <= 0) {
                ST_WARNING("Invalid layer size[%s].", keyvalue[1]);
                goto ERR;
            }
        } else {
            ST_WARNING("Unkown key[%s].", keyvalue[0]);
        }
    }

    return layer;

ERR:
    safe_layer_destroy(layer);
    return NULL;
}

layer_t* layer_dup(layer_t *l)
{
    layer_t *layer = NULL;

    ST_CHECK_PARAM(l == NULL, NULL);

    layer = (layer_t *) malloc(sizeof(layer_t));
    if (layer == NULL) {
        ST_WARNING("Falied to malloc layer_t.");
        goto ERR;
    }
    memset(layer, 0, sizeof(layer_t));

    strncpy(layer->name, l->name, MAX_NAME_LEN);
    layer->name[MAX_NAME_LEN - 1] = '\0';
    layer->id = l->id;

    layer->forward = l->forward;
    layer->backprop = l->backprop;

    return layer;

ERR:
    safe_layer_destroy(layer);
    return NULL;
}

