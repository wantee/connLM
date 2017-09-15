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
#include <assert.h>

#include <stutils/st_log.h>
#include <stutils/st_io.h>
#include <stutils/st_string.h>
#include <stutils/st_rand.h>
#include <stutils/st_mem.h>

#include "weight.h"

static const int WT_MAGIC_NUM = 626140498 + 90;

static const char *init_str[] = {
    "Undefined",
    "Constant",
    "Uniform",
    "Norm",
    "Trunc_Norm",
    "Identity",
};

static const char* init2str(wt_init_type_t m)
{
    return init_str[m];
}

static wt_init_type_t str2init(const char *str)
{
    int i;

    for (i = 0; i < sizeof(init_str) / sizeof(init_str[0]); i++) {
        if (strcasecmp(init_str[i], str) == 0) {
            return (wt_init_type_t)i;
        }
    }

    return WT_INIT_UNKNOWN;
}

void wt_destroy(weight_t *wt)
{
    if (wt == NULL) {
        return;
    }

    mat_destroy(&wt->w);
    vec_destroy(&wt->bias);
}

static int wt_resize(weight_t *wt, size_t row, size_t col)
{
    ST_CHECK_PARAM(wt == NULL || row <= 0 || col <= 0, -1);

    if (mat_resize(&wt->w, row, col, NAN) < 0) {
        ST_WARNING("Failed to mat_resize w.");
        return -1;
    }

    if (! isinf(wt->init_bias)) {
        if (vec_resize(&wt->bias, row, NAN) < 0) {
            ST_WARNING("Failed to vec_resize bias.");
            return -1;
        }
    }

    return 0;
}

weight_t* wt_dup(weight_t *src)
{
    weight_t *dst = NULL;

    ST_CHECK_PARAM(src == NULL, NULL);

    dst = (weight_t *)st_malloc(sizeof(weight_t));
    if (dst == NULL) {
        ST_WARNING("Failed to st_malloc weight_t.");
        return NULL;
    }
    memset(dst, 0, sizeof(weight_t));

    dst->init_type = src->init_type;
    dst->init_param = src->init_param;
    dst->init_bias = src->init_bias;

    if (mat_cpy(&dst->w, &src->w) < 0) {
        ST_WARNING("Failed to mat_cpy.");
        goto ERR;
    }

    if (vec_cpy(&dst->bias, &src->bias) < 0) {
        ST_WARNING("Failed to vec_cpy.");
        goto ERR;
    }

    return dst;

ERR:
    safe_wt_destroy(dst);
    return NULL;
}

