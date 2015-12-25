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

#ifndef  _CONNLM_GLUE_H_
#define  _CONNLM_GLUE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <connlm/config.h>
#include "layers/layer.h"

/** @defgroup g_glue NNet glue. 
 * Data structures and functions for NNet glue.
 */

typedef int glue_id_t;
typedef int glue_offset_t;

/**
 * NNet glue.
 * @ingroup g_glue
 */
typedef struct _glue_t_ {
    char name[MAX_NAME_LEN]; /**< glue name. */
    char type[MAX_NAME_LEN]; /**< glue type. */
    layer_id_t* in_layers; /**< input layers. */
    glue_offset_t* in_offsets; /**< offset for input layers. */
    real_t* in_scales; /**< scale for input layers. */
    layer_id_t num_in_layer; /**< number of input layers. */
    layer_id_t* out_layers; /**< output layers. */
    glue_offset_t* out_offsets; /**< offset for output layers. */
    real_t* out_scales; /**< scale for output layers. */
    layer_id_t num_out_layer; /**< number of output layers. */

    int (*forward)(struct _glue_t_ *glue); /**< forward function. */
    int (*backprop)(struct _glue_t_ *glue); /**< backprop function. */

    void *extra; /**< hook to store extra data. */
} glue_t;

/**
 * Destroy a glue and set the pointer to NULL.
 * @ingroup g_glue
 * @param[in] ptr pointer to glue_t.
 */
#define safe_glue_destroy(ptr) do {\
    if((ptr) != NULL) {\
        glue_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a glue.
 * @ingroup g_glue
 * @param[in] glue glue to be destroyed.
 */
void glue_destroy(glue_t* glue);

/**
 * Duplicate a glue.
 * @ingroup g_glue
 * @param[in] g glue to be duplicated.
 * @return the duplicated glue. 
 */
glue_t* glue_dup(glue_t *g);

/**
 * Parse a topo config line, and return a new glue.
 * @ingroup g_glue
 * @param[in] line topo config line.
 * @param[in] layers named layers.
 * @param[in] n_layer number of named layers.
 * @return a new glue or NULL if error.
 */
glue_t* glue_parse_topo(const char *line, layer_t **layers, int n_layer);

#ifdef __cplusplus
}
#endif

#endif
