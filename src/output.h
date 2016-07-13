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
#include <stutils/st_alphabet.h>

#include <connlm/config.h>

#include "layers/layer.h"

/** @defgroup g_output Output Layer
 * Data structures and functions for Output Layer.
 */

typedef unsigned int output_node_id_t;
#define OUTPUT_NODE_FMT "%u"

#define OUTPUT_NODE_NONE ((output_node_id_t)-1)

/**
 * Type of constructing methods.
 * @ingroup g_output
 */
typedef enum _output_construct_method_t_ {
    OM_UNKNOWN = -1, /**< Unknown method. */
    OM_TOP_DOWN = 0, /**< Top-down method. */
    OM_BOTTOM_UP, /**< Bottom-up method. */
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
    real_t *ac; /**< activation of output layer. */
    real_t *er; /**< error of output layer. */

    real_t p; /**< Probility for word. */
} output_neuron_t;

#define s_children(tree, node) (tree)->nodes[node].children_s
#define e_children(tree, node) (tree)->nodes[node].children_e
#define is_leaf(tree, node) (s_children(tree, node) >= e_children(tree, node))

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
    output_node_id_t num_node; /**< number of nodes on output tree. */
    output_node_id_t cap_node; /**< capacity of nodes array. */

    output_node_id_t root; /**< root node of output tree. */
    output_node_id_t num_leaf; /**< number of leaf nodes. */
    int *leaf2word; /**< map leaf node id to word id. */
    output_node_id_t *word2leaf; /**< map word id to leaf node id. */
} output_tree_t;

/**
 * Output tree Path.
 * @ingroup g_output
 */
typedef struct _output_tree_path_t_ {
    output_node_id_t *nodes; /**< nodes on the path. */ // may exclude root
    output_node_id_t num_node; /**< number of nodes in the path. */
} output_path_t;

/**
 * Type of output layer normalization function.
 * @ingroup g_output
 */
typedef enum _output_normalization_method_t_ {
    ON_UNKNOWN = -1, /**< Unknown. */
    ON_UNDEFINED = 0, /**< Undefined. */
    ON_SOFTMAX, /**< Softmax. */
    ON_NCE, /**< Noise Contrastive Estimation. */
} output_norm_t;
/**
 * Output Layer.
 * @ingroup g_output
 */
typedef struct _output_t_ {
    output_opt_t output_opt; /**< output tree options. */

    int output_size; /**< size of output tree. */
    output_norm_t norm; /**< normalization method. */

    output_neuron_t *neurons; /**< output tree neurons. */
    int n_neu; /**< number of threads/neurons. */

    output_tree_t *tree; /**< output tree. */
    output_path_t *paths; /**< Store a path for every leaf(word) node. */

    output_node_id_t *param_map; /**< map a node to index in param weight(used by Softmax). */
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
 * @param[out] output output tree initialised.
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
 * @param[in] output_opt options for generating output.
 * @param[in] word_cnts counts of words, descendingly sorted.
 * @param[in] output_size size of output layer.
 * @return generated output, NULL if any error.
 */
output_t* output_generate(output_opt_t *output_opt, count_t *word_cnts,
       int output_size);

/**
 * Setup a output layer for running.
 * @ingroup g_output
 * @param[in] output output layer.
 * @return non-zero value if any error.
 */
int output_setup(output_t *output);

/**
 * Whether two output tree is equal.
 * @ingroup g_output
 * @param[in] output1 first output.
 * @param[in] output2 second output.
 * @return true, if equal, false otherwise.
 */
bool output_equal(output_t *output1, output_t *output2);

/**
 * Convert output to Graphviz dot language.
 * @ingroup g_output
 * @param[in] output output tree.
 * @param[in] fp FILE to be writed.
 * @param[in] word_cnts counts for words, could be NULL.
 * @param[in] vocab vocab of words, could be NULL.
 * @return non-zero value if any error.
 */
int output_draw(output_t *output, FILE *fp, count_t *word_cnts,
        st_alphabet_t *vocab);

#define OUTPUT_LAYER_NAME "output"

/**
 * Return a layer struct for output.
 * @ingroup g_output
 * @param[in] output the output layer.
 * @return layer for the output, else NULL.
 */
layer_t* output_get_layer(output_t *output);

/**
 * Parse topo file for output layer.
 * @ingroup g_output
 * @param[in] output the output layer.
 * @param[in] topo topo configs.
 * @return non-zero value if any error.
 */
int output_parse_topo(output_t *output, const char *topo);

/**
 * Walk through a path from root to word(not including) in output tree.
 * @ingroup g_output
 * @param[in] output the output layer.
 * @param[in] word the leaf word.
 * @param[in] walker walker callback called on every node in path.
 * @param[in] args args passed to walker.
 * @return non-zero value if any error.
 */
int output_walk_through_path(output_t *output, int word,
        int (*walker)(output_t *output, output_node_id_t node,
            output_node_id_t child_s, output_node_id_t child_e, void *args),
        void *args);

#ifdef __cplusplus
}
#endif

#endif