int wt_parse_topo(weight_t *wt, char *line, size_t line_len)
{
    char keyvalue[2*MAX_LINE_LEN];
    char token[MAX_LINE_LEN];
    char untouch_topo[MAX_LINE_LEN];

    const char *p;

    ST_CHECK_PARAM(wt == NULL || line == NULL, -1);

    wt->init_type = WT_INIT_UNKNOWN;
    wt->init_param = NAN;
    wt->init_bias = NAN;

    p = line;
    untouch_topo[0] = '\0';
    while (p != NULL) {
        p = get_next_token(p, token);
        if (token[0] == '\0') {
            continue;
        }

        if (split_line(token, keyvalue, 2, MAX_LINE_LEN, "=") != 2) {
            ST_WARNING("Failed to split key/value. [%s]", token);
            goto ERR;
        }

        if (strcasecmp("init", keyvalue) == 0) {
            if (wt->init_type != WT_INIT_UNKNOWN) {
                ST_WARNING("Duplicated init tag");
                goto ERR;
            }
            wt->init_type = str2init(keyvalue + MAX_LINE_LEN);
            if (wt->init_type == WT_INIT_UNKNOWN) {
                ST_WARNING("Unknown init type.");
                goto ERR;
            }
        } else if (strcasecmp("init_param", keyvalue) == 0) {
            if (! isnan(wt->init_param)) {
                ST_WARNING("Duplicated init_param tag");
                goto ERR;
            }
            wt->init_param = atof(keyvalue + MAX_LINE_LEN);
        } else if (strcasecmp("bias", keyvalue) == 0) {
            if (! isnan(wt->init_bias)) {
               ST_WARNING("Duplicated bias tag");
                goto ERR;
            }

            if (strcasecmp(keyvalue + MAX_LINE_LEN, "None") == 0) {
                wt->init_bias = INFINITY;
            } else {
                wt->init_bias = atof(keyvalue + MAX_LINE_LEN);
            }
        } else {
            strncpy(untouch_topo + strlen(untouch_topo), token,
                    MAX_LINE_LEN - strlen(untouch_topo));
            if (strlen(untouch_topo) < MAX_LINE_LEN) {
                untouch_topo[strlen(untouch_topo)] = ' ';
            }
            untouch_topo[MAX_LINE_LEN - 1] = '\0';
        }
    }

    if (wt->init_type == WT_INIT_UNKNOWN) {
        wt->init_type = WT_INIT_UNIFORM;
    }

    if (wt->init_type == WT_INIT_CONST) {
        if (isnan(wt->init_param)) {
            wt->init_param = 0;
        }
        if (wt->init_param != 0.0) {
            ST_WARNING("WT_INIT_CONST should be 0.0.");
            wt->init_param = 0;
        }
    } else if (wt->init_type == WT_INIT_UNIFORM) {
        if (isnan(wt->init_param)) {
            wt->init_param = 0.1;
        }
        if (wt->init_param <= 0) {
            ST_WARNING("Error init_param[%g]", wt->init_param);
            goto ERR;
        }
    } else if (wt->init_type == WT_INIT_NORM) {
        if (isnan(wt->init_param)) {
            wt->init_param = 0.1;
        }
        if (wt->init_param <= 0) {
            ST_WARNING("Error init_param[%g]", wt->init_param);
            goto ERR;
        }
    } else if (wt->init_type == WT_INIT_TRUNC_NORM) {
        if (isnan(wt->init_param)) {
            wt->init_param = 0.1;
        }
        if (wt->init_param <= 0) {
            ST_WARNING("Error init_param[%g]", wt->init_param);
            goto ERR;
        }
    } else if (wt->init_type == WT_INIT_IDENTITY) {
        if (isnan(wt->init_param)) {
            wt->init_param = 1.0;
        }
    }

    if (isnan(wt->init_bias)) {
        wt->init_bias = 0.0;
    }

    strncpy(line, untouch_topo, line_len);
    line[line_len - 1] = '\0';

    return 0;

ERR:
    return -1;
}

