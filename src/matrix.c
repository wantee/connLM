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
#include <stutils/st_io.h>
#include <stutils/st_string.h>

#include "matrix.h"

static const int MAT_MAGIC_NUM = 626140498 + 80;

void mat_destroy(mat_t *mat)
{
    if (mat == NULL) {
        return;
    }

    if (! mat->is_const) {
        safe_st_aligned_free(mat->vals);
    }

    mat->num_rows = 0;
    mat->num_cols = 0;
    mat->stride = 0;
    mat->capacity = 0;
}

int mat_clear(mat_t *mat)
{
    ST_CHECK_PARAM(mat == NULL, -1);

    if (mat->is_const) {
        if (mat->num_rows != 0 || mat->num_cols != 0) {
            ST_WARNING("Can not clear a const mat.");
            return -1;
        }
    }

    mat->num_rows = 0;
    mat->num_cols = 0;
    mat->stride = 0;

    return 0;
}

int mat_resize(mat_t *mat, size_t num_rows, size_t num_cols, real_t init_val)
{
    size_t stride;

    ST_CHECK_PARAM(mat == NULL, -1);

    if (mat->is_const) {
        if (mat->num_rows != num_rows || mat->num_cols != num_cols) {
            ST_WARNING("Can not resize a const matrix.");
            return -1;
        }
    }

    stride = num_cols;
    stride += ((ALIGN_SIZE / sizeof(real_t)) - num_cols % (ALIGN_SIZE / sizeof(real_t)))
                  % (ALIGN_SIZE / sizeof(real_t));

    if (num_rows * stride > mat->capacity) {
        mat->vals = (real_t *)st_aligned_realloc(mat->vals,
                sizeof(real_t) * num_rows * stride, ALIGN_SIZE);
        if (mat->vals == NULL) {
            ST_WARNING("Failed to st_aligned_realloc mat->vals.");
            return -1;
        }
        mat->capacity = num_rows * stride;
    }
    mat->num_rows = num_rows;
    mat->num_cols = num_cols;
    mat->stride = stride;

    if (! isnan(init_val)) {
        mat_set(mat, init_val);
    }

    return 0;
}

int mat_resize_row(mat_t *mat, size_t num_rows, real_t init_val)
{
    ST_CHECK_PARAM(mat == NULL || num_rows <= 0, -1);

    if (mat->is_const) {
        if (mat->num_rows != num_rows) {
            ST_WARNING("Can not resize a const matrix.");
            return -1;
        }
    }

    if (num_rows * mat->stride > mat->capacity) {
        mat->vals = (real_t *)st_aligned_realloc(mat->vals,
                sizeof(real_t) * num_rows * mat->stride, ALIGN_SIZE);
        if (mat->vals == NULL) {
            ST_WARNING("Failed to st_aligned_realloc mat->vals.");
            return -1;
        }
        mat->capacity = num_rows * mat->stride;
    }
    mat->num_rows = num_rows;

    if (! isnan(init_val)) {
        mat_set(mat, init_val);
    }

    return 0;
}

int mat_append(mat_t *dst, mat_t* src)
{
    size_t i, dst_row;
    real_t *src_vals, *dst_vals;

    ST_CHECK_PARAM(dst == NULL || src == NULL, -1);

    dst_row = dst->num_rows;

    if (dst->num_rows == 0 && dst->num_cols == 0) {
        if (mat_resize(dst, src->num_rows, src->num_cols, NAN) < 0) {
            ST_WARNING("Failed to mat_resize.");
            return -1;
        }
    } else {
        if (dst->num_cols != src->num_cols) {
            ST_WARNING("num_cols not match.[%zu:%zu]", dst->num_cols, src->num_cols);
            return -1;
        }

        if (mat_resize_row(dst, dst->num_rows + src->num_rows, NAN) < 0) {
            ST_WARNING("Failed to mat_resize_row.");
            return -1;
        }
    }

    dst_vals = dst->vals + dst_row * dst->stride;
    src_vals = src->vals;
    for (i = 0; i < src->num_rows; i++) {
        memcpy(dst_vals, src_vals, sizeof(real_t) * src->num_cols);
        src_vals += src->stride;
        dst_vals += dst->stride;
    }

    return 0;
}

int mat_cpy(mat_t *dst, mat_t *src)
{
    ST_CHECK_PARAM(dst == NULL || src == NULL, -1);

    if (mat_resize(dst, src->num_rows, src->num_cols, NAN) < 0) {
        ST_WARNING("Failed to mat_resize.");
        return -1;
    }

    memcpy(dst->vals, src->vals,
            sizeof(real_t) * src->num_rows * src->stride);

    return 0;
}

