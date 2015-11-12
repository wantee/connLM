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

/** @defgroup g_rnn RNN Model
 * Data structures and functions for RNN model.
 */

/**
 * Parameters for RNN model.
 * @ingroup g_rnn
 */
typedef struct _rnn_model_opt_t_ {
    real_t scale;     /**< output scale. */

    int hidden_size;  /**< size of hidden layer. */
} rnn_model_opt_t;

/**
 * Parameters for training RNN model.
 * @ingroup g_rnn
 *
 */
typedef struct _rnn_train_opt_t_ {
    int bptt;          /**< number of BPTT steps. */
    int bptt_block;    /**< size of BPTT block. */

    real_t er_cutoff;  /**< cutoff of error values. */
    param_t param;     /**< training parameters. */
} rnn_train_opt_t;

/**
 * Neuron of RNN model.
 * Each neuron used in one single thread.
 * @ingroup g_rnn
 */
typedef struct _rnn_neuron_t {
    int last_word; /**< last input word. */

    param_arg_t arg_ih_w; /**< parameter arguments for input-hidden weights. */
    param_arg_t arg_ih_h; /**< parameter arguments for hidden-hidden weights. */
    param_arg_t arg_ho_c; /**< parameter arguments for class part of hidden-output weights. */
    param_arg_t arg_ho_w; /**< parameter arguments for word part of input-output weights. */
    int step; /**< steps within one sentence. */
    int block_step; /**< steps within one bptt block. */

    int *bptt_hist; /**< buffer storing word history for bptt. */
    real_t *ac_bptt_h; /**< buffer for activation of hidden for bptt. */
    real_t *er_bptt_h; /**< buffer for error of hidden for bptt. */
    real_t *wt_bptt_ih_w; /**< buffer for input-hidden weights for bptt. */
#ifdef _MINI_UPDATE_
    real_t *er_h_buf; /**< errors of hiddend layer for one mini-batch */
    real_t *ac_i_h_buf; /**< activation of hiddend part of input layer for one mini-batch */
#else
    real_t *wt_bptt_ih_h; /**< buffer for hidden-hidden weights for bptt. */
#endif
    int ih_buf_num; /**< size of er_h_buf and ac_i_h_buf, or used to indicate wt_i_h updated during mini-batch. */
    bool *dirty_word; /**< byte map for updated words. */

    // input layer
    real_t *ac_i_h; /**< activation of hidden layer for last time step. */
    real_t *er_i_h; /**< error of hidden layer for last time step. */

    // hidden layer
    real_t *ac_h; /**< activation of hidden layer. */
    real_t *er_h; /**< error of hidden layer. */

    /** @name Variables for mini-batch update. */
    /**@{*/
    int mini_step; /**< steps of mini-batch. */
    int *ho_w_hist; /**< words updated for hidden-output layer during one mini-batch. */
    int ho_w_hist_num; /**< number of words in ho_w_hist. */
    int *ih_w_hist; /**< words updated for input-hidden layer during one mini-batch. */
    int ih_w_hist_num; /**< number of words in in_w_hist. */
    bool *dirty_class; /**< byte map for updated classes. */

    real_t *wt_ih_w; /**< delta weight of input-hidden for one mini-batch */
#ifndef _MINI_UPDATE_
    real_t *wt_ih_h; /**< delta weight of hidden-hidden for one mini-batch */
#endif

    real_t *wt_ho_w; /**< delta weight of word part of hidden-output for one mini-batch */
#ifdef _MINI_UPDATE_
    real_t *er_o_c_buf; /**< errors of class part of output layer for one mini-batch */
    real_t *ac_h_buf; /**< activation of hidden layer for one mini-batch */
    int ho_c_buf_num; /**< size of er_o_c_buf and ac_h_buf. */ // should be always equal to ho_w_hist_num
#else
    real_t *wt_ho_c; /**< delta weight of class part of hidden-output for one mini-batch */
#endif
    /**@}*/
} rnn_neuron_t;

/**
 * RNN model.
 * @ingroup g_rnn
 */
