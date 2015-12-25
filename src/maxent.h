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

#ifndef  _CONNLM_MAXENT_H_
#define  _CONNLM_MAXENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <st_opt.h>

#include <connlm/config.h>
#include "param.h"
#include "output.h"

/** @defgroup g_maxent MaxEnt Model
 * Data structures and functions for MaxEnt model.
 */

/**
 * Parameters for MaxEnt model.
 * @ingroup g_maxent
 */
typedef struct _maxent_model_opt_t_ {
    real_t scale;      /**< output scale. */

    hash_size_t sz_w;  /**< size of hash for words. */
    hash_size_t sz_c;  /**< size of hash for classes. */
    int order;         /**< order of model. */
} maxent_model_opt_t;

/**
 * Parameters for training MaxEnt model.
 * @ingroup g_maxent
 *
 */
typedef struct _maxent_train_opt_t_ {
    param_t param;     /**< training parameters. */
} maxent_train_opt_t;

/**
 * An interval for a hash array.
 * Used to get union of sets of hashes.
 * @ingroup g_maxent
 */
typedef struct _hash_range_t_ {
    hash_t s;         /**< start position. */
    union {
        hash_t e;     /**< end position. */
        hash_t num;   /**< interval length. */
    };
} hash_range_t;

/**
 * Histories of hashes.
 * Used for mini-batch updates.
 * @ingroup g_maxent
 */
typedef struct _hash_hist_t_ {
    int order;           /**< order of histories. */
    hash_range_t *range; /**< range of every order. */
} hash_hist_t;

/**
 * Neuron of MaxEnt model.
 * Each neuron used in one single thread.
 * @ingroup g_maxent
 */
typedef struct _maxent_neuron_t_ {
    param_arg_t param_arg_c;  /**< parameter arguments for classes. */
    param_arg_t param_arg_w;  /**< parameter arguments for words. */

    int *hist;                /**< word histories of n-gram. */

    hash_t *hash_c;           /**< position of class hashes
                                   actived by a n-gram history. */
    hash_t *hash_w;           /**< position of word hashes
                                   actived by a n-gram history. */
#ifndef _MAXENT_BP_CALC_HASH_
    int hash_order_c;         /**< order of class hashes history. */
    int hash_order_w;         /**< order of word hashes history. */
#endif

    /** @name Variables for mini-batch update. */
    /**@{*/
    int mini_step;            /**< steps of mini-batch. */
    hash_range_t *hash_union; /**< union of hashes for one mini-batch. */
    hash_hist_t *hash_hist_c; /**< history of hashes for class in one mini-batch. */
    int hash_hist_c_num;      /**< number of histories for class in one mini-batch. */
    hash_hist_t *hash_hist_w; /**< history of hashes for word in one mini-batch. */
    int hash_hist_w_num;      /**< number of histories for word in one mini-batch. */
    real_t *wt_c; /**< delta weight of class for one mini-batch */
    real_t *wt_w; /**< delta weight of word for one mini-batch */
    /**@}*/
} maxent_neuron_t;

/**
 * MaxEnt model.
 * @ingroup g_maxent
 */
typedef struct _maxent_t_ {
    maxent_model_opt_t model_opt; /**< option for this model. */

    int vocab_size; /**< size of vocab. */
    int class_size; /**< size of class. No class if class_size less or equal to 0 */

    real_t *wt_c;  /**< weight for classes. */
    real_t *wt_w;  /**< weight for words. */

    output_t *output; /**< output layer of this model. */

    maxent_train_opt_t train_opt; /**< training option. */
    maxent_neuron_t *neurons; /**< neurons of the model. An array of size num_thrs. */
    int num_thrs; /**< number of threads/neurons. */
} maxent_t;

/**
 * Load maxent model option.
 * @ingroup g_maxent
 * @param[out] model_opt options loaded.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @return non-zero value if any error.
 */
int maxent_load_model_opt(maxent_model_opt_t *model_opt,
        st_opt_t *opt, const char *sec_name);

/**
 * Load maxent train option.
 * @ingroup g_maxent
 * @param[out] train_opt options loaded.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @param[in] param default values of training parameters.
 * @return non-zero value if any error.
 */
int maxent_load_train_opt(maxent_train_opt_t *train_opt,
        st_opt_t *opt, const char *sec_name, param_t *param);

/**
 * Initialise a maxent model.
 * @ingroup g_maxent
 * @param[out] pmaxent initialised maxent.
 * @param[in] model_opt model options used to initialise.
 * @param[in] output output layer for the model.
 * @return non-zero value if any error.
 */
int maxent_init(maxent_t **pmaxent, maxent_model_opt_t *model_opt,
        output_t *output);

/**
 * Destroy a MaxEnt model and set the pointer to NULL.
 * @ingroup g_maxent
 * @param[in] ptr pointer to maxent_t.
 */
