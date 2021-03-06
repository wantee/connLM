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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

#include <stutils/st_macro.h>
#include <stutils/st_log.h>
#include <stutils/st_io.h>
#include <stutils/st_string.h>

#include "utils.h"
#include "output.h"

static const int OUTPUT_MAGIC_NUM = 626140498 + 20;
static const int OUTPUT_TREE_MAGIC_NUM = 626140498 + 21;

static const char *method_str[] = {
    "TopDown",
    "BottomUp",
};

static const char* method2str(output_method_t m)
{
    return method_str[m];
}

static output_method_t str2method(const char *str)
{
    int i;

    for (i = 0; i < sizeof(method_str) / sizeof(method_str[0]); i++) {
        if (strcasecmp(method_str[i], str) == 0) {
            return (output_method_t)i;
        }
    }

    return (output_method_t)-1;
}

static const char *norm_str[] = {
    "Undefined",
    "Softmax",
    "NCE",
};

static const char* norm2str(output_norm_t n)
{
    return norm_str[n];
}

static output_norm_t str2norm(const char *str)
{
    int i;

    for (i = 0; i < sizeof(norm_str) / sizeof(norm_str[0]); i++) {
        if (strcasecmp(norm_str[i], str) == 0) {
            return (output_norm_t)i;
        }
    }

    return ON_UNKNOWN;
}

int output_load_opt(output_opt_t *output_opt, st_opt_t *opt,
        const char *sec_name)
{
    char method_str[MAX_ST_CONF_LEN];
    int def_depth;
    int def_branch;

    ST_CHECK_PARAM(output_opt == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_STR(opt, sec_name, "METHOD",
            method_str, MAX_ST_CONF_LEN, "TopDown",
            "Constructing method(TopDown/BottomUp)");
    if (strcasecmp(method_str, "topdown") == 0
            || strcasecmp(method_str, "t") == 0
            || strcasecmp(method_str, "td") == 0) {
        output_opt->method = OM_TOP_DOWN;
        /* default value for class-based output. */
        def_depth = 2;
        def_branch = 100;
    } else if (strcasecmp(method_str, "bottomup") == 0
            || strcasecmp(method_str, "b") == 0
            || strcasecmp(method_str, "bu") == 0) {
        output_opt->method = OM_BOTTOM_UP;
        /* default value for hierarchical softmax output. */
        def_depth = 0;
        def_branch = 2;
    } else {
        ST_ERROR("Unknown construcing method[%s].", method_str);
        goto ST_OPT_ERR;
    }

    ST_OPT_SEC_GET_INT(opt, sec_name, "MAX_DEPTH",
            output_opt->max_depth, def_depth,
            "Maximum depth of output tree");

    ST_OPT_SEC_GET_INT(opt, sec_name, "MAX_BRANCH",
            output_opt->max_branch, def_branch,
            "Maximum branch of output tree");
    if (output_opt->max_branch <= 1) {
        ST_ERROR("MAX_BRANCH must larger than 1.");
        goto ST_OPT_ERR;
    }

    return 0;

ST_OPT_ERR:
    return -1;
}

#define NUM_REALLOC 100

#define safe_tree_destroy(ptr) do {\
    if((ptr) != NULL) {\
        output_tree_destroy(ptr);\
        safe_st_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)

static void output_tree_destroy(output_tree_t *tree)
{
    if (tree == NULL) {
        return;
    }

    safe_st_free(tree->nodes);

    tree->cap_node = 0;
    tree->num_node = 0;
    tree->root = OUTPUT_NODE_NONE;

    safe_st_free(tree->leaf2word);
    safe_st_free(tree->word2leaf);
}

output_tree_t* output_tree_dup(output_tree_t *t)
{
    output_tree_t *tree = NULL;
    size_t sz;

    ST_CHECK_PARAM(t == NULL, NULL);

    tree = (output_tree_t *)st_malloc(sizeof(output_tree_t));
    if (tree == NULL) {
        ST_ERROR("Failed to st_malloc output_tree_t.");
        goto ERR;
    }
    memset(tree, 0, sizeof(output_tree_t));

    *tree = *t;

    if (t->nodes != NULL) {
        sz = sizeof(output_tree_node_t) * tree->cap_node;
        tree->nodes = (output_tree_node_t *)st_malloc(sz);
        if (tree->nodes == NULL) {
            ST_ERROR("Failed to st_malloc nodes for tree.");
            goto ERR;
        }

        sz = sizeof(output_tree_node_t) * tree->num_node;
        memcpy(tree->nodes, t->nodes, sz);
    }

    if (t->word2leaf != NULL) {
        sz = sizeof(output_node_id_t)*t->num_leaf;
        tree->word2leaf = (output_node_id_t *)st_malloc(sz);
        if (tree->word2leaf == NULL) {
            ST_ERROR("Failed to st_malloc word2leaf for tree.");
            goto ERR;
        }

        memcpy(tree->word2leaf, t->word2leaf, sz);
    }
    if (t->leaf2word != NULL) {
        sz = sizeof(int)*t->num_node;
        tree->leaf2word = (int *)st_malloc(sz);
        if (tree->leaf2word == NULL) {
            ST_ERROR("Failed to st_malloc leaf2word for tree.");
            goto ERR;
        }

        memcpy(tree->leaf2word, t->leaf2word, sz);
    }
    return tree;

ERR:
    safe_tree_destroy(tree);
    return NULL;
}

bool output_tree_equal(output_tree_t *tree1, output_tree_t *tree2)
{
    output_node_id_t n;

    ST_CHECK_PARAM(tree1 == NULL || tree2 == NULL, false);

    if (tree1->root != tree2->root) {
        return false;
    }

    if (tree1->num_node != tree2->num_node) {
        return false;
    }

    if (tree1->num_leaf != tree2->num_leaf) {
        return false;
    }

    for (n = 0; n < tree1->num_node; n++) {
        if (s_children(tree1, n) != s_children(tree2, n)) {
            return false;
        }
        if (e_children(tree1, n) != e_children(tree2, n)) {
            return false;
        }
    }

    if ((tree1->word2leaf != NULL && tree2->word2leaf == NULL)
            || (tree1->word2leaf == NULL && tree2->word2leaf != NULL)) {
        return false;
    }

    if (tree1->word2leaf != NULL) {
        for (n = 0; n < tree1->num_leaf; n++) {
            if (tree1->word2leaf[n] != tree2->word2leaf[n]) {
                return false;
            }
        }
    }

    if ((tree1->leaf2word != NULL && tree2->leaf2word == NULL)
            || (tree1->leaf2word == NULL && tree2->leaf2word != NULL)) {
        return false;
    }

    if (tree1->leaf2word != NULL) {
        for (n = 0; n < tree1->num_node; n++) {
            if (tree1->leaf2word[n] != tree2->leaf2word[n]) {
                return false;
            }
        }
    }
    return true;
}

int output_tree_load_header(output_tree_t **tree, int version,
        FILE *fp, connlm_fmt_t *fmt, FILE *fo_info)
{
    char sym[MAX_LINE_LEN];
    size_t sz;
    union {
        char str[4];
        int magic_num;
    } flag;

    output_node_id_t num_node;
    output_node_id_t root;
    output_node_id_t num_leaf;
    bool has_leafmap;

    ST_CHECK_PARAM((tree == NULL && fo_info == NULL) || fp == NULL
            || fmt == NULL, -1);

    if (version < 3) {
        ST_ERROR("Too old version of connlm file");
        return -1;
    }

    if (fread(&flag.magic_num, sizeof(int), 1, fp) != 1) {
        ST_ERROR("Failed to load magic num.");
        return -1;
    }

    *fmt = CONN_FMT_UNKNOWN;
    if (strncmp(flag.str, "    ", 4) == 0) {
        *fmt = CONN_FMT_TXT;
    } else if (OUTPUT_TREE_MAGIC_NUM != flag.magic_num) {
        ST_ERROR("magic num wrong.");
        return -2;
    }

    if (tree != NULL) {
        *tree = NULL;
    }

    if (*fmt != CONN_FMT_TXT) {
        if (version >= 12) {
            if (fread(fmt, sizeof(connlm_fmt_t), 1, fp) != 1) {
                ST_ERROR("Failed to read fmt.");
                goto ERR;
            }
        } else {
            *fmt = CONN_FMT_BIN;
        }

        if (fread(&num_node, sizeof(output_node_id_t), 1, fp) != 1) {
            ST_ERROR("Failed to read output size.");
            return -1;
        }

        if (fread(&root, sizeof(output_node_id_t), 1, fp) != 1) {
            ST_ERROR("Failed to read root.");
            return -1;
        }

        if (fread(&num_leaf, sizeof(output_node_id_t), 1, fp) != 1) {
            ST_ERROR("Failed to read num_leaf.");
            return -1;
        }

        if (fread(&has_leafmap, sizeof(bool), 1, fp) != 1) {
            ST_ERROR("Failed to read has_leafmap.");
            return -1;
        }
    } else {
        if (st_readline(fp, "") != 0) {
            ST_ERROR("tag error.");
            goto ERR;
        }
        if (st_readline(fp, "<OUTPUT-TREE>") != 0) {
            ST_ERROR("tag error.");
            goto ERR;
        }

        if (st_readline(fp, "Num nodes: " OUTPUT_NODE_FMT,
                    &num_node) != 1) {
            ST_ERROR("Failed to parse num_node.");
            goto ERR;
        }

        if (st_readline(fp, "Root: " OUTPUT_NODE_FMT, &root) != 1) {
            ST_ERROR("Failed to parse root.");
            return -1;
        }

        if (st_readline(fp, "Num leaf: " OUTPUT_NODE_FMT,
                    &num_leaf) != 1) {
            ST_ERROR("Failed to parse num_leaf.");
            return -1;
        }

        if (st_readline(fp, "Has leaf map: %"xSTR(MAX_LINE_LEN)"s",
                    sym) != 1) {
            ST_ERROR("Failed to parse has_leafmap.");
            return -1;
        }
        sym[MAX_LINE_LEN - 1] = '\0';
        has_leafmap = str2bool(sym);
    }

    if (tree != NULL) {
        *tree = (output_tree_t *)st_malloc(sizeof(output_tree_t));
        if (*tree == NULL) {
            ST_ERROR("Failed to st_malloc output_tree_t");
            goto ERR;
        }
        memset(*tree, 0, sizeof(output_tree_t));

        (*tree)->num_node = num_node;
        (*tree)->cap_node = num_node;
        (*tree)->root = root;
        (*tree)->num_leaf = num_leaf;
        if (has_leafmap) {
            sz = sizeof(output_node_id_t)*num_leaf;
            (*tree)->word2leaf = (output_node_id_t *)st_malloc(sz);
            if ((*tree)->word2leaf == NULL) {
                ST_ERROR("Failed to st_malloc word2leaf.");
                goto ERR;
            }
            sz = sizeof(int)*num_node;
            (*tree)->leaf2word = (int *)st_malloc(sz);
            if ((*tree)->leaf2word == NULL) {
                ST_ERROR("Failed to st_malloc leaf2word.");
                goto ERR;
            }
        } else {
            (*tree)->word2leaf = NULL;
            (*tree)->leaf2word = NULL;
        }
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<OUTPUT-TREE>\n");
        fprintf(fo_info, "Num nodes: " OUTPUT_NODE_FMT "\n", num_node);
        fprintf(fo_info, "Root: " OUTPUT_NODE_FMT "\n", root);
        fprintf(fo_info, "Num leaf: " OUTPUT_NODE_FMT "\n", num_leaf);
        fprintf(fo_info, "Has leaf map: %s\n", bool2str(has_leafmap));
    }

    return 0;

ERR:
    if (tree != NULL) {
        safe_tree_destroy(*tree);
    }
    return -1;
}

int output_tree_load_body(output_tree_t *tree, int version,
        FILE *fp, connlm_fmt_t fmt)
{
    size_t sz;

    int n;
    output_node_id_t i, tmp;

    ST_CHECK_PARAM(tree == NULL || fp == NULL, -1);

    if (version < 3) {
        ST_ERROR("Too old version of connlm file");
        return -1;
    }

    tree->nodes = NULL;

    if (tree->num_node > 0) {
        sz = sizeof(output_tree_node_t) * tree->num_node;
        tree->nodes = (output_tree_node_t *)st_malloc(sz);
        if (tree->nodes == NULL) {
            ST_ERROR("Failed to st_malloc nodes.");
            goto ERR;
        }
        memset(tree->nodes, 0, sz);
    }

    if (connlm_fmt_is_bin(fmt)) {
        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to read magic num.");
            goto ERR;
        }

        if (n != -OUTPUT_TREE_MAGIC_NUM) {
            ST_ERROR("Magic num error.");
            goto ERR;
        }

        if (tree->num_node > 0) {
            if (fread(tree->nodes, sizeof(output_tree_node_t),
                        tree->num_node, fp) != tree->num_node) {
                ST_ERROR("Failed to read nodes.");
                goto ERR;
            }
        }

        if (tree->word2leaf != NULL) {
            if (fread(tree->word2leaf, sizeof(output_node_id_t),
                        tree->num_leaf, fp) != tree->num_leaf) {
                ST_ERROR("Failed to read word2leaf.");
                goto ERR;
            }
        }
        if (tree->leaf2word != NULL) {
            if (fread(tree->leaf2word, sizeof(int),
                        tree->num_node, fp) != tree->num_node) {
                ST_ERROR("Failed to read leaf2word.");
                goto ERR;
            }
        }
    } else {
        if (st_readline(fp, "<OUTPUT-TREE-DATA>") != 0) {
            ST_ERROR("body flag error.");
            goto ERR;
        }

        if (tree->num_node > 0) {
            if (st_readline(fp, "Nodes:") != 0) {
                ST_ERROR("nodes flag error.");
                goto ERR;
            }
            for (i = 0; i < tree->num_node; i++) {
                if (st_readline(fp, "\t"OUTPUT_NODE_FMT
                            "\t"OUTPUT_NODE_FMT"\t"OUTPUT_NODE_FMT,
                            &tmp,
                            &(s_children(tree, i)),
                            &(e_children(tree, i))) != 3) {
                    ST_ERROR("Failed to parse nodes.");
                    goto ERR;
                }
            }
        }
        if (tree->word2leaf != NULL) {
            if (st_readline(fp, "Word2leaf:") != 0) {
                ST_ERROR("word2leaf flag error.");
                goto ERR;
            }
            for (i = 0; i < tree->num_leaf; i++) {
                if (st_readline(fp, "\t"OUTPUT_NODE_FMT
                            "\t"OUTPUT_NODE_FMT,
                            &tmp, tree->word2leaf + i) != 2) {
                    ST_ERROR("Failed to parse word2leaf.");
                    goto ERR;
                }
            }
        }
        if (tree->leaf2word != NULL) {
            if (st_readline(fp, "Leaf2word:") != 0) {
                ST_ERROR("leaf2word flag error.");
                goto ERR;
            }
            for (i = 0; i < tree->num_node; i++) {
                if (st_readline(fp, "\t"OUTPUT_NODE_FMT
                            "\t"OUTPUT_NODE_FMT,
                            &tmp, tree->leaf2word + i) != 2) {
                    ST_ERROR("Failed to parse leaf2word.");
                    goto ERR;
                }
            }
        }
    }

    return 0;
ERR:
    safe_st_free(tree->nodes);

    return -1;
}

