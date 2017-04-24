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

#ifndef  _CONNLM_LAYER_UPDATER_H_
#define  _CONNLM_LAYER_UPDATER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <connlm/config.h>

#include "layers/layer.h"

/** @defgroup g_updater_layer Updater for Hidden Layer
 * @ingroup g_updater
 * Perform forward and backprop on a hidden layer.
 */

typedef struct _layer_updater_t_ layer_updater_t;
typedef int (*activate_func_t)(layer_t *layer, real_t *vec, int size); /**< activate function. */
typedef int (*deriv_func_t)(layer_t *layer, real_t *er, real_t *ac, int size); /**< deriv function. */
typedef int (*random_state_func_t)(layer_t *layer, real_t *state, int size); /**< random state function. */
/**
 * Layer updater.
 * @ingroup g_updater_layer
 */
typedef struct _layer_updater_t_ {
    layer_t *layer; /**< the layer. */

    real_t *ac; /**< activation of layer. */
    real_t *er; /**< error of layer. */

    activate_func_t activate; /**< activate function. */
    bool activated; /**< activation indicator. */
    deriv_func_t deriv; /**< deriv function. */
    bool derived; /**< derived indicator. */
    random_state_func_t random_state; /**< random state function. */

    real_t *ac_state; /**< state of activation(for prev timestep). */
    real_t *er_raw; /**< raw value of error(before derived). */
} layer_updater_t;

/**
 * Destroy a layer_updater and set the pointer to NULL.
 * @ingroup g_updater_layer
 * @param[in] ptr pointer to updater_t.
 */
#define safe_layer_updater_destroy(ptr) do {\
    if((ptr) != NULL) {\
        layer_updater_destroy(ptr);\
        safe_st_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a layer_updater.
 * @ingroup g_updater_layer
 * @param[in] layer_updater layer_updater to be destroyed.
 */
void layer_updater_destroy(layer_updater_t *layer_updater);

/**
 * Create a layer_updater.
 * @ingroup g_updater_layer
 * @param[in] layer the layer.
 * @return layer_updater on success, otherwise NULL.
 */
layer_updater_t* layer_updater_create(layer_t *layer);

/**
 * Setup layer_updater for running.
 * @ingroup g_updater_layer
 * @param[in] layer_updater layer_updater.
 * @param[in] backprop whether do backprop.
 * @return non-zero value if any error.
 */
int layer_updater_setup(layer_updater_t *layer_updater, bool backprop);

/**
 * Setup layer_updater state for running.
 * @ingroup g_updater_layer
 * @param[in] layer_updater layer_updater.
 * @param[in] backprop whether do backprop.
 * @return non-zero value if any error.
 */
int layer_updater_setup_state(layer_updater_t *layer_updater, bool backprop);

/**
 * Setup layer_updater er_raw for BPTT.
 * @ingroup g_updater_layer
 * @param[in] layer_updater layer_updater.
 * @return non-zero value if any error.
 */
int layer_updater_setup_er_raw(layer_updater_t *layer_updater);

/**
 * Activate a layer_updater.
 * @ingroup g_updater_layer
 * @param[in] layer_updater the layer_updater.
 * @return non-zero value if any error.
 */
int layer_updater_activate(layer_updater_t *layer_updater);

/**
 * Deriv a layer_updater.
 * @ingroup g_updater_layer
 * @param[in] layer_updater the layer_updater.
 * @return non-zero value if any error.
 */
int layer_updater_deriv(layer_updater_t *layer_updater);

/**
 * Save state for a layer_updater. i.e. copy the activation to state.
 * @ingroup g_updater_layer
 * @param[in] layer_updater the layer_updater.
 * @return non-zero value if any error.
 */
int layer_updater_save_state(layer_updater_t *layer_updater);

/**
 * Clear a layer_updater.
 * @ingroup g_updater_layer
 * @param[in] layer_updater the layer_updater.
 * @return non-zero value if any error.
 */
int layer_updater_clear(layer_updater_t *layer_updater);

/**
 * Reset a layer_updater.
 * @ingroup g_updater_layer
 * @param[in] layer_updater the layer_updater.
 * @return non-zero value if any error.
 */
int layer_updater_reset(layer_updater_t *layer_updater);

/**
 * Get the size of state in layer_updater.
 * @ingroup g_updater_layer
 * @param[in] layer_updater layer_updater.
 * @return state size of layer_updater, -1 if any error.
 */
int layer_updater_state_size(layer_updater_t *layer_updater);

/**
 * Dump the state of layer_updater.
 * @ingroup g_updater_layer
 * @param[in] layer_updater layer_updater.
 * @param[out] state pointer to store the dumped state. Size of state
 *             must be larger than or equal to the state_size returned
 *             by layer_updater_state_size.
 * @return non-zero value if any error.
 */
int layer_updater_dump_state(layer_updater_t *layer_updater, real_t *state);

/**
 * Fedd the state of layer_updater.
 * @ingroup g_updater_layer
 * @param[in] layer_updater layer_updater.
 * @param[in] state pointer to values to be fed into state. Size of state
 *             must be larger than or equal to the state_size returned
 *             by layer_updater_state_size.
 * @return non-zero value if any error.
 */
int layer_updater_feed_state(layer_updater_t *layer_updater, real_t *state);

/**
 * Generate random state of layer_updater.
 * @ingroup g_updater_layer
 * @param[in] layer_updater layer_updater.
 * @param[out] state pointer to generated state. Size of state
 *             must be larger than or equal to the state_size returned
 *             by layer_updater_state_size.
 * @return non-zero value if any error.
 */
int layer_updater_random_state(layer_updater_t *layer_updater, real_t *state);

#ifdef __cplusplus
}
#endif

#endif
