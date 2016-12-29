/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Wang Jian
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

#include <string.h>

#include <stutils/st_macro.h>
#include <stutils/st_log.h>
#include <stutils/st_rand.h>
#include <stutils/st_mem.h>

#include "utils.h"
#include "output_updater.h"

void out_updater_destroy(out_updater_t *out_updater)
{
    if (out_updater == NULL) {
        return;
    }

    safe_st_aligned_free(out_updater->ac);
    safe_st_aligned_free(out_updater->er);

    out_updater->output = NULL;
}

out_updater_t* out_updater_create(output_t *output)
{
    out_updater_t *out_updater = NULL;

    ST_CHECK_PARAM(output == NULL, NULL);

    out_updater = (out_updater_t *)malloc(sizeof(out_updater_t));
    if (out_updater == NULL) {
        ST_WARNING("Failed to malloc out_updater.");
        goto ERR;
    }
    memset(out_updater, 0, sizeof(out_updater_t));

    out_updater->output = output;

    return out_updater;

ERR:
    safe_out_updater_destroy(out_updater);
    return NULL;
}

int out_updater_setup(out_updater_t *out_updater, bool backprop)
{
    size_t sz;

    ST_CHECK_PARAM(out_updater == NULL, -1);

    safe_st_aligned_free(out_updater->ac);
    safe_st_aligned_free(out_updater->er);

    sz = sizeof(real_t) * out_updater->output->tree->num_node;
    out_updater->ac = st_aligned_malloc(sz, ALIGN_SIZE);
    if (out_updater->ac == NULL) {
        ST_WARNING("Failed to st_aligned_malloc ac.");
        goto ERR;
    }
    memset(out_updater->ac, 0, sz);

    if (backprop) {
        out_updater->er = st_aligned_malloc(sz, ALIGN_SIZE);
        if (out_updater->er == NULL) {
            ST_WARNING("Failed to st_aligned_malloc er.");
            goto ERR;
        }
        memset(out_updater->er, 0, sz);
    }

    return 0;

ERR:

    safe_st_aligned_free(out_updater->ac);
    safe_st_aligned_free(out_updater->er);

    return -1;
}

int out_updater_reset(out_updater_t *out_updater)
{
    ST_CHECK_PARAM(out_updater == NULL, -1);

    return 0;
}

static int out_clear_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    out_updater_t *out_updater;
    size_t sz;

    out_updater = (out_updater_t *)args;

    if (child_s >= child_e) {
        return 0;
    }

    sz = (child_e - child_s) * sizeof(real_t);
    memset(out_updater->ac + child_s, 0, sz);
    if (out_updater->er != NULL) {
        memset(out_updater->er + child_s, 0, sz);
    }

    return 0;
}

int out_updater_clear(out_updater_t *out_updater, int word)
{
    ST_CHECK_PARAM(out_updater == NULL || word < 0, -1);

    if (output_walk_through_path(out_updater->output, word, out_clear_walker,
                (void *)out_updater) < 0) {
        ST_WARNING("Failed to output_walk_through_path.");
        return -1;
    }

    return 0;
}

typedef struct _out_act_walker_args_t_ {
    double logp;
    real_t *ac;
} out_act_walker_args_t;

static int out_act_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    out_act_walker_args_t *oaw_args;

    oaw_args = (out_act_walker_args_t *)args;

    if (child_e <= child_s || child_e == OUTPUT_NODE_NONE) {
        return 0;
    }

    if (output->norm == ON_SOFTMAX) {
        oaw_args->ac[child_e - 1] = 0;
        softmax(oaw_args->ac + child_s, child_e - child_s);
    }

    oaw_args->logp += log(oaw_args->ac[next_node]);

    return 0;
}

int out_updater_activate(out_updater_t *out_updater, int word, double *logp)
{
    out_act_walker_args_t oaw_args;

    ST_CHECK_PARAM(out_updater == NULL || word < 0 || logp == NULL, -1);

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Activate:output: word[%d]", word);
#endif

    oaw_args.logp = 0.0;
    oaw_args.ac = out_updater->ac;

    if (output_walk_through_path(out_updater->output, word, out_act_walker,
                (void *)&oaw_args) < 0) {
        ST_WARNING("Failed to output_walk_through_path.");
        return -1;
    }

    *logp = oaw_args.logp;

    return 0;
}

static int out_loss_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    out_updater_t *out_updater;
    output_node_id_t ch;

    out_updater = (out_updater_t *)args;

    if (output->norm == ON_SOFTMAX) {
        for (ch = child_s; ch < child_e; ++ch) {
            out_updater->er[ch] = (0 - out_updater->ac[ch]);
        }
        out_updater->er[next_node] = (1 - out_updater->ac[next_node]);
    }

    return 0;
}

