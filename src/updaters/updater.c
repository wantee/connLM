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

#include "updater.h"

static int updater_reset(updater_t *updater)
{
    int c;

    ST_CHECK_PARAM(updater == NULL, -1);

    for (c = 0; c < updater->connlm->num_comp; c++) {
        if (comp_updater_reset(updater->comp_updaters[c]) < 0) {
            ST_WARNING("Failed to comp_updater_reset[%s].",
                    updater->connlm->comps[c]->name);
            return -1;
        }
    }

    if (out_updater_reset(updater->out_updater) < 0) {
        ST_WARNING("Failed to out_updater_reset.");
        return -1;
    }

    return 0;
}

#define tgt_word(updater) (updater)->sent.words[(updater)->sent.tgt_pos]

static int updater_start(updater_t *updater)
{
    int c;

    ST_CHECK_PARAM(updater == NULL, -1);

    for (c = 0; c < updater->connlm->num_comp; c++) {
        if (comp_updater_start(updater->comp_updaters[c]) < 0) {
            ST_WARNING("Failed to comp_updater_start[%s].",
                    updater->connlm->comps[c]->name);
            return -1;
        }
    }

    if (out_updater_start(updater->out_updater, tgt_word(updater)) < 0) {
        ST_WARNING("Failed to out_updater_start.");
        return -1;
    }

    return 0;
}

static int updater_forward(updater_t *updater)
{
    int c;

    ST_CHECK_PARAM(updater == NULL, -1);

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Forward: word[%d]", tgt_word(updater));
#endif

    for (c = 0; c < updater->connlm->num_comp; c++) {
        if (comp_updater_forward(updater->comp_updaters[c],
                &updater->sent) < 0) {
            ST_WARNING("Failed to comp_updater_forward[%s].",
                    updater->connlm->comps[c]->name);
            return -1;
        }
    }

    if (out_updater_activate(updater->out_updater, tgt_word(updater)) < 0) {
        ST_WARNING("Failed to out_updater_forward.");
        return -1;
    }

    return 0;
}

static int updater_backprop(updater_t *updater)
{
    int c;

    ST_CHECK_PARAM(updater == NULL, -1);

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Backprop: word[%d]", tgt_word(updater));
#endif

    if (out_updater_loss(updater->out_updater, tgt_word(updater)) < 0) {
        ST_WARNING("Failed to out_updater_backprop.");
        return -1;
    }

    for (c = 0; c < updater->connlm->num_comp; c++) {
        if (comp_updater_backprop(updater->comp_updaters[c],
                    &updater->sent) < 0) {
            ST_WARNING("Failed to comp_updater_backprop[%s].",
                    updater->connlm->comps[c]->name);
            return -1;
        }
    }

    return 0;
}

static int updater_end(updater_t *updater)
{
    int c;

    ST_CHECK_PARAM(updater == NULL, -1);

    for (c = 0; c < updater->connlm->num_comp; c++) {
        if (comp_updater_end(updater->comp_updaters[c]) < 0) {
            ST_WARNING("Failed to comp_updater_end[%s].",
                    updater->connlm->comps[c]->name);
            return -1;
        }
    }

    if (out_updater_end(updater->out_updater) < 0) {
        ST_WARNING("Failed to out_updater_end.");
        return -1;
    }

    return 0;
}

static int updater_finish(updater_t *updater)
{
    int c;

    ST_CHECK_PARAM(updater == NULL, -1);

    for (c = 0; c < updater->connlm->num_comp; c++) {
        if (comp_updater_finish(updater->comp_updaters[c]) < 0) {
            ST_WARNING("Failed to comp_updater_finish[%s].",
                    updater->connlm->comps[c]->name);
            return -1;
        }
    }

    if (out_updater_finish(updater->out_updater) < 0) {
        ST_WARNING("Failed to out_updater_finish.");
        return -1;
    }

    return 0;
}

