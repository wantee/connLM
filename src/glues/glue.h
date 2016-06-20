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

#include <stutils/st_opt.h>

#include <connlm/config.h>

#include "input.h"
#include "output.h"
#include "layers/layer.h"
#include "param.h"

/** @defgroup g_glue NNet glue.
 * Data structures and functions for NNet glue.
 */

typedef int glue_id_t;
#define GLUE_ID_NONE -1
#define GLUE_ID_FMT "%d"
typedef int glue_offset_t;
#define GLUE_OFFSET_FMT "%d"

typedef struct _glue_t_ glue_t;
/**
 * Implementation of a NNet glue.
 * @ingroup g_glue
 */
typedef struct _glue_implementation_t_ {
    char type[MAX_NAME_LEN]; /**< type of glue. */

    int (*init)(glue_t *glue); /**< init glue. */

    void (*destroy)(glue_t *glue); /**< destroy glue. */

    int (*dup)(glue_t *dst, glue_t *src); /**< duplicate glue. */

    int (*parse_topo)(glue_t *glue,
            const char *line); /**< parse topo for glue. */

    bool (*check)(glue_t *glue, layer_t **layers,
            layer_id_t n_layer); /**< check glue's definition. */

    char* (*draw_label)(glue_t *glue, char *label,
            size_t label_len); /**< label for drawing a glue. */

    int (*load_header)(void **extra, int version, FILE *fp,
            bool *binary, FILE *fo_info); /**< load header of glue. */

    int (*load_body)(void *extra, int version, FILE *fp,
            bool binary); /**< load body of glue. */

    int (*save_header)(void *extra, FILE *fp,
            bool binary); /**< save header of glue. */

    int (*save_body)(void *extra, FILE *fp,
            bool binary); /**< save body of glue. */

    int (*init_data)(glue_t *glue, input_t *input,
            layer_t **layers, output_t *output); /**< init data of glue. */

    int (*load_train_opt)(glue_t *glue, st_opt_t *opt,
            const char *sec_name,
            param_t *parent); /**< load train opt for  glue. */
} glue_impl_t;

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
    bool recur; /**< whether this glue is recurrent. */

    glue_impl_t *impl; /**< implementation for glue. */
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

/**
 * Check a glue after loading from topo line.
 * @ingroup g_glue
 * @param[in] glue the loaded glue.
 * @return true if OK, else false
 */
bool glue_check(glue_t *glue);

/**
 * Load glue header and initialise a new glue.
 * @ingroup g_glue
 * @param[out] glue glue initialised.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[out] binary whether the file stream is in binary format.
 * @param[in] fo file stream used to print information, if it is not NULL.
 * @see glue_load_body
 * @see glue_save_header, glue_save_body
 * @return non-zero value if any error.
 */
int glue_load_header(glue_t **glue, int version,
        FILE *fp, bool *binary, FILE *fo_info);

/**
 * Load glue body.
 * @ingroup g_glue
 * @param[in] glue glue to be loaded.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[in] binary whether to use binary format.
 * @see glue_load_header
 * @see glue_save_header, glue_save_body
 * @return non-zero value if any error.
 */
int glue_load_body(glue_t *glue, int version, FILE *fp, bool binary);

/**
 * Save glue header.
 * @ingroup g_glue
 * @param[in] glue glue to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see glue_save_body
 * @see glue_load_header, glue_load_body
 * @return non-zero value if any error.
 */
int glue_save_header(glue_t *glue, FILE *fp, bool binary);

/**
 * Save glue body.
 * @ingroup g_glue
 * @param[in] glue glue to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see glue_save_header
 * @see glue_load_header, glue_load_body
 * @return non-zero value if any error.
 */
int glue_save_body(glue_t *glue, FILE *fp, bool binary);

/**
 * Provide label string for drawing glue.
 * @ingroup g_glue
 * @param[in] glue glue.
 * @param[out] label buffer to write string.
 * @param[in] label_len length of label.
 * @param[in] verbose verbose output.
 * @return label on success, NULL if any error.
 */
char* glue_draw_label(glue_t *glue, char *label, size_t label_len,
        bool verbose);

/**
 * Provide label string for drawing one link of glue.
 * @ingroup g_glue
 * @param[in] glue glue.
 * @param[in] in_layer id of layer.
 * @param[out] label buffer to write string.
 * @param[in] labe_len length of label.
 * @param[in] verbose verbose output.
 * @return label on success, NULL if any error.
 */
char* glue_draw_label_one(glue_t *glue, layer_id_t lid,
        char *label, size_t label_len, bool verbose);

/**
 * Initialise extra data of a glue.
 * @ingroup g_glue
 * @param[in] glue glue.
 * @param[in] input input layer of network.
 * @param[in] layers layers of network.
 * @param[in] output output layer of network.
 * @return non-zero if any error
 */
int glue_init_data(glue_t *glue, input_t *input,
        layer_t **layers, output_t *output);

/**
 * Load glue train option.
 * @ingroup g_glue
 * @param[in] glue glue to be loaded with.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @param[in] parent parent param.
 * @return non-zero value if any error.
 */
int glue_load_train_opt(glue_t *glue, st_opt_t *opt,
        const char *sec_name, param_t *parent);

#ifdef __cplusplus
}
#endif

#endif
