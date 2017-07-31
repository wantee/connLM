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

#include "matrix.h"

static const int MAT_MAGIC_NUM = 626140498 + 80;

void mat_destroy(mat_t *mat)
{
    if (mat == NULL) {
        return;
    }

    safe_st_aligned_free(mat->vals);
    mat->num_rows = 0;
    mat->num_cols = 0;
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

    return 0;
}

int mat_resize(mat_t *mat, size_t num_rows, size_t num_cols, real_t init_val)
{
    size_t i;

    ST_CHECK_PARAM(mat == NULL, -1);

    if (mat->is_const) {
        if (mat->num_rows != num_rows || mat->num_cols != num_cols) {
            ST_WARNING("Can not resize a const matrix.");
            return -1;
        }
    }

    if (num_rows * num_cols > mat->capacity) {
        mat->vals = (real_t *)st_aligned_realloc(mat->vals,
                sizeof(real_t) * num_rows * num_cols, ALIGN_SIZE);
        if (mat->vals == NULL) {
            ST_WARNING("Failed to st_aligned_realloc mat->vals.");
            return -1;
        }
        if (! isnan(init_val)) {
            if (init_val == 0.0) {
                memset(mat->vals + mat->capacity, 0,
                    sizeof(real_t) * (num_rows * num_cols - mat->capacity));
            } else {
                for (i = mat->capacity; i < num_rows * num_cols; i++) {
                    mat->vals[i] = init_val;
                }
            }
        }
        mat->capacity = num_rows * num_cols;
    }
    mat->num_rows = num_rows;
    mat->num_cols = num_cols;

    return 0;
}

int mat_resize_row(mat_t *mat, size_t num_rows, real_t init_val)
{
    size_t i;

    ST_CHECK_PARAM(mat == NULL || num_rows <= 0, -1);

    if (mat->is_const) {
        if (mat->num_rows != num_rows) {
            ST_WARNING("Can not resize a const matrix.");
            return -1;
        }
    }

    if (num_rows * mat->num_cols > mat->capacity) {
        mat->vals = (real_t *)st_aligned_realloc(mat->vals,
                sizeof(real_t) * num_rows * mat->num_cols, ALIGN_SIZE);
        if (mat->vals == NULL) {
            ST_WARNING("Failed to st_aligned_realloc mat->vals.");
            return -1;
        }
        if (! isnan(init_val)) {
            if (init_val == 0.0) {
                memset(mat->vals + mat->capacity, 0,
                    sizeof(real_t) * (num_rows * mat->num_cols - mat->capacity));
            } else {
                for (i = mat->capacity; i < num_rows * mat->num_cols; i++) {
                    mat->vals[i] = init_val;
                }
            }
        }
        mat->capacity = num_rows * mat->num_cols;
    }
    mat->num_rows = num_rows;

    return 0;
}

int mat_append(mat_t *dst, mat_t* src)
{
    ST_CHECK_PARAM(dst == NULL || src == NULL, -1);

    if (dst->num_cols != src->num_cols) {
        ST_WARNING("num_cols not match");
        return -1;
    }

    if (mat_resize_row(dst, dst->num_rows + src->num_rows, NAN) < 0) {
        ST_WARNING("Failed to mat_resize_row.");
    }

    memcpy(dst->vals + dst->num_rows * dst->num_cols, src->vals,
            sizeof(real_t) * src->num_rows * src->num_cols);
    dst->num_rows += src->num_rows;

    return 0;
}

int mat_append_row(mat_t *mat, real_t* row)
{
    ST_CHECK_PARAM(mat == NULL || row == NULL, -1);

    if (mat_resize_row(mat, mat->num_rows + 1, NAN) < 0) {
        ST_WARNING("Failed to matrix_resize.");
        return -1;
    }

    memcpy(mat->vals + mat->num_rows * mat->num_cols, row,
            sizeof(real_t) * mat->num_cols);
    mat->num_rows++;

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
            sizeof(real_t) * src->num_rows * src->num_cols);

    return 0;
}

void mat_assign(mat_t *dst, mat_t *src)
{
    ST_CHECK_PARAM_VOID(dst == NULL || src == NULL);

    dst = src;
    dst->is_const = true;
}

