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

#ifndef  _CONNLM_WEIGHT_H_
#define  _CONNLM_WEIGHT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <connlm/config.h>

/** @defgroup g_weight NNet weight.
 * Data structures and functions for NNet weight.
 */

/**
 * NNet weight.
 * @ingroup g_weight
 */
typedef struct _weight_t_ {
    real_t *matrix; /**< weight matrix. */
    int row; /**< number row of weight matrix. */
    int col; /**< number column of weight matrix. */
} weight_t;

/**
 * Destroy a weight and set the pointer to NULL.
 * @ingroup g_weight
 * @param[in] ptr pointer to weight_t.
 */
#define safe_wt_destroy(ptr) do {\
    if((ptr) != NULL) {\
        wt_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a weight.
 * @ingroup g_weight
 * @param[in] wt weight to be destroyed.
 */
void wt_destroy(weight_t* wt);

/**
 * Duplicate a weight.
 * @ingroup g_weight
 * @param[in] w weight to be duplicated.
 * @return the duplicated weight.
 */
weight_t* wt_dup(weight_t *w);

/**
 * Load wt header and initialise a new wt.
 * @ingroup g_weight
 * @param[out] wt wt initialised.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[out] binary whether the file stream is in binary format.
 * @param[in] fo file stream used to print information, if it is not NULL.
 * @see wt_load_body
 * @see wt_save_header, wt_save_body
 * @return non-zero value if any error.
 */
int wt_load_header(weight_t **wt, int version,
        FILE *fp, bool *binary, FILE *fo_info);

/**
 * Load wt body.
 * @ingroup g_weight
 * @param[in] wt wt to be loaded.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[in] binary whether to use binary format.
 * @see wt_load_header
 * @see wt_save_header, wt_save_body
 * @return non-zero value if any error.
 */
int wt_load_body(weight_t *wt, int version, FILE *fp, bool binary);

/**
 * Save wt header.
 * @ingroup g_weight
 * @param[in] wt wt to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see wt_save_body
 * @see wt_load_header, wt_load_body
 * @return non-zero value if any error.
 */
int wt_save_header(weight_t *wt, FILE *fp, bool binary);

/**
 * Save wt body.
 * @ingroup g_weight
 * @param[in] wt wt to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see wt_save_header
 * @see wt_load_header, wt_load_body
 * @return non-zero value if any error.
 */
int wt_save_body(weight_t *wt, FILE *fp, bool binary);

/**
 * Initialise weight.
 * @ingroup g_weight
 * @param[in] row num of row in matrix.
 * @param[in] col num of column in matrix.
 * @return initialised weight if success, otherwise NULL.
 */
weight_t* wt_init(int row, int col);

#ifdef __cplusplus
}
#endif

#endif
