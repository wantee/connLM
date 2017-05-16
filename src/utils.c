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

#include <stdio.h>
#include <string.h>
#include <float.h>
#include <math.h>

#include <stutils/st_macro.h>
#include <stutils/st_log.h>
#include <stutils/st_utils.h>
#include <stutils/st_mem.h>
#include <stutils/st_string.h>

#include "utils.h"
#include "fastexp.h"
#include "blas.h"

#define REALLOC_NUM 100

void matXvec(real_t *dst, real_t *mat, real_t *vec,
        int mat_row, int in_vec_size, real_t scale)
{
#ifndef _USE_BLAS_
    int i;
    int j;
#endif

#ifdef _USE_BLAS_
    cblas_gemv(CblasRowMajor, CblasNoTrans, mat_row, in_vec_size, scale,
            mat, in_vec_size, vec, 1, 1.0, dst, 1);
#elif defined(_MAT_X_VEC_RAW_)
    for (i = 0; i < mat_row; i++) {
        for (j = 0; j < in_vec_size; j++) {
            dst[i] += vec[j] * mat[i*in_vec_size + j] * scale;
        }
    }
#else

#define N 8
    real_t t0;
    real_t t1;
    real_t t2;
    real_t t3;

    real_t t4;
    real_t t5;
    real_t t6;
    real_t t7;

    for (i = 0; i < mat_row / N; i++) {
        t0 = 0;
        t1 = 0;
        t2 = 0;
        t3 = 0;

        t4 = 0;
        t5 = 0;
        t6 = 0;
        t7 = 0;

        for (j = 0; j < in_vec_size; j++) {
            t0 += vec[j] * mat[(i * N + 0)*in_vec_size + j];
            t1 += vec[j] * mat[(i * N + 1)*in_vec_size + j];
            t2 += vec[j] * mat[(i * N + 2)*in_vec_size + j];
            t3 += vec[j] * mat[(i * N + 3)*in_vec_size + j];

            t4 += vec[j] * mat[(i * N + 4)*in_vec_size + j];
            t5 += vec[j] * mat[(i * N + 5)*in_vec_size + j];
            t6 += vec[j] * mat[(i * N + 6)*in_vec_size + j];
            t7 += vec[j] * mat[(i * N + 7)*in_vec_size + j];
        }

        dst[i * N + 0] += t0 * scale;
        dst[i * N + 1] += t1 * scale;
        dst[i * N + 2] += t2 * scale;
        dst[i * N + 3] += t3 * scale;

        dst[i * N + 4] += t4 * scale;
        dst[i * N + 5] += t5 * scale;
        dst[i * N + 6] += t6 * scale;
        dst[i * N + 7] += t7 * scale;
    }

    for (i = i * N; i < mat_row; i++) {
        for (j = 0; j < in_vec_size; j++) {
            dst[i] += vec[j] * mat[i*in_vec_size + j] * scale;
        }
    }
#endif
}

void vecXmat(real_t *dst, real_t *vec, real_t *mat,
        int mat_col, int in_vec_size, real_t scale)
{
#ifndef _USE_BLAS_
    int i;
    int j;
#endif

#ifdef _USE_BLAS_
    cblas_gemv(CblasRowMajor, CblasTrans, in_vec_size, mat_col, scale,
            mat, mat_col, vec, 1, 1.0, dst, 1);
#elif defined(_MAT_X_VEC_RAW_)
    for (i = 0; i < mat_col; i++) {
        for (j = 0; j < in_vec_size; j++) {
            dst[i] += vec[j] * mat[j*mat_col + i] * scale;
        }
    }
#else

#define N 8
    real_t t0;
    real_t t1;
    real_t t2;
    real_t t3;

    real_t t4;
    real_t t5;
    real_t t6;
    real_t t7;

    for (i = 0; i < mat_col / N; i++) {
        t0 = 0;
        t1 = 0;
        t2 = 0;
        t3 = 0;

        t4 = 0;
        t5 = 0;
        t6 = 0;
        t7 = 0;

        for (j = 0; j < in_vec_size; j++) {
            t0 += vec[j] * mat[(i * N + 0) + j*mat_col];
            t1 += vec[j] * mat[(i * N + 1) + j*mat_col];
            t2 += vec[j] * mat[(i * N + 2) + j*mat_col];
            t3 += vec[j] * mat[(i * N + 3) + j*mat_col];

            t4 += vec[j] * mat[(i * N + 4) + j*mat_col];
            t5 += vec[j] * mat[(i * N + 5) + j*mat_col];
            t6 += vec[j] * mat[(i * N + 6) + j*mat_col];
            t7 += vec[j] * mat[(i * N + 7) + j*mat_col];
        }

        dst[i * N + 0] += t0 * scale;
        dst[i * N + 1] += t1 * scale;
        dst[i * N + 2] += t2 * scale;
        dst[i * N + 3] += t3 * scale;

        dst[i * N + 4] += t4 * scale;
        dst[i * N + 5] += t5 * scale;
        dst[i * N + 6] += t6 * scale;
        dst[i * N + 7] += t7 * scale;
    }

    for (i = i * N; i < mat_col; i++) {
        for (j = 0; j < in_vec_size; j++) {
            dst[i] += vec[j] * mat[i + j*mat_col] * scale;
        }
    }
#endif
}