void updater_destroy(updater_t *updater)
{
    int c;

    if (updater == NULL) {
        return;
    }

    safe_out_updater_destroy(updater->out_updater);
    if (updater->comp_updaters != NULL) {
        for (c = 0; c < updater->connlm->num_comp; c++) {
            safe_comp_updater_destroy(updater->comp_updaters[c]);
        }
        safe_free(updater->comp_updaters);
    }
    updater->connlm = NULL;
}

updater_t* updater_create(connlm_t *connlm)
{
    updater_t *updater = NULL;

    size_t sz;
    int c;

    ST_CHECK_PARAM(connlm == NULL, NULL);

    updater = (updater_t *)malloc(sizeof(updater_t));
    if (updater == NULL) {
        ST_WARNING("Failed to malloc updater.");
        goto ERR;
    }
    memset(updater, 0, sizeof(updater_t));

    updater->connlm = connlm;

    updater->input_updater = input_updater_create();
    if (updater->input_updater == NULL) {
        ST_WARNING("Failed to out_updater_create.");
        goto ERR;
    }

    updater->out_updater = out_updater_create(connlm->output);
    if (updater->out_updater == NULL) {
        ST_WARNING("Failed to out_updater_create.");
        goto ERR;
    }

    if (connlm->num_comp > 0) {
        sz = sizeof(comp_updater_t*)*connlm->num_comp;
        updater->comp_updaters = (comp_updater_t **)malloc(sz);
        if (updater->comp_updaters == NULL) {
            ST_WARNING("Failed to malloc comp_updaters.");
            goto ERR;
        }

        for (c = 0; c < connlm->num_comp; c++) {
            updater->comp_updaters[c] = comp_updater_create(connlm->comps[c],
                    updater->out_updater);
            if (updater->comp_updaters[c] == NULL) {
                ST_WARNING("Failed to comp_updater_create[%s].",
                        connlm->comps[c]->name);
                goto ERR;
            }
        }
    }

    return updater;

ERR:
    safe_updater_destroy(updater);
    return NULL;
}

int updater_setup(updater_t *updater, bool backprop)
{
    input_t *input;

    int c;

    int ctx_leftmost;
    int ctx_rightmost;

    ST_CHECK_PARAM(updater == NULL, -1);

    updater->backprop = backprop;

    if (out_updater_setup(updater->out_updater, backprop) < 0) {
        ST_WARNING("Failed to out_updater_setup.");
        goto ERR;
    }

    ctx_leftmost = 0;
    ctx_rightmost = 0;
    for (c = 0; c < updater->connlm->num_comp; c++) {
        if (comp_updater_setup(updater->comp_updaters[c], backprop) < 0) {
            ST_WARNING("Failed to comp_updater_setup[%s].",
                    updater->connlm->comps[c]->name);
            goto ERR;
        }

        input = updater->connlm->comps[c]->input;
        if (input->context[0].i < 0) {
            if (-input->context[0].i > ctx_leftmost) {
                ctx_leftmost = -input->context[0].i;
            }
        }
        if (input->context[input->n_ctx - 1].i > 0) {
            if (input->context[input->n_ctx - 1].i > ctx_rightmost) {
                ctx_rightmost = input->context[input->n_ctx - 1].i;
            }
        }
    }

    if (input_updater_setup(updater->input_updater, ctx_leftmost,
                ctx_rightmost) < 0) {
        ST_WARNING("Failed to input_updater_setup.");
        goto ERR;
    }

    updater->finalized = false;

    if (updater_reset(updater) < 0) {
        ST_WARNING("Failed to updater_reset.");
        goto ERR;
    }

    return 0;

ERR:
    return -1;
}

int updater_feed(updater_t *updater, int *words, int n_word)
{
    ST_CHECK_PARAM(updater == NULL, -1);

    if (input_updater_feed(updater->input_updater, words, n_word,
                &updater->sent) < 0) {
        ST_WARNING("Failed to input_updater_feed.");
        return -1;
    }

    return 0;
}

