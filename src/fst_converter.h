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

#ifndef  _CONNLM_FST_CONVERTER_H_
#define  _CONNLM_FST_CONVERTER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <pthread.h>

#include <stutils/st_block_cache.h>

#include <connlm/config.h>

#include "connlm.h"
#include "updaters/updater.h"

/** @defgroup g_conv connLM to WFST Converter
 * Convert a connLM model to WFST.
 */

/**
 * Type of back-off methods.
 * @ingroup g_conv
 */
typedef enum _backoff_method_t_ {
    BOM_UNKNOWN = -1, /**< Unknown method. */
    BOM_BEAM = 0, /**< Beam method. */
    BOM_SAMPLING = 1, /**< Sampling method. */
} backoff_method_t;

/**
 * Options for converter.
 * @ingroup g_conv
 */
typedef struct _fst_converter_opt_t_ {
    bool print_syms; /**< print symbols instead of id, if true. */
    backoff_method_t bom; /**< backoff method. */
    union {
        double beam; /**< threshold for beam method. */
        double boost; /**< boost probability for sampling method. */
    };
    unsigned int init_rand_seed; /**< initial random seed. */
} fst_conv_opt_t;

/**
 * Load converter option.
 * @ingroup g_conv
 * @param[out] conv_opt options to be loaded.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @return non-zero value if any error.
 */
int fst_conv_load_opt(fst_conv_opt_t *conv_opt,
        st_opt_t *opt, const char *sec_name);

/**
 * FST State.
 * Contain the information for fst state.
 * @ingroup g_conv
 */
typedef struct _fst_state_t_ {
    int word_id; /**< word id. */
    int model_state_id; /**< model state id. */
    int parent; /**< parent state id of fst state. */
} fst_state_t;

/**
 * FST Converter.
 * @ingroup g_conv
 */
typedef struct _fst_converter_t_ {
    connlm_t *connlm; /**< the model. */

    updater_t **updaters; /**< the updaters. */
    int n_thr; /**< number of working threads. */

    int err; /**< error indicator. */

    int n_fst_state; /**< number of states of FST. */
    pthread_mutex_t n_fst_state_lock; /**< lock for n_fst_state. */

    st_block_cache_t *model_state_cache; /**< cache for internal state of model. */
    pthread_mutex_t model_state_cache_lock; /**< lock for model_state_cache. */

    fst_state_t *fst_states; /**< fst_state_info array indexed by fst state id. */
    int cap_fst_states; /**< capacity of fst_states. */
    pthread_mutex_t fst_state_lock; /**< lock for fst_states. */

    FILE *fst_fp; /**< output file pointer for fst file. */
    pthread_mutex_t fst_fp_lock; /**< lock for fst_fp/ */

    fst_conv_opt_t conv_opt; /**< options. */
} fst_conv_t;

/**
 * Destroy a fst converter and set the pointer to NULL.
 * @ingroup g_conv
 * @param[in] ptr pointer to fst_conv_t.
 */
#define safe_fst_conv_destroy(ptr) do {\
    if((ptr) != NULL) {\
        fst_conv_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a fst converter.
 * @ingroup g_conv
 * @param[in] conv converter to be destroyed.
 */
void fst_conv_destroy(fst_conv_t *conv);

/**
 * Create a fst converter.
 * @ingroup g_conv
 * @param[in] connlm the connlm model.
 * @param[in] n_thr number of worker threads.
 * @param[in] conv_opt options for convert.
 * @return converter on success, otherwise NULL.
 */
fst_conv_t* fst_conv_create(connlm_t *connlm, int n_thr,
        fst_conv_opt_t *conv_opt);

/**
 * Convert the connLM model to WFST.
 * @ingroup g_conv
 * @param[in] conv the fst converter.
 * @param[in] fst_fp stream to print out the WFST.
 * @return non-zero value if any error.
 */
int fst_conv_convert(fst_conv_t *conv, FILE *fst_fp);

#ifdef __cplusplus
}
#endif

#endif
