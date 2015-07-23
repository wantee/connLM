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

#ifndef  _OUTPUT_H_
#define  _OUTPUT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <st_opt.h>
#include <st_alphabet.h>

#include "config.h"

/** @defgroup output Output Layer
 * Data structures and functions for Output Layer.
 */

/**
 * Parameters for output layer.
 * @ingroup output
 */
typedef struct _output_opt_t_ {
    int class_size; /**< size of class. May be zero */
    bool hs;        /**< whether using Hierarchical softmax */
} output_opt_t;

/**
 * Neuron of Output Layer.
 * Each neuron used in one single thread.
 * @ingroup output
 */
typedef struct _output_neuron_t_ {
    real_t *ac_o_w; /**< activation of output of word part. */
    real_t *er_o_w; /**< error of output of word part. */
    real_t *ac_o_c; /**< activation of output of class part. */
    real_t *er_o_c; /**< error of output of class part. */
} output_neuron_t;

/**
 * Output Layer.
 * @ingroup output
 */
typedef struct _output_t_ {
    output_opt_t output_opt; /**< output layer options. */

    int output_size; /**< size of output layer. */

    output_neuron_t *neurons; /**< output layer neurons. */
    int num_thrs; /**< number of threads/neurons. */

    /** @name Variables for classes. */
    /**@{*/
    int *w2c;   /**< word to class map. */
    int *c2w_s; /**< start for class to word map. */
    int *c2w_e; /**< end for class to word map. */
    /**@}*/
} output_t;

/**
 * Load output layer option.
 * @ingroup output
 * @param[out] output_opt options loaded.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @return non-zero value if any error.
 */
int output_load_opt(output_opt_t *output_opt, st_opt_t *opt,
        const char *sec_name);

/**
 * Create a output layer with options.
 * @ingroup output
 * @param[in] output_opt options.
 * @param[in] output_size size of output layer.
 * @return a new output layer.
 */
output_t *output_create(output_opt_t *output_opt, int output_size);
/**
 * Destroy a output layer and set the pointer to NULL.
 * @ingroup output
 * @param[in] ptr pointer to output_t.
 */
