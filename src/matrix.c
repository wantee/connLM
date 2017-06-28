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

#include "matrix.h"

void matrix_destroy(matrix_t *mat)
{
    if (mat == NULL) {
        return;
    }

    safe_st_free(mat->vals);
    mat->rows = 0;
    mat->cols = 0;
    mat->capacity = 0;
}

int matrix_resize(matrix_t *mat, size_t rows, size_t cols)
{
    ST_CHECK_PARAM(mat == NULL || rows <= 0 || cols <= 0, -1);

    if (rows * cols > mat->capacity) {
        mat->vals = (real_t *)st_realloc(mat->vals,
                sizeof(real_t) * rows * cols);
        if (mat->vals == NULL) {
            ST_WARNING("Failed to st_malloc mat->vals.");
            return -1;
        }
        mat->capacity = rows * cols;
    }
    mat->rows = rows;
    mat->cols = cols;

    return 0;
}
