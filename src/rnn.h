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

#ifndef  _RNN_H_
#define  _RNN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <st_opt.h>

#include "config.h"
#include "nn.h"

typedef unsigned long long hash_t;

typedef struct _rnn_opt_t {
    real_t scale;

    int hidden_size;

    int bptt;
    int bptt_block;

    nn_param_t param;
} rnn_opt_t;

typedef struct _rnn_t_ {
    rnn_opt_t rnn_opt;

    int iter;
    double logp;
    double llogp;

    long counter;
    long train_cur_pos;
    long train_words;
    long anti_k;

    int vocab_size;
    int class_size;
    int hidden_size;

    int bptt;
    int bptt_block;

    int *bptt_hist;
    real_t *ac_bptt_h;
    real_t *er_bptt_h;
    real_t *wt_bptt_ih_w;
    real_t *wt_bptt_ih_h;

    long long direct_size;
    int direct_order;
    int *d_hist;
    hash_t *d_hash;

    real_t starting_alpha;
    real_t alpha;
    int    alpha_divide;
    real_t beta;
    real_t min_improvement;
    int max_iterations;

    int independent;

    // input layer
    //real_t *ac_i_w;             // word vector
    //real_t *er_i_w;
    real_t *ac_i_h;             // hidden layer for last time step
    real_t *er_i_h;

    // hidden layer
    real_t *ac_h;
    real_t *er_h;

    // output layer
    real_t *ac_o_c;             // class part
    real_t *er_o_c;
    real_t *ac_o_w;             // word part
    real_t *er_o_w;

    // weights between input and hidden layer
    real_t *wt_ih_w;            // word vs hidden
    real_t *wt_ih_h;            // hidden vs hidden

    // weights between hidden and output layer
    real_t *wt_ho_c;            // hidden vs class
    real_t *wt_ho_w;            // hidden vs word

    // MaxEnt parameters
    real_t *wt_d;

    // weights backup
    real_t *wt_ih_w_bak;
    real_t *wt_ih_h_bak;

    real_t *wt_ho_c_bak;
    real_t *wt_ho_w_bak;

    real_t *wt_d_bak;

    real_t gradient_cutoff;
    long report_progress;
} rnn_t;

int rnn_load_opt(rnn_opt_t* rnn_opt, st_opt_t *opt, const char *sec_name,
        nn_param_t *param);

rnn_t *rnn_create();
#define safe_rnn_destroy(ptr) do {\
    if((ptr) != NULL) {\
        rnn_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
void rnn_destroy(rnn_t *rnn);
rnn_t* rnn_dup(rnn_t *r);

int rnn_load_header(rnn_t **rnn, FILE *fp, bool *binary, FILE *fo_info);
int rnn_load_body(rnn_t *rnn, FILE *fp, bool binary);
int rnn_save_header(rnn_t *rnn, FILE *fp, bool binary);
int rnn_save_body(rnn_t *rnn, FILE *fp, bool binary);

int rnn_setup_train(rnn_t **rnn, rnn_opt_t *rnn_opt);
int rnn_train(rnn_t * rnn);

#ifdef __cplusplus
}
#endif

#endif
