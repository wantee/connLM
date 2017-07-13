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
#include <stutils/st_int.h>

#include "vector.h"

void svec_destroy(svec_t *vec)
{
    if (vec == NULL) {
        return;
    }

    safe_st_aligned_free(vec->vals);
    vec->size = 0;
    vec->capacity = 0;
}

int svec_clear(svec_t *vec)
{
    ST_CHECK_PARAM(vec == NULL, -1);

    vec->size = 0;

    return 0;
}

int svec_resize(svec_t *vec, int size, float init_val)
{
    int i;

    ST_CHECK_PARAM(vec == NULL || size <= 0, -1);

    if (size > vec->capacity) {
        vec->vals = (float *)st_aligned_realloc(vec->vals,
                sizeof(float) * size, ALIGN_SIZE);
        if (vec->vals == NULL) {
            ST_WARNING("Failed to st_aligned_realloc vec->vals.");
            return -1;
        }
        if (! isnan(init_val)) {
            if (init_val == 0.0) {
                memset(vec->vals + vec->capacity, 0,
                    sizeof(float) * (size - vec->capacity));
            } else {
                for (i = vec->capacity; i < size; i++) {
                    vec->vals[i] = init_val;
                }
            }
        }
        vec->capacity = size;
    }
    vec->size = size;

    return 0;
}

void dvec_destroy(dvec_t *vec)
{
    if (vec == NULL) {
        return;
    }

    safe_st_aligned_free(vec->vals);
    vec->size = 0;
    vec->capacity = 0;
}

int dvec_clear(dvec_t *vec)
{
    ST_CHECK_PARAM(vec == NULL, -1);

    vec->size = 0;

    return 0;
}

int dvec_resize(dvec_t *vec, int size, double init_val)
{
    int i;

    ST_CHECK_PARAM(vec == NULL || size <= 0, -1);

    if (size > vec->capacity) {
        vec->vals = (double *)st_aligned_realloc(vec->vals,
                sizeof(double) * size, ALIGN_SIZE);
        if (vec->vals == NULL) {
            ST_WARNING("Failed to st_aligned_realloc vec->vals.");
            return -1;
        }
        if (! isnan(init_val)) {
            if (init_val == 0.0) {
                memset(vec->vals + vec->capacity, 0,
                    sizeof(double) * (size - vec->capacity));
            } else {
                for (i = vec->capacity; i < size; i++) {
                    vec->vals[i] = init_val;
                }
            }
        }
        vec->capacity = size;
    }
    vec->size = size;

    return 0;
}

void ivec_destroy(ivec_t *vec)
{
    if (vec == NULL) {
        return;
    }

    safe_st_aligned_free(vec->vals);
    vec->size = 0;
    vec->capacity = 0;
}

int ivec_clear(ivec_t *vec)
{
    ST_CHECK_PARAM(vec == NULL, -1);

    vec->size = 0;

    return 0;
}

int ivec_resize(ivec_t *vec, int size)
{
    ST_CHECK_PARAM(vec == NULL || size <= 0, -1);

    if (size > vec->capacity) {
        vec->vals = (int *)st_aligned_realloc(vec->vals,
                sizeof(int) * size, ALIGN_SIZE);
        if (vec->vals == NULL) {
            ST_WARNING("Failed to st_aligned_realloc vec->vals.");
            return -1;
        }
        vec->capacity = size;
    }
    vec->size = size;

    return 0;
}

int ivec_insert(ivec_t *vec, int n)
{
    int pos;

    ST_CHECK_PARAM(vec == NULL, -1);

    if (ivec_resize(vec, vec->size + 1) < 0) {
        ST_WARNING("Failed to ivec_resize.");
        return -1;
    }

    pos = st_int_insert(vec->vals, vec->capacity, &vec->size, n);
    if (pos < 0) {
        ST_WARNING("Failed to st_int_insert.");
        return -1;
    }

    return pos;
}

int ivec_set(ivec_t *vec, int *vals, int n)
{
    ST_CHECK_PARAM(vec == NULL || vals == NULL, -1);

    if (ivec_resize(vec, n) < 0) {
        ST_WARNING("Failed to ivec_resize.");
        return -1;
    }

    memcpy(vec->vals, vals, sizeof(int) * n);
    vec->size = n;

    return 0;
}
