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

#ifndef  _CONNLM_LAYER_H_
#define  _CONNLM_LAYER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <connlm/config.h>

/** @defgroup g_layer NNet hidden layer. 
 * Data structures and functions for NNet hidden layer.
 */

typedef int layer_id_t;
#define LAYER_ID_NONE -1

/**
 * NNet hidden layer.
 * @ingroup g_layer
 */
typedef struct _layer_t_ {
    char name[MAX_NAME_LEN]; /**< layer name. */
    char type[MAX_NAME_LEN]; /**< layer type. */
    int size; /**< layer size. */

    int (*forward)(struct _layer_t_ *layer); /**< forward function. */
    int (*backprop)(struct _layer_t_ *layer); /**< backprop function. */

    void *extra; /**< hook to store extra data. */
} layer_t;

/**
 * Destroy a layer and set the pointer to NULL.
 * @ingroup g_layer
 * @param[in] ptr pointer to layer_t.
 */
#define safe_layer_destroy(ptr) do {\
    if((ptr) != NULL) {\
        layer_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a layer.
 * @ingroup g_layer
 * @param[in] layer layer to be destroyed.
 */
void layer_destroy(layer_t* layer);

/**
 * Duplicate a layer.
 * @ingroup g_layer
 * @param[in] l layer to be duplicated.
 * @return the duplicated layer. 
 */
layer_t* layer_dup(layer_t *l);

/**
 * Parse a topo config line, and return a new layer.
 * @ingroup g_layer
 * @param[in] line topo config line.
 * @return a new layer or NULL if error.
 */
layer_t* layer_parse_topo(const char *line);

#ifdef __cplusplus
}
#endif

#endif
