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

#include <stutils/st_macro.h>
#include <stutils/st_log.h>
#include <stutils/st_io.h>
#include <stutils/st_string.h>

#include "direct_glue.h"

static const int DIRECT_GLUE_MAGIC_NUM = 626140498 + 71;

#define safe_direct_glue_data_destroy(ptr) do {\
    if((ptr) != NULL) {\
        direct_glue_data_destroy((direct_glue_data_t *)ptr);\
        safe_st_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)

static void direct_glue_data_destroy(direct_glue_data_t *data)
{
    if (data == NULL) {
        return;
    }

    data->hash_sz = 0;
}

static direct_glue_data_t* direct_glue_data_init()
{
    direct_glue_data_t *data = NULL;

    data = (direct_glue_data_t *)st_malloc(sizeof(direct_glue_data_t));
    if (data == NULL) {
        ST_ERROR("Failed to st_malloc direct_glue_data.");
        goto ERR;
    }
    memset(data, 0, sizeof(direct_glue_data_t));

    return data;
ERR:
    safe_direct_glue_data_destroy(data);
    return NULL;
}

static direct_glue_data_t* direct_glue_data_dup(direct_glue_data_t *src)
{
    direct_glue_data_t *dst = NULL;

    ST_CHECK_PARAM(src == NULL, NULL);

    dst = direct_glue_data_init();
    if (dst == NULL) {
        ST_ERROR("Failed to direct_glue_data_init.");
        goto ERR;
    }
    dst->hash_sz = ((direct_glue_data_t *)src)->hash_sz;

    return (void *)dst;
ERR:
    safe_direct_glue_data_destroy(dst);
    return NULL;
}

void direct_glue_destroy(glue_t *glue)
{
    if (glue == NULL) {
        return;
    }

    safe_direct_glue_data_destroy(glue->extra);
}

int direct_glue_init(glue_t *glue)
{
    ST_CHECK_PARAM(glue == NULL, -1);

    if (strcasecmp(glue->type, DIRECT_GLUE_NAME) != 0) {
        ST_ERROR("Not a sum glue. [%s]", glue->type);
        return -1;
    }

    glue->extra = (void *)direct_glue_data_init();
    if (glue->extra == NULL) {
        ST_ERROR("Failed to direct_glue_data_init.");
        goto ERR;
    }

    return 0;

ERR:
    safe_direct_glue_data_destroy(glue->extra);
    return -1;
}

int direct_glue_dup(glue_t *dst, glue_t *src)
{
    ST_CHECK_PARAM(dst == NULL || src == NULL, -1);

    if (strcasecmp(dst->type, DIRECT_GLUE_NAME) != 0) {
        ST_ERROR("dst is Not a direct glue. [%s]", dst->type);
        return -1;
    }

    if (strcasecmp(src->type, DIRECT_GLUE_NAME) != 0) {
        ST_ERROR("src is Not a direct glue. [%s]", src->type);
        return -1;
    }

    dst->extra = (void *)direct_glue_data_dup((direct_glue_data_t *)src->extra);
    if (dst->extra == NULL) {
        ST_ERROR("Failed to direct_glue_data_dup.");
        goto ERR;
    }

    return 0;

ERR:
    safe_direct_glue_data_destroy(dst->extra);
    return -1;
}

int direct_glue_parse_topo(glue_t *glue, const char *line)
{
    char keyvalue[2*MAX_LINE_LEN];
    char token[MAX_LINE_LEN];

    direct_glue_data_t *data;
    const char *p;

    ST_CHECK_PARAM(glue == NULL || line == NULL, -1);

    data = (direct_glue_data_t *)glue->extra;
    p = line;

    while (p != NULL) {
        p = get_next_token(p, token);
        if (token[0] == '\0') {
            continue;
        }

        if (split_line(token, keyvalue, 2, MAX_LINE_LEN, "=") != 2) {
            ST_ERROR("Failed to split key/value. [%s]", token);
            goto ERR;
        }

        if (strcasecmp("size", keyvalue) == 0) {
            data->hash_sz = st_str2ll(keyvalue + MAX_LINE_LEN);
            if (data->hash_sz <= 0) {
                ST_ERROR("Illegal size[%s]", keyvalue + MAX_LINE_LEN);
                goto ERR;
            }
        } else {
            ST_ERROR("Unknown key/value[%s]", token);
        }
    }

    if (data->hash_sz <= 0) {
        ST_ERROR("Hash size not set");
        goto ERR;
    }

    return 0;

ERR:
    return -1;
}

