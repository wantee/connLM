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

    mat_destroy(&out_updater->ac);
    mat_destroy(&out_updater->er);

    out_updater->output = NULL;
}

out_updater_t* out_updater_create(output_t *output)
{
    out_updater_t *out_updater = NULL;

    ST_CHECK_PARAM(output == NULL, NULL);

    out_updater = (out_updater_t *)st_malloc(sizeof(out_updater_t));
    if (out_updater == NULL) {
        ST_WARNING("Failed to st_malloc out_updater.");
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
    ST_CHECK_PARAM(out_updater == NULL, -1);

    return 0;
}

typedef struct _out_clear_walker_args_t_ {
    real_t *ac;
    real_t *er;
} out_clear_walker_args_t;

static int out_clear_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    out_clear_walker_args_t *ocw_args;
    size_t sz;

    ocw_args = (out_clear_walker_args_t *)args;

    if (child_s >= child_e) {
        return 0;
    }

    sz = (child_e - child_s) * sizeof(real_t);
    memset(ocw_args->ac + child_s, 0, sz);
    if (ocw_args->er != NULL) {
        memset(ocw_args->er + child_s, 0, sz);
    }

    return 0;
}

int out_updater_clear(out_updater_t *out_updater, ivec_t *targets)
{
    out_clear_walker_args_t ocw_args;

    int i;

    ST_CHECK_PARAM(out_updater == NULL, -1);

    for (i = 0; i < targets->size; i++) {
        ocw_args.ac = MAT_VALP(&out_updater->ac, i, 0);
        if (out_updater->er.num_rows > 0) {
            ocw_args.er = MAT_VALP(&out_updater->er, i, 0);
        } else {
            ocw_args.er = NULL;
        }

        if (output_walk_through_path(out_updater->output, VEC_VAL(targets, i),
                    out_clear_walker, (void *)&ocw_args) < 0) {
            ST_WARNING("Failed to output_walk_through_path.");
            return -1;
        }
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

int out_updater_activate(out_updater_t *out_updater,
        ivec_t *targets, dvec_t *logps)
{
    out_act_walker_args_t oaw_args;
    output_t *output;

    int i;

    ST_CHECK_PARAM(out_updater == NULL || targets == NULL || logps == NULL, -1);

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Activate:output: word[%d]", word);
#endif

    output = out_updater->output;

    if (mat_resize(&out_updater->ac, targets->size,
                output->tree->num_node, 0.0) < 0) {
        ST_WARNING("Failed to mat_resize");
        return -1;
    }

    if (dvec_resize(logps, targets->size, 0.0) < 0) {
        ST_WARNING("Failed to dvec_resize");
        return -1;
    }

    for (i = 0; i < targets->size; i++) {
        oaw_args.logp = 0.0;
        oaw_args.ac = MAT_VALP(&out_updater->ac, i, 0);

        if (output_walk_through_path(output, VEC_VAL(targets, i),
                    out_act_walker, (void *)&oaw_args) < 0) {
            ST_WARNING("Failed to output_walk_through_path.");
            return -1;
        }

        VEC_VAL(logps, i) = oaw_args.logp;
    }

    return 0;
}

typedef struct _out_loss_walker_args_t_ {
    real_t *ac;
    real_t *er;
} out_loss_walker_args_t;

static int out_loss_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    out_loss_walker_args_t *olw_args;
    output_node_id_t ch;

    olw_args = (out_loss_walker_args_t *)args;

    if (output->norm == ON_SOFTMAX) {
        for (ch = child_s; ch < child_e; ++ch) {
            olw_args->er[ch] = (0 - olw_args->ac[ch]);
        }
        olw_args->er[next_node] = (1 - olw_args->ac[next_node]);
    }

    return 0;
}

int out_updater_loss(out_updater_t *out_updater, ivec_t *targets)
{
    out_loss_walker_args_t olw_args;
    output_t *output;
    int i;

    ST_CHECK_PARAM(out_updater == NULL || targets == NULL, -1);

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Loss:output: word[%d]", word);
#endif

    output = out_updater->output;

    if (mat_resize(&out_updater->er, targets->size,
                output->tree->num_node, NAN) < 0) {
        ST_WARNING("Failed to mat_resize");
        return -1;
    }

    for (i = 0; i < targets->size; i++) {
        olw_args.ac = MAT_VALP(&out_updater->ac, i, 0);
        olw_args.er = MAT_VALP(&out_updater->er, i, 0);

        if (output_walk_through_path(output, VEC_VAL(targets, i),
                    out_loss_walker, (void *)&olw_args) < 0) {
            ST_WARNING("Failed to output_walk_through_path.");
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

output_node_id_t out_updater_sample(out_updater_t *out_updater,
        output_node_id_t node)
{
    output_t *output;
    real_t *ac;
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
            if (word != UNK_ID) {
                break;
            }
        }
        if (sampled == e) {
            ST_WARNING("Can't sample from node["OUTPUT_NODE_FMT"], "
                   "because all its children are <unk>.", node);
            return OUTPUT_NODE_NONE;
        }
    }

    ac = MAT_VALP(&out_updater->ac, 0, 0); // batch_size == 1
    if (output->norm == ON_SOFTMAX) {
        ac[e - 1] = 0;
        softmax(ac + s, e - s);
    }

    while (true) {
        u = st_random(0, 1);
        sampled = s;

        p = 0;

        for (sampled = s; sampled < e; sampled++) {
            p += ac[sampled];

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