/*
 * Computing C = alpha * A' * B + beta * C
 * A is [k X m]; B is [k X n]; C is [m X n]
 * A' is transpose of A.
 */
void matXmat(real_t *C, real_t *A, real_t *B, int m, int n, int k,
        real_t alpha, real_t beta)
{
#ifdef _USE_BLAS_
    cblas_gemm(CblasRowMajor, CblasTrans, CblasNoTrans, m, n, k,
            alpha, A, m, B, n, beta, C, n);
#else
    real_t *cc;
    int i, j, t;

    if (beta != 1.0) {
        for (i = 0; i < m * n; i++) {
            C[i] = beta * C[i];
        }
    }

    for (t = 0; t < k; t++) {
        cc = C;
        for (j = 0; j < m; j++) {
            for (i = 0; i < n; i++) {
                cc[i] += alpha * A[j] * B[i];
            }
            cc += n;
        }
        A += m;
        B += n;
    }
#endif
}

real_t dot_product(real_t *v1, real_t *v2, int vec_size)
{
    double s;
    int a;

    s = 0.0;
    for (a = 0; a < vec_size; a++) {
        s += v1[a] * v2[a];
    }

    return (real_t)s;
}

void sigmoid(real_t *vec, int vec_size)
{
    int a;

    for (a = 0; a < vec_size; a++) {
        if (vec[a] > 50) {
            vec[a] = 50;
        }
        if (vec[a] < -50) {
            vec[a] = -50;
        }

        vec[a] = fastersigmoid(vec[a]);
    }
}

void softmax(real_t *vec, int vec_size)
{
    double sum;
    real_t max;

    int i;

    max = -FLT_MAX;
    sum = 0;

    for (i = 0; i < vec_size; i++) {
        if (vec[i] > max) {
            max = vec[i]; //this prevents the need to check for overflow
        }
    }

    for (i = 0; i < vec_size; i++) {
#ifdef _SOFTMAX_NO_CLIP_
        vec[i] = fasterexp(vec[i] - max);
#else
        vec[i] = vec[i] - max;
        if (vec[i] < -50) {
            vec[i] = -50;
        }
        vec[i] = fasterexp(vec[i]);
#endif
        sum += vec[i];
    }

    for (i = 0; i < vec_size; i++) {
        vec[i] = vec[i] / sum;
    }
}

void tanH(real_t *vec, int vec_size)
{
    int a;

    for (a = 0; a < vec_size; a++) {
        if (vec[a] > 25) {
            vec[a] = 25;
        }
        if (vec[a] < -25) {
            vec[a] = -25;
        }

//        vec[a] = fastertanh(vec[a]);
        vec[a] = tanh(vec[a]);
    }
}

void propagate_error(real_t *dst, real_t *vec, real_t *mat,
        int mat_col, int in_vec_size, real_t er_cutoff, real_t scale)
{
    int a;

    vecXmat(dst, vec, mat, mat_col, in_vec_size, scale);

    if (er_cutoff > 0) {
        for (a = 0; a < mat_col; a++) {
            if (dst[a] > er_cutoff) {
                dst[a] = er_cutoff;
            } else if (dst[a] < -er_cutoff) {
                dst[a] = -er_cutoff;
            }
        }
    }
}

