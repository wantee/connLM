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

#ifndef  _CONNLM_OUTPUT_GLUE_H_
#define  _CONNLM_OUTPUT_GLUE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <connlm/config.h>
#include "param.h"
#include "glue.h"

/** @defgroup g_glue_out Output Glue
 * @ingroup g_glue
 * Glue connects a hidden layers and the output layer.
 */

#define OUT_GLUE_NAME "out"

/**
 * Parse a topo config line.
 * @ingroup g_glue_out
 * @param[in] glue specific glue.
 * @param[in] line topo config line.
 * @return non-zero if any error
 */
int out_glue_parse_topo(glue_t *glue, const char *line);

/**
 * Check a out glue is valid.
 * @ingroup g_glue_out
 * @param[in] glue specific glue.
 * @param[in] layers layers in the component.
 * @param[in] n_layer number of layers.
 * @param[in] input input layer.
 * @param[in] output output layer.
 * @return non-zero if any error
 */
bool out_glue_check(glue_t *glue, layer_t **layers, int n_layer,
        input_t *input, output_t *output);

/**
 * Initialise data of out glue.
 * @ingroup g_glue_out
 * @param[in] glue glue.
 * @param[in] input input layer of network.
 * @param[in] layers layers of network.
 * @param[in] output output layer of network.
 * @return non-zero if any error
 */
int out_glue_init_data(glue_t *glue, input_t *input,
        layer_t **layers, output_t *output);

#ifdef __cplusplus
}
#endif

#endif
