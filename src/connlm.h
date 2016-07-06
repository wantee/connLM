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

#ifndef  _CONNLM_H_
#define  _CONNLM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>

#include <stutils/st_utils.h>
#include <stutils/st_alphabet.h>
#include <stutils/st_semaphore.h>

#include <connlm/config.h>
#include "utils.h"
#include "reader.h"
#include "vocab.h"
#include "output.h"
#include "param.h"

#include "component.h"

/** @defgroup g_connlm connLM Model
 * Data structure and functions for connLM model.
 */

/**
 * Return connLM git revision.
 * @ingroup g_connlm
 * @return revision.
 */
const char* connlm_revision();

/**
 * connLM model.
 * @ingroup g_connlm
 */
typedef struct _connlm_t_ {
    output_t *output; /**< output layer */
    vocab_t *vocab;   /**< vocab */

    component_t **comps; /**< components. */
    int num_comp; /**< number of components. */
} connlm_t;

/**
 * Load connlm train param.
 * @ingroup g_connlm
 * @param[in] connlm connlm model to be loaded with.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @return non-zero value if any error.
 */
int connlm_load_param(connlm_t *connlm, st_opt_t *opt,
        const char *sec_name);

/**
 * Destroy a connLM model and set the pointer to NULL.
 * @ingroup g_connlm
 * @param[in] ptr pointer to connlm_t.
 */
#define safe_connlm_destroy(ptr) do {\
    if((ptr) != NULL) {\
        connlm_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a connlm model.
 * @ingroup g_connlm
 * @param[in] connlm connlm to be destroyed.
 */
void connlm_destroy(connlm_t *connlm);

/**
 * Initialise a connlm model.
 * @ingroup g_connlm
 * @param[in] connlm connlm to be initialised.
 * @param[in] topo_fp File stream for topology file.
 * @return non-zero value if any error.
 */
int connlm_init(connlm_t *connlm, FILE *topo_fp);

/**
 * New a connlm model with some components.
 * @ingroup g_connlm
 * @param[in] vocab vocab.
 * @param[in] output output layer.
 * @param[in] comps components.
 * @param[in] n_comp number of components.
 * @return a connlm model.
 */
connlm_t *connlm_new(vocab_t *vocab, output_t *output,
        component_t **comps, int n_comp);

/**
 * Load a connlm model from file stream.
 * @ingroup g_connlm
 * @param[in] fp file stream loaded from.
 * @return a connlm model.
 */
connlm_t* connlm_load(FILE *fp);

/**
 * Save a connlm model.
 * @ingroup g_connlm
 * @param[in] connlm connlm to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @return non-zero value if any error.
 */
int connlm_save(connlm_t *connlm, FILE *fp, bool binary);

/**
 * Convert a connlm model into graphviz format.
 * @ingroup g_connlm
 * @param[in] connlm connlm model.
 * @param[in] fp file stream to be printed out.
 * @param[in] verbose verbose output.
 * @return non-zero value if any error.
 */
int connlm_draw(connlm_t *connlm, FILE *fp, bool verbose);

/**
 * Print info of a connlm model file stream.
 * @ingroup g_connlm
 * @param[in] model_fp file stream read from.
 * @param[in] fo file stream print info to.
 * @return non-zero value if any error.
 */
int connlm_print_info(FILE *model_fp, FILE *fo);

/**
 * Filter a connlm model according to filter.
 * @ingroup g_connlm
 * @param[in] connlm connlm model.
 * @param[in] mf model filter.
 * @param[in] comp_names component names for filter.
 * @param[in] num_comp number of component names for filter.
 * @return non-zero value if any error.
 */
int connlm_filter(connlm_t *connlm, model_filter_t mf,
        const char *comp_names, int num_comp);

#ifdef __cplusplus
}
#endif

#endif