void mat_assign(mat_t *dst, mat_t *src)
{
    ST_CHECK_PARAM_VOID(dst == NULL || src == NULL);

    *dst = *src;
    dst->is_const = true;
}

bool mat_eq(mat_t *mat1, mat_t *mat2)
{
    real_t *vals1, *vals2;
    size_t i, j;

    ST_CHECK_PARAM(mat1 == NULL || mat2 == NULL, false);

    if (mat1->num_rows != mat2->num_rows) {
        return false;
    }

    if (mat1->num_cols != mat2->num_cols) {
        return false;
    }

    vals1 = mat1->vals;
    vals2 = mat2->vals;
    for (i = 0; i < mat1->num_rows; i++) {
        for (j = 0; j < mat1->num_cols; j++) {
            if (vals1[j] != vals2[j]) {
                return false;
            }
        }
        vals1 += mat1->stride;
        vals2 += mat2->stride;
    }

    return true;
}

int mat_move_up(mat_t *mat, size_t dst_row, size_t src_row)
{
    ST_CHECK_PARAM(mat == NULL, -1);

    if (dst_row >= src_row) {
        ST_WARNING("dst_row must be larger than src_row.");
        return -1;
    }

    memmove(mat->vals + dst_row * mat->stride,
            mat->vals + src_row * mat->stride,
            sizeof(real_t) * (mat->num_rows - src_row) * mat->stride);

    return 0;
}

int mat_cpy_row(mat_t *dst, size_t dst_row, mat_t *src, size_t src_row)
{
    ST_CHECK_PARAM(dst == NULL || src == NULL, -1);

    if (dst_row >= dst->num_rows) {
        ST_WARNING("Invalid dst row index [%zu/%zu]", dst_row, dst->num_rows);
        return -1;
    }

    if (src_row >= src->num_rows) {
        ST_WARNING("Invalid src row index [%zu/%zu]", src_row, src->num_rows);
        return -1;
    }

    if (dst->num_cols != src->num_cols) {
        ST_WARNING("num_cols not match [%zu/%zu]", dst->num_cols, src->num_cols);
        return -1;
    }

    memcpy(dst->vals + dst_row * dst->stride,
           src->vals + src_row * src->stride,
           sizeof(real_t) * dst->num_cols);

    return 0;
}

int mat_acc_row(mat_t *dst, size_t dst_row, mat_t *src, size_t src_row)
{
    real_t *dst_vals, *src_vals;
    size_t j;

    ST_CHECK_PARAM(dst == NULL || src == NULL, -1);

    if (dst_row >= dst->num_rows) {
        ST_WARNING("Invalid dst row index [%zu/%zu]", dst_row, dst->num_rows);
        return -1;
    }

    if (src_row >= src->num_rows) {
        ST_WARNING("Invalid src row index [%zu/%zu]", src_row, src->num_rows);
        return -1;
    }

    if (dst->num_cols != src->num_cols) {
        ST_WARNING("num_cols not match [%zu/%zu]", dst->num_cols, src->num_cols);
        return -1;
    }

    dst_vals = dst->vals + dst_row * dst->stride;
    src_vals = src->vals + src_row * src->stride;
    for (j = 0; j < dst->num_cols; j++) {
        dst_vals[j] += src_vals[j];
    }

    return 0;
}

int mat_submat(mat_t *mat, size_t row_s, size_t num_rows,
        size_t col_s, size_t num_cols, mat_t *sub)
{
    ST_CHECK_PARAM(mat == NULL || sub == NULL, -1);

    if (row_s >= mat->num_rows || col_s >= mat->num_cols) {
        ST_WARNING("Error row[%zu/%zu] or col[%zu/%zu] index.",
                row_s, mat->num_rows, col_s, mat->num_cols);
        return -1;
    }

    if (num_rows <= 0) {
        num_rows = mat->num_rows - row_s;
    } else {
        if (row_s + num_rows > mat->num_rows) {
            ST_WARNING("Not enough rows to extract [%zu + %zu > %zu].",
                    row_s, num_rows, mat->num_rows);
            return -1;
        }
    }

    if (num_cols <= 0) {
        num_cols = mat->num_cols - col_s;
    } else {
        if (col_s + num_cols > mat->num_cols) {
            ST_WARNING("Not enough cols to extract [%zu + %zu > %zu].",
                    col_s, num_cols, mat->num_cols);
            return -1;
        }
    }

    sub->vals = mat->vals + row_s * mat->stride + col_s;
    sub->num_rows = num_rows;
    sub->num_cols = num_cols;
    sub->stride = mat->stride;
    sub->capacity = mat->capacity - (sub->vals - mat->vals);

    sub->is_const = true;

    return 0;
}

