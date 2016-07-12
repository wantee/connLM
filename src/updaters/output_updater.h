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

#include "output.h"

/** @defgroup g_updater_output Updater for output layer.
 * @ingroup g_updater
 * Data structures and functions for output layer updater.
 */

/**
 * Output layer updater.
 * @ingroup g_updater_output
 */
typedef struct _output_updater_t_ {
    output_t *output; /**< the output layer. */

    real_t *ac; /**< activation of output layer. */
    real_t *er; /**< error of output layer. */
} out_updater_t;

/**
 * Destroy a out_updater and set the pointer to NULL.
 * @ingroup g_updater_output
 * @param[in] ptr pointer to updater_t.
 */
#define safe_out_updater_destroy(ptr) do {\
    if((ptr) != NULL) {\
        out_updater_destroy(ptr);\
        safe_free(ptr);\
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
 * @param[in] connlm the connlm model.
 * @return out_updater on success, otherwise NULL.
 */
out_updater_t* out_updater_create(output_t *output);

/**
 * Setup out_updater for running.
 * @ingroup g_updater_output
 * @param[in] out_updater out_updater.
 * @param[in] backprob whether do backprob.
 * @return non-zero value if any error.
 */
int out_updater_setup(out_updater_t *out_updater, bool backprob);

/**
 * Reset running for out_updater.
 * Called before every input sentence to be performed.
 * @ingroup g_updater_out
 * @param[in] out_updater out_updater.
 * @return non-zero value if any error.
 */
int out_updater_reset(out_updater_t *out_updater);

/**
 * Start running for out_updater.
 * Called before every input word to be performed.
 * @ingroup g_updater_out
 * @param[in] out_updater out_updater.
 * @param[in] word current predicted word.
 * @return non-zero value if any error.
 */
int out_updater_start(out_updater_t *out_updater, int word);

/**
 * Feed-forward one word for a out_updater.
 * @ingroup g_updater_out
 * @param[in] out_updater out_updater.
 * @param[in] word current predicted word.
 * @see out_updater_backprop
 * @return non-zero value if any error.
 */
int out_updater_forward(out_updater_t *out_updater, int word);

/**
 * Back-propagate one word for a out_updater.
 * @ingroup g_updater_out
 * @param[in] out_updater out_updater.
 * @param[in] word current predicted word.
 * @see out_updater_forward
 * @return non-zero value if any error.
 */
int out_updater_backprop(out_updater_t *out_updater, int word);

/**
 * End running for out_updater.
 * Called after every input word performed.
 * @ingroup g_updater_out
 * @param[in] out_updater out_updater.
 * @return non-zero value if any error.
 */
int out_updater_end(out_updater_t *out_updater);

/**
 * Finish running for out_updater.
 * Called after all words performed.
 * @ingroup g_updater_out
 * @param[in] out_updater out_updater.
 * @return non-zero value if any error.
 */
int out_updater_finish(out_updater_t *out_updater);

#if 0
/**
 * Compute loss of neurons of output tree.
 * @ingroup g_output
 * @param[in] output output tree related.
 * @param[in] word target word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int output_loss(output_t *output, int word, int tid);

/**
 * Generate a word from output tree.
 * @ingroup g_output
 * @param[in] output output tree related.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int output_gen_word(output_t *output, int tid);

/**
 * Get log probability of a word.
 * @ingroup g_output
 * @param[in] output output tree related.
 * @param[in] word target word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
double output_get_logprob(output_t *output, int word, int tid);
#endif

#ifdef __cplusplus
}
#endif

#endif
