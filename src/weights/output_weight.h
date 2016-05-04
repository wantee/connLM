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
    output_t *output; /**< associating output layer. */

    /** forward function. */
    int (*forward)(struct _output_weight_t_ *wt);
    /** backprop function. */
    int (*backprop)(struct _output_weight_t_ *wt);
} output_wt_t;

/**
 * Destroy a output weight and set the pointer to NULL.
 * @ingroup g_wt_output
 * @param[in] ptr pointer to output_wt_t.
 */
#define safe_output_wt_destroy(ptr) do {\
    if((ptr) != NULL) {\
        output_wt_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a output weight.
 * @ingroup g_wt_output
 * @param[in] output_wt output weight to be destroyed.
 */
void output_wt_destroy(output_wt_t* output_wt);

/**
 * Duplicate a output weight.
 * @ingroup g_wt_output
 * @param[in] o output weight to be duplicated.
 * @return the duplicated output weight.
 */
output_wt_t* output_wt_dup(output_wt_t *o);

/**
 * Load output_wt header and initialise a new output_wt.
 * @ingroup g_output_wt
 * @param[out] output_wt output_wt initialised.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[out] binary whether the file stream is in binary format.
 * @param[in] fo file stream used to print information, if it is not NULL.
 * @see output_wt_load_body
 * @see output_wt_save_header, output_wt_save_body
 * @return non-zero value if any error.
 */
int output_wt_load_header(output_wt_t **output_wt, int version,
        FILE *fp, bool *binary, FILE *fo_info);

/**
 * Load output_wt body.
 * @ingroup g_output_wt
 * @param[in] output_wt output_wt to be loaded.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[in] binary whether to use binary format.
 * @see output_wt_load_header
 * @see output_wt_save_header, output_wt_save_body
 * @return non-zero value if any error.
 */
int output_wt_load_body(output_wt_t *output_wt, int version, FILE *fp, bool binary);

/**
 * Save output_wt header.
 * @ingroup g_output_wt
 * @param[in] output_wt output_wt to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see output_wt_save_body
 * @see output_wt_load_header, output_wt_load_body
 * @return non-zero value if any error.
 */
int output_wt_save_header(output_wt_t *output_wt, FILE *fp, bool binary);

/**
 * Save output_wt body.
 * @ingroup g_output_wt
 * @param[in] output_wt output_wt to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see output_wt_save_header
 * @see output_wt_load_header, output_wt_load_body
 * @return non-zero value if any error.
 */
int output_wt_save_body(output_wt_t *output_wt, FILE *fp, bool binary);
#ifdef __cplusplus
}
#endif

#endif
