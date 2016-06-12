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

#ifndef  _CONNLM_OUTPUT_WEIGHT_H_
#define  _CONNLM_OUTPUT_WEIGHT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <connlm/config.h>

#include "output.h"

/** @defgroup g_wt_output NNet output weight.
 * @ingroup g_weight
 * Data structures and functions for NNet output weight.
 */

/**
 * Output weight.
 * @ingroup g_wt_output
 */
typedef struct _output_weight_t_ {
    output_node_id_t output_node_num; /**< num of nodes in output tree. */
    int in_layer_sz; /**< size of in layer. */
    real_t *matrix; /**< weight matrix. */

    /** forward function. */
    int (*forward)(struct _output_weight_t_ *wt);
    /** backprop function. */
    int (*backprop)(struct _output_weight_t_ *wt);
} out_wt_t;

/**
 * Destroy a output weight and set the pointer to NULL.
 * @ingroup g_wt_output
 * @param[in] ptr pointer to out_wt_t.
 */
#define safe_out_wt_destroy(ptr) do {\
    if((ptr) != NULL) {\
        out_wt_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a output weight.
 * @ingroup g_wt_output
 * @param[in] out_wt output weight to be destroyed.
 */
void out_wt_destroy(out_wt_t* out_wt);

/**
 * Duplicate a output weight.
 * @ingroup g_wt_output
 * @param[in] o output weight to be duplicated.
 * @return the duplicated output weight.
 */
out_wt_t* out_wt_dup(out_wt_t *o);

/**
 * Load out_wt header and initialise a new out_wt.
 * @ingroup g_out_wt
 * @param[out] out_wt out_wt initialised.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[out] binary whether the file stream is in binary format.
 * @param[in] fo file stream used to print information, if it is not NULL.
 * @see out_wt_load_body
 * @see out_wt_save_header, out_wt_save_body
 * @return non-zero value if any error.
 */
int out_wt_load_header(out_wt_t **out_wt, int version,
        FILE *fp, bool *binary, FILE *fo_info);

/**
 * Load out_wt body.
 * @ingroup g_out_wt
 * @param[in] out_wt out_wt to be loaded.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[in] binary whether to use binary format.
 * @see out_wt_load_header
 * @see out_wt_save_header, out_wt_save_body
 * @return non-zero value if any error.
 */
int out_wt_load_body(out_wt_t *out_wt, int version, FILE *fp, bool binary);

/**
 * Save out_wt header.
 * @ingroup g_out_wt
 * @param[in] out_wt out_wt to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see out_wt_save_body
 * @see out_wt_load_header, out_wt_load_body
 * @return non-zero value if any error.
 */
int out_wt_save_header(out_wt_t *out_wt, FILE *fp, bool binary);

/**
 * Save out_wt body.
 * @ingroup g_out_wt
 * @param[in] out_wt out_wt to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see out_wt_save_header
 * @see out_wt_load_header, out_wt_load_body
 * @return non-zero value if any error.
 */
int out_wt_save_body(out_wt_t *out_wt, FILE *fp, bool binary);

/**
 * Initialise output weight.
 * @ingroup g_out_wt
 * @param[in] in_layer_sz size of in layer.
 * @param[in] output_node_num number of nodes in output tree.
 * @return initialised output weight if success, otherwise NULL.
 */
out_wt_t* out_wt_init(int in_layer_sz, output_node_id_t output_node_num);

#ifdef __cplusplus
}
#endif

#endif
