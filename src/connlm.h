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
 * Common parameters for running with connLM model.
 * @ingroup g_connlm
 */
typedef struct _connlm_opt_t_ {
    int num_thread;           /**< number of threads. */
    int epoch_size;  /**< number sentences read one time per thread. */
    unsigned int rand_seed;   /**< seed for random function. */
    bool shuffle;             /**< whether shuffle the sentences. */
    char debug_file[MAX_DIR_LEN]; /**< file to print out debug infos. */
} connlm_opt_t;

/**
 * Parameters for training connLM model.
 * @ingroup g_connlm
 */
typedef struct _connlm_train_opt_t_ {
    param_t param;            /**< training parameters. */
} connlm_train_opt_t;

/**
 * Parameters for evaluating connLM model.
 * @ingroup g_connlm
 */
typedef struct _connlm_eval_opt_t_ {
    bool print_sent_prob; /**< print sentence prob only, if true. */
    real_t out_log_base; /**< log base for printing prob. */
} connlm_eval_opt_t;

/**
 * Parameters for generate text.
 * @ingroup g_connlm
 */
typedef struct _connlm_gen_opt_t_ {
    char prefix_file[MAX_DIR_LEN]; /**< file storing the prefix(es) for generating text. */
    unsigned int rand_seed;   /**< seed for random function. */
} connlm_gen_opt_t;

/**
 * A bunch of word
 * @ingroup g_connlm
 */
typedef struct _connlm_egs_t_ {
    int *words; /**< word ids. */
    int size; /**< size of words. */
    int capacity; /**< capacity of words. */
    struct _connlm_egs_t_ *next; /**< pointer to the next list element. */
} connlm_egs_t;

/**
 * connLM model.
 * @ingroup g_connlm
 */
typedef struct _connlm_t_ {
    connlm_opt_t opt; /**< common options */
    connlm_train_opt_t train_opt; /**< training options */
    connlm_eval_opt_t eval_opt; /**< evaluating options */
    connlm_gen_opt_t gen_opt; /**< generating options */

    char text_file[MAX_DIR_LEN]; /**< training/evaling file name. */

    connlm_egs_t *full_egs; /**< pool for egs filled with data. */
    connlm_egs_t *empty_egs; /**< pool for egs with data consumed. */
    st_sem_t sem_full; /**< number of full egs. */
    st_sem_t sem_empty; /**< number of empty egs. */
    pthread_mutex_t full_egs_lock; /**< lock for full egs pool. */
    pthread_mutex_t empty_egs_lock; /**< lock for empty egs pool. */

    int err; /**< error indicator. */
    FILE *fp_log;    /**< file pointer to print out log. */
    pthread_mutex_t fp_lock; /**< lock for fp_log. */

    FILE *fp_debug; /**< file pointer to print out debug info. */
    pthread_mutex_t fp_debug_lock; /**< lock for fp_debug_log. */

    output_t *output; /**< output layer */
    vocab_t *vocab;   /**< vocab */

    component_t **comps; /**< components. */
    comp_id_t num_comp; /**< number of components. */
} connlm_t;

/**
 * Load connlm option.
 * @ingroup g_connlm
 * @param[out] connlm_opt options loaded.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @return non-zero value if any error.
 */
int connlm_load_opt(connlm_opt_t *connlm_opt, st_opt_t *opt,
        const char *sec_name);

/**
 * Load connlm eval option.
 * @ingroup g_connlm
 * @param[out] eval_opt options loaded.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @return non-zero value if any error.
 */
int connlm_load_eval_opt(connlm_eval_opt_t *eval_opt,
        st_opt_t *opt, const char *sec_name);

/**
 * Load connlm gen option.
 * @ingroup g_connlm
 * @param[out] gen_opt options loaded.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @return non-zero value if any error.
 */
int connlm_load_gen_opt(connlm_gen_opt_t *gen_opt,
        st_opt_t *opt, const char *sec_name);

/**
 * Load connlm train option.
 * @ingroup g_connlm
 * @param[in] connlm connlm model to be loaded with.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @return non-zero value if any error.
 */
int connlm_load_train_opt(connlm_t *connlm, st_opt_t *opt,
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
 * @return non-zero value if any error.
 */
int connlm_draw(connlm_t *connlm, FILE *fp);

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

/**
 * Feed-forward one word for a thread of connlm model.
 * @ingroup g_connlm
 * @param[in] connlm connlm model.
 * @param[in] word current word.
 * @param[in] tid thread id.
 * @see connlm_backprop
 * @return non-zero value if any error.
 */
int connlm_forward(connlm_t *connlm, int word, int tid);

/**
 * Back-propagate one word for a thread of connlm model.
 * @ingroup g_connlm
 * @param[in] connlm connlm model.
 * @param[in] word current word.
 * @param[in] tid thread id.
 * @see connlm_forward
 * @return non-zero value if any error.
 */
int connlm_backprop(connlm_t *connlm, int word, int tid);

/**
 * Setup training for connlm model. Called before training.
 * @ingroup g_connlm
 * @param[in] connlm connlm model.
 * @param[in] train_opt training options.
 * @param[in] train_file training corpus file.
 * @return non-zero value if any error.
 */
int connlm_setup_train(connlm_t *connlm, connlm_opt_t *connlm_opt,
        const char *train_file);

/**
 * Training between feed-forward and back-propagate for connlm model.
 * Called between forward and backprop during training a word.
 * @ingroup g_connlm
 * @param[in] connlm connlm model.
 * @param[in] word current word.
 * @param[in] tid thread id.
 * @return non-zero value if any error.
 */
int connlm_fwd_bp(connlm_t *connlm, int word, int tid);

/**
 * Training a connlm model.
 * @ingroup g_connlm
 * @param[in] connlm connlm model.
 * @return non-zero value if any error.
 */
int connlm_train(connlm_t *connlm);

/**
 * Setup evaluating for connlm model. Called before evaluating.
 * @ingroup g_connlm
 * @param[in] connlm connlm model.
 * @param[in] eval_opt evaluating options.
 * @param[in] eval_file evaluating corpus file.
 * @return non-zero value if any error.
 */
int connlm_setup_eval(connlm_t *connlm, connlm_opt_t *eval_opt,
        const char *eval_file);

/**
 * Evaluating a connlm model.
 * @ingroup g_connlm
 * @param[in] connlm connlm model.
 * @param[in] fp_log file stream to write log out.
 * @return non-zero value if any error.
 */
int connlm_eval(connlm_t *connlm, FILE *fp_log);

/**
 * Setup generating for connlm model. Called before generating.
 * @ingroup g_connlm
 * @param[in] connlm connlm model.
 * @param[in] gen_opt generating options.
 * @return non-zero value if any error.
 */
int connlm_setup_gen(connlm_t *connlm, connlm_gen_opt_t *gen_opt);

/**
 * Testing a connlm model.
 * @ingroup g_connlm
 * @param[in] connlm connlm model.
 * @param[in] num_sents number of sentences to be generate.
 * @return non-zero value if any error.
 */
int connlm_gen(connlm_t *connlm, int num_sents);

#ifdef __cplusplus
}
#endif

#endif