#define safe_output_destroy(ptr) do {\
    if((ptr) != NULL) {\
        output_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a output layer.
 * @ingroup output
 * @param[in] output output to be destroyed.
 */
void output_destroy(output_t *output);
/**
 * Duplicate a output layer.
 * @ingroup output
 * @param[in] o output to be duplicated.
 * @return the duplicated output.
 */
output_t* output_dup(output_t *o);

/**
 * Load output layer header and initialise a new output layer.
 * @ingroup output
 * @param[out] output layer initialised.
 * @param[in] fp file stream loaded from.
 * @param[out] binary whether the file stream is in binary format.
 * @param[in] fo file stream used to print information, if it is not NULL.
 * @see output_load_body
 * @see output_save_header, output_save_body
 * @return non-zero value if any error.
 */
int output_load_header(output_t **output, FILE *fp,
        bool *binary, FILE *fo);
/**
 * Load output layer body.
 * @ingroup output
 * @param[in] output output layer to be loaded.
 * @param[in] fp file stream loaded from.
 * @param[in] binary whether to use binary format.
 * @see output_load_header
 * @see output_save_header, output_save_body
 * @return non-zero value if any error.
 */
int output_load_body(output_t *output, FILE *fp, bool binary);
/**
 * Save output layer header.
 * @ingroup output
 * @param[in] output output layer to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see output_save_body
 * @see output_load_header, output_load_body
 * @return non-zero value if any error.
 */
int output_save_header(output_t *output, FILE *fp, bool binary);
/**
 * Save output layer body.
 * @ingroup output
 * @param[in] output output layer to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see output_save_header
 * @see output_load_header, output_load_body
 * @return non-zero value if any error.
 */
int output_save_body(output_t *output, FILE *fp, bool binary);

/**
 * Generate output layer with word counts.
 * @ingroup output
 * @param[in] output output layer to be generated.
 * @param[in] word_cnts counts of words.
 * @return non-zero value if any error.
 */
int output_generate(output_t *output, count_t *word_cnts);

/**
 * Activate neurons of pre output layer.
 * @ingroup output
 * @param[in] output output layer related.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int output_activate_pre_layer(output_t *output, int tid);
/**
 * Activate neurons of last output layer.
 * @ingroup output
 * @param[in] output output layer related.
 * @param[in] cls class of current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int output_activate_last_layer(output_t *output, int cls, int tid);
/**
 * Compute loss of neurons of output layer.
 * @ingroup output
 * @param[in] output output layer related.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int output_loss(output_t *output, int word, int tid);
/**
 * Get probability of a word.
 * @ingroup output
 * @param[in] output output layer related.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
double output_get_prob(output_t *output, int word, int tid);
/**
 * Get class probability of a class.
 * @ingroup output
 * @param[in] output output layer related.
 * @param[in] cls the specific class.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
double output_get_class_prob_for_class(output_t *output, int cls, int tid);
/**
 * Get class probability of a word.
 * @ingroup output
 * @param[in] output output layer related.
 * @param[in] word the specific word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
double output_get_class_prob(output_t *output, int word, int tid);
/**
 * Get word probability of a word.
 * @ingroup output
 * @param[in] output output layer related.
 * @param[in] word the specific word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
double output_get_word_prob(output_t *output, int word, int tid);

/**
 * Setup training for output layer.
 * Called before training.
 * @ingroup output
 * @param[in] output output layer.
 * @param[in] num_thrs number of thread to be used.
 * @return non-zero value if any error.
 */
int output_setup_train(output_t *output, int num_thrs);
/**
 * Reset training for output layer.
 * Called before every input sentence to be trained.
 * @ingroup output
 * @param[in] output output model.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int output_reset_train(output_t *output, int tid);
/**
 * Start training for output layer.
 * Called before every input word to be trained.
 * @ingroup output
 * @param[in] output output layer.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int output_start_train(output_t *output, int word, int tid);
/**
 * End training for output layer.
 * Called after every input word trained.
 * @ingroup output
 * @param[in] output output layer.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int output_end_train(output_t *output, int word, int tid);
/**
 * Finish training for output model.
 * Called after all words trained.
 * @ingroup output
 * @param[in] output output layer.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int output_finish_train(output_t *output, int tid);

/**
 * Setup testing for output layer.
 * Called before testing.
 * @ingroup output
 * @param[in] output output layer.
 * @param[in] num_thrs number of thread to be used.
 * @return non-zero value if any error.
 */
int output_setup_test(output_t *output, int num_thrs);
/**
 * Reset testing for output layer.
 * Called before every input sentence to be tested.
 * @ingroup output
 * @param[in] output output layer.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int output_reset_test(output_t *output, int tid);
/**
 * Start testing for output layer.
 * Called before every input word to be tested.
 * @ingroup output
 * @param[in] output output layer.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int output_start_test(output_t *output, int word, int tid);
/**
 * End testing for output layer.
 * Called after every input word tested.
 * @ingroup output
 * @param[in] output output layer.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int output_end_test(output_t *output, int word, int tid);

/**
 * Setup generating for output layer.
 * Called before generating.
 * @ingroup output
 * @param[in] output output layer.
 * @return non-zero value if any error.
 */
int output_setup_gen(output_t *output);
/**
 * Reset generating for output layer.
 * Called before every sentence to be generated.
 * @ingroup output
 * @param[in] output output layer.
 * @return non-zero value if any error.
 */
int output_reset_gen(output_t *output);
/**
 * End generating for output layer.
 * Called after every word generated.
 * @ingroup output
 * @param[in] output output layer.
 * @param[in] word current generated word.
 * @return non-zero value if any error.
 */
int output_end_gen(output_t *output, int word);

#ifdef __cplusplus
}
#endif

#endif
