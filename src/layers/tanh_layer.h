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

#ifndef  _CONNLM_TANH_LAYER_H_
#define  _CONNLM_TANH_LAYER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <connlm/config.h>
#include "matrix.h"
#include "layer.h"

/** @defgroup g_layer_tanh Hyperbolic Tangent Layer
 * @ingroup g_layer
 * Hidden layer with tanh activation.
 */

#define TANH_NAME "tanh"

/**
 * Activate tanh layer.
 * @ingroup g_layer_tanh
 * @param[in] layer the tanh layer.
 * @param[in] ac the activation, activated row by row.
 * @return non-zero value if any error.
 */
int tanh_activate(layer_t *layer, mat_t *ac);

/**
 * Deriv tanh layer.
 * @ingroup g_layer_tanh
 * @param[in] layer the tanh layer.
 * @param[in] er the error.
 * @param[in] ac the activation.
 * @return non-zero value if any error.
 */
int tanh_deriv(layer_t *layer, mat_t *er, mat_t *ac);

/**
 * Generate random state for tanh layer.
 * @ingroup g_layer_tanh
 * @param[in] layer the tanh layer.
 * @param[out] state the generated state, row by row.
 * @return non-zero value if any error.
 */
int tanh_random_state(layer_t *layer, mat_t *state);

#ifdef __cplusplus
}
#endif

#endif
