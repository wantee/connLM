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

#ifndef  _CONNLM_OUTPUT_WT_GLUE_H_
#define  _CONNLM_OUTPUT_WT_GLUE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <connlm/config.h>
#include "weights/output_weight.h"
#include "param.h"
#include "glue.h"

/** @defgroup g_glue_out_wt output weight glue.
 * @ingroup g_glue
 * Data structures and functions for out_wt glue.
 */

#define OUT_WT_GLUE_NAME "out_wt"

typedef struct _out_wt_glue_data_t_ {
    out_wt_t *out_wt;

    param_t param;
} out_wt_glue_data_t;

/**
 * Destroy a out_wt glue.
 * @ingroup g_glue_out_wt
 * @param[in] glue out_wt glue to be destroyed.
 */
void out_wt_glue_destroy(glue_t* glue);

/**
 * Initialize a out_wt glue.
 * @ingroup g_glue_out_wt
 * @param[in] glue out_wt glue to be initialized.
 * @return non-zero if any error
 */
int out_wt_glue_init(glue_t *glue);

/**
 * Duplicate a out_wt glue.
 * @ingroup g_glue_out_wt
 * @param[out] dst dst glue to be duplicated.
 * @param[in] src src glue to be duplicated.
 * @return non-zero if any error
 */
int out_wt_glue_dup(glue_t *dst, glue_t *src);

/**
 * Parse a topo config line.
 * @ingroup g_glue_out_wt
 * @param[in] glue specific glue.
 * @param[in] line topo config line.
 * @return non-zero if any error
 */
int out_wt_glue_parse_topo(glue_t *glue, const char *line);

/**
 * Check a out_wt glue is valid.
 * @ingroup g_glue_out_wt
 * @param[in] glue specific glue.
 * @param[in] layers layers in the component.
 * @param[in] n_layer number of layers.
 * @return non-zero if any error
 */
bool out_wt_glue_check(glue_t *glue, layer_t **layers,
        layer_id_t n_layer);

/**
 * Provide label string for drawing out_wt glue.
 * @ingroup g_glue_out_wt
 * @param[in] glue glue.
 * @param[out] label buffer to write string.
 * @param[in] labe_len length of label.
 * @return label on success, NULL if any error.
 */
char* out_wt_glue_draw_label(glue_t *glue, char *label,
        size_t label_len);

/**
 * Load out_wt_glue header and initialise a new out_wt_glue.
 * @ingroup g_glue_out_wt
 * @param[out] extra extra data to be initialised.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[out] binary whether the file stream is in binary format.
 * @param[in] fo file stream used to print information, if it is not NULL.
 * @see out_wt_glue_load_body
 * @see out_wt_glue_save_header, out_wt_glue_save_body
 * @return non-zero value if any error.
 */
int out_wt_glue_load_header(void **extra, int version,
        FILE *fp, bool *binary, FILE *fo_info);

/**
 * Load out_wt_glue body.
 * @ingroup g_glue_out_wt
 * @param[in] extra extra data to be loaded.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[in] binary whether to use binary format.
 * @see out_wt_glue_load_header
 * @see out_wt_glue_save_header, out_wt_glue_save_body
 * @return non-zero value if any error.
 */
int out_wt_glue_load_body(void *extra, int version, FILE *fp, bool binary);

/**
 * Save out_wt_glue header.
 * @ingroup g_glue_out_wt
 * @param[in] extra extra data to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see out_wt_glue_save_body
 * @see out_wt_glue_load_header, out_wt_glue_load_body
 * @return non-zero value if any error.
 */
int out_wt_glue_save_header(void *extra, FILE *fp, bool binary);

/**
 * Save out_wt_glue body.
 * @ingroup g_glue_out_wt
 * @param[in] extra extra data to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see out_wt_glue_save_header
 * @see out_wt_glue_load_header, out_wt_glue_load_body
 * @return non-zero value if any error.
 */
int out_wt_glue_save_body(void *extra, FILE *fp, bool binary);

/**
 * Initialise extra data of out_wt glue.
 * @ingroup g_glue_out_wt
 * @param[in] glue glue.
 * @param[in] input input layer of network.
 * @param[in] layers layers of network.
 * @param[in] output output layer of network.
 * @return non-zero if any error
 */
int out_wt_glue_init_data(glue_t *glue, input_t *input,
        layer_t **layers, output_t *output);

/**
 * Load out_wt_glue train option.
 * @ingroup g_glue_out_wt
 * @param[in] glue glue to be loaded with.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @param[in] parent parent param.
 * @return non-zero value if any error.
 */
int out_wt_glue_load_train_opt(glue_t *glue, st_opt_t *opt,
        const char *sec_name, param_t *parent);

#ifdef __cplusplus
}
#endif

#endif