int mat_fill(mat_t *mat, size_t row_s, size_t num_rows,
        size_t col_s, size_t num_cols, mat_t *val)
{
    mat_t sub;

    ST_CHECK_PARAM(mat == NULL || val == NULL, -1);

    if (mat_submat(mat, row_s, num_rows, col_s, num_cols, &sub) < 0) {
        ST_WARNING("Failed to mat_submat.");
        return -1;
    }

    if (mat_cpy(&sub, val) < 0) {
        ST_WARNING("Failed to mat_cpy");
        return -1;
    }

    return 0;
}

void mat_scale(mat_t *mat, real_t scale)
{
    real_t *vals;
    size_t i, j;

    ST_CHECK_PARAM_VOID(mat == NULL);

    if (scale == 1.0) {
        return;
    }

    vals = mat->vals;
    for (i = 0; i < mat->num_rows; i++) {
        for (j = 0; j < mat->num_cols; j++) {
            vals[j] *= scale;
        }
        vals += mat->stride;
    }
}

void mat_cutoff(mat_t *mat, real_t cutoff)
{
    real_t *vals;
    size_t i, j;

    ST_CHECK_PARAM_VOID(mat == NULL || cutoff < 0);

    vals = mat->vals;
    for (i = 0; i < mat->num_rows; i++) {
        for (j = 0; j < mat->num_cols; j++) {
            if (vals[j] > cutoff) {
                vals[j] = cutoff;
            } else if (vals[j] < -cutoff) {
                vals[j] = -cutoff;
            }
        }
        vals += mat->stride;
    }
}

void mat_set(mat_t *mat, real_t val)
{
    real_t *vals;
    size_t i, j;

    ST_CHECK_PARAM_VOID(mat == NULL);

    if (val == 0.0) {
        memset(mat->vals, 0, sizeof(real_t) * mat->num_rows * mat->stride);
    } else {
        vals = mat->vals;
        for (i = 0; i < mat->num_rows; i++) {
            for (j = 0; j < mat->num_cols; j++) {
                vals[j] = val;
            }
            vals += mat->stride;
        }
    }
}

int mat_set_row(mat_t *mat, size_t row, real_t val)
{
    real_t *vals;
    size_t j;

    ST_CHECK_PARAM(mat == NULL, -1);

    if (row >= mat->num_rows) {
        ST_WARNING("Invalid row index [%zu/%zu]", row, mat->num_rows);
        return -1;
    }

    vals = mat->vals + row * mat->stride;
    if (val == 0.0) {
        memset(vals, 0, sizeof(real_t) * mat->num_cols);
    } else {
        for (j = 0; j < mat->num_cols; j++) {
            vals[j] = val;
        }
    }

    return 0;
}

void mat_from_array(mat_t *mat, real_t *arr, size_t len, bool row_vec)
{
    ST_CHECK_PARAM_VOID(mat == NULL || arr == NULL);

    mat->vals = arr;
    if (row_vec) {
        mat->num_rows = 1;
        mat->num_cols = len;
        mat->stride = len;
    } else {
        mat->num_rows = len;
        mat->num_cols = 1;
        mat->stride = 1;
    }
    mat->capacity = len;
    mat->is_const = true;
}

int mat_add_elems(mat_t *mat1, real_t s1, mat_t *mat2, real_t s2, mat_t *out)
{
    real_t *valso, *vals1, *vals2;
    size_t i, j;

    ST_CHECK_PARAM(mat1 == NULL || mat2 == NULL || out == NULL, -1);

    if (mat1->num_rows != mat2->num_rows || mat1->num_cols != mat2->num_cols
            || mat1->num_rows != out->num_rows
            || mat1->num_cols != out->num_cols) {
        ST_WARNING("Diemension not match.");
        return -1;
    }

    valso = out->vals;
    vals1 = mat1->vals;
    vals2 = mat2->vals;
    for (i = 0; i < mat1->num_rows; i++) {
        for (j = 0; j < mat1->num_cols; j++) {
            valso[j] = s1 * vals1[j] + s2 * vals2[j];
        }
        valso += out->stride;
        vals1 += mat1->stride;
        vals2 += mat2->stride;
    }

    return 0;
}