typedef struct _rnn_t_ {
    rnn_model_opt_t model_opt; /**< option for this model. */

    int vocab_size; /**< size of vocab */
    int class_size; /**< size of class for output layer */

    output_t *output; /**< output layer of this model. */

    // weights between input and hidden layer
    real_t *wt_ih_w;  /**< input-hidden weights. */
    real_t *wt_ih_h;  /**< hidden-hidden weights. */

    // weights between hidden and output layer
    real_t *wt_ho_c;  /**< class part of hidden-output weights. */
    real_t *wt_ho_w;  /**< word part of hidden-output weights. */

    rnn_train_opt_t train_opt; /**< training options. */

    rnn_neuron_t *neurons; /**< neurons of the model. An array of size num_thrs. */
    int num_thrs; /**< number of threads/neurons. */
} rnn_t;

/**
 * Load rnn model option.
 * @ingroup g_rnn
 * @param[out] model_opt options loaded.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @return non-zero value if any error.
 */
int rnn_load_model_opt(rnn_model_opt_t *model_opt, st_opt_t *opt,
        const char *sec_name);
/**
 * Load rnn train option.
 * @ingroup g_rnn
 * @param[out] train_opt options loaded.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @param[in] param default values of training parameters.
 * @return non-zero value if any error.
 */
int rnn_load_train_opt(rnn_train_opt_t *train_opt, st_opt_t *opt,
        const char *sec_name, param_t *param);

/**
 * Initialise a rnn model.
 * @ingroup g_rnn
 * @param[out] prnn initialised rnn.
 * @param[in] model_opt model options used to initialise.
 * @param[in] output output layer for the model.
 * @return non-zero value if any error.
 */
int rnn_init(rnn_t **prnn, rnn_model_opt_t *model_opt, output_t *output);

/**
 * Destroy a RNN model and set the pointer to NULL.
 * @ingroup g_rnn
 * @param[in] ptr pointer to rnn_t.
 */
