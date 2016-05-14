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

#include <stutils/st_log.h>
#include <stutils/st_utils.h>

#include "sigmoid_layer.h"
#include "tanh_layer.h"
#include "layer.h"

static const int LAYER_MAGIC_NUM = 626140498 + 60;

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

    reg = layer_get_reg(layer->type);
    if (reg != NULL) {
        reg->destroy(layer);
    }
    layer->type[0] = '\0';
}

layer_t* layer_parse_topo(const char *line)
{
    layer_t *layer = NULL;
    layer_reg_t *reg = NULL;

    char keyvalue[2*MAX_LINE_LEN];
    char token[MAX_LINE_LEN];
    char reg_topo[MAX_LINE_LEN];

    const char *p;

    ST_CHECK_PARAM(line == NULL, NULL);

    layer = (layer_t *)malloc(sizeof(layer_t));
    if (layer == NULL) {
        ST_WARNING("Failed to malloc layer_t.");
        goto ERR;
    }
    memset(layer, 0, sizeof(layer_t));

    p = line;

    p = get_next_token(p, token);
    if (strcasecmp("layer", token) != 0) {
        ST_WARNING("Not layer line.");
        goto ERR;
    }

    reg_topo[0] = '\0';
    while (p != NULL) {
        p = get_next_token(p, token);
        if (split_line(token, keyvalue, 2, MAX_LINE_LEN, "=") != 2) {
            ST_WARNING("Failed to split key/value. [%s][%s]", line, token);
            goto ERR;
        }

        if (strcasecmp("name", keyvalue) == 0) {
            if (layer->name[0] != '\0') {
                ST_WARNING("Duplicated name.");
            }
            strncpy(layer->name, keyvalue + MAX_LINE_LEN, MAX_NAME_LEN);
            layer->name[MAX_NAME_LEN - 1] = '\0';
        } else if (strcasecmp("type", keyvalue) == 0) {
            if (layer->type[0] != '\0') {
                ST_WARNING("Duplicated type.");
            }
            strncpy(layer->type, keyvalue + MAX_LINE_LEN, MAX_NAME_LEN);
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
        } else if (strcasecmp("size", keyvalue) == 0) {
            if (layer->size != 0) {
                ST_WARNING("Duplicated size.");
            }
            layer->size = atoi(keyvalue + MAX_LINE_LEN);
            if (layer->size <= 0) {
                ST_WARNING("Invalid layer size[%s].",
                        keyvalue + MAX_LINE_LEN);
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
    strncpy(layer->type, l->type, MAX_NAME_LEN);
    layer->type[MAX_NAME_LEN - 1] = '\0';
    layer->size = l->size;

    layer->forward = l->forward;
    layer->backprop = l->backprop;

    return layer;

ERR:
    safe_layer_destroy(layer);
    return NULL;
}

int layer_load_header(layer_t **layer, int version,
        FILE *fp, bool *binary, FILE *fo_info)
{
    union {
        char str[4];
        int magic_num;
    } flag;

    char name[MAX_NAME_LEN];
    char type[MAX_NAME_LEN];
    int size;

    ST_CHECK_PARAM((layer == NULL && fo_info == NULL) || fp == NULL
            || binary == NULL, -1);

    if (version < 3) {
        ST_WARNING("Too old version of connlm file");
        return -1;
    }

    if (fread(&flag.magic_num, sizeof(int), 1, fp) != 1) {
        ST_WARNING("Failed to load magic num.");
        return -1;
    }

    if (strncmp(flag.str, "    ", 4) == 0) {
        *binary = false;
    } else if (LAYER_MAGIC_NUM != flag.magic_num) {
        ST_WARNING("magic num wrong.");
        return -2;
    } else {
        *binary = true;
    }

    if (layer != NULL) {
        *layer = NULL;
    }

    if (*binary) {
        if (fread(name, sizeof(char), MAX_NAME_LEN, fp) != MAX_NAME_LEN) {
            ST_WARNING("Failed to read name.");
            return -1;
        }
        name[MAX_NAME_LEN - 1] = '\0';
        if (fread(type, sizeof(char), MAX_NAME_LEN, fp) != MAX_NAME_LEN) {
            ST_WARNING("Failed to read type.");
            return -1;
        }
        type[MAX_NAME_LEN - 1] = '\0';
        if (fread(&size, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read size.");
            return -1;
        }
    } else {
        if (st_readline(fp, "") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }
        if (st_readline(fp, "<LAYER>") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }

        if (st_readline(fp, "Name: %"xSTR(MAX_NAME_LEN)"s", name) != 1) {
            ST_WARNING("Failed to parse name.");
            goto ERR;
        }
        name[MAX_NAME_LEN - 1] = '\0';
        if (st_readline(fp, "Type: %"xSTR(MAX_NAME_LEN)"s", type) != 1) {
            ST_WARNING("Failed to parse type.");
            goto ERR;
        }
        type[MAX_NAME_LEN - 1] = '\0';

        if (st_readline(fp, "Size: %d", &size) != 1) {
            ST_WARNING("Failed to parse size.");
            goto ERR;
        }
    }

    if (layer != NULL) {
        *layer = (layer_t *)malloc(sizeof(layer_t));
        if (*layer == NULL) {
            ST_WARNING("Failed to malloc layer_t");
            goto ERR;
        }
        memset(*layer, 0, sizeof(layer_t));

        strcpy((*layer)->name, name);
        strcpy((*layer)->type, type);
        (*layer)->size = size;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<LAYER>\n");
        fprintf(fo_info, "Name: %s\n", name);
        fprintf(fo_info, "Type: %s\n", type);
        fprintf(fo_info, "Size: %d\n", size);
    }

    return 0;

ERR:
    if (layer != NULL) {
        safe_layer_destroy(*layer);
    }
    return -1;
}

int layer_load_body(layer_t *layer, int version, FILE *fp, bool binary)
{
    int n;

    ST_CHECK_PARAM(layer == NULL || fp == NULL, -1);

    if (version < 3) {
        ST_WARNING("Too old version of connlm file");
        return -1;
    }

    if (binary) {
        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read magic num.");
            goto ERR;
        }

        if (n != -LAYER_MAGIC_NUM) {
            ST_WARNING("Magic num error.");
            goto ERR;
        }
    } else {
        if (st_readline(fp, "<LAYER-DATA>") != 0) {
            ST_WARNING("body flag error.");
            goto ERR;
        }
    }

    return 0;
ERR:

    return -1;
}

int layer_save_header(layer_t *layer, FILE *fp, bool binary)
{
    int n;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (binary) {
        if (fwrite(&LAYER_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }
        if (layer == NULL) {
            n = 0;
            if (fwrite(&n, sizeof(int), 1, fp) != 1) {
                ST_WARNING("Failed to write layer size.");
                return -1;
            }
            return 0;
        }

        if (fwrite(layer->name, sizeof(char), MAX_NAME_LEN,
                    fp) != MAX_NAME_LEN) {
            ST_WARNING("Failed to write name.");
            return -1;
        }
        if (fwrite(layer->type, sizeof(char), MAX_NAME_LEN,
                    fp) != MAX_NAME_LEN) {
            ST_WARNING("Failed to write type.");
            return -1;
        }
        if (fwrite(&layer->size, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write size.");
            return -1;
        }
    } else {
        if (fprintf(fp, "    \n<LAYER>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }

        if (fprintf(fp, "Name: %s\n", layer->name) < 0) {
            ST_WARNING("Failed to fprintf name.");
            return -1;
        }
        if (fprintf(fp, "Type: %s\n", layer->type) < 0) {
            ST_WARNING("Failed to fprintf type.");
            return -1;
        }
        if (fprintf(fp, "Size: %d\n", layer->size) < 0) {
            ST_WARNING("Failed to fprintf size.");
            return -1;
        }
    }

    return 0;
}

int layer_save_body(layer_t *layer, FILE *fp, bool binary)
{
    int n;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (layer == NULL) {
        return 0;
    }

    if (binary) {
        n = -LAYER_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

    } else {
        if (fprintf(fp, "<LAYER-DATA>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }
    }

    return 0;
}
