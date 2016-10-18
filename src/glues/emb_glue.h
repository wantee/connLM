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

#ifndef  _CONNLM_EMB_GLUE_H_
#define  _CONNLM_EMB_GLUE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <connlm/config.h>

#include "param.h"
#include "glue.h"

/** @defgroup g_glue_emb Embedding Glue
 * @ingroup g_glue
 * Glue connects the input layer and a hidden layer.
 */

#define EMB_GLUE_NAME "emb"

/**
 * Parse a topo config line.
 * @ingroup g_glue_emb
 * @param[in] glue specific glue.
 * @param[in] line topo config line.
 * @return non-zero if any error
 */
int emb_glue_parse_topo(glue_t *glue, const char *line);

/**
 * Check a emb glue is valid.
 * @ingroup g_glue_emb
 * @param[in] glue specific glue.
 * @param[in] layers layers in the component.
 * @param[in] n_layer number of layers.
 * @param[in] input input layer.
 * @param[in] output output layer.
 * @return non-zero if any error
 */
bool emb_glue_check(glue_t *glue, layer_t **layers, int n_layer,
        input_t *input, output_t *output);

/**
 * Initialise data of emb glue.
 * @ingroup g_glue_emb
 * @param[in] glue glue.
 * @param[in] input input layer of network.
 * @param[in] layers layers of network.
 * @param[in] output output layer of network.
 * @return non-zero if any error
 */
int emb_glue_init_data(glue_t *glue, input_t *input,
        layer_t **layers, output_t *output);

/**
 * Initialize a wt_updater for emb glue.
 * @ingroup g_glue_emb
 * @param[in] glue the glue.
 * @param[in] param param used for initialising, if not NULL.
 * @return wt_updater on success, otherwise NULL.
 */
wt_updater_t* emb_glue_init_wt_updater(glue_t *glue, param_t *param);

#ifdef __cplusplus
}
#endif

#endif