bool updater_steppable(updater_t *updater)
{
    ST_CHECK_PARAM(updater == NULL, false);

    if (updater->sent.n_word <= 0) {
        return false;
    }

    if (updater->sent.tgt_pos + updater->input_updater->ctx_rightmost
            < updater->sent.n_word) {
        return true;
    }

    if (updater->finalized) {
        return updater->sent.tgt_pos < updater->sent.n_word;
    }

    // if the sentence if over, we step to the end
    return (updater->sent.words[updater->sent.n_word - 1] == SENT_END_ID);
}

int updater_move_input(updater_t *updater)
{
    ST_CHECK_PARAM(updater == NULL, -1);

    if (input_updater_move(updater->input_updater, &updater->sent) < 0) {
        ST_WARNING("Failed to input_updater_move.");
        return -1;
    }

    return 0;
}

int updater_step(updater_t *updater)
{
    int word;

    ST_CHECK_PARAM(updater == NULL, -1);

    word = tgt_word(updater);

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Step: word[%s]/[%d]",
            vocab_get_word(updater->connlm->vocab, word), word);
#endif

    if (updater_start(updater) < 0) {
        ST_WARNING("updater_start.");
        return -1;
    }

    if (updater_forward(updater) < 0) {
        ST_WARNING("Failed to updater_forward.");
        return -1;
    }

    if (updater->backprop) {
        if (updater_backprop(updater) < 0) {
            ST_WARNING("Failed to updater_backprop.");
            return -1;
        }
    }

    if (updater_end(updater) < 0) {
        ST_WARNING("updater_end.");
        return -1;
    }

    if (word == SENT_END_ID) {
        if (updater_reset(updater) < 0) {
            ST_WARNING("Failed to updater_reset.");
            return -1;
        }
    }

    if (updater_move_input(updater) < 0) {
        ST_WARNING("Failed to updater_move_input.");
        return -1;
    }

    if (updater->finalized) {
        if (updater_finish(updater) < 0) {
            ST_WARNING("Failed to updater_finish.");
            return -1;
        }
    }

    return word;
}

int updater_finalize(updater_t *updater)
{
    ST_CHECK_PARAM(updater == NULL, -1);

    updater->finalized = true;

    return 0;
}

int updater_get_logp(updater_t *updater, int word, double *logp)
{
    ST_CHECK_PARAM(updater == NULL, -1);

    if (out_updater_get_logp(updater->out_updater, word, logp) < 0) {
        ST_WARNING("Failed to out_updater_get_logp.");
        return -1;
    }

    return 0;
}

static int updater_forward_util_out(updater_t *updater)
{
    int c;

    ST_CHECK_PARAM(updater == NULL, -1);

    for (c = 0; c < updater->connlm->num_comp; c++) {
        if (comp_updater_forward_util_out(updater->comp_updaters[c],
                    &updater->sent) < 0) {
            ST_WARNING("Failed to comp_updater_forward[%s].",
                    updater->connlm->comps[c]->name);
            return -1;
        }
    }

    return 0;
}

static int updater_sample_out(updater_t *updater)
{
    output_t *output;
    output_node_id_t node;
    int c;

    ST_CHECK_PARAM(updater == NULL, -1);

    output = updater->connlm->output;
    node = output->tree->root;

    while (!is_leaf(output->tree, node)) {
        for (c = 0; c < updater->connlm->num_comp; c++) {
            if (comp_updater_forward_out(updater->comp_updaters[c], node) < 0) {
                ST_WARNING("Failed to comp_updater_forward[%s].",
                        updater->connlm->comps[c]->name);
                return -1;
            }

            node = out_updater_sample(updater->out_updater, node);
            if (node == OUTPUT_NODE_NONE) {
                ST_WARNING("Failed to out_updater_sample.");
                return -1;
            }
        }
    }

    return output_tree_leaf2word(output->tree, node);
}

int updater_sampling(updater_t *updater)
{
    int word;

    ST_CHECK_PARAM(updater == NULL, -1);

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Sampling");
#endif

    if (updater_forward_util_out(updater) < 0) {
        ST_WARNING("Failed to updater_forward_util_out.");
        return -1;
    }

    word = updater_sample_out(updater);
    if (word < 0) {
        ST_WARNING("Failed to updater_sample_out.");
        return -1;
    }

    return word;
}
