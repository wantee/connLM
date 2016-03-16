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

#ifndef  _CONNLM_OUTPUT_H_
#define  _CONNLM_OUTPUT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stutils/st_opt.h>

#include <connlm/config.h>

/** @defgroup g_output Output Layer
 * Data structures and functions for Output Layer.
 */

typedef unsigned int output_node_id_t;

#define OUTPUT_NODE_NONE ((output_node_id_t)-1)

/**
 * Type of constructing methods.
 * @ingroup g_output
 */
typedef enum _output_construct_method_t_ {
    TOP_DOWN, /**< Top-down method. */
    BOTTOM_UP, /**< Bottom-up method. */
} output_method_t;

/**
 * Parameters for output tree.
 * @ingroup g_output
 */
typedef struct _output_opt_t_ {
    output_method_t method; /**< constructing method. */
    int max_depth; /**< maximum depth of tree. */
    int max_branch; /**< maximum number of branches. */
} output_opt_t;

/**
 * Neuron of Output Layer.
 * Each neuron used in one single thread.
 * @ingroup g_output
 */
typedef struct _output_neuron_t_ {
    real_t *ac_o_w; /**< activation of output of word part. */
    real_t *er_o_w; /**< error of output of word part. */
    real_t *ac_o_c; /**< activation of output of class part. */
    real_t *er_o_c; /**< error of output of class part. */

    real_t *wt_hs_c; /**< HS weights for classes. */
    real_t *wt_hs_w; /**< HS weights for words. */

    real_t *ac_hs_c; /**< HS activiation for classes. */
    real_t *ac_hs_w; /**< HS activiation for words. */

    real_t p_hs_c; /**< Probility for class of HS. */
    real_t p_hs_w; /**< Probility for word of HS. */
} output_neuron_t;

/**
 * Output tree Node.
 * @ingroup g_output
 */
typedef struct _output_tree_node_t_ {
    output_node_id_t children_s; /**< start index of children. */
    output_node_id_t children_e; /**< end index of children. */
} output_tree_node_t;

/**
 * Output tree.
 * @ingroup g_output
 */
typedef struct _output_tree_t_ {
    output_tree_node_t *nodes; /**< nodes on output tree. */
    output_node_id_t num_nodes; /**< number of nodes on output tree. */
    output_node_id_t cap_nodes; /**< capacity of nodes array. */

    output_node_id_t root; /**< root node of output tree. */
} output_tree_t;

/**
 * Output tree Path.
 * @ingroup g_output
 */
typedef struct _output_tree_path_t_ {
    output_node_id_t *nodes; /**< nodes on the path exclude root. */
    output_node_id_t num_node; /**< number of nodes in the path. */
} output_path_t;

/**
 * Output Layer.
 * @ingroup g_output
 */
typedef struct _output_t_ {
    output_opt_t output_opt; /**< output tree options. */

    int output_size; /**< size of output tree. */
    int in_size; /**< size of input for output tree. */

    output_neuron_t *neurons; /**< output tree neurons. */
    int num_thrs; /**< number of threads/neurons. */

    output_tree_t *tree; /**< output tree. */
    output_path_t *paths; /**< Store a path for every leaf(word) node. */
} output_t;

/**
 * Load output tree option.
 * @ingroup g_output
 * @param[out] output_opt options loaded.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @return non-zero value if any error.
 */
int output_load_opt(output_opt_t *output_opt, st_opt_t *opt,
        const char *sec_name);

/**
 * Create a output tree with options.
 * @ingroup g_output
 * @param[in] output_opt options.
 * @param[in] output_size size of output tree.
 * @return a new output tree.
 */
output_t *output_create(output_opt_t *output_opt, int output_size);
/**
 * Destroy a output tree and set the pointer to NULL.
 * @ingroup g_output
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
 * Destroy a output tree.
 * @ingroup g_output
 * @param[in] output output to be destroyed.
 */
void output_destroy(output_t *output);
/**
 * Duplicate a output tree.
 * @ingroup g_output
 * @param[in] o output to be duplicated.
 * @return the duplicated output.
 */
output_t* output_dup(output_t *o);

/**
 * Load output tree header and initialise a new output tree.
 * @ingroup g_output
 * @param[out] output tree initialised.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[out] binary whether the file stream is in binary format.
 * @param[in] fo file stream used to print information, if it is not NULL.
 * @see output_load_body
 * @see output_save_header, output_save_body
 * @return non-zero value if any error.
 */
int output_load_header(output_t **output, int version, FILE *fp,
        bool *binary, FILE *fo);
/**
 * Load output tree body.
 * @ingroup g_output
 * @param[in] output output tree to be loaded.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[in] binary whether to use binary format.
 * @see output_load_header
 * @see output_save_header, output_save_body
 * @return non-zero value if any error.
 */
int output_load_body(output_t *output, int version, FILE *fp, bool binary);
/**
 * Save output tree header.
 * @ingroup g_output
 * @param[in] output output tree to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see output_save_body
 * @see output_load_header, output_load_body
 * @return non-zero value if any error.
 */
