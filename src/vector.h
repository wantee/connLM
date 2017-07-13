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

#ifndef  _CONNLM_VECTOR_H
#define  _CONNLM_VECTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <connlm/config.h>

/** @defgroup g_vector Vector
 */

#if _USE_DOUBLE_ == 1
#define dvec_t vec_t
#define dvec_destroy vec_destroy
#define dvec_clear vec_clear
#define dvec_resize vec_resize
#else
#define svec_t vec_t
#define svec_destroy vec_destroy
#define svec_clear vec_clear
#define svec_resize vec_resize
#endif

#define VEC_VAL(vec, idx) ((vec)->vals[idx])
#define VEC_VALP(vec, idx) ((vec)->vals + (idx))

/**
 * Single Float Vector
 * @ingroup g_vector
 */
typedef struct _single_vector_t_ {
    float *vals; /**< values of vector. */
    int size; /**< size of vals. */
    int capacity; /**< capacity of vals. */
} svec_t;

/**
 * Destroy a single float vector.
 * @ingroup g_vector
 * @param[in] vec vector to be destroyed.
 */
void svec_destroy(svec_t *vec);

/**
 * Clear a single float vector.
 * @ingroup g_vector
 * @param[in] vec vector to be cleared.
 * @return non-zero if any error.
 */
int svec_clear(svec_t *vec);

/**
 * Resize a single float vector.
 * @ingroup g_vector
 * @param[in] vec the vector.
 * @param[in] size new size.
 * @return non-zero if any error.
 */
int svec_resize(svec_t *vec, int size);


/**
 * Double Float Vector
 * @ingroup g_vector
 */
typedef struct _double_vector_t_ {
    double *vals; /**< values of vector. */
    int size; /**< size of vals. */
    int capacity; /**< capacity of vals. */
} dvec_t;

/**
 * Destroy a double float vector.
 * @ingroup g_vector
 * @param[in] vec vector to be destroyed.
 */
void dvec_destroy(dvec_t *vec);

/**
 * Clear a double float vector.
 * @ingroup g_vector
 * @param[in] vec vector to be cleared.
 * @return non-zero if any error.
 */
int dvec_clear(dvec_t *vec);

/**
 * Resize a double float vector.
 * @ingroup g_vector
 * @param[in] vec the vector.
 * @param[in] size new size.
 * @return non-zero if any error.
 */
int dvec_resize(dvec_t *vec, int size);

/**
 * Int Vector
 * @ingroup g_vector
 */
typedef struct _int_vector_t_ {
    int *vals; /**< values of vector. */
    int size; /**< size of vals. */
    int capacity; /**< capacity of vals. */
} ivec_t;

/**
 * Destroy a int float vector.
 * @ingroup g_vector
 * @param[in] vec vector to be destroyed.
 */
void ivec_destroy(ivec_t *vec);

/**
 * Clear a int float vector.
 * @ingroup g_vector
 * @param[in] vec vector to be cleared.
 * @return non-zero if any error.
 */
int ivec_clear(ivec_t *vec);

/**
 * Resize a int float vector.
 * @ingroup g_vector
 * @param[in] vec the vector.
 * @param[in] size new size.
 * @return non-zero if any error.
 */
int ivec_resize(ivec_t *vec, int size);

/**
 * Insert a int into a int vector. same numbers are merged.
 * @ingroup g_vector
 * @param[in] vec the vector, must be uniq and sorted.
 * @param[in] n the number.
 * @return position of the number in vecitor, -1 if any error.
 */
int ivec_insert(ivec_t *vec, int n);

#ifdef __cplusplus
}
#endif

#endif
