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

#ifndef  _CONNLM_COMP_UPDATER_H_
#define  _CONNLM_COMP_UPDATER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <connlm/config.h>

#include "component.h"
#include "updaters/output_updater.h"
#include "updaters/layer_updater.h"
#include "updaters/glue_updaters/glue_updater.h"
#include "updaters/bptt_updater.h"

/** @defgroup g_updater_comp Updater for Component
 * @ingroup g_updater
 * Perform forward and backprop on a component.
 */

/**
 * Component updater.
 * @ingroup g_updater_comp
 */
typedef struct _component_updater_t_ {
    component_t *comp; /**< the component. */
    out_updater_t *out_updater; /**< the output updater. */

    layer_updater_t **layer_updaters; /**< layer updaters. */
    glue_updater_t **glue_updaters; /**< glue updaters. */

    int bptt_step; /**< bptt step in one sentence. */
    bptt_updater_t **bptt_updaters; /**< bptt updater. Every cycle in
                                      comp->glue_cycles has one bptt_updater. */

    mat_t bptt_er0; /**< buffer for er used by BPTT. */
    mat_t bptt_er1; /**< buffer for er used by BPTT. */

    unsigned int *rand_seed; /**< random seed. */

    egs_batch_t batch; /**< current data batch, filled by input_updater. */
} comp_updater_t;

/**
 * Destroy a comp_updater and set the pointer to NULL.
 * @ingroup g_updater_comp
 * @param[in] ptr pointer to updater_t.
 */