void connlm_show_usage(const char *module_name, const char *header,
        const char *usage, const char *eg,
        st_opt_t *opt, const char *trailer)
{
    fprintf(stderr, "\nConnectionist Language Modelling Toolkit\n");
    fprintf(stderr, "    -- %s\n", header);
    fprintf(stderr, "Version  : %s (%s)\n", CONNLM_VERSION, CONNLM_GIT_COMMIT);
    fprintf(stderr, "File version: %d\n", CONNLM_FILE_VERSION);
    fprintf(stderr, "Real type: %s\n",
            (sizeof(real_t) == sizeof(double)) ? "double" : "float");
    fprintf(stderr, "Usage    : %s [options] %s\n", module_name, usage);
    if (eg != NULL) {
        fprintf(stderr, "e.g.: \n");
        fprintf(stderr, "  %s %s\n", module_name, eg);
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "Options  : \n");
    st_opt_show_usage(opt, stderr, true);
    if (trailer != NULL) {
        fprintf(stderr, "\n%s\n", trailer);
    }
}

model_filter_t parse_model_filter(const char *mdl_filter,
        char *mdl_file, size_t mdl_file_len, char **comp_names,
        int *num_comp)
{
    char *ptr;
    char *ptr_fname;
    char *ptr_comp;
    model_filter_t mf;
    bool add;

    ST_CHECK_PARAM(mdl_filter == NULL || mdl_file == NULL ||
            comp_names == NULL || num_comp == NULL, MF_ERR);

    *comp_names = NULL;
    *num_comp = 0;

    if (strncmp(mdl_filter, "mdl,", 4) != 0) {
        ptr_fname = (char *)mdl_filter;
        mf = MF_ALL;
        goto RET;
    }

    ptr = (char *)mdl_filter + 4;
    ptr_fname = strchr(ptr, ':');
    if (ptr_fname == NULL) { // not filter format
        ptr_fname = (char *)mdl_filter;
        mf = MF_ALL;
        goto RET;
    }

    if (*ptr == '-') {
        mf = MF_ALL;
        add = false;

        ptr++;
    } else {
        mf = MF_NONE;
        add = true;

        if (*ptr == '+') {
            ptr++;
        }
    }

    while (ptr < ptr_fname) {
        if (*ptr == ',') {
            ptr++;
            continue;
        }

        switch (*ptr) {
            case 'o':
                if (add) {
                    mf |= MF_OUTPUT;
                } else {
                    mf &= ~MF_OUTPUT;
                }
                break;
            case 'v':
                if (add) {
                    mf |= MF_VOCAB;
                } else {
                    mf &= ~MF_VOCAB;
                }
                break;
            case 'c':
                if (*(ptr + 1) != '{') {
                    *num_comp = -1;
                    break;
                } else {
                    ptr++;
                    ptr++;
                    ptr_comp = ptr;
                    while (ptr < ptr_fname) {
                        if (*ptr == '}') {
                            break;
                        }
                        ptr++;
                    }
                    if (*ptr == '}') {
                        if (*num_comp == -1) { // already set all components with single 'c'
                            break;
                        }
                        *comp_names = st_realloc(*comp_names,
                                MAX_NAME_LEN*(*num_comp + 1));
                        if (*comp_names == NULL) {
                            ST_WARNING("Failed to st_realloc comp_names");
                            goto ERR;
                        }

                        if (ptr - ptr_comp >= MAX_NAME_LEN) {
                            ST_WARNING("Too long name[%s]", ptr_comp);
                            goto ERR;
                        }
                        strncpy(*comp_names + MAX_NAME_LEN*(*num_comp),
                                ptr_comp, ptr - ptr_comp);
                        (*comp_names)[MAX_NAME_LEN*(*num_comp) + ptr - ptr_comp] = '\0';
                        (*num_comp)++;
                        break;
                    }
                }
                /* FALL THROUGH */
            default:
                ptr_fname = (char *)mdl_filter;
                mf = MF_ALL;
                goto RET;
        }

        ptr++;
    }

    ptr_fname++;

RET:
    if (strlen(ptr_fname) >= mdl_file_len) {
        ST_WARNING("mdl_file_len too small.[%zu/%zu]",
                mdl_file_len, strlen(ptr_fname));
        goto ERR;
    }
    strcpy(mdl_file, ptr_fname);

    return mf;

ERR:
    safe_st_free(*comp_names);
    *num_comp = 0;
    mdl_file[0] = '\0';
    return MF_ERR;
}