int mat_move_up(mat_t *mat, size_t dst_row, size_t src_row)
{
    ST_CHECK_PARAM(mat == NULL, -1);

    if (dst_row >= src_row) {
        ST_WARNING("dst_row must be larger than src_row.");
        return -1;
    }

    memmove(mat->vals + dst_row * mat->num_cols,
            mat->vals + src_row * mat->num_cols,
            sizeof(real_t) * (mat->num_rows - src_row) * mat->num_cols);

    return 0;
}

int mat_submat(mat_t *mat, size_t row_s, size_t num_rows,
        size_t col_s, size_t num_cols, mat_t *sub)
{
    ST_CHECK_PARAM(mat == NULL || sub == NULL, -1);

    if (row_s >= mat->num_rows || col_s >= mat->num_cols) {
        ST_WARNING("Error row or col index.");
        return -1;
    }

    if (num_rows <= 0) {
        num_rows = mat->num_rows - row_s;
    } else {
        if (row_s + num_rows > mat->num_rows) {
            ST_WARNING("Not enough rows to extract [%d + %d > %d].",
                    row_s, num_rows, mat->num_rows);
            return -1;
        }
    }

    if (num_cols <= 0) {
        num_cols = mat->num_cols - col_s;
    } else {
        if (col_s + num_cols > mat->num_cols) {
            ST_WARNING("Not enough cols to extract [%d + %d > %d].",
                    col_s, num_cols, mat->num_cols);
            return -1;
        }
    }

    sub->vals = mat->vals + row_s * mat->num_cols + col_s;
    sub->num_rows = num_rows;
    sub->num_cols = num_cols;
    sub->capacity = num_rows * num_cols;

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
    size_t i;

    ST_CHECK_PARAM_VOID(mat == NULL);

    if (scale == 1.0) {
        return;
    }

    for (i = 0; i < mat->num_rows * mat->num_cols; i++) {
        mat->vals[i] *= scale;
    }
}

void mat_set(mat_t *mat, real_t val)
{
    size_t i;

    ST_CHECK_PARAM_VOID(mat == NULL);

    for (i = 0; i < mat->num_rows * mat->num_cols; i++) {
        mat->vals[i] = val;
    }
}

int mat_set_row(mat_t *mat, size_t row, real_t val)
{
    size_t i;

    ST_CHECK_PARAM(mat == NULL, -1);

    if (row >= mat->num_rows) {
        ST_WARNING("Invalid row index [%d/%d]", row, mat->num_rows);
        return -1;
    }

    for (i = row * mat->num_cols; i < (row + 1) * mat->num_cols; i++) {
        mat->vals[i] = val;
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
    } else {
        mat->num_rows = len;
        mat->num_cols = 1;
    }
    mat->capacity = len;
    mat->is_const = true;
}

int mat_add_elems(mat_t *mat1, mat_t *mat2, mat_t *out)
{
    size_t i;

    ST_CHECK_PARAM(mat1 == NULL || mat2 == NULL || out == NULL, -1);

    if (mat1->num_rows != mat2->num_rows || mat1->num_cols != mat2->num_cols
            || mat1->num_rows != out->num_rows
            || mat1->num_cols != out->num_cols) {
        ST_WARNING("Diemension not match.");
        return -1;
    }

    for (i = 0; i < mat1->num_rows * mat1->num_cols; i++) {
        out->vals[i] = mat1->vals[i] + mat2->vals[i];
    }

    return 0;
}

int mat_mul_elems(mat_t *mat1, mat_t *mat2, mat_t *out)
{
    size_t i;

    ST_CHECK_PARAM(mat1 == NULL || mat2 == NULL || out == NULL, -1);

    if (mat1->num_rows != mat2->num_rows || mat1->num_cols != mat2->num_cols
            || mat1->num_rows != out->num_rows
            || mat1->num_cols != out->num_cols) {
        ST_WARNING("Diemension not match.");
        return -1;
    }

    for (i = 0; i < mat1->num_rows * mat1->num_cols; i++) {
        out->vals[i] = mat1->vals[i] * mat2->vals[i];
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
    cblas_gemm(CblasRowMajor, (enum CBLAS_TRANSPOSE)trans_A,
            (enum CBLAS_TRANSPOSE)trans_B, m, n, k,
            alpha, A->vals, A->num_cols, B->vals, B->num_cols,
            beta, C->vals, C->num_cols);
#else
    real_t *aa, *bb, *cc, sum;
    size_t i, j, t;

    if (beta == 0.0) {
        memset(C->vals, 0, sizeof(real_t) * m * n);
    } else if (beta != 1.0) {
        for (i = 0; i < m * n; i++) {
            C->vals[i] *= beta;
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
                    sum += aa[t] * bb[t*n];
                }
                cc[j] = alpha * sum;
            }
            aa += k;
            cc += n;
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
                cc += n;
            }
            aa += m;
            bb += n;
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
                cc[j] = alpha * sum;
                bb += k;
            }
            aa += k;
            cc += n;
        }
    } else if (trans_A == MT_Trans && trans_B == MT_Trans) {
        bb = B->vals;
        cc = C->vals;
        for (i = 0; i < m; i++) {
            aa = A->vals + i;
            for (j = 0; j < n; j++) {
                sum = 0.0;
                for (t = 0; t < k; t++) {
                    sum += aa[t*m] * bb[t];
                }
                cc[j] = alpha * sum;
            }
            bb += k;
            cc += n;
        }
    }
