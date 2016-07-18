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

#include "utils.h"
#include "output_updater.h"

void out_updater_destroy(out_updater_t *out_updater)
{
    if (out_updater == NULL) {
        return;
    }

    safe_free(out_updater->ac);
    safe_free(out_updater->er);

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

    safe_free(out_updater->ac);
    safe_free(out_updater->er);

    sz = sizeof(real_t) * out_updater->output->tree->num_node;
    if (posix_memalign((void **)&out_updater->ac, ALIGN_SIZE, sz) != 0
            || out_updater->ac == NULL) {
        ST_WARNING("Failed to malloc ac.");
        goto ERR;
    }
    memset(out_updater->ac, 0, sz);

    if (backprop) {
        if (posix_memalign((void **)&out_updater->er, ALIGN_SIZE, sz) != 0
                || out_updater->er == NULL) {
            ST_WARNING("Failed to malloc er.");
            goto ERR;
        }
        memset(out_updater->er, 0, sz);
    }

    return 0;

ERR:

    safe_free(out_updater->ac);
    safe_free(out_updater->er);

    return -1;
}

int out_updater_reset(out_updater_t *out_updater)
{
    ST_CHECK_PARAM(out_updater == NULL, -1);

    return 0;
}

static int out_start_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    out_updater_t *out_updater;
    output_node_id_t ch;

    out_updater = (out_updater_t *)args;

    for (ch = child_s; ch < child_e; ch++) {
        out_updater->ac[ch] = 0.0;
        if (out_updater->er != NULL) {
            out_updater->er[ch] = 0.0;
        }
    }

    return 0;
}

int out_updater_start(out_updater_t *out_updater, int word)
{
    ST_CHECK_PARAM(out_updater == NULL || word < 0, -1);

    if (output_walk_through_path(out_updater->output, word, out_start_walker,
                (void *)out_updater) < 0) {
        ST_WARNING("Failed to output_walk_through_path.");
        return -1;
    }

    return 0;
}

static int out_act_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    out_updater_t *out_updater;

    out_updater = (out_updater_t *)args;

    if (child_e <= child_s || child_e == OUTPUT_NODE_NONE) {
        return 0;
    }

    if (output->norm == ON_SOFTMAX) {
        out_updater->ac[child_e - 1] = 0;
        softmax(out_updater->ac + child_s, child_e - child_s);
    }

    return 0;
}

int out_updater_activate(out_updater_t *out_updater, int word)
{
    ST_CHECK_PARAM(out_updater == NULL || word < 0, -1);

#if _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Activate:output: word[%d]", word);
#endif

    if (output_walk_through_path(out_updater->output, word, out_act_walker,
                (void *)out_updater) < 0) {
        ST_WARNING("Failed to output_walk_through_path.");
        return -1;
    }

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

#if _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Loss:output: word[%d]", word);
#endif

    if (output_walk_through_path(out_updater->output, word, out_loss_walker,
                (void *)out_updater) < 0) {
        ST_WARNING("Failed to output_walk_through_path.");
        return -1;
    }

    return 0;
}

int out_updater_end(out_updater_t *out_updater)
{
    ST_CHECK_PARAM(out_updater == NULL, -1);

    return 0;
}

int out_updater_finish(out_updater_t *out_updater)
{
    ST_CHECK_PARAM(out_updater == NULL, -1);

    return 0;
}

typedef struct _out_logp_walker_args_t_ {
    double logp;
    real_t *ac;
} out_logp_walker_args_t;

static int out_logp_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    out_logp_walker_args_t *olw_args;

    olw_args = (out_logp_walker_args_t *)args;

    if (node == output->tree->root) {
        return 0;
    }

    olw_args->logp += log10(olw_args->ac[node]);

    return 0;
}

int out_updater_get_logp(out_updater_t *out_updater, int word, double *logp)
{
    out_logp_walker_args_t olw_args;
    output_t *output;

    output_node_id_t node;

    ST_CHECK_PARAM(out_updater == NULL || word < 0 || logp == NULL, -1);

    output = out_updater->output;

    olw_args.logp = 0.0;
    olw_args.ac = out_updater->ac;
    if (output_walk_through_path(out_updater->output, word, out_logp_walker,
                (void *)&olw_args) < 0) {
        ST_WARNING("Failed to output_walk_through_path.");
        return -1;
    }
    *logp = olw_args.logp;

    node = output_tree_word2leaf(output->tree, word);
    *logp += log10(out_updater->ac[node]);

    return 0;
}

output_node_id_t out_updater_sample(out_updater_t *out_updater,
        output_node_id_t node)
{
    output_t *output;
    output_node_id_t s, e, sampled;

    double u, p;

    ST_CHECK_PARAM(out_updater == NULL || node == OUTPUT_NODE_NONE,
            OUTPUT_NODE_NONE);

    output = out_updater->output;

    s = s_children(output->tree, node);
    e = e_children(output->tree, node);
    if (s >= e) {
        ST_WARNING("No children to be sampled.");
        return OUTPUT_NODE_NONE;
    }

    if (output->norm == ON_SOFTMAX) {
        out_updater->ac[e - 1] = 0;
        softmax(out_updater->ac + s, e - s);
    }

    u = st_random(0, 1);
    sampled = s;

    p = 0;

    for (sampled = s; sampled < e; sampled++) {
        p += out_updater->ac[sampled];

        if (p >= u) {
            break;
        }
    }

    return sampled;
}
