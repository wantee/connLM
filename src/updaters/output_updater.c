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
#include <assert.h>

#include <stutils/st_macro.h>
#include <stutils/st_log.h>
#include <stutils/st_rand.h>
#include <stutils/st_mem.h>

#include "utils.h"
#include "output_updater.h"

void out_updater_destroy(out_updater_t *out_updater)
{
    int i;

    if (out_updater == NULL) {
        return;
    }

    safe_st_free(out_updater->node_iters);

    if (out_updater->node_acs != NULL) {
        for (i = 0; i < out_updater->output->tree->num_node; i++) {
            mat_destroy(out_updater->node_acs + i);
        }
        safe_st_free(out_updater->node_acs);
    }

    if (out_updater->node_ers != NULL) {
        for (i = 0; i < out_updater->output->tree->num_node; i++) {
            mat_destroy(out_updater->node_ers + i);
        }
        safe_st_free(out_updater->node_ers);
    }

    out_updater->output = NULL;
}

out_updater_t* out_updater_create(output_t *output)
{
    out_updater_t *out_updater = NULL;

    ST_CHECK_PARAM(output == NULL, NULL);

    out_updater = (out_updater_t *)st_malloc(sizeof(out_updater_t));
    if (out_updater == NULL) {
        ST_ERROR("Failed to st_malloc out_updater.");
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
    int num_nodes;

    ST_CHECK_PARAM(out_updater == NULL, -1);

    num_nodes = out_updater->output->tree->num_node;
    out_updater->node_iters = (int *)st_malloc(sizeof(int) * num_nodes);
    if (out_updater->node_iters == NULL) {
        ST_ERROR("Failed to st_malloc node_iters.");
        goto ERR;
    }
    memset(out_updater->node_iters, 0, sizeof(int) * num_nodes);

    out_updater->node_acs = (mat_t *)st_malloc(sizeof(mat_t) * num_nodes);
    if (out_updater->node_acs == NULL) {
        ST_ERROR("Failed to st_malloc node_acs.");
        goto ERR;
    }
    memset(out_updater->node_acs, 0, sizeof(mat_t) * num_nodes);

    if (backprop) {
        out_updater->node_ers = (mat_t *)st_malloc(sizeof(mat_t) * num_nodes);
        if (out_updater->node_ers == NULL) {
            ST_ERROR("Failed to st_malloc node_ers.");
            goto ERR;
        }
        memset(out_updater->node_ers, 0, sizeof(mat_t) * num_nodes);
    }

    return 0;
ERR:
    out_updater_destroy(out_updater);
    return -1;
}

static int out_reset_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    int *node_iters;

    node_iters = (int *) args;

    node_iters[node] = 0;

    return 0;
}

int out_updater_reset_iters(out_updater_t *out_updater, ivec_t *targets)
{
    int i;

    ST_CHECK_PARAM(out_updater == NULL || targets == NULL, -1);

    for (i = 0; i < targets->size; i++) {
        if (VEC_VAL(targets, i) == PADDING_ID) {
            continue;
        }
        if (output_walk_through_path(out_updater->output, VEC_VAL(targets, i),
                    out_reset_walker, (void *)out_updater->node_iters) < 0) {
            ST_ERROR("Failed to output_walk_through_path.");
            return -1;
        }
    }

    return 0;
}

static int out_acc_iter_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    int *node_iters;

    node_iters = (int *) args;

    node_iters[node]++;

    return 0;
}

static int out_updater_acc_iters(out_updater_t *out_updater, ivec_t *targets)
{
    int i;

    ST_CHECK_PARAM(out_updater == NULL, -1);

    if (out_updater_reset_iters(out_updater, targets) < 0) {
        ST_ERROR("Failed to out_updater_reset_iters.");
        return -1;
    }

    for (i = 0; i < targets->size; i++) {
        if (VEC_VAL(targets, i) == PADDING_ID) {
            continue;
        }
        if (output_walk_through_path(out_updater->output, VEC_VAL(targets, i),
                    out_acc_iter_walker, (void *)out_updater->node_iters) < 0) {
            ST_ERROR("Failed to output_walk_through_path.");
            return -1;
        }
    }

    return 0;
}

static int out_prepare_node_buf(out_updater_t *out_updater,
        output_node_id_t node, int node_batch_size, int num_children)
{
    if (mat_resize(out_updater->node_acs + node, node_batch_size,
                num_children, 0.0) < 0) {
        ST_ERROR("Failed to mat_resize node_acs["OUTPUT_NODE_FMT".", node);
        return -1;
    }


    if (out_updater->node_ers != NULL) {
        if (mat_resize(out_updater->node_ers + node,
                    node_batch_size, num_children,
                    NAN /* no need to init ers. */) < 0) {
            ST_ERROR("Failed to mat_resize node_ers["OUTPUT_NODE_FMT".", node);
            return -1;
        }
    }

    return 0;
}

