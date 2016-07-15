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

#include "sigmoid_layer.h"

static const int SIGMOID_LAYER_MAGIC_NUM = 626140498 + 61;

#define safe_sigmoid_data_destroy(ptr) do {\
    if((ptr) != NULL) {\
        sigmoid_data_destroy((sigmoid_data_t *)ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)

void sigmoid_data_destroy(sigmoid_data_t *data)
{
    if (data == NULL) {
        return;
    }
    data->steepness = 0.0;
}

sigmoid_data_t* sigmoid_data_init()
{
    sigmoid_data_t *data = NULL;

    data = (sigmoid_data_t *)malloc(sizeof(sigmoid_data_t));
    if (data == NULL) {
        ST_WARNING("Failed to malloc sigmoid_data.");
        goto ERR;
    }
    memset(data, 0, sizeof(sigmoid_data_t));

    data->steepness = 1.0;

    return data;
ERR:
    safe_sigmoid_data_destroy(data);
    return NULL;
}

sigmoid_data_t* sigmoid_data_dup(sigmoid_data_t *src)
{
    sigmoid_data_t *dst = NULL;

    ST_CHECK_PARAM(src == NULL, NULL);

    dst = sigmoid_data_init();
    if (dst == NULL) {
        ST_WARNING("Failed to sigmoid_data_init.");
        goto ERR;
    }

    dst->steepness = src->steepness;

    return dst;
ERR:
    safe_sigmoid_data_destroy(dst);
    return NULL;
}

void sigmoid_destroy(layer_t *layer)
{
    if (layer == NULL) {
        return;
    }

    safe_sigmoid_data_destroy(layer->extra);
}

int sigmoid_init(layer_t *layer)
{
    ST_CHECK_PARAM(layer == NULL, -1);

    if (strcasecmp(layer->type, SIGMOID_NAME) != 0) {
        ST_WARNING("Not a sigmoid layer. [%s]", layer->type);
        return -1;
    }

    layer->extra = (void *)sigmoid_data_init();
    if (layer->extra == NULL) {
        ST_WARNING("Failed to sigmoid_data_init.");
        goto ERR;
    }

    return 0;

ERR:
    safe_sigmoid_data_destroy(layer->extra);
    return -1;
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

    dst->extra = (void *)sigmoid_data_dup((sigmoid_data_t *)src->extra);
    if (dst->extra == NULL) {
        ST_WARNING("Failed to sigmoid_data_dup.");
        goto ERR;
    }

    return 0;

ERR:
    safe_sigmoid_data_destroy(dst->extra);
    return -1;
}

int sigmoid_parse_topo(layer_t *layer, const char *line)
{
    char keyvalue[2*MAX_LINE_LEN];
    char token[MAX_LINE_LEN];

    sigmoid_data_t *data = NULL;
    const char *p;

    ST_CHECK_PARAM(layer == NULL || line == NULL, -1);

    data = (sigmoid_data_t *)layer->extra;

    p = line;

    while (p != NULL) {
        p = get_next_token(p, token);
        if (*token == '\0') {
            break;
        }

        if (split_line(token, keyvalue, 2, MAX_LINE_LEN, "=") != 2) {
            ST_WARNING("Failed to split key/value. [%s]", token);
            goto ERR;
        }

        if (strcasecmp("steepness", keyvalue) == 0) {
            data->steepness = atof(keyvalue + MAX_LINE_LEN);
        } else {
            ST_WARNING("Unknown key/value[%s]", token);
        }
    }

    return 0;

ERR:
    return -1;
}

char* sigmoid_draw_label(layer_t *layer, char *label, size_t label_len)
{
    ST_CHECK_PARAM(layer == NULL || label == NULL || label_len <= 0, NULL);

    label[0] = '\0';

    return label;
}

int sigmoid_load_header(void **extra, int version,
        FILE *fp, bool *binary, FILE *fo_info)
{
    sigmoid_data_t *data = NULL;

    union {
        char str[4];
        int magic_num;
    } flag;

    real_t steepness;

    ST_CHECK_PARAM((extra == NULL && fo_info == NULL) || fp == NULL
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
    } else if (SIGMOID_LAYER_MAGIC_NUM != flag.magic_num) {
        ST_WARNING("magic num wrong.");
        return -2;
    } else {
        *binary = true;
    }

    if (extra != NULL) {
        *extra = NULL;
    }

    if (*binary) {
        if (fread(&steepness, sizeof(real_t), 1, fp) != 1) {
            ST_WARNING("Failed to read steepness.");
            return -1;
        }
    } else {
        if (st_readline(fp, "") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }
        if (st_readline(fp, "<SIGMOID-LAYER>") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }

        if (st_readline(fp, "Stepness: "REAL_FMT, &steepness) != 1) {
            ST_WARNING("Failed to parse steepness.");
            goto ERR;
        }
    }

    if (extra != NULL) {
        data = sigmoid_data_init();
        if (data == NULL) {
            ST_WARNING("Failed to sigmoid_data_init.");
            goto ERR;
        }

        *extra = (void *)data;
        data->steepness = steepness;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<SIGMOID-LAYER>\n");
        fprintf(fo_info, "Stepness: "REAL_FMT"\n", steepness);
    }

    return 0;

ERR:
    if (extra != NULL) {
        safe_sigmoid_data_destroy(*extra);
    }
    return -1;
}

int sigmoid_load_body(void *extra, int version, FILE *fp, bool binary)
{
    int n;

    ST_CHECK_PARAM(extra == NULL || fp == NULL, -1);

    if (version < 3) {
        ST_WARNING("Too old version of connlm file");
        return -1;
    }

    if (binary) {
        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read magic num.");
            goto ERR;
        }

        if (n != -SIGMOID_LAYER_MAGIC_NUM) {
            ST_WARNING("Magic num error.");
            goto ERR;
        }
    } else {
        if (st_readline(fp, "<SIGMOID-LAYER-DATA>") != 0) {
            ST_WARNING("body flag error.");
            goto ERR;
        }
    }

    return 0;
ERR:

    return -1;
}

int sigmoid_save_header(void *extra, FILE *fp, bool binary)
{
    sigmoid_data_t *data;

    ST_CHECK_PARAM(extra == NULL || fp == NULL, -1);

    data = (sigmoid_data_t *)extra;

    if (binary) {
        if (fwrite(&SIGMOID_LAYER_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        if (fwrite(&(data->steepness), sizeof(real_t), 1, fp) != 1) {
            ST_WARNING("Failed to write steepness.");
            return -1;
        }
    } else {
        if (fprintf(fp, "    \n<SIGMOID-LAYER>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }

        if (fprintf(fp, "Stepness: "REAL_FMT"\n", data->steepness) < 0) {
            ST_WARNING("Failed to fprintf steepness.");
            return -1;
        }
    }

    return 0;
}

int sigmoid_save_body(void *extra, FILE *fp, bool binary)
{
    int n;

    ST_CHECK_PARAM(extra == NULL || fp == NULL, -1);

    if (binary) {
        n = -SIGMOID_LAYER_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

    } else {
        if (fprintf(fp, "<SIGMOID-LAYER-DATA>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }
    }

    return 0;
}