int output_tree_save_header(output_tree_t *tree, FILE *fp, connlm_fmt_t fmt)
{
    int n;
    bool has_leafmap;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (tree->word2leaf != NULL) {
        has_leafmap = true;
    } else {
        has_leafmap = false;
    }

    if (connlm_fmt_is_bin(fmt)) {
        if (fwrite(&OUTPUT_TREE_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to write magic num.");
            return -1;
        }
        if (fwrite(&fmt, sizeof(connlm_fmt_t), 1, fp) != 1) {
            ST_ERROR("Failed to write fmt.");
            return -1;
        }

        if (tree == NULL) {
            n = 0;
            if (fwrite(&n, sizeof(int), 1, fp) != 1) {
                ST_ERROR("Failed to write output size.");
                return -1;
            }
            return 0;
        }

        if (fwrite(&tree->num_node, sizeof(output_node_id_t),
                    1, fp) != 1) {
            ST_ERROR("Failed to write num_node.");
            return -1;
        }

        if (fwrite(&tree->root, sizeof(output_node_id_t), 1, fp) != 1) {
            ST_ERROR("Failed to write root.");
            return -1;
        }

        if (fwrite(&tree->num_leaf, sizeof(output_node_id_t),
                    1, fp) != 1) {
            ST_ERROR("Failed to write num_leaf.");
            return -1;
        }

        if (fwrite(&has_leafmap, sizeof(bool), 1, fp) != 1) {
            ST_ERROR("Failed to write root.");
            return -1;
        }
    } else {
        if (fprintf(fp, "    \n<OUTPUT-TREE>\n") < 0) {
            ST_ERROR("Failed to fprintf header.");
            return -1;
        }

        if (tree == NULL || tree->num_node <= 0) {
            if (fprintf(fp, "Num nodes: 0\n") < 0) {
                ST_ERROR("Failed to fprintf num nodes.");
                return -1;
            }
            return 0;
        }

        if (fprintf(fp, "Num nodes: " OUTPUT_NODE_FMT "\n",
                    tree->num_node) < 0) {
            ST_ERROR("Failed to fprintf num nodes.");
            return -1;
        }
        if (fprintf(fp, "Root: " OUTPUT_NODE_FMT "\n", tree->root) < 0) {
            ST_ERROR("Failed to fprintf root.");
            return -1;
        }
        if (fprintf(fp, "Num leaf: " OUTPUT_NODE_FMT "\n",
                    tree->num_leaf) < 0) {
            ST_ERROR("Failed to fprintf num leaf.");
            return -1;
        }
        if (fprintf(fp, "Has leaf map: %s\n",
                    bool2str(has_leafmap)) < 0) {
            ST_ERROR("Failed to fprintf has leaf map.");
            return -1;
        }
    }

    return 0;
}

int output_tree_save_body(output_tree_t *tree, FILE *fp, connlm_fmt_t fmt)
{
    output_node_id_t i;
    int n;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (tree == NULL) {
        return 0;
    }

    if (connlm_fmt_is_bin(fmt)) {
        n = -OUTPUT_TREE_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to write magic num.");
            return -1;
        }

        if (tree->num_node > 0) {
            if (fwrite(tree->nodes, sizeof(output_tree_node_t),
                        tree->num_node, fp) != tree->num_node) {
                ST_ERROR("Failed to write nodes.");
                return -1;
            }
        }
        if (tree->word2leaf != NULL) {
            if (fwrite(tree->word2leaf, sizeof(output_node_id_t),
                        tree->num_leaf, fp) != tree->num_leaf) {
                ST_ERROR("Failed to write word2leaf.");
                return -1;
            }
        }
        if (tree->leaf2word != NULL) {
            if (fwrite(tree->leaf2word, sizeof(int),
                        tree->num_node, fp) != tree->num_node) {
                ST_ERROR("Failed to write leaf2word.");
                return -1;
            }
        }
    } else {
        if (fprintf(fp, "<OUTPUT-TREE-DATA>\n") < 0) {
            ST_ERROR("Failed to fprintf header.");
            return -1;
        }
        if (tree->num_node > 0) {
            if (fprintf(fp, "Nodes:\n") < 0) {
                ST_ERROR("Failed to fprintf nodes.");
                return -1;
            }
            for (i = 0; i < tree->num_node; i++) {
                if (fprintf(fp, "\t"OUTPUT_NODE_FMT"\t"
                        OUTPUT_NODE_FMT"\t"OUTPUT_NODE_FMT"\n", i,
                        s_children(tree, i), e_children(tree, i)) < 0) {
                    ST_ERROR("Failed to fprintf node["
                            OUTPUT_NODE_FMT"]", i);
                    return -1;
                }
            }
        }
        if (tree->word2leaf != NULL) {
            if (fprintf(fp, "Word2leaf:\n") < 0) {
                ST_ERROR("Failed to fprintf word2leaf.");
                return -1;
            }
            for (i = 0; i < tree->num_leaf; i++) {
                if (fprintf(fp, "\t"OUTPUT_NODE_FMT"\t"OUTPUT_NODE_FMT"\n",
                            i, tree->word2leaf[i]) < 0) {
                    ST_ERROR("Failed to fprintf word2leaf["
                            OUTPUT_NODE_FMT"]", i);
                    return -1;
                }
            }
        }
        if (tree->leaf2word != NULL) {
            if (fprintf(fp, "Leaf2word:\n") < 0) {
                ST_ERROR("Failed to fprintf leaf2word.");
                return -1;
            }
            for (i = 0; i < tree->num_node; i++) {
                if (fprintf(fp, "\t"OUTPUT_NODE_FMT"\t"OUTPUT_NODE_FMT"\n",
                            i, tree->leaf2word[i]) < 0) {
                    ST_ERROR("Failed to fprintf leaf2word["
                            OUTPUT_NODE_FMT"]", i);
                    return -1;
                }
            }
        }
    }

    return 0;
}