const char* model_filter_help()
{
    return "A model filter can be: 'mdl,[+-][ovc{comp1}c{comp2}]:file_name'.\n"
         "Any string not in such format will be treated as a model file name.";
}

char* escape_dot(char *out, size_t len, const char *str)
{
    const char *p;
    char *q;
    size_t i;

    p = str;
    q = out;
    i = 0;
    while (*p != '\0') {
        if (*p == '<' || *p == '>' || *p == '\\') {
            if (i >= len - 1) {
                return NULL;
            }
            *(q++) = '\\';
            ++i;
        }
        if (i >= len - 1) {
            return NULL;
        }
        *(q++) = *(p++);
        ++i;
    }
    *q = '\0';

    return out;
}

void concat_mat_destroy(concat_mat_t *mat)
{
    if (mat == NULL) {
        return;
    }

    safe_st_aligned_free(mat->val);
    mat->col = 0;
    mat->n_row = 0;
    mat->cap_row = 0;
}

int concat_mat_add_row(concat_mat_t *mat, real_t *vec, int vec_size)
{
    size_t sz;

    ST_CHECK_PARAM(mat == NULL || vec == NULL || vec_size <= 0, -1);

    if (mat->col == 0) {
        mat->col = vec_size;
    } else if (mat->col != vec_size) {
        ST_WARNING("col not match.");
        return -1;
    }

    if (mat->n_row >= mat->cap_row) {
        mat->cap_row += REALLOC_NUM;
        sz = mat->cap_row * mat->col * sizeof(real_t);
        mat->val = (real_t *)st_aligned_realloc(mat->val, sz, ALIGN_SIZE);
        if (mat->val == NULL) {
            ST_WARNING("Failed to st_realloc val");
            return -1;
        }
    }

    memcpy(mat->val + mat->n_row * mat->col, vec, sizeof(real_t) * mat->col);
    mat->n_row++;

    return 0;
}

int concat_mat_add_mat(concat_mat_t *dst, concat_mat_t *src)
{
    size_t sz;

    ST_CHECK_PARAM(dst == NULL || src == NULL, -1);

    if (src->col != dst->col) {
        ST_WARNING("col not match.");
        return -1;
    }

    if (dst->n_row + src->n_row >= dst->cap_row) {
        dst->cap_row += src->n_row;
        sz = dst->cap_row * dst->col * sizeof(real_t);
        dst->val = (real_t *)st_aligned_realloc(dst->val, sz, ALIGN_SIZE);
        if (dst->val == NULL) {
            ST_WARNING("Failed to st_realloc val");
            return -1;
        }
    }

    memcpy(dst->val + dst->n_row * dst->col, src->val,
            sizeof(real_t) * src->n_row * src->col);
    dst->n_row += src->n_row;

    return 0;
}

