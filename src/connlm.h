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

#include <st_utils.h>
#include <st_alphabet.h>
#include <st_semaphore.h>

#include "config.h"
#include "vocab.h"
#include "output.h"
#include "param.h"
#include "rnn.h"
#include "maxent.h"
#include "lbl.h"
#include "ffnn.h"

/** @defgroup g_connlm connLM Model 
 * Data structure and functions for connLM model.
 */

/**
 * Parameters for connLM model.
 * @ingroup g_connlm
 */
typedef struct _connlm_model_opt_t_ {
    unsigned int rand_seed;       /**< seed for random function. */

    output_opt_t output_opt;       /**< options for output layer */

    rnn_model_opt_t rnn_opt;       /**< options for RNN model */
    maxent_model_opt_t maxent_opt; /**< options for MaxEnt model */
    lbl_model_opt_t lbl_opt;       /**< options for LBL model */
    ffnn_model_opt_t ffnn_opt;     /**< options for FFNN model */
} connlm_model_opt_t;

/**
 * Parameters for training connLM model.
 * @ingroup g_connlm
 */
typedef struct _connlm_train_opt_t_ {
    param_t param;            /**< training parameters. */

    int num_thread;           /**< number of threads. */
    unsigned int rand_seed;   /**< seed for random function. */
 
    int epoch_size;  /**< number sentences read one time per thread. */
    bool shuffle;             /**< whether shuffle the sentences. */

    rnn_train_opt_t rnn_opt; /**< training options for RNN model */
    maxent_train_opt_t maxent_opt; /**< training options for MaxEnt model */
    lbl_train_opt_t lbl_opt; /**< training options for LBL model */
    ffnn_train_opt_t ffnn_opt; /**< training options for FFNN model */

    char debug_file[MAX_DIR_LEN]; /**< file to print out debug infos. */
} connlm_train_opt_t;

/**
 * Parameters for testing connLM model.
 * @ingroup g_connlm
 */
typedef struct _connlm_test_opt_t_ {
    int num_thread;           /**< number of threads. */

    int epoch_size; /**< number sentences read one time per thread. */
    bool print_sent_prob; /**< print sentence prob only, if true. */
    real_t out_log_base; /**< log base for printing prob. */

    char debug_file[MAX_DIR_LEN]; /**< file to print out debug infos. */
} connlm_test_opt_t;

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
    connlm_model_opt_t model_opt; /**< model options */
    connlm_train_opt_t train_opt; /**< training options */
    connlm_test_opt_t test_opt; /**< testing options */
    connlm_gen_opt_t gen_opt; /**< generating options */

    char text_file[MAX_DIR_LEN]; /**< training/testing file name. */

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
    rnn_t *rnn;       /**< RNN model. May be NULL */
    maxent_t *maxent; /**< MaxEnt model. May be NULL */
    lbl_t *lbl;       /**< LBL model. May be NULL */
    ffnn_t *ffnn;     /**< FFNN model. May be NULL */
} connlm_t;

/**
 * Load connlm model option.
 * @ingroup g_connlm
 * @param[out] connlm_opt options loaded.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @return non-zero value if any error.
 */
int connlm_load_model_opt(connlm_model_opt_t *connlm_opt, 
        st_opt_t *opt, const char *sec_name);

/**
 * Load connlm train option.
 * @ingroup g_connlm
 * @param[out] train_opt options loaded.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @return non-zero value if any error.
 */
int connlm_load_train_opt(connlm_train_opt_t *train_opt, 
        st_opt_t *opt, const char *sec_name);

/**
 * Load connlm test option.
 * @ingroup g_connlm
 * @param[out] test_opt options loaded.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @return non-zero value if any error.
 */