static output_tree_t* output_tree_init(output_node_id_t cap_node)
{
    output_tree_t *tree = NULL;
    size_t sz;

    ST_CHECK_PARAM(cap_node <= 0, NULL);

    sz = sizeof(output_tree_t);
    tree = (output_tree_t *)st_malloc(sz);
    if (tree == NULL) {
        ST_ERROR("Failed to st_malloc tree.");
        goto ERR;
    }
    memset(tree, 0, sz);

    sz = cap_node * sizeof(output_tree_node_t);
    tree->nodes = (output_tree_node_t *)st_malloc(sz);
    if (tree->nodes == NULL) {
        ST_ERROR("Failed to st_malloc nodes.");
        goto ERR;
    }

    tree->num_node = 0;
    tree->cap_node = cap_node;
    tree->root = OUTPUT_NODE_NONE;

    return tree;
ERR:
    safe_tree_destroy(tree);
    return NULL;
}

static output_node_id_t output_tree_add_node(output_tree_t *tree)
{
    output_node_id_t node;

    ST_CHECK_PARAM(tree == NULL, OUTPUT_NODE_NONE);

    if (tree->num_node >= tree->cap_node) {
        tree->cap_node += NUM_REALLOC;
        tree->nodes = (output_tree_node_t *)st_realloc(tree->nodes,
                sizeof(output_tree_node_t) * tree->cap_node);
        if (tree->nodes == NULL) {
            ST_ERROR("Failed to st_realloc nodes.");
            return OUTPUT_NODE_NONE;
        }
    }

    node = tree->num_node;
    s_children(tree, node) = OUTPUT_NODE_NONE;
    e_children(tree, node) = OUTPUT_NODE_NONE;

    ++tree->num_node;

    return node;
}

static output_node_id_t output_tree_add_node_m(output_tree_t *tree,
        output_node_id_t n)
{
    output_node_id_t node;
    output_node_id_t i;

    ST_CHECK_PARAM(tree == NULL || n <= 0, OUTPUT_NODE_NONE);

    if (tree->num_node + n - 1 >= tree->cap_node) {
        tree->cap_node += n + NUM_REALLOC;
        tree->nodes = (output_tree_node_t *)st_realloc(tree->nodes,
                sizeof(output_tree_node_t) * tree->cap_node);
        if (tree->nodes == NULL) {
            ST_ERROR("Failed to st_realloc nodes.");
            return OUTPUT_NODE_NONE;
        }
    }

    node = tree->num_node;

    for (i = node; i < node + n; ++i) {
        s_children(tree, i) = OUTPUT_NODE_NONE;
        e_children(tree, i) = OUTPUT_NODE_NONE;
    }

    tree->num_node += n;

    return node;
}

typedef struct _huffman_tree_node_t_ { // two range of nodes for children
    output_node_id_t children_s[2];
    output_node_id_t children_e[2];
    count_t cnt;
} huffman_tree_node_t;

typedef struct _huffman_tree_t_ {
    huffman_tree_node_t *nodes;
    output_node_id_t num_node;
    output_node_id_t cap_node;

    output_node_id_t root;
} huffman_tree_t;

#define n_cnt(tree, node) tree->nodes[node].cnt

#define safe_huffman_tree_destroy(ptr) do {\
    if((ptr) != NULL) {\
        huffman_tree_destroy(ptr);\
        safe_st_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)

static void huffman_tree_destroy(huffman_tree_t *tree)
{
    if (tree == NULL) {
        return;
    }

    safe_st_free(tree->nodes);

    tree->cap_node = 0;
    tree->num_node = 0;
    tree->root = OUTPUT_NODE_NONE;
}

static huffman_tree_t* huffman_tree_init(output_node_id_t cap_node)
{
    huffman_tree_t *tree = NULL;
    size_t sz;

    ST_CHECK_PARAM(cap_node <= 0, NULL);

    sz = sizeof(huffman_tree_t);
    tree = (huffman_tree_t *)st_malloc(sz);
    if (tree == NULL) {
        ST_ERROR("Failed to st_malloc tree.");
        goto ERR;
    }
    memset(tree, 0, sz);

    sz = cap_node * sizeof(huffman_tree_node_t);
    tree->nodes = (huffman_tree_node_t *)st_malloc(sz);
    if (tree->nodes == NULL) {
        ST_ERROR("Failed to st_malloc nodes.");
        goto ERR;
    }

    tree->num_node = 0;
    tree->cap_node = cap_node;
    tree->root = OUTPUT_NODE_NONE;

    return tree;
ERR:
    safe_huffman_tree_destroy(tree);
    return NULL;
}

static output_node_id_t huffman_tree_add_node(huffman_tree_t *tree)
{
    output_node_id_t node;

    ST_CHECK_PARAM(tree == NULL, OUTPUT_NODE_NONE);

    if (tree->num_node >= tree->cap_node) {
        tree->cap_node += NUM_REALLOC;
        tree->nodes = (huffman_tree_node_t *)st_realloc(tree->nodes,
                sizeof(huffman_tree_node_t) * tree->cap_node);
        if (tree->nodes == NULL) {
            ST_ERROR("Failed to st_realloc nodes.");
            return OUTPUT_NODE_NONE;
        }
    }

    node = tree->num_node;
    n_cnt(tree, node) = 0;
    s_children(tree, node)[0] = OUTPUT_NODE_NONE;
    e_children(tree, node)[0] = OUTPUT_NODE_NONE;
    s_children(tree, node)[1] = OUTPUT_NODE_NONE;
    e_children(tree, node)[1] = OUTPUT_NODE_NONE;

    ++tree->num_node;

    return node;
}

static output_node_id_t huffman_tree_add_node_m(huffman_tree_t *tree,
        output_node_id_t n)
{
    output_node_id_t node;
    output_node_id_t i;

    ST_CHECK_PARAM(tree == NULL || n <= 0, OUTPUT_NODE_NONE);

    if (tree->num_node + n - 1 >= tree->cap_node) {
        tree->cap_node += n + NUM_REALLOC;
        tree->nodes = (huffman_tree_node_t *)st_realloc(tree->nodes,
                sizeof(huffman_tree_node_t) * tree->cap_node);
        if (tree->nodes == NULL) {
            ST_ERROR("Failed to st_realloc nodes.");
            return OUTPUT_NODE_NONE;
        }
    }

    node = tree->num_node;

    for (i = node; i < node + n; ++i) {
        n_cnt(tree, i) = 0;
        s_children(tree, i)[0] = OUTPUT_NODE_NONE;
        e_children(tree, i)[0] = OUTPUT_NODE_NONE;
        s_children(tree, i)[1] = OUTPUT_NODE_NONE;
        e_children(tree, i)[1] = OUTPUT_NODE_NONE;
    }

    tree->num_node += n;

    return node;
}

void output_destroy(output_t *output)
{
    int i;

    if (output == NULL) {
        return;
    }

    safe_tree_destroy(output->tree);

    if (output->paths != NULL) {
        for (i = 0; i < output->output_size; i++) {
            safe_st_free(output->paths[i].nodes);
        }
        safe_st_free(output->paths);
    }
    safe_st_free(output->param_map);
    output->num_param_map = 0;

    output->output_size = 0;
}

output_t* output_dup(output_t *o)
{
    output_t *output = NULL;
    size_t sz;

    int i;

    ST_CHECK_PARAM(o == NULL, NULL);

    output = (output_t *)st_malloc(sizeof(output_t));
    if (output == NULL) {
        ST_ERROR("Failed to st_malloc output_t.");
        goto ERR;
    }
    memset(output, 0, sizeof(output_t));

    output->output_opt = o->output_opt;
    *output = *o;

    if (o->tree != NULL) {
        output->tree = output_tree_dup(o->tree);
        if (output->tree == NULL) {
            ST_ERROR("Failed to output_tree_dup.");
            goto ERR;
        }

        if (o->param_map != NULL) {
            sz = sizeof(output_node_id_t) * (output->tree->num_node);
            output->param_map = (output_node_id_t *)st_malloc(sz);
            if (output->param_map == NULL) {
                ST_ERROR("Failed to st_malloc param_map.");
                goto ERR;
            }
            memcpy(output->param_map, o->param_map, sz);
        }
    }

    if (o->paths != NULL) {
        sz = sizeof(output_path_t) * output->output_size;
        output->paths = (output_path_t *)st_malloc(sz);
        if (output->paths == NULL) {
            ST_ERROR("Failed to st_malloc paths.");
            goto ERR;
        }
        for (i = 0; i < output->output_size; i++) {
            if (o->paths[i].num_node > 0) {
                output->paths[i].num_node = o->paths[i].num_node;
                sz = sizeof(output_node_id_t) * output->paths[i].num_node;
                output->paths[i].nodes = (output_node_id_t *)st_malloc(sz);
                if (output->paths[i].nodes == NULL) {
                    ST_ERROR("Failed to st_malloc nodes for path");
                    goto ERR;
                }
                memcpy(output->paths[i].nodes, o->paths[i].nodes, sz);
            } else {
                output->paths[i].nodes = NULL;
                output->paths[i].num_node = 0;
            }
        }
    }

    return output;

ERR:
    safe_output_destroy(output);
    return NULL;
}