int out_updater_loss(out_updater_t *out_updater, int word)
{
    ST_CHECK_PARAM(out_updater == NULL || word < 0, -1);

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Loss:output: word[%d]", word);
#endif

    if (output_walk_through_path(out_updater->output, word, out_loss_walker,
                (void *)out_updater) < 0) {
        ST_WARNING("Failed to output_walk_through_path.");
        return -1;
    }

    return 0;
}

int out_updater_finish(out_updater_t *out_updater)
{
    ST_CHECK_PARAM(out_updater == NULL, -1);

    return 0;
}

output_node_id_t out_updater_sample(out_updater_t *out_updater,
        output_node_id_t node)
{
    output_t *output;
    output_node_id_t s, e, sampled;

    double u, p;
    int word;

    ST_CHECK_PARAM(out_updater == NULL || node == OUTPUT_NODE_NONE,
            OUTPUT_NODE_NONE);

    output = out_updater->output;

    s = s_children(output->tree, node);
    e = e_children(output->tree, node);
    if (s >= e) {
        ST_WARNING("No children to be sampled.");
        return OUTPUT_NODE_NONE;
    }

    if (e - s <= 2) {
        for (sampled = s; sampled < e; sampled++) {
            if (!is_leaf(output->tree, sampled)) {
                break;
            }
            word = output_tree_leaf2word(output->tree, sampled);
            if (word != UNK_ID && word != SENT_START_ID) {
                break;
            }
        }
        if (sampled == e) {
            ST_WARNING("Can't sample from node["OUTPUT_NODE_FMT"], "
                   "because all its children are <unk> or <s>.");
            return OUTPUT_NODE_NONE;
        }
    }

    if (output->norm == ON_SOFTMAX) {
        out_updater->ac[e - 1] = 0;
        softmax(out_updater->ac + s, e - s);
    }

    while (true) {
        u = st_random(0, 1);
        sampled = s;

        p = 0;

        for (sampled = s; sampled < e; sampled++) {
            p += out_updater->ac[sampled];

            if (p >= u) {
                break;
            }
        }

        if (is_leaf(output->tree, sampled)) {
            word = output_tree_leaf2word(output->tree, sampled);
            if (word == UNK_ID || word == SENT_START_ID) {
                continue;
            }
        }

        break;
    }

    return sampled;
}

typedef struct _output_activate_all_args_t_ {
    double *node_probs;
    double *word_probs;
    real_t *ac;
    output_norm_t norm;
} output_activate_all_args_t;

static int output_tree_bfs_trav_activate_all(output_tree_t *tree,
        output_node_id_t node, void *args)
{
    output_activate_all_args_t *oaa_args;

    output_node_id_t child_s;
    output_node_id_t child_e;
    output_node_id_t ch;
    int word;
    double parent_logp;

    ST_CHECK_PARAM(tree == NULL || args == NULL, -1);

    oaa_args = (output_activate_all_args_t *)args;

    child_s = s_children(tree, node);
    child_e = e_children(tree, node);

    if (child_s >= child_e) {
        word = output_tree_leaf2word(tree, node);
        oaa_args->word_probs[word] = oaa_args->node_probs[node];
    } else {
        if (oaa_args->norm == ON_SOFTMAX) {
            oaa_args->ac[child_e - 1] = 0;
            softmax(oaa_args->ac + child_s, child_e - child_s);
        }
        parent_logp = oaa_args->node_probs[node];
        for (ch = child_s; ch < child_e; ch++) {
            oaa_args->node_probs[ch] = parent_logp + log(oaa_args->ac[ch]);
        }
    }

    return 0;
}

int out_updater_init_all(out_updater_t *out_updater)
{
    ST_CHECK_PARAM(out_updater == NULL, -1);

    out_updater->node_probs = (double *)malloc(sizeof(double)
            * out_updater->output->tree->num_node);
    if (out_updater->node_probs == NULL) {
        ST_WARNING("Failed to malloc node_probs.");
        return -1;
    }

    return 0;
}

int out_updater_activate_all(out_updater_t *out_updater,
        double *output_probs)
{
    output_activate_all_args_t oaa_args;

    ST_CHECK_PARAM(out_updater == NULL || output_probs == NULL, -1);

    oaa_args.node_probs = out_updater->node_probs;
    oaa_args.word_probs = output_probs;
    oaa_args.ac = out_updater->ac;
    oaa_args.norm = out_updater->output->norm;

    oaa_args.node_probs[out_updater->output->tree->root] = 0.0;
    if (output_tree_bfs(out_updater->output->tree,
                output_tree_bfs_trav_activate_all,
                &oaa_args) < 0) {
        ST_WARNING("Failed to output_tree_bfs.");
        return -1;
    }

    return 0;
}

int out_updater_clear_all(out_updater_t *out_updater)
{
    ST_CHECK_PARAM(out_updater == NULL, -1);

    memset(out_updater->ac, 0, sizeof(real_t)
            * out_updater->output->tree->num_node);
    if (out_updater->er != NULL) {
        memset(out_updater->er, 0, sizeof(real_t)
                * out_updater->output->tree->num_node);
    }

    return 0;
}