#define safe_rnn_destroy(ptr) do {\
    if((ptr) != NULL) {\
        rnn_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a rnn model.
 * @ingroup g_rnn
 * @param[in] rnn rnn to be destroyed.
 */
void rnn_destroy(rnn_t *rnn);
/**
 * Duplicate a rnn model.
 * @ingroup g_rnn
 * @param[in] r rnn to be duplicated.
 * @return the duplicated rnn model
 */
rnn_t* rnn_dup(rnn_t *r);

/**
 * Get the size for HS tree input.
 * @ingroup g_rnn
 * @param[in] rnn rnn model.
 * @return the size, zero or negtive value if any error.
 */
int rnn_get_hs_size(rnn_t *rnn);

/**
 * Load rnn header and initialise a new rnn.
 * @ingroup g_rnn
 * @param[out] rnn rnn initialised.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[out] binary whether the file stream is in binary format.
 * @param[in] fo_info file stream used to print information, if it is not NULL.
 * @see rnn_load_body
 * @see rnn_save_header, rnn_save_body
 * @return non-zero value if any error.
 */
int rnn_load_header(rnn_t **rnn, int version, FILE *fp,
        bool *binary, FILE *fo_info);
/**
 * Load rnn body.
 * @ingroup g_rnn
 * @param[in] rnn rnn to be loaded.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[in] binary whether to use binary format.
 * @see rnn_load_header
 * @see rnn_save_header, rnn_save_body
 * @return non-zero value if any error.
 */
int rnn_load_body(rnn_t *rnn, int version, FILE *fp, bool binary);
/**
 * Save rnn header.
 * @ingroup g_rnn
 * @param[in] rnn rnn to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see rnn_save_body
 * @see rnn_load_header, rnn_load_body
 * @return non-zero value if any error.
 */
int rnn_save_header(rnn_t *rnn, FILE *fp, bool binary);
/**
 * Save rnn body.
 * @ingroup g_rnn
 * @param[in] rnn rnn to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see rnn_save_header
 * @see rnn_load_header, rnn_load_body
 * @return non-zero value if any error.
 */
int rnn_save_body(rnn_t *rnn, FILE *fp, bool binary);

/**
 * Feed-forward one word for pre-non-output layer of one thread of rnn model.
 * @ingroup g_rnn
 * @param[in] rnn rnn model.
 * @param[in] tid thread id (neuron id).
 * @see rnn_forward_last_layer
 * @return non-zero value if any error.
 */
int rnn_forward_pre_layer(rnn_t *rnn, int tid);
/**
 * Feed-forward one word for last output layer of one thread of rnn model.
 * @ingroup g_rnn
 * @param[in] rnn rnn model.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @see rnn_forward_pre_layer
 * @return non-zero value if any error.
 */
int rnn_forward_last_layer(rnn_t *rnn, int word, int tid);
/**
 * Back-propagate one word for a thread of rnn model.
 * @ingroup g_rnn
 * @param[in] rnn rnn model.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @see rnn_forward
 * @return non-zero value if any error.
 */
int rnn_backprop(rnn_t *rnn, int word, int tid);

/**
 * Setup training for rnn model.
 * Called before training.
 * @ingroup g_rnn
 * @param[in] rnn rnn model.
 * @param[in] train_opt training options.
 * @param[in] output output layer.
 * @param[in] num_thrs number of thread to be used.
 * @return non-zero value if any error.
 */
int rnn_setup_train(rnn_t *rnn, rnn_train_opt_t *train_opt,
        output_t *output, int num_thrs);
/**
 * Reset training for rnn model.
 * Called before every input sentence to be trained.
 * @ingroup g_rnn
 * @param[in] rnn rnn model.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int rnn_reset_train(rnn_t *rnn, int tid);
/**
 * Start training for rnn model.
 * Called before every input word to be trained.
 * @ingroup g_rnn
 * @param[in] rnn rnn model.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int rnn_start_train(rnn_t *rnn, int word, int tid);
/**
 * Training between feed-forward and back-propagate for rnn model.
 * Called between forward and backprop during training a word.
 * @ingroup g_rnn
 * @param[in] rnn rnn model.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int rnn_fwd_bp(rnn_t *rnn, int word, int tid);
/**
 * End training for rnn model.
 * Called after every input word trained.
 * @ingroup g_rnn
 * @param[in] rnn rnn model.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int rnn_end_train(rnn_t *rnn, int word, int tid);
/**
 * Finish training for rnn model.
 * Called after all words trained.
 * @ingroup g_rnn
 * @param[in] rnn rnn model.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int rnn_finish_train(rnn_t *rnn, int tid);

/**
 * Setup testing for rnn model.
 * Called before testing.
 * @ingroup g_rnn
 * @param[in] rnn rnn model.
 * @param[in] output output layer.
 * @param[in] num_thrs number of thread to be used.
 * @return non-zero value if any error.
 */
int rnn_setup_test(rnn_t *rnn, output_t *output, int num_thrs);
/**
 * Reset testing for rnn model.
 * Called before every input sentence to be tested.
 * @ingroup g_rnn
 * @param[in] rnn rnn model.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int rnn_reset_test(rnn_t *rnn, int tid);
/**
 * Start testing for rnn model.
 * Called before every input word to be tested.
 * @ingroup g_rnn
 * @param[in] rnn rnn model.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int rnn_start_test(rnn_t *rnn, int word, int tid);
/**
 * End testing for rnn model.
 * Called after every input word tested.
 * @ingroup g_rnn
 * @param[in] rnn rnn model.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int rnn_end_test(rnn_t *rnn, int word, int tid);

/**
 * Setup generating for rnn model.
 * Called before generating.
 * @ingroup g_rnn
 * @param[in] rnn rnn model.
 * @param[in] output output layer.
 * @return non-zero value if any error.
 */
int rnn_setup_gen(rnn_t *rnn, output_t *output);
/**
 * Reset generating for rnn model.
 * Called before every sentence to be generated.
 * @ingroup g_rnn
 * @param[in] rnn rnn model.
 * @return non-zero value if any error.
 */
int rnn_reset_gen(rnn_t *rnn);
/**
 * End generating for rnn model.
 * Called after every word generated.
 * @ingroup g_rnn
 * @param[in] rnn rnn model.
 * @param[in] word current generated word.
 * @return non-zero value if any error.
 */
int rnn_end_gen(rnn_t *rnn, int word);

#ifdef __cplusplus
}
#endif

#endif
