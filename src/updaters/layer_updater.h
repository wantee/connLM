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

/** @defgroup g_updater_layer Updater for layer.
 * @ingroup g_updater
 * Data structures and functions for layer updater.
 */

/**
 * Layer updater.
 * @ingroup g_updater_layer
 */
typedef struct _layer_updater_t_ {
    layer_t *layer; /**< the layer. */

    real_t *ac; /**< activation of layer. */
    real_t *er; /**< error of layer. */

    bool activated; /**< activation indicator. */
    bool cleared; /**< cleared indicator. */
} layer_updater_t;

/**
 * Destroy a layer_updater and set the pointer to NULL.
 * @ingroup g_updater_layer
 * @param[in] ptr pointer to updater_t.
 */
#define safe_layer_updater_destroy(ptr) do {\
    if((ptr) != NULL) {\
        layer_updater_destroy(ptr);\
        safe_free(ptr);\
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
 * @param[in] connlm the connlm model.
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
 * Activate a layer_updater.
 * @ingroup g_updater_layer
 * @param[in] layer_updater the layer_updater.
 * @param[in] offset offset of layer.
 * @return non-zero value if any error.
 */
int layer_updater_activate(layer_updater_t *layer_updater, int offset);

/**
 * Clear a layer_updater.
 * @ingroup g_updater_layer
 * @param[in] layer_updater the layer_updater.
 * @param[in] offset offset of layer.
 * @return non-zero value if any error.
 */
int layer_updater_clear(layer_updater_t *layer_updater, int offset);

#ifdef __cplusplus
}
#endif

#endif
