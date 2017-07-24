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
#include "blas.h"

/** @defgroup g_matrix Matrix
 */
typedef enum matrix_transpose_t_ {
#ifdef _USE_BLAS_
    MT_Trans    = CblasTrans,
    MT_NoTrans  = CblasNoTrans
#else
    MT_Trans = 0,
    MT_NoTrans = 1
#endif
} mat_trans_t;

/**
 * Matrix
 * @ingroup g_matrix
 */
typedef struct _matrix_t_ {
    real_t *vals; /**< values of matrix. */
    int num_rows; /**< number of rows. */
    int num_cols; /**< number of cols. */
    int capacity; /**< capacity of vals. */
    bool is_const; /**< whether the diemention of matrix is const. */
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
 * Clear one row of a matrix.
 * @ingroup g_matrix
 * @param[in] mat matrix to be cleared.
 * @param[in] row row index to be cleared.
 * @return non-zero if any error.
 */
int mat_clear_row(mat_t *mat, int row);

/**
 * Resize a matrix.
 * @ingroup g_matrix
 * @param[in] mat the matrix.
 * @param[in] num_rows new number of rows.
 * @param[in] num_cols new number of cols.
 * @param[in] init_val initialization value, do not initialize if init_val == NAN
 * @return non-zero if any error.
 */
int mat_resize(mat_t *mat, int num_rows, int num_cols, real_t init_val);

/**
 * Resize row of a matrix.
 * @ingroup g_matrix
 * @param[in] mat the matrix.
 * @param[in] num_rows new number of rows.
 * @param[in] init_val initialization value, do not initialize if init_val == NAN
 * @return non-zero if any error.
 */
int mat_resize_row(mat_t *mat, int num_rows, real_t init_val);

/**
 * Append a matrix into a matrix, col must be the same.
 * @ingroup g_matrix
 * @param[out] dst dst matrix.
 * @param[in] src src matrix.
 * @return non-zero if any error.
 */
int mat_append(mat_t *dst, mat_t* src);

/**
 * Append a row into a matrix.
 * @ingroup g_matrix
 * @param[out] mat the matrix.
 * @param[in] row values of row, size must be same as mat.num_cols.
 * @return non-zero if any error.
 */
int mat_append_row(mat_t *mat, real_t* row);

/**
 * Copy a matrix to the other.
 * @ingroup g_matrix
 * @param[out] dst the dst matrix.
 * @param[in] src the src matrix.
 * @return non-zero if any error.
 */
int mat_cpy(mat_t *dst, mat_t *src);

/**
 * Assing a matrix to the other.
 * @ingroup g_matrix
 * @param[in] dst the dst matrix.
 * @param[in] src the src matrix.
 */
void mat_assign(mat_t *dst, mat_t *src);

/**
 * Move rows in a matrix.
 * @ingroup g_matrix
 * @param[in] mat the matrix.
 * @param[in] dst_row the dst row of matrix.
 * @param[in] src_row the src row of matrix.
 * @return non-zero if any error.
 */
int mat_move_up(mat_t *mat, int dst_row, int src_row);

/**
 * Extract a sub-matrix from one matrix.
 * @ingroup g_matrix
 * @param[in] mat the matrix.
 * @param[in] row_s start row index in original matrix.
 * @param[in] num_rows number of rows in sub-matrix.
 *                     set to non-positive value to extract all the rest
 *                     rows from original matrix
 * @param[in] col_s start col index in original matrix.
 * @param[in] num_cols number of cols in sub-matrix,
 *                     set to non-positive value to extract all the rest
 *                     cols from original matrix
 * @param[out] sub the sub-matrix.
 * @return non-zero if any error.
 */
int mat_submat(mat_t *mat, int row_s, int num_rows,
        int col_s, int num_cols, mat_t *sub);

/**
 * Fill a matrix with sub-matrix.
 * @ingroup g_matrix
 * @param[out] mat the matrix.
 * @param[in] row_s start row index in original matrix.
 * @param[in] num_rows number of rows in sub-matrix.
 *                     set to non-positive value to extract all the rest
 *                     rows from original matrix
 * @param[in] col_s start col index in original matrix.
 * @param[in] num_cols number of cols in sub-matrix,
 *                     set to non-positive value to extract all the rest
 *                     cols from original matrix
 * @param[in] val the sub-matrix contains values.
 * @return non-zero if any error.
 */
int mat_fill(mat_t *mat, int row_s, int num_rows,
        int col_s, int num_cols, mat_t *val);

/**
 * Scale elements in a matrix.
 * @ingroup g_matrix
 * @param[in] mat the matrix.
 * @param[in] scale the scale.
 */
void mat_scale(mat_t *mat, real_t scale);

/**
 * Set elements to a val in a matrix.
 * @ingroup g_matrix
 * @param[in] mat the matrix.
 * @param[in] val the value.
 */
void mat_set_value(mat_t *mat, real_t val);

/**
 * Add element-by-element for two matrix.
 * @ingroup g_matrix
 * @param[in] mat1 the first matrix.
 * @param[in] mat2 the second matrix.
 * @param[out] out the output matrix.
 * @return non-zero if any error.
 */
int mat_add_elems(mat_t *mat1, mat_t *mat2, mat_t *out);

/**
 * Multiple element-by-element for two matrix.
 * @ingroup g_matrix
 * @param[in] mat1 the first matrix.
 * @param[in] mat2 the second matrix.
 * @param[out] out the output matrix.
 * @return non-zero if any error.
 */
int mat_mul_elems(mat_t *mat1, mat_t *mat2, mat_t *out);

/*
 * Computing C = alpha * A * B + beta * C
 * @ingroup g_matrix
 * @param[in] alpha scale alpha.
 * @param[in] A matrix A.
 * @param[in] trans_A transpose of A.
 * @param[in] B matrix B.
 * @param[in] trans_B transpose of B.
 * @param[in] beta scale beta.
 * @param[out] C the output matrix.
 * @return non-zero if any error.
 */
int add_mat_mat(real_t alpha, mat_t *A, mat_trans_t trans_A,
        mat_t *B, mat_trans_t trans_B, real_t beta, mat_t *C);

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
    int size; /**< number of values. */
    int capacity; /**< capacity of values. */

    union {
        struct _csr_t_ {
            int *cols; /**< col indexes for values. */
            int *row_s; /**< begin index for row. */
            int *row_e; /**< end index for row. */
            int num_rows; /**< number of rows. */
            int cap_rows; /**< capacity of rows. */
        } csr;

        struct _csc_t_ {
            int *rows; /**< row indexes for values. */
            int *col_s; /**< begin index for col. */
            int *col_e; /**< end index for col. */
            int num_cols; /**< number of cols. */
            int cap_cols; /**< capacity of cols. */
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
int sp_mat_resize(sp_mat_t *sp_mat, int size,
        int num_rows, int num_cols);

#ifdef __cplusplus
}
#endif

#endif