int wt_load_header(weight_t **wt, int version,
        FILE *fp, connlm_fmt_t *fmt, FILE *fo_info)
{
    char sym[MAX_LINE_LEN];
    union {
        char str[4];
        int magic_num;
    } flag;
    connlm_fmt_t f;

    int i;
    real_t init_param;
    real_t init_bias = INFINITY;

    ST_CHECK_PARAM((wt == NULL && fo_info == NULL) || fp == NULL
            || fmt == NULL, -1);

    if (version < 20) {
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
    } else if (WT_MAGIC_NUM != flag.magic_num) {
        ST_WARNING("magic num wrong.");
        return -2;
    }

    if (wt != NULL) {
        *wt = NULL;
    }

    if (*fmt != CONN_FMT_TXT) {
        if (fread(fmt, sizeof(connlm_fmt_t), 1, fp) != 1) {
            ST_WARNING("Failed to read fmt.");
            goto ERR;
        }

        if (fread(&i, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read init.");
            goto ERR;
        }
        if (fread(&init_param, sizeof(real_t), 1, fp) != 1) {
            ST_WARNING("Failed to read init_param.");
            goto ERR;
        }
        if (fread(&init_bias, sizeof(real_t), 1, fp) != 1) {
            ST_WARNING("Failed to read init_bias.");
            goto ERR;
        }
    } else {
        if (st_readline(fp, "") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }
        if (st_readline(fp, "<WEIGHT>") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }

        if (st_readline(fp, "Init: %"xSTR(MAX_LINE_LEN)"s", sym) != 1) {
            ST_WARNING("Failed to parse init.");
            goto ERR;
        }
        sym[MAX_LINE_LEN - 1] = '\0';
        i = (int)str2init(sym);
        if (i == (int)WT_INIT_UNKNOWN) {
            ST_WARNING("Unknown init[%s]", sym);
            goto ERR;
        }

        if (st_readline(fp, "Init param: "REAL_FMT, &init_param) != 1) {
            ST_WARNING("Failed to parse init param.");
            goto ERR;
        }

        if (st_readline(fp, "Bias: %"xSTR(MAX_LINE_LEN)"s", sym) != 1) {
            ST_WARNING("Failed to parse init.");
            goto ERR;
        }
        sym[MAX_LINE_LEN - 1] = '\0';
        if (strcasecmp(sym, "none") == 0) {
            init_bias = INFINITY;
        } else {
            init_bias = atof(sym);
        }
    }

    if (wt != NULL) {
        *wt = (weight_t *)st_malloc(sizeof(weight_t));
        if (*wt == NULL) {
            ST_WARNING("Failed to st_malloc weight_t");
            goto ERR;
        }
        memset(*wt, 0, sizeof(weight_t));

        (*wt)->init_type = (wt_init_type_t)i;
        (*wt)->init_param = init_param;
        (*wt)->init_bias = init_bias;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<WEIGHT>\n");
        fprintf(fo_info, "Init: %s\n", init2str((wt_init_type_t)i));
        fprintf(fo_info, "Init param: %g\n", init_param);
        if (isinf(init_bias)) {
            fprintf(fo_info, "Bias : None\n");
        } else {
            fprintf(fo_info, "Bias : %g\n", init_bias);
        }
    }

    if (mat_load_header((wt == NULL) ? NULL : &((*wt)->w),
                version, fp, &f, fo_info) < 0) {
        ST_WARNING("Failed to mat_load_header.");
        return -1;
    }
    if (*fmt != f) {
        ST_WARNING("Multiple formats in one file.");
        return -1;
    }

    if (! isinf(init_bias)) {
        if (vec_load_header((wt == NULL) ? NULL : &((*wt)->bias),
                    version, fp, &f, fo_info) < 0) {
            ST_WARNING("Failed to vec_load_header.");
            return -1;
        }
        if (*fmt != f) {
            ST_WARNING("Multiple formats in one file.");
            return -1;
        }
    }

    return 0;

ERR:
    if (wt != NULL) {
        safe_wt_destroy(*wt);
    }
    return -1;
}

int wt_load_body(weight_t *wt, int version, FILE *fp, connlm_fmt_t fmt)
{
    int n;

    ST_CHECK_PARAM(wt == NULL || fp == NULL, -1);

    if (version < 20) {
        ST_WARNING("Too old version of connlm file");
        return -1;
    }

    if (connlm_fmt_is_bin(fmt)) {
        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read magic num.");
            return -1;
        }

        if (n != -WT_MAGIC_NUM) {
            ST_WARNING("Magic num error.[%d]", n);
            return -1;
        }
    } else {
        if (st_readline(fp, "<WEIGHT-DATA>") != 0) {
            ST_WARNING("body flag error.");
            return -1;
        }
    }

    if (mat_load_body(&wt->w, version, fp, fmt) < 0) {
        ST_WARNING("Failed to mat_load_body w");
        return -1;
    }

    if (! isinf(wt->init_bias)) {
        if (vec_load_body(&wt->bias, version, fp, fmt) < 0) {
            ST_WARNING("Failed to vec_load_body bias");
            return -1;
        }
    }

    return 0;
}

