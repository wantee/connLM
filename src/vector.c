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
#include <string.h>

#include <stutils/st_macro.h>
#include <stutils/st_log.h>
#include <stutils/st_mem.h>
#include <stutils/st_int.h>
#include <stutils/st_io.h>
#include <stutils/st_string.h>

#include "vector.h"

static const int VEC_MAGIC_NUM = 626140498 + 81;

int vec_load_header(vec_t *vec, int version,
        FILE *fp, connlm_fmt_t *fmt, FILE *fo_info)
{
    union {
        char str[4];
        int magic_num;
    } flag;

    size_t size;

    ST_CHECK_PARAM(fp == NULL || fmt == NULL, -1);

    if (fread(&flag.magic_num, sizeof(int), 1, fp) != 1) {
        ST_ERROR("Failed to load magic num.");
        return -1;
    }

    *fmt = CONN_FMT_UNKNOWN;
    if (strncmp(flag.str, "    ", 4) == 0) {
        *fmt = CONN_FMT_TXT;
    } else if (VEC_MAGIC_NUM != flag.magic_num) {
        ST_ERROR("magic num wrong.");
        return -2;
    }

    if (vec != NULL) {
        memset(vec, 0, sizeof(vec_t));
    }

    if (*fmt != CONN_FMT_TXT) {
        if (fread(fmt, sizeof(connlm_fmt_t), 1, fp) != 1) {
            ST_ERROR("Failed to read fmt.");
            goto ERR;
        }

        if (fread(&size, sizeof(size_t), 1, fp) != 1) {
            ST_ERROR("Failed to read size.");
            goto ERR;
        }
    } else {
        if (st_readline(fp, "") != 0) {
            ST_ERROR("tag error.");
            goto ERR;
        }
        if (st_readline(fp, "<VECTOR>") != 0) {
            ST_ERROR("tag error.");
            goto ERR;
        }

        if (st_readline(fp, "Size: %zu", &size) != 1) {
            ST_ERROR("Failed to parse size.");
            goto ERR;
        }
    }

    if (vec != NULL) {
        if (vec_resize(vec, size, NAN) < 0) {
            ST_ERROR("Failed to vec_resize.");
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
        vec_destroy(vec);
    }
    return -1;
}

int vec_load_body(vec_t *vec, int version, FILE *fp, connlm_fmt_t fmt)
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
            ST_ERROR("Failed to read magic num.");
            return -1;
        }

        if (n != -VEC_MAGIC_NUM) {
            ST_ERROR("Magic num error.[%d]", n);
            return -1;
        }

        if (fmt == CONN_FMT_BIN) {
            if (fread(VEC_VALP(vec, 0), sizeof(real_t),
                        vec->size, fp) != vec->size) {
                ST_ERROR("Failed to read vec.");
                return -1;
            }
        } else if (fmt & CONN_FMT_SHORT_QUANTIZATION) {
            if (fmt & CONN_FMT_ZEROS_COMPRESSED) {
                if (load_sq_zc(VEC_VALP(vec, 0), vec->size, fp) < 0) {
                    ST_ERROR("Failed to load_sq_zc for vec.");
                    return -1;
                }
            } else {
                if (load_sq(VEC_VALP(vec, 0), vec->size, fp) < 0) {
                    ST_ERROR("Failed to load_sq for vec.");
                    return -1;
                }
            }
        } else if (fmt & CONN_FMT_ZEROS_COMPRESSED) {
            if (load_zc(VEC_VALP(vec, 0), vec->size, fp) < 0) {
                ST_ERROR("Failed to load_zc for vec.");
                return -1;
            }
        }
    } else {
        if (st_readline(fp, "<VECTOR-DATA>") != 0) {
            ST_ERROR("body flag error.");
            goto ERR;
        }
        if (st_readline(fp, "%"xSTR(MAX_NAME_LEN)"s", name) != 1) {
            ST_ERROR("name error.");
            goto ERR;
        }

        if (st_fgets(&line, &line_sz, fp, &err) == NULL || err) {
            ST_ERROR("'[' expected.");
            goto ERR;
        }
        p = get_next_token(line, token);
        if (p == NULL || ! (token[0] == '[' && token[1] == '\0')) {
            ST_ERROR("'[' expected.");
            goto ERR;
        }
        if (vec->size > 0) {
            if (st_fgets(&line, &line_sz, fp, &err) == NULL || err) {
                ST_ERROR("Failed to parse vec.");
                goto ERR;
            }
            p = line;
            for (i = 0; i < vec->size; i++) {
                p = get_next_token(p, token);
                if (p == NULL || sscanf(token, REAL_FMT,
                            VEC_VALP(vec, i)) != 1) {
                    ST_ERROR("Failed to parse elem[%zu].", i);
                    goto ERR;
                }
            }
        }
        if (st_fgets(&line, &line_sz, fp, &err) == NULL || err) {
            ST_ERROR("']' expected.");
            goto ERR;
        }
        p = get_next_token(line, token);
        if (p == NULL || ! (token[0] == ']' && token[1] == '\0')) {
            ST_ERROR("']' expected.");
            goto ERR;
        }

        safe_st_free(line);
    }

    return 0;
