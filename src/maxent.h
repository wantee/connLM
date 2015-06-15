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

#ifndef  _MAXENT_H_
#define  _MAXENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <st_opt.h>

#include "config.h"
#include "param.h"
#include "output.h"

typedef struct _maxent_model_opt_t {
    real_t scale;

    hash_size_t sz_w;
    hash_size_t sz_c;
    int order;
} maxent_model_opt_t;

typedef struct _maxent_train_opt_t {
    param_t param;
} maxent_train_opt_t;

typedef struct _hash_range_t_ {
    hash_t s;
    union {
        hash_t e;
        hash_t num;
    };
} hash_range_t;

typedef struct _hash_hist_t_ {
    int order;
    hash_range_t *range;
} hash_hist_t;

typedef struct _maxent_neuron_t_ {
    param_arg_t param_arg_c;
    param_arg_t param_arg_w;

    int *hist;
    // neuron actived by a n-gram feature of history
    hash_t *hash_c; 
    hash_t *hash_w; 
#ifndef _MAXENT_BP_CALC_HASH_
    int hash_order_c;
    int hash_order_w;
#endif
    int step;

    // caches for mini-batch
    hash_range_t *hash_union;
    int max_hash_hist_num;
    hash_hist_t *hash_hist_c;
    int hash_hist_c_num;
    hash_hist_t *hash_hist_w;
    int hash_hist_w_num;
    real_t *wt_c;
    real_t *wt_w;
} maxent_neuron_t;

typedef struct _maxent_t_ {
    maxent_model_opt_t model_opt;

    int vocab_size;
    int class_size;

    real_t *wt_c;
    real_t *wt_w;

    output_t *output;

    maxent_train_opt_t train_opt;
    maxent_neuron_t *neurons;
    int num_thrs;
} maxent_t;

int maxent_load_model_opt(maxent_model_opt_t *model_opt,
        st_opt_t *opt, const char *sec_name);
int maxent_load_train_opt(maxent_train_opt_t *train_opt,
        st_opt_t *opt, const char *sec_name, param_t *param);

int maxent_init(maxent_t **pmaxent, maxent_model_opt_t *model_opt,
        output_t *output);
#define safe_maxent_destroy(ptr) do {\
    if((ptr) != NULL) {\
        maxent_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
void maxent_destroy(maxent_t *maxent);
maxent_t* maxent_dup(maxent_t *m);

int maxent_load_header(maxent_t **maxent, FILE *fp,
        bool *binary, FILE *fo_info);
int maxent_load_body(maxent_t *maxent, FILE *fp, bool binary);
int maxent_save_header(maxent_t *maxent, FILE *fp, bool binary);
int maxent_save_body(maxent_t *maxent, FILE *fp, bool binary);

int maxent_forward(maxent_t *maxent, int word, int tid);
int maxent_backprop(maxent_t *maxent, int word, int tid);

int maxent_setup_train(maxent_t *maxent, maxent_train_opt_t *train_opt,
        output_t *output, int num_thrs);
int maxent_reset_train(maxent_t *maxent, int tid);
int maxent_start_train(maxent_t *maxent, int word, int tid);
int maxent_end_train(maxent_t *maxent, int word, int tid);
int maxent_finish_train(maxent_t *maxent, int tid);

int maxent_setup_test(maxent_t *maxent, output_t *output, int num_thrs);
int maxent_reset_test(maxent_t *maxent, int tid);
int maxent_start_test(maxent_t *maxent, int word, int tid);
int maxent_end_test(maxent_t *maxent, int word, int tid);

int maxent_union(hash_range_t *range, int *n_range, hash_range_t *hash,
        int num_hash, hash_size_t sz_hash);
#ifdef __cplusplus
}
#endif

#endif
