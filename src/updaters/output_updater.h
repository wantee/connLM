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

#ifndef  _CONNLM_OUTPUT_UPDATER_H_
#define  _CONNLM_OUTPUT_UPDATER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <connlm/config.h>

#include "vector.h"
#include "matrix.h"
#include "output.h"

/** @defgroup g_updater_output Updater for Output Layer
 * @ingroup g_updater
 * Perform forward and backprop on a output layer.
 */

/**
 * Output layer updater.
 * @ingroup g_updater_output
 */
typedef struct _output_updater_t_ {
    output_t *output; /**< the output layer. */

    mat_t *node_acs; /**< activation of each output tree node. */
    mat_t *node_ers; /**< error of each output tree node. */
    int *node_iters; /**< error of each output tree node. */
} out_updater_t;

/**
 * Destroy a out_updater and set the pointer to NULL.
 * @ingroup g_updater_output
 * @param[in] ptr pointer to updater_t.
 */
#define safe_out_updater_destroy(ptr) do {\
    if((ptr) != NULL) {\
        out_updater_destroy(ptr);\
        safe_st_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a out_updater.
 * @ingroup g_updater_output
 * @param[in] out_updater out_updater to be destroyed.
 */
void out_updater_destroy(out_updater_t *out_updater);

/**
 * Create a out_updater.
 * @ingroup g_updater_output
 * @param[in] output related output layer.
 * @return out_updater on success, otherwise NULL.
 */
out_updater_t* out_updater_create(output_t *output);

/**
 * Setup out_updater for running.
 * @ingroup g_updater_output
 * @param[in] out_updater out_updater.
 * @param[in] backprop whether do backprop.
 * @return non-zero value if any error.
 */
int out_updater_setup(out_updater_t *out_updater, bool backprop);

/**
 * Clear path for a word in out_updater.
 * @ingroup g_updater_out
 * @param[in] out_updater out_updater.
 * @param[in] targets target words in current batch.
 * @return non-zero value if any error.
 */
int out_updater_clear(out_updater_t *out_updater, ivec_t *targets);

/**
 * Activate one mini-batch for a out_updater.
 * @ingroup g_updater_out
 * @param[in] out_updater out_updater.
 * @param[in] targets target words in current batch.
 * @param[out] logps logp for words in current batch.
 * @see out_updater_loss
 * @return non-zero value if any error.
 */
int out_updater_activate(out_updater_t *out_updater,
        ivec_t *targets, dvec_t *logps);

/**
 * Compute loss of mini-batch for a out_updater.
 * @ingroup g_updater_out
 * @param[in] out_updater out_updater.
 * @param[in] targets target words in current batch.
 * @see out_updater_activate
 * @return non-zero value if any error.
 */
int out_updater_loss(out_updater_t *out_updater, ivec_t *targets);

/**
 * Finish running for out_updater.
 * Called after all words performed.
 * @ingroup g_updater_out
 * @param[in] out_updater the out_updater.
 * @return non-zero value if any error.
 */
int out_updater_finish(out_updater_t *out_updater);

/**
 * Sample a node from children.
 * @ingroup g_updater_output
 * @param[in] out_updater the out_updater.
 * @param[in] node the parent node.
 * @return sampled child, OUTPUT_NODE_NONE if any error.
 */
output_node_id_t out_updater_sample(out_updater_t *out_updater,
        output_node_id_t node);

#ifdef __cplusplus
}
#endif

#endif
