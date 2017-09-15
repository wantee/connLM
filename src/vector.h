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
#include "utils.h"

/** @defgroup g_vector Vector
 */

#define VEC_VAL(vec, idx) ((vec)->vals[idx])
#define VEC_VALP(vec, idx) ((vec)->vals + (idx))

/**
 * Vector
 * @ingroup g_vector
 */
typedef struct _vector_t_ {
    real_t *vals; /**< values of vector. */
    size_t size; /**< size of vals. */
    size_t capacity; /**< capacity of vals. */
    bool is_const; /**< whether the diemention of vector is const. */
} vec_t;

/**
 * Load vector header.
 * @ingroup g_vector
 * @param[out] vec vector to be initialised.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[out] fmt storage format.
 * @param[out] fo_info file stream used to print information, if it is not NULL.
 * @see vec_load_body
 * @see vec_save_header, vec_save_body
 * @return non-zero value if any error.
 */
int vec_load_header(vec_t *vec, int version,
        FILE *fp, connlm_fmt_t *fmt, FILE *fo_info);

/**
 * Load vector body.
 * @ingroup g_vector
 * @param[in] vec vector to be loaded.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[in] fmt storage format.
 * @see vec_load_header
 * @see vec_save_header, vec_save_body
 * @return non-zero value if any error.
 */
int vec_load_body(vec_t *vec, int version, FILE *fp, connlm_fmt_t fmt);

/**
 * Save vector header.
 * @ingroup g_vector
 * @param[in] vec vector to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] fmt storage format.
 * @see vec_save_body
 * @see vec_load_header, vec_load_body
 * @return non-zero value if any error.
 */
int vec_save_header(vec_t *vec, FILE *fp, connlm_fmt_t fmt);

/**
 * Save vector body.
 * @ingroup g_vector
 * @param[in] vec vector to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] fmt storage format.
 * @param[in] name name of the weight.
 * @see vec_save_header
 * @see vec_load_header, vec_load_body
 * @return non-zero value if any error.
 */
int vec_save_body(vec_t *vec, FILE *fp, connlm_fmt_t fmt, char *name);

/**
 * Destroy a vector.
 * @ingroup g_vector
 * @param[in] vec vector to be destroyed.
 */
void vec_destroy(vec_t *vec);

/**
 * Clear a vector.
 * @ingroup g_vector
 * @param[in] vec vector to be cleared.
 * @return non-zero if any error.
 */
int vec_clear(vec_t *vec);

/**
 * Resize a vector.
 * @ingroup g_vector
 * @param[in] vec the vector.
 * @param[in] size new size.
 * @param[in] init_val initialization value, do not initialize if init_val == NAN
 * @return non-zero if any error.
 */
int vec_resize(vec_t *vec, size_t size, real_t init_val);

/**
 * Copy a vector to the other.
 * @ingroup g_vector
 * @param[out] dst the dst vector.
 * @param[in] src the src vector.
 * @return non-zero if any error.
 */
int vec_cpy(vec_t *dst, vec_t *src);

/**
 * Assing a vector to the other.
 * @ingroup g_vector
 * @param[in] dst the dst vector.
 * @param[in] src the src vector.
 */
void vec_assign(vec_t *dst, vec_t *src);

/**
 * Set elements to a val in a vector.
 * @ingroup g_vector
 * @param[in] vec the vector.
 * @param[in] val the value.
 */
void vec_set(vec_t *vec, real_t val);

/**
 * Extract a sub-vector from one vector.
 * @ingroup g_vector
 * @param[in] vec the vector.
 * @param[in] start start index in original vector.
 * @param[in] size size of sub-vector.
 *                 set to non-positive value to extract all the rest
 *                 elems from original vector
 * @param[out] sub the sub-vector.
 * @return non-zero if any error.
 */
int vec_subvec(vec_t *vec, size_t start, size_t size, vec_t *sub);


/**
 * Double Vector
 * @ingroup g_vector
 */
typedef struct _double_vector_t_ {
    double *vals; /**< values of vector. */
    size_t size; /**< size of vals. */
    size_t capacity; /**< capacity of vals. */
    bool is_const; /**< whether the diemention of vector is const. */
} dvec_t;

/**
 * Destroy a double vector.
 * @ingroup g_vector
 * @param[in] vec vector to be destroyed.
 */
void dvec_destroy(dvec_t *vec);

/**
 * Clear a double vector.
 * @ingroup g_vector
 * @param[in] vec vector to be cleared.
 * @return non-zero if any error.
 */
int dvec_clear(dvec_t *vec);

/**
 * Resize a double vector.
 * @ingroup g_vector
 * @param[in] vec the vector.
 * @param[in] size new size.
 * @param[in] init_val initialization value, do not initialize if init_val == NAN
 * @return non-zero if any error.
 */
int dvec_resize(dvec_t *vec, size_t size, double init_val);

/**
 * Set elements to a val in a double vector.
 * @ingroup g_vector
 * @param[in] vec the vector.
 * @param[in] val the value.
 */
void dvec_set(dvec_t *vec, double val);


/**
 * Int Vector
 * @ingroup g_vector
 */
typedef struct _int_vector_t_ {
    int *vals; /**< values of vector. */
    size_t size; /**< size of vals. */
    size_t capacity; /**< capacity of vals. */
} ivec_t;

/**
 * Destroy a int vector.
 * @ingroup g_vector
 * @param[in] vec vector to be destroyed.
 */
void ivec_destroy(ivec_t *vec);

/**
 * Clear a int vector.
 * @ingroup g_vector
 * @param[in] vec vector to be cleared.
 * @return non-zero if any error.
 */
int ivec_clear(ivec_t *vec);

/**
 * Resize a int vector.
 * @ingroup g_vector
 * @param[in] vec the vector.
 * @param[in] size new size.
 * @return non-zero if any error.
 */
int ivec_resize(ivec_t *vec, size_t size);

/**
 * Insert a int into a int vector. same numbers are merged.
 * @ingroup g_vector
 * @param[in] vec the vector, must be uniq and sorted.
 * @param[in] n the number.
 * @return position of the number in vecitor, -1 if any error.
 */
int ivec_insert(ivec_t *vec, int n);

/**
 * Set value of vector.
 * @ingroup g_vector
 * @param[in] vec the vector
 * @param[in] vals the numbers.
 * @param[in] n number of vals.
 * @return non-zero if any error.
 */
int ivec_set(ivec_t *vec, int *vals, size_t n);

#ifdef __cplusplus
}
#endif

#endif
