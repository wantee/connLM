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
#include <stutils/st_int.h>
#include <stutils/st_alphabet.h>
#include <stutils/st_queue.h>
#include <stutils/st_stack.h>

#include <connlm/config.h>

#include "utils.h"
#include "layers/layer.h"

/** @defgroup g_output Output Layer
 * Output Layer of NNet, structured as a tree.
 */

typedef int output_node_id_t;
#define OUTPUT_NODE_FMT "%d"

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

#define s_children(tree, node) (tree)->nodes[node].children_s
#define e_children(tree, node) (tree)->nodes[node].children_e
#define n_children(tree, node) (e_children(tree, node) - s_children(tree, node))
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

static inline int output_tree_leaf2word(output_tree_t *tree,
        output_node_id_t node)
{
    if (tree->leaf2word != NULL) {
        return tree->leaf2word[node];
    } else {
        return (int)node;
    }
}

static inline output_node_id_t output_tree_word2leaf(output_tree_t *tree,
        int word)
{
    if (tree->word2leaf != NULL) {
        return tree->word2leaf[word];
    } else {
        return (output_node_id_t)word;
    }
}

/**
 * aux data for output tree BFS.
 * @ingroup g_output
 */
typedef struct _output_tree_bfs_aux_t_ {
    st_queue_t *node_queue; /**< queue for tree node. */
} output_tree_bfs_aux_t;

/**
 * Create a output tree bfs aux.
 * @ingroup g_output
 * @param[in] tree output tree.
 * @return a new output tree bfs aux.
 */
output_tree_bfs_aux_t *output_tree_bfs_aux_create(output_tree_t *tree);
/**
 * Destroy a output tree bfs aux and set the pointer to NULL.
 * @ingroup g_output
 * @param[in] ptr pointer to output_tree_bfs_aux_t.
 */
#define safe_output_tree_bfs_aux_destroy(ptr) do {\
    if((ptr) != NULL) {\
        output_tree_bfs_aux_destroy(ptr);\
        safe_st_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a output tree bfs aux.
 * @ingroup g_output
 * @param[in] bfs_aux output tree bfs aux to be destroyed.
 */
void output_tree_bfs_aux_destroy(output_tree_bfs_aux_t *bfs_aux);

/**
 * Do BFS on a output tree.
 * @ingroup g_output
 * @param[in] tree the output tree.
 * @param[in] bfs_aux aux for bfs.
 * @param[in] visitor visitor callback called on every node in path.
 * @param[in] args args passed to visitor.
 * @return non-zero value if any error.
 */
int output_tree_bfs(output_tree_t *tree, output_tree_bfs_aux_t *bfs_aux,
        int (*visitor)(output_tree_t *tree,
            output_node_id_t node, void *args), void *args);

/**
 * aux data for output tree DFS.
 * @ingroup g_output
 */
typedef struct _output_tree_dfs_aux_t_ {
    st_stack_t *node_stack; /**< stack for tree node. */
    output_node_id_t *child_in_stack; /**< current child in the stack for every node*/
} output_tree_dfs_aux_t;

/**
 * Create a output tree dfs aux.
 * @ingroup g_output
 * @param[in] tree output tree.
 * @return a new output tree dfs aux.
 */
output_tree_dfs_aux_t *output_tree_dfs_aux_create(output_tree_t *tree);
/**
 * Destroy a output tree dfs aux and set the pointer to NULL.
 * @ingroup g_output
 * @param[in] ptr pointer to output_tree_dfs_aux_t.
 */
#define safe_output_tree_dfs_aux_destroy(ptr) do {\
    if((ptr) != NULL) {\
        output_tree_dfs_aux_destroy(ptr);\
        safe_st_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a output tree dfs aux.
 * @ingroup g_output
 * @param[in] dfs_aux output tree dfs aux to be destroyed.
 */
void output_tree_dfs_aux_destroy(output_tree_dfs_aux_t *dfs_aux);

/**
 * Do DFS on a output tree.
 * @ingroup g_output
 * @param[in] tree the output tree.
 * @param[in] dfs_aux aux for dfs.
 * @param[in] visitor visitor callback called on every node in path.
 * @param[in] args args passed to visitor.
 * @return non-zero value if any error.
 */
int output_tree_dfs(output_tree_t *tree, output_tree_dfs_aux_t *dfs_aux,
        int (*visitor)(output_tree_t *tree, output_node_id_t node,
            st_stack_t* stack, void *args), void *args);

/**
 * Output tree path.
 * @ingroup g_output
 */
typedef struct _output_tree_path_t_ {
    output_node_id_t *nodes; /**< nodes on the path. *EXCLUDE* root & leaf. */
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

    int output_size; /**< size of output layer. */
    output_norm_t norm; /**< normalization method. */

    output_tree_t *tree; /**< output tree. */
    output_path_t *paths; /**< Store a path for every leaf(word) node. */
    output_node_id_t unk_root; /**< root of subtree with only one path towards \<unk\>. */

    output_node_id_t *param_map; /**< map a node to index in param weight(used by Softmax). */
    output_node_id_t num_param_map; /**< number of param maps. */
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
 * Destroy a output tree and set the pointer to NULL.
 * @ingroup g_output
 * @param[in] ptr pointer to output_t.
 */
#define safe_output_destroy(ptr) do {\
    if((ptr) != NULL) {\
        output_destroy(ptr);\
        safe_st_free(ptr);\
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
 * @param[out] fmt storage format.
 * @param[out] fo file stream used to print information, if it is not NULL.
 * @see output_load_body
 * @see output_save_header, output_save_body
 * @return non-zero value if any error.
 */
int output_load_header(output_t **output, int version, FILE *fp,
        connlm_fmt_t *fmt, FILE *fo);
/**
 * Load output tree body.
 * @ingroup g_output
 * @param[in] output output tree to be loaded.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[in] fmt storage format.
 * @see output_load_header
 * @see output_save_header, output_save_body
 * @return non-zero value if any error.
 */
int output_load_body(output_t *output, int version, FILE *fp, connlm_fmt_t fmt);
/**
 * Save output tree header.
 * @ingroup g_output
 * @param[in] output output tree to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] fmt storage format.
 * @see output_save_body
 * @see output_load_header, output_load_body
 * @return non-zero value if any error.
 */
int output_save_header(output_t *output, FILE *fp, connlm_fmt_t fmt);
/**
 * Save output tree body.
 * @ingroup g_output
 * @param[in] output output tree to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] fmt storage format.
 * @see output_save_header
 * @see output_load_header, output_load_body
 * @return non-zero value if any error.
 */
int output_save_body(output_t *output, FILE *fp, connlm_fmt_t fmt);

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
            output_node_id_t next_node,
            output_node_id_t child_s, output_node_id_t child_e, void *args),
        void *args);

/**
 * Map node id to param idx
 * @ingroup g_output
 * @param[in] output the output layer.
 * @param[in] n node id.
 * @return the (mapped) param idx.
 */
static inline output_node_id_t output_param_idx(output_t *output,
        output_node_id_t n)
{
    if (output->param_map != NULL) {
        return output->param_map[n];
    }

    return n;
}

/**
 * Get output param size.
 * @ingroup g_output
 * @param[in] output the output layer.
 * @return the param size.
 */
static inline output_node_id_t output_param_size(output_t *output)
{
    if (output->param_map != NULL) {
        return output->num_param_map;
    }

    return output->tree->num_node;
}

#ifdef __cplusplus
}
#endif

#endif