static int output_gen_td_split(output_t *output, output_node_id_t node,
        count_t *word_cnts)
{
    int i;
    int a;
    long b;
    double dd;
    double df;

    int start;
    int end;
    int branches;

    output_node_id_t new_node;

    ST_CHECK_PARAM(output == NULL || word_cnts == NULL, -1);

    start = s_children(output->tree, node);
    end = e_children(output->tree, node);
    branches = output->output_opt.max_branch;

    if (end - start <= branches) {
        return 0;
    }

    if (start >= output->output_size) {
        ST_ERROR("parent of non-leaf node can not be splited.");
        return -1;
    }

    new_node = output_tree_add_node_m(output->tree, branches);
    if (new_node == OUTPUT_NODE_NONE) {
        ST_ERROR("Failed to output_tree_add_node_m.");
        return -1;
    }
    s_children(output->tree, node) = new_node;
    e_children(output->tree, node) = new_node + branches;

    a = 0;
    b = 0;
    dd = 0.0;
    df = 0.0;

    for (i = start; i < end; i++) {
        b += word_cnts[i];
    }
    for (i = start; i < end; i++) {
        dd += sqrt(word_cnts[i] / (double) b);
    }

    s_children(output->tree, new_node) = start;
    for (i = start; i < end; i++) {
        df += sqrt(word_cnts[i] / (double) b) / dd;
        if (df > 1) {
            df = 1;
        }
        if (df > (a + 1) / (double) branches) {
            if (a < branches - 1) {
                e_children(output->tree, new_node + a) = i + 1;
                a++;
                s_children(output->tree, new_node + a) = i + 1;
            }
        }
    }
    e_children(output->tree, new_node + branches - 1) = end;

    return 0;
}

static int output_gen_td(output_t *output, count_t *word_cnts)
{
    output_node_id_t node;
    output_node_id_t cap_node;

    output_node_id_t num_node;
    int depth;

    ST_CHECK_PARAM(output == NULL || word_cnts == NULL, -1);

    cap_node = output->output_size;
    cap_node += iceil(cap_node - 1, output->output_opt.max_branch - 1);

    output->tree = output_tree_init(cap_node);
    if (output->tree == NULL) {
        ST_ERROR("Failed to output_tree_init.");
        goto ERR;
    }

    /* init leaf nodes */
    if ((output_tree_add_node_m(output->tree, output->output_size))
            == OUTPUT_NODE_NONE) {
        ST_ERROR("Failed to output_tree_add_node_m.");
        goto ERR;
    }

    node = output_tree_add_node(output->tree);
    if (node == OUTPUT_NODE_NONE) {
        ST_ERROR("Failed to output_tree_add_node.");
        goto ERR;
    }
    s_children(output->tree, node) = 0;
    e_children(output->tree, node) = output->output_size;

    output->tree->root = node;

    num_node = output->tree->num_node;
    depth = 1;
    while (node < num_node) {
        if (output->output_opt.max_depth > 0
                && depth >= output->output_opt.max_depth) {
            break;
        }
        for (; node < num_node; ++node) {
            if (output_gen_td_split(output, node, word_cnts) < 0) {
                ST_ERROR("Failed to output_gen_td_split.");
                goto ERR;
            }
        }
        num_node = output->tree->num_node;
        ++depth;
    }

    return 0;

ERR:
    safe_tree_destroy(output->tree);
    return -1;
}

static huffman_tree_t* output_gen_bu_huffman(output_t *output,
        count_t *cnts)
{
    huffman_tree_t *huffman = NULL;

    output_node_id_t *depths = NULL;
    output_node_id_t depth;
    size_t sz;

    output_node_id_t cap_node;
    output_node_id_t parent;
    int n, pos0, pos1;
    int br;
    int move;
    bool exceed_depth;

    ST_CHECK_PARAM(output == NULL || cnts == NULL, NULL);

    cap_node = output->output_size;
    cap_node += iceil(cap_node - 1, output->output_opt.max_branch - 1);

    huffman = huffman_tree_init(cap_node);
    if (huffman == NULL) {
        ST_ERROR("Failed to huffman_tree_init.");
        goto ERR;
    }

    /* init leaf nodes */
    if ((huffman_tree_add_node_m(huffman, output->output_size))
            == OUTPUT_NODE_NONE) {
        ST_ERROR("Failed to huffman_tree_add_node_m.");
        goto ERR;
    }

    if (output->output_opt.max_depth == 1) {
        parent = huffman_tree_add_node(huffman);
        if (parent == OUTPUT_NODE_NONE) {
            ST_ERROR("Failed to huffman_tree_add_node parent.");
            goto ERR;
        }
        s_children(huffman, parent)[0] = 0;
        e_children(huffman, parent)[0] = output->output_size;
        n_cnt(huffman, parent) = 0;
        for (n = s_children(huffman, parent)[0];
                n < e_children(huffman, parent)[0]; ++n) {
            n_cnt(huffman, parent) += cnts[n];
        }

        huffman->root = parent;
        return huffman;
    }

    for (n = 0; n < huffman->num_node; ++n) {
        n_cnt(huffman, n) = cnts[n];
    }

    // Construct Huffman Tree
    if (output->output_opt.max_depth > 0) {
        sz = sizeof(output_node_id_t)*cap_node;
        depths = (output_node_id_t *)st_malloc(sz);
        if (depths == NULL) {
            ST_ERROR("Failed to st_malloc depths");
            goto ERR;
        }
        memset(depths, 0, sz);
    }
    pos0 = huffman->num_node - 1;
    pos1 = huffman->num_node;
    br = output->output_opt.max_branch;

    exceed_depth = false;
    while (br == output->output_opt.max_branch) {
        parent = huffman_tree_add_node(huffman);
        if (parent == OUTPUT_NODE_NONE) {
            ST_ERROR("Failed to huffman_tree_add_node parent.");
            goto ERR;
        }
        s_children(huffman, parent)[0] = pos0 + 1;
        e_children(huffman, parent)[0] = pos0 + 1;
        s_children(huffman, parent)[1] = pos1;
        e_children(huffman, parent)[1] = pos1;
        n_cnt(huffman, parent) = 0;

        depth = 0;
        br = 0;
        while (br < output->output_opt.max_branch) {
            move = -1;
            if ((pos1 < huffman->num_node - 1 && !exceed_depth)
                    && pos0 >= 0) { // both sides
                if (n_cnt(huffman, pos0) <= n_cnt(huffman, pos1)) {
                    move = 0;
                } else {
                    move = 1;
                }
            } else if (pos0 >= 0) { // only leaf side
                move = 0;
            } else if (pos1 < huffman->num_node - 1 && !exceed_depth) {
                // only internal side
                move = 1;
            } else {
                break;
            }

            if (move == 0) {
                n_cnt(huffman, parent) += n_cnt(huffman, pos0);
                if (depths != NULL) {
                    if (depths[pos0] > depth) {
                        depth = depths[pos0];
                    }
                    depths[pos0]++;
                }
                --pos0;
                --(s_children(huffman, parent)[0]);
            } else if (move == 1) {
                n_cnt(huffman, parent) += n_cnt(huffman, pos1);
                if (depths != NULL) {
                    if (depths[pos1] > depth) {
                        depth = depths[pos1];
                    }
                    depths[pos1]++;
                }
                ++pos1;
                ++(e_children(huffman, parent)[1]);
            } else {
                ST_ERROR("Impossible!!!");
                goto ERR;
            }

            ++br;
        }

        if (output->output_opt.max_depth > 0
                && depth + 2 >= output->output_opt.max_depth) {
            exceed_depth = true;
        }

        if(pos0 < 0 && pos1 == huffman->num_node - 1) {
            /* only last one node left. */
            ++pos1; /* avoid adding root */
            break;
        }

        if (depths != NULL) {
            depths[parent] = depth + 1;
        }
    }

    if (pos1 < huffman->num_node) {
        parent = huffman_tree_add_node(huffman);
        if (parent == OUTPUT_NODE_NONE) {
            ST_ERROR("Failed to huffman_tree_add_node parent.");
            goto ERR;
        }
        s_children(huffman, parent)[0] = 0;
        e_children(huffman, parent)[0] = pos0 + 1;
        s_children(huffman, parent)[1] = pos1;
        e_children(huffman, parent)[1] = huffman->num_node - 1;
        n_cnt(huffman, parent) = 0;
        for (n = s_children(huffman, parent)[0];
                n < e_children(huffman, parent)[0]; ++n) {
            n_cnt(huffman, parent) += n_cnt(huffman, n);
        }
        for (n = s_children(huffman, parent)[1];
                n < e_children(huffman, parent)[1]; ++n) {
            n_cnt(huffman, parent) += n_cnt(huffman, n);
        }
    }
    huffman->root = parent;

    safe_st_free(depths);
    return huffman;

ERR:
    safe_st_free(depths);
    safe_huffman_tree_destroy(huffman);
    return NULL;
}

typedef struct _renum_arg_t_ {
    output_node_id_t *nodemap;
    output_node_id_t num_node;
} renum_arg_t;

static int huffman_trav_renumber(huffman_tree_t *huffman,
        output_node_id_t node, void *args)
{
    renum_arg_t *renum;

    ST_CHECK_PARAM(huffman == NULL || args == NULL
            || node == OUTPUT_NODE_NONE, -1);

    renum = (renum_arg_t *)args;

    if (renum->nodemap[node] != OUTPUT_NODE_NONE) {
        ST_ERROR("node[" OUTPUT_NODE_FMT "] visited twice.");
        return -1;
    }

    renum->nodemap[node] = renum->num_node++;

    return 0;
}

typedef struct _h2t_arg_t_ {
    output_node_id_t *nodemap;
    output_tree_t *tree;
} h2t_arg_t;

