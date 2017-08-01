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

#if _USE_DOUBLE_ == 1
#define dvec_t vec_t
#define dvec_load_header vec_load_header
#define dvec_load_body vec_load_body
#define dvec_save_header vec_save_header
#define dvec_save_body vec_save_body
#define dvec_destroy vec_destroy
#define dvec_clear vec_clear
#define dvec_resize vec_resize
#define dvec_cpy vec_cpy
#define dvec_set vec_set
#else
#define svec_t vec_t
#define svec_load_header vec_load_header
#define svec_load_body vec_load_body
#define svec_save_header vec_save_header
#define svec_save_body vec_save_body
#define svec_destroy vec_destroy
#define svec_clear vec_clear
#define svec_resize vec_resize
#define svec_cpy vec_cpy
#define svec_set vec_set
#endif

#define VEC_VAL(vec, idx) ((vec)->vals[idx])
#define VEC_VALP(vec, idx) ((vec)->vals + (idx))

/**
 * Single Float Vector
 * @ingroup g_vector
 */
typedef struct _single_vector_t_ {
    float *vals; /**< values of vector. */
    size_t size; /**< size of vals. */
    size_t capacity; /**< capacity of vals. */
} svec_t;

/**
 * Load single float vector header.
 * @ingroup g_vector
 * @param[out] vec vector to be initialised.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[out] fmt storage format.
 * @param[out] fo_info file stream used to print information, if it is not NULL.
 * @see svec_load_body
 * @see svec_save_header, svec_save_body
 * @return non-zero value if any error.
 */
int svec_load_header(svec_t *vec, int version,
        FILE *fp, connlm_fmt_t *fmt, FILE *fo_info);

/**
 * Load single float vector body.
 * @ingroup g_vector
 * @param[in] vec vector to be loaded.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[in] fmt storage format.
 * @see svec_load_header
 * @see svec_save_header, svec_save_body
 * @return non-zero value if any error.
 */
int svec_load_body(svec_t *vec, int version, FILE *fp, connlm_fmt_t fmt);

/**
 * Save single float vector header.
 * @ingroup g_vector
 * @param[in] vec vector to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] fmt storage format.
 * @see svec_save_body
 * @see svec_load_header, svec_load_body
 * @return non-zero value if any error.
 */
int svec_save_header(svec_t *vec, FILE *fp, connlm_fmt_t fmt);

/**
 * Save single float vector body.
 * @ingroup g_vector
 * @param[in] vec vector to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] fmt storage format.
 * @param[in] name name of the weight.
 * @see svec_save_header
 * @see svec_load_header, svec_load_body
 * @return non-zero value if any error.
 */
int svec_save_body(svec_t *vec, FILE *fp, connlm_fmt_t fmt, char *name);

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
 * @param[in] init_val initialization value, do not initialize if init_val == NAN
 * @return non-zero if any error.
 */
int svec_resize(svec_t *vec, size_t size, float init_val);

/**
 * Copy a single float vector to the other.
 * @ingroup g_vector
 * @param[out] dst the dst vector.
 * @param[in] src the src vector.
 * @return non-zero if any error.
 */
int svec_cpy(svec_t *dst, svec_t *src);

/**
 * Set elements to a val in a vector.
 * @ingroup g_vector
 * @param[in] vec the vector.
 * @param[in] val the value.
 */
void svec_set(svec_t *vec, float val);


/**
 * Double Float Vector
 * @ingroup g_vector
 */
typedef struct _double_vector_t_ {
    double *vals; /**< values of vector. */
    size_t size; /**< size of vals. */
    size_t capacity; /**< capacity of vals. */
} dvec_t;

/**
 * Load double float vector header.
 * @ingroup g_vector
 * @param[out] vec vector to be initialised.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[out] fmt storage format.
 * @param[out] fo_info file stream used to print information, if it is not NULL.
 * @see dvec_load_body
 * @see dvec_save_header, dvec_save_body
 * @return non-zero value if any error.
 */
int dvec_load_header(dvec_t *vec, int version,
        FILE *fp, connlm_fmt_t *fmt, FILE *fo_info);

/**
 * Load double float vector body.
 * @ingroup g_vector
 * @param[in] vec vector to be loaded.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[in] fmt storage format.
 * @see dvec_load_header
 * @see dvec_save_header, dvec_save_body
 * @return non-zero value if any error.
 */
int dvec_load_body(dvec_t *vec, int version, FILE *fp, connlm_fmt_t fmt);

/**
 * Save double float vector header.
 * @ingroup g_vector
 * @param[in] vec vector to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] fmt storage format.
 * @see dvec_save_body
 * @see dvec_load_header, dvec_load_body
 * @return non-zero value if any error.
 */
int dvec_save_header(dvec_t *vec, FILE *fp, connlm_fmt_t fmt);

/**
 * Save double float vector body.
 * @ingroup g_vector
 * @param[in] vec vector to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] fmt storage format.
 * @param[in] name name of the weight.
 * @see dvec_save_header
 * @see dvec_load_header, dvec_load_body
 * @return non-zero value if any error.
 */
int dvec_save_body(dvec_t *vec, FILE *fp, connlm_fmt_t fmt, char *name);

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
 * @param[in] init_val initialization value, do not initialize if init_val == NAN
 * @return non-zero if any error.
 */
int dvec_resize(dvec_t *vec, size_t size, double init_val);

/**
 * Copy a double float vector to the other.
 * @ingroup g_vector
 * @param[out] dst the dst vector.
 * @param[in] src the src vector.
 * @return non-zero if any error.
 */
int dvec_cpy(dvec_t *dst, dvec_t *src);

/**
 * Set elements to a val in a vector.
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
