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

#ifndef  _CONNLM_LBL_H_
#define  _CONNLM_LBL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stutils/st_opt.h>

#include <connlm/config.h>
#include "param.h"
#include "output.h"

/** @defgroup g_lbl LBL Model
 * Data structures and functions for LBL model.
 */

/**
 * Parameters for LBL model.
 * @ingroup g_lbl
 */
typedef struct _lbl_model_opt_t_ {
    real_t scale;     /**< output scale. */
} lbl_model_opt_t;

/**
 * Parameters for training LBL model.
 * @ingroup g_lbl
 *
 */
typedef struct _lbl_train_opt_t_ {
    param_t param;     /**< training parameters. */
} lbl_train_opt_t;

/**
 * Neuron of LBL model.
 * Each neuron used in one single thread.
 * @ingroup g_lbl
 */
typedef struct _lbl_neuron_t_ {
} lbl_neuron_t;

/**
 * LBL model.
 * @ingroup g_lbl
 */
typedef struct _lbl_t_ {
    lbl_model_opt_t model_opt; /**< option for this model. */
    output_t *output; /**< output layer of this model. */

    lbl_train_opt_t train_opt; /**< training option. */
    lbl_neuron_t *neurons; /**< neurons of the model. An array of size num_thrs. */
    int num_thrs; /**< number of threads/neurons. */
} lbl_t;

/**
 * Load lbl model option.
 * @ingroup g_lbl
 * @param[out] model_opt options loaded.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @return non-zero value if any error.
 */
int lbl_load_model_opt(lbl_model_opt_t *model_opt, st_opt_t *opt,
        const char *sec_name);
/**
 * Load lbl train option.
 * @ingroup g_lbl
 * @param[out] train_opt options loaded.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @param[in] param default values of training parameters.
 * @return non-zero value if any error.
 */
int lbl_load_train_opt(lbl_train_opt_t *train_opt, st_opt_t *opt,
        const char *sec_name, param_t *param);

/**
 * Initialise a lbl model.
 * @ingroup g_lbl
 * @param[out] plbl initialised lbl.
 * @param[in] model_opt model options used to initialise.
 * @param[in] output output layer for the model.
 * @return non-zero value if any error.
 */
int lbl_init(lbl_t **plbl, lbl_model_opt_t *model_opt, output_t *output);
/**
 * Destroy a LBL model and set the pointer to NULL.
 * @ingroup g_lbl
 * @param[in] ptr pointer to lbl_t.
 */
