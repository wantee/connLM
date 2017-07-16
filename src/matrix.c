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

    memset(mat->vals, 0, sizeof(real_t) * mat->num_rows * mat->num_cols);

    return 0;
}

int mat_clear_row(mat_t *mat, int row)
{
    ST_CHECK_PARAM(mat == NULL, -1);

    memset(mat->vals + row * mat->num_cols, 0, sizeof(real_t) * mat->num_cols);

    return 0;
}

int mat_resize(mat_t *mat, int num_rows, int num_cols, real_t init_val)
{
    int i;

    ST_CHECK_PARAM(mat == NULL || num_rows <= 0 || num_cols <= 0, -1);

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

int mat_resize_row(mat_t *mat, int num_rows, real_t init_val)
{
    int i;

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

int mat_append_row(mat_t *mat, real_t* row)
{
    ST_CHECK_PARAM(mat == NULL || row == NULL, -1);

    if (mat_resize(mat, mat->num_rows + 1, mat->num_cols, NAN) < 0) {
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

int mat_submat(mat_t *mat, int row_s, int num_rows,
        int col_s, int num_cols, mat_t *sub)
{
    ST_CHECK_PARAM(mat == NULL || sub == NULL || row_s < 0 || col_s < 0, -1);

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

    sub->is_const = true;

    return 0;
}

void mat_scale(mat_t *mat, real_t scale)
{
    int i;

    ST_CHECK_PARAM_VOID(mat == NULL);

    if (scale == 1.0) {
        return;
    }

    for (i = 0; i < mat->num_rows * mat->num_cols; i++) {
        mat->vals[i] *= scale;
    }
}

int mat_mul_elems(mat_t *mat1, mat_t *mat2, mat_t *out)
{
    int i;

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

void sp_mat_destroy(sp_mat_t *sp_mat)
{
    if (sp_mat == NULL) {
        return;
    }

    if (sp_mat->fmt == SP_MAT_CSR) {
        safe_st_aligned_free(sp_mat->csr.cols);
        safe_st_aligned_free(sp_mat->csr.row_s);
        safe_st_aligned_free(sp_mat->csr.row_e);
        sp_mat->csr.num_rows = 0;
        sp_mat->csr.cap_rows = 0;
    } else if (sp_mat->fmt == SP_MAT_CSC) {
        safe_st_aligned_free(sp_mat->csc.rows);
        safe_st_aligned_free(sp_mat->csc.col_s);
        safe_st_aligned_free(sp_mat->csc.col_e);
        sp_mat->csc.num_cols = 0;
        sp_mat->csc.cap_cols = 0;
    } else if (sp_mat->fmt == SP_MAT_COO) {
        safe_st_aligned_free(sp_mat->coo.rows);
        safe_st_aligned_free(sp_mat->coo.cols);
    }

    safe_st_aligned_free(sp_mat->vals);
    sp_mat->size = 0;
    sp_mat->capacity = 0;
}

int sp_mat_resize(sp_mat_t *sp_mat, int size,
        int num_rows, int num_cols)
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
            sp_mat->coo.rows = (int *)st_aligned_realloc(sp_mat->coo.rows,
                    sizeof(int) * size, ALIGN_SIZE);
            if (sp_mat->coo.rows == NULL) {
                ST_WARNING("Failed to st_aligned_realloc sp_mat->coo.rows.");
                return -1;
            }
            sp_mat->coo.cols = (int *)st_aligned_realloc(sp_mat->coo.cols,
                    sizeof(int) * size, ALIGN_SIZE);
            if (sp_mat->coo.cols == NULL) {
                ST_WARNING("Failed to st_aligned_realloc sp_mat->coo.cols.");
                return -1;
            }
        }

        sp_mat->capacity = size;
    }
    sp_mat->size = size;

    if (sp_mat->fmt == SP_MAT_CSC) {
        ST_CHECK_PARAM(num_cols <= 0, -1);

        if (num_cols > sp_mat->csc.cap_cols) {
            sp_mat->csc.col_s = (int *)st_aligned_realloc(sp_mat->csc.col_s,
                    sizeof(int) * num_cols, ALIGN_SIZE);
            if (sp_mat->csc.col_s == NULL) {
                ST_WARNING("Failed to st_aligned_realloc sp_mat->csc.col_s.");
                return -1;
            }

            sp_mat->csc.col_e = (int *)st_aligned_realloc(sp_mat->csc.col_e,
                    sizeof(int) * num_cols, ALIGN_SIZE);
            if (sp_mat->csc.col_e == NULL) {
                ST_WARNING("Failed to st_aligned_realloc sp_mat->csc.col_e.");
                return -1;
            }

            sp_mat->csc.cap_cols = num_cols;
        }
        sp_mat->csc.num_cols = num_cols;
    } else if (sp_mat->fmt == SP_MAT_CSR) {
        ST_CHECK_PARAM(num_rows <= 0, -1);

        if (num_rows > sp_mat->csr.cap_rows) {
            sp_mat->csr.row_s = (int *)st_aligned_realloc(sp_mat->csr.row_s,
                    sizeof(int) * num_rows, ALIGN_SIZE);
            if (sp_mat->csr.row_s == NULL) {
                ST_WARNING("Failed to st_aligned_realloc sp_mat->csr.row_s.");
                return -1;
            }

            sp_mat->csr.row_e = (int *)st_aligned_realloc(sp_mat->csr.row_e,
                    sizeof(int) * num_rows, ALIGN_SIZE);
            if (sp_mat->csr.row_e == NULL) {
                ST_WARNING("Failed to st_aligned_realloc sp_mat->csr.row_e.");
                return -1;
            }

            sp_mat->csr.cap_rows = num_rows;
        }
        sp_mat->csr.num_rows = num_rows;
    }

    return 0;
}