#define safe_maxent_destroy(ptr) do {\
    if((ptr) != NULL) {\
        maxent_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a maxent model.
 * @ingroup g_maxent
 * @param[in] maxent maxent to be destroyed.
 */
void maxent_destroy(maxent_t *maxent);

/**
 * Duplicate a maxent model.
 * @ingroup g_maxent
 * @param[in] m maxent to be duplicated.
 * @return the duplicated maxent model
 */
maxent_t* maxent_dup(maxent_t *m);

/**
 * Get the size for HS tree input.
 * @ingroup g_maxent
 * @param[in] maxent maxent model.
 * @return the size, zero or negtive value if any error.
 */
int maxent_get_hs_size(maxent_t *maxent);

/**
 * Load maxent header and initialise a new maxent.
 * @ingroup g_maxent
 * @param[out] maxent maxent initialised.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[out] binary whether the file stream is in binary format.
 * @param[in] fo_info file stream used to print information, if it is not NULL.
 * @see maxent_load_body
 * @see maxent_save_header, maxent_save_body
 * @return non-zero value if any error.
 */
int maxent_load_header(maxent_t **maxent, int version, FILE *fp,
        bool *binary, FILE *fo_info);
/**
 * Load maxent body.
 * @ingroup g_maxent
 * @param[in] maxent maxent to be loaded.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[in] binary whether to use binary format.
 * @see maxent_load_header
 * @see maxent_save_header, maxent_save_body
 * @return non-zero value if any error.
 */
int maxent_load_body(maxent_t *maxent, int version, FILE *fp, bool binary);
/**
 * Save maxent header.
 * @ingroup g_maxent
 * @param[in] maxent maxent to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see maxent_save_body
 * @see maxent_load_header, maxent_load_body
 * @return non-zero value if any error.
 */
int maxent_save_header(maxent_t *maxent, FILE *fp, bool binary);
/**
 * Save maxent body.
 * @ingroup g_maxent
 * @param[in] maxent maxent to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see maxent_save_header
 * @see maxent_load_header, maxent_load_body
 * @return non-zero value if any error.
 */
int maxent_save_body(maxent_t *maxent, FILE *fp, bool binary);

/**
 * Feed-forward one word for pre-non-output layer of one thread of maxent model.
 * @ingroup g_maxent
 * @param[in] maxent maxent model.
 * @param[in] tid thread id (neuron id).
 * @see maxent_forward_last_layer
 * @return non-zero value if any error.
 */
int maxent_forward_pre_layer(maxent_t *maxent, int tid);
/**
 * Feed-forward one word for last output layer of one thread of maxent model.
 * @ingroup g_maxent
 * @param[in] maxent maxent model.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @see maxent_forward_pre_layer
 * @return non-zero value if any error.
 */
int maxent_forward_last_layer(maxent_t *maxent, int word, int tid);
/**
 * Back-propagate one word for a thread of maxent model.
 * @ingroup g_maxent
 * @param[in] maxent maxent model.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @see maxent_forward
 * @return non-zero value if any error.
 */
int maxent_backprop(maxent_t *maxent, int word, int tid);

/**
 * Setup training for maxent model.
 * Called before training.
 * @ingroup g_maxent
 * @param[in] maxent maxent model.
 * @param[in] train_opt training options.
 * @param[in] output output layer.
 * @param[in] num_thrs number of thread to be used.
 * @return non-zero value if any error.
 */
int maxent_setup_train(maxent_t *maxent, maxent_train_opt_t *train_opt,
        output_t *output, int num_thrs);
/**
 * Reset training for maxent model.
 * Called before every input sentence to be trained.
 * @ingroup g_maxent
 * @param[in] maxent maxent model.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int maxent_reset_train(maxent_t *maxent, int tid);
/**
 * Start training for maxent model.
 * Called before every input word to be trained.
 * @ingroup g_maxent
 * @param[in] maxent maxent model.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int maxent_start_train(maxent_t *maxent, int word, int tid);
/**
 * End training for maxent model.
 * Called after every input word trained.
 * @ingroup g_maxent
 * @param[in] maxent maxent model.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int maxent_end_train(maxent_t *maxent, int word, int tid);
/**
 * Finish training for maxent model.
 * Called after all words trained.
 * @ingroup g_maxent
 * @param[in] maxent maxent model.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int maxent_finish_train(maxent_t *maxent, int tid);

/**
 * Setup testing for maxent model.
 * Called before testing.
 * @ingroup g_maxent
 * @param[in] maxent maxent model.
 * @param[in] output output layer.
 * @param[in] num_thrs number of thread to be used.
 * @return non-zero value if any error.
 */
int maxent_setup_test(maxent_t *maxent, output_t *output, int num_thrs);
/**
 * Reset testing for maxent model.
 * Called before every input sentence to be tested.
 * @ingroup g_maxent
 * @param[in] maxent maxent model.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int maxent_reset_test(maxent_t *maxent, int tid);
/**
 * Start testing for maxent model.
 * Called before every input word to be tested.
 * @ingroup g_maxent
 * @param[in] maxent maxent model.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int maxent_start_test(maxent_t *maxent, int word, int tid);
/**
 * End testing for maxent model.
 * Called after every input word tested.
 * @ingroup g_maxent
 * @param[in] maxent maxent model.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int maxent_end_test(maxent_t *maxent, int word, int tid);

/**
 * Setup generating for maxent model.
 * Called before generating.
 * @ingroup g_maxent
 * @param[in] maxent maxent model.
 * @param[in] output output layer.
 * @return non-zero value if any error.
 */
int maxent_setup_gen(maxent_t *maxent, output_t *output);
/**
 * Reset generating for maxent model.
 * Called before every sentence to be generated.
 * @ingroup g_maxent
 * @param[in] maxent maxent model.
 * @return non-zero value if any error.
 */
int maxent_reset_gen(maxent_t *maxent);
/**
 * End generating for maxent model.
 * Called after every word generated.
 * @ingroup g_maxent
 * @param[in] maxent maxent model.
 * @param[in] word current generated word.
 * @return non-zero value if any error.
 */
int maxent_end_gen(maxent_t *maxent, int word);

#ifdef __cplusplus
}
#endif

#endif
