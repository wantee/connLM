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
#include "utils.h"
#include "vector.h"

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
    size_t num_rows; /**< number of rows. */
    size_t num_cols; /**< number of cols. */
    size_t stride; /**< true number of columns for the internal matrix. */
    size_t capacity; /**< capacity of vals. */
    bool is_const; /**< whether the diemention of matrix is const. */
} mat_t;

#define MAT_VAL(mat, row, col) ((mat)->vals[(row)*((mat)->stride) + (col)])
#define MAT_VALP(mat, row, col) ((mat)->vals + ((row)*((mat)->stride) + (col)))

/**
 * Load matrix header.
 * @ingroup g_matrix
 * @param[out] mat matrix to be initialised.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[out] fmt storage format.
 * @param[out] fo_info file stream used to print information, if it is not NULL.
 * @see mat_load_body
 * @see mat_save_header, mat_save_body
 * @return non-zero value if any error.
 */
int mat_load_header(mat_t *mat, int version,
        FILE *fp, connlm_fmt_t *fmt, FILE *fo_info);

/**
 * Load matrix body.
 * @ingroup g_matrix
 * @param[in] mat matrix to be loaded.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[in] fmt storage format.
 * @see mat_load_header
 * @see mat_save_header, mat_save_body
 * @return non-zero value if any error.
 */
int mat_load_body(mat_t *mat, int version, FILE *fp, connlm_fmt_t fmt);

/**
 * Save matrix header.
 * @ingroup g_matrix
 * @param[in] mat matrix to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] fmt storage format.
 * @see mat_save_body
 * @see mat_load_header, mat_load_body
 * @return non-zero value if any error.
 */
int mat_save_header(mat_t *mat, FILE *fp, connlm_fmt_t fmt);

/**
 * Save matrix body.
 * @ingroup g_matrix
 * @param[in] mat matrix to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] fmt storage format.
 * @param[in] name name of the weight.
 * @see mat_save_header
 * @see mat_load_header, mat_load_body
 * @return non-zero value if any error.
 */
int mat_save_body(mat_t *mat, FILE *fp, connlm_fmt_t fmt, char *name);

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
int mat_clear_row(mat_t *mat, size_t row);

/**
 * Resize a matrix.
 * @ingroup g_matrix
 * @param[in] mat the matrix.
 * @param[in] num_rows new number of rows.
 * @param[in] num_cols new number of cols.
 * @param[in] init_val initialization value, do not initialize if init_val == NAN
 * @return non-zero if any error.
 */
int mat_resize(mat_t *mat, size_t num_rows, size_t num_cols, real_t init_val);

/**
 * Resize row of a matrix.
 * @ingroup g_matrix
 * @param[in] mat the matrix.
 * @param[in] num_rows new number of rows.
 * @param[in] init_val initialization value, do not initialize if init_val == NAN
 * @return non-zero if any error.
 */
int mat_resize_row(mat_t *mat, size_t num_rows, real_t init_val);

/**
 * Append a matrix into a matrix, col must be the same.
 * @ingroup g_matrix
 * @param[out] dst dst matrix.
 * @param[in] src src matrix.
 * @return non-zero if any error.
 */
int mat_append(mat_t *dst, mat_t* src);

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
 * Check whether two matrice are equal.
 * @ingroup g_matrix
 * @param[in] mat1 the first matrix.
 * @param[in] mat2 the first matrix.
 * @return true if equal, false otherwise.
 */
bool mat_eq(mat_t *mat1, mat_t *mat2);

/**
 * Move rows in a matrix.
 * @ingroup g_matrix
 * @param[in] mat the matrix.
 * @param[in] dst_row the dst row of matrix.
 * @param[in] src_row the src row of matrix.
 * @return non-zero if any error.
 */
int mat_move_up(mat_t *mat, size_t dst_row, size_t src_row);

/**
 * Copy a row between two matrice.
 * num_cols of the two matrice must be equal.
 * @ingroup g_matrix
 * @param[in] dst the dst matrix.
 * @param[in] dst_row the row of dst matrix.
 * @param[in] src the src matrix.
 * @param[in] src_row the row of src matrix.
 * @see mat_acc_row
 * @return non-zero if any error.
 */
int mat_cpy_row(mat_t *dst, size_t dst_row, mat_t *src, size_t src_row);

/**
 * Accumulate a row from one matrice onto another.
 * num_cols of the two matrice must be equal.
 * @ingroup g_matrix
 * @param[in] dst the dst matrix.
 * @param[in] dst_row the row of dst matrix.
 * @param[in] src the src matrix.
 * @param[in] src_row the row of src matrix.
 * @see mat_cpy_row
 * @return non-zero if any error.
 */
int mat_acc_row(mat_t *dst, size_t dst_row, mat_t *src, size_t src_row);

/**
 * Extract a sub-matrix from one matrix.
 * @ingroup g_matrix
 * @param[in] mat the matrix.
 * @param[in] row_s start row index in original matrix.
 * @param[in] num_rows number of rows in sub-matrix.
 *                     set to zero to extract all the rest
 *                     rows from original matrix
 * @param[in] col_s start col index in original matrix.
 * @param[in] num_cols number of cols in sub-matrix,
 *                     set to zero to extract all the rest
 *                     cols from original matrix
 * @param[out] sub the sub-matrix.
 * @return non-zero if any error.
 */
int mat_submat(mat_t *mat, size_t row_s, size_t num_rows,
        size_t col_s, size_t num_cols, mat_t *sub);

