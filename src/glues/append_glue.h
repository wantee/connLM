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

#ifndef  _CONNLM_APPEND_GLUE_H_
#define  _CONNLM_APPEND_GLUE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"
#include "glue.h"

/** @defgroup g_glue_append append glue. 
 * @ingroup g_glue
 * Data structures and functions for append glue.
 */

#define APPEND_GLUE_NAME "append"

/**
 * Destroy a append glue.
 * @ingroup g_glue_append
 * @param[in] glue append glue to be destroyed.
 */
void append_glue_destroy(glue_t* glue);

/**
 * Initialize a append glue.
 * @ingroup g_glue_append
 * @param[in] glue append glue to be initialized.
 * @return non-zero if any error
 */
int append_glue_init(glue_t *glue);

/**
 * Duplicate a append glue.
 * @ingroup g_glue_append
 * @param[out] dst dst glue to be duplicated.
 * @param[in] src src glue to be duplicated.
 * @return non-zero if any error
 */
int append_glue_dup(glue_t *dst, glue_t *src);

/**
 * Parse a topo config line.
 * @ingroup g_glue_append
 * @param[in] glue specific glue.
 * @param[in] line topo config line.
 * @return non-zero if any error
 */
int append_glue_parse_topo(glue_t *glue, const char *line);

/**
 * Check a append glue is valid.
 * @ingroup g_glue_append
 * @param[in] glue specific glue.
 * @param[in] layers layers in the component.
 * @param[in] n_layer number of layers.
 * @return non-zero if any error
 */
bool append_glue_check(glue_t *glue, layer_t **layer, layer_id_t n_layer);

#ifdef __cplusplus
}
#endif

#endif
