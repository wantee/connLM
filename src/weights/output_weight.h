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

#ifdef __cplusplus
}
#endif

#endif