static int out_prepare_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    out_updater_t *out_updater;

    out_updater = (out_updater_t *)args;

    if (child_e - child_s <= 1 || out_updater->node_iters[node] <= 0) {
        return 0;
    }

    if (out_prepare_node_buf(out_updater, node,
                out_updater->node_iters[node], child_e - child_s - 1) < 0) {
        ST_ERROR("Failed to out_prepare_node_buf.");
        return -1;
    }

    out_updater->node_iters[node] = 0;

    return 0;
}

int out_updater_prepare(out_updater_t *out_updater, ivec_t *targets)
{
    int i;

    ST_CHECK_PARAM(out_updater == NULL, -1);

    if (out_updater_acc_iters(out_updater, targets) < 0) {
        ST_ERROR("Failed to out_updater_acc_iters.");
        return -1;
    }

    for (i = 0; i < targets->size; i++) {
        if (VEC_VAL(targets, i) == PADDING_ID) {
            continue;
        }
        if (output_walk_through_path(out_updater->output, VEC_VAL(targets, i),
                    out_prepare_walker, (void *)out_updater) < 0) {
            ST_ERROR("Failed to output_walk_through_path.");
            return -1;
        }
    }

    return 0;
}

typedef struct _out_act_walker_args_t_ {
    out_updater_t *out_updater;
    dvec_t *logps;
    int batch_i;
} out_act_walker_args_t;

static int out_act_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    out_act_walker_args_t *oaw_args;
    mat_t *ac;
    size_t j;
    int this_row;
    double sum;

    if (child_e - child_s <= 1 || child_e == OUTPUT_NODE_NONE) {
        return 0;
    }

    oaw_args = (out_act_walker_args_t *) args;
    ac = oaw_args->out_updater->node_acs + node;
    this_row = oaw_args->out_updater->node_iters[node];

    assert(ac->num_cols == child_e - child_s - 1);
    multi_logit(MAT_VALP(ac, this_row, 0), ac->num_cols);

    if (next_node == child_e - 1) {
        sum = 0.0;
        for (j = 0; j < ac->num_cols; j++) {
            sum += MAT_VAL(ac, this_row, j);
        }
        VEC_VAL(oaw_args->logps, oaw_args->batch_i) += log(1.0 - sum);
    } else {
        VEC_VAL(oaw_args->logps, oaw_args->batch_i) +=
            log(MAT_VAL(ac, this_row, next_node - child_s));
    }
    oaw_args->out_updater->node_iters[node]++;

    return 0;
}

int out_updater_activate(out_updater_t *out_updater,
        ivec_t *targets, dvec_t *logps)
{
    out_act_walker_args_t oaw_args;
    output_t *output;

    int i;

    ST_CHECK_PARAM(out_updater == NULL || targets == NULL || logps == NULL, -1);

#ifdef _CONNLM_TRACE_PROCEDURE_
    char buf[MAX_LINE_LEN];
    ST_TRACE("Activate:output: words[%s]",
            ivec_dump(targets, buf, MAX_LINE_LEN));
#endif

    output = out_updater->output;

    if (dvec_resize(logps, targets->size, 0.0) < 0) {
        ST_ERROR("Failed to dvec_resize");
        return -1;
    }
    dvec_set(logps, 0.0);

    if (out_updater_reset_iters(out_updater, targets) < 0) {
        ST_ERROR("Failed to out_updater_reset_iters.");
        return -1;
    }

    oaw_args.out_updater = out_updater;
    oaw_args.logps = logps;
    for (i = 0; i < targets->size; i++) {
        if (VEC_VAL(targets, i) == PADDING_ID) {
            continue;
        }
        oaw_args.batch_i = i;
        if (output_walk_through_path(output, VEC_VAL(targets, i),
                    out_act_walker, (void *)&oaw_args) < 0) {
            ST_ERROR("Failed to output_walk_through_path.");
            return -1;
        }
    }

    return 0;
}

