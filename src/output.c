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
#include <stutils/st_utils.h>

#include "utils.h"
#include "output.h"

static const int OUTPUT_MAGIC_NUM = 626140498 + 6;

static inline int output_hs_wt_w_pos(int cls, int s)
{
    if (cls < 0) {
        return 0;
    }

    /* Every class should contain n_c - 1 nodes */
    return s - cls;
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
        output_opt->method = TOP_DOWN;
        /* default value for class-based output. */
        def_depth = 1;
        def_branch = 100;
    } else if (strcasecmp(method_str, "bottomup") == 0
            || strcasecmp(method_str, "b") == 0
            || strcasecmp(method_str, "bu") == 0) {
        output_opt->method = BOTTOM_UP;
        /* default value for hierarchical softmax output. */
        def_depth = 0;
        def_branch = 2;
    } else {
        ST_WARNING("Unknown construcing method[%s].", method_str);
        goto ST_OPT_ERR;
    }

    ST_OPT_SEC_GET_INT(opt, sec_name, "MAX_DEPTH",
            output_opt->max_depth, def_depth,
            "Maximum depth of output tree");

    ST_OPT_SEC_GET_INT(opt, sec_name, "MAX_BRANCH",
            output_opt->max_branch, def_branch,
            "Maximum branch of output tree");
    if (output_opt->max_branch <= 1) {
        ST_WARNING("MAX_BRANCH must larger than 1.");
        goto ST_OPT_ERR;
    }

    return 0;

ST_OPT_ERR:
    return -1;
}

#define safe_tree_destroy(ptr) do {\
    if((ptr) != NULL) {\
        output_tree_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)

static void output_tree_destroy(output_tree_t *tree)
{
    if (tree == NULL) {
        return;
    }

    safe_free(tree->nodes);

    tree->cap_nodes = 0;
    tree->num_nodes = 0;
    tree->root = OUTPUT_NODE_NONE;
}

static output_tree_t* output_tree_init(output_node_id_t cap_nodes)
{
    output_tree_t *tree = NULL;
    size_t sz;

    ST_CHECK_PARAM(cap_nodes <= 0, NULL);

    sz = sizeof(output_tree_t);
    tree = (output_tree_t *)malloc(sz);
    if (tree == NULL) {
        ST_WARNING("Failed to malloc tree.");
        goto ERR;
    }
    memset(tree, 0, sz);

    sz = cap_nodes * sizeof(output_tree_node_t);
    tree->nodes = (output_tree_node_t *)malloc(sz);
    if (tree->nodes == NULL) {
        ST_WARNING("Failed to malloc nodes.");
        goto ERR;
    }

    tree->num_nodes = 0;
    tree->cap_nodes = cap_nodes;
    tree->root = OUTPUT_NODE_NONE;

    return tree;
ERR:
    safe_tree_destroy(tree);
    return NULL;
}

#define s_children(tree, node) tree->nodes[node].children_s
#define e_children(tree, node) tree->nodes[node].children_e
#define n_children(tree, node) e_children(tree, node) - s_children(tree, node)

#define NUM_REALLOC 100
static output_node_id_t output_tree_add_node(output_tree_t *tree)
{
    output_node_id_t node;

    ST_CHECK_PARAM(tree == NULL, OUTPUT_NODE_NONE);

    if (tree->num_nodes >= tree->cap_nodes) {
        tree->cap_nodes += NUM_REALLOC;
        tree->nodes = (output_tree_node_t *)realloc(tree->nodes,
                sizeof(output_tree_node_t) * tree->cap_nodes);
        if (tree->nodes == NULL) {
            ST_WARNING("Failed to realloc nodes.");
            return OUTPUT_NODE_NONE;
        }
    }

    node = tree->num_nodes;
    s_children(tree, node) = 0;
    e_children(tree, node) = 0;

    ++tree->num_nodes;

    return node;
}

static output_node_id_t output_tree_add_node_m(output_tree_t *tree,
        output_node_id_t n)
{
    output_node_id_t node;
    output_node_id_t i;

    ST_CHECK_PARAM(tree == NULL || n <= 0, OUTPUT_NODE_NONE);

    if (tree->num_nodes >= tree->cap_nodes) {
        tree->cap_nodes += n + NUM_REALLOC;
        tree->nodes = (output_tree_node_t *)realloc(tree->nodes,
                sizeof(output_tree_node_t) * tree->cap_nodes);
        if (tree->nodes == NULL) {
            ST_WARNING("Failed to realloc nodes.");
            return OUTPUT_NODE_NONE;
        }
    }

    node = tree->num_nodes;

    for (i = node; i < node + n; ++i) {
        s_children(tree, i) = 0;
        e_children(tree, i) = 0;
    }

    tree->num_nodes += n;

    return node;
}

void output_destroy(output_t *output)
{
    int i;

    if (output == NULL) {
        return;
    }

    safe_tree_destroy(output->tree);
    safe_free(output->paths);

    if (output->neurons != NULL) {
        for (i = 0; i < output->num_thrs; i++) {
            safe_free(output->neurons[i].ac_o_c);
            safe_free(output->neurons[i].er_o_c);
            safe_free(output->neurons[i].ac_o_w);
            safe_free(output->neurons[i].er_o_w);

            safe_free(output->neurons[i].wt_hs_c);
            safe_free(output->neurons[i].wt_hs_w);

            safe_free(output->neurons[i].ac_hs_c);
            safe_free(output->neurons[i].ac_hs_w);

            output->neurons[i].p_hs_c = -1.0;
            output->neurons[i].p_hs_w = -1.0;
        }
        safe_free(output->neurons);
    }
    output->num_thrs = 0;
}

output_t* output_dup(output_t *o)
{
    output_t *output = NULL;
    size_t sz;

    int class_size;
    int max_code_len;

    ST_CHECK_PARAM(o == NULL, NULL);

    output = (output_t *) malloc(sizeof(output_t));
    if (output == NULL) {
        ST_WARNING("Falied to malloc output_t.");
        goto ERR;
    }
    memset(output, 0, sizeof(output_t));

    output->output_opt = o->output_opt;
    *output = *o;

    class_size = o->output_opt.class_size;
    max_code_len = o->output_opt.max_code_len;

    if(class_size > 0) {
        sz = sizeof(int) * o->output_size;
        output->w2c = (int *) malloc(sz);
        if (output->w2c == NULL) {
            ST_WARNING("Failed to malloc w2c.");
            goto ERR;
        }
        memcpy(output->w2c, o->w2c, sz);

        sz = sizeof(int) * class_size;
        output->c2w_s = (int *) malloc(sz);
        if (output->c2w_s == NULL) {
            ST_WARNING("Failed to malloc c2w_s.");
            goto ERR;
        }
        memcpy(output->c2w_s, o->c2w_s, sz);

        output->c2w_e = (int *) malloc(sz);
        if (output->c2w_e == NULL) {
            ST_WARNING("Failed to malloc c2w_e.");
            goto ERR;
        }
        memcpy(output->c2w_e, o->c2w_e, sz);
    }

    if(o->output_opt.hs) {
        if(class_size > 0 && o->output_opt.class_hs) {
            sz = sizeof(char) * class_size * max_code_len;
            output->code_c = (char *) malloc(sz);
            if (output->code_c == NULL) {
                ST_WARNING("Failed to malloc code_c.");
                goto ERR;
            }
            memcpy(output->code_c, o->code_c, sz);

            sz = sizeof(int) * class_size * max_code_len;
            output->pt_c = (int *) malloc(sz);
            if (output->pt_c == NULL) {
                ST_WARNING("Failed to malloc pt_c.");
                goto ERR;
            }
            memcpy(output->pt_c, o->pt_c, sz);
        }

        sz = sizeof(char) * o->output_size * max_code_len;
        output->code_w = (char *) malloc(sz);
        if (output->code_w == NULL) {
            ST_WARNING("Failed to malloc code_w.");
            goto ERR;
        }
        memcpy(output->code_w, o->code_w, sz);

        sz = sizeof(int) * o->output_size * max_code_len;
        output->pt_w = (int *) malloc(sz);
        if (output->pt_w == NULL) {
            ST_WARNING("Failed to malloc pt_w.");
            goto ERR;
        }
        memcpy(output->pt_w, o->pt_w, sz);
    }
    return output;

ERR:
    safe_output_destroy(output);
    return NULL;
}

