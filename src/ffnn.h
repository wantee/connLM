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

#ifndef  _CONNLM_FFNN_H_
#define  _CONNLM_FFNN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <st_opt.h>

#include <connlm/config.h>
#include "param.h"
#include "output.h"

/** @defgroup g_ffnn FFNN Model
 * Data structures and functions for FFNN model.
 */

/**
 * Parameters for FFNN model.
 * @ingroup g_ffnn
 */
typedef struct _ffnn_model_opt_t_ {
    real_t scale;     /**< output scale. */
} ffnn_model_opt_t;

/**
 * Parameters for training FFNN model.
 * @ingroup g_ffnn
 *
 */
typedef struct _ffnn_train_opt_t_ {
    param_t param;     /**< training parameters. */
} ffnn_train_opt_t;

/**
 * Neuron of FFNN model.
 * Each neuron used in one single thread.
 * @ingroup g_ffnn
 */
typedef struct _ffnn_neuron_t_ {
} ffnn_neuron_t;

/**
 * FFNN model.
 * @ingroup g_ffnn
 */
typedef struct _ffnn_t_ {
    ffnn_model_opt_t model_opt; /**< option for this model. */
    output_t *output; /**< output layer of this model. */

    ffnn_train_opt_t train_opt; /**< training option. */
    ffnn_neuron_t *neurons; /**< neurons of the model. An array of size num_thrs. */
    int num_thrs; /**< number of threads/neurons. */
} ffnn_t;

/**
 * Load ffnn model option.
 * @ingroup g_ffnn
 * @param[out] model_opt options loaded.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @return non-zero value if any error.
 */
int ffnn_load_model_opt(ffnn_model_opt_t *model_opt, st_opt_t *opt,
        const char *sec_name);
/**
 * Load ffnn train option.
 * @ingroup g_ffnn
 * @param[out] train_opt options loaded.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @param[in] param default values of training parameters.
 * @return non-zero value if any error.
 */
int ffnn_load_train_opt(ffnn_train_opt_t *train_opt, st_opt_t *opt,
        const char *sec_name, param_t *param);

/**
 * Initialise a ffnn model.
 * @ingroup g_ffnn
 * @param[out] pffnn initialised ffnn.
 * @param[in] model_opt model options used to initialise.
 * @param[in] output output layer for the model.
 * @return non-zero value if any error.
 */
int ffnn_init(ffnn_t **pffnn, ffnn_model_opt_t *model_opt,
        output_t *output);
/**
 * Destroy a FFNN model and set the pointer to NULL.
 * @ingroup g_ffnn
 * @param[in] ptr pointer to ffnn_t.
 */