int mat_mul_elems(mat_t *mat1, mat_t *mat2, mat_t *out)
{
    real_t *valso, *vals1, *vals2;
    size_t i, j;

    ST_CHECK_PARAM(mat1 == NULL || mat2 == NULL || out == NULL, -1);

    if (mat1->num_rows != mat2->num_rows || mat1->num_cols != mat2->num_cols
            || mat1->num_rows != out->num_rows
            || mat1->num_cols != out->num_cols) {
        ST_WARNING("Diemension not match.");
        return -1;
    }

    valso = out->vals;
    vals1 = mat1->vals;
    vals2 = mat2->vals;
    for (i = 0; i < mat1->num_rows; i++) {
        for (j = 0; j < mat1->num_cols; j++) {
            valso[j] = vals1[j] * vals2[j];
        }
        valso += out->stride;
        vals1 += mat1->stride;
        vals2 += mat2->stride;
    }

    return 0;
}

int mat_add_vec(mat_t *mat, vec_t *vec, real_t scale)
{
    real_t *vals;
    size_t i, j;

    ST_CHECK_PARAM(mat == NULL || vec == NULL, -1);

    if (mat->num_cols != vec->size) {
        ST_WARNING("Diemension not match.");
        return -1;
    }

    if (scale != 1.0) {
        vals = mat->vals;
        for (i = 0; i < mat->num_rows; i++) {
            for (j = 0; j < mat->num_cols; j++) {
                vals[j] += scale * vec->vals[j];
            }
            vals += mat->stride;
        }
    } else {
        vals = mat->vals;
        for (i = 0; i < mat->num_rows; i++) {
            for (j = 0; j < mat->num_cols; j++) {
                vals[j] += vec->vals[j];
            }
            vals += mat->stride;
        }
    }

    return 0;
}

int add_mat_mat(real_t alpha, mat_t *A, mat_trans_t trans_A,
        mat_t *B, mat_trans_t trans_B, real_t beta, mat_t *C)
{
    size_t m, n, k;

    ST_CHECK_PARAM(A == NULL || B == NULL || C == NULL, -1);

    if (trans_A == MT_NoTrans && trans_B == MT_NoTrans) {
        if (A->num_cols != B->num_rows || A->num_rows != C->num_rows
                || B->num_cols != C->num_cols) {
            ST_WARNING("diemensions not match.");
            return -1;
        }
    } else if (trans_A == MT_Trans && trans_B == MT_NoTrans) {
        if (A->num_rows != B->num_rows || A->num_cols != C->num_rows
                || B->num_cols != C->num_cols) {
            ST_WARNING("diemensions not match.");
            return -1;
        }
    } else if (trans_A == MT_NoTrans && trans_B == MT_Trans) {
        if (A->num_cols != B->num_cols || A->num_rows != C->num_rows
                || B->num_rows != C->num_cols) {
            ST_WARNING("diemensions not match.");
            return -1;
        }
    } else if (trans_A == MT_Trans && trans_B == MT_Trans) {
        if (A->num_rows != B->num_cols || A->num_cols != C->num_rows
                || B->num_rows != C->num_cols) {
            ST_WARNING("diemensions not match.");
            return -1;
        }
    }

    m = C->num_rows;
    n = C->num_cols;
    if (trans_A == MT_NoTrans) {
        k = A->num_cols;
    } else {
        k = A->num_rows;
    }

#ifdef _USE_BLAS_
#ifdef _HAVE_MKL_
#  define CBLAS_TRANSPOSE_CAST CBLAS_TRANSPOSE
#else
#  define CBLAS_TRANSPOSE_CAST enum CBLAS_TRANSPOSE
#endif
    cblas_gemm(CblasRowMajor, (CBLAS_TRANSPOSE_CAST)trans_A,
            (CBLAS_TRANSPOSE_CAST)trans_B, m, n, k,
            alpha, A->vals, A->stride, B->vals, B->stride,
            beta, C->vals, C->stride);
#else
    real_t *aa, *bb, *cc, sum;
    size_t i, j, t;

    if (beta == 0.0) {
        memset(C->vals, 0, sizeof(real_t) * m * C->stride);
    } else if (beta != 1.0) {
        cc = C->vals;
        for (i = 0; i < m; i++) {
            for (j = 0; j < n; j++) {
                cc[j] *= beta;
            }
            cc += C->stride;
        }
    }

    if (trans_A == MT_NoTrans && trans_B == MT_NoTrans) {
        aa = A->vals;
        cc = C->vals;
        for (i = 0; i < m; i++) {
            for (j = 0; j < n; j++) {
                bb = B->vals + j;
                sum = 0.0;
                for (t = 0; t < k; t++) {
                    sum += aa[t] * bb[t * B->stride];
                }
                cc[j] += alpha * sum;
            }
            aa += A->stride;
            cc += C->stride;
        }
    } else if (trans_A == MT_Trans && trans_B == MT_NoTrans) {
        aa = A->vals;
        bb = B->vals;
        for (t = 0; t < k; t++) {
            cc = C->vals;
            for (i = 0; i < m; i++) {
                for (j = 0; j < n; j++) {
                    cc[j] += alpha * aa[i] * bb[j];
                }
                cc += C->stride;
            }
            aa += A->stride;
            bb += B->stride;
        }
    } else if (trans_A == MT_NoTrans && trans_B == MT_Trans) {
        aa = A->vals;
        cc = C->vals;
        for (i = 0; i < m; i++) {
            bb = B->vals;
            for (j = 0; j < n; j++) {
                sum = 0.0;
                for (t = 0; t < k; t++) {
                    sum += aa[t] * bb[t];
                }
                cc[j] += alpha * sum;
                bb += B->stride;
            }
            aa += A->stride;
            cc += C->stride;
        }
    } else if (trans_A == MT_Trans && trans_B == MT_Trans) {
        bb = B->vals;
        for (j = 0; j < n; j++) {
            cc = C->vals + j;
            for (i = 0; i < m; i++) {
                aa = A->vals + i;
                sum = 0.0;
                for (t = 0; t < k; t++) {
                    sum += aa[t * A->stride] * bb[t];
                }
                cc[i * C->stride] += alpha * sum;
            }
            bb += B->stride;
        }
    }
#endif

    return 0;
}

