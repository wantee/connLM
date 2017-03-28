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

#include <stutils/st_mem.h>

#include <connlm/config.h>

/** @defgroup g_layer NNet hidden layer
 * Hidden layer for NNet with various activation types.
 */

typedef struct _layer_t_ layer_t;
/**
 * NNet hidden layer implementaion.
 * @ingroup g_layer
 */
typedef struct _layer_implementation_t_ {
    char type[MAX_NAME_LEN]; /**< type of implementaion. */

    int (*init)(layer_t *layer); /**< init layer. */

    void (*destroy)(layer_t *layer); /**< destroy layer. */

    int (*dup)(layer_t *dst, layer_t *src); /**< duplicate layer. */

    int (*parse_topo)(layer_t *layer,
            const char *line); /**< parse topo of layer. */

    char* (*draw_label)(layer_t *layer, char *label,
            size_t label_len); /**< label for drawing layer. */

    int (*load_header)(void **extra, int version, FILE *fp,
            bool *binary, FILE *fo_info); /**< load header of layer. */

    int (*load_body)(void *extra, int version, FILE *fp,
            bool binary); /**< load body of layer. */

    int (*save_header)(void *extra, FILE *fp,
            bool binary); /**< save header of layer. */

    int (*save_body)(void *extra, FILE *fp,
            bool binary); /**< save body layer. */
} layer_impl_t;

/**
 * NNet hidden layer.
 * @ingroup g_layer
 */
typedef struct _layer_t_ {
    char name[MAX_NAME_LEN]; /**< layer name. */
    char type[MAX_NAME_LEN]; /**< layer type. */
    int size; /**< layer size. */

    layer_impl_t *impl; /**< implementaion of the layer. */
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
        safe_st_free(ptr);\
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

/**
 * Load layer header and initialise a new layer.
 * @ingroup g_layer
 * @param[out] layer layer initialised.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[out] binary whether the file stream is in binary format.
 * @param[in] fo_info file stream used to print information, if it is not NULL.
 * @see layer_load_body
 * @see layer_save_header, layer_save_body
 * @return non-zero value if any error.
 */
int layer_load_header(layer_t **layer, int version,
        FILE *fp, bool *binary, FILE *fo_info);

/**
 * Load layer body.
 * @ingroup g_layer
 * @param[in] layer layer to be loaded.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[in] binary whether to use binary format.
 * @see layer_load_header
 * @see layer_save_header, layer_save_body
 * @return non-zero value if any error.
 */
int layer_load_body(layer_t *layer, int version, FILE *fp, bool binary);

/**
 * Save layer header.
 * @ingroup g_layer
 * @param[in] layer layer to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see layer_save_body
 * @see layer_load_header, layer_load_body
 * @return non-zero value if any error.
 */
int layer_save_header(layer_t *layer, FILE *fp, bool binary);

/**
 * Save layer body.
 * @ingroup g_layer
 * @param[in] layer layer to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see layer_save_header
 * @see layer_load_header, layer_load_body
 * @return non-zero value if any error.
 */
int layer_save_body(layer_t *layer, FILE *fp, bool binary);

/**
 * Provide label string for drawing layer.
 * @ingroup g_layer
 * @param[in] layer layer.
 * @param[out] label buffer to write string.
 * @param[in] label_len length of label.
 * @return label on success, NULL if any error.
 */
char* layer_draw_label(layer_t *layer, char *label, size_t label_len);

#ifdef __cplusplus
}
#endif

#endif
