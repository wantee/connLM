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

#ifndef  _CONNLM_DRIVER_H_
#define  _CONNLM_DRIVER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#include <stutils/st_semaphore.h>

#include <connlm/config.h>

#include "connlm.h"
#include "reader.h"
#include "updaters/updater.h"

/** @defgroup g_driver connLM Driver
 * Driver to perform forward or backprop with connLM model.
 */

/**
 * Driver mode.
 * @ingroup g_driver
 */
typedef enum _driver_mode_t_ {
    DRIVER_TRAIN = 0, /**< train. */
    DRIVER_EVAL, /**< evaluate. */
    DRIVER_GEN, /**< generate. */
} driver_mode_t;

/**
 * Options for evaluating connLM model.
 * @ingroup g_driver
 */
typedef struct _driver_eval_opt_t_ {
    bool print_sent_prob; /**< print sentence prob only, if true. */
    real_t out_log_base; /**< log base for printing prob. */
} driver_eval_opt_t;

/**
 * Options for generate text.
 * @ingroup g_driver
 */
typedef struct _driver_gen_opt_t_ {
    char prefix_file[MAX_DIR_LEN]; /**< file storing the prefix(es) for generating text. */
    unsigned int rand_seed;   /**< seed for random function. */
} driver_gen_opt_t;

/**
 * Load eval option.
 * @ingroup g_driver
 * @param[out] eval_opt options to be loaded.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @return non-zero value if any error.
 */
int driver_load_eval_opt(driver_eval_opt_t *eval_opt,
        st_opt_t *opt, const char *sec_name);

/**
 * Load gen option.
 * @ingroup g_driver
 * @param[out] gen_opt options to be loaded.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @return non-zero value if any error.
 */
int driver_load_gen_opt(driver_gen_opt_t *gen_opt,
        st_opt_t *opt, const char *sec_name);

/**
 * Driver.
 * @ingroup g_driver
 */
typedef struct _driver_t_ {
    connlm_t *connlm; /**< the model. */
    reader_t *reader; /**< the reader. */

    updater_t **updaters; /**< the updaters. */
    int n_thr; /**< number of working threads. */

    int err; /**< error indicator. */

    driver_mode_t mode; /**< driver mode. */
    // for eval
    driver_eval_opt_t eval_opt; /**< Eval options. */
    FILE *fp_log;    /**< file pointer to print out log. */

    // for gen
    driver_gen_opt_t gen_opt; /**< Gen options. */
    int gen_num_sents;    /**< number of sentences to be generated. */
} driver_t;

/**
 * Destroy a driver and set the pointer to NULL.
 * @ingroup g_driver
 * @param[in] ptr pointer to driver_t.
 */
#define safe_driver_destroy(ptr) do {\
    if((ptr) != NULL) {\
        driver_destroy(ptr);\
        safe_st_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a driver.
 * @ingroup g_driver
 * @param[in] driver driver to be destroyed.
 */
void driver_destroy(driver_t *driver);

/**
 * Create a driver.
 * @ingroup g_driver
 * @param[in] connlm the connlm model.
 * @param[in] reader the reader.
 * @param[in] n_thr number of worker threads.
 * @return driver on success, otherwise NULL.
 */
driver_t* driver_create(connlm_t *connlm, reader_t *reader, int n_thr);

/**
 * Setup driver for running.
 * @ingroup g_driver
 * @param[in] driver driver.
 * @param[in] mode running mode.
 * @return non-zero value if any error.
 */
int driver_setup(driver_t *driver, driver_mode_t mode);

/**
 * Start running driver.
 * @ingroup g_driver
 * @param[in] driver driver.
 * @return non-zero value if any error.
 */
int driver_run(driver_t *driver);

/**
 * Set options for eval.
 * @ingroup g_driver
 * @param[in] driver driver.
 * @param[in] eval_opt eval option.
 * @param[in] fp_log logging fp. used for printing out prob.
 * @return non-zero value if any error.
 */
int driver_set_eval(driver_t *driver, driver_eval_opt_t *eval_opt,
        FILE *fp_log);

/**
 * Set options for gen.
 * @ingroup g_driver
 * @param[in] driver driver.
 * @param[in] gen_opt gen option.
 * @param[in] num_sents number of sentences to be generated.
 * @return non-zero value if any error.
 */
int driver_set_gen(driver_t *driver, driver_gen_opt_t *gen_opt,
        int num_sents);

#ifdef __cplusplus
}
#endif

#endif