int vec_add_col_sum_mat(vec_t *vec, real_t alpha, mat_t *mat, real_t beta)
{
    size_t i, j;
    double sum;

    ST_CHECK_PARAM(vec == NULL || mat == NULL, -1);

    if (mat->num_cols != vec->size) {
        ST_WARNING("dimension not match.");
        return -1;
    }

    for (j = 0; j < vec->size; j++) {
        sum = 0.0;
        for (i = 0; i < mat->num_rows; i++) {
            sum += MAT_VAL(mat, i, j);
        }
        VEC_VAL(vec, j) = alpha * sum + beta * VEC_VAL(vec, j);
    }

    return 0;
}

int mat_load_header(mat_t *mat, int version,
        FILE *fp, connlm_fmt_t *fmt, FILE *fo_info)
{
    union {
        char str[4];
        int magic_num;
    } flag;

    size_t num_rows;
    size_t num_cols;

    ST_CHECK_PARAM(fp == NULL || fmt == NULL, -1);

    if (fread(&flag.magic_num, sizeof(int), 1, fp) != 1) {
        ST_WARNING("Failed to load magic num.");
        return -1;
    }

    *fmt = CONN_FMT_UNKNOWN;
    if (strncmp(flag.str, "    ", 4) == 0) {
        *fmt = CONN_FMT_TXT;
    } else if (MAT_MAGIC_NUM != flag.magic_num) {
        ST_WARNING("magic num wrong.");
        return -2;
    }

    if (mat != NULL) {
        memset(mat, 0, sizeof(mat_t));
    }

    if (*fmt != CONN_FMT_TXT) {
        if (fread(fmt, sizeof(connlm_fmt_t), 1, fp) != 1) {
            ST_WARNING("Failed to read fmt.");
            goto ERR;
        }

        if (fread(&num_rows, sizeof(size_t), 1, fp) != 1) {
            ST_WARNING("Failed to read num_rows.");
            goto ERR;
        }
        if (fread(&num_cols, sizeof(size_t), 1, fp) != 1) {
            ST_WARNING("Failed to read num_cols.");
            goto ERR;
        }
    } else {
        if (st_readline(fp, "") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }
        if (st_readline(fp, "<MATRIX>") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }

        if (st_readline(fp, "Row: %zu", &num_rows) != 1) {
            ST_WARNING("Failed to parse num_rows.");
            goto ERR;
        }

        if (st_readline(fp, "Col: %zu", &num_cols) != 1) {
            ST_WARNING("Failed to parse num_cols.");
            goto ERR;
        }
    }

    if (mat != NULL) {
        if (mat_resize(mat, num_rows, num_cols, NAN) < 0) {
            ST_WARNING("Failed to mat_resize.");
            goto ERR;
        }
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<MATRIX>\n");
        fprintf(fo_info, "Row: %zu\n", num_rows);
        fprintf(fo_info, "Col: %zu\n", num_cols);
    }

    return 0;

ERR:
    if (mat != NULL) {
        mat_destroy(mat);
    }
    return -1;
}

