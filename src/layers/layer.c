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

#include <string.h>

#include <st_log.h>
#include <st_utils.h>

#include "sigmoid_layer.h"
#include "tanh_layer.h"
#include "layer.h"

typedef struct _layer_register_t_ {
    char type[MAX_NAME_LEN];
    int (*init)(layer_t *layer);
    void (*destroy)(layer_t *layer);
    int (*dup)(layer_t *dst, layer_t *src);
    int (*parse_topo)(layer_t *layer, const char *line);
} layer_reg_t;

static layer_reg_t LAYER_REG[] = {
    {SIGMOID_NAME, sigmoid_init, sigmoid_destroy, sigmoid_dup, sigmoid_parse_topo},
    {TANH_NAME, tanh_init, tanh_destroy, tanh_dup, tanh_parse_topo},
};


static layer_reg_t* layer_get_reg(const char *type)
{
    int t;

    for (t = 0; t < sizeof(LAYER_REG) / sizeof(LAYER_REG[0]); t++) {
        if (strcasecmp(type, LAYER_REG[t].type) == 0) {
            return LAYER_REG + t;
        } 
    }

    return NULL;
}

void layer_destroy(layer_t *layer)
{
    layer_reg_t *reg;

    if (layer == NULL) {
        return;
    }

    layer->name[0] = '\0';
    layer->id = -1;

    reg = layer_get_reg(layer->type);
    if (reg != NULL) {
        reg->destroy(layer);
    }
    layer->type[0] = '\0';
}

layer_t* layer_parse_topo(const char *line)
{
    layer_t *layer = NULL;
    layer_reg_t *reg;

    char keyvalue[2][MAX_LINE_LEN];
    char token[MAX_LINE_LEN];
    char reg_topo[MAX_LINE_LEN];

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

    reg_topo[0] = '\0';
    while (p != NULL) {
        p = get_next_token(p, token);
        if (split_line(token, keyvalue, 2, "=") != 2) {
            ST_WARNING("Failed to split key/value. [%s][%s]", line, token);
            goto ERR;
        }

        if (strcasecmp("name", keyvalue[0]) == 0) {
            if (layer->name[0] != '\0') {
                ST_WARNING("Duplicated name.");
            }
            strncpy(layer->name, keyvalue[1], MAX_NAME_LEN);
            layer->name[MAX_NAME_LEN - 1] = '\0';
        } else if (strcasecmp("type", keyvalue[0]) == 0) {
            if (layer->type[0] != '\0') {
                ST_WARNING("Duplicated type.");
            }
            strncpy(layer->type, keyvalue[1], MAX_NAME_LEN);
            layer->type[MAX_NAME_LEN - 1] = '\0';

            reg = layer_get_reg(layer->type);
            if (reg == NULL) {
                ST_WARNING("Unkown type of layer [%s].",
                        layer->type);
                goto ERR;
            }
            if (reg->init(layer) < 0) {
                ST_WARNING("Failed to init reg layer.");
                goto ERR;
            }
        } else if (strcasecmp("size", keyvalue[0]) == 0) {
            if (layer->size != 0) {
                ST_WARNING("Duplicated size.");
            }
            layer->size = atoi(keyvalue[1]);
            if (layer->size <= 0) {
                ST_WARNING("Invalid layer size[%s].", keyvalue[1]);
                goto ERR;
            }
        } else {
            strncpy(reg_topo + strlen(reg_topo), token,
                    MAX_LINE_LEN - strlen(reg_topo));
            if (strlen(reg_topo) < MAX_LINE_LEN) {
                reg_topo[strlen(reg_topo)] = ' ';
            }
            reg_topo[MAX_LINE_LEN - 1] = '\0';
        }
    }

    if (layer->name[0] == '\0') {
        ST_WARNING("No layer name found.");
        goto ERR;
    }
    if (reg == NULL) {
        ST_WARNING("No layer type found.");
        goto ERR;
    }
    if (layer->size <= 0) {
        ST_WARNING("No layer size found.");
        goto ERR;
    }
    if (reg->parse_topo(layer, reg_topo) < 0) {
        ST_WARNING("Failed to parse_topo for reg layer.");
        goto ERR;
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

