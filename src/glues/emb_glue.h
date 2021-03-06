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
 * Type of combination methods.
 * @ingroup g_glue_emb
 */
typedef enum _emb_combination_method_t_ {
    EC_UNKNOWN = -1, /**< Unknown combination method. */
    EC_UNDEFINED = 0, /**< Undefined combination method. */
    EC_SUM, /**< Sum up all contexts' embedding. */
    EC_AVG, /**< Average of all contexts' embedding. */
    EC_CONCAT, /**< Concat all contexts' embedding. */
} emb_combine_t;

/**
 * Data for emb glue
 * @ingroup g_glue_emb
 */
typedef struct _emb_glue_data_t_ {
    emb_combine_t combine; /**< combine method. */
} emb_glue_data_t;

/**
 * Destroy a emb glue.
 * @ingroup g_glue_emb
 * @param[in] glue emb glue to be destroyed.
 */
void emb_glue_destroy(glue_t* glue);

/**
 * Initialize a emb glue.
 * @ingroup g_glue_emb
 * @param[in] glue emb glue to be initialized.
 * @return non-zero if any error
 */
int emb_glue_init(glue_t *glue);

/**
 * Duplicate a emb glue.
 * @ingroup g_glue_emb
 * @param[out] dst dst glue to be duplicated.
 * @param[in] src src glue to be duplicated.
 * @return non-zero if any error
 */
int emb_glue_dup(glue_t *dst, glue_t *src);

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
 * Provide label string for drawing emb glue.
 * @ingroup g_glue_emb
 * @param[in] glue glue.
 * @param[out] label buffer to write string.
 * @param[in] label_len length of label.
 * @return label on success, NULL if any error.
 */
char* emb_glue_draw_label(glue_t *glue, char *label, size_t label_len);

/**
 * Load emb glue header and initialise a new emb_glue.
 * @ingroup g_glue_emb
 * @param[out] extra extra data to be initialised.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[out] fmt storage format.
 * @param[out] fo_info file stream used to print information, if it is not NULL.
 * @see emb_glue_save_header
 * @return non-zero value if any error.
 */
int emb_glue_load_header(void **extra, int version,
        FILE *fp, connlm_fmt_t *fmt, FILE *fo_info);

/**
 * Save emb glue header.
 * @ingroup g_glue_emb
 * @param[in] extra extra data to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] fmt storage format.
 * @see emb_glue_load_header
 * @return non-zero value if any error.
 */
int emb_glue_save_header(void *extra, FILE *fp, connlm_fmt_t fmt);

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

#ifdef __cplusplus
}
#endif

#endif