/**
 * Fill a matrix with sub-matrix.
 * @ingroup g_matrix
 * @param[out] mat the matrix.
 * @param[in] row_s start row index in original matrix.
 * @param[in] num_rows number of rows in sub-matrix.
 *                     set to zero to extract all the rest
 *                     rows from original matrix
 * @param[in] col_s start col index in original matrix.
 * @param[in] num_cols number of cols in sub-matrix,
 *                     set to zero to extract all the rest
 *                     cols from original matrix
 * @param[in] val the sub-matrix contains values.
 * @return non-zero if any error.
 */
int mat_fill(mat_t *mat, size_t row_s, size_t num_rows,
        size_t col_s, size_t num_cols, mat_t *val);

/**
 * Scale elements in a matrix.
 * @ingroup g_matrix
 * @param[in] mat the matrix.
 * @param[in] scale the scale.
 */
void mat_scale(mat_t *mat, real_t scale);

/**
 * Cutoff elements in a matrix.
 * @ingroup g_matrix
 * @param[in] mat the matrix.
 * @param[in] cutoff the cutoff.
 */
void mat_cutoff(mat_t *mat, real_t cutoff);

/**
 * Set elements to a val in a matrix.
 * @ingroup g_matrix
 * @param[in] mat the matrix.
 * @param[in] val the value.
 */
void mat_set(mat_t *mat, real_t val);

/**
 * Set a row of elements to a val in a matrix.
 * @ingroup g_matrix
 * @param[in] mat the matrix.
 * @param[in] row the row index.
 * @param[in] val the value.
 * @return non-zero if any error.
 */
int mat_set_row(mat_t *mat, size_t row, real_t val);

/**
 * Construct a matrix from a array.
 * @ingroup g_matrix
 * @param[out] mat the output matrix.
 * @param[in] arr the array.
 * @param[in] len lenth of array.
 * @param[in] row_vec true for row vector, false for column vector.
 */
void mat_from_array(mat_t *mat, real_t *arr, size_t len, bool row_vec);

/**
 * Add element-by-element for two matrix.
 * out = s1 * mat1 + s2 * mat2
 * @ingroup g_matrix
 * @param[in] mat1 the first matrix.
 * @param[in] s1 the first scale.
 * @param[in] mat2 the second matrix.
 * @param[in] s2 the second scale.
 * @param[out] out the output matrix.
 * @return non-zero if any error.
 */
int mat_add_elems(mat_t *mat1, real_t s1, mat_t *mat2, real_t s2, mat_t *out);

/**
 * Multiple element-by-element for two matrix.
 * @ingroup g_matrix
 * @param[in] mat1 the first matrix.
 * @param[in] mat2 the second matrix.
 * @param[out] out the output matrix.
 * @return non-zero if any error.
 */
int mat_mul_elems(mat_t *mat1, mat_t *mat2, mat_t *out);

/**
 * Add vector for every row of matrix.
 * assert vec->size == mat->num_cols
*
 * @ingroup g_matrix
 * @param[in,out] mat the matrix.
 * @param[in] vec the vector.
 * @param[in] scale the scale
 * @return non-zero if any error.
 */
int mat_add_vec(mat_t *mat, vec_t *vec, real_t scale);

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
 * Sum the rows of the matrix, add to vector
 * Does vec = alpha * (sum of rows of mat) + beta * vec.
 * @ingroup g_vector
 * @param[out] vec the vector.
 * @param[in] alpha the vector scale.
 * @param[in] mat the matrix.
 * @param[in] beta the matrix scale.
 * @return non-zero if any error.
 */
int vec_add_col_sum_mat(vec_t *vec, real_t alpha, mat_t *mat, real_t beta);


/**
 * Sparse matrix format. see https://software.intel.com/en-us/mkl-developer-reference-c-sparse-blas-csr-matrix-storage-format
 * @ingroup g_matrix
 */
typedef enum _sparse_matrix_format_t_ {
    SP_MAT_UNSET = 0, /**< unset format. */
    SP_MAT_CSR = 1, /**< CSR format. */
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
    size_t num_rows; /**< number rows of the matrix. */
    size_t num_cols; /**< number cols of the matrix. */

    real_t *vals; /**< values of matrix. */
    size_t size; /**< number of values. */
    size_t capacity; /**< capacity of values. */

    union {
        struct _csr_t_ {
            size_t *cols; /**< col indexes for values. */
            size_t *row_s; /**< begin index for row. */
            size_t *row_e; /**< end index for row. */
            size_t cap_rows; /**< capacity of rows. */
        } csr;

        struct _csc_t_ {
            size_t *rows; /**< row indexes for values. */
            size_t *col_s; /**< begin index for col. */
            size_t *col_e; /**< end index for col. */
            size_t cap_cols; /**< capacity of cols. */
        } csc;

        struct _coo_t_ {
            size_t *rows; /**< row indexes for values. */
            size_t *cols; /**< col indexes for values. */
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

/**
 * Clear a sparse matrix.
 * @ingroup g_matrix
 * @param[in] sp_mat sparse matrix to be cleared.
 * @return non-zero if any error.
 */
int sp_mat_clear(sp_mat_t *sp_mat);

/**
 * Add an element to sparse matrix.
 * @ingroup g_matrix
 * @param[in] sp_mat the sparse matrix.
 * @param[in] row index of row.
 * @param[in] col index of col.
 * @param[in] val value of element.
 * @return non-zero if any error.
 */
int sp_mat_coo_add(sp_mat_t *sp_mat, size_t row, size_t col, real_t val);

#ifdef __cplusplus
}
#endif

#endif
