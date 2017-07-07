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
    size_t rows; /**< number of rows. */
    size_t cols; /**< number of cols. */
    size_t capacity; /**< capacity of val. */
} matrix_t;

#define MAT_VAL(mat, row, col) ((mat)->vals[(row)*((mat)->cols) + (col)])

/**
 * Destroy a matrix.
 * @ingroup g_matrix
 * @param[in] matrix matrix to be destroyed.
 */
void matrix_destroy(matrix_t *mat);

/**
 * Resize a matrix.
 * @ingroup g_matrix
 * @param[in] mat the matrix.
 * @param[in] rows new number of rows.
 * @param[in] cols new number of cols.
 * @return non-zero if any error.
 */
int matrix_resize(matrix_t *mat, size_t rows, size_t cols);

#ifdef __cplusplus
}
#endif

#endif