connlm_fmt_t connlm_format_parse(const char *str)
{
    connlm_fmt_t fmt;

    char buf[MAX_LINE_LEN];
    int i;
    const char *p;

    ST_CHECK_PARAM(str == NULL, CONN_FMT_UNKNOWN);

    fmt = CONN_FMT_UNKNOWN;
    p = str;
    while (*p != '\0') {
        i = 0;
        while (*p != '|' && *p != '\0') {
            if (i >= MAX_LINE_LEN - 1) {
                ST_WARNING("Too long name");
                return CONN_FMT_UNKNOWN;
            }
            buf[i] = *p;
            ++i;
            ++p;
        }
        buf[i] = '\0';
        trim(buf);

        if (strcasecmp(buf, "Text") == 0
                || strcasecmp(buf, "Txt") == 0
                || strcasecmp(buf, "T") == 0) {
            if (connlm_fmt_is_bin(fmt)) {
                ST_WARNING("Text and Binary format can't exist simultaneously");
                return CONN_FMT_UNKNOWN;
            }
            fmt = CONN_FMT_TXT;
            continue;
        }

        if (fmt == CONN_FMT_TXT) {
            ST_WARNING("Text and Binary format can't exist simultaneously");
            return CONN_FMT_UNKNOWN;
        }

        if (strcasecmp(buf, "Binary") == 0
                || strcasecmp(buf, "Bin") == 0
                || strcasecmp(buf, "B") == 0) {
            fmt |= CONN_FMT_BIN;
        } else if (strcasecmp(buf, "Zeros-Compressed") == 0
                || strcasecmp(buf, "Zeros-Compress") == 0
                || strcasecmp(buf, "ZC") == 0) {
            fmt |= CONN_FMT_ZEROS_COMPRESSED;
        } else if (strcasecmp(buf, "Short-Quantization") == 0
                || strcasecmp(buf, "Short-Q") == 0
                || strcasecmp(buf, "SQ") == 0) {
            fmt |= CONN_FMT_SHORT_QUANTIZATION;
        }

        if (*p == '\0') {
            break;
        }
        ++p;
    }

    return fmt;
}

int16_t quantify_int16(real_t r, real_t multiple)
{
    r = r * multiple;
    r = min(r, (1 << 15) - 1);
    r = max(r, -(1 << 15));

    return (int16_t)r;
}

int print_vec(FILE *fp, real_t *vec, size_t size, const char *name)
{
    size_t i;

    ST_CHECK_PARAM(fp == NULL || vec == NULL || size <= 0, -1);

    if (name != NULL) {
        if (fprintf(fp, "%s: ", name) < 0) {
            ST_WARNING("Failed to fprintf name, Disk full?");
            return -1;
        }
    }
    if (fprintf(fp, "[ ") < 0) {
        ST_WARNING("Failed to fprintf, Disk full?");
        return -1;
    }

    for (i = 0; i < size; i++) {
        if (fprintf(fp, REAL_FMT" ", vec[i]) < 0) {
            ST_WARNING("Failed to fprintf, Disk full?");
            return -1;
        }
    }

    if (fprintf(fp, "]\n") < 0) {
        ST_WARNING("Failed to fprintf, Disk full?");
        return -1;
    }

    return 0;
}

int parse_vec(const char *line, real_t **vec, char *name, size_t name_len)
{
    char token[MAX_NAME_LEN];
    const char *p, *q, *r;
    int i, size, cap;

    ST_CHECK_PARAM(line == NULL || vec == NULL, -1);

    *vec = NULL;
    cap = 128;
    size = 0;

    p = strchr(line, '[');
    if (p == NULL) {
        ST_WARNING("Error vector format: '[' expected.");
        return -1;
    }

    q = p;
    while (q > line) {
        if (*q == ':') {
            break;
        }
        --q;
    }

    if (name != NULL && name_len > 0) {
        i = 0;
        r = line;
        while (i < name_len && r < q) {
            name[i] = *r;
            ++i;
            ++r;
        }
        if (i < name_len) {
            name[i] = '\0';
        } else {
            name[name_len - 1] = '\0';
        }
    }

    *vec = (real_t *)st_malloc(sizeof(real_t) * cap);
    if (*vec == NULL) {
        ST_WARNING("Failed to st_malloc vec");
        goto ERR;
    }

    ++p;
    while (*p == ' ' || *p == '\t') {
        ++p;
    }
    while (true) {
        p = get_next_token(p, token);
        if (p == NULL || token[0] == ']') {
            break;
        }

        if (size >= cap) {
            *vec = (real_t *)st_realloc(*vec, sizeof(real_t) * (cap + 128));
            if (*vec == NULL) {
                ST_WARNING("Failed to st_realloc vec");
                goto ERR;
            }
            cap += 128;
        }

        if (sscanf(token, REAL_FMT, *vec + size) != 1) {
            ST_WARNING("Error vector format: Not a float numbers[%s]", token);
            goto ERR;
        }
        ++size;
    }

    if (token[0] != ']') {
        ST_WARNING("Error vector format: ']' expected.");
        goto ERR;
    }

    return size;

ERR:
    safe_st_free(*vec);
    return -1;
}