ERR:

    safe_st_free(line);
    return -1;
}

int vec_save_header(vec_t *vec, FILE *fp, connlm_fmt_t fmt)
{
    ST_CHECK_PARAM(fp == NULL, -1);

    if (connlm_fmt_is_bin(fmt)) {
        if (fwrite(&VEC_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to write magic num.");
            return -1;
        }
        if (fwrite(&fmt, sizeof(connlm_fmt_t), 1, fp) != 1) {
            ST_ERROR("Failed to write fmt.");
            return -1;
        }

        if (fwrite(&vec->size, sizeof(size_t), 1, fp) != 1) {
            ST_ERROR("Failed to write size.");
            return -1;
        }
    } else {
        if (fprintf(fp, "    \n<VECTOR>\n") < 0) {
            ST_ERROR("Failed to fprintf header.");
            return -1;
        }

        if (fprintf(fp, "Size: %zu\n", vec->size) < 0) {
            ST_ERROR("Failed to fprintf size.");
            return -1;
        }
    }

    return 0;
}

int vec_save_body(vec_t *vec, FILE *fp, connlm_fmt_t fmt, char *name)
{
    int n;
    size_t i;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (connlm_fmt_is_bin(fmt)) {
        n = -VEC_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to write magic num.");
            return -1;
        }

        if (vec->size <= 0) {
            return 0;
        }

        if (fmt == CONN_FMT_BIN) {
            if (fwrite(VEC_VALP(vec, 0), sizeof(real_t),
                        vec->size, fp) != vec->size) {
                ST_ERROR("Failed to write vec.");
                return -1;
            }
        } else if (fmt & CONN_FMT_SHORT_QUANTIZATION) {
            if (fmt & CONN_FMT_ZEROS_COMPRESSED) {
                if (save_sq_zc(VEC_VALP(vec, 0), vec->size, fp) < 0) {
                    ST_ERROR("Failed to save_sq_zc for vec.");
                    return -1;
                }
            } else {
                if (save_sq(VEC_VALP(vec, 0), vec->size, fp) < 0) {
                    ST_ERROR("Failed to save_sq for vec.");
                    return -1;
                }
            }
        } else if (fmt & CONN_FMT_ZEROS_COMPRESSED) {
            if (save_zc(VEC_VALP(vec, 0), vec->size, fp) < 0) {
                ST_ERROR("Failed to save_zc for vec.");
                return -1;
            }
        }
    } else {
        if (fprintf(fp, "<VECTOR-DATA>\n") < 0) {
            ST_ERROR("Failed to fprintf header.");
            return -1;
        }
        if (fprintf(fp, "%s\n", name != NULL ? name : "") < 0) {
            ST_ERROR("Failed to fprintf name.");
            return -1;
        }

        if (fprintf(fp, "[\n") < 0) {
            ST_ERROR("Failed to fprintf '['.");
            return -1;
        }
        if (vec->size > 0) {
            for (i = 0; i < vec->size - 1; i++) {
                if (fprintf(fp, REAL_FMT" ", VEC_VAL(vec, i)) < 0) {
                    ST_ERROR("Failed to fprintf vec[%zu].", i);
                    return -1;
                }
            }
            if (fprintf(fp, REAL_FMT"\n", VEC_VAL(vec, i)) < 0) {
                ST_ERROR("Failed to fprintf vec[%zu].", i);
                return -1;
            }
        }
        if (fprintf(fp, "]\n") < 0) {
            ST_ERROR("Failed to fprintf ']'.");
            return -1;
        }
    }

    return 0;
}