static int huffman_trav_2tree(huffman_tree_t *huffman,
        output_node_id_t node, void *args)
{
    h2t_arg_t *h2t;

    output_node_id_t tnode;
    output_node_id_t start, end;

    ST_CHECK_PARAM(huffman == NULL || args == NULL
            || node == OUTPUT_NODE_NONE, -1);

    h2t = (h2t_arg_t *)args;

    tnode = output_tree_add_node(h2t->tree);
    if (tnode == OUTPUT_NODE_NONE) {
        ST_ERROR("Failed to output_tree_add_node.");
        return -1;
    }
    if (tnode != h2t->nodemap[node]) {
        ST_ERROR("nodemap not match.");
        return -1;
    }

    start = OUTPUT_NODE_NONE;
    end = OUTPUT_NODE_NONE;

    if (s_children(huffman, node)[0] < e_children(huffman, node)[0]) {
        start = h2t->nodemap[s_children(huffman, node)[0]];
        end = h2t->nodemap[e_children(huffman,node)[0] - 1] + 1;
    }

    if (s_children(huffman, node)[1] < e_children(huffman, node)[1]) {
        if (start == OUTPUT_NODE_NONE) {
            start = h2t->nodemap[s_children(huffman,node)[1]];
        } else {
            if (end != h2t->nodemap[s_children(huffman, node)[1]]) {
                ST_ERROR("children not continuous. "
                        "node["OUTPUT_NODE_FMT"/"OUTPUT_NODE_FMT"], "
                        "s_children["OUTPUT_NODE_FMT"/"OUTPUT_NODE_FMT"], "
                        "end["OUTPUT_NODE_FMT"].",
                        node, h2t->nodemap[node],
                        s_children(huffman, node)[1],
                        h2t->nodemap[s_children(huffman, node)[1]],
                        end);
                return -1;
            }
        }
        end = h2t->nodemap[e_children(huffman,node)[1] - 1] + 1;
    }

    s_children(h2t->tree, tnode) = start;
    e_children(h2t->tree, tnode) = end;

    return 0;
}

static int huffman_traverse(huffman_tree_t *huffman,
        int (*visitor)(huffman_tree_t *huffman,
            output_node_id_t node, void *args), void *args)
{
    st_queue_t *node_queue = NULL;

    void *tmp;
    output_node_id_t node, child;
    int sub;

    ST_CHECK_PARAM(huffman == NULL || visitor == NULL, -1);

    node_queue = st_queue_create(huffman->num_node);
    if(node_queue == NULL) {
        ST_ERROR("Failed to create node_queue.");
        goto ERR;
    }

    st_queue_clear(node_queue);
    if(st_enqueue(node_queue, (void *)(long)huffman->root)
            != ST_QUEUE_OK) {
        ST_ERROR("Failed to st_enqueue root");
        goto ERR;
    }

    while(!st_queue_empty(node_queue)) {
        if(st_dequeue(node_queue, &tmp) != ST_QUEUE_OK) {
            ST_ERROR("Failed to st_dequeue.");
            goto ERR;
        }
        node = (output_node_id_t)(long)tmp;

        if(visitor(huffman, node, args) < 0) {
            ST_ERROR("Failed to visitor.");
            goto ERR;
        }

        for (sub = 0; sub <= 1; ++sub) {
            for (child = s_children(huffman, node)[sub];
                    child < e_children(huffman, node)[sub]; ++child) {
                if(st_enqueue(node_queue, (void *)(long)child)
                        != ST_QUEUE_OK) {
                    ST_ERROR("Failed to st_enqueue child");
                    goto ERR;
                }
            }
        }
    }

    safe_st_queue_destroy(node_queue);
    return 0;

ERR:
    safe_st_queue_destroy(node_queue);
    return -1;
}

static int output_gen_bu_huffman2tree(output_t *output,
        huffman_tree_t *huffman)
{
    renum_arg_t renum;
    h2t_arg_t h2t;

    output_node_id_t *nodemap = NULL;
    output_node_id_t *rnodemap = NULL;

    size_t sz;
    output_node_id_t n;
    int i;

    ST_CHECK_PARAM(output == NULL || huffman == NULL, -1);

    sz = sizeof(output_node_id_t)*huffman->num_node;
    nodemap = (output_node_id_t *)st_malloc(sz);
    if (nodemap == NULL) {
        ST_ERROR("Failed to st_malloc nodemap.");
        goto ERR;
    }
    for (n = 0; n < huffman->num_node; ++n) {
        nodemap[n] = OUTPUT_NODE_NONE;
    }

    renum.nodemap = nodemap;
    renum.num_node = 0;
    if (huffman_traverse(huffman, huffman_trav_renumber, &renum) < 0) {
        ST_ERROR("Failed to huffman_traverse renumbering.");
        goto ERR;
    }
    if (renum.num_node != huffman->num_node) {
        ST_ERROR("some nodes are not renumbered.");
        goto ERR;
    }
#ifdef _OUTPUT_DEBUG_
    {
        output_node_id_t n;
        ST_DEBUG("NodeMap: ");
        for (n = 0; n < huffman->num_node; n++) {
            ST_DEBUG(OUTPUT_NODE_FMT" -> "OUTPUT_NODE_FMT,
                    n, renum.nodemap[n]);
        }
    }
#endif

    output->tree = output_tree_init(huffman->num_node);
    if (output->tree == NULL) {
        ST_ERROR("Failed to output_tree_init.");
        goto ERR;
    }
    output->tree->root = nodemap[huffman->root];

    h2t.nodemap = nodemap;
    h2t.tree = output->tree;
    if (huffman_traverse(huffman, huffman_trav_2tree, &h2t) < 0) {
        ST_ERROR("Failed to huffman_traverse huffman2tree.");
        goto ERR;
    }

    sz = sizeof(output_node_id_t) * output->output_size;
    output->tree->word2leaf = (output_node_id_t *)st_malloc(sz);
    if (output->tree->word2leaf == NULL) {
        ST_ERROR("Failed to st_malloc word2leaf.");
        goto ERR;
    }

    for (i = 0; i < output->output_size; i++) {
        output->tree->word2leaf[i] = nodemap[i];
    }

    sz = sizeof(output_node_id_t)*huffman->num_node;
    rnodemap = (output_node_id_t *)st_malloc(sz);
    if (rnodemap == NULL) {
        ST_ERROR("Failed to st_malloc rnodemap.");
        goto ERR;
    }
    for (n = 0; n < huffman->num_node; n++) {
        rnodemap[nodemap[n]] = n;
    }
    sz = sizeof(int) * output->tree->num_node;
    output->tree->leaf2word = (int *)st_malloc(sz);
    if (output->tree->leaf2word == NULL) {
        ST_ERROR("Failed to st_malloc leaf2word.");
        goto ERR;
    }
    for (n = 0; n < output->tree->num_node; n++) {
        if (is_leaf(output->tree, n)) {
            output->tree->leaf2word[n] = rnodemap[n];
        } else {
            output->tree->leaf2word[n] = -1;
        }
    }

    safe_st_free(nodemap);
    safe_st_free(rnodemap);

    return 0;

ERR:
    safe_st_free(nodemap);
    safe_st_free(rnodemap);
    safe_tree_destroy(output->tree);
    return -1;
}

static int output_gen_bu(output_t *output, count_t *word_cnts)
{
    huffman_tree_t *huffman = NULL;

    ST_CHECK_PARAM(output == NULL || word_cnts == NULL, -1);

    huffman = output_gen_bu_huffman(output, word_cnts);
    if (huffman == NULL) {
        ST_ERROR("Failed to output_gen_bu_huffman.");
        goto ERR;
    }

#ifdef _OUTPUT_DEBUG_
    {
        output_node_id_t n;
        ST_DEBUG("Built Huffman Tree. Nodes: "OUTPUT_NODE_FMT, huffman->num_node);
        ST_DEBUG("Root: "OUTPUT_NODE_FMT, huffman->root);
        for (n = 0; n < huffman->num_node; n++) {
            ST_DEBUG("Node: "OUTPUT_NODE_FMT", Child: "OUTPUT_NODE_FMT
                    "/"OUTPUT_NODE_FMT":"OUTPUT_NODE_FMT"/"OUTPUT_NODE_FMT, n,
                    s_children(huffman, n)[0], e_children(huffman, n)[0],
                    s_children(huffman, n)[1], e_children(huffman, n)[1]);
        }
    }
#endif

    if (output_gen_bu_huffman2tree(output, huffman) < 0) {
        ST_ERROR("Failed to output_gen_bu_huffman2tree.");
        goto ERR;
    }
    safe_huffman_tree_destroy(huffman);

    return 0;

ERR:
    safe_huffman_tree_destroy(huffman);
    return -1;
}

output_tree_dfs_aux_t* output_tree_dfs_aux_create(output_tree_t *tree)
{
    output_tree_dfs_aux_t *dfs_aux = NULL;

    ST_CHECK_PARAM(tree == NULL, NULL);

    dfs_aux = (output_tree_dfs_aux_t *)st_malloc(sizeof(output_tree_dfs_aux_t));
    if (dfs_aux == NULL) {
        ST_ERROR("Failed to st_malloc dfs_aux.");
        goto ERR;
    }
    memset(dfs_aux, 0, sizeof(output_tree_dfs_aux_t));

    dfs_aux->node_stack = st_stack_create(tree->num_node);
    if(dfs_aux->node_stack == NULL) {
        ST_ERROR("Failed to create node_stack.");
        goto ERR;
    }

    dfs_aux->child_in_stack = (output_node_id_t *)st_malloc(
            sizeof(output_node_id_t)*tree->num_node);
    if(dfs_aux->child_in_stack == NULL) {
        ST_ERROR("Failed to st_malloc child_in_stack.");
        goto ERR;
    }

    return dfs_aux;
ERR:
    safe_output_tree_dfs_aux_destroy(dfs_aux);
    return NULL;
}

void output_tree_dfs_aux_destroy(output_tree_dfs_aux_t* dfs_aux)
{
    if (dfs_aux == NULL) {
        return;
    }

    safe_st_stack_destroy(dfs_aux->node_stack);
    safe_st_free(dfs_aux->child_in_stack);
}

