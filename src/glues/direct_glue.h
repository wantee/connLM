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

#ifndef  _CONNLM_DIRECT_GLUE_H_
#define  _CONNLM_DIRECT_GLUE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <connlm/config.h>

#include "input.h"
#include "output.h"
#include "param.h"

#include "glue.h"

/** @defgroup g_glue_direct direct glue.
 * @ingroup g_glue
 * Data structures and functions for direct glue.
 */

#define DIRECT_GLUE_NAME "direct"

typedef struct _direct_glue_data_t_ {
    int hash_sz; /**< size of hash wt. */
} direct_glue_data_t;

/**
 * Destroy a sum glue.
 * @ingroup g_glue_sum
 * @param[in] glue sum glue to be destroyed.
 */
void direct_glue_destroy(glue_t* glue);

/**
 * Initialize a sum glue.
 * @ingroup g_glue_sum
 * @param[in] glue sum glue to be initialized.
 * @return non-zero if any error
 */
int direct_glue_init(glue_t *glue);

/**
 * Duplicate a sum glue.
 * @ingroup g_glue_sum
 * @param[in] extra source data.
 * @return duplicated extra data, NULL if error.
 */
void* direct_glue_dup(void *extra);

/**
 * Parse a topo config line.
 * @ingroup g_glue_direct
 * @param[in] glue specific glue.
 * @param[in] line topo config line.
 * @return non-zero if any error
 */
int direct_glue_parse_topo(glue_t *glue, const char *line);

/**
 * Check a direct glue is valid.
 * @ingroup g_glue_direct
 * @param[in] glue specific glue.
 * @param[in] layers layers in the component.
 * @param[in] n_layer number of layers.
 * @param[in] input input layer.
 * @param[in] output output layer.
 * @return non-zero if any error
 */
bool direct_glue_check(glue_t *glue, layer_t **layers, int n_layer,
        input_t *input, output_t *output);

/**
 * Provide label string for drawing direct glue.
 * @ingroup g_glue_direct
 * @param[in] glue glue.
 * @param[out] label buffer to write string.
 * @param[in] labe_len length of label.
 * @return label on success, NULL if any error.
 */
char* direct_glue_draw_label(glue_t *glue, char *label, size_t label_len);

/**
 * Initialise data of direct glue.
 * @ingroup g_glue_direct
 * @param[in] glue glue.
 * @param[in] input input layer of network.
 * @param[in] layers layers of network.
 * @param[in] output output layer of network.
 * @return non-zero if any error
 */
int direct_glue_init_data(glue_t *glue, input_t *input,
        layer_t **layers, output_t *output);

#ifdef __cplusplus
}
#endif

#endif