#define safe_lbl_destroy(ptr) do {\
    if((ptr) != NULL) {\
        lbl_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a lbl model.
 * @ingroup g_lbl
 * @param[in] lbl lbl to be destroyed.
 */
void lbl_destroy(lbl_t *lbl);
/**
 * Duplicate a lbl model.
 * @ingroup g_lbl
 * @param[in] l lbl to be duplicated.
 * @return the duplicated lbl model
 */
lbl_t* lbl_dup(lbl_t *l);

/**
 * Get the size for HS tree input.
 * @ingroup g_lbl
 * @param[in] lbl lbl model.
 * @return the size, zero or negtive value if any error.
 */
int lbl_get_hs_size(lbl_t *lbl);

/**
 * Load lbl header and initialise a new lbl.
 * @ingroup g_lbl
 * @param[out] lbl lbl initialised.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[out] binary whether the file stream is in binary format.
 * @param[in] fo_info file stream used to print information, if it is not NULL.
 * @see lbl_load_body
 * @see lbl_save_header, lbl_save_body
 * @return non-zero value if any error.
 */
int lbl_load_header(lbl_t **lbl, int version, FILE *fp,
        bool *binary, FILE *fo_info);
/**
 * Load lbl body.
 * @ingroup g_lbl
 * @param[in] lbl lbl to be loaded.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[in] binary whether to use binary format.
 * @see lbl_load_header
 * @see lbl_save_header, lbl_save_body
 * @return non-zero value if any error.
 */
int lbl_load_body(lbl_t *lbl, int version, FILE *fp, bool binary);
/**
 * Save lbl header.
 * @ingroup g_lbl
 * @param[in] lbl lbl to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see lbl_save_body
 * @see lbl_load_header, lbl_load_body
 * @return non-zero value if any error.
 */
int lbl_save_header(lbl_t *lbl, FILE *fp, bool binary);
/**
 * Save lbl body.
 * @ingroup g_lbl
 * @param[in] lbl lbl to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see lbl_save_header
 * @see lbl_load_header, lbl_load_body
 * @return non-zero value if any error.
 */
int lbl_save_body(lbl_t *lbl, FILE *fp, bool binary);

/**
 * Feed-forward one word for pre-non-output layer of one thread of lbl model.
 * @ingroup g_lbl
 * @param[in] lbl lbl model.
 * @param[in] tid thread id (neuron id).
 * @see lbl_forward_last_layer
 * @return non-zero value if any error.
 */
int lbl_forward_pre_layer(lbl_t *lbl, int tid);
/**
 * Feed-forward one word for last output layer of one thread of lbl model.
 * @ingroup g_lbl
 * @param[in] lbl lbl model.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @see lbl_forward_pre_layer
 * @return non-zero value if any error.
 */
int lbl_forward_last_layer(lbl_t *lbl, int word, int tid);
/**
 * Back-propagate one word for a thread of lbl model.
 * @ingroup g_lbl
 * @param[in] lbl lbl model.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @see lbl_forward
 * @return non-zero value if any error.
 */
int lbl_backprop(lbl_t *lbl, int word, int tid);

/**
 * Setup training for lbl model.
 * Called before training.
 * @ingroup g_lbl
 * @param[in] lbl lbl model.
 * @param[in] train_opt training options.
 * @param[in] output output layer.
 * @param[in] num_thrs number of thread to be used.
 * @return non-zero value if any error.
 */
int lbl_setup_train(lbl_t *lbl, lbl_train_opt_t *train_opt,
        output_t *output, int num_thrs);
/**
 * Reset training for lbl model.
 * Called before every input sentence to be trained.
 * @ingroup g_lbl
 * @param[in] lbl lbl model.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int lbl_reset_train(lbl_t *lbl, int tid);
/**
 * Start training for lbl model.
 * Called before every input word to be trained.
 * @ingroup g_lbl
 * @param[in] lbl lbl model.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int lbl_start_train(lbl_t *lbl, int word, int tid);
/**
 * End training for lbl model.
 * Called after every input word trained.
 * @ingroup g_lbl
 * @param[in] lbl lbl model.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int lbl_end_train(lbl_t *lbl, int word, int tid);
/**
 * Finish training for lbl model.
 * Called after all words trained.
 * @ingroup g_lbl
 * @param[in] lbl lbl model.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int lbl_finish_train(lbl_t *lbl, int tid);

/**
 * Setup testing for lbl model.
 * Called before testing.
 * @ingroup g_lbl
 * @param[in] lbl lbl model.
 * @param[in] output output layer.
 * @param[in] num_thrs number of thread to be used.
 * @return non-zero value if any error.
 */
int lbl_setup_test(lbl_t *lbl, output_t *output, int num_thrs);
/**
 * Reset testing for lbl model.
 * Called before every input sentence to be tested.
 * @ingroup g_lbl
 * @param[in] lbl lbl model.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int lbl_reset_test(lbl_t *lbl, int tid);
/**
 * Start testing for lbl model.
 * Called before every input word to be tested.
 * @ingroup g_lbl
 * @param[in] lbl lbl model.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int lbl_start_test(lbl_t *lbl, int word, int tid);
/**
 * End testing for lbl model.
 * Called after every input word tested.
 * @ingroup g_lbl
 * @param[in] lbl lbl model.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int lbl_end_test(lbl_t *lbl, int word, int tid);

/**
 * Setup generating for lbl model.
 * Called before generating.
 * @ingroup g_lbl
 * @param[in] lbl lbl model.
 * @param[in] output output layer.
 * @return non-zero value if any error.
 */
int lbl_setup_gen(lbl_t *lbl, output_t *output);
/**
 * Reset generating for lbl model.
 * Called before every sentence to be generated.
 * @ingroup g_lbl
 * @param[in] lbl lbl model.
 * @return non-zero value if any error.
 */
int lbl_reset_gen(lbl_t *lbl);
/**
 * End generating for lbl model.
 * Called after every word generated.
 * @ingroup g_lbl
 * @param[in] lbl lbl model.
 * @param[in] word current generated word.
 * @return non-zero value if any error.
 */
int lbl_end_gen(lbl_t *lbl, int word);

#ifdef __cplusplus
}
#endif

#endif
