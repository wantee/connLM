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

#ifndef  _CONNLM_DIRECT_GLUE_H_
#define  _CONNLM_DIRECT_GLUE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <connlm/config.h>

#include "input.h"
#include "output.h"
#include "weights/direct_weight.h"
#include "param.h"

#include "glue.h"

/** @defgroup g_glue_direct direct glue.
 * @ingroup g_glue
 * Data structures and functions for direct glue.
 */

#define DIRECT_GLUE_NAME "direct_wt"

typedef struct _direct_glue_data_t_ {
    hash_size_t hash_sz;
    direct_wt_t *direct_wt;

    param_t param;
} direct_glue_data_t;

/**
 * Destroy a direct glue.
 * @ingroup g_glue_direct
 * @param[in] glue direct glue to be destroyed.
 */
void direct_glue_destroy(glue_t* glue);

/**
 * Initialize a direct glue.
 * @ingroup g_glue_direct
 * @param[in] glue direct glue to be initialized.
 * @return non-zero if any error
 */
int direct_glue_init(glue_t *glue);

/**
 * Duplicate a direct glue.
 * @ingroup g_glue_direct
 * @param[out] dst dst glue to be duplicated.
 * @param[in] src src glue to be duplicated.
 * @return non-zero if any error
 */
int direct_glue_dup(glue_t *dst, glue_t *src);

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
 * Load direct_glue header and initialise a new direct_glue.
 * @ingroup g_glue_direct
 * @param[out] extra extra data to be initialised.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[out] binary whether the file stream is in binary format.
 * @param[in] fo file stream used to print information, if it is not NULL.
 * @see direct_glue_load_body
 * @see direct_glue_save_header, direct_glue_save_body
 * @return non-zero value if any error.
 */
int direct_glue_load_header(void **extra, int version,
        FILE *fp, bool *binary, FILE *fo_info);

/**
 * Load direct_glue body.
 * @ingroup g_glue_direct
 * @param[in] extra extra data to be loaded.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[in] binary whether to use binary format.
 * @see direct_glue_load_header
 * @see direct_glue_save_header, direct_glue_save_body
 * @return non-zero value if any error.
 */
int direct_glue_load_body(void *extra, int version, FILE *fp, bool binary);

/**
 * Save direct_glue header.
 * @ingroup g_glue_direct
 * @param[in] extra extra data to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see direct_glue_save_body
 * @see direct_glue_load_header, direct_glue_load_body
 * @return non-zero value if any error.
 */
int direct_glue_save_header(void *extra, FILE *fp, bool binary);

/**
 * Save direct_glue body.
 * @ingroup g_glue_direct
 * @param[in] extra extra data to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see direct_glue_save_header
 * @see direct_glue_load_header, direct_glue_load_body
 * @return non-zero value if any error.
 */
int direct_glue_save_body(void *extra, FILE *fp, bool binary);

/**
 * Initialise extra data of direct glue.
 * @ingroup g_glue_direct
 * @param[in] glue glue.
 * @param[in] input input layer of network.
 * @param[in] layers layers of network.
 * @param[in] output output layer of network.
 * @return non-zero if any error
 */
int direct_glue_init_data(glue_t *glue, input_t *input,
        layer_t **layers, output_t *output);

/**
 * Load direct_glue train option.
 * @ingroup g_glue_direct
 * @param[in] glue glue to be loaded with.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @param[in] parent parent param.
 * @return non-zero value if any error.
 */
int direct_glue_load_train_opt(glue_t *glue, st_opt_t *opt,
        const char *sec_name, param_t *parent);

/**
 * Feed-forward one word for a direct_glue.
 * @ingroup g_glue_direct
 * @param[in] glue glue.
 * @param[in] comp_updater the comp_updater.
 * @param[in] out_updater the out_updater.
 * @return non-zero value if any error.
 */
int direct_glue_forward(glue_t *glue, comp_updater_t *comp_updater,
        out_updater_t *out_updater);

#ifdef __cplusplus
}
#endif

#endif