int connlm_load_test_opt(connlm_test_opt_t *test_opt, 
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
 * @param[in] model_opt model options used to initialise.
 * @return non-zero value if any error.
 */
int connlm_init(connlm_t *connlm, connlm_model_opt_t *model_opt);

/**
 * New a connlm model with some components.
 * @ingroup g_connlm
 * @param[in] vocab vocab.
 * @param[in] output output layer.
 * @param[in] rnn RNN model.
 * @param[in] maxent MaxEnt model.
 * @param[in] lbl LBL model.
 * @param[in] ffnn FFNN model.
 * @return a connlm model.
 */
connlm_t *connlm_new(vocab_t *vocab, output_t *output,
        rnn_t *rnn, maxent_t *maxent, lbl_t* lbl, ffnn_t *ffnn);

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
 * Print info of a connlm model file stream.
 * @ingroup g_connlm
 * @param[in] model_fp file stream read from.
 * @param[in] fo file stream print info to.
 * @return non-zero value if any error.
 */
int connlm_print_info(FILE *model_fp, FILE *fo);

/**
 * Get the size for HS tree input.
 * @ingroup g_connlm
 * @param[in] connlm connlm model.
 * @return the size, zero or negtive value if any error.
 */
int connlm_get_hs_size(connlm_t *connlm);

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
int connlm_setup_train(connlm_t *connlm, connlm_train_opt_t *train_opt,
        const char *train_file);

/**
 * Reset training for connlm model.
 * Called before every input sentence to be trained.
 * @ingroup g_connlm
 * @param[in] connlm connlm model.
 * @param[in] tid thread id.
 * @return non-zero value if any error.
 */
int connlm_reset_train(connlm_t *connlm, int tid);

/**
 * Start training for connlm model.
 * Called before every input word to be trained.
 * @ingroup g_connlm
 * @param[in] connlm connlm model.
 * @param[in] word current word.
 * @param[in] tid thread id.
 * @return non-zero value if any error.
 */
int connlm_start_train(connlm_t *connlm, int word, int tid);

/**
 * End training for connlm model.
 * Called after every input word trained.
 * @ingroup g_connlm
 * @param[in] connlm connlm model.
 * @param[in] word current word.
 * @param[in] tid thread id.
 * @return non-zero value if any error.
 */
int connlm_end_train(connlm_t *connlm, int word, int tid);

/**
 * Finish training for connlm model.
 * Called after all words trained.
 * @ingroup g_connlm
 * @param[in] connlm connlm model.
 * @param[in] tid thread id.
 * @return non-zero value if any error.
 */
int connlm_finish_train(connlm_t *connlm, int tid);

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
 * Setup testing for connlm model. Called before testing.
 * @ingroup g_connlm
 * @param[in] connlm connlm model.
 * @param[in] test_opt testing options.
 * @param[in] test_file testing corpus file.
 * @return non-zero value if any error.
 */
int connlm_setup_test(connlm_t *connlm, connlm_test_opt_t *test_opt,
        const char *test_file);

/**
 * Reset testing for connlm model.
 * Called before every input sentence to be tested.
 * @ingroup g_connlm
 * @param[in] connlm connlm model.
 * @param[in] tid thread id.
 * @return non-zero value if any error.
 */
int connlm_reset_test(connlm_t *connlm, int tid);

/**
 * Start testing for connlm model.
 * Called before every input word to be tested.
 * @ingroup g_connlm
 * @param[in] connlm connlm model.
 * @param[in] word current word.
 * @param[in] tid thread id.
 * @return non-zero value if any error.
 */
int connlm_start_test(connlm_t *connlm, int word, int tid);

/**
 * End testing for connlm model.
 * Called after every input word tested.
 * @ingroup g_connlm
 * @param[in] connlm connlm model.
 * @param[in] word current word.
 * @param[in] tid thread id.
 * @return non-zero value if any error.
 */
int connlm_end_test(connlm_t *connlm, int word, int tid);

/**
 * Testing a connlm model.
 * @ingroup g_connlm
 * @param[in] connlm connlm model.
 * @param[in] fp_log file stream to write log out.
 * @return non-zero value if any error.
 */
int connlm_test(connlm_t *connlm, FILE *fp_log);

/**
 * Setup generating for connlm model. Called before generating.
 * @ingroup g_connlm
 * @param[in] connlm connlm model.
 * @param[in] gen_opt generating options.
 * @return non-zero value if any error.
 */
int connlm_setup_gen(connlm_t *connlm, connlm_gen_opt_t *gen_opt);

/**
 * Reset generating for connlm model.
 * Called before every input sentence to be generated.
 * @ingroup g_connlm
 * @param[in] connlm connlm model.
 * @return non-zero value if any error.
 */
int connlm_reset_gen(connlm_t *connlm);

/**
 * End generating for connlm model.
 * Called after every input word generated.
 * @ingroup g_connlm
 * @param[in] connlm connlm model.
 * @param[in] word current generated word.
 * @return non-zero value if any error.
 */
int connlm_end_gen(connlm_t *connlm, int word);

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
