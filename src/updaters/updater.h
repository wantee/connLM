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

#ifndef  _CONNLM_UPDATER_H_
#define  _CONNLM_UPDATER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <connlm/config.h>

#include "connlm.h"
#include "updaters/output_updater.h"
#include "updaters/component_updater.h"

/** @defgroup g_updater Updater to update connlm parameter.
 * Data structure and functions for updater.
 */

/**
 * Updater.
 * @ingroup g_updater
 */
typedef struct _updater_t_ {
    connlm_t *connlm; /**< the model. */

    out_updater_t *out_updater; /**< output layer updater. */
    comp_updater_t **comp_updaters; /**< component updaters. */

    int *words; /**< buffer for input words. */
    int n_word; /**< number of input words. */
    int cap_word; /**< capacity of input words buffer. */
    int tgt_pos; /**< position of target predicted word in word buffer. */
    int ctx_leftmost; /**< leftmost for all input contexts. */
    int ctx_rightmost; /**< rightmost for all input contexts. */

    count_t n_step; /**< updating step. */

    bool finalized; /**< whether finalized by caller. */
    bool backprop; /**< whether do backpropagation. */
} updater_t;

/**
 * Destroy a updater and set the pointer to NULL.
 * @ingroup g_updater
 * @param[in] ptr pointer to updater_t.
 */
#define safe_updater_destroy(ptr) do {\
    if((ptr) != NULL) {\
        updater_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a updater.
 * @ingroup g_updater
 * @param[in] updater updater to be destroyed.
 */
void updater_destroy(updater_t *updater);

/**
 * Create a updater.
 * @ingroup g_updater
 * @param[in] connlm the connlm model.
 * @return updater on success, otherwise NULL.
 */
updater_t* updater_create(connlm_t *connlm);

/**
 * Setup updater for running.
 * @ingroup g_updater
 * @param[in] updater updater.
 * @param[in] backprop whether do backprop.
 * @return non-zero value if any error.
 */
int updater_setup(updater_t *updater, bool backprop);

/**
 * Feed input words to a updater.
 * @ingroup g_updater
 * @param[in] updater updater.
 * @param[in] words input words buffer.
 * @param[in] n_word number of input words.
 * @return non-zero value if any error.
 */
int updater_feed(updater_t *updater, int *words, int n_word);

/**
 * Determine whethre can perform a step for a updater.
 * @ingroup g_updater
 * @param[in] updater updater.
 * @return true if steppabel, otherwise false.
 */
bool updater_steppable(updater_t *updater);

/**
 * Step one word for a updater.
 * @ingroup g_updater
 * @param[in] updater updater.
 * @return word for this step, -1 if any error.
 */
int updater_step(updater_t *updater);

/**
 * Finalize a updater for running.
 * @ingroup g_updater
 * @param[in] updater updater.
 * @return non-zero value if any error.
 */
int updater_finalize(updater_t *updater);

/**
 * Get log probablity for word from a updater.
 * @ingroup g_updater
 * @param[in] updater updater.
 * @param[in] word predicted word.
 * @param[out] logp log probablity.
 * @return non-zero value if any error.
 */
int updater_get_logp(updater_t *updater, int word, double *logp);

/**
 * Sampling a word.
 * @ingroup g_updater
 * @param[in] updater updater.
 * @return the sampled word, -1 if any error.
 */
int updater_sampling(updater_t *updater);

#ifdef __cplusplus
}
#endif

#endif
