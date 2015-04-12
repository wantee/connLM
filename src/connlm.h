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

#include "config.h"
#include "vocab.h"
#include "output.h"
#include "param.h"
#include "rnn.h"
#include "maxent.h"
#include "lbl.h"
#include "ffnn.h"

typedef struct _connlm_model_opt_t_ {
    int rand_seed;

    output_opt_t output_opt;

    rnn_model_opt_t rnn_opt;
    maxent_model_opt_t maxent_opt;
    lbl_model_opt_t lbl_opt;
    ffnn_model_opt_t ffnn_opt;
} connlm_model_opt_t;

typedef struct _connlm_train_opt_t_ {
    param_t param;

    int num_thread;
    int max_word_per_sent;
    int rand_seed;
 
    int num_line_read;
    bool shuffle;

    rnn_train_opt_t rnn_opt;
    maxent_train_opt_t maxent_opt;
    lbl_train_opt_t lbl_opt;
    ffnn_train_opt_t ffnn_opt;
} connlm_train_opt_t;

typedef struct _connlm_test_opt_t_ {
    int num_thread;
    int max_word_per_sent;

    int num_line_read;

    real_t oov_penalty;
} connlm_test_opt_t;

struct _connlm_t_;
typedef struct _connlm_thread_t_ {
    struct _connlm_t_ *connlm;
    int tid;

    int eg_s;
    int eg_e;

    count_t words;
    count_t sents;
    double logp;

    count_t oovs;
} connlm_thr_t;

typedef struct _connlm_t_ {
    connlm_model_opt_t model_opt;
    connlm_train_opt_t train_opt;
    connlm_test_opt_t test_opt;

    unsigned random;

    FILE *text_fp;
    off_t fsize;
    int *egs;
    int *shuffle_buf;

    pthread_t *pts;
    int err;
    connlm_thr_t *thrs;
    FILE *fp_log;
    pthread_mutex_t fp_lock;

    output_t *output;
    vocab_t *vocab;
    rnn_t *rnn;
    maxent_t *maxent;
    lbl_t *lbl;
    ffnn_t *ffnn;
} connlm_t;

int connlm_load_model_opt(connlm_model_opt_t *connlm_opt, 
        st_opt_t *opt, const char *sec_name);

int connlm_load_train_opt(connlm_train_opt_t *train_opt, 
        st_opt_t *opt, const char *sec_name);

int connlm_load_test_opt(connlm_test_opt_t *test_opt, 
        st_opt_t *opt, const char *sec_name);

#define safe_connlm_destroy(ptr) do {\
    if((ptr) != NULL) {\
        connlm_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
void connlm_destroy(connlm_t *connlm);

int connlm_init(connlm_t *connlm, connlm_model_opt_t *model_opt);

connlm_t *connlm_new(vocab_t *vocab, output_t *output,
        rnn_t *rnn, maxent_t *maxent, lbl_t* lbl, ffnn_t *ffnn);

connlm_t* connlm_load(FILE *fp);
int connlm_save(connlm_t *connlm, FILE *fp, bool binary);

int connlm_print_info(FILE *model_fp, FILE *fo);

/**
 * Forward propagate
 */
int connlm_forward(connlm_t *connlm, int word, int tid);

/**
 * Backpropagate
 */
int connlm_backprop(connlm_t *connlm, int word, int tid);

/**
 * Setup training.
 */
int connlm_setup_train(connlm_t *connlm, connlm_train_opt_t *train_opt,
        const char *train_file);

/**
 * Reset training.
 * Called every sentence.
 */
int connlm_reset_train(connlm_t *connlm, int tid);

/**
 * Start training a word.
 */
int connlm_start_train(connlm_t *connlm, int word, int tid);

/**
 * End training a word.
 */
int connlm_end_train(connlm_t *connlm, int word, int tid);

/**
 * Between forward and backprop during training a word.
 */
int connlm_fwd_bp(connlm_t *connlm, int word, int tid);

/**
 * Training
 */
int connlm_train(connlm_t *connlm);

/**
 * Setup testing.
 */
int connlm_setup_test(connlm_t *connlm, connlm_test_opt_t *test_opt,
        const char *test_file);

/**
 * Reset testing.
 * Called every sentence.
 */
int connlm_reset_test(connlm_t *connlm, int tid);

/**
 * Start testing a word.
 */
int connlm_start_test(connlm_t *connlm, int word, int tid);

/**
 * End testing a word.
 */
int connlm_end_test(connlm_t *connlm, int word, int tid);

/**
 * Testing
 */
int connlm_test(connlm_t *connlm, FILE *fp_log);

#ifdef __cplusplus
}
#endif

#endif
