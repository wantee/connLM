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

#include <stutils/st_semaphore.h>

#include <connlm/config.h>

#include "connlm.h"
#include "updaters/updater.h"

/** @defgroup g_converter connLM to WFST Converter
 * Convert a connLM model to WFST.
 */

/**
 * Options for converter.
 * @ingroup g_converter
 */
typedef struct _fst_converter_opt_t_ {
    bool print_syms; /**< print symbols instead of id, if true. */
} fst_converter_opt_t;

/**
 * Load converter option.
 * @ingroup g_converter
 * @param[out] eval_opt options loaded.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @return non-zero value if any error.
 */
int fst_converter_load_opt(fst_converter_opt_t *converter_opt,
        st_opt_t *opt, const char *sec_name);

/**
 * FST Converter.
 * @ingroup g_converter
 */
typedef struct _fst_converter_t_ {
    connlm_t *connlm; /**< the model. */

    updater_t **updaters; /**< the updaters. */
    int n_thr; /**< number of working threads. */

    int err; /**< error indicator. */

    fst_converter_opt_t converter_opt; /**< options. */
} fst_converter_t;

/**
 * Destroy a fst converter and set the pointer to NULL.
 * @ingroup g_converter
 * @param[in] ptr pointer to fst_converter_t.
 */
#define safe_fst_converter_destroy(ptr) do {\
    if((ptr) != NULL) {\
        fst_converter_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a fst converter.
 * @ingroup g_converter
 * @param[in] converter converter to be destroyed.
 */
void fst_converter_destroy(fst_converter_t *converter);

/**
 * Create a fst converter.
 * @ingroup g_converter
 * @param[in] connlm the connlm model.
 * @param[in] n_thr number of worker threads.
 * @param[in] converter_opt options for convert.
 * @return converter on success, otherwise NULL.
 */
fst_converter_t* fst_converter_create(connlm_t *connlm, int n_thr,
        fst_converter_opt_t *converter_opt);

/**
 * Convert the connLM model to WFST.
 * @ingroup g_converter
 * @param[in] converter the fst converter.
 * @param[in] fst_fp stream to print out the WFST.
 * @return non-zero value if any error.
 */
int fst_converter_convert(fst_converter_t *converter, FILE *fst_fp);

#ifdef __cplusplus
}
#endif

#endif
