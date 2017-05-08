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

#ifndef  _CONNLM_INPUT_H_
#define  _CONNLM_INPUT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stutils/st_opt.h>
#include <stutils/st_int.h>

#include <connlm/config.h>

#include "layers/layer.h"

/** @defgroup g_input Input Layer
 * Input layer of NNet.
 */

/**
 * Type of combination methods.
 * @ingroup g_input
 */
typedef enum _input_combination_method_t_ {
    IC_UNKNOWN = -1, /**< Unknown combination method. */
    IC_UNDEFINED = 0, /**< Undefined combination method. */
    IC_SUM, /**< Sum up all contexts' embedding. */
    IC_AVG, /**< Average of all contexts' embedding. */
    IC_CONCAT, /**< Concat all contexts' embedding. */
} input_combine_t;

/**
 * Input Layer.
 * @ingroup g_input
 */
typedef struct _input_t_ {
    int input_size; /**< size of input layer. */

    st_wt_int_t *context; /**< context for input layer. */
    int n_ctx; /**< number of contexts for input layer. */

    input_combine_t combine; /**< combine method of input layer. */
} input_t;

/**
 * Destroy a input layer and set the pointer to NULL.
 * @ingroup g_input
 * @param[in] ptr pointer to input_t.
 */
#define safe_input_destroy(ptr) do {\
    if((ptr) != NULL) {\
        input_destroy(ptr);\
        safe_st_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a input layer.
 * @ingroup g_input
 * @param[in] input input layer to be destroyed.
 */
void input_destroy(input_t* input);

/**
 * Duplicate a input layer.
 * @ingroup g_input
 * @param[in] i input layer to be duplicated.
 * @return the duplicated input layer.
 */
input_t* input_dup(input_t *i);

/**
 * Parse a topo config line, and return a new input layer.
 * @ingroup g_input
 * @param[in] line topo config line.
 * @param[in] input_size size of input layer.
 * @return a new input layer or NULL if error.
 */
input_t* input_parse_topo(const char *line, int input_size);

#define INPUT_LAYER_NAME "input"

/**
 * return a layer struct for input.
 * @ingroup g_input
 * @param[in] input the input layer.
 * @return layer for the input, else NULL.
 */
layer_t* input_get_layer(input_t *input);

/**
 * Load input header and initialise a new input.
 * @ingroup g_input
 * @param[out] input input initialised.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[out] fmt storage format.
 * @param[out] fo_info file stream used to print information, if it is not NULL.
 * @see input_load_body
 * @see input_save_header, input_save_body
 * @return non-zero value if any error.
 */
int input_load_header(input_t **input, int version,
        FILE *fp, connlm_fmt_t *fmt, FILE *fo_info);

/**
 * Load input body.
 * @ingroup g_input
 * @param[in] input input to be loaded.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[in] fmt storage format.
 * @see input_load_header
 * @see input_save_header, input_save_body
 * @return non-zero value if any error.
 */
int input_load_body(input_t *input, int version, FILE *fp, connlm_fmt_t fmt);

/**
 * Save input header.
 * @ingroup g_input
 * @param[in] input input to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] fmt storage format.
 * @see input_save_body
 * @see input_load_header, input_load_body
 * @return non-zero value if any error.
 */
int input_save_header(input_t *input, FILE *fp, connlm_fmt_t fmt);

/**
 * Save input body.
 * @ingroup g_input
 * @param[in] input input to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] fmt storage format.
 * @see input_save_header
 * @see input_load_header, input_load_body
 * @return non-zero value if any error.
 */
int input_save_body(input_t *input, FILE *fp, connlm_fmt_t fmt);

/**
 * Provide label string for drawing input.
 * @ingroup g_input
 * @param[in] input input layer.
 * @param[out] label buffer to write string.
 * @param[in] label_len length of label.
 * @return label on success, NULL if any error.
 */
char* input_draw_label(input_t *input, char *label, size_t label_len);

/**
 * Print verbose info of a input.
 * @ingroup g_input
 * @param[in] input the input layer.
 * @param[in] fo file stream print info to.
 */
void input_print_verbose_info(input_t *input, FILE *fo);

#ifdef __cplusplus
}
#endif

#endif
