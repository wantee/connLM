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

#ifndef  _CONNLM_TANH_LAYER_H_
#define  _CONNLM_TANH_LAYER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <connlm/config.h>
#include "layer.h"

/** @defgroup g_layer_tanh tanh layer. 
 * @ingroup g_layer
 * Data structures and functions for tanh layer.
 */

#define TANH_NAME "tanh"

/**
 * Destroy a tanh layer.
 * @ingroup g_layer_tanh
 * @param[in] layer tanh layer to be destroyed.
 */
void tanh_destroy(layer_t* layer);

/**
 * Initialize a tanh layer.
 * @ingroup g_layer_tanh
 * @param[in] layer tanh layer to be initialized.
 * @return non-zero if any error
 */
int tanh_init(layer_t *layer);

/**
 * Duplicate a tanh layer.
 * @ingroup g_layer_tanh
 * @param[out] dst dst layer to be duplicated.
 * @param[in] src src layer to be duplicated.
 * @return non-zero if any error
 */
int tanh_dup(layer_t *dst, layer_t *src);

/**
 * Parse a topo config line.
 * @ingroup g_layer_tanh
 * @param[in] layer specific layer.
 * @param[in] line topo config line.
 * @return non-zero if any error
 */
int tanh_parse_topo(layer_t *layer, const char *line);

#ifdef __cplusplus
}
#endif

#endif