int mat_load_body(mat_t *mat, int version, FILE *fp, connlm_fmt_t fmt)
{
    char name[MAX_NAME_LEN];
    int n;
    size_t i, j;

    char *line = NULL;
    size_t line_sz = 0;
    bool err;
    const char *p;
    char token[MAX_NAME_LEN];

    ST_CHECK_PARAM(mat == NULL || fp == NULL, -1);

    if (connlm_fmt_is_bin(fmt)) {
        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read magic num.");
            return -1;
        }

        if (n != -MAT_MAGIC_NUM) {
            ST_WARNING("Magic num error.[%d]", n);
            return -1;
        }

        if (fmt == CONN_FMT_BIN) {
            for (i = 0; i < mat->num_rows; i++) {
                if (fread(MAT_VALP(mat, i, 0), sizeof(real_t),
                            mat->num_cols, fp) != mat->num_cols) {
                    ST_WARNING("Failed to read row[%zu].", i);
                    return -1;
                }
            }
        } else if (fmt & CONN_FMT_SHORT_QUANTIZATION) {
            if (fmt & CONN_FMT_ZEROS_COMPRESSED) {
                for (i = 0; i < mat->num_rows; i++) {
                    if (load_sq_zc(MAT_VALP(mat, i, 0), mat->num_cols, fp) < 0) {
                        ST_WARNING("Failed to load_sq_zc for row[%zu].", i);
                        return -1;
                    }
                }
            } else {
                for (i = 0; i < mat->num_rows; i++) {
                    if (load_sq(MAT_VALP(mat, i, 0), mat->num_cols, fp) < 0) {
                        ST_WARNING("Failed to load_sq for row[%zu].", i);
                        return -1;
                    }
                }
            }
        } else if (fmt & CONN_FMT_ZEROS_COMPRESSED) {
            for (i = 0; i < mat->num_rows; i++) {
                if (load_zc(MAT_VALP(mat, i, 0), mat->num_cols, fp) < 0) {
                    ST_WARNING("Failed to load_zc for row[%zu].", i);
                    return -1;
                }
            }
        }
    } else {
        if (st_readline(fp, "<MATRIX-DATA>") != 0) {
            ST_WARNING("body flag error.");
            goto ERR;
        }
        if (st_readline(fp, "%"xSTR(MAX_NAME_LEN)"s", name) != 1) {
            ST_WARNING("name error.");
            goto ERR;
        }

        if (st_fgets(&line, &line_sz, fp, &err) == NULL || err) {
            ST_WARNING("'[' expected.");
            goto ERR;
        }
        p = get_next_token(line, token);
        if (p == NULL || ! (p[0] == '[' && p[1] == '\0')) {
            ST_WARNING("'[' expected.");
            goto ERR;
        }
        for (i = 0; i < mat->num_rows; i++) {
            if (st_fgets(&line, &line_sz, fp, &err) == NULL || err) {
                ST_WARNING("Failed to parse row[%zu].", i);
                goto ERR;
            }
            p = line;
            for (j = 0; j < mat->num_cols; j++) {
                p = get_next_token(p, token);
                if (p == NULL || sscanf(token, REAL_FMT,
                            MAT_VALP(mat, i, j)) != 1) {
                    ST_WARNING("Failed to parse elem[%zu, %zu].", i, j);
                    goto ERR;
                }
            }
        }
        p = get_next_token(p, token);
        if (p == NULL || ! (p[0] == ']' && p[1] == '\0')) {
            ST_WARNING("']' expected.");
            goto ERR;
        }

        safe_st_free(line);
    }

    return 0;
ERR:

    safe_st_free(line);
    return -1;
}

