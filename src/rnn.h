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
#include "param.h"
#include "output.h"

typedef struct _rnn_model_opt_t {
    real_t scale;

    int hidden_size;
} rnn_model_opt_t;

typedef struct _rnn_train_opt_t {
    int bptt;
    int bptt_block;

    real_t er_cutoff;
    param_t param;
} rnn_train_opt_t;

typedef struct _rnn_t_ {
    rnn_model_opt_t model_opt;

    int vocab_size;
    int class_size;

    output_t *output;

    int last_word;

    // weights between input and hidden layer
    real_t *wt_ih_w;            // word vs hidden
    real_t *wt_ih_h;            // hidden vs hidden

    // weights between hidden and output layer
    real_t *wt_ho_c;            // hidden vs class
    real_t *wt_ho_w;            // hidden vs word

    rnn_train_opt_t train_opt;
    param_arg_t arg_ih_w;
    param_arg_t arg_ih_h;
    param_arg_t arg_ho_c;
    param_arg_t arg_ho_w;
    int step;
    int block_step;

    int *bptt_hist;
    real_t *ac_bptt_h;
    real_t *er_bptt_h;
    real_t *wt_bptt_ih_w;
    real_t *wt_bptt_ih_h;

    // input layer
    real_t *ac_i_h;             // hidden layer for last time step
    real_t *er_i_h;

    // hidden layer
    real_t *ac_h;
    real_t *er_h;
} rnn_t;

int rnn_load_model_opt(rnn_model_opt_t *model_opt, st_opt_t *opt,
        const char *sec_name);
int rnn_load_train_opt(rnn_train_opt_t *train_opt, st_opt_t *opt,
        const char *sec_name, param_t *param);

int rnn_init(rnn_t **prnn, rnn_model_opt_t *rnn_opt, output_t *output);

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

int rnn_forward(rnn_t *rnn, int word);
int rnn_backprop(rnn_t *rnn, int word);

int rnn_setup_train(rnn_t *rnn, rnn_train_opt_t *train_opt,
        output_t *output);
int rnn_reset_train(rnn_t *rnn);
int rnn_start_train(rnn_t *rnn, int word);
int rnn_fwd_bp(rnn_t *rnn, int word);
int rnn_end_train(rnn_t *rnn, int word);

int rnn_setup_test(rnn_t *rnn, output_t *output);
int rnn_reset_test(rnn_t *rnn);
int rnn_start_test(rnn_t *rnn, int word);
int rnn_end_test(rnn_t *rnn, int word);

#ifdef __cplusplus
}
#endif

#endif
