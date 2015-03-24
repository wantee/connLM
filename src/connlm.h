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

#include <st_utils.h>
#include <st_alphabet.h>

#include "config.h"
#include "rnn.h"
#include "maxent.h"
#include "lbl.h"
#include "ffnn.h"

typedef struct _connlm_param_t_ {
    real_t learn_rate;
    real_t l1_penalty;
    real_t l2_penalty;
    real_t momentum;
    real_t gradient_cutoff;
} connlm_param_t;

typedef struct _connlm_output_opt_t_ {
    int class_size;
    bool hs;
} connlm_output_opt_t;

typedef enum _connlm_mode_t_ {
    MODE_TRAIN,
    MODE_TEST,
} connlm_mode_t;

typedef struct _connlm_train_opt_t_ {
    char train_file[MAX_DIR_LEN];
    char valid_file[MAX_DIR_LEN];

    int max_word_num;

    char in_model_file[MAX_DIR_LEN];
    char out_model_file[MAX_DIR_LEN];

    bool binary;
    bool report_progress;

    bool independent;

    int num_line_read;
    bool shuffle;

    connlm_param_t param;
    connlm_output_opt_t output_opt;

    rnn_opt_t rnn_opt;
    maxent_opt_t maxent_opt;
    lbl_opt_t lbl_opt;
    ffnn_opt_t ffnn_opt;
} connlm_train_opt_t;

typedef struct _connlm_test_opt_t {
    char test_file[MAX_DIR_LEN];

    char model_file[MAX_DIR_LEN];
} connlm_test_opt_t;

typedef struct _connlm_opt_t_ {
    int mode;
    int num_thread;
    int max_word_per_sent;
    int rand_seed;

    union {
        connlm_train_opt_t train_opt;
        connlm_test_opt_t test_opt;
    };
} connlm_opt_t;

typedef struct _word_info_t_ {
    int id;
    unsigned long cnt;
} word_info_t;

typedef struct _connlm_t_ {
    connlm_opt_t connlm_opt;

    st_rand_t random;

    FILE *text_fp;
    int *egs;
    int *shuffle_buf;

    st_alphabet_t *vocab;
    int vocab_size;
    word_info_t *word_infos;

    rnn_t *rnn;
    maxent_t *maxent;
    lbl_t *lbl;
    ffnn_t *ffnn;
} connlm_t;

int connlm_param_load(connlm_param_t *connlm_param, 
        stconf_t *pconf, const char *sec_name);

int connlm_output_opt_load(connlm_output_opt_t *output_opt, 
        stconf_t *pconf, const char *sec_name);

int connlm_load_opt(connlm_opt_t *connlm_opt, 
        stconf_t *pconf, const char *sec_name);

#define safe_connlm_destroy(ptr) do {\
    if(ptr != NULL) {\
        connlm_destroy(ptr);\
        safe_free(ptr);\
        ptr = NULL;\
    }\
    } while(0)
void connlm_destroy(connlm_t *connlm);

connlm_t *connlm_create(connlm_opt_t *connlm_opt);

int connlm_train(connlm_t *connlm);
int connlm_test(connlm_t *connlm);

#ifdef __cplusplus
}
#endif

#endif