int mat_save_header(mat_t *mat, FILE *fp, connlm_fmt_t fmt)
{
    ST_CHECK_PARAM(fp == NULL, -1);

    if (connlm_fmt_is_bin(fmt)) {
        if (fwrite(&MAT_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }
        if (fwrite(&fmt, sizeof(connlm_fmt_t), 1, fp) != 1) {
            ST_WARNING("Failed to write fmt.");
            return -1;
        }

        if (fwrite(&mat->num_rows, sizeof(size_t), 1, fp) != 1) {
            ST_WARNING("Failed to write num_rows.");
            return -1;
        }
        if (fwrite(&mat->num_cols, sizeof(size_t), 1, fp) != 1) {
            ST_WARNING("Failed to write num_cols.");
            return -1;
        }
    } else {
        if (fprintf(fp, "    \n<MATRIX>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }

        if (fprintf(fp, "Row: %zu\n", mat->num_rows) < 0) {
            ST_WARNING("Failed to fprintf num_rows.");
            return -1;
        }
        if (fprintf(fp, "Col: %zu\n", mat->num_cols) < 0) {
            ST_WARNING("Failed to fprintf num_cols.");
            return -1;
        }
    }

    return 0;
}

int mat_save_body(mat_t *mat, FILE *fp, connlm_fmt_t fmt, char *name)
{
    int n;
    size_t i, j;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (connlm_fmt_is_bin(fmt)) {
        n = -MAT_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        if (mat->num_rows <= 0 || mat->num_cols <= 0) {
            return 0;
        }

        if (fmt == CONN_FMT_BIN) {
            for (i = 0; i < mat->num_rows; i++) {
                if (fwrite(MAT_VALP(mat, i, 0), sizeof(real_t),
                            mat->num_cols, fp) != mat->num_cols) {
                    ST_WARNING("Failed to write row[%zu].", i);
                    return -1;
                }
            }
        } else if (fmt & CONN_FMT_SHORT_QUANTIZATION) {
            if (fmt & CONN_FMT_ZEROS_COMPRESSED) {
                for (i = 0; i < mat->num_rows; i++) {
                    if (save_sq_zc(MAT_VALP(mat, i, 0), mat->num_cols, fp) < 0) {
                        ST_WARNING("Failed to save_sq_zc for row[%zu].", i);
                        return -1;
                    }
                }
            } else {
                for (i = 0; i < mat->num_rows; i++) {
                    if (save_sq(MAT_VALP(mat, i, 0), mat->num_cols, fp) < 0) {
                        ST_WARNING("Failed to save_sq for row[%zu].", i);
                        return -1;
                    }
                }
            }
        } else if (fmt & CONN_FMT_ZEROS_COMPRESSED) {
            for (i = 0; i < mat->num_rows; i++) {
                if (save_zc(MAT_VALP(mat, i, 0), mat->num_cols, fp) < 0) {
                    ST_WARNING("Failed to save_zc for row[%zu].", i);
                    return -1;
                }
            }
        }
    } else {
        if (fprintf(fp, "<MATRIX-DATA>\n") < 0) {
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
        if (mat->num_rows > 0 && mat->num_cols > 0) {
            for (i = 0; i < mat->num_rows; i++) {
                for (j = 0; j < mat->num_cols - 1; j++) {
                    if (fprintf(fp, REAL_FMT" ", MAT_VAL(mat, i, j)) < 0) {
                        ST_WARNING("Failed to fprintf mat[%zu,%zu].", i, j);
                        return -1;
                    }
                }
                if (fprintf(fp, REAL_FMT"\n", MAT_VAL(mat, i, j)) < 0) {
                    ST_WARNING("Failed to fprintf mat[%zu/%zu].", i, j);
                    return -1;
                }
            }
        }
        if (fprintf(fp, "]\n") < 0) {
            ST_WARNING("Failed to fprintf ']'.");
            return -1;
        }
    }

    return 0;
}

void sp_mat_destroy(sp_mat_t *sp_mat)
{
    if (sp_mat == NULL) {
        return;
    }

    if (sp_mat->fmt == SP_MAT_CSR) {
        safe_st_aligned_free(sp_mat->csr.cols);
        safe_st_aligned_free(sp_mat->csr.row_s);
        safe_st_aligned_free(sp_mat->csr.row_e);
        sp_mat->csr.cap_rows = 0;
    } else if (sp_mat->fmt == SP_MAT_CSC) {
        safe_st_aligned_free(sp_mat->csc.rows);
        safe_st_aligned_free(sp_mat->csc.col_s);
        safe_st_aligned_free(sp_mat->csc.col_e);
        sp_mat->csc.cap_cols = 0;
    } else if (sp_mat->fmt == SP_MAT_COO) {
        safe_st_aligned_free(sp_mat->coo.rows);
        safe_st_aligned_free(sp_mat->coo.cols);
    }

    safe_st_aligned_free(sp_mat->vals);
    sp_mat->num_rows = 0;
    sp_mat->num_cols = 0;
    sp_mat->size = 0;
    sp_mat->capacity = 0;
}

static int sp_mat_resize_elems(sp_mat_t *sp_mat, size_t size)
{
    ST_CHECK_PARAM(sp_mat == NULL || size <= 0, -1);

    if (size > sp_mat->capacity) {
        sp_mat->vals = (real_t *)st_aligned_realloc(sp_mat->vals,
                sizeof(real_t) * size, ALIGN_SIZE);
        if (sp_mat->vals == NULL) {
            ST_WARNING("Failed to st_aligned_realloc sp_mat->vals.");
            return -1;
        }

        if (sp_mat->fmt == SP_MAT_COO) {
            sp_mat->coo.rows = (size_t *)st_aligned_realloc(sp_mat->coo.rows,
                    sizeof(size_t) * size, ALIGN_SIZE);
            if (sp_mat->coo.rows == NULL) {
                ST_WARNING("Failed to st_aligned_realloc sp_mat->coo.rows.");
                return -1;
            }
            sp_mat->coo.cols = (size_t *)st_aligned_realloc(sp_mat->coo.cols,
                    sizeof(size_t) * size, ALIGN_SIZE);
            if (sp_mat->coo.cols == NULL) {
                ST_WARNING("Failed to st_aligned_realloc sp_mat->coo.cols.");
                return -1;
            }
        }

        sp_mat->capacity = size;
    }
    sp_mat->size = size;

    return 0;
}

int sp_mat_resize(sp_mat_t *sp_mat, size_t size,
        size_t num_rows, size_t num_cols)
{
    ST_CHECK_PARAM(sp_mat == NULL || size <= 0 || num_rows <= 0
            || num_cols <= 0, -1);

    if (sp_mat_resize_elems(sp_mat, size) < 0) {
        ST_WARNING("Failed to sp_mat_resize_elems.");
        return -1;
    }

    if (sp_mat->fmt == SP_MAT_CSC) {
        if (num_cols > sp_mat->csc.cap_cols) {
            sp_mat->csc.col_s = (size_t *)st_aligned_realloc(sp_mat->csc.col_s,
                    sizeof(size_t) * num_cols, ALIGN_SIZE);
            if (sp_mat->csc.col_s == NULL) {
                ST_WARNING("Failed to st_aligned_realloc sp_mat->csc.col_s.");
                return -1;
            }

            sp_mat->csc.col_e = (size_t *)st_aligned_realloc(sp_mat->csc.col_e,
                    sizeof(size_t) * num_cols, ALIGN_SIZE);
            if (sp_mat->csc.col_e == NULL) {
                ST_WARNING("Failed to st_aligned_realloc sp_mat->csc.col_e.");
                return -1;
            }

            sp_mat->csc.cap_cols = num_cols;
        }
    } else if (sp_mat->fmt == SP_MAT_CSR) {
        if (num_rows > sp_mat->csr.cap_rows) {
            sp_mat->csr.row_s = (size_t *)st_aligned_realloc(sp_mat->csr.row_s,
                    sizeof(size_t) * num_rows, ALIGN_SIZE);
            if (sp_mat->csr.row_s == NULL) {
                ST_WARNING("Failed to st_aligned_realloc sp_mat->csr.row_s.");
                return -1;
            }

            sp_mat->csr.row_e = (size_t *)st_aligned_realloc(sp_mat->csr.row_e,
                    sizeof(size_t) * num_rows, ALIGN_SIZE);
            if (sp_mat->csr.row_e == NULL) {
                ST_WARNING("Failed to st_aligned_realloc sp_mat->csr.row_e.");
                return -1;
            }

            sp_mat->csr.cap_rows = num_rows;
        }
    }
    sp_mat->num_rows = num_rows;
    sp_mat->num_cols = num_cols;

    return 0;
}

int sp_mat_clear(sp_mat_t *sp_mat)
{
    ST_CHECK_PARAM(sp_mat == NULL, -1);

    sp_mat->size = 0;

    return 0;
}

int sp_mat_coo_add(sp_mat_t *sp_mat, size_t row, size_t col, real_t val)
{
    ST_CHECK_PARAM(sp_mat == NULL, -1);

    if (sp_mat->fmt != SP_MAT_COO) {
        ST_WARNING("Format not COO");
        return -1;
    }

    if (row >= sp_mat->num_rows) {
        ST_WARNING("row overflow[%zu/%zu].", row, sp_mat->num_rows);
        return -1;
    }

    if (col >= sp_mat->num_cols) {
        ST_WARNING("col overflow[%zu/%zu].", col, sp_mat->num_cols);
        return -1;
    }

    if (sp_mat_resize_elems(sp_mat, sp_mat->size + 1) < 0) {
        ST_WARNING("Failed to sp_mat_resize_elems.");
        return -1;
    }

    sp_mat->vals[sp_mat->size - 1] = val;
    sp_mat->coo.rows[sp_mat->size - 1] = row;
    sp_mat->coo.cols[sp_mat->size - 1] = col;

    return 0;
}