static int out_loss_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    out_updater_t *out_updater;
    output_node_id_t ch;
    mat_t *ac;
    mat_t *er;
    int this_row;

    if (child_e - child_s <= 1 || child_e == OUTPUT_NODE_NONE) {
        return 0;
    }

    out_updater = (out_updater_t *) args;
    ac = out_updater->node_acs + node;
    er = out_updater->node_ers + node;
    this_row = out_updater->node_iters[node];

    assert(ac->num_cols == child_e - child_s - 1);
    for (ch = 0; ch < ac->num_cols; ++ch) {
        MAT_VAL(er, this_row, ch) = (0 - MAT_VAL(ac, this_row, ch));
    }
    if (next_node < child_e - 1) {
        MAT_VAL(er, this_row, next_node - child_s) =
            (1 - MAT_VAL(ac, this_row, next_node - child_s));
    }

    out_updater->node_iters[node]++;

    return 0;
}

int out_updater_loss(out_updater_t *out_updater, ivec_t *targets)
{
    output_t *output;
    int i;

    ST_CHECK_PARAM(out_updater == NULL || targets == NULL, -1);

#ifdef _CONNLM_TRACE_PROCEDURE_
    char buf[MAX_LINE_LEN];
    ST_TRACE("Loss:output: words[%s]",
            ivec_dump(targets, buf, MAX_LINE_LEN));
#endif

    output = out_updater->output;

    if (out_updater_reset_iters(out_updater, targets) < 0) {
        ST_ERROR("Failed to out_updater_reset_iters.");
        return -1;
    }

    for (i = 0; i < targets->size; i++) {
        if (VEC_VAL(targets, i) == PADDING_ID) {
            continue;
        }
        if (output_walk_through_path(output, VEC_VAL(targets, i),
                    out_loss_walker, (void *)out_updater) < 0) {
            ST_ERROR("Failed to output_walk_through_path.");
            return -1;
        }
    }

    return 0;
}

int out_updater_finish(out_updater_t *out_updater)
{
    ST_CHECK_PARAM(out_updater == NULL, -1);

    return 0;
}

int out_updater_prepare_node(out_updater_t *out_updater, output_node_id_t node,
        int node_batch_size)
{
    output_t *output;
    output_node_id_t child_s, child_e;

    ST_CHECK_PARAM(out_updater == NULL || node == OUTPUT_NODE_NONE, -1);

    output = out_updater->output;

    child_s = s_children(output->tree, node);
    child_e = e_children(output->tree, node);

    if (out_prepare_node_buf(out_updater, node,
                node_batch_size, child_e - child_s - 1) < 0) {
        ST_ERROR("Failed to out_prepare_node_buf.");
        return -1;
    }

    return 0;
}

output_node_id_t out_updater_sample(out_updater_t *out_updater,
        output_node_id_t node)
{
    output_t *output;
    mat_t *ac;
    output_node_id_t s, e, sampled;

    double u, p;
    int word;
    int row;

    ST_CHECK_PARAM(out_updater == NULL || node == OUTPUT_NODE_NONE,
            OUTPUT_NODE_NONE);

    output = out_updater->output;

    s = s_children(output->tree, node);
    e = e_children(output->tree, node);
    if (s >= e) {
        ST_ERROR("No children to be sampled.");
        return OUTPUT_NODE_NONE;
    }

    if (e - s <= 2) {
        for (sampled = s; sampled < e; sampled++) {
            if (!is_leaf(output->tree, sampled)) {
                break;
            }
            word = output_tree_leaf2word(output->tree, sampled);
            if (word != UNK_ID) {
                break;
            }
        }
        if (sampled == e) {
            ST_ERROR("Can't sample from node["OUTPUT_NODE_FMT"], "
                   "because all its children are <unk>.", node);
            return OUTPUT_NODE_NONE;
        }
    }

    ac = out_updater->node_acs + node;
    row = 0; // batch_size == 1
    multi_logit(MAT_VALP(ac, row, 0), ac->num_cols);

    while (true) {
        u = st_random(0, 1);
        sampled = s;

        p = 0;

        for (sampled = s; sampled < e; sampled++) {
            if (sampled == e - 1) {
                p = 1.0;
            } else {
                p += MAT_VAL(ac, row, sampled - s);
            }

            if (p >= u) {
                break;
            }
        }

        if (is_leaf(output->tree, sampled)) {
            word = output_tree_leaf2word(output->tree, sampled);
            if (word == UNK_ID) {
                continue;
            }
        } else {
            if (sampled == output->unk_root) {
                continue;
            }
        }

        break;
    }

    return sampled;
}

int out_updater_clear_node(out_updater_t *out_updater, output_node_id_t node)
{
    output_t *output;
    output_node_id_t s, e;

    ST_CHECK_PARAM(out_updater == NULL || node == OUTPUT_NODE_NONE, -1);

    output = out_updater->output;

    s = s_children(output->tree, node);
    e = e_children(output->tree, node);
    if (s >= e) {
        return 0;
    }

    mat_set(out_updater->node_acs + node, 0.0);

    return 0;
}
