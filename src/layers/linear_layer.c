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

#include "linear_layer.h"

static const int LINEAR_LAYER_MAGIC_NUM = 626140498 + 61;

#define safe_linear_data_destroy(ptr) do {\
    if((ptr) != NULL) {\
        linear_data_destroy((linear_data_t *)ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)

void linear_data_destroy(linear_data_t *data)
{
    if (data == NULL) {
        return;
    }
    data->scale = 0.0;
}

linear_data_t* linear_data_init()
{
    linear_data_t *data = NULL;

    data = (linear_data_t *)malloc(sizeof(linear_data_t));
    if (data == NULL) {
        ST_WARNING("Failed to malloc linear_data.");
        goto ERR;
    }
    memset(data, 0, sizeof(linear_data_t));

    data->scale = 1.0;

    return data;
ERR:
    safe_linear_data_destroy(data);
    return NULL;
}

linear_data_t* linear_data_dup(linear_data_t *src)
{
    linear_data_t *dst = NULL;

    ST_CHECK_PARAM(src == NULL, NULL);

    dst = linear_data_init();
    if (dst == NULL) {
        ST_WARNING("Failed to linear_data_init.");
        goto ERR;
    }

    dst->scale = src->scale;

    return dst;
ERR:
    safe_linear_data_destroy(dst);
    return NULL;
}

void linear_destroy(layer_t *layer)
{
    if (layer == NULL) {
        return;
    }

    safe_linear_data_destroy(layer->extra);
}

int linear_init(layer_t *layer)
{
    ST_CHECK_PARAM(layer == NULL, -1);

    if (strcasecmp(layer->type, LINEAR_NAME) != 0) {
        ST_WARNING("Not a linear layer. [%s]", layer->type);
        return -1;
    }

    layer->extra = (void *)linear_data_init();
    if (layer->extra == NULL) {
        ST_WARNING("Failed to linear_data_init.");
        goto ERR;
    }

    return 0;

ERR:
    safe_linear_data_destroy(layer->extra);
    return -1;
}

int linear_dup(layer_t *dst, layer_t *src)
{
    ST_CHECK_PARAM(dst == NULL || src == NULL, -1);

    if (strcasecmp(dst->type, LINEAR_NAME) != 0) {
        ST_WARNING("dst is Not a linear layer. [%s]", dst->type);
        return -1;
    }

    if (strcasecmp(src->type, LINEAR_NAME) != 0) {
        ST_WARNING("src is Not a linear layer. [%s]", src->type);
        return -1;
    }

    dst->extra = (void *)linear_data_dup((linear_data_t *)src->extra);
    if (dst->extra == NULL) {
        ST_WARNING("Failed to linear_data_dup.");
        goto ERR;
    }

    return 0;

ERR:
    safe_linear_data_destroy(dst->extra);
    return -1;
}

int linear_parse_topo(layer_t *layer, const char *line)
{
    char keyvalue[2*MAX_LINE_LEN];
    char token[MAX_LINE_LEN];

    linear_data_t *data = NULL;
    const char *p;

    ST_CHECK_PARAM(layer == NULL || line == NULL, -1);

    data = (linear_data_t *)layer->extra;

    p = line;

    while (p != NULL) {
        p = get_next_token(p, token);
        if (token[0] == '\0') {
            continue;
        }

        if (split_line(token, keyvalue, 2, MAX_LINE_LEN, "=") != 2) {
            ST_WARNING("Failed to split key/value. [%s]", token);
            goto ERR;
        }

        if (strcasecmp("scale", keyvalue) == 0) {
            data->scale = atof(keyvalue + MAX_LINE_LEN);
            if (data->scale == 0) {
                ST_WARNING("Scale should not be zero.");
                goto ERR;
            }
        } else {
            ST_WARNING("Unknown key/value[%s]", token);
        }
    }

    return 0;

ERR:
    return -1;
}

int linear_load_header(void **extra, int version,
        FILE *fp, bool *binary, FILE *fo_info)
{
    linear_data_t *data = NULL;

    union {
        char str[4];
        int magic_num;
    } flag;

    real_t scale;

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
    } else if (LINEAR_LAYER_MAGIC_NUM != flag.magic_num) {
        ST_WARNING("magic num wrong.");
        return -2;
    } else {
        *binary = true;
    }

    if (extra != NULL) {
        *extra = NULL;
    }

    if (*binary) {
        if (fread(&scale, sizeof(real_t), 1, fp) != 1) {
            ST_WARNING("Failed to read scale.");
            return -1;
        }
    } else {
        if (st_readline(fp, "") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }
        if (st_readline(fp, "<LINEAR-LAYER>") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }

        if (st_readline(fp, "Scale: "REAL_FMT, &scale) != 1) {
            ST_WARNING("Failed to parse scale.");
            goto ERR;
        }
    }

    if (extra != NULL) {
        data = linear_data_init();
        if (data == NULL) {
            ST_WARNING("Failed to linear_data_init.");
            goto ERR;
        }

        *extra = (void *)data;
        data->scale = scale;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<LINEAR-LAYER>\n");
        fprintf(fo_info, "Scale: "REAL_FMT"\n", scale);
    }

    return 0;

ERR:
    if (extra != NULL) {
        safe_linear_data_destroy(*extra);
    }
    return -1;
}

int linear_load_body(void *extra, int version, FILE *fp, bool binary)
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

        if (n != -LINEAR_LAYER_MAGIC_NUM) {
            ST_WARNING("Magic num error.");
            goto ERR;
        }
    } else {
        if (st_readline(fp, "<LINEAR-LAYER-DATA>") != 0) {
            ST_WARNING("body flag error.");
            goto ERR;
        }
    }

    return 0;
ERR:

    return -1;
}

int linear_save_header(void *extra, FILE *fp, bool binary)
{
    linear_data_t *data;

    ST_CHECK_PARAM(extra == NULL || fp == NULL, -1);

    data = (linear_data_t *)extra;

    if (binary) {
        if (fwrite(&LINEAR_LAYER_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        if (fwrite(&(data->scale), sizeof(real_t), 1, fp) != 1) {
            ST_WARNING("Failed to write scale.");
            return -1;
        }
    } else {
        if (fprintf(fp, "    \n<LINEAR-LAYER>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }

        if (fprintf(fp, "Scale: "REAL_FMT"\n", data->scale) < 0) {
            ST_WARNING("Failed to fprintf scale.");
            return -1;
        }
    }

    return 0;
}

int linear_save_body(void *extra, FILE *fp, bool binary)
{
    int n;

    ST_CHECK_PARAM(extra == NULL || fp == NULL, -1);

    if (binary) {
        n = -LINEAR_LAYER_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

    } else {
        if (fprintf(fp, "<LINEAR-LAYER-DATA>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }
    }

    return 0;
}

int linear_activate(layer_t *layer, real_t *vec, int size)
{
    linear_data_t *param;
    int i;

    ST_CHECK_PARAM(layer == NULL || vec == NULL, -1);

    param = (linear_data_t *)layer->extra;

    if (param->scale != 1.0) {
        for (i = 0; i < size; i++) {
            vec[i] *= param->scale;
        }
    }

    return 0;
}

int linear_deriv(layer_t *layer, real_t *er, real_t *ac, int size)
{
    linear_data_t *param;
    int i;

    ST_CHECK_PARAM(layer == NULL || er == NULL || ac == NULL, -1);

    param = (linear_data_t *)layer->extra;

    if (param->scale != 1.0) {
        for (i = 0; i < size; i++) {
            er[i] *= param->scale;
        }
    }

    return 0;
}