int output_save_header(output_t *output, FILE *fp, bool binary);
/**
 * Save output tree body.
 * @ingroup g_output
 * @param[in] output output tree to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see output_save_header
 * @see output_load_header, output_load_body
 * @return non-zero value if any error.
 */
int output_save_body(output_t *output, FILE *fp, bool binary);

/**
 * Generate output tree with word counts.
 * @ingroup g_output
 * @param[in] output output tree to be generated.
 * @param[in] word_cnts counts of words.
 * @return non-zero value if any error.
 */
int output_generate(output_t *output, count_t *word_cnts);

/**
 * Initializing HS for output tree.
 * @ingroup g_output
 * @param[in] output output tree.
 * @param[in] hs_size input size for HS.
 * @return non-zero value if any error.
 */
int output_hs_init(output_t *output, int hs_input_size);

/**
 * Activate neurons of pre output tree.
 * @ingroup g_output
 * @param[in] output output tree related.
 * @param[in] cls class of current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int output_activate_pre_layer(output_t *output, int cls, int tid);
/**
 * Activate neurons of last output tree.
 * @ingroup g_output
 * @param[in] output output tree related.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int output_activate_last_layer(output_t *output, int word, int tid);
/**
 * Compute loss of neurons of output tree.
 * @ingroup g_output
 * @param[in] output output tree related.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int output_loss(output_t *output, int word, int tid);
/**
 * Get probability of a word.
 * @ingroup g_output
 * @param[in] output output tree related.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
double output_get_prob(output_t *output, int word, int tid);
/**
 * Get class probability of a class.
 * @ingroup g_output
 * @param[in] output output tree related.
 * @param[in] cls the specific class.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
double output_get_class_prob_for_class(output_t *output, int cls, int tid);
/**
 * Get class probability of a word.
 * @ingroup g_output
 * @param[in] output output tree related.
 * @param[in] word the specific word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
double output_get_class_prob(output_t *output, int word, int tid);
/**
 * Get word probability of a word.
 * @ingroup g_output
 * @param[in] output output tree related.
 * @param[in] word the specific word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
double output_get_word_prob(output_t *output, int word, int tid);

/**
 * Setup training for output tree.
 * Called before training.
 * @ingroup g_output
 * @param[in] output output tree.
 * @param[in] num_thrs number of thread to be used.
 * @return non-zero value if any error.
 */
int output_setup_train(output_t *output, int num_thrs);
/**
 * Reset training for output tree.
 * Called before every input sentence to be trained.
 * @ingroup g_output
 * @param[in] output output tree.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int output_reset_train(output_t *output, int tid);
/**
 * Start training for output tree.
 * Called before every input word to be trained.
 * @ingroup g_output
 * @param[in] output output tree.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int output_start_train(output_t *output, int word, int tid);
/**
 * End training for output tree.
 * Called after every input word trained.
 * @ingroup g_output
 * @param[in] output output tree.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int output_end_train(output_t *output, int word, int tid);
/**
 * Finish training for output tree.
 * Called after all words trained.
 * @ingroup g_output
 * @param[in] output output tree.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int output_finish_train(output_t *output, int tid);

/**
 * Setup testing for output tree.
 * Called before testing.
 * @ingroup g_output
 * @param[in] output output tree.
 * @param[in] num_thrs number of thread to be used.
 * @return non-zero value if any error.
 */
int output_setup_test(output_t *output, int num_thrs);
/**
 * Reset testing for output tree.
 * Called before every input sentence to be tested.
 * @ingroup g_output
 * @param[in] output output tree.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int output_reset_test(output_t *output, int tid);
/**
 * Start testing for output tree.
 * Called before every input word to be tested.
 * @ingroup g_output
 * @param[in] output output tree.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int output_start_test(output_t *output, int word, int tid);
/**
 * End testing for output tree.
 * Called after every input word tested.
 * @ingroup g_output
 * @param[in] output output tree.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int output_end_test(output_t *output, int word, int tid);

/**
 * Setup generating for output tree.
 * Called before generating.
 * @ingroup g_output
 * @param[in] output output tree.
 * @return non-zero value if any error.
 */
int output_setup_gen(output_t *output);
/**
 * Reset generating for output tree.
 * Called before every sentence to be generated.
 * @ingroup g_output
 * @param[in] output output tree.
 * @return non-zero value if any error.
 */
int output_reset_gen(output_t *output);
/**
 * End generating for output tree.
 * Called after every word generated.
 * @ingroup g_output
 * @param[in] output output tree.
 * @param[in] word current generated word.
 * @return non-zero value if any error.
 */
int output_end_gen(output_t *output, int word);

/**
 * Whether two output tree is equal.
 * @ingroup g_output
 * @param[in] output1 first output.
 * @param[in] output2 second output.
 * @return true, if equal, false otherwise.
 */
bool output_equal(output_t *output1, output_t *output2);

#ifdef __cplusplus
}
#endif

#endif
