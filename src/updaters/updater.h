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
#include "updaters/input_updater.h"
#include "updaters/output_updater.h"
#include "updaters/component_updater.h"

/** @defgroup g_updater Updater
 * Perform forward and backprop on a connlm model.
 */

/**
 * Updater.
 * @ingroup g_updater
 */
typedef struct _updater_t_ {
    connlm_t *connlm; /**< the model. */

    input_updater_t *input_updater; /**< input layer updater. */
    out_updater_t *out_updater; /**< output layer updater. */
    comp_updater_t **comp_updaters; /**< component updaters. */

    bool finalized; /**< whether finalized by caller. */
    bool backprop; /**< whether do backpropagation. */

    sent_t sent; /**< current sentence, filled by input_updater. */
    double logp; /**< logp for current target word. */
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
 * Sampling a word.
 * @ingroup g_updater
 * @param[in] updater updater.
 * @param[in] startover if true, feed \<s\> before sampling.
 * @return the sampled word, -1 if any error.
 */
int updater_sampling(updater_t *updater, bool startover);

/**
 * Get the size of state in updater.
 * @ingroup g_updater
 * @param[in] updater updater.
 * @return state size of updater, -1 if any error.
 */
int updater_state_size(updater_t *updater);

/**
 * Dump the state of updater.
 * @ingroup g_updater
 * @param[in] updater updater.
 * @param[out] state pointer to store the dumped state. Size of state
 *             must be larger than or equal to the state_size returned
 *             by updater_state_size.
 * @return non-zero value if any error.
 */
int updater_dump_state(updater_t *updater, real_t *state);

/**
 * Feed the state of updater.
 * @ingroup g_updater
 * @param[in] updater updater.
 * @param[in] state pointer to values to be fed into state. Size of state
 *             must be larger than or equal to the state_size returned
 *             by updater_state_size.
 * @return non-zero value if any error.
 */
int updater_feed_state(updater_t *updater, real_t *state);

/**
 * Setup updater for running with activating all words in one step.
 * @ingroup g_updater
 * @param[in] updater updater.
 * @return non-zero value if any error.
 */
int updater_setup_all(updater_t *updater);

/**
 * Run one step with specified state and history.
 * @ingroup g_updater
 * @param[in] updater updater.
 * @param[in] state state for model, from updater_dump_state.
 * @param[in] hist word history.
 * @param[in] num_hist number of word history.
 * @param[out] output_probs log-probs for all words.
 * @return non-zero value if any error.
 */
int updater_step_with_state(updater_t *updater, real_t *state,
        int *hist, int num_hist, double *output_probs);

#ifdef __cplusplus
}
#endif

#endif
