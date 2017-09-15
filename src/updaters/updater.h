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

    unsigned int rand_seed; /**< rand seed. */

    egs_batch_t *batches; /**< current data batches, filled by input_updater. */
    ivec_t targets; /**< target words in current batch. */
    dvec_t logps; /**< logp for words in current batch. */

    word_pool_t tmp_wp; /**< temp buffer for word pool. */
} updater_t;

/**
 * Destroy a updater and set the pointer to NULL.
 * @ingroup g_updater
 * @param[in] ptr pointer to updater_t.
 */
#define safe_updater_destroy(ptr) do {\
    if((ptr) != NULL) {\
        updater_destroy(ptr);\
        safe_st_free(ptr);\
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
 * @param[in] updater the updater.
 * @param[in] backprop whether do backprop.
 * @return non-zero value if any error.
 */
int updater_setup(updater_t *updater, bool backprop);

/**
 * Set rand_seed for updater.
 * @ingroup g_updater
 * @param[in] updater the updater.
 * @param[in] seed the rand seed.
 * @return non-zero value if any error.
 */
int updater_set_rand_seed(updater_t *updater, unsigned int seed);

/**
 * Feed input words to a updater.
 * @ingroup g_updater
 * @param[in] updater the updater.
 * @param[in] wp input words pool.
 * @return non-zero value if any error.
 */
int updater_feed(updater_t *updater, word_pool_t *wp);

/**
 * Determine whethre can perform a step for a updater.
 * @ingroup g_updater
 * @param[in] updater the updater.
 * @return true if steppabel, otherwise false.
 */
bool updater_steppable(updater_t *updater);

/**
 * Step one word for a updater.
 * @ingroup g_updater
 * @param[in] updater the updater.
 * @return word for this step, -1 if any error.
 */
int updater_step(updater_t *updater);

/**
 * Finalize a updater for running.
 * @ingroup g_updater
 * @param[in] updater the updater.
 * @return non-zero value if any error.
 */
int updater_finalize(updater_t *updater);

/**
 * Sampling a word.
 * @ingroup g_updater
 * @param[in] updater the updater.
 * @param[in] startover if true, feed \<s\> before sampling.
 * @return the sampled word, -1 if any error.
 */
int updater_sampling(updater_t *updater, bool startover);

/**
 * Get the size of state in updater.
 * @ingroup g_updater
 * @param[in] updater the updater.
 * @return state size of updater, -1 if any error.
 */
int updater_state_size(updater_t *updater);

/**
 * Dump the state of updater.
 * @ingroup g_updater
 * @param[in] updater the updater.
 * @param[out] state pointer to store the dumped state. Size of state
 *             must be larger than or equal to the state_size returned
 *             by updater_state_size.
 * @return non-zero value if any error.
 */
int updater_dump_state(updater_t *updater, mat_t *state);

/**
 * Dump the pre-activation state of updater.
 * @ingroup g_updater
 * @param[in] updater the updater.
 * @param[out] state pointer to store the dumped state. Size of state
 *             must be larger than or equal to the state_size returned
 *             by updater_state_size.
 * @return non-zero value if any error.
 */
int updater_dump_pre_ac_state(updater_t *updater, mat_t *state);

/**
 * Feed the state of updater.
 * @ingroup g_updater
 * @param[in] updater the updater.
 * @param[in] state pointer to values to be fed into state. Size of state
 *             must be larger than or equal to the state_size returned
 *             by updater_state_size.
 * @return non-zero value if any error.
 */
int updater_feed_state(updater_t *updater, mat_t *state);

/**
 * Generate random state of updater.
 * @ingroup g_updater
 * @param[in] updater the updater.
 * @param[out] state pointer to generated state. Size of state
 *             must be larger than or equal to the state_size returned
 *             by updater_state_size.
 * @return non-zero value if any error.
 */
int updater_random_state(updater_t *updater, mat_t *state);

/**
 * Run one step with specified state and histories, without forward output.
 * @ingroup g_updater
 * @param[in] updater the updater.
 * @param[in] state state for model, from updater_dump_state.
 * @param[in] hists array of word histories.
 * @param[in] num_hists size of hists array.
 * @return non-zero value if any error.
 */
int updater_step_with_state(updater_t *updater, mat_t *state,
        ivec_t *hists, int num_hists);

/**
 * Forward and activate in output layer for a word.
 * Activate the word if logp != NULL
 * @ingroup g_updater
 * @param[in] updater the updater.
 * @param[in] words the words.
 * @param[out] logps log-prob for the words.
 * @return non-zero value if any error.
 */
int updater_forward_out_words(updater_t *updater, ivec_t *words, dvec_t *logps);

/**
 * Activate state with updater.
 * @ingroup g_updater
 * @param[in] updater the updater with activatation functions.
 * @param[out] state pointer to activated state. Size of state
 *             must be larger than or equal to the state_size returned
 *             by updater_state_size.
 * @return non-zero value if any error.
 */
int updater_activate_state(updater_t *updater, mat_t *state);

/**
 * Sampling a word and return the state.
 * @ingroup g_updater
 * @param[in] updater the updater.
 * @param[out] state pointer to activated state. Set to NULL if not needed.
 * @param[out] pre_ac_state pointer to pre-activated state.
 *                                 Set to NULL if not needed.
 *                                 Size of the above states must be larger
 *                                 than or equal to the state_size returned
 *                                 by updater_state_size.
 * @param[in] startover if true, feed \<s\> before sampling.
 * @return the sampled word, -1 if any error.
 */
int updater_sampling_state(updater_t *updater, mat_t *state,
        mat_t *pre_ac_state, bool startover);

/**
 * Setup pre-activated state for updater.
 * @ingroup g_updater
 * @param[in] updater the updater.
 * @return non-zero value if any error.
 */
int updater_setup_pre_ac_state(updater_t *updater);

/**
 * Step one word for a updater and return the state.
 * @ingroup g_updater
 * @param[in] updater the updater.
 * @param[out] state pointer to activated state. Set to NULL if not needed.
 * @param[out] pre_ac_state pointer to pre-activated state.
 *                                 Set to NULL if not needed.
 *                                 Size of the above states must be larger
 *                                 than or equal to the state_size returned
 *                                 by updater_state_size.
 * @return word for this step, -1 if any error.
 */
int updater_step_state(updater_t *updater, mat_t *state,
        mat_t *pre_ac_state);

#ifdef __cplusplus
}
#endif

#endif