#define safe_ffnn_destroy(ptr) do {\
    if((ptr) != NULL) {\
        ffnn_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a ffnn model.
 * @ingroup g_ffnn
 * @param[in] ffnn ffnn to be destroyed.
 */
void ffnn_destroy(ffnn_t *ffnn);
/**
 * Duplicate a ffnn model.
 * @ingroup g_ffnn
 * @param[in] f ffnn to be duplicated.
 * @return the duplicated ffnn model
 */
ffnn_t* ffnn_dup(ffnn_t *f);

/**
 * Get the size for HS tree input.
 * @ingroup g_ffnn
 * @param[in] ffnn ffnn model.
 * @return the size, zero or negtive value if any error.
 */
int ffnn_get_hs_size(ffnn_t *ffnn);

/**
 * Load ffnn header and initialise a new ffnn.
 * @ingroup g_ffnn
 * @param[out] ffnn ffnn initialised.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[out] binary whether the file stream is in binary format.
 * @param[in] fo_info file stream used to print information, if it is not NULL.
 * @see ffnn_load_body
 * @see ffnn_save_header, ffnn_save_body
 * @return non-zero value if any error.
 */
int ffnn_load_header(ffnn_t **ffnn, int version, FILE *fp,
        bool *binary, FILE *fo_info);
/**
 * Load ffnn body.
 * @ingroup g_ffnn
 * @param[in] ffnn ffnn to be loaded.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[in] binary whether to use binary format.
 * @see ffnn_load_header
 * @see ffnn_save_header, ffnn_save_body
 * @return non-zero value if any error.
 */
int ffnn_load_body(ffnn_t *ffnn, int version, FILE *fp, bool binary);
/**
 * Save ffnn header.
 * @ingroup g_ffnn
 * @param[in] ffnn ffnn to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see ffnn_save_body
 * @see ffnn_load_header, ffnn_load_body
 * @return non-zero value if any error.
 */
int ffnn_save_header(ffnn_t *ffnn, FILE *fp, bool binary);
/**
 * Save ffnn body.
 * @ingroup g_ffnn
 * @param[in] ffnn ffnn to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see ffnn_save_header
 * @see ffnn_load_header, ffnn_load_body
 * @return non-zero value if any error.
 */
int ffnn_save_body(ffnn_t *ffnn, FILE *fp, bool binary);

/**
 * Feed-forward one word for pre-non-output layer of one thread of ffnn model.
 * @ingroup g_ffnn
 * @param[in] ffnn ffnn model.
 * @param[in] tid thread id (neuron id).
 * @see ffnn_forward_last_layer
 * @return non-zero value if any error.
 */
int ffnn_forward_pre_layer(ffnn_t *ffnn, int tid);
/**
 * Feed-forward one word for last output layer of one thread of ffnn model.
 * @ingroup g_ffnn
 * @param[in] ffnn ffnn model.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @see ffnn_forward_pre_layer
 * @return non-zero value if any error.
 */
int ffnn_forward_last_layer(ffnn_t *ffnn, int word, int tid);
/**
 * Back-propagate one word for a thread of ffnn model.
 * @ingroup g_ffnn
 * @param[in] ffnn ffnn model.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @see ffnn_forward
 * @return non-zero value if any error.
 */
int ffnn_backprop(ffnn_t *ffnn, int word, int tid);

/**
 * Setup training for ffnn model.
 * Called before training.
 * @ingroup g_ffnn
 * @param[in] ffnn ffnn model.
 * @param[in] train_opt training options.
 * @param[in] output output layer.
 * @param[in] num_thrs number of thread to be used.
 * @return non-zero value if any error.
 */
int ffnn_setup_train(ffnn_t *ffnn, ffnn_train_opt_t *train_opt,
        output_t *output, int num_thrs);
/**
 * Reset training for ffnn model.
 * Called before every input sentence to be trained.
 * @ingroup g_ffnn
 * @param[in] ffnn ffnn model.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int ffnn_reset_train(ffnn_t *ffnn, int tid);
/**
 * Start training for ffnn model.
 * Called before every input word to be trained.
 * @ingroup g_ffnn
 * @param[in] ffnn ffnn model.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int ffnn_start_train(ffnn_t *ffnn, int word, int tid);
/**
 * End training for ffnn model.
 * Called after every input word trained.
 * @ingroup g_ffnn
 * @param[in] ffnn ffnn model.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int ffnn_end_train(ffnn_t *ffnn, int word, int tid);
/**
 * Finish training for ffnn model.
 * Called after all words trained.
 * @ingroup g_ffnn
 * @param[in] ffnn ffnn model.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int ffnn_finish_train(ffnn_t *ffnn, int tid);

/**
 * Setup testing for ffnn model.
 * Called before testing.
 * @ingroup g_ffnn
 * @param[in] ffnn ffnn model.
 * @param[in] output output layer.
 * @param[in] num_thrs number of thread to be used.
 * @return non-zero value if any error.
 */
int ffnn_setup_test(ffnn_t *ffnn, output_t *output, int num_thrs);
/**
 * Reset testing for ffnn model.
 * Called before every input sentence to be tested.
 * @ingroup g_ffnn
 * @param[in] ffnn ffnn model.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int ffnn_reset_test(ffnn_t *ffnn, int tid);
/**
 * Start testing for ffnn model.
 * Called before every input word to be tested.
 * @ingroup g_ffnn
 * @param[in] ffnn ffnn model.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int ffnn_start_test(ffnn_t *ffnn, int word, int tid);
/**
 * End testing for ffnn model.
 * Called after every input word tested.
 * @ingroup g_ffnn
 * @param[in] ffnn ffnn model.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int ffnn_end_test(ffnn_t *ffnn, int word, int tid);

/**
 * Setup generating for ffnn model.
 * Called before generating.
 * @ingroup g_ffnn
 * @param[in] ffnn ffnn model.
 * @param[in] output output layer.
 * @return non-zero value if any error.
 */
int ffnn_setup_gen(ffnn_t *ffnn, output_t *output);
/**
 * Reset generating for ffnn model.
 * Called before every sentence to be generated.
 * @ingroup g_ffnn
 * @param[in] ffnn ffnn model.
 * @return non-zero value if any error.
 */
int ffnn_reset_gen(ffnn_t *ffnn);
/**
 * End generating for ffnn model.
 * Called after every word generated.
 * @ingroup g_ffnn
 * @param[in] ffnn ffnn model.
 * @param[in] word current generated word.
 * @return non-zero value if any error.
 */
int ffnn_end_gen(ffnn_t *ffnn, int word);

#ifdef __cplusplus
}
#endif

#endif