int wt_save_header(weight_t *wt, FILE *fp, connlm_fmt_t fmt)
{
    int n;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (connlm_fmt_is_bin(fmt)) {
        if (fwrite(&WT_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }
        if (fwrite(&fmt, sizeof(connlm_fmt_t), 1, fp) != 1) {
            ST_WARNING("Failed to write fmt.");
            return -1;
        }

        n = (int)wt->init_type;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write init_type.");
            return -1;
        }
        if (fwrite(&wt->init_param, sizeof(real_t), 1, fp) != 1) {
            ST_WARNING("Failed to write init_param.");
            return -1;
        }

        if (fwrite(&wt->init_bias, sizeof(real_t), 1, fp) != 1) {
            ST_WARNING("Failed to write init_bias.");
            return -1;
        }
    } else {
        if (fprintf(fp, "    \n<WEIGHT>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }

        if (fprintf(fp, "Init: %s\n", init2str(wt->init_type)) < 0) {
            ST_WARNING("Failed to fprintf init type.");
            return -1;
        }
        if (fprintf(fp, "Init param: %f\n", wt->init_param) < 0) {
            ST_WARNING("Failed to fprintf init param.");
            return -1;
        }
        if (isinf(wt->init_bias)) {
            if (fprintf(fp, "Bias: None\n") < 0) {
                ST_WARNING("Failed to fprintf init bias.");
                return -1;
            }
        } else {
            if (fprintf(fp, "Bias: %f\n", wt->init_bias) < 0) {
                ST_WARNING("Failed to fprintf init bias.");
                return -1;
            }
        }
    }

    if (mat_save_header(&wt->w, fp, fmt) < 0) {
        ST_WARNING("Failed to mat_save_header.");
        return -1;
    }

    if (! isinf(wt->init_bias)) {
        if (vec_save_header(&wt->bias, fp, fmt) < 0) {
            ST_WARNING("Failed to vec_save_header.");
            return -1;
        }
    }

    return 0;
}

int wt_save_body(weight_t *wt, FILE *fp, connlm_fmt_t fmt, char *name)
{
    int n;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (connlm_fmt_is_bin(fmt)) {
        n = -WT_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

    } else {
        if (fprintf(fp, "<WEIGHT-DATA>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }
    }

    if (mat_save_body(&wt->w, fp, fmt, name) < 0) {
        ST_WARNING("Failed to mat_save_body w");
        return -1;
    }

    if (! isinf(wt->init_bias)) {
        if (vec_save_body(&wt->bias, fp, fmt, NULL) < 0) {
            ST_WARNING("Failed to vec_save_body bias");
            return -1;
        }
    }


    return 0;
}

int wt_init(weight_t *wt, size_t row, size_t col)
{
    size_t i, j;

    ST_CHECK_PARAM(wt == NULL || row <= 0, -1);

    if (wt_resize(wt, row, col) < 0) {
        ST_WARNING("Failed to wt_resize.");
        return -1;
    }

    switch(wt->init_type) {
        case WT_INIT_CONST:
            mat_set(&wt->w, 0.0);
        case WT_INIT_UNIFORM:
            for (i = 0; i < wt->w.num_rows; i++) {
                for (j = 0; j < wt->w.num_cols; j++) {
                    MAT_VAL(&wt->w, i, j) = st_random(-wt->init_param,
                            wt->init_param);
                }
            }
            break;
        case WT_INIT_NORM:
            for (i = 0; i < wt->w.num_rows; i++) {
                for (j = 0; j < wt->w.num_cols; j++) {
                    MAT_VAL(&wt->w, i, j) = st_normrand(0, wt->init_param);
                }
            }
        case WT_INIT_TRUNC_NORM:
            for (i = 0; i < wt->w.num_rows; i++) {
                for (j = 0; j < wt->w.num_cols; j++) {
                    MAT_VAL(&wt->w, i, j) = st_trunc_normrand(0,
                            wt->init_param, 2.0);
                }
            }
            break;
        case WT_INIT_IDENTITY:
            if (wt->w.num_rows != wt->w.num_cols) {
                ST_WARNING("Identity initialization: row and col must be equal.");
                return -1;
            }
            mat_set(&wt->w, 0.0);
            for (i = 0; i < wt->w.num_rows; i++) {
                MAT_VAL(&wt->w, i, i) = wt->init_param;
            }
            break;
        default:
            ST_WARNING("Unknown init type[%d]", wt->init_type);
            return -1;
    }

    if (wt->bias.size > 0) {
        vec_set(&wt->bias, wt->init_bias);
    }

    return 0;
}

void wt_sanity_check(weight_t *wt, const char *name)
{
    size_t i, j, n;

    ST_CHECK_PARAM_VOID(wt == NULL);

    n = 0;
    for (i = 0; i < wt->w.num_rows; i++) {
        for (j = 0; j < wt->w.num_cols; j++) {
            if (MAT_VAL(&wt->w, i, j) >= 100 || MAT_VAL(&wt->w, i, j) <= -100) {
                n++;
            }
        }
    }

    if (n > 0) {
        ST_WARNING("There are %zu extraordinary values in weight of [%s]",
                n, name);
    }

    if (wt->bias.size > 0) {
        n = 0;
        for (i = 0; i < wt->bias.size; i++) {
            if (VEC_VAL(&wt->bias, i) >= 100
                    || VEC_VAL(&wt->bias, i) <= -100) {
                n++;
            }
        }

        if (n > 0) {
            ST_WARNING("There are %zu extraordinary values in bias of [%s]",
                    n, name);
        }
    }
}