#endif

    return 0;
}

int mat_load_header(mat_t *mat, int version,
        FILE *fp, connlm_fmt_t *fmt, FILE *fo_info)
{
    union {
        char str[4];
        int magic_num;
    } flag;
    connlm_fmt_t f;

    int row;
    int col;

    ST_CHECK_PARAM((mat == NULL && fo_info == NULL) || fp == NULL
            || fmt == NULL, -1);

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

        if (fread(&row, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read row.");
            goto ERR;
        }
        if (fread(&col, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read col.");
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

        if (st_readline(fp, "Row: %d", &row) != 1) {
            ST_WARNING("Failed to parse row.");
            goto ERR;
        }

        if (st_readline(fp, "Col: %d", &col) != 1) {
            ST_WARNING("Failed to parse col.");
            goto ERR;
        }
    }

    if (mat != NULL) {
        if (mat_resize(mat, row, col, NAN) < 0) {
            ST_WARNING("Failed to mat_resize.");
            goto ERR;
        }
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<MATRIX>\n");
        fprintf(fo_info, "Row: %d\n", row);
        fprintf(fo_info, "Col: %d\n", col);
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
    size_t sz;

    char *line = NULL;
    size_t line_sz = 0;
    bool err;
    const char *p;
    char token[MAX_NAME_LEN];

    ST_CHECK_PARAM(wt == NULL || fp == NULL, -1);

    if (connlm_fmt_is_bin(fmt)) {
        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read magic num.");
            goto ERR;
        }

        if (n != -MAT_MAGIC_NUM) {
            ST_WARNING("Magic num error.[%d]", n);
            goto ERR;
        }

        if (fmt == CONN_FMT_BIN) {
            if (fread(mat, sizeof(real_t), sz, fp) != sz) {
                ST_WARNING("Failed to read mat.");
                goto ERR;
            }

            if (wt->bias != NULL) {
                if (fread(wt->bias, sizeof(real_t), wt->row, fp) != wt->row) {
                    ST_WARNING("Failed to read bias.");
                    goto ERR;
                }
            }
        } else if (fmt & CONN_FMT_SHORT_QUANTIZATION) {
            if (fmt & CONN_FMT_ZEROS_COMPRESSED) {
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
                if (wt_load_sq(wt->mat, sz, fp) < 0) {
                    ST_WARNING("Failed to wt_load_sq for mat.");
                    return -1;
                }
                if (wt->bias != NULL) {
                    if (wt_load_sq(wt->bias, wt->row, fp) < 0) {
                        ST_WARNING("Failed to wt_load_sq for bias.");
                        return -1;
                    }
                }
            }
        } else if (fmt & CONN_FMT_ZEROS_COMPRESSED) {
            if (wt_load_zc(wt->mat, sz, fp) < 0) {
                ST_WARNING("Failed to wt_load_zc for mat.");
                goto ERR;
            }
            if (wt->bias != NULL) {
                if (wt_load_zc(wt->bias, wt->row, fp) < 0) {
                    ST_WARNING("Failed to wt_load_zc for bias.");
                    goto ERR;
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

    safe_st_free(line);
    return 0;
ERR:

    safe_st_free(line);
    wt_destroy(wt);
    return -1;
}

int wt_save_header(weight_t *wt, FILE *fp, connlm_fmt_t fmt)
{
    int n;

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
        n = -MAT_MAGIC_NUM;
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
        } else if (fmt & CONN_FMT_SHORT_QUANTIZATION) {
            if (fmt & CONN_FMT_ZEROS_COMPRESSED) {
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
        } else if (fmt & CONN_FMT_ZEROS_COMPRESSED) {
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
