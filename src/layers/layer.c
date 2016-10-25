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
#include <stutils/st_io.h>
#include <stutils/st_string.h>

#include "input.h"
#include "output.h"
#include "linear_layer.h"
#include "sigmoid_layer.h"
#include "tanh_layer.h"
#include "relu_layer.h"
#include "layer.h"

static const int LAYER_MAGIC_NUM = 626140498 + 60;

static layer_impl_t LAYER_IMPL[] = {
    /* pseudo-layers. */
    {INPUT_LAYER_NAME, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL},
    {OUTPUT_LAYER_NAME, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL},

    /* register-layers. */
    {LINEAR_NAME, linear_init, linear_destroy, linear_dup,
        linear_parse_topo, NULL, linear_load_header,
        linear_load_body, linear_save_header, NULL},
    {SIGMOID_NAME, sigmoid_init, sigmoid_destroy, sigmoid_dup,
        sigmoid_parse_topo, NULL, sigmoid_load_header,
        sigmoid_load_body, sigmoid_save_header, NULL},
    {TANH_NAME, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL},
    {RELU_NAME, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL},
};

static layer_impl_t* layer_get_impl(const char *type)
{
    int t;

    for (t = 0; t < sizeof(LAYER_IMPL) / sizeof(LAYER_IMPL[0]); t++) {
        if (strcasecmp(type, LAYER_IMPL[t].type) == 0) {
            return LAYER_IMPL + t;
        }
    }

    return NULL;
}

void layer_destroy(layer_t *layer)
{
    if (layer == NULL) {
        return;
    }

    layer->name[0] = '\0';

    if (layer->impl != NULL && layer->impl->destroy != NULL) {
        layer->impl->destroy(layer);
    }
    layer->type[0] = '\0';
}

layer_t* layer_parse_topo(const char *line)
{
    layer_t *layer = NULL;

    char keyvalue[2*MAX_LINE_LEN];
    char token[MAX_LINE_LEN];
    char impl_topo[MAX_LINE_LEN];

    const char *p;

    ST_CHECK_PARAM(line == NULL, NULL);

    layer = (layer_t *)malloc(sizeof(layer_t));
    if (layer == NULL) {
        ST_WARNING("Failed to malloc layer_t.");
        goto ERR;
    }
    memset(layer, 0, sizeof(layer_t));

    p = get_next_token(line, token);
    if (strcasecmp("layer", token) != 0) {
        ST_WARNING("Not layer line.");
        goto ERR;
    }

    impl_topo[0] = '\0';
    while (p != NULL) {
        p = get_next_token(p, token);
        if (token[0] == '\0') {
            continue;
        }

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

            layer->impl = layer_get_impl(layer->type);
            if (layer->impl == NULL) {
                ST_WARNING("Unknown type of layer [%s].", layer->type);
                goto ERR;
            }
            if (layer->impl->init != NULL) {
                if (layer->impl->init(layer) < 0) {
                    ST_WARNING("Failed to layer->impl->init.");
                    goto ERR;
                }
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
            strncpy(impl_topo + strlen(impl_topo), token,
                    MAX_LINE_LEN - strlen(impl_topo));
            if (strlen(impl_topo) < MAX_LINE_LEN) {
                impl_topo[strlen(impl_topo)] = ' ';
            }
            impl_topo[MAX_LINE_LEN - 1] = '\0';
        }
    }

    if (layer->name[0] == '\0') {
        ST_WARNING("No layer name found.");
        goto ERR;
    }
    if (layer->impl == NULL) {
        ST_WARNING("No layer type found.");
        goto ERR;
    }
    if (layer->size <= 0) {
        ST_WARNING("No layer size found.");
        goto ERR;
    }
    if (layer->impl->parse_topo != NULL) {
        if (layer->impl->parse_topo(layer, impl_topo) < 0) {
            ST_WARNING("Failed to parse_topo for layer->impl layer.");
            goto ERR;
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
    strncpy(layer->type, l->type, MAX_NAME_LEN);
    layer->type[MAX_NAME_LEN - 1] = '\0';
    layer->size = l->size;

    layer->impl = l->impl;
    if (layer->impl != NULL && layer->impl->dup != NULL) {
        if (layer->impl->dup(layer, l) < 0) {
            ST_WARNING("Failed to layer->impl->dup[%s].", layer->name);
            goto ERR;
        }
    }

    return layer;

ERR:
    safe_layer_destroy(layer);
    return NULL;
}

int layer_load_header(layer_t **layer, int version,
        FILE *fp, bool *binary, FILE *fo_info)
{
    layer_impl_t *impl;

    union {
        char str[4];
        int magic_num;
    } flag;

    char name[MAX_NAME_LEN];
    char type[MAX_NAME_LEN];
    int size;
    bool b;

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

    impl = layer_get_impl(type);
    if (impl == NULL) {
        ST_WARNING("Unknown layer type[%s].", type);
        goto ERR;
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
        (*layer)->impl = impl;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<LAYER>:%s\n", name);
        fprintf(fo_info, "Type: %s\n", type);
        fprintf(fo_info, "Size: %d\n", size);
    }

    if (impl->load_header != NULL) {
        if (impl->load_header(layer != NULL ? &((*layer)->extra) : NULL,
                    version, fp, &b, fo_info) < 0) {
            ST_WARNING("Failed to impl->load_header.");
            goto ERR;
        }

        if (b != *binary) {
            ST_WARNING("Tree binary not match with output binary");
            goto ERR;
        }
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

    if (layer->impl->load_body != NULL) {
        if (layer->impl->load_body(layer->extra, version, fp, binary) < 0) {
            ST_WARNING("Failed to glue->impl->load_body.");
            goto ERR;
        }
    }

    return 0;
ERR:

    return -1;
}

int layer_save_header(layer_t *layer, FILE *fp, bool binary)
{
    ST_CHECK_PARAM(layer == NULL || fp == NULL, -1);

    if (binary) {
        if (fwrite(&LAYER_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
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

    if (layer->impl != NULL && layer->impl->save_header != NULL) {
        if (layer->impl->save_header(layer->extra, fp, binary) < 0) {
            ST_WARNING("Failed to glue->impl->save_header.");
            return -1;
        }
    }

    return 0;
}

int layer_save_body(layer_t *layer, FILE *fp, bool binary)
{
    int n;

    ST_CHECK_PARAM(layer == NULL || fp == NULL, -1);

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

    if (layer->impl != NULL && layer->impl->save_body != NULL) {
        if (layer->impl->save_body(layer->extra, fp, binary) < 0) {
            ST_WARNING("Failed to layer->impl->save_body.");
            return -1;
        }
    }

    return 0;
}

char* layer_draw_label(layer_t *layer, char *label, size_t label_len)
{
    char buf[MAX_LINE_LEN];

    ST_CHECK_PARAM(layer == NULL || label == NULL, NULL);

    snprintf(label, label_len, "%s/type=%s,size=%d%s", layer->name,
            layer->type, layer->size,
            layer->impl != NULL && layer->impl->draw_label != NULL
                ? layer->impl->draw_label(layer, buf, MAX_LINE_LEN)
                : "");

    return label;
}
