/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Wang Jian
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

#include <stutils/st_log.h>
#include <stutils/st_rand.h>

#include "utils.h"
#include "relu_layer.h"

int relu_activate(layer_t *layer, mat_t *ac)
{
    int i, j;

    ST_CHECK_PARAM(layer == NULL || ac == NULL, -1);

    for (i = 0; i < ac->num_rows; i++) {
        for (j = 0; j < ac->num_cols; j++) {
            if (MAT_VAL(ac, i, j) < 0) {
                MAT_VAL(ac, i, j) = 0.0;
            }
        }
    }

    return 0;
}

int relu_deriv(layer_t *layer, mat_t *er, mat_t *ac)
{
    int i, j;

    ST_CHECK_PARAM(layer == NULL || er == NULL || ac == NULL, -1);

    if (er->num_rows != ac->num_rows || er->num_cols != ac->num_cols) {
        ST_ERROR("er and ac size not match");
        return -1;
    }

    for (i = 0; i < er->num_rows; i++) {
        for (j = 0; j < er->num_cols; j++) {
            if (MAT_VAL(ac, i, j) <= 0) {
                MAT_VAL(er, i, j) = 0.0;
            }
        }
    }

    return 0;
}

int relu_random_state(layer_t *layer, mat_t *state)
{
    int i, j;

    ST_CHECK_PARAM(layer == NULL || state == NULL, -1);

    for (i = 0; i < state->num_rows; i++) {
        for (j = 0; j < state->num_cols; j++) {
            MAT_VAL(state, i, j) = st_random(-50.0, 50.0);
            if (MAT_VAL(state, i, j) < 0) {
                MAT_VAL(state, i, j) = 0.0;
            }
        }
    }

    return 0;
}