int output_tree_dfs(output_tree_t *tree, output_tree_dfs_aux_t *dfs_aux,
        int (*visitor)(output_tree_t *tree, output_node_id_t node,
            st_stack_t* stack, void *args), void *args)
{
    st_stack_t *node_stack;
    output_node_id_t *child_in_stack;

    void *tmp;
    output_node_id_t node, child;

    ST_CHECK_PARAM(tree == NULL || dfs_aux == NULL || visitor == NULL, -1);

    node_stack = dfs_aux->node_stack;
    child_in_stack = dfs_aux->child_in_stack;

    for (node = 0; node < tree->num_node; node++) {
        child_in_stack[node] = OUTPUT_NODE_NONE;
    }
    st_stack_clear(node_stack);

    if(st_stack_push(node_stack, (void *)(long)tree->root)
            != ST_STACK_OK) {
        ST_ERROR("Failed to st_stack_push root");
        return -1;
    }

#ifdef _OUTPUT_DEBUG_
    ST_DEBUG("PUSH: " OUTPUT_NODE_FMT, tree->root);
#endif

    while(!st_stack_empty(node_stack)) {
        if(st_stack_top(node_stack, &tmp) != ST_STACK_OK) {
            ST_ERROR("Failed to st_stack_top.");
            return -1;
        }
        node = (output_node_id_t)(long)tmp;

        if (is_leaf(tree, node)
                || (child_in_stack[node] != OUTPUT_NODE_NONE
                    && child_in_stack[node] >= e_children(tree, node))) {
            if(st_stack_pop(node_stack, &tmp) != ST_STACK_OK) {
                ST_ERROR("Failed to st_stack_pop.");
                return -1;
            }
#ifdef _OUTPUT_DEBUG_
            ST_DEBUG("POP: " OUTPUT_NODE_FMT, node);
#endif
            if(visitor(tree, node, node_stack, args) < 0) {
                ST_ERROR("Failed to visitor.");
                return -1;
            }
            continue;
        }

        if (child_in_stack[node] == OUTPUT_NODE_NONE) {
            child = s_children(tree, node);
            child_in_stack[node] = child;
        } else {
            child = ++(child_in_stack[node]);
        }
        if (child_in_stack[node] < e_children(tree, node)) {
            if(st_stack_push(node_stack, (void *)(long)child)
                    != ST_STACK_OK) {
                ST_ERROR("Failed to st_stack_push child");
                return -1;
            }
#ifdef _OUTPUT_DEBUG_
            ST_DEBUG("PUSH: " OUTPUT_NODE_FMT, child);
#endif
        }
    }

    return 0;
}

output_tree_bfs_aux_t* output_tree_bfs_aux_create(output_tree_t *tree)
{
    output_tree_bfs_aux_t *bfs_aux = NULL;

    ST_CHECK_PARAM(tree == NULL, NULL);

    bfs_aux = (output_tree_bfs_aux_t *)st_malloc(sizeof(output_tree_bfs_aux_t));
    if (bfs_aux == NULL) {
        ST_ERROR("Failed to st_malloc bfs_aux.");
        goto ERR;
    }
    memset(bfs_aux, 0, sizeof(output_tree_bfs_aux_t));

    bfs_aux->node_queue = st_queue_create(tree->num_node);
    if(bfs_aux->node_queue == NULL) {
        ST_ERROR("Failed to create node_queue.");
        goto ERR;
    }

    return bfs_aux;
ERR:
    safe_output_tree_bfs_aux_destroy(bfs_aux);
    return NULL;
}

void output_tree_bfs_aux_destroy(output_tree_bfs_aux_t* bfs_aux)
{
    if (bfs_aux == NULL) {
        return;
    }

    safe_st_queue_destroy(bfs_aux->node_queue);
}

int output_tree_bfs(output_tree_t *tree, output_tree_bfs_aux_t *bfs_aux,
        int (*visitor)(output_tree_t *tree,
            output_node_id_t node, void *args), void *args)
{
    st_queue_t *node_queue;

    void *tmp;
    output_node_id_t node, child;

    ST_CHECK_PARAM(tree == NULL || bfs_aux == NULL || visitor == NULL, -1);

    node_queue = bfs_aux->node_queue;
    st_queue_clear(node_queue);

    if(st_enqueue(node_queue, (void *)(long)tree->root) != ST_QUEUE_OK) {
        ST_ERROR("Failed to st_enqueue root");
        return -1;
    }

    while(!st_queue_empty(node_queue)) {
        if(st_dequeue(node_queue, &tmp) != ST_QUEUE_OK) {
            ST_ERROR("Failed to st_dequeue.");
            return -1;
        }
        node = (output_node_id_t)(long)tmp;

        if(visitor(tree, node, args) < 0) {
            ST_ERROR("Failed to visitor.");
            return -1;
        }

        for (child = s_children(tree, node);
                child < e_children(tree, node); ++child) {
            if(st_enqueue(node_queue, (void *)(long)child) != ST_QUEUE_OK) {
                ST_ERROR("Failed to st_enqueue child");
                return -1;
            }
        }
    }

    return 0;
}

static int output_tree_dfs_trav_gen_path(output_tree_t *tree,
        output_node_id_t node, st_stack_t *stack, void *args)
{
    output_t *output;

    void *tmp;
    st_stack_id_t i, n;
    int word;

    ST_CHECK_PARAM(tree == NULL || stack == NULL || args == NULL, -1);

    if (!is_leaf(tree, node)) {
        return 0;
    }

    output = (output_t *)args;
    word = output_tree_leaf2word(tree, node);
    if (word < 0 || word >= output->output_size) {
        ST_ERROR("Error leaf node["OUTPUT_NODE_FMT"], word[%d]",
                node, word);
        return -1;
    }

    if (output->paths[word].num_node > 0) {
        ST_ERROR("Duplicated leaf node["OUTPUT_NODE_FMT"] during DFS",
                node);
        return -1;
    }

    n = st_stack_size(stack);
    if (n > 1) {
        output->paths[word].nodes = (output_node_id_t *)st_malloc(
                sizeof(output_node_id_t)*(n-1));
        if (output->paths[word].nodes == NULL) {
            ST_ERROR("Failed to st_malloc nodes for path");
            goto ERR;
        }
        output->paths[word].num_node = n - 1;
    }

    for (i = n - 1; i > 0; i--) { /* exclude *root* */
        if(st_stack_topn(stack, i, &tmp) != ST_STACK_OK) {
            ST_ERROR("Failed to st_stack_topn[%d].", i);
            goto ERR;
        }
        output->paths[word].nodes[n-1-i] = (output_node_id_t)(long)tmp;
    }

    return 0;
ERR:
    safe_st_free(output->paths[word].nodes);
    output->paths[word].num_node = 0;

    return -1;
}

static int output_generate_path(output_t *output)
{
    output_tree_dfs_aux_t *dfs_aux = NULL;
    size_t sz;

    output_path_t *path;
    output_node_id_t n;

    ST_CHECK_PARAM(output == NULL || output->tree == NULL, -1);

    sz = sizeof(output_path_t) * output->output_size;
    output->paths = (output_path_t *)st_malloc(sz);
    if (output->paths == NULL) {
        ST_ERROR("Failed to st_malloc paths.");
        goto ERR;
    }
    memset(output->paths, 0, sz);

    dfs_aux = output_tree_dfs_aux_create(output->tree);
    if (dfs_aux == NULL) {
        ST_ERROR("Failed to output_tree_dfs_aux_create.");
        goto ERR;
    }

    if (output_tree_dfs(output->tree, dfs_aux,
                output_tree_dfs_trav_gen_path,
                output) < 0) {
        ST_ERROR("Failed to output_tree_dfs.");
        goto ERR;
    }

    safe_output_tree_dfs_aux_destroy(dfs_aux);

    // get <unk> root
    output->unk_root = OUTPUT_NODE_NONE;
    path = output->paths + UNK_ID;
    if (path->num_node > 0) {
        for (n = path->num_node - 1; n >= 0; n--) {
            if (n_children(output->tree, path->nodes[n]) > 1) {
                break;
            }
            output->unk_root = path->nodes[n];
        }
    }

    return 0;

ERR:
    safe_st_free(output->paths);
    safe_output_tree_dfs_aux_destroy(dfs_aux);
    return -1;
}

typedef struct _output_gen_param_map_args_t_ {
    output_node_id_t acc;
    output_node_id_t *param_map;
} output_gen_param_map_args_t;

static int output_tree_bfs_trav_gen_param_map(output_tree_t *tree,
        output_node_id_t node, void *args)
{
    output_gen_param_map_args_t *ogpm_args;

    output_node_id_t s_child;
    output_node_id_t e_child;

    ST_CHECK_PARAM(tree == NULL || args == NULL, -1);

    ogpm_args = (output_gen_param_map_args_t *)args;

    if (node == tree->root) {
        ogpm_args->param_map[node] = OUTPUT_NODE_NONE;
    } else if (ogpm_args->param_map[node] != OUTPUT_NODE_NONE) {
        ogpm_args->param_map[node] = ogpm_args->acc;
        ogpm_args->acc += 1;
    }

    s_child = s_children(tree, node);
    e_child = e_children(tree, node);

    if (e_child > s_child && e_child != OUTPUT_NODE_NONE) {
        ogpm_args->param_map[e_child - 1] = OUTPUT_NODE_NONE;
    }

    return 0;
}