int output_load_header(output_t **output, int version,
        FILE *fp, bool *binary, FILE *fo_info)
{
    char sym[MAX_LINE_LEN];
    union {
        char str[4];
        int magic_num;
    } flag;

    int output_size;
    int class_size;
    int max_code_len = -1;
    int hs_input_size = 0;
    bool hs = false;
    bool class_hs = false;

    ST_CHECK_PARAM((output == NULL && fo_info == NULL) || fp == NULL
            || binary == NULL, -1);

    if (fread(&flag.magic_num, sizeof(int), 1, fp) != 1) {
        ST_WARNING("Failed to load magic num.");
        return -1;
    }

    if (strncmp(flag.str, "    ", 4) == 0) {
        *binary = false;
    } else if (OUTPUT_MAGIC_NUM != flag.magic_num) {
        ST_WARNING("magic num wrong.");
        return -2;
    } else {
        *binary = true;
    }

    if (output != NULL) {
        *output = NULL;
    }
    
    if (*binary) {
        if (fread(&output_size, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read output size.");
            return -1;
        }

        if (output_size <= 0) {
            if (fo_info != NULL) {
                fprintf(fo_info, "\n<OUTPUT>: None\n");
            }
            return 0;
        }

        if (fread(&class_size, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read class size.");
            return -1;
        }

        if (fread(&hs, sizeof(bool), 1, fp) != 1) {
            ST_WARNING("Failed to read hs.");
            return -1;
        }

        if (version >= 2) {
            if (hs) {
                if (fread(&max_code_len, sizeof(int), 1, fp) != 1) {
                    ST_WARNING("Failed to read max_code_len.");
                    return -1;
                }
                if (fread(&class_hs, sizeof(bool), 1, fp) != 1) {
                    ST_WARNING("Failed to read class_hs.");
                    return -1;
                }
                if (fread(&hs_input_size, sizeof(int), 1, fp) != 1) {
                    ST_WARNING("Failed to read hs_input_size.");
                    return -1;
                }
            }
        }
    } else {
        if (st_readline(fp, "") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }
        if (st_readline(fp, "<OUTPUT>") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }

        if (st_readline(fp, "Output size: %d", &output_size) != 1) {
            ST_WARNING("Failed to parse output_size.");
            goto ERR;
        }

        if (output_size <= 0) {
            if (fo_info != NULL) {
                fprintf(fo_info, "\n<OUTPUT>: None\n");
            }
            return 0;
        }

        if (st_readline(fp, "Class size: %d", &class_size) != 1) {
            ST_WARNING("Failed to parse class size.");
            return -1;
        }

        if (st_readline(fp, "HS: %s", sym) != 1) {
            ST_WARNING("Failed to parse hs.");
            return -1;
        }
        hs = str2bool(sym);

        if (version >= 2) {
            if (hs) {
                if (st_readline(fp, "\tMax code len: %d",
                            &max_code_len) != 1) {
                    ST_WARNING("Failed to parse max_code_len.");
                    return -1;
                }
                if (st_readline(fp, "\tClass HS: %s", sym) != 1) {
                    ST_WARNING("Failed to parse class_hs.");
                    return -1;
                }
                class_hs = str2bool(sym);

                if (st_readline(fp, "\tHS input size: %d",
                            &hs_input_size) != 1) {
                    ST_WARNING("Failed to parse hs_input_size.");
                    return -1;
                }
            }
        }
    }

    if (output != NULL) {
        *output = (output_t *)malloc(sizeof(output_t));
        if (*output == NULL) {
            ST_WARNING("Failed to malloc output_t");
            goto ERR;
        }
        memset(*output, 0, sizeof(output_t));

        (*output)->output_size = output_size;
        (*output)->output_opt.class_size = class_size;
        (*output)->output_opt.hs = hs;
        (*output)->output_opt.class_hs = class_hs;
        (*output)->output_opt.max_code_len = max_code_len;
        (*output)->hs_input_size = hs_input_size;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<OUTPUT>\n");
        fprintf(fo_info, "Output size: %d\n", output_size);
        fprintf(fo_info, "Class size: %d\n", class_size);
        fprintf(fo_info, "HS: %s\n", bool2str(hs));
        if (version >= 2) {
            if (hs) {
                fprintf(fo_info, "    Max code len: %d\n", max_code_len);
                fprintf(fo_info, "    Class HS: %s\n", bool2str(class_hs));
                fprintf(fo_info, "    HS input size: %d\n", hs_input_size);
            }
        }
    }

    return 0;

ERR:
    if (output != NULL) {
        safe_output_destroy(*output);
    }
    return -1;
}

int output_load_body(output_t *output, int version, FILE *fp, bool binary)
{
    size_t sz;
    int n;

    int i;
    int class_size;
    int max_code_len;
    int hs_input_size;
    int word_size = -1;

    int t;

    ST_CHECK_PARAM(output == NULL || fp == NULL, -1);

    output->w2c = NULL;
    output->c2w_s = NULL;
    output->c2w_e = NULL;

    output->code_c = NULL;
    output->pt_c = NULL;
    output->code_w = NULL;
    output->pt_w = NULL;

    if (output->output_size <= 0) {
        return 0;
    }

    class_size = output->output_opt.class_size;
    max_code_len = output->output_opt.max_code_len;
    hs_input_size = output->hs_input_size;

    if (class_size > 0) {
        sz = sizeof(int) * output->output_size;
        output->w2c = (int *) malloc(sz);
        if (output->w2c == NULL) {
            ST_WARNING("Failed to malloc w2c.");
            goto ERR;
        }
        memset(output->w2c, 0, sz);

        sz = sizeof(int) * class_size;
        output->c2w_s = (int *) malloc(sz);
        if (output->c2w_s == NULL) {
            ST_WARNING("Failed to malloc c2w_s.");
            goto ERR;
        }
        memset(output->c2w_s, 0, sz);

        output->c2w_e = (int *) malloc(sz);
        if (output->c2w_e == NULL) {
            ST_WARNING("Failed to malloc c2w_e.");
            goto ERR;
        }
        memset(output->c2w_e, 0, sz);
    }

    if(version >= 2 && output->output_opt.hs) {
        if(class_size > 0 && output->output_opt.class_hs) {
            sz = sizeof(char) * class_size * max_code_len;
            output->code_c = (char *) malloc(sz);
            if (output->code_c == NULL) {
                ST_WARNING("Failed to malloc code_c.");
                goto ERR;
            }
            memset(output->code_c, -1, sz);

            sz = sizeof(int) * class_size * max_code_len;
            output->pt_c = (int *) malloc(sz);
            if (output->pt_c == NULL) {
                ST_WARNING("Failed to malloc pt_c.");
                goto ERR;
            }
            memset(output->pt_c, -1, sz);

            sz = (class_size - 1) * hs_input_size;
            if (posix_memalign((void **)&output->wt_hs_c, ALIGN_SIZE,
                        sizeof(real_t) * sz) != 0
                    || output->wt_hs_c == NULL) {
                ST_WARNING("Failed to malloc wt_hs_c.");
                goto ERR;
            }
        }

        sz = sizeof(char) * output->output_size * max_code_len;
        output->code_w = (char *) malloc(sz);
        if (output->code_w == NULL) {
            ST_WARNING("Failed to malloc code_w.");
            goto ERR;
        }
        memset(output->code_w, -1, sz);

        sz = sizeof(int) * output->output_size * max_code_len;
        output->pt_w = (int *) malloc(sz);
        if (output->pt_w == NULL) {
            ST_WARNING("Failed to malloc pt_w.");
            goto ERR;
        }
        memset(output->pt_w, -1, sz);

        if (class_size > 0) {
            word_size = output->output_size - class_size;
        } else {
            word_size = output->output_size - 1;
        }
        sz = word_size * hs_input_size;
        if (posix_memalign((void **)&output->wt_hs_w, ALIGN_SIZE,
                    sizeof(real_t) * sz) != 0
                || output->wt_hs_w == NULL) {
            ST_WARNING("Failed to malloc wt_hs_w.");
            goto ERR;
        }
    }

    if (binary) {
        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read magic num.");
            goto ERR;
        }

        if (n != -OUTPUT_MAGIC_NUM) {
            ST_WARNING("Magic num error.");
            goto ERR;
        }

        if (class_size > 0) {
            if (fread(output->w2c, sizeof(int), output->output_size, fp)
                    != output->output_size) {
                ST_WARNING("Failed to read w2c.");
                goto ERR;
            }

            if (fread(output->c2w_s, sizeof(int), class_size, fp)
                    != class_size) {
                ST_WARNING("Failed to read c2w_s.");
                goto ERR;
            }

            if (fread(output->c2w_e, sizeof(int), class_size, fp)
                    != class_size) {
                ST_WARNING("Failed to read c2w_e.");
                goto ERR;
            }
        }

        if (version >= 2 && output->output_opt.hs) {
            if(class_size > 0 && output->output_opt.class_hs) {
                sz = class_size * max_code_len;
                if (fread(output->code_c, sizeof(char), sz, fp) != sz) {
                    ST_WARNING("Failed to read code_c.");
                    goto ERR;
                }

                sz = class_size * max_code_len;
                if (fread(output->pt_c, sizeof(int), sz, fp) != sz) {
                    ST_WARNING("Failed to read pt_c.");
                    goto ERR;
                }

                sz = (class_size - 1) * hs_input_size;
                if (fread(output->wt_hs_c, sizeof(real_t), sz, fp) != sz) {
                    ST_WARNING("Failed to read wt_hs_c.");
                    goto ERR;
                }
            }

            sz = output->output_size * max_code_len;
            if (fread(output->code_w, sizeof(char), sz, fp) != sz) {
                ST_WARNING("Failed to read code_w.");
                goto ERR;
            }

            sz = output->output_size * max_code_len;
            if (fread(output->pt_w, sizeof(int), sz, fp) != sz) {
                ST_WARNING("Failed to read pt_w.");
                goto ERR;
            }

            sz = word_size * hs_input_size;
            if (fread(output->wt_hs_w, sizeof(real_t), sz, fp) != sz) {
                ST_WARNING("Failed to read wt_hs_w.");
                goto ERR;
            }
        }
    } else {
        if (st_readline(fp, "<OUTPUT-DATA>") != 0) {
            ST_WARNING("body flag error.");
            goto ERR;
        }

        if (class_size > 0) {
            if (st_readline(fp, "Words to Classes:") != 0) {
                ST_WARNING("w2c flag error.");
                goto ERR;
            }
            for (i = 0; i < output->output_size; i++) {
                if (st_readline(fp, "\t%*d\t%d", output->w2c + i) != 1) {
                    ST_WARNING("Failed to parse w2c.");
                    goto ERR;
                }
            }

            if (st_readline(fp, "Classes to Words:") != 0) {
                ST_WARNING("c2w flag error.");
                goto ERR;
            }
            for (i = 0; i < class_size; i++) {
                if (st_readline(fp, "\t%*d\t%d\t%d", output->c2w_s + i,
                            output->c2w_e + i) != 2) {
                    ST_WARNING("Failed to parse c2w.");
                    goto ERR;
                }
            }
        }

        if (version >= 2 && output->output_opt.hs) {
            if(class_size > 0 && output->output_opt.class_hs) {
                if (st_readline(fp, "Class codes:") != 0) {
                    ST_WARNING("code_c flag error.");
                    goto ERR;
                }
                sz = class_size * max_code_len;
                for (i = 0; i < sz; i++) {
                    if (st_readline(fp, "\t%*d\t%d", &t) != 1) {
                        ST_WARNING("Failed to parse code_c.");
                        goto ERR;
                    }
                    output->code_c[i] = t;
                }

                if (st_readline(fp, "Class points:") != 0) {
                    ST_WARNING("code_c flag error.");
                    goto ERR;
                }
                sz = class_size * max_code_len;
                for (i = 0; i < sz; i++) {
                    if (st_readline(fp, "\t%*d\t%d", &t) != 1) {
                        ST_WARNING("Failed to parse pt_c.");
                        goto ERR;
                    }
                    output->pt_c[i] = t;
                }

                if (hs_input_size > 0) {
                    if (st_readline(fp, "Class HS weights:") != 0) {
                        ST_WARNING("wt_hs_c flag error.");
                        goto ERR;
                    }
                    sz = (class_size - 1) * hs_input_size;
                    for (i = 0; i < sz; i++) {
                        if (st_readline(fp, "\t%*d\t" REAL_FMT,
                                    output->wt_hs_c + i) != 1) {
                            ST_WARNING("Failed to parse wt_hs_c.");
                            goto ERR;
                        }
                    }
                }
            }

            if (st_readline(fp, "Word codes:") != 0) {
                ST_WARNING("code_w flag error.");
                goto ERR;
            }
            sz = output->output_size * max_code_len;
            for (i = 0; i < sz; i++) {
                if (st_readline(fp, "\t%*d\t%*d\t%d", &t) != 1) {
                    ST_WARNING("Failed to parse code_w.");
                    goto ERR;
                }
                output->code_w[i] = t;
            }

            if (st_readline(fp, "Word points:") != 0) {
                ST_WARNING("code_w flag error.");
                goto ERR;
            }
            sz = output->output_size * max_code_len;
            for (i = 0; i < sz; i++) {
                if (st_readline(fp, "\t%*d\t%*d\t%d", &t) != 1) {
                    ST_WARNING("Failed to parse pt_w.");
                    goto ERR;
                }
                output->pt_w[i] = t;
            }

            if (hs_input_size > 0) {
                if (st_readline(fp, "Word HS weights:") != 0) {
                    ST_WARNING("wt_hs_w flag error.");
                    goto ERR;
                }
                sz = word_size * hs_input_size;
                for (i = 0; i < sz; i++) {
                    if (st_readline(fp, "\t%*d\t%*d\t" REAL_FMT,
                                output->wt_hs_w + i) != 1) {
                        ST_WARNING("Failed to parse wt_hs_w.");
                        goto ERR;
                    }
                }
            }
        }
    }

    return 0;
ERR:
    safe_free(output->w2c);
    safe_free(output->c2w_s);
    safe_free(output->c2w_e);

    safe_free(output->code_c);
    safe_free(output->pt_c);
    safe_free(output->code_w);
    safe_free(output->pt_w);

    safe_free(output->wt_hs_c);
    safe_free(output->wt_hs_w);

    return -1;
}

int output_save_header(output_t *output, FILE *fp, bool binary)
{
    int n;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (binary) {
        if (fwrite(&OUTPUT_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }
        if (output == NULL) {
            n = 0;
            if (fwrite(&n, sizeof(int), 1, fp) != 1) {
                ST_WARNING("Failed to write output size.");
                return -1;
            }
            return 0;
        }

        if (fwrite(&output->output_size, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write output size.");
            return -1;
        }

        if (fwrite(&output->output_opt.class_size,
                    sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write class size.");
            return -1;
        }

        if (fwrite(&output->output_opt.hs, sizeof(bool), 1, fp) != 1) {
            ST_WARNING("Failed to write hs.");
            return -1;
        }

        // Version 2
        if (output->output_opt.hs) {
            if (fwrite(&output->output_opt.max_code_len, sizeof(int),
                        1, fp) != 1) {
                ST_WARNING("Failed to write max_code_len.");
                return -1;
            }

            if (fwrite(&output->output_opt.class_hs, sizeof(bool),
                        1, fp) != 1) {
                ST_WARNING("Failed to write class_hs.");
                return -1;
            }

            if (fwrite(&output->hs_input_size, sizeof(int), 1, fp) != 1) {
                ST_WARNING("Failed to write hs_input_size.");
                return -1;
            }
        }
    } else {
        fprintf(fp, "    \n<OUTPUT>\n");

        if (output == NULL) {
            fprintf(fp, "Output size: 0\n");
            return 0;
        }

        fprintf(fp, "Output size: %d\n", output->output_size);
        fprintf(fp, "Class size: %d\n", output->output_opt.class_size);
        fprintf(fp, "HS: %s\n", bool2str(output->output_opt.hs));
        // Version 2
        if (output->output_opt.hs) {
            fprintf(fp, "    Max code len: %d\n",
                    output->output_opt.max_code_len);
            fprintf(fp, "    Class HS: %s\n",
                    bool2str(output->output_opt.class_hs));
            fprintf(fp, "    HS input size: %d\n", output->hs_input_size);
        }
    }

    return 0;
}

int output_save_body(output_t *output, FILE *fp, bool binary)
{
    size_t sz;

    int class_size;
    int max_code_len;
    int hs_input_size;

    int n;

    int i;
    int t;
    int c, s, e;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (output == NULL) {
        return 0;
    }

    class_size = output->output_opt.class_size;
    max_code_len = output->output_opt.max_code_len;
    hs_input_size = output->hs_input_size;

    if (binary) {
        n = -OUTPUT_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        if (class_size > 0) {
            if (fwrite(output->w2c, sizeof(int), output->output_size, fp)
                    != output->output_size) {
                ST_WARNING("Failed to write w2c.");
                return -1;
            }

            if (fwrite(output->c2w_s, sizeof(int),
                        class_size, fp) != class_size) {
                ST_WARNING("Failed to write c2w_s.");
                return -1;
            }

            if (fwrite(output->c2w_e, sizeof(int),
                        class_size, fp) != class_size) {
                ST_WARNING("Failed to write c2w_e.");
                return -1;
            }
        }

        if (output->output_opt.hs) {
            if(class_size > 0 && output->output_opt.class_hs) {
                sz = class_size * max_code_len;
                if (fwrite(output->code_c, sizeof(char), sz, fp) != sz) {
                    ST_WARNING("Failed to write code_c.");
                    return -1;
                }

                sz = class_size * max_code_len;
                if (fwrite(output->pt_c, sizeof(int), sz, fp) != sz) {
                    ST_WARNING("Failed to write pt_c.");
                    return -1;
                }

                sz = (class_size - 1) * output->hs_input_size;
                if (fwrite(output->wt_hs_c, sizeof(real_t), sz, fp) != sz) {
                    ST_WARNING("Failed to write wt_hs_c.");
                    return -1;
                }
            }

            sz = output->output_size * max_code_len;
            if (fwrite(output->code_w, sizeof(char), sz, fp) != sz) {
                ST_WARNING("Failed to write code_w.");
                return -1;
            }

            sz = output->output_size * max_code_len;
            if (fwrite(output->pt_w, sizeof(int), sz, fp) != sz) {
                ST_WARNING("Failed to write pt_w.");
                return -1;
            }

            if (class_size > 0) {
                sz = (output->output_size - class_size);
            } else {
                sz = (output->output_size - 1);
            }
            sz = sz * output->hs_input_size;
            if (fwrite(output->wt_hs_w, sizeof(real_t), sz, fp) != sz) {
                ST_WARNING("Failed to write wt_hs_w.");
                return -1;
            }
        }
    } else {
        fprintf(fp, "<OUTPUT-DATA>\n");

        if (class_size > 0) {
            fprintf(fp, "Words to Classes:\n");
            for (i = 0; i < output->output_size; i++) {
                fprintf(fp, "\t%d\t%d\n", i, output->w2c[i]);
            }

            fprintf(fp, "Classes to Words:\n");
            for (i = 0; i < class_size; i++) {
                fprintf(fp, "\t%d\t%d\t%d\n", i, output->c2w_s[i],
                        output->c2w_e[i]);
            }
        }

        if (output->output_opt.hs) {
            if(class_size > 0 && output->output_opt.class_hs) {
                fprintf(fp, "Class codes:\n");
                sz = class_size * max_code_len;
                for (i = 0; i < sz; i++) {
                    t = output->code_c[i];
                    fprintf(fp, "\t%d\t%d\n", i / max_code_len, t);
                }

                fprintf(fp, "Class points:\n");
                sz = class_size * max_code_len;
                for (i = 0; i < sz; i++) {
                    t = output->pt_c[i];
                    fprintf(fp, "\t%d\t%d\n", i / max_code_len, t);
                }

                if (hs_input_size > 0) {
                    fprintf(fp, "Class HS weights:\n");
                    sz = (class_size  - 1) * hs_input_size;
                    for (i = 0; i < sz; i++) {
                        fprintf(fp, "\t%d\t" REAL_FMT "\n",
                                i / hs_input_size, output->wt_hs_c[i]);
                    }
                }
            }

            fprintf(fp, "Word codes:\n");
            if (class_size > 0) {
                for (c = 0; c < class_size; c++) {
                    s = output->c2w_s[c];
                    e = output->c2w_e[c];
                    sz = (e - s) * max_code_len;
                    for (i = 0; i < sz; i++) {
                        t = output->code_w[s * max_code_len + i];
                        fprintf(fp, "\t%d\t%d\t%d\n", c,
                                i / max_code_len, t);
                    }
                }
            } else {
                sz = output->output_size * max_code_len;
                for (i = 0; i < sz; i++) {
                    t = output->code_w[i];
                    fprintf(fp, "\t%d\t%d\t%d\n", -1, i / max_code_len, t);
                }
            }

            fprintf(fp, "Word points:\n");
            if (class_size > 0) {
                for (c = 0; c < class_size; c++) {
                    s = output->c2w_s[c];
                    e = output->c2w_e[c];
                    sz = (e - s) * max_code_len;
                    for (i = 0; i < sz; i++) {
                        t = output->pt_w[s * max_code_len + i];
                        fprintf(fp, "\t%d\t%d\t%d\n", c,
                                i / max_code_len, t);
                    }
                }
            } else {
                sz = output->output_size * max_code_len;
                for (i = 0; i < sz; i++) {
                    t = output->pt_w[i];
                    fprintf(fp, "\t%d\t%d\t%d\n", -1, i / max_code_len, t);
                }
            }

            if (hs_input_size > 0) {
                fprintf(fp, "Word HS weights:\n");
                if (class_size > 0) {
                    for (c = 0; c < class_size; c++) {
                        s = output->c2w_s[c];
                        e = output->c2w_e[c];
                        sz = (e - s - 1) * hs_input_size;
                        for (i = 0; i < sz; i++) {
                            fprintf(fp, "\t%d\t%d\t" REAL_FMT "\n", c,
                                i / hs_input_size,
                                output->wt_hs_w[output_hs_wt_w_pos(c, s)
                                    * hs_input_size + i]);
                        }
                    }
                } else {
                    sz = (output->output_size - 1) * hs_input_size;
                    for (i = 0; i < sz; i++) {
                        fprintf(fp, "\t%d\t%d\t" REAL_FMT "\n", -1,
                            i / hs_input_size, output->wt_hs_w[i]);
                    }
                }
            }
        }
    }

    return 0;
}

static int output_generate_class(output_t *output, int class_size,
        count_t *word_cnts)
{
    int i;
    int a;
    long b;
    double dd;
    double df;

    ST_CHECK_PARAM(output == NULL || class_size <= 0
            || word_cnts == NULL, -1);

    safe_free(output->w2c);
    safe_free(output->c2w_s);
    safe_free(output->c2w_e);

    output->w2c = (int *) malloc(sizeof(int) * output->output_size);
    if (output->w2c == NULL) {
        ST_WARNING("Failed to malloc w2c.");
        goto ERR;
    }
    memset(output->w2c, 0, sizeof(int) * output->output_size);

    output->c2w_s = (int *) malloc(sizeof(int) * class_size);
    if (output->c2w_s == NULL) {
        ST_WARNING("Failed to malloc c2w_s.");
        goto ERR;
    }
    memset(output->c2w_s, 0, sizeof(int) * class_size);

    output->c2w_e = (int *) malloc(sizeof(int) * class_size);
    if (output->c2w_e == NULL) {
        ST_WARNING("Failed to malloc c2w_e.");
        goto ERR;
    }
    memset(output->c2w_e, 0, sizeof(int) * class_size);

    a = 0;
    b = 0;
    dd = 0.0;
    df = 0.0;
    for (i = 0; i < output->output_size; i++) {
        b += word_cnts[i];
    }
    for (i = 0; i < output->output_size; i++) {
        dd += sqrt(word_cnts[i] / (double) b);
    }

    for (i = 0; i < output->output_size; i++) {
        df += sqrt(word_cnts[i] / (double) b) / dd;
        if (df > 1) {
            df = 1;
        }
        if (df > (a + 1) / (double) class_size) {
            output->w2c[i] = a;
            if (a < class_size - 1) {
                output->c2w_e[a] = i + 1;
                a++;
                output->c2w_s[a] = i + 1;
            }
        } else {
            output->w2c[i] = a;
        }
    }
    output->c2w_e[class_size - 1] = output->output_size;

    return 0;

ERR:
    safe_free(output->w2c);
    safe_free(output->c2w_s);
    safe_free(output->c2w_e);
    return -1;
}

/**
 * Generate binary tree for HS
 * @ingroup output
 * @param[in] cnts counts for leaf nodes. MUST be sorted descendingly.
 * @param[out] pts generated points with length at least n_node * max_code_len.
 * @param[out] codes generated codes with length at least n_node * max_code_len.
 * @param[in] n_node number of leaf nodes.
 * @param[in] max_code_len max length of code.
 * @return non-zero value if any error.
 */
static int output_hs_generate_tree(count_t *cnts, int *pts, char *codes,
        size_t n_node, int max_code_len)
{
    int *point = NULL;
    char *code = NULL;
    count_t *count = NULL;
    char *binary = NULL;
    int *parent = NULL;

    size_t sz;
    int i, j, k, min1i, min2i, pos1, pos2;

    ST_CHECK_PARAM(cnts == NULL || pts == NULL || codes == NULL
            || n_node <= 0 || max_code_len <= 0, -1);

    sz = sizeof(int) * max_code_len;
    point = (int *)malloc(sz);
    if (point == NULL) {
        ST_WARNING("Failed to malloc point.");
        goto ERR;
    }
    memset(point, -1, sz);

    sz = sizeof(char) * max_code_len;
    code = (char *)malloc(sz);
    if (code == NULL) {
        ST_WARNING("Failed to malloc code.");
        goto ERR;
    }
    memset(code, 0, sz);

    sz = sizeof(count_t) * (n_node * 2 - 1);
    count = (count_t *)malloc(sz);
    if (count == NULL) {
        ST_WARNING("Failed to malloc count.");
        goto ERR;
    }
    memset(count, 0, sz);

    sz = sizeof(char) * (n_node * 2 - 1);
    binary = (char *)malloc(sz);
    if (binary == NULL) {
        ST_WARNING("Failed to malloc binary.");
        goto ERR;
    }
    memset(binary, 0, sz);

    sz = sizeof(int) * (n_node * 2 - 1);
    parent = (int *)malloc(sz);
    if (parent == NULL) {
        ST_WARNING("Failed to malloc parent.");
        goto ERR;
    }
    memset(parent, 0, sz);

    for (i = 0; i < n_node; i++) {
        count[i] = cnts[i];
    }

    for (i = n_node; i < n_node * 2 - 1; i++) {
        count[i] = COUNT_MAX;
    }

    pos1 = n_node - 1;
    pos2 = n_node;

    // Construct Huffman Tree
    for (i = 0; i < n_node - 1; i++) {
        if (pos1 >= 0) {
            if (count[pos1] < count[pos2]) {
                min1i = pos1;
                pos1--;
            } else {
                min1i = pos2;
                pos2++;
            }
        } else {
            min1i = pos2;
            pos2++;
        }
        if (pos1 >= 0) {
            if (count[pos1] < count[pos2]) {
                min2i = pos1;
                pos1--;
            } else {
                min2i = pos2;
                pos2++;
            }
        } else {
            min2i = pos2;
            pos2++;
        }

        count[n_node + i] = count[min1i] + count[min2i];
        parent[min1i] = n_node + i;
        parent[min2i] = n_node + i;
        binary[min2i] = 1;
    }

    // Assign binary code to each leaf node
    for (i = 0; i < n_node; i++) {
        j = i;
        k = 0;
        while (1) {
            if (k >= max_code_len) {
                ST_WARNING("Too deep tree.[%d/%d].", k, max_code_len);
                goto ERR;
            }
            code[k] = binary[j];
            point[k] = j;
            k++;
            j = parent[j];
            if (j == n_node * 2 - 2) break;
        }
        pts[i * max_code_len] = n_node - 2;
        for (j = 0; j < k; j++) {
            codes[i * max_code_len + k - j - 1] = code[j];
            if (j == 0 && k < max_code_len) {
                pts[i * max_code_len + k - j] = -1;
            } else {
                pts[i * max_code_len + k - j] = point[j] - n_node;
            }
        }
    }

    safe_free(code);
    safe_free(point);
    safe_free(count);
    safe_free(binary);
    safe_free(parent);

    return 0;

ERR:
    safe_free(code);
    safe_free(point);
    safe_free(count);
    safe_free(binary);
    safe_free(parent);

    return -1;
}

static count_t* output_count_class(output_t *output, count_t *word_cnts)
{
    count_t *cls_cnts = NULL;

    size_t sz;

    int class_size;
    int c;
    int s;
    int e;
    int w;

    ST_CHECK_PARAM(output == NULL || word_cnts == NULL, NULL);

    class_size = output->output_opt.class_size;

    sz = sizeof(count_t) * class_size;
    cls_cnts = (count_t *)malloc(sz);
    if (cls_cnts == NULL) {
        ST_WARNING("Failed to malloc cls_cnts.");
        goto ERR;
    }
    memset(cls_cnts, 0, sz);

    for (c = 0; c < class_size; c++) {
        s = output->c2w_s[c];
        e = output->c2w_e[c];

        for (w = s; w < e; w++) {
            cls_cnts[c] += word_cnts[w];
        }
    }

    return cls_cnts;

ERR:
    safe_free(cls_cnts);
    return NULL;
}

static int output_generate_hs(output_t *output, count_t *word_cnts)
{
    count_t *cls_cnts = NULL;

    size_t sz;

    int class_size;
    int max_code_len;

    int c, s, e;

    ST_CHECK_PARAM(output == NULL || word_cnts == NULL, -1);

    safe_free(output->code_c);
    safe_free(output->pt_c);
    safe_free(output->code_w);
    safe_free(output->pt_w);

    class_size = output->output_opt.class_size;
    max_code_len = output->output_opt.max_code_len;

    if(class_size > 0 && output->output_opt.class_hs) {
        sz = sizeof(char) * class_size * max_code_len;
        output->code_c = (char *) malloc(sz);
        if (output->code_c == NULL) {
            ST_WARNING("Failed to malloc code_c.");
            goto ERR;
        }
        memset(output->code_c, -1, sz);

        sz = sizeof(int) * class_size * max_code_len;
        output->pt_c = (int *) malloc(sz);
        if (output->pt_c == NULL) {
            ST_WARNING("Failed to malloc pt_c.");
            goto ERR;
        }
        memset(output->pt_c, -1, sz);

        cls_cnts = output_count_class(output, word_cnts);
        if (cls_cnts == NULL) {
            ST_WARNING("Failed to output_count_class.");
            goto ERR;
        }

        /* cls_cnts was almost descending. */
        if (output_hs_generate_tree(cls_cnts, output->pt_c,
                    output->code_c, class_size, max_code_len) < 0) {
            ST_WARNING("Failed to output_hs_generate_tree for class.");
            goto ERR;
        }

        safe_free(cls_cnts);
    }

    sz = sizeof(char) * output->output_size * max_code_len;
    output->code_w = (char *) malloc(sz);
    if (output->code_w == NULL) {
        ST_WARNING("Failed to malloc code_w.");
        goto ERR;
    }
    memset(output->code_w, -1, sz);

    sz = sizeof(int) * output->output_size * max_code_len;
    output->pt_w = (int *) malloc(sz);
    if (output->pt_w == NULL) {
        ST_WARNING("Failed to malloc pt_w.");
        goto ERR;
    }
    memset(output->pt_w, -1, sz);

    if(class_size > 0) {
        for (c = 0; c < class_size; c++) {
            s = output->c2w_s[c];
            e = output->c2w_e[c];

            if (output_hs_generate_tree(word_cnts + s,
                        output->pt_w + s * max_code_len,
                        output->code_w + s * max_code_len,
                        e - s, max_code_len) < 0) {
                ST_WARNING("Failed to output_hs_generate_tree for word.");
                goto ERR;
            }
        }
    } else {
        if (output_hs_generate_tree(word_cnts, output->pt_w,
                    output->code_w, output->output_size,
                    max_code_len) < 0) {
            ST_WARNING("Failed to output_hs_generate_tree for word.");
            goto ERR;
        }
    }

    return 0;

ERR:
    safe_free(output->code_c);
    safe_free(output->pt_c);
    safe_free(output->code_w);
    safe_free(output->pt_w);

    safe_free(cls_cnts);

    return -1;
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

    ST_CHECK_PARAM(output == NULL || word_cnts == NULL, -1);

    start = s_children(output->tree, node);
    end = e_children(output->tree, node);
    branches = output->output_opt.max_branch;

    if (end - start <= branches) {
        return 0;
    }

    if (start >= output->output_size) {
        ST_WARNING("parent of non-leaf node can not be splited.");
        return -1;
    }

    new_node = output_tree_add_node_m(output->tree, branches);
    if (new_node == OUTPUT_NODE_NONE) {
        ST_WARNING("Failed to output_tree_add_node_m.");
        return -1;
    }

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

    ST_CHECK_PARAM(output == NULL || word_cnts == NULL, -1);

    node = output_tree_add_node(output->tree);
    if (node == OUTPUT_NODE_NONE) {
        ST_WARNING("Failed to output_tree_add_node.");
        return -1;
    }
    s_children(output->tree, node) = 0;
    e_children(output->tree, node) = output->output_size;

    output->tree->root = node;

    for (; node < output->tree->num_nodes; ++node) {
        if (output_gen_td_split(output, node, word_cnts) < 0) {
            ST_WARNING("Failed to output_gen_td_split.");
            return -1;
        }
    }

    return 0;
}

output_t* output_generate(output_opt_t *output_opt, count_t *word_cnts,
       int output_size)
{
    output_t *output = NULL;
    size_t sz;
    output_node_id_t cap_nodes;

    ST_CHECK_PARAM(output_opt == NULL || word_cnts == NULL
            || output_size <= 0, NULL);

    output = (output_t *) malloc(sizeof(output_t));
    if (output == NULL) {
        ST_WARNING("Falied to malloc output_t.");
        goto ERR;
    }
    memset(output, 0, sizeof(output_t));

    output->output_opt = *output_opt;
    output->output_size = output_size;

    cap_nodes = output->output_size;
    cap_nodes += (cap_nodes - 1)/(output->output_opt->max_branch - 1) + 1;

    output->tree = output_tree_init(cap_nodes);
    if (tree == NULL) {
        ST_WARNING("Failed to output_tree_init.");
        goto ERR;
    }

    /* init leaf nodes */
    if ((output_tree_add_node_m(output->tree, output->output_size))
            == OUTPUT_NODE_NONE) {
        ST_WARNING("Failed to output_tree_add_node_m.");
        goto ERR;
    }

    sz = output->output_size * sizeof(output_tree_path_t);
    output->paths = (output_tree_path_t *)malloc(sz);
    if (output->paths == NULL) {
        ST_WARNING("Failed to malloc paths.");
        goto ERR;
    }
    memset(output->paths, 0, sz);

    switch (output->output_opt.method) {
        case TOP_DOWN:
            if (output_gen_td(output, word_cnts) < 0) {
                ST_WARNING("Failed to output_gen_td.");
                goto ERR;
            }
            break;
        case BOTTOM_UP:
            if (output_gen_bu(output, word_cnts) < 0) {
                ST_WARNING("Failed to output_gen_bu.");
                goto ERR;
            }
            break;
        default:
            ST_WARNING("Unknown constructing method[%d]",
                    output->output_opt.method);
            break;
    }

    return output;

  ERR:
    safe_output_destroy(output);
    return NULL;
}

int output_hs_init(output_t *output, int hs_input_size)
{
    size_t i;
    size_t sz;

    int class_size;
    int word_size;

    ST_CHECK_PARAM(output == NULL || hs_input_size < 0, -1);

    safe_free(output->wt_hs_c);
    safe_free(output->wt_hs_w);

    if (!output->output_opt.hs) {
        return 0;
    }

    output->hs_input_size = hs_input_size;
    class_size = output->output_opt.class_size;

    if (class_size > 0) { 
        if (output->output_opt.class_hs) {
            sz = (class_size - 1) * hs_input_size;
            if (posix_memalign((void **)&output->wt_hs_c, ALIGN_SIZE,
                        sizeof(real_t) * sz) != 0
                    || output->wt_hs_c == NULL) {
                ST_WARNING("Failed to malloc wt_hs_c.");
                goto ERR;
            }

            for (i = 0; i < sz; i++) {
                output->wt_hs_c[i] = st_random(-0.1, 0.1)
                    + st_random(-0.1, 0.1) + st_random(-0.1, 0.1);
            }
        }

        word_size = output->output_size - class_size;
    } else {
        word_size = output->output_size - 1;
    }

    sz = word_size * hs_input_size;
    if (posix_memalign((void **)&output->wt_hs_w, ALIGN_SIZE,
                sizeof(real_t) * sz) != 0
            || output->wt_hs_w == NULL) {
        ST_WARNING("Failed to malloc wt_hs_w.");
        goto ERR;
    }

    for (i = 0; i < sz; i++) {
        output->wt_hs_w[i] = st_random(-0.1, 0.1)
            + st_random(-0.1, 0.1) + st_random(-0.1, 0.1);
    }

    return 0;

ERR:
    safe_free(output->wt_hs_c);
    safe_free(output->wt_hs_w);

    return -1;
}

static int output_hs_forward(real_t *p, real_t *in, real_t *wt,
        int input_size, int *pts, char *codes, int max_code_len,
        real_t *ac)
{
    double d;
    int c;

    ST_CHECK_PARAM(p == NULL || in == NULL || wt == NULL
            || input_size <= 0 || pts == NULL || codes == NULL
            || max_code_len <= 0 || ac == NULL, -1);

    d = 1.0;
    c = 0;
    while (pts[c] > 0 && c < max_code_len) {
        ac[c] = dot_product(ac, wt + pts[c] * input_size, input_size);

        sigmoid(ac + c, 1);

        if (codes[c] == 0) {
            d *= ac[c];
        } else {
            d *= (1 - ac[c]);
        }

        c++;
    }

    *p = d;

    return 0;
}

int output_activate_pre_layer(output_t *output, int cls, int tid)
{
    output_neuron_t *neu;

    ST_CHECK_PARAM(output == NULL || tid < 0, -1);

    neu = output->neurons + tid;
    if (output->output_opt.class_size > 0) {
        if (output->output_opt.hs && output->output_opt.class_hs) {
            if (cls < 0) {
                ST_WARNING("class not specified.");
                return -1;
            }
            if (output_hs_forward(&neu->p_hs_c, neu->ac_o_c,
                    output->wt_hs_c,
                    output->hs_input_size,
                    output->pt_c + cls * output->output_opt.max_code_len,
                    output->code_c + cls * output->output_opt.max_code_len,
                    output->output_opt.max_code_len,
                    neu->ac_hs_c) < 0) {
                ST_WARNING("Failed to output_hs_forward class");
                return -1;
            }
        } else {
            softmax(neu->ac_o_c, output->output_opt.class_size);
        }
    }

    return 0;
}

int output_activate_last_layer(output_t *output, int word, int tid)
{
    output_neuron_t *neu;

    int cls;
    int s;
    int e;

    ST_CHECK_PARAM(output == NULL || word < 0 || tid < 0, -1);

    neu = output->neurons + tid;

    if (output->output_opt.class_size > 0) {
        cls = output->w2c[word];
        s = output->c2w_s[cls];
        e = output->c2w_e[cls];
    } else {
        cls = -1;
        s = 0;
        e = output->output_size;
    }

    if (output->output_opt.hs) {
        if (output_hs_forward(&neu->p_hs_w, neu->ac_o_w + s,
                output->wt_hs_w + output->hs_input_size
                               * output_hs_wt_w_pos(cls, s),
                output->hs_input_size,
                output->pt_w + word * output->output_opt.max_code_len,
                output->code_w + word * output->output_opt.max_code_len,
                output->output_opt.max_code_len,
                neu->ac_hs_w) < 0) {
            ST_WARNING("Failed to output_hs_forward word");
            return -1;
        }
    } else {
        softmax(neu->ac_o_w + s, e - s);
    }

    return 0;
}

int output_loss(output_t *output, int word, int tid)
{
    output_neuron_t *neu;

    int c;
    int s;
    int e;

    int o;

    ST_CHECK_PARAM(output == NULL || tid < 0, -1);

    if (word < 0) {
        return 0;
    }

    neu = output->neurons + tid;
    if (output->output_opt.class_size > 0) {
        c = output->w2c[word];
        s = output->c2w_s[c];
        e = output->c2w_e[c];

        for (o = 0; o < output->output_opt.class_size; o++) {
            neu->er_o_c[o] = (0 - neu->ac_o_c[o]);
        }
        neu->er_o_c[c] = (1 - neu->ac_o_c[c]);
    } else {
        s = 0;
        e = output->output_size;
    }

    for (o = s; o < e; o++) {
        neu->er_o_w[o] = (0 - neu->ac_o_w[o]);
    }
    neu->er_o_w[word] = (1 - neu->ac_o_w[word]);

    return 0;
}

double output_get_prob(output_t *output, int word, int tid)
{
    output_neuron_t *neu;
    double p = 1;

    neu = output->neurons + tid;

    if (output->output_opt.hs) {
        if (output->output_opt.class_size > 0) {
            p *= neu->p_hs_c;
        }
        p *= neu->p_hs_w;
    } else {
        if (output->output_opt.class_size > 0) {
            p *= neu->ac_o_c[output->w2c[word]];
        }

        p *= neu->ac_o_w[word];
    }

    return p;
}

double output_get_class_prob_for_class(output_t *output, int cls, int tid)
{
    output_neuron_t *neu;

    neu = output->neurons + tid;

    if (output->output_opt.class_size > 0) {
        if (output->output_opt.hs) {
            return neu->p_hs_c;
        } else {
            return neu->ac_o_c[cls];
        }
    }

    return 1.0;
}

double output_get_class_prob(output_t *output, int word, int tid)
{
    output_neuron_t *neu;

    neu = output->neurons + tid;

    if (output->output_opt.class_size > 0) {
        if (output->output_opt.hs) {
            return neu->p_hs_c;
        } else {
            return neu->ac_o_c[output->w2c[word]];
        }
    }

    return 1.0;
}

double output_get_word_prob(output_t *output, int word, int tid)
{
    output_neuron_t *neu;

    neu = output->neurons + tid;

    if (output->output_opt.hs) {
        return neu->p_hs_w;
    } else {
        return neu->ac_o_w[word];
    }
}

int output_setup_train(output_t *output, int num_thrs)
{
    output_neuron_t *neu;
    size_t sz;

    int class_size;
    int word_size;
    int max_code_len;

    int i;
    int t;

    ST_CHECK_PARAM(output == NULL || num_thrs < 0, -1);

    output->num_thrs = num_thrs;
    sz = sizeof(output_neuron_t) * num_thrs;
    output->neurons = (output_neuron_t *)malloc(sz);
    if (output->neurons == NULL) {
        ST_WARNING("Failed to malloc neurons.");
        goto ERR;
    }
    memset(output->neurons, 0, sz);

    max_code_len = output->output_opt.max_code_len;
    class_size = output->output_opt.class_size;
    if (class_size > 0) {
        if (output->output_opt.class_hs) {
            class_size = output->hs_input_size;
        }

        for (t = 0; t < num_thrs; t++) {
            neu = output->neurons + t;
            if (posix_memalign((void **)&neu->ac_o_c, ALIGN_SIZE,
                        sizeof(real_t) * class_size) != 0
                    || neu->ac_o_c == NULL) {
                ST_WARNING("Failed to malloc ac_o_c.");
                goto ERR;
            }

            if (posix_memalign((void **)&neu->er_o_c, ALIGN_SIZE,
                        sizeof(real_t) * class_size) != 0
                    || neu->er_o_c == NULL) {
                ST_WARNING("Failed to malloc er_o_c.");
                goto ERR;
            }

            for (i = 0; i < class_size; i++) {
                neu->ac_o_c[i] = 0;
                neu->er_o_c[i] = 0;
            }

            if (output->output_opt.hs && output->output_opt.class_hs) {
                if (posix_memalign((void **)&neu->ac_hs_c, ALIGN_SIZE,
                            sizeof(real_t) * max_code_len) != 0
                        || neu->ac_hs_c == NULL) {
                    ST_WARNING("Failed to malloc ac_hs_c.");
                    goto ERR;
                }

                for (i = 0; i < max_code_len; i++) {
                    neu->ac_hs_c[i] = 0;
                }

                neu->p_hs_c = -1.0;
            }
        }
    }

    if (output->output_opt.hs) {
        word_size = output->hs_input_size;
    } else {
        word_size = output->output_size;
    }

    for (t = 0; t < num_thrs; t++) {
        neu = output->neurons + t;

        if (posix_memalign((void **)&neu->ac_o_w, ALIGN_SIZE,
                    sizeof(real_t) * word_size) != 0
                || neu->ac_o_w == NULL) {
            ST_WARNING("Failed to malloc ac_o_w.");
            goto ERR;
        }

        if (posix_memalign((void **)&neu->er_o_w, ALIGN_SIZE,
                    sizeof(real_t) * word_size) != 0
                || neu->er_o_w == NULL) {
            ST_WARNING("Failed to malloc er_o_w.");
            goto ERR;
        }

        for (i = 0; i < word_size; i++) {
            neu->ac_o_w[i] = 0;
            neu->er_o_w[i] = 0;
        }

        if (output->output_opt.hs) {
            if (posix_memalign((void **)&neu->ac_hs_w, ALIGN_SIZE,
                        sizeof(real_t) * max_code_len) != 0
                    || neu->ac_hs_w == NULL) {
                ST_WARNING("Failed to malloc ac_hs_w.");
                goto ERR;
            }

            for (i = 0; i < max_code_len; i++) {
                neu->ac_hs_w[i] = 0;
            }

            neu->p_hs_w = -1.0;
        }
    }

    return 0;

ERR:
    if (output->neurons != NULL) {
        for (i = 0; i < output->num_thrs; i++) {
            safe_free(output->neurons[i].ac_o_c);
            safe_free(output->neurons[i].er_o_c);
            safe_free(output->neurons[i].ac_o_w);
            safe_free(output->neurons[i].er_o_w);

            safe_free(output->neurons[i].wt_hs_c);
            safe_free(output->neurons[i].wt_hs_w);

            safe_free(output->neurons[i].ac_hs_c);
            safe_free(output->neurons[i].ac_hs_w);

            output->neurons[i].p_hs_c = -1.0;
            output->neurons[i].p_hs_w = -1.0;
        }
        safe_free(output->neurons);
    }
    output->num_thrs = 0;

    return -1;
}

int output_reset_train(output_t *output, int tid)
{
#if 0
    int class_size;

    int i;

    ST_CHECK_PARAM(output == NULL, -1);

    class_size = output->output_opt.class_size;
    if (class_size > 0) {
        for (i = 0; i < class_size; i++) {
            output->ac_o_c[i] = 0;
            output->er_o_c[i] = 0;
        }
    }

    for (i = 0; i < output->output_size; i++) {
        output->ac_o_w[i] = 0;
        output->er_o_w[i] = 0;
    }
#endif

    return 0;
}

int output_start_train(output_t *output, int word, int tid)
{
    output_neuron_t *neu;
    int class_size;

    int c;
    int s;
    int e;

    int i;

    ST_CHECK_PARAM(output == NULL || tid < 0, -1);

    if (word < 0) {
        return 0;
    }

    neu = output->neurons + tid;
    class_size = output->output_opt.class_size;
    if (class_size > 0) {
        for (i = 0; i < class_size; i++) {
            neu->ac_o_c[i] = 0;
            neu->er_o_c[i] = 0;
        }

        c = output->w2c[word];
        s = output->c2w_s[c];
        e = output->c2w_e[c];
    } else {
        s = 0;
        e = output->output_size;
    }

    for (i = s; i < e; i++) {
        neu->ac_o_w[i] = 0;
        neu->er_o_w[i] = 0;
    }

    return 0;
}

int output_end_train(output_t *output, int word, int tid)
{
    ST_CHECK_PARAM(output == NULL, -1);

    return 0;
}

int output_finish_train(output_t *output, int tid)
{
    ST_CHECK_PARAM(output == NULL, -1);

    return 0;
}

int output_setup_test(output_t *output, int num_thrs)
{
    output_neuron_t *neu;
    size_t sz;

    int class_size;
    int max_code_len;

    int i;
    int t;

    ST_CHECK_PARAM(output == NULL || num_thrs < 0, -1);

    output->num_thrs = num_thrs;
    sz = sizeof(output_neuron_t) * num_thrs;
    output->neurons = (output_neuron_t *)malloc(sz);
    if (output->neurons == NULL) {
        ST_WARNING("Failed to malloc output neurons.");
        goto ERR;
    }
    memset(output->neurons, 0, sz);

    class_size = output->output_opt.class_size;
    max_code_len = output->output_opt.max_code_len;

    for (t = 0; t < num_thrs; t++) {
        neu = output->neurons + t;

        if (class_size > 0) {
            if (posix_memalign((void **)&neu->ac_o_c, ALIGN_SIZE,
                        sizeof(real_t) * class_size) != 0
                    || neu->ac_o_c == NULL) {
                ST_WARNING("Failed to malloc ac_o_c.");
                goto ERR;
            }

            for (i = 0; i < class_size; i++) {
                neu->ac_o_c[i] = 0;
            }

            if (output->output_opt.hs && output->output_opt.class_hs) {
                if (posix_memalign((void **)&neu->ac_hs_c, ALIGN_SIZE,
                            sizeof(real_t) * max_code_len) != 0
                        || neu->ac_hs_c == NULL) {
                    ST_WARNING("Failed to malloc ac_hs_c.");
                    goto ERR;
                }

                for (i = 0; i < max_code_len; i++) {
                    neu->ac_hs_c[i] = 0;
                }

                neu->p_hs_c = -1.0;
            }
        }

        if (posix_memalign((void **)&neu->ac_o_w, ALIGN_SIZE,
                    sizeof(real_t) * output->output_size) != 0
                || neu->ac_o_w == NULL) {
            ST_WARNING("Failed to malloc ac_o_w.");
            goto ERR;
        }

        for (i = 0; i < output->output_size; i++) {
            neu->ac_o_w[i] = 0;
        }

        if (output->output_opt.hs) {
            if (posix_memalign((void **)&neu->ac_hs_w, ALIGN_SIZE,
                        sizeof(real_t) * max_code_len) != 0
                    || neu->ac_hs_w == NULL) {
                ST_WARNING("Failed to malloc ac_hs_w.");
                goto ERR;
            }

            for (i = 0; i < max_code_len; i++) {
                neu->ac_hs_w[i] = 0;
            }

            neu->p_hs_w = -1.0;
        }
    }

    return 0;

ERR:
    if (output->neurons != NULL) {
        for (i = 0; i < output->num_thrs; i++) {
            safe_free(output->neurons[i].ac_o_c);
            safe_free(output->neurons[i].er_o_c);
            safe_free(output->neurons[i].ac_o_w);
            safe_free(output->neurons[i].er_o_w);

            safe_free(output->neurons[i].wt_hs_c);
            safe_free(output->neurons[i].wt_hs_w);

            safe_free(output->neurons[i].ac_hs_c);
            safe_free(output->neurons[i].ac_hs_w);

            output->neurons[i].p_hs_c = -1.0;
            output->neurons[i].p_hs_w = -1.0;
        }
        safe_free(output->neurons);
    }
    output->num_thrs = 0;

    return -1;
}

int output_reset_test(output_t *output, int tid)
{
#if 0
    int class_size;

    int i;

    ST_CHECK_PARAM(output == NULL, -1);

    class_size = output->output_opt.class_size;
    if (class_size > 0) {
        for (i = 0; i < class_size; i++) {
            output->ac_o_c[i] = 0;
        }
    }

    for (i = 0; i < output->output_size; i++) {
        output->ac_o_w[i] = 0;
    }
#endif

    return 0;
}

int output_start_test(output_t *output, int word, int tid)
{
    output_neuron_t *neu;
    int class_size;

    int c;
    int s;
    int e;

    int i;

    ST_CHECK_PARAM(output == NULL || tid < 0, -1);

    if (word < 0) {
        return 0;
    }

    neu = output->neurons + tid;
    class_size = output->output_opt.class_size;
    if (class_size > 0) {
        for (i = 0; i < class_size; i++) {
            neu->ac_o_c[i] = 0;
        }

        c = output->w2c[word];
        s = output->c2w_s[c];
        e = output->c2w_e[c];
    } else {
        s = 0;
        e = output->output_size;
    }

    for (i = s; i < e; i++) {
        neu->ac_o_w[i] = 0;
    }

    return 0;
}

int output_end_test(output_t *output, int word, int tid)
{
    ST_CHECK_PARAM(output == NULL, -1);

    return 0;
}

int output_setup_gen(output_t *output)
{
    return output_setup_test(output, 1);
}

int output_reset_gen(output_t *output)
{
    return output_reset_test(output, 0);
}

int output_end_gen(output_t *output, int word)
{
    return output_end_test(output, word, 0);
}

bool output_equal(output_t *output1, output_t *output2)
{
    int i;

    ST_CHECK_PARAM(output1 == NULL || output2 == NULL, false);

    if (output1->output_size != output2->output_size) {
        return false;
    }

    if (output1->output_opt.class_size != output2->output_opt.class_size) {
        return false;
    }

    if (output1->output_opt.class_size > 0) {
        for (i = 0; i < output1->output_size; i++) {
            if (output1->w2c[i] != output2->w2c[i]) {
                return false;
            }
        }

        for (i = 0; i < output1->output_opt.class_size; i++) {
            if (output1->c2w_s[i] != output2->c2w_s[i]) {
                return false;
            }
            if (output1->c2w_e[i] != output2->c2w_e[i]) {
                return false;
            }
        }
    }

    if (output1->output_opt.hs != output2->output_opt.hs) {
        return false;
    }

    if (output1->output_opt.hs) {
    }

    return true;
}
