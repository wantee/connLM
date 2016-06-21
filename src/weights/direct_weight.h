/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Wang Jian
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

#ifndef  _CONNLM_DIRECT_WEIGHT_H_
#define  _CONNLM_DIRECT_WEIGHT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <connlm/config.h>

#include "input.h"
#include "output.h"

/** @defgroup g_wt_direct NNet direct weight.
 * @ingroup g_weight
 * Data structures and functions for NNet direct weight.
 */

#define MAX_HASH_ORDER 16
/**
 * Direct weight.
 * @ingroup g_wt_direct
 */
typedef struct _direct_weight_t_ {
    real_t *hash_wt; /**< weight vector. */
    hash_size_t wt_sz;  /**< size of weight vector. */

    /** forward function. */
    int (*forward)(struct _direct_weight_t_ *wt);
    /** backprop function. */
    int (*backprop)(struct _direct_weight_t_ *wt);
} direct_wt_t;

/**
 * Destroy a direct weight and set the pointer to NULL.
 * @ingroup g_wt_direct
 * @param[in] ptr pointer to direct_wt_t.
 */
#define safe_direct_wt_destroy(ptr) do {\
    if((ptr) != NULL) {\
        direct_wt_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a direct weight.
 * @ingroup g_wt_direct
 * @param[in] direct_wt direct weight to be destroyed.
 */
void direct_wt_destroy(direct_wt_t* direct_wt);

/**
 * Duplicate a direct weight.
 * @ingroup g_wt_direct
 * @param[in] d direct weight to be duplicated.
 * @return the duplicated direct weight.
 */
direct_wt_t* direct_wt_dup(direct_wt_t *d);

/**
 * Load direct_wt header and initialise a new direct_wt.
 * @ingroup g_wt_direct
 * @param[out] direct_wt direct_wt initialised.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[out] binary whether the file stream is in binary format.
 * @param[in] fo file stream used to print information, if it is not NULL.
 * @see direct_wt_load_body
 * @see direct_wt_save_header, direct_wt_save_body
 * @return non-zero value if any error.
 */
int direct_wt_load_header(direct_wt_t **direct_wt, int version,
        FILE *fp, bool *binary, FILE *fo_info);

/**
 * Load direct_wt body.
 * @ingroup g_wt_direct
 * @param[in] direct_wt direct_wt to be loaded.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[in] binary whether to use binary format.
 * @see direct_wt_load_header
 * @see direct_wt_save_header, direct_wt_save_body
 * @return non-zero value if any error.
 */
int direct_wt_load_body(direct_wt_t *direct_wt, int version, FILE *fp, bool binary);

/**
 * Save direct_wt header.
 * @ingroup g_wt_direct
 * @param[in] direct_wt direct_wt to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see direct_wt_save_body
 * @see direct_wt_load_header, direct_wt_load_body
 * @return non-zero value if any error.
 */
int direct_wt_save_header(direct_wt_t *direct_wt, FILE *fp, bool binary);

/**
 * Save direct_wt body.
 * @ingroup g_wt_direct
 * @param[in] direct_wt direct_wt to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see direct_wt_save_header
 * @see direct_wt_load_header, direct_wt_load_body
 * @return non-zero value if any error.
 */
int direct_wt_save_body(direct_wt_t *direct_wt, FILE *fp, bool binary);

/**
 * Initialise direct weight.
 * @ingroup g_wt_direct
 * @param[in] hash_sz size of hash.
 * @return initialised direct weight if success, otherwise NULL.
 */
direct_wt_t* direct_wt_init(hash_size_t hash_sz);

/**
 * Feed-forward one word for a thread of direct_wt.
 * @ingroup g_wt_direct
 * @param[in] direct_wt direct_wt.
 * @param[in] scale out activation scale.
 * @param[in] input input layer.
 * @param[in] output output layer.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int direct_wt_forward(direct_wt_t *direct_wt, real_t scale, input_t *input,
        output_t *output, int tid);

#ifdef __cplusplus
}
#endif

#endif
