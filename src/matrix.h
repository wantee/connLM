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

#ifndef  _CONNLM_MATRIX_H_
#define  _CONNLM_MATRIX_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <connlm/config.h>

/** @defgroup g_matrix Matrix
 */

/**
 * Matrix
 * @ingroup g_matrix
 */
typedef struct _matrix_t_ {
    real_t *vals; /**< values of matrix. */
    size_t num_rows; /**< number of rows. */
    size_t num_cols; /**< number of cols. */
    size_t capacity; /**< capacity of vals. */
} mat_t;

#define MAT_VAL(mat, row, col) ((mat)->vals[(row)*((mat)->num_cols) + (col)])
#define MAT_VALP(mat, row, col) ((mat)->vals + ((row)*((mat)->num_cols) + (col)))

/**
 * Destroy a matrix.
 * @ingroup g_matrix
 * @param[in] mat matrix to be destroyed.
 */
void mat_destroy(mat_t *mat);

/**
 * Clear a matrix.
 * @ingroup g_matrix
 * @param[in] mat matrix to be cleared.
 * @return non-zero if any error.
 */
int mat_clear(mat_t *mat);

/**
 * Resize a matrix.
 * @ingroup g_matrix
 * @param[in] mat the matrix.
 * @param[in] rows new number of rows.
 * @param[in] cols new number of cols.
 * @return non-zero if any error.
 */
int mat_resize(mat_t *mat, size_t num_rows, size_t num_cols);

/**
 * Append a row into a matrix.
 * @ingroup g_matrix
 * @param[in] mat the matrix.
 * @param[in] row values of row, size must be same as mat.num_cols.
 * @return non-zero if any error.
 */
int mat_append_row(mat_t *mat, real_t* row);

/**
 * Sparse matrix format. see https://software.intel.com/en-us/mkl-developer-reference-c-sparse-blas-csr-matrix-storage-format
 * @ingroup g_matrix
 */
typedef enum _sparse_matrix_format_t_ {
    SP_MAT_CSR = 0, /**< CSR format. */
    SP_MAT_CSC, /**< CSC format. */
    SP_MAT_COO, /**< COO format. */
} sp_mat_fmt_t;

/**
 * Sparse Matrix.
 * We only support CSC format now.
 * @ingroup g_matrix
 */
typedef struct _sparse_matrix_t_ {
    sp_mat_fmt_t fmt; /**< format. */
    real_t *vals; /**< values of matrix. */
    size_t size; /**< number of values. */
    size_t capacity; /**< capacity of values. */

    union {
        struct _csr_t_ {
            int *cols; /**< col indexes for values. */
            int *row_s; /**< begin index for row. */
            int *row_e; /**< end index for row. */
            size_t num_rows; /**< number of rows. */
            size_t cap_rows; /**< capacity of rows. */
        } csr;

        struct _csc_t_ {
            int *rows; /**< row indexes for values. */
            int *col_s; /**< begin index for col. */
            int *col_e; /**< end index for col. */
            size_t num_cols; /**< number of cols. */
            size_t cap_cols; /**< capacity of cols. */
        } csc;

        struct _coo_t_ {
            int *rows; /**< row indexes for values. */
            int *cols; /**< col indexes for values. */
        } coo;
    };
} sp_mat_t;

/**
 * Destroy a sparse matrix.
 * @ingroup g_matrix
 * @param[in] sp_mat sparse matrix to be destroyed.
 */
void sp_mat_destroy(sp_mat_t *sp_mat);

/**
 * Resize a sparse matrix.
 * @ingroup g_matrix
 * @param[in] sp_mat the sparse matrix.
 * @param[in] size new number of values.
 * @param[in] num_rows new number of rows.
 * @param[in] num_cols new number of cols.
 * @return non-zero if any error.
 */
int sp_mat_resize(sp_mat_t *sp_mat, size_t size,
        size_t num_rows, size_t num_cols);

#ifdef __cplusplus
}
#endif

#endif