static int output_generate_param_map(output_t *output)
{
    output_tree_bfs_aux_t *bfs_aux = NULL;
    output_gen_param_map_args_t ogpm_args;
    size_t sz;

    ST_CHECK_PARAM(output == NULL || output->tree == NULL, -1);

    safe_st_free(output->param_map);

    sz = sizeof(output_node_id_t) * (output->tree->num_node);
    output->param_map = (output_node_id_t *)st_malloc(sz);
    if (output->param_map == NULL) {
        ST_ERROR("Failed to st_malloc param_map.");
        goto ERR;
    }
    memset(output->param_map, 0, sz);

    bfs_aux = output_tree_bfs_aux_create(output->tree);
    if (bfs_aux == NULL) {
        ST_ERROR("Failed to output_tree_bfs_aux_create.");
        goto ERR;
    }
    ogpm_args.param_map = output->param_map;
    ogpm_args.acc = 0;
    if (output_tree_bfs(output->tree, bfs_aux,
                output_tree_bfs_trav_gen_param_map,
                &ogpm_args) < 0) {
        ST_ERROR("Failed to output_tree_bfs.");
        goto ERR;
    }
    output->num_param_map = ogpm_args.acc;

#ifdef _OUTPUT_DEBUG_
    {
        output_node_id_t i;
        for (i = 0; i < output->tree->num_node; i++) {
            ST_DEBUG("param map["OUTPUT_NODE_FMT"] = "OUTPUT_NODE_FMT,
                    i, output->param_map[i]);
        }
    }
#endif

    safe_output_tree_bfs_aux_destroy(bfs_aux);
    return 0;

ERR:
    safe_st_free(output->param_map);
    safe_output_tree_bfs_aux_destroy(bfs_aux);
    return -1;
}

output_t* output_generate(output_opt_t *output_opt, count_t *word_cnts,
       int output_size)
{
    output_t *output = NULL;

    ST_CHECK_PARAM(output_opt == NULL || word_cnts == NULL
            || output_size <= 0, NULL);

    output = (output_t *)st_malloc(sizeof(output_t));
    if (output == NULL) {
        ST_ERROR("Failed to st_malloc output_t.");
        goto ERR;
    }
    memset(output, 0, sizeof(output_t));

    output->output_opt = *output_opt;
    output->output_size = output_size;

    switch (output->output_opt.method) {
        case OM_TOP_DOWN:
            if (output_gen_td(output, word_cnts) < 0) {
                ST_ERROR("Failed to output_gen_td.");
                goto ERR;
            }
            break;
        case OM_BOTTOM_UP:
            if (output_gen_bu(output, word_cnts) < 0) {
                ST_ERROR("Failed to output_gen_bu.");
                goto ERR;
            }
            break;
        default:
            ST_ERROR("Unknown constructing method[%d]",
                    output->output_opt.method);
            break;
    }
    output->tree->num_leaf = output->output_size;

    ST_NOTICE("Built Tree. Nodes: "OUTPUT_NODE_FMT, output->tree->num_node);

#ifdef _OUTPUT_DEBUG_
    {
        output_node_id_t n;
        ST_DEBUG("Root: "OUTPUT_NODE_FMT, output->tree->root);
        for (n = 0; n < output->tree->num_node; n++) {
            ST_DEBUG("Node: "OUTPUT_NODE_FMT", Child: "OUTPUT_NODE_FMT
                    "/"OUTPUT_NODE_FMT, n,
                    s_children(output->tree, n), e_children(output->tree, n));
        }
    }
#endif

    return output;

  ERR:
    safe_output_destroy(output);
    return NULL;
}

int output_setup(output_t *output)
{
    ST_CHECK_PARAM(output == NULL, -1);

    if (output->paths == NULL) {
        if (output_generate_path(output) < 0) {
            ST_ERROR("Failed to output_generate_path.");
            return -1;
        }
    }

    if (output->param_map == NULL) {
        if (output_generate_param_map(output) < 0) {
            ST_ERROR("Failed to output_generate_param_map.");
            return -1;
        }
    }

    return 0;
}

bool output_equal(output_t *output1, output_t *output2)
{
    ST_CHECK_PARAM(output1 == NULL || output2 == NULL, false);

    if (output1->output_size != output2->output_size) {
        return false;
    }

    if (output1->norm != output2->norm) {
        return false;
    }

    return output_tree_equal(output1->tree, output2->tree);
}

