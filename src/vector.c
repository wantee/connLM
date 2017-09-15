/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Wang Jian
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

#include <stdlib.h>

#include <stutils/st_macro.h>
#include <stutils/st_log.h>
#include <stutils/st_mem.h>
#include <stutils/st_int.h>
#include <stutils/st_io.h>
#include <stutils/st_string.h>

#include "vector.h"

static const int VEC_MAGIC_NUM = 626140498 + 81;

int svec_load_header(svec_t *vec, int version,
        FILE *fp, connlm_fmt_t *fmt, FILE *fo_info)
{
    union {
        char str[4];
        int magic_num;
    } flag;

    size_t size;

    ST_CHECK_PARAM((vec == NULL && fo_info == NULL) || fp == NULL
            || fmt == NULL, -1);

    if (fread(&flag.magic_num, sizeof(int), 1, fp) != 1) {
        ST_WARNING("Failed to load magic num.");
        return -1;
    }

    *fmt = CONN_FMT_UNKNOWN;
    if (strncmp(flag.str, "    ", 4) == 0) {
        *fmt = CONN_FMT_TXT;
    } else if (VEC_MAGIC_NUM != flag.magic_num) {
        ST_WARNING("magic num wrong.");
        return -2;
    }

    if (vec != NULL) {
        memset(vec, 0, sizeof(svec_t));
    }

    if (*fmt != CONN_FMT_TXT) {
        if (fread(fmt, sizeof(connlm_fmt_t), 1, fp) != 1) {
            ST_WARNING("Failed to read fmt.");
            goto ERR;
        }

        if (fread(&size, sizeof(size_t), 1, fp) != 1) {
            ST_WARNING("Failed to read size.");
            goto ERR;
        }
    } else {
        if (st_readline(fp, "") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }
        if (st_readline(fp, "<VECTOR>") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }

        if (st_readline(fp, "Size: %zu", &size) != 1) {
            ST_WARNING("Failed to parse size.");
            goto ERR;
        }
    }

    if (vec != NULL) {
        if (svec_resize(vec, size, NAN) < 0) {
            ST_WARNING("Failed to svec_resize.");
            goto ERR;
        }
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<VECTOR>\n");
        fprintf(fo_info, "Size: %zu\n", size);
    }

    return 0;

ERR:
    if (vec != NULL) {
        svec_destroy(vec);
    }
    return -1;
}

int svec_load_body(svec_t *vec, int version, FILE *fp, connlm_fmt_t fmt)
{
    char name[MAX_NAME_LEN];
    int n;
    size_t i;

    char *line = NULL;
    size_t line_sz = 0;
    bool err;
    const char *p;
    char token[MAX_NAME_LEN];

    ST_CHECK_PARAM(vec == NULL || fp == NULL, -1);

    if (connlm_fmt_is_bin(fmt)) {
        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read magic num.");
            goto ERR;
        }

        if (n != -VEC_MAGIC_NUM) {
            ST_WARNING("Magic num error.[%d]", n);
            goto ERR;
        }

        if (fmt == CONN_FMT_BIN) {
            if (fread(VEC_VALP(vec, 0), sizeof(real_t),
                        vec->size, fp) != vec->size) {
                ST_WARNING("Failed to read vec.");
                goto ERR;
            }
        } else if (fmt & CONN_FMT_SHORT_QUANTIZATION) {
            if (fmt & CONN_FMT_ZEROS_COMPRESSED) {
                if (load_sq_zc(VEC_VALP(vec, 0), vec->size, fp) < 0) {
                    ST_WARNING("Failed to load_sq_zc for vec.");
                    return -1;
                }
            } else {
                if (load_sq(VEC_VALP(vec, 0), vec->size, fp) < 0) {
                    ST_WARNING("Failed to load_sq for vec.");
                    return -1;
                }
            }
        } else if (fmt & CONN_FMT_ZEROS_COMPRESSED) {
            if (load_zc(VEC_VALP(vec, 0), vec->size, fp) < 0) {
                ST_WARNING("Failed to load_zc for vec.");
                return -1;
            }
        }
    } else {
        if (st_readline(fp, "<VECTOR-DATA>") != 0) {
            ST_WARNING("body flag error.");
            goto ERR;
        }
        if (st_readline(fp, "%"xSTR(MAX_NAME_LEN)"s", name) != 1) {
            ST_WARNING("name error.");
            goto ERR;
        }

        if (st_fgets(&line, &line_sz, fp, &err) == NULL || err) {
            ST_WARNING("Failed to parse vec.");
        }
        p = get_next_token(line, token);
        if (p == NULL || ! (p[0] == '[' && p[1] == '\0')) {
            ST_WARNING("'[' expected.");
            goto ERR;
        }
        for (i = 0; i < vec->size; i++) {
            p = get_next_token(p, token);
            if (p == NULL || sscanf(token, REAL_FMT,
                        VEC_VALP(vec, i)) != 1) {
                ST_WARNING("Failed to parse elem[%zu].", i);
                goto ERR;
            }
        }
        p = get_next_token(p, token);
        if (p == NULL || ! (p[0] == ']' && p[1] == '\0')) {
            ST_WARNING("']' expected.");
            goto ERR;
        }
    }

    safe_st_free(line);
    return 0;