#define safe_comp_updater_destroy(ptr) do {\
    if((ptr) != NULL) {\
        comp_updater_destroy(ptr);\
        safe_st_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a comp_updater.
 * @ingroup g_updater_comp
 * @param[in] comp_updater comp_updater to be destroyed.
 */
void comp_updater_destroy(comp_updater_t *comp_updater);

/**
 * Create a comp_updater.
 * @ingroup g_updater_comp
 * @param[in] comp the component.
 * @param[in] out_updater the output updater.
 * @return comp_updater on success, otherwise NULL.
 */
comp_updater_t* comp_updater_create(component_t *comp,
        out_updater_t *out_updater);

/**
 * Setup comp_updater for running.
 * @ingroup g_updater_comp
 * @param[in] comp_updater comp_updater.
 * @param[in] backprop whether do backprop.
 * @return non-zero value if any error.
 */
int comp_updater_setup(comp_updater_t *comp_updater, bool backprop);

/**
 * Set rand seed for comp_updater.
 * @ingroup g_updater_comp
 * @param[in] comp_updater comp_updater.
 * @param[in] seed the rand seed pointer.
 * @return non-zero value if any error.
 */
int comp_updater_set_rand_seed(comp_updater_t *comp_updater, unsigned int *seed);

/**
 * Reset running for comp_updater.
 * Called before every input sentence to be performed.
 * @ingroup g_updater_comp
 * @param[in] comp_updater comp_updater.
 * @param[in] batch_i index in batch to be reset.
 * @return non-zero value if any error.
 */
int comp_updater_reset(comp_updater_t *comp_updater, int batch_i);

/**
 * Feed-forward a batch for a comp_updater.
 * @ingroup g_updater_comp
 * @param[in] comp_updater the comp_updater.
 * @param[in] batch egs batch.
 * @see comp_updater_backprop
 * @return non-zero value if any error.
 */
int comp_updater_forward(comp_updater_t *comp_updater, egs_batch_t *batch);

/**
 * Back-propagate a batch for a comp_updater.
 * @ingroup g_updater_comp
 * @param[in] comp_updater the comp_updater.
 * @param[in] batch egs batch.
 * @see comp_updater_forward
 * @return non-zero value if any error.
 */
int comp_updater_backprop(comp_updater_t *comp_updater, egs_batch_t *batch);

/**
 * Save state for a comp_updater.
 * @ingroup g_updater_layer
 * @param[in] comp_updater the comp_updater.
 * @return non-zero value if any error.
 */
int comp_updater_save_state(comp_updater_t *comp_updater);

/**
 * Clear buffers for comp_updater.
 * @ingroup g_updater_comp
 * @param[in] comp_updater comp_updater.
 * @return non-zero value if any error.
 */
int comp_updater_clear(comp_updater_t *comp_updater);

/**
 * Finish running for comp_updater.
 * Called after all words performed.
 * @ingroup g_updater_comp
 * @param[in] comp_updater comp_updater.
 * @return non-zero value if any error.
 */
int comp_updater_finish(comp_updater_t *comp_updater);

/**
 * Feed-forward util output layer.
 * @ingroup g_updater_comp
 * @param[in] comp_updater the comp_updater.
 * @param[in] batch egs batch.
 * @return non-zero value if any error.
 */
int comp_updater_forward_util_out(comp_updater_t *comp_updater,
        egs_batch_t *batch);

/**
 * Feed-forward one node of output layer.
 * @ingroup g_updater_comp
 * @param[in] comp_updater the comp_updater.
 * @param[in] node node of output tree.
 * @return non-zero value if any error.
 */
int comp_updater_forward_out(comp_updater_t *comp_updater,
        output_node_id_t node);

/**
 * Feed-forward batch of words of output layer.
 * @ingroup g_updater_comp
 * @param[in] comp_updater the comp_updater.
 * @param[in] words the words.
 * @return non-zero value if any error.
 */
int comp_updater_forward_out_word(comp_updater_t *comp_updater, ivec_t* words);

/**
 * Get the size of state in comp_updater.
 * @ingroup g_updater_comp
 * @param[in] comp_updater the comp_updater.
 * @return state size of comp_updater, -1 if any error.
 */
int comp_updater_state_size(comp_updater_t *comp_updater);

/**
 * Dump the state of comp_updater.
 * @ingroup g_updater_comp
 * @param[in] comp_updater the comp_updater.
 * @param[out] state pointer to store the dumped state. Size of state
 *             must be larger than or equal to the state_size returned
 *             by comp_updater_state_size.
 * @return non-zero value if any error.
 */
int comp_updater_dump_state(comp_updater_t *comp_updater, mat_t *state);

/**
 * Dump the pre-activation state of comp_updater.
 * @ingroup g_updater_comp
 * @param[in] comp_updater the comp_updater.
 * @param[out] state pointer to store the dumped state. Size of state
 *             must be larger than or equal to the state_size returned
 *             by comp_updater_state_size.
 * @return non-zero value if any error.
 */
int comp_updater_dump_pre_ac_state(comp_updater_t *comp_updater,
        mat_t *state);

/**
 * Feed the state of comp_updater.
 * @ingroup g_updater_comp
 * @param[in] comp_updater the comp_updater.
 * @param[in] state pointer to values to be fed into state. Size of state
 *             must be larger than or equal to the state_size returned
 *             by comp_updater_state_size.
 * @return non-zero value if any error.
 */
int comp_updater_feed_state(comp_updater_t *comp_updater, mat_t *state);

/**
 * Generate random state of comp_updater.
 * @ingroup g_updater_comp
 * @param[in] comp_updater the comp_updater.
 * @param[out] state pointer to generated state. Size of state
 *             must be larger than or equal to the state_size returned
 *             by comp_updater_state_size.
 * @return non-zero value if any error.
 */
int comp_updater_random_state(comp_updater_t *comp_updater, mat_t *state);

/**
 * Initialize multi-call of forward_out_word.
 * @ingroup g_updater_comp
 * @param[in] comp_updater the comp_updater.
 * @param[in] output the output layer.
 * @return non-zero value if any error.
 */
int comp_updater_init_multicall(comp_updater_t *comp_updater,
        output_t *output);

/**
 * Clear multi-call of forward_out_word.
 * @ingroup g_updater_comp
 * @param[in] comp_updater the comp_updater.
 * @param[in] output the output layer.
 * @return non-zero value if any error.
 */
int comp_updater_clear_multicall(comp_updater_t *comp_updater,
        output_t *output);

/**
 * Activate state with comp_updater.
 * @ingroup g_updater_comp
 * @param[in] comp_updater the comp_updater with activatation functions.
 * @param[out] state pointer to activated state. Size of state
 *             must be larger than or equal to the state_size returned
 *             by comp_updater_state_size.
 * @return non-zero value if any error.
 */
int comp_updater_activate_state(comp_updater_t *comp_updater, mat_t *state);

/**
 * Setup pre-activation state for comp_updater.
 * @ingroup g_updater_comp
 * @param[in] comp_updater comp_updater.
 * @return non-zero value if any error.
 */
int comp_updater_setup_pre_ac_state(comp_updater_t *comp_updater);

#ifdef __cplusplus
}
#endif

#endif