int output_load_header(output_t **output, int version,
        FILE *fp, connlm_fmt_t *fmt, FILE *fo_info)
{
    char sym[MAX_LINE_LEN];
    union {
        char str[4];
        int magic_num;
    } flag;

    int output_size;
    int n;
    int m;
    int max_depth;
    int max_branch;
    connlm_fmt_t f;

    ST_CHECK_PARAM((output == NULL && fo_info == NULL) || fp == NULL
            || fmt == NULL, -1);

    if (version < 3) {
        ST_ERROR("Too old version of connlm file");
        return -1;
    }

    if (fread(&flag.magic_num, sizeof(int), 1, fp) != 1) {
        ST_ERROR("Failed to load magic num.");
        return -1;
    }

    *fmt = CONN_FMT_UNKNOWN;
    if (strncmp(flag.str, "    ", 4) == 0) {
        *fmt = CONN_FMT_TXT;
    } else if (OUTPUT_MAGIC_NUM != flag.magic_num) {
        ST_ERROR("magic num wrong.");
        return -2;
    }

    if (output != NULL) {
        *output = NULL;
    }

    if (*fmt != CONN_FMT_TXT) {
        if (version >= 12) {
            if (fread(fmt, sizeof(connlm_fmt_t), 1, fp) != 1) {
                ST_ERROR("Failed to read fmt.");
                goto ERR;
            }
        } else {
            *fmt = CONN_FMT_BIN;
        }

        if (fread(&output_size, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to read output size.");
            goto ERR;
        }

        if (output_size <= 0) {
            if (fo_info != NULL) {
                fprintf(fo_info, "\n<OUTPUT>: None\n");
            }
            return 0;
        }

        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to read norm.");
            goto ERR;
        }

        if (fread(&m, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to read output_method.");
            goto ERR;
        }

        if (fread(&max_depth, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to read max_depth.");
            goto ERR;
        }

        if (fread(&max_branch, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to read max_branch.");
            goto ERR;
        }
    } else {
        if (st_readline(fp, "") != 0) {
            ST_ERROR("tag error.");
            goto ERR;
        }
        if (st_readline(fp, "<OUTPUT>") != 0) {
            ST_ERROR("tag error.");
            goto ERR;
        }

        if (st_readline(fp, "Output size: %d", &output_size) != 1) {
            ST_ERROR("Failed to parse output_size.");
            goto ERR;
        }

        if (output_size <= 0) {
            if (fo_info != NULL) {
                fprintf(fo_info, "\n<OUTPUT>: None\n");
            }
            return 0;
        }

        if (st_readline(fp, "Normalization: %"xSTR(MAX_LINE_LEN)"s",
                    sym) != 1) {
            ST_ERROR("Failed to parse norm.");
            goto ERR;
        }
        sym[MAX_LINE_LEN - 1] = '\0';
        n = (int)str2norm(sym);
        if (n == (int)ON_UNKNOWN) {
            ST_ERROR("Unknown Normalization[%s]", sym);
            goto ERR;
        }

        if (st_readline(fp, "Method: %"xSTR(MAX_LINE_LEN)"s", sym) != 1) {
            ST_ERROR("Failed to parse method.");
            goto ERR;
        }
        sym[MAX_LINE_LEN - 1] = '\0';
        m = (int)str2method(sym);
        if (m == (int)OM_UNKNOWN) {
            ST_ERROR("Unknown method[%s]", sym);
            goto ERR;
        }

        if (st_readline(fp, "Max Depth: %d", &max_depth) != 1) {
            ST_ERROR("Failed to parse in max depth.");
            goto ERR;
        }

        if (st_readline(fp, "Max Branch: %d", &max_branch) != 1) {
            ST_ERROR("Failed to parse in max branch.");
            goto ERR;
        }
    }

    if (output != NULL) {
        *output = (output_t *)st_malloc(sizeof(output_t));
        if (*output == NULL) {
            ST_ERROR("Failed to st_malloc output_t");
            goto ERR;
        }
        memset(*output, 0, sizeof(output_t));

        (*output)->output_size = output_size;
        (*output)->norm = (output_norm_t)n;
        (*output)->output_opt.method = (output_method_t)m;
        (*output)->output_opt.max_depth = max_depth;
        (*output)->output_opt.max_branch = max_branch;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<OUTPUT>\n");
        fprintf(fo_info, "Output size: %d\n", output_size);
        fprintf(fo_info, "Normalization: %s\n",
                norm2str((output_norm_t)n));
        fprintf(fo_info, "Method: %s\n", method2str((output_method_t)m));
        fprintf(fo_info, "Max Depth: %d\n", max_depth);
        fprintf(fo_info, "Max Branch: %d\n", max_branch);
    }

    if (output_tree_load_header(output != NULL ? &((*output)->tree) : NULL,
                version, fp, &f, fo_info) < 0) {
        ST_ERROR("Failed to output_tree_load_header.");
        goto ERR;
    }
    if (*fmt != f) {
        ST_ERROR("Multiple formats in one file.");
        return -1;
    }

    return 0;

ERR:
    if (output != NULL) {
        safe_output_destroy(*output);
    }
    return -1;
}

int output_load_body(output_t *output, int version, FILE *fp, connlm_fmt_t fmt)
{
    int n;

    ST_CHECK_PARAM(output == NULL || fp == NULL, -1);

    if (version < 3) {
        ST_ERROR("Too old version of connlm file");
        return -1;
    }

    if (connlm_fmt_is_bin(fmt)) {
        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to read magic num.");
            goto ERR;
        }

        if (n != -OUTPUT_MAGIC_NUM) {
            ST_ERROR("Magic num error.");
            goto ERR;
        }

    } else {
        if (st_readline(fp, "<OUTPUT-DATA>") != 0) {
            ST_ERROR("body flag error.");
            goto ERR;
        }
    }

    if (output_tree_load_body(output->tree, version, fp, fmt) < 0) {
        ST_ERROR("Failed to output_tree_load_body.");
        goto ERR;
    }

    if (output_setup(output) < 0) {
        ST_ERROR("Failed to output_setup");
        goto ERR;
    }

    return 0;
ERR:
    safe_tree_destroy(output->tree);

    return -1;
}

int output_save_header(output_t *output, FILE *fp, connlm_fmt_t fmt)
{
    int n;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (connlm_fmt_is_bin(fmt)) {
        if (fwrite(&OUTPUT_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to write magic num.");
            return -1;
        }
        if (fwrite(&fmt, sizeof(connlm_fmt_t), 1, fp) != 1) {
            ST_ERROR("Failed to write fmt.");
            return -1;
        }

        if (output == NULL) {
            n = 0;
            if (fwrite(&n, sizeof(int), 1, fp) != 1) {
                ST_ERROR("Failed to write output size.");
                return -1;
            }
            return 0;
        }

        if (fwrite(&output->output_size, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to write output size.");
            return -1;
        }

        n = (int)output->norm;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to write norm.");
            return -1;
        }

        n = (int)output->output_opt.method;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to write method.");
            return -1;
        }

        if (fwrite(&output->output_opt.max_depth, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to write max_depth.");
            return -1;
        }

        if (fwrite(&output->output_opt.max_branch, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to write max_branch.");
            return -1;
        }
    } else {
        if (fprintf(fp, "    \n<OUTPUT>\n") < 0) {
            ST_ERROR("Failed to fprintf header.");
            return -1;
        }

        if (output == NULL) {
            if (fprintf(fp, "Output size: 0\n") < 0) {
                ST_ERROR("Failed to fprintf output size.");
                return -1;
            }
            return 0;
        }

        if (fprintf(fp, "Output size: %d\n", output->output_size) < 0) {
            ST_ERROR("Failed to fprintf output size.");
            return -1;
        }
        if (fprintf(fp, "Normalization: %s\n",
                    norm2str(output->norm)) < 0) {
            ST_ERROR("Failed to fprintf norm.");
            return -1;
        }
        if (fprintf(fp, "Method: %s\n",
                    method2str(output->output_opt.method)) < 0) {
            ST_ERROR("Failed to fprintf method.");
            return -1;
        }
        if (fprintf(fp, "Max Depth: %d\n",
                    output->output_opt.max_depth) < 0) {
            ST_ERROR("Failed to fprintf max depth.");
            return -1;
        }
        if (fprintf(fp, "Max Branch: %d\n",
                    output->output_opt.max_branch) < 0) {
            ST_ERROR("Failed to fprintf max branch.");
            return -1;
        }
    }

    if (output_tree_save_header(output->tree, fp, fmt) < 0) {
        ST_ERROR("Failed to output_tree_save_header.");
        return -1;
    }

    return 0;
}

int output_save_body(output_t *output, FILE *fp, connlm_fmt_t fmt)
{
    int n;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (output == NULL) {
        return 0;
    }

    if (connlm_fmt_is_bin(fmt)) {
        n = -OUTPUT_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to write magic num.");
            return -1;
        }
    } else {
        if (fprintf(fp, "<OUTPUT-DATA>\n") < 0) {
            ST_ERROR("Failed to fprintf header.");
            return -1;
        }
    }

    if (output_tree_save_body(output->tree, fp, fmt) < 0) {
        ST_ERROR("Failed to output_tree_save_body.");
        return -1;
    }

    return 0;
}

int output_tree_dfs_trav_draw(output_tree_t *tree,
        output_node_id_t node, st_stack_t *stack, void *args)
{
    FILE *fp;
    void *tmp;
    output_node_id_t parent;

    ST_CHECK_PARAM(tree == NULL || stack == NULL || args == NULL, -1);

    fp = (FILE *)args;

    parent = OUTPUT_NODE_NONE;
    if (st_stack_top(stack, &tmp) == ST_STACK_OK) {
        parent = (output_node_id_t)(long)tmp;
    }

    if (parent != OUTPUT_NODE_NONE) {
        fprintf(fp, "    "OUTPUT_NODE_FMT" -> "OUTPUT_NODE_FMT";\n",
                parent, node);
    }

    return 0;
}

int output_draw(output_t *output, FILE *fp, count_t *word_cnts,
        st_alphabet_t *vocab)
{
    char sym[MAX_SYM_LEN];
    output_tree_dfs_aux_t *dfs_aux = NULL;
    int i;
    output_node_id_t m, n;

    ST_CHECK_PARAM(output == NULL || fp == NULL, -1);

    if (output->paths == NULL) {
        if (output_generate_path(output) < 0) {
            ST_ERROR("Failed to output_generate_path.");
            goto ERR;
        }
    }

    fprintf(fp, "digraph output {\n");
    fprintf(fp, "  rankdir=TB;\n");
    fprintf(fp, "  labelloc=t;\n");
    fprintf(fp, "  label=\"Output Tree\";\n\n");

    fprintf(fp, "  subgraph cluster_param {\n");
    fprintf(fp, "    label=\"Params\";\n");
    fprintf(fp, "    node [shape=plaintext, style=solid];\n");
    fprintf(fp, "    edge [style=invis];\n");
    fprintf(fp, "    legend [label=\"# Node: "OUTPUT_NODE_FMT"\\l"
                     "# Leaf: %d\\lmethod: %s\\l"
                     "max depth: %d\\lmax branch: %d\\l"
                     "Normalization: %s\"];\n",
                output->tree->num_node, output->output_size,
                method2str(output->output_opt.method),
                output->output_opt.max_depth,
                output->output_opt.max_branch,
                norm2str(output->norm));
    fprintf(fp, "  }\n\n");

    fprintf(fp, "  subgraph cluster_tree {\n");
    fprintf(fp, "    label=\"\"\n");
    fprintf(fp, "    graph [ranksep=0];\n");
    fprintf(fp, "    node [shape=record];\n\n");

    for (i = 0; i < output->output_size; i++) {
        n = output_tree_word2leaf(output->tree, i);
        fprintf(fp, "    "OUTPUT_NODE_FMT" [label=\"{{"OUTPUT_NODE_FMT"|",
                n, n);
        if (vocab) {
            fprintf(fp, "%s", escape_dot(sym, MAX_SYM_LEN,
                        st_alphabet_get_label(vocab, i)));
        }
        fprintf(fp, "}|{");
        if (word_cnts != NULL) {
            fprintf(fp, COUNT_FMT, word_cnts[i]);
        }
        fprintf(fp, "|");

        if (output->paths != NULL) {
            if (output->paths[i].num_node > 0) {
                for (m = 0; m < output->paths[i].num_node - 1; m++) {
                    fprintf(fp, OUTPUT_NODE_FMT",", output->paths[i].nodes[m]);
                }
                fprintf(fp, OUTPUT_NODE_FMT, output->paths[i].nodes[m]);
            }
        }
        fprintf(fp, "}}\"];\n");
    }
    fprintf(fp, "\n");

    dfs_aux = output_tree_dfs_aux_create(output->tree);
    if (dfs_aux == NULL) {
        ST_ERROR("Failed to output_tree_dfs_aux_create.");
        goto ERR;
    }

    if (output_tree_dfs(output->tree, dfs_aux,
                output_tree_dfs_trav_draw, fp) < 0) {
        ST_ERROR("Failed to output_tree_dfs.");
        goto ERR;
    }

    safe_output_tree_dfs_aux_destroy(dfs_aux);

    fprintf(fp, "  }\n");
    fprintf(fp, "}\n");

    return 0;

ERR:
    safe_output_tree_dfs_aux_destroy(dfs_aux);
    return -1;
}

layer_t* output_get_layer(output_t *output)
{
    layer_t *layer = NULL;

    ST_CHECK_PARAM(output == NULL, NULL);

    layer = (layer_t *)st_malloc(sizeof(layer_t));
    if (layer == NULL) {
        ST_ERROR("Failed to st_malloc layer.");
        goto ERR;
    }
    memset(layer, 0, sizeof(layer_t));

    strncat(layer->name, OUTPUT_LAYER_NAME, MAX_NAME_LEN - 1);
    strncat(layer->type, OUTPUT_LAYER_NAME, MAX_NAME_LEN - 1);
    layer->size = output->tree->num_node;

    return layer;
ERR:
    safe_st_free(layer);
    return NULL;
}

int output_parse_topo(output_t *output, const char *topo)
{
    char keyvalue[2*MAX_LINE_LEN];
    char token[MAX_LINE_LEN];
    const char *p;

    ST_CHECK_PARAM(output == NULL || topo == NULL, -1);

    p = topo;

    p = get_next_token(p, token);
    if (strcasecmp("property", token) != 0) {
        ST_ERROR("Not property line.");
        return -1;
    }

    while (p != NULL) {
        p = get_next_token(p, token);
        if (token[0] == '\0') {
            continue;
        }

        if (split_line(token, keyvalue, 2, MAX_LINE_LEN, "=") < 0) {
            ST_ERROR("Failed to split key/value. [%s]", token);
            return -1;
        }

        if (strcasecmp("norm", keyvalue) == 0) {
            output->norm = str2norm(keyvalue + MAX_LINE_LEN);
            if (output->norm == ON_UNKNOWN) {
                ST_ERROR("Unknown norm.");
                return -1;
            }
        } else {
            ST_ERROR("Unknown key[%s].", keyvalue);
        }
    }

    if (output_setup(output) < 0) {
        ST_ERROR("Failed to output_setup.");
        return -1;
    }

    return 0;
}

int output_walk_through_path(output_t *output, int word,
        int (*walker)(output_t *output, output_node_id_t node,
            output_node_id_t next_node,
            output_node_id_t child_s, output_node_id_t child_e, void *args),
        void *args)
{
    output_path_t *path;
    output_node_id_t node;
    output_node_id_t next_node;
    output_node_id_t n;

    ST_CHECK_PARAM(output == NULL || word < 0, -1);

    path = output->paths + word;

    node = output->tree->root;
    if (path->num_node > 0) {
        next_node = path->nodes[0];
    } else {
        next_node = output_tree_word2leaf(output->tree, word);
    }
    if (walker(output, node, next_node, s_children(output->tree, node),
                e_children(output->tree, node), args) < 0) {
        ST_ERROR("Failed to walker. node["OUTPUT_NODE_FMT"]", node);
        return -1;
    }
    for (n = 0; n < path->num_node; n++) {
        node = path->nodes[n];
        if (n < path->num_node - 1) {
            next_node = path->nodes[n + 1];
        } else {
            next_node = output_tree_word2leaf(output->tree, word);
        }
        if (walker(output, node, next_node, s_children(output->tree, node),
                    e_children(output->tree, node), args) < 0) {
            ST_ERROR("Failed to walker. node["OUTPUT_NODE_FMT"]", node);
            return -1;
        }
    }

    return 0;
}