bool direct_glue_check(glue_t *glue, layer_t **layers, int n_layer,
        input_t *input, output_t *output)
{
    direct_glue_data_t *data;
    int i;

    ST_CHECK_PARAM(glue == NULL || input == NULL, false);

    data = (direct_glue_data_t *)glue->extra;

    if (strcasecmp(glue->type, DIRECT_GLUE_NAME) != 0) {
        ST_ERROR("Not a direct glue. [%s]", glue->type);
        return false;
    }

    if (strcasecmp(layers[glue->in_layer]->type, INPUT_LAYER_NAME) != 0) {
        ST_ERROR("direct_wt glue: in layer should be input layer.");
        return false;
    }

    if (strcasecmp(layers[glue->out_layer]->type, OUTPUT_LAYER_NAME) != 0) {
        ST_ERROR("direct_wt glue: out layer should be output layer.");
        return false;
    }

    if (data->hash_sz <= output->tree->num_node) {
        ST_ERROR("Hash size should NOT less than output tree nodes.");
        return false;
    }

    for (i = 0; i < glue->num_wts; i++) {
        if (glue->wts[i]->init_type != WT_INIT_CONST) {
            glue->wts[i]->init_type = WT_INIT_CONST;
            glue->wts[i]->init_param = 0;
        }

        glue->wts[i]->init_bias = INFINITY; // direct weight will not have bias
    }

    return true;
}

char* direct_glue_draw_label(glue_t *glue, char *label, size_t label_len)
{
    char buf[MAX_NAME_LEN];
    direct_glue_data_t *data;

    ST_CHECK_PARAM(glue == NULL || label == NULL, NULL);

    data = (direct_glue_data_t *)glue->extra;

    snprintf(label, label_len, ",size=%s",
            st_ll2str(buf, MAX_NAME_LEN, (long long)data->hash_sz, false));

    return label;
}

int direct_glue_load_header(void **extra, int version,
        FILE *fp, connlm_fmt_t *fmt, FILE *fo_info)
{
    direct_glue_data_t *data = NULL;

    union {
        char str[4];
        int magic_num;
    } flag;

    size_t hash_sz;

    ST_CHECK_PARAM((extra == NULL && fo_info == NULL) || fp == NULL
            || fmt == NULL, -1);

    if (version < 9) {
        ST_ERROR("Too old version of connlm file");
        return -1;
    }

    if (fread(&flag.magic_num, sizeof(int), 1, fp) != 1) {
        ST_ERROR("Failed to load magic num.");
        return -1;
    }

    *fmt = CONN_FMT_UNKNOWN;
    if (strncmp(flag.str, "    ", 4) == 0) {
        *fmt = CONN_FMT_TXT;
    } else if (DIRECT_GLUE_MAGIC_NUM != flag.magic_num) {
        ST_ERROR("magic num wrong.");
        return -2;
    }

    if (extra != NULL) {
        *extra = NULL;
    }

    if (*fmt != CONN_FMT_TXT) {
        if (version >= 12) {
            if (fread(fmt, sizeof(connlm_fmt_t), 1, fp) != 1) {
                ST_ERROR("Failed to read fmt.");
                goto ERR;
            }
        } else {
            *fmt = CONN_FMT_BIN;
        }

        if (fread(&hash_sz, sizeof(size_t), 1, fp) != 1) {
            ST_ERROR("Failed to read hash_sz.");
            return -1;
        }
    } else {
        if (st_readline(fp, "") != 0) {
            ST_ERROR("tag error.");
            goto ERR;
        }
        if (st_readline(fp, "<DIRECT-GLUE>") != 0) {
            ST_ERROR("tag error.");
            goto ERR;
        }

        if (st_readline(fp, "Hash size: %zu", &hash_sz) != 1) {
            ST_ERROR("Failed to parse hash_sz.");
            goto ERR;
        }
    }

    if (extra != NULL) {
        data = direct_glue_data_init();
        if (data == NULL) {
            ST_ERROR("Failed to direct_glue_data_init.");
            goto ERR;
        }

        *extra = (void *)data;
        data->hash_sz = hash_sz;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<DIRECT-GLUE>\n");
        fprintf(fo_info, "Hash size: %zu\n", hash_sz);
    }

    return 0;

ERR:
    if (extra != NULL) {
        safe_direct_glue_data_destroy(*extra);
    }
    return -1;
}

