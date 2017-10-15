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
#include <stutils/st_rand.h>

#include "utils.h"
#include "sigmoid_layer.h"

static const int SIGMOID_LAYER_MAGIC_NUM = 626140498 + 62;

#define safe_sigmoid_data_destroy(ptr) do {\
    if((ptr) != NULL) {\
        sigmoid_data_destroy((sigmoid_data_t *)ptr);\
        safe_st_free(ptr);\
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

    data = (sigmoid_data_t *)st_malloc(sizeof(sigmoid_data_t));
    if (data == NULL) {
        ST_WARNING("Failed to st_malloc sigmoid_data.");
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
        if (token[0] == '\0') {
            continue;
        }

        if (split_line(token, keyvalue, 2, MAX_LINE_LEN, "=") != 2) {
            ST_WARNING("Failed to split key/value. [%s]", token);
            goto ERR;
        }

        if (strcasecmp("steepness", keyvalue) == 0) {
            data->steepness = atof(keyvalue + MAX_LINE_LEN);
            if (data->steepness <= 0) {
                ST_WARNING("Stepness should not be non-positive.");
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

int sigmoid_load_header(void **extra, int version,
        FILE *fp, connlm_fmt_t *fmt, FILE *fo_info)
{
    sigmoid_data_t *data = NULL;

    union {
        char str[4];
        int magic_num;
    } flag;

    real_t steepness;

    ST_CHECK_PARAM((extra == NULL && fo_info == NULL) || fp == NULL
            || fmt == NULL, -1);

    if (version < 3) {
        ST_WARNING("Too old version of connlm file");
        return -1;
    }

    if (fread(&flag.magic_num, sizeof(int), 1, fp) != 1) {
        ST_WARNING("Failed to load magic num.");
        return -1;
    }

    *fmt = CONN_FMT_UNKNOWN;
    if (strncmp(flag.str, "    ", 4) == 0) {
        *fmt = CONN_FMT_TXT;
    } else if (SIGMOID_LAYER_MAGIC_NUM != flag.magic_num) {
        ST_WARNING("magic num wrong.");
        return -2;
    }

    if (extra != NULL) {
        *extra = NULL;
    }

    if (*fmt != CONN_FMT_TXT) {
        if (version >= 12) {
            if (fread(fmt, sizeof(connlm_fmt_t), 1, fp) != 1) {
                ST_WARNING("Failed to read fmt.");
                goto ERR;
            }
        } else {
            *fmt = CONN_FMT_BIN;
        }

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

int sigmoid_load_body(void *extra, int version, FILE *fp, connlm_fmt_t fmt)
{
    int n;

    ST_CHECK_PARAM(extra == NULL || fp == NULL, -1);

    if (version < 3) {
        ST_WARNING("Too old version of connlm file");
        return -1;
    } else if (version >= 4) { // Remove this empty body from version 4
        return 0;
    }

    if (connlm_fmt_is_bin(fmt)) {
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

int sigmoid_save_header(void *extra, FILE *fp, connlm_fmt_t fmt)
{
    sigmoid_data_t *data;

    ST_CHECK_PARAM(extra == NULL || fp == NULL, -1);

    data = (sigmoid_data_t *)extra;

    if (connlm_fmt_is_bin(fmt)) {
        if (fwrite(&SIGMOID_LAYER_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }
        if (fwrite(&fmt, sizeof(connlm_fmt_t), 1, fp) != 1) {
            ST_WARNING("Failed to write fmt.");
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

int sigmoid_activate(layer_t *layer, mat_t *ac)
{
    sigmoid_data_t *param;
    int i, j;

    ST_CHECK_PARAM(layer == NULL || ac == NULL, -1);

    param = (sigmoid_data_t *)layer->extra;

    if (param->steepness != 1.0) {
        for (i = 0; i < ac->num_rows; i++) {
            for (j = 0; j < ac->num_cols; j++) {
                MAT_VAL(ac, i, j) *= param->steepness;
            }
        }
    }

    for (i = 0; i < ac->num_rows; i++) {
        sigmoid(MAT_VALP(ac, i, 0), ac->num_cols);
    }

    return 0;
}

int sigmoid_deriv(layer_t *layer, mat_t *er, mat_t *ac)
{
    sigmoid_data_t *param;
    int i, j;

    ST_CHECK_PARAM(layer == NULL || er == NULL || ac == NULL, -1);

    param = (sigmoid_data_t *)layer->extra;

    if (er->num_rows != ac->num_rows || er->num_cols != ac->num_cols) {
        ST_WARNING("er and ac size not match");
        return -1;
    }

    if (param->steepness == 1.0) {
        for (i = 0; i < er->num_rows; i++) {
            for (j = 0; j < er->num_cols; j++) {
                MAT_VAL(er, i, j) *= (1 - MAT_VAL(ac, i, j)) * MAT_VAL(ac, i, j);
            }
        }
    } else {
        for (i = 0; i < er->num_rows; i++) {
            for (j = 0; j < er->num_cols; j++) {
                MAT_VAL(er, i, j) *= param->steepness
                    * (1 - MAT_VAL(ac, i, j)) * MAT_VAL(ac, i, j);
            }
        }
    }

    return 0;
}

int sigmoid_random_state(layer_t *layer, mat_t *state)
{
    int i, j;

    ST_CHECK_PARAM(layer == NULL || state == NULL, -1);

    for (i = 0; i < state->num_rows; i++) {
        for (j = 0; j < state->num_cols; j++) {
            MAT_VAL(state, i, j) = st_random(0.0, 1.0);
        }
    }

    return 0;
}
