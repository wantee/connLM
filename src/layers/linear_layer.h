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

#ifndef  _CONNLM_LINEAR_LAYER_H_
#define  _CONNLM_LINEAR_LAYER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <connlm/config.h>
#include "layer.h"

/** @defgroup g_layer_linear Linear Layer
 * @ingroup g_layer
 * Hidden layer with linear activation.
 */

#define LINEAR_NAME "linear"

/**
 * Data for linear layer.
 * @ingroup g_layer_linear
 */
typedef struct _linear_data_t_ {
    real_t scale; /**< scale for linear. y = scale * x */
} linear_data_t;

/**
 * Destroy a linear layer.
 * @ingroup g_layer_linear
 * @param[in] layer linear layer to be destroyed.
 */
void linear_destroy(layer_t* layer);

/**
 * Initialize a linear layer.
 * @ingroup g_layer_linear
 * @param[in] layer linear layer to be initialized.
 * @return non-zero if any error
 */
int linear_init(layer_t *layer);

/**
 * Duplicate a linear layer.
 * @ingroup g_layer_linear
 * @param[out] dst dst layer to be duplicated.
 * @param[in] src src layer to be duplicated.
 * @return non-zero if any error
 */
int linear_dup(layer_t *dst, layer_t *src);

/**
 * Parse a topo config line.
 * @ingroup g_layer_linear
 * @param[in] layer specific layer.
 * @param[in] line topo config line.
 * @return non-zero if any error
 */
int linear_parse_topo(layer_t *layer, const char *line);

/**
 * Load linear layer header and initialise a new linear.
 * @ingroup g_layer_linear
 * @param[out] extra extra data to be initialised.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[out] fmt storage format.
 * @param[out] fo_info file stream used to print information, if it is not NULL.
 * @see linear_load_body
 * @see linear_save_header
 * @return non-zero value if any error.
 */
int linear_load_header(void **extra, int version,
        FILE *fp, connlm_fmt_t *fmt, FILE *fo_info);

/**
 * Load linear layer body.
 * @ingroup g_layer_linear
 * @param[in] extra extra data to be loaded.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[in] fmt storage format.
 * @see linear_load_header
 * @see linear_save_header
 * @return non-zero value if any error.
 */
int linear_load_body(void *extra, int version, FILE *fp, connlm_fmt_t fmt);

/**
 * Save linear layer header.
 * @ingroup g_layer_linear
 * @param[in] extra extra data to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] fmt storage format.
 * @see linear_load_header, linear_load_body
 * @return non-zero value if any error.
 */
int linear_save_header(void *extra, FILE *fp, connlm_fmt_t fmt);

/**
 * Activate linear layer.
 * @ingroup g_layer_linear
 * @param[in] layer the linear layer.
 * @param[in] vec the vector.
 * @param[in] size size of vector.
 * @return non-zero value if any error.
 */
int linear_activate(layer_t *layer, real_t *vec, int size);

/**
 * Deriv linear layer.
 * @ingroup g_layer_linear
 * @param[in] layer the linear layer.
 * @param[in] er the error.
 * @param[in] ac the activation.
 * @param[in] size size of vectors.
 * @return non-zero value if any error.
 */
int linear_deriv(layer_t *layer, real_t *er, real_t *ac, int size);

#ifdef __cplusplus
}
#endif

#endif