int direct_glue_save_header(void *extra, FILE *fp, connlm_fmt_t fmt)
{
    direct_glue_data_t *data;

    ST_CHECK_PARAM(extra == NULL || fp == NULL, -1);

    data = (direct_glue_data_t *)extra;

    if (connlm_fmt_is_bin(fmt)) {
        if (fwrite(&DIRECT_GLUE_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to write magic num.");
            return -1;
        }
        if (fwrite(&fmt, sizeof(connlm_fmt_t), 1, fp) != 1) {
            ST_ERROR("Failed to write fmt.");
            return -1;
        }

        if (fwrite(&(data->hash_sz), sizeof(size_t), 1, fp) != 1) {
            ST_ERROR("Failed to write hash_sz.");
            return -1;
        }
    } else {
        if (fprintf(fp, "    \n<DIRECT-GLUE>\n") < 0) {
            ST_ERROR("Failed to fprintf header.");
            return -1;
        }

        if (fprintf(fp, "Hash size: %zu\n", data->hash_sz) < 0) {
            ST_ERROR("Failed to fprintf hash_sz.");
            return -1;
        }
    }

    return 0;
}

int direct_glue_init_data(glue_t *glue, input_t *input,
        layer_t **layers, output_t *output)
{
    direct_glue_data_t *data;
    int i;

    ST_CHECK_PARAM(glue == NULL || glue->wts == NULL
            || input == NULL || output == NULL, -1);

    if (strcasecmp(glue->type, DIRECT_GLUE_NAME) != 0) {
        ST_ERROR("Not a direct glue. [%s]", glue->type);
        return -1;
    }

    data = (direct_glue_data_t *)glue->extra;

    for (i = 0; i < glue->num_wts; i++) {
        if (wt_init(glue->wts[i], 1, data->hash_sz) < 0) {
            ST_ERROR("Failed to wt_init[%d].", i);
            return -1;
        }
    }

    return 0;
}

static float direct_glue_load_factor(glue_t *glue)
{
    size_t j;
    size_t n, total;
    int i;

    ST_CHECK_PARAM(glue == NULL, 0.0);

    n = 0;
    total = 0;
    for (i = 0; i < glue->num_wts; i++) {
        for (j = 0; j < glue->wts[i]->w.num_rows; j++) {
            if (MAT_VAL(&glue->wts[i]->w, j, 0) != 0) {
                ++n;
            }
        }
        total += glue->wts[i]->w.num_rows;
    }

    return n / (float)total;
}

void direct_glue_print_verbose_info(glue_t *glue, FILE *fo)
{
    ST_CHECK_PARAM_VOID(glue == NULL || fo == NULL);

    fprintf(fo, "<DIRECT_GLUE>: %s\n", glue->name);
    fprintf(fo, "Load factor: %.3f\n", direct_glue_load_factor(glue));
}