void vec_destroy(vec_t *vec)
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

int vec_clear(vec_t *vec)
{
    ST_CHECK_PARAM(vec == NULL, -1);

    if (vec->is_const) {
        if (vec->size != 0) {
            ST_ERROR("Can not clear a const vector.");
            return -1;
        }
    }

    vec->size = 0;

    return 0;
}

int vec_resize(vec_t *vec, size_t size, real_t init_val)
{
    size_t i;

    ST_CHECK_PARAM(vec == NULL || size < 0, -1);

    if (vec->is_const) {
        if (vec->size != size) {
            ST_ERROR("Can not resize a const vector.");
            return -1;
        }
    }

    if (size > vec->capacity) {
        vec->vals = (real_t *)st_aligned_realloc(vec->vals,
                sizeof(real_t) * size, ALIGN_SIZE);
        if (vec->vals == NULL) {
            ST_ERROR("Failed to st_aligned_realloc vec->vals.");
            return -1;
        }
        if (! isnan(init_val)) {
            if (init_val == 0.0) {
                memset(vec->vals + vec->capacity, 0,
                    sizeof(real_t) * (size - vec->capacity));
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

int vec_cpy(vec_t *dst, vec_t *src)
{
    ST_CHECK_PARAM(dst == NULL || src == NULL, -1);

    if (vec_resize(dst, src->size, NAN) < 0) {
        ST_ERROR("Failed to vec_resize.");
        return -1;
    }

    memcpy(dst->vals, src->vals, sizeof(real_t) * src->size);

    return 0;
}

void vec_assign(vec_t *dst, vec_t *src)
{
    ST_CHECK_PARAM_VOID(dst == NULL || src == NULL);

    *dst = *src;
    dst->is_const = true;
}

void vec_set(vec_t *vec, real_t val)
{
    size_t i;

    ST_CHECK_PARAM_VOID(vec == NULL);

    for (i = 0; i < vec->size; i++) {
        vec->vals[i] = val;
    }
}

int vec_subvec(vec_t *vec, size_t start, size_t size, vec_t *sub)
{
    ST_CHECK_PARAM(vec == NULL || sub == NULL, -1);

    if (start >= vec->size) {
        ST_ERROR("Error start index.");
        return -1;
    }

    if (size <= 0) {
        size = vec->size - start;
    } else {
        if (start + size > vec->size) {
            ST_ERROR("Not enough elems to extract [%d + %d > %d].",
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

int vec_add_elems(vec_t *vec1, real_t s1, vec_t *vec2, real_t s2, vec_t *out)
{
    size_t i;

    ST_CHECK_PARAM(vec1 == NULL || vec2 == NULL || out == NULL, -1);

    if (vec1->size != vec2->size || vec1->size != out->size) {
        ST_ERROR("Diemension not match.");
        return -1;
    }

    for (i = 0; i < vec1->size; i++) {
        out->vals[i] = s1 * vec1->vals[i] + s2 * vec2->vals[i];
    }

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
            ST_ERROR("Can not clear a const vector.");
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
            ST_ERROR("Can not resize a const vector.");
            return -1;
        }
    }

    if (size > vec->capacity) {
        vec->vals = (double *)st_aligned_realloc(vec->vals,
                sizeof(double) * size, ALIGN_SIZE);
        if (vec->vals == NULL) {
            ST_ERROR("Failed to st_aligned_realloc vec->vals.");
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

void dvec_set(dvec_t *vec, double val)
{
    size_t i;

    ST_CHECK_PARAM_VOID(vec == NULL);

    for (i = 0; i < vec->size; i++) {
        vec->vals[i] = val;
    }
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

int ivec_reserve(ivec_t *vec, size_t capacity)
{
    ST_CHECK_PARAM(vec == NULL || capacity <= 0, -1);

    if (capacity > vec->capacity) {
        vec->vals = (int *)st_aligned_realloc(vec->vals,
                sizeof(int) * capacity, ALIGN_SIZE);
        if (vec->vals == NULL) {
            ST_ERROR("Failed to st_aligned_realloc vec->vals.");
            return -1;
        }
        vec->capacity = capacity;
    }

    return 0;
}

int ivec_ext_reserve(ivec_t *vec, size_t ext_capacity)
{
    ST_CHECK_PARAM(vec == NULL || ext_capacity < 0, -1);

    if (ivec_reserve(vec, vec->size + ext_capacity) < 0) {
        ST_ERROR("Failed to ivec_reserve.");
        return -1;
    }

    return 0;
}

int ivec_resize(ivec_t *vec, size_t size)
{
    ST_CHECK_PARAM(vec == NULL || size <= 0, -1);

    if (ivec_reserve(vec, size) < 0) {
        ST_ERROR("Failed to ivec_reserve.");
        return -1;
    }
    vec->size = size;

    return 0;
}

int ivec_extsize(ivec_t *vec, size_t ext_size)
{
    ST_CHECK_PARAM(vec == NULL || ext_size <= 0, -1);

    if (ivec_resize(vec, ext_size + vec->size) < 0) {
        ST_ERROR("Failed to ivec_resize.");
        return -1;
    }

    return 0;
}

int ivec_append(ivec_t *vec, int n)
{
    if (ivec_reserve(vec, vec->size + NUM_IVEC_RESIZE) < 0) {
        ST_ERROR("Failed to ivec_reserve.");
        return -1;
    }

    vec->vals[vec->size] = n;
    vec->size++;

    return 0;
}

int ivec_insert(ivec_t *vec, int n)
{
    size_t pos;

    ST_CHECK_PARAM(vec == NULL, -1);

    if (ivec_reserve(vec, NUM_IVEC_RESIZE) < 0) {
        ST_ERROR("Failed to ivec_reserve.");
        return -1;
    }

    if (st_int_insert(vec->vals, vec->capacity, &vec->size, &pos, n) < 0) {
        ST_ERROR("Failed to st_int_insert.");
        return -1;
    }

    return pos;
}

int ivec_set(ivec_t *vec, int *vals, size_t n)
{
    ST_CHECK_PARAM(vec == NULL || vals == NULL, -1);

    if (ivec_resize(vec, n) < 0) {
        ST_ERROR("Failed to ivec_resize.");
        return -1;
    }

    memcpy(vec->vals, vals, sizeof(int) * n);
    vec->size = n;

    return 0;
}

int ivec_cpy(ivec_t *dst, ivec_t *src)
{
    ST_CHECK_PARAM(dst == NULL || src == NULL, -1);

    if (ivec_resize(dst, src->size) < 0) {
        ST_ERROR("Failed to ivec_resize.");
        return -1;
    }

    memcpy(dst->vals, src->vals, sizeof(int) * src->size);

    return 0;
}

#define SWAP(a, b, t) \
    (t) = (a); (a) = (b); (b) = (t);
int ivec_swap(ivec_t *vec1, ivec_t *vec2)
{
    int *tmp_vals;
    size_t tmp_sz;

    ST_CHECK_PARAM(vec1 == NULL || vec2 == NULL, -1);

    SWAP(vec1->vals, vec2->vals, tmp_vals);
    SWAP(vec1->size, vec2->size, tmp_sz);
    SWAP(vec1->capacity, vec2->capacity, tmp_sz);

    return 0;
}

int ivec_extend(ivec_t *vec, ivec_t *src, size_t start, size_t end)
{
    ST_CHECK_PARAM(vec == NULL || src == NULL, -1);

    if (ivec_ext_reserve(vec, end - start) < 0) {
        ST_ERROR("Failed to ivec_ext_reserve.");
        return -1;
    }

    memcpy(vec->vals + vec->size, src->vals + start,
            sizeof(int) * (end - start));
    vec->size += end - start;

    return 0;
}

char* ivec_dump(ivec_t *vec, char *buf, size_t buf_len)
{
    size_t i;

    const char *sep = ",";

    ST_CHECK_PARAM(vec == NULL || buf == NULL || buf_len <= 0, NULL);

    buf[0] = '\0';
    st_strncatf(buf, buf_len, "%zu: [", vec->size);
    if (vec->size > 0) {
        for (i = 0; i < vec->size - 1; i++) {
            st_strncatf(buf, buf_len, "%d%s", VEC_VAL(vec, i), sep);
        }
        st_strncatf(buf, buf_len, "%d", VEC_VAL(vec, i));
    }
    st_strncatf(buf, buf_len, "]");

    return buf;
}
