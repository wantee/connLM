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
#include <stutils/st_varint.h>

#include "weight.h"

#define SQ_MULTIPLE 1024.0

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

    safe_st_aligned_free(wt->mat);
    safe_st_aligned_free(wt->bias);
    wt->row = 0;
    wt->col = 0;
}

static int wt_alloc(weight_t *wt, size_t row, size_t col)
{
    ST_CHECK_PARAM(wt == NULL || row <= 0 || col <= 0, -1);

    wt->mat = st_aligned_malloc(row * col * sizeof(real_t), ALIGN_SIZE);
    if (wt->mat == NULL) {
        ST_WARNING("Failed to st_aligned_malloc mat.");
        goto ERR;
    }

    if (! isinf(wt->init_bias)) {
        wt->bias = st_aligned_malloc(row * sizeof(real_t), ALIGN_SIZE);
        if (wt->bias == NULL) {
            ST_WARNING("Failed to st_aligned_malloc bias.");
            goto ERR;
        }
    }

    wt->row = row;
    wt->col = col;

    return 0;

ERR:
    safe_st_aligned_free(wt->mat);
    safe_st_aligned_free(wt->bias);
    wt->row = 0;
    wt->col = 0;
    return -1;
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

    if (wt_alloc(dst, src->row, src->col) < 0) {
        ST_WARNING("Failed to wt_alloc.");
        goto ERR;
    }

    memcpy(dst->mat, src->mat, src->row * src->col * sizeof(real_t));

    if (src->bias != NULL) {
        memcpy(dst->bias, src->bias, sizeof(real_t) * src->row);
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

    size_t row;
    size_t col;
    int i;
    real_t init_param;
    real_t init_bias = INFINITY;

    ST_CHECK_PARAM((wt == NULL && fo_info == NULL) || fp == NULL
            || fmt == NULL, -1);

    if (version < 9) {
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
        if (version >= 12) {
            if (fread(fmt, sizeof(connlm_fmt_t), 1, fp) != 1) {
                ST_WARNING("Failed to read fmt.");
                goto ERR;
            }
        } else {
            *fmt = CONN_FMT_BIN;
        }

        if (fread(&row, sizeof(size_t), 1, fp) != 1) {
            ST_WARNING("Failed to read row.");
            return -1;
        }
        if (fread(&col, sizeof(size_t), 1, fp) != 1) {
            ST_WARNING("Failed to read col.");
            return -1;
        }
        if (fread(&i, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read init.");
            goto ERR;
        }
        if (fread(&init_param, sizeof(real_t), 1, fp) != 1) {
            ST_WARNING("Failed to read init_param.");
            goto ERR;
        }
        if (version >= 8) {
            if (fread(&init_bias, sizeof(real_t), 1, fp) != 1) {
                ST_WARNING("Failed to read init_bias.");
                goto ERR;
            }
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

        if (st_readline(fp, "Row: %zu", &row) != 1) {
            ST_WARNING("Failed to parse row.");
            goto ERR;
        }
        if (st_readline(fp, "Col: %zu", &col) != 1) {
            ST_WARNING("Failed to parse col.");
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

        if (version >= 8) {
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
    }

    if (wt != NULL) {
        *wt = (weight_t *)st_malloc(sizeof(weight_t));
        if (*wt == NULL) {
            ST_WARNING("Failed to st_malloc weight_t");
            goto ERR;
        }
        memset(*wt, 0, sizeof(weight_t));

        (*wt)->row = row;
        (*wt)->col = col;
        (*wt)->init_type = (wt_init_type_t)i;
        (*wt)->init_param = init_param;
        (*wt)->init_bias = init_bias;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<WEIGHT>\n");
        fprintf(fo_info, "Row: %zu\n", row);
        fprintf(fo_info, "Col: %zu\n", col);
        fprintf(fo_info, "Init: %s\n", init2str((wt_init_type_t)i));
        fprintf(fo_info, "Init param: %g\n", init_param);
        if (isinf(init_bias)) {
            fprintf(fo_info, "Bias : None\n");
        } else {
            fprintf(fo_info, "Bias : %g\n", init_bias);
        }
    }

    return 0;

ERR:
    if (wt != NULL) {
        safe_wt_destroy(*wt);
    }
    return -1;
}

static int wt_load_sq_zc(real_t *a, size_t sz, FILE *fp)
{
    size_t i;
    uint64_t num_zeros;
    int16_t si;

    i = 0;
    while (i < sz) {
        if (fread(&si, sizeof(int16_t), 1, fp) != 1) {
            ST_WARNING("Failed to read short.");
            return -1;
        }

        if (si != 0) {
            a[i] = ((real_t)si) / SQ_MULTIPLE;
            ++i;
            continue;
        }

        if (st_varint_decode_stream_uint64(fp, &num_zeros) < 0) {
            ST_WARNING("Failed to st_varint_decode_stream_uint64");
            return -1;
        }

        assert(i + num_zeros <= sz);
        memset(a + i, 0, sizeof(real_t) * num_zeros);
        i += num_zeros;
    }

    return 0;
}

static int wt_load_sq(real_t *a, size_t sz, FILE *fp)
{
    size_t i;
    int16_t si;

    for (i = 0; i < sz; i++) {
        if (fread(&si, sizeof(int16_t), 1, fp) != 1) {
            ST_WARNING("Failed to read short.");
            return -1;
        }

        a[i] = ((real_t)si) / SQ_MULTIPLE;
    }

    return 0;
}

static int wt_load_zc(real_t *a, size_t sz, FILE *fp)
{
    size_t i;
    uint64_t num_zeros;
    real_t r;

    i = 0;
    while (i < sz) {
        if (fread(&r, sizeof(real_t), 1, fp) != 1) {
            ST_WARNING("Failed to read real_t.");
            return -1;
        }

        if (r != 0) {
            a[i] = r;
            ++i;
            continue;
        }

        if (st_varint_decode_stream_uint64(fp, &num_zeros) < 0) {
            ST_WARNING("Failed to st_varint_decode_stream_uint64");
            return -1;
        }

        assert(i + num_zeros <= sz);
        memset(a + i, 0, sizeof(real_t) * num_zeros);
        i += num_zeros;
    }

    return 0;
}

int wt_load_body(weight_t *wt, int version, FILE *fp, connlm_fmt_t fmt)
{
    char name[MAX_NAME_LEN];
    int n;
    size_t i, j;
    size_t sz;

    char *line = NULL;
    size_t line_sz = 0;
    bool err;
    const char *p;
    char token[MAX_NAME_LEN];

    ST_CHECK_PARAM(wt == NULL || fp == NULL, -1);

    if (version < 9) {
        ST_WARNING("Too old version of connlm file");
        return -1;
    }

    if (wt->row <= 0) {
        return 0;
    }

    if (wt_alloc(wt, wt->row, wt->col) < 0) {
        ST_WARNING("Failed to wt_alloc.");
        return -1;
    }

    if (connlm_fmt_is_bin(fmt)) {
        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read magic num.");
            goto ERR;
        }

        if (n != -WT_MAGIC_NUM) {
            ST_WARNING("Magic num error.[%d]", n);
            goto ERR;
        }

        sz = wt->row * wt->col;
        if (fmt == CONN_FMT_BIN) {
            if (fread(wt->mat, sizeof(real_t), sz, fp) != sz) {
                ST_WARNING("Failed to read mat.");
                goto ERR;
            }

            if (wt->bias != NULL) {
                if (fread(wt->bias, sizeof(real_t), wt->row, fp) != wt->row) {
                    ST_WARNING("Failed to read bias.");
                    goto ERR;
                }
            }
        } else if (fmt | CONN_FMT_SHORT_QUANTIZATION) {
            if (fmt | CONN_FMT_ZEROS_COMPRESSED) {
                if (wt_load_sq_zc(wt->mat, sz, fp) < 0) {
                    ST_WARNING("Failed to wt_load_sq_zc for mat.");
                    return -1;
                }
                if (wt->bias != NULL) {
                    if (wt_load_sq_zc(wt->bias, wt->row, fp) < 0) {
                        ST_WARNING("Failed to wt_load_sq_zc for bias.");
                        return -1;
                    }
                }
            } else {
                for (i = 0; i < sz; i++) {
                    if (wt_load_sq(wt->mat, sz, fp) < 0) {
                        ST_WARNING("Failed to wt_load_sq for mat.");
                        return -1;
                    }
                }
                if (wt->bias != NULL) {
                    if (wt_load_sq(wt->bias, wt->row, fp) < 0) {
                        ST_WARNING("Failed to wt_load_sq for bias.");
                        return -1;
                    }
                }
            }
        } else if (fmt | CONN_FMT_ZEROS_COMPRESSED) {
            if (wt_load_zc(wt->mat, sz, fp) < 0) {
                ST_WARNING("Failed to wt_load_sq for mat.");
                return -1;
            }
            if (wt->bias != NULL) {
                if (wt_load_zc(wt->bias, wt->row, fp) < 0) {
                    ST_WARNING("Failed to wt_load_sq for bias.");
                    return -1;
                }
            }
        }
    } else {
        if (st_readline(fp, "<WEIGHT-DATA>") != 0) {
            ST_WARNING("body flag error.");
            goto ERR;
        }
        if (st_readline(fp, "%"xSTR(MAX_NAME_LEN)"s", name) != 1) {
            ST_WARNING("name error.");
            goto ERR;
        }

        for (i = 0; i < wt->row; i++) {
            if (st_fgets(&line, &line_sz, fp, &err) == NULL || err) {
                ST_WARNING("Failed to parse mat[%zu].", i);
            }
            p = line;
            for (j = 0; j < wt->col; j++) {
                p = get_next_token(p, token);
                if (p == NULL
                        || sscanf(token, REAL_FMT, wt->mat+i*wt->col+j) != 1) {
                    ST_WARNING("Failed to parse mat[%zu, %zu].", i, j);
                    goto ERR;
                }
            }
        }
        if (wt->bias != NULL) {
            if (st_fgets(&line, &line_sz, fp, &err) == NULL || err) {
                ST_WARNING("Failed to parse bias.");
            }
            p = line;
            for (i = 0; i < wt->row; i++) {
                p = get_next_token(p, token);
                if (p == NULL || sscanf(token, REAL_FMT, wt->bias + i) != 1) {
                    ST_WARNING("Failed to parse bias[%zu].", i);
                    goto ERR;
                }
            }
        }
    }

    return 0;
ERR:

    wt_destroy(wt);
    return -1;
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

        if (fwrite(&wt->row, sizeof(size_t), 1, fp) != 1) {
            ST_WARNING("Failed to write row.");
            return -1;
        }
        if (fwrite(&wt->col, sizeof(size_t), 1, fp) != 1) {
            ST_WARNING("Failed to write col.");
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

        if (fprintf(fp, "Row: %zu\n", wt->row) < 0) {
            ST_WARNING("Failed to fprintf row.");
            return -1;
        }
        if (fprintf(fp, "Col: %zu\n", wt->col) < 0) {
            ST_WARNING("Failed to fprintf col.");
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
        if (fprintf(fp, "Bias: %f\n", wt->init_bias) < 0) {
            ST_WARNING("Failed to fprintf init bias.");
            return -1;
        }
    }

    return 0;
}

static int write_zeros_int16(uint64_t num_zeros, FILE *fp)
{
    int16_t zero = 0;

    if (fwrite(&zero, sizeof(int16_t), 1, fp) != 1) {
        ST_WARNING("Failed to write zero.");
        return -1;
    }
    if (st_varint_encode_stream_uint64(num_zeros, fp) < 0) {
        ST_WARNING("Failed to st_varint_encode_stream_uint64[%"PRIu64"]",
                num_zeros);
        return -1;
    }

    return 0;
}

static int wt_save_sq_zc(real_t *a, size_t sz, FILE *fp)
{
    uint64_t num_zeros;
    size_t i;
    int16_t si;

    num_zeros = 0;
    for (i = 0; i < sz; i++) {
        si = quantify_int16(a[i], SQ_MULTIPLE);
        if (si == 0) {
            ++num_zeros;
            continue;
        }

        if (num_zeros > 0) {
            if (write_zeros_int16(num_zeros, fp) < 0) {
                ST_WARNING("Failed to write_zeros_int16.");
                return -1;
            }
            num_zeros = 0;
        }

        if (fwrite(&si, sizeof(int16_t), 1, fp) != 1) {
            ST_WARNING("Failed to write short.");
            return -1;
        }
    }
    if (num_zeros > 0) {
        if (write_zeros_int16(num_zeros, fp) < 0) {
            ST_WARNING("Failed to write_zeros_int16.");
            return -1;
        }
    }

    return 0;
}

static int wt_save_sq(real_t *a, size_t sz, FILE *fp)
{
    size_t i;
    int16_t si;

    for (i = 0; i < sz; i++) {
        si = quantify_int16(a[i], SQ_MULTIPLE);
        if (fwrite(&si, sizeof(int16_t), 1, fp) != 1) {
            ST_WARNING("Failed to write short.");
            return -1;
        }
    }
}

static int write_zeros_real(uint64_t num_zeros, FILE *fp)
{
    real_t zero = 0;

    if (fwrite(&zero, sizeof(real_t), 1, fp) != 1) {
        ST_WARNING("Failed to write zero.");
        return -1;
    }
    if (st_varint_encode_stream_uint64(num_zeros, fp) < 0) {
        ST_WARNING("Failed to st_varint_encode_stream_uint64[%"PRIu64"]",
                num_zeros);
        return -1;
    }

    return 0;
}

static int wt_save_zc(real_t *a, size_t sz, FILE *fp)
{
    uint64_t num_zeros;
    size_t i;

    num_zeros = 0;
    for (i = 0; i < sz; i++) {
        if (a[i] == 0.0) {
            ++num_zeros;
            continue;
        }

        if (num_zeros > 0) {
            if (write_zeros_real(num_zeros, fp) < 0) {
                ST_WARNING("Failed to write_zeros_real.");
                return -1;
            }
            num_zeros = 0;
        }

        if (fwrite(a + i, sizeof(real_t), 1, fp) != 1) {
            ST_WARNING("Failed to write real_t.");
            return -1;
        }
    }
    if (num_zeros > 0) {
        if (write_zeros_real(num_zeros, fp) < 0) {
            ST_WARNING("Failed to write_zeros_real.");
            return -1;
        }
    }

    return 0;
}

int wt_save_body(weight_t *wt, FILE *fp, connlm_fmt_t fmt, char *name)
{
    int n;
    size_t i, j;
    size_t sz;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (wt->row <= 0) {
        return 0;
    }

    if (connlm_fmt_is_bin(fmt)) {
        n = -WT_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }
        sz = wt->row * wt->col;

        if (fmt == CONN_FMT_BIN) {
            if (fwrite(wt->mat, sizeof(real_t), sz, fp) != sz) {
                ST_WARNING("Failed to write mat.");
                return -1;
            }
            if (wt->bias != NULL) {
                if (fwrite(wt->bias, sizeof(real_t), wt->row, fp) != wt->row) {
                    ST_WARNING("Failed to write bias.");
                    return -1;
                }
            }
        } else if (fmt | CONN_FMT_SHORT_QUANTIZATION) {
            if (fmt | CONN_FMT_ZEROS_COMPRESSED) {
                if (wt_save_sq_zc(wt->mat, sz, fp) < 0) {
                    ST_WARNING("Failed to wt_save_sq_zc for mat.");
                    return -1;
                }
                if (wt->bias != NULL) {
                    if (wt_save_sq_zc(wt->bias, wt->row, fp) < 0) {
                        ST_WARNING("Failed to wt_save_sq_zc for bias.");
                        return -1;
                    }
                }
            } else {
                if (wt_save_sq(wt->mat, sz, fp) < 0) {
                    ST_WARNING("Failed to wt_save_sq for mat.");
                    return -1;
                }
                if (wt->bias != NULL) {
                    if (wt_save_sq(wt->bias, wt->row, fp) < 0) {
                        ST_WARNING("Failed to wt_save_sq for bias.");
                        return -1;
                    }
                }
            }
        } else if (fmt | CONN_FMT_ZEROS_COMPRESSED) {
            if (wt_save_zc(wt->mat, sz, fp) < 0) {
                ST_WARNING("Failed to wt_save_zc for mat.");
                return -1;
            }
            if (wt->bias != NULL) {
                if (wt_save_zc(wt->bias, wt->row, fp) < 0) {
                    ST_WARNING("Failed to wt_save_zc for bias.");
                    return -1;
                }
            }
        }
    } else {
        if (fprintf(fp, "<WEIGHT-DATA>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }
        if (fprintf(fp, "%s\n", name != NULL ? name : "") < 0) {
            ST_WARNING("Failed to fprintf name.");
            return -1;
        }

        for (i = 0; i < wt->row; i++) {
            for (j = 0; j < wt->col - 1; j++) {
                if (fprintf(fp, REAL_FMT" ", wt->mat[i*wt->col + j]) < 0) {
                    ST_WARNING("Failed to fprintf mat[%zu,%zu].", i, j);
                    return -1;
                }
            }
            if (fprintf(fp, REAL_FMT"\n", wt->mat[i*wt->col + j]) < 0) {
                ST_WARNING("Failed to fprintf mat[%zu/%zu].", i, j);
                return -1;
            }
        }
        if (wt->bias != NULL) {
            for (i = 0; i < wt->row - 1; i++) {
                if (fprintf(fp, REAL_FMT" ", wt->bias[i]) < 0) {
                    ST_WARNING("Failed to fprintf bias[%zu].", i);
                    return -1;
                }
            }
            if (fprintf(fp, REAL_FMT"\n", wt->bias[i]) < 0) {
                ST_WARNING("Failed to fprintf bias[%zu].", i);
                return -1;
            }
        }
    }

    return 0;
}

int wt_init(weight_t *wt, size_t row, size_t col)
{
    size_t i;
    size_t sz;

    ST_CHECK_PARAM(wt == NULL || row <= 0, -1);

    if (wt_alloc(wt, row, col) < 0) {
        ST_WARNING("Failed to wt_alloc.");
        return -1;
    }

    sz = wt->row * wt->col;
    switch(wt->init_type) {
        case WT_INIT_CONST:
            for (i = 0; i < sz; i++) {
                wt->mat[i] = 0;
            }
        case WT_INIT_UNIFORM:
            for (i = 0; i < sz; i++) {
                wt->mat[i] = st_random(-wt->init_param, wt->init_param);
            }
            break;
        case WT_INIT_NORM:
            for (i = 0; i < sz; i++) {
                wt->mat[i] = st_normrand(0, wt->init_param);
            }
        case WT_INIT_TRUNC_NORM:
            for (i = 0; i < sz; i++) {
                wt->mat[i] = st_trunc_normrand(0, wt->init_param, 2.0);
            }
            break;
        case WT_INIT_IDENTITY:
            if (wt->row != wt->col) {
                ST_WARNING("Identity initialization: row and col must be equal.");
                return -1;
            }
            for (i = 0; i < sz; i++) {
                wt->mat[i] = 0;
            }
            for (i = 0; i < wt->row; i++) {
                wt->mat[i * wt->col + i] = wt->init_param;
            }
            break;
        default:
            ST_WARNING("Unknown init type[%d]", wt->init_type);
            return -1;
    }

    if (wt->bias != NULL) {
        for (i = 0; i < wt->row; i++) {
            wt->bias[i] = wt->init_bias;
        }
    }

    return 0;
}

void wt_sanity_check(weight_t *wt, const char *name)
{
    size_t i, n, sz;

    ST_CHECK_PARAM_VOID(wt == NULL);

    sz = wt->row * wt->col;

    n = 0;
    for (i = 0; i < sz; i++) {
        if (wt->mat[i] >= 100 || wt->mat[i] <= -100) {
            n++;
        }
    }

    if (n > 0) {
        ST_WARNING("There are %zu extraordinary values in weight of [%s]",
                n, name);
    }

    if (wt->bias != NULL) {
        n = 0;
        for (i = 0; i < wt->row; i++) {
            if (wt->bias[i] >= 100 || wt->bias[i] <= -100) {
                n++;
            }
        }

        if (n > 0) {
            ST_WARNING("There are %zu extraordinary values in bias of [%s]",
                    n, name);
        }
    }
}