ERR:

    safe_st_free(line);
    return -1;
}

int svec_save_header(svec_t *vec, FILE *fp, connlm_fmt_t fmt)
{
    ST_CHECK_PARAM(fp == NULL, -1);

    if (connlm_fmt_is_bin(fmt)) {
        if (fwrite(&VEC_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }
        if (fwrite(&fmt, sizeof(connlm_fmt_t), 1, fp) != 1) {
            ST_WARNING("Failed to write fmt.");
            return -1;
        }

        if (fwrite(&vec->size, sizeof(size_t), 1, fp) != 1) {
            ST_WARNING("Failed to write size.");
            return -1;
        }
    } else {
        if (fprintf(fp, "    \n<VECTOR>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }

        if (fprintf(fp, "Size: %zu\n", vec->size) < 0) {
            ST_WARNING("Failed to fprintf size.");
            return -1;
        }
    }

    return 0;
}

int svec_save_body(svec_t *vec, FILE *fp, connlm_fmt_t fmt, char *name)
{
    int n;
    size_t i;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (connlm_fmt_is_bin(fmt)) {
        n = -VEC_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        if (fmt == CONN_FMT_BIN) {
            if (fwrite(VEC_VALP(vec, 0), sizeof(real_t),
                        vec->size, fp) != vec->size) {
                ST_WARNING("Failed to write vec.");
                return -1;
            }
        } else if (fmt & CONN_FMT_SHORT_QUANTIZATION) {
            if (fmt & CONN_FMT_ZEROS_COMPRESSED) {
                if (save_sq_zc(VEC_VALP(vec, 0), vec->size, fp) < 0) {
                    ST_WARNING("Failed to save_sq_zc for vec.");
                    return -1;
                }
            } else {
                if (save_sq(VEC_VALP(vec, 0), vec->size, fp) < 0) {
                    ST_WARNING("Failed to save_sq for vec.");
                    return -1;
                }
            }
        } else if (fmt & CONN_FMT_ZEROS_COMPRESSED) {
            if (save_zc(VEC_VALP(vec, 0), vec->size, fp) < 0) {
                ST_WARNING("Failed to save_zc for vec.");
                return -1;
            }
        }
    } else {
        if (fprintf(fp, "<VECTOR-DATA>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }
        if (fprintf(fp, "%s\n", name != NULL ? name : "") < 0) {
            ST_WARNING("Failed to fprintf name.");
            return -1;
        }

        if (fprintf(fp, "[\n") < 0) {
            ST_WARNING("Failed to fprintf '['.");
            return -1;
        }
        for (i = 0; i < vec->size - 1; i++) {
            if (fprintf(fp, REAL_FMT" ", VEC_VAL(vec, i)) < 0) {
                ST_WARNING("Failed to fprintf vec[%zu].", i);
                return -1;
            }
        }
        if (fprintf(fp, REAL_FMT"\n", VEC_VAL(vec, i)) < 0) {
            ST_WARNING("Failed to fprintf vec[%zu].", i);
            return -1;
        }
        if (fprintf(fp, "]\n") < 0) {
            ST_WARNING("Failed to fprintf ']'.");
            return -1;
        }
    }

    return 0;
}

void svec_destroy(svec_t *vec)
{
    if (vec == NULL) {
        return;
    }

    if (! vec->is_const) {
        safe_st_aligned_free(vec->vals);
    }
    vec->size = 0;
    vec->capacity = 0;
}

int svec_clear(svec_t *vec)
{
    ST_CHECK_PARAM(vec == NULL, -1);

    if (vec->is_const) {
        if (vec->size != 0) {
            ST_WARNING("Can not clear a const vector.");
            return -1;
        }
    }

    vec->size = 0;

    return 0;
}

int svec_resize(svec_t *vec, size_t size, float init_val)
{
    size_t i;

    ST_CHECK_PARAM(vec == NULL || size <= 0, -1);

    if (vec->is_const) {
        if (vec->size != size) {
            ST_WARNING("Can not resize a const vector.");
            return -1;
        }
    }

    if (size > vec->capacity) {
        vec->vals = (float *)st_aligned_realloc(vec->vals,
                sizeof(float) * size, ALIGN_SIZE);
        if (vec->vals == NULL) {
            ST_WARNING("Failed to st_aligned_realloc vec->vals.");
            return -1;
        }
        if (! isnan(init_val)) {
            if (init_val == 0.0) {
                memset(vec->vals + vec->capacity, 0,
                    sizeof(float) * (size - vec->capacity));
            } else {
                for (i = vec->capacity; i < size; i++) {
                    vec->vals[i] = init_val;
                }
            }
        }
        vec->capacity = size;
    }
    vec->size = size;

    return 0;
}

int svec_cpy(svec_t *dst, svec_t *src)
{
    ST_CHECK_PARAM(dst == NULL || src == NULL, -1);

    if (svec_resize(dst, src->size, NAN) < 0) {
        ST_WARNING("Failed to svec_resize.");
        return -1;
    }

    memcpy(dst->vals, src->vals, sizeof(float) * src->size);

    return 0;
}

void svec_assign(svec_t *dst, svec_t *src)
{
    ST_CHECK_PARAM_VOID(dst == NULL || src == NULL);

    dst = src;
    dst->is_const = true;
}

void svec_set(svec_t *vec, float val)
{
    size_t i;

    ST_CHECK_PARAM_VOID(vec == NULL);

    for (i = 0; i < vec->size; i++) {
        vec->vals[i] = val;
    }
}

int svec_subvec(svec_t *vec, size_t start, size_t size, svec_t *sub)
{
    ST_CHECK_PARAM(vec == NULL || sub == NULL, -1);

    if (start >= vec->size) {
        ST_WARNING("Error start index.");
        return -1;
    }

    if (size <= 0) {
        size = vec->size - start;
    } else {
        if (start + size > vec->size) {
            ST_WARNING("Not enough elems to extract [%d + %d > %d].",
                    start, size, vec->size);
            return -1;
        }
    }

    sub->vals = vec->vals + start;
    sub->size = size;
    sub->capacity = vec->capacity - (sub->vals - vec->vals);

    sub->is_const = true;

    return 0;
}

void dvec_destroy(dvec_t *vec)
{
    if (vec == NULL) {
        return;
    }

    if (! vec->is_const) {
        safe_st_aligned_free(vec->vals);
    }
    vec->size = 0;
    vec->capacity = 0;
}

int dvec_clear(dvec_t *vec)
{
    ST_CHECK_PARAM(vec == NULL, -1);

    if (vec->is_const) {
        if (vec->size != 0) {
            ST_WARNING("Can not clear a const vector.");
            return -1;
        }
    }

    vec->size = 0;

    return 0;
}

int dvec_resize(dvec_t *vec, size_t size, double init_val)
{
    size_t i;

    ST_CHECK_PARAM(vec == NULL || size <= 0, -1);

    if (vec->is_const) {
        if (vec->size != size) {
            ST_WARNING("Can not resize a const vector.");
            return -1;
        }
    }

    if (size > vec->capacity) {
        vec->vals = (double *)st_aligned_realloc(vec->vals,
                sizeof(double) * size, ALIGN_SIZE);
        if (vec->vals == NULL) {
            ST_WARNING("Failed to st_aligned_realloc vec->vals.");
            return -1;
        }
        if (! isnan(init_val)) {
            if (init_val == 0.0) {
                memset(vec->vals + vec->capacity, 0,
                    sizeof(double) * (size - vec->capacity));
            } else {
                for (i = vec->capacity; i < size; i++) {
                    vec->vals[i] = init_val;
                }
            }
        }
        vec->capacity = size;
    }
    vec->size = size;

    return 0;
}

int dvec_cpy(dvec_t *dst, dvec_t *src)
{
    ST_CHECK_PARAM(dst == NULL || src == NULL, -1);

    if (dvec_resize(dst, src->size, NAN) < 0) {
        ST_WARNING("Failed to dvec_resize.");
        return -1;
    }

    memcpy(dst->vals, src->vals, sizeof(double) * src->size);

    return 0;
}

void dvec_assign(dvec_t *dst, dvec_t *src)
{
    ST_CHECK_PARAM_VOID(dst == NULL || src == NULL);

    dst = src;
    dst->is_const = true;
}

void dvec_set(dvec_t *vec, double val)
{
    size_t i;

    ST_CHECK_PARAM_VOID(vec == NULL);

    for (i = 0; i < vec->size; i++) {
        vec->vals[i] = val;
    }
}

int dvec_subvec(dvec_t *vec, size_t start, size_t size, dvec_t *sub)
{
    ST_CHECK_PARAM(vec == NULL || sub == NULL, -1);

    if (start >= vec->size) {
        ST_WARNING("Error start index.");
        return -1;
    }

    if (size <= 0) {
        size = vec->size - start;
    } else {
        if (start + size > vec->size) {
            ST_WARNING("Not enough elems to extract [%d + %d > %d].",
                    start, size, vec->size);
            return -1;
        }
    }

    sub->vals = vec->vals + start;
    sub->size = size;
    sub->capacity = vec->capacity - (sub->vals - vec->vals);

    sub->is_const = true;

    return 0;
}

void ivec_destroy(ivec_t *vec)
{
    if (vec == NULL) {
        return;
    }

    safe_st_aligned_free(vec->vals);
    vec->size = 0;
    vec->capacity = 0;
}

int ivec_clear(ivec_t *vec)
{
    ST_CHECK_PARAM(vec == NULL, -1);

    vec->size = 0;

    return 0;
}

int ivec_resize(ivec_t *vec, size_t size)
{
    ST_CHECK_PARAM(vec == NULL || size <= 0, -1);

    if (size > vec->capacity) {
        vec->vals = (int *)st_aligned_realloc(vec->vals,
                sizeof(int) * size, ALIGN_SIZE);
        if (vec->vals == NULL) {
            ST_WARNING("Failed to st_aligned_realloc vec->vals.");
            return -1;
        }
        vec->capacity = size;
    }
    vec->size = size;

    return 0;
}

int ivec_insert(ivec_t *vec, int n)
{
    size_t pos;

    ST_CHECK_PARAM(vec == NULL, -1);

    if (ivec_resize(vec, vec->size + 1) < 0) {
        ST_WARNING("Failed to ivec_resize.");
        return -1;
    }

    if (st_int_insert(vec->vals, vec->capacity, &vec->size, &pos, n) < 0) {
        ST_WARNING("Failed to st_int_insert.");
        return -1;
    }

    return pos;
}

int ivec_set(ivec_t *vec, int *vals, size_t n)
{
    ST_CHECK_PARAM(vec == NULL || vals == NULL, -1);

    if (ivec_resize(vec, n) < 0) {
        ST_WARNING("Failed to ivec_resize.");
        return -1;
    }

    memcpy(vec->vals, vals, sizeof(int) * n);
    vec->size = n;

    return 0;
}
