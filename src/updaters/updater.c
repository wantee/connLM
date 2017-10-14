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
    int i, c;

    ST_CHECK_PARAM(updater == NULL, -1);

    for (i = 0; i < updater->targets.size; i++) {
        if (VEC_VAL(&updater->targets, i) == SENT_END_ID) {
            for (c = 0; c < updater->connlm->num_comp; c++) {
                if (comp_updater_reset(updater->comp_updaters[c], i) < 0) {
                    ST_WARNING("Failed to comp_updater_reset[%s].",
                            updater->connlm->comps[c]->name);
                    return -1;
                }
            }
        }
    }

    return 0;
}

static int updater_prepare(updater_t *updater)
{
    int c;

    ST_CHECK_PARAM(updater == NULL, -1);

    for (c = 0; c < updater->connlm->num_comp; c++) {
        if (comp_updater_prepare(updater->comp_updaters[c],
                    updater->batches + c) < 0) {
            ST_WARNING("Failed to comp_updater_prepare[%s].",
                    updater->connlm->comps[c]->name);
            return -1;
        }
    }

    // updater->targets.size == 0 when called in sampling
    if (out_updater_prepare(updater->out_updater, &updater->targets) < 0) {
        ST_WARNING("Failed to out_updater_prepare.");
        return -1;
    }

    return 0;
}

static int updater_save_state(updater_t *updater)
{
    int c;

    ST_CHECK_PARAM(updater == NULL, -1);

    for (c = 0; c < updater->connlm->num_comp; c++) {
        if (comp_updater_save_state(updater->comp_updaters[c]) < 0) {
            ST_WARNING("Failed to comp_updater_save_state[%s].",
                    updater->connlm->comps[c]->name);
            return -1;
        }
    }

    return 0;
}

static int updater_forward_comp(updater_t *updater)
{
    int c;

    ST_CHECK_PARAM(updater == NULL, -1);

    for (c = 0; c < updater->connlm->num_comp; c++) {
        if (comp_updater_forward(updater->comp_updaters[c],
                updater->batches + c) < 0) {
            ST_WARNING("Failed to comp_updater_forward[%s].",
                    updater->connlm->comps[c]->name);
            return -1;
        }
    }

    return 0;
}

static int updater_forward(updater_t *updater)
{
    ST_CHECK_PARAM(updater == NULL, -1);

#ifdef _CONNLM_TRACE_PROCEDURE_
    char buf[MAX_LINE_LEN];
    ST_TRACE("Forward: words[%s]",
            ivec_dump(&updater->targets, buf, MAX_LINE_LEN));
#endif

    if (updater_forward_comp(updater) < 0) {
        ST_WARNING("Failed to updater_forward_comp.");
        return -1;
    }

    if (out_updater_activate(updater->out_updater,
                &updater->targets, &updater->logps) < 0) {
        ST_WARNING("Failed to out_updater_activate.");
        return -1;
    }

    return 0;
}

static int updater_backprop(updater_t *updater)
{
    int c;

    ST_CHECK_PARAM(updater == NULL, -1);

#ifdef _CONNLM_TRACE_PROCEDURE_
    char buf[MAX_LINE_LEN];
    ST_TRACE("Backprop: words[%s]",
            ivec_dump(&updater->targets, buf, MAX_LINE_LEN));
#endif

    if (out_updater_loss(updater->out_updater, &updater->targets) < 0) {
        ST_WARNING("Failed to out_updater_backprop.");
        return -1;
    }

    for (c = 0; c < updater->connlm->num_comp; c++) {
        if (comp_updater_backprop(updater->comp_updaters[c],
                    updater->batches + c) < 0) {
            ST_WARNING("Failed to comp_updater_backprop[%s].",
                    updater->connlm->comps[c]->name);
            return -1;
        }
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

    safe_input_updater_destroy(updater->input_updater);
    safe_out_updater_destroy(updater->out_updater);
    if (updater->comp_updaters != NULL) {
        for (c = 0; c < updater->connlm->num_comp; c++) {
            safe_comp_updater_destroy(updater->comp_updaters[c]);
        }
        safe_st_free(updater->comp_updaters);
    }
    if (updater->batches != NULL) {
        for (c = 0; c < updater->connlm->num_comp; c++) {
            egs_batch_destroy(updater->batches + c);
        }
        safe_st_free(updater->batches);
    }

    ivec_destroy(&updater->targets);
    dvec_destroy(&updater->logps);
    word_pool_destroy(&updater->tmp_wp);

    updater->connlm = NULL;
}

updater_t* updater_create(connlm_t *connlm)
{
    updater_t *updater = NULL;

    size_t sz;
    int c;

    ST_CHECK_PARAM(connlm == NULL, NULL);

    updater = (updater_t *)st_malloc(sizeof(updater_t));
    if (updater == NULL) {
        ST_WARNING("Failed to st_malloc updater.");
        goto ERR;
    }
    memset(updater, 0, sizeof(updater_t));

    updater->connlm = connlm;

    updater->input_updater = input_updater_create(
            vocab_get_id(connlm->vocab, SENT_START));
    if (updater->input_updater == NULL) {
        ST_WARNING("Failed to input_updater_create.");
        goto ERR;
    }

    updater->out_updater = out_updater_create(connlm->output);
    if (updater->out_updater == NULL) {
        ST_WARNING("Failed to out_updater_create.");
        goto ERR;
    }

    if (connlm->num_comp > 0) {
        sz = sizeof(comp_updater_t*)*connlm->num_comp;
        updater->comp_updaters = (comp_updater_t **)st_malloc(sz);
        if (updater->comp_updaters == NULL) {
            ST_WARNING("Failed to st_malloc comp_updaters.");
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

        sz = sizeof(egs_batch_t) * connlm->num_comp;
        updater->batches = (egs_batch_t *)st_malloc(sz);
        if (updater->batches == NULL) {
            ST_WARNING("Failed to st_malloc batches.");
            goto ERR;
        }
        memset(updater->batches, 0, sz);
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

int updater_set_rand_seed(updater_t *updater, unsigned int seed)
{
    int c;

    ST_CHECK_PARAM(updater == NULL, -1);

    updater->rand_seed = seed;

    for (c = 0; c < updater->connlm->num_comp; c++) {
        if (comp_updater_set_rand_seed(updater->comp_updaters[c],
                    &updater->rand_seed) < 0) {
            ST_WARNING("Failed to comp_updater_set_rand_seed[%s].",
                    updater->connlm->comps[c]->name);
            return -1;
        }
    }

    return 0;
}

int updater_feed(updater_t *updater, word_pool_t *wp)
{
    ST_CHECK_PARAM(updater == NULL, -1);

    if (input_updater_feed(updater->input_updater, wp) < 0) {
        ST_WARNING("Failed to input_updater_feed.");
        return -1;
    }

    return 0;
}

bool updater_steppable(updater_t *updater)
{
    ST_CHECK_PARAM(updater == NULL, false);

    return input_updater_movable(updater->input_updater, updater->finalized);
}

int updater_move_input(updater_t *updater)
{
    int c;

    ST_CHECK_PARAM(updater == NULL, -1);

    if (input_updater_move(updater->input_updater) < 0) {
        ST_WARNING("Failed to input_updater_move.");
        return -1;
    }

    for (c = 0; c < updater->connlm->num_comp; c++) {
        if (input_updater_update_batch(updater->input_updater,
                    updater->comp_updaters[c]->comp->input,
                    updater->batches + c) < 0) {
            ST_WARNING("Failed to input_updater_update_batch[%s].",
                    updater->connlm->comps[c]->name);
            return -1;
        }
    }

    if (ivec_set(&updater->targets,
                updater->batches[0].targets,
                updater->batches[0].num_egs) < 0) {
        ST_WARNING("Failed to ivec_set targets.");
        return -1;
    }

    return 0;
}

int updater_step(updater_t *updater)
{
    ST_CHECK_PARAM(updater == NULL, -1);

    if (updater_move_input(updater) < 0) {
        ST_WARNING("Failed to updater_move_input.");
        return -1;
    }

#ifdef _CONNLM_TRACE_PROCEDURE_
    char buf[MAX_LINE_LEN];
    ST_TRACE("Step: words[%s]",
            ivec_dump(&updater->targets, buf, MAX_LINE_LEN));
#endif

    if (updater_prepare(updater) < 0) {
        ST_WARNING("updater_prepare.");
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

    if (updater_save_state(updater) < 0) {
        ST_WARNING("updater_save_state.");
        return -1;
    }

    if (updater_reset(updater) < 0) {
        ST_WARNING("Failed to updater_reset.");
        return -1;
    }

    if (updater->finalized) {
        if (updater_finish(updater) < 0) {
            ST_WARNING("Failed to updater_finish.");
            return -1;
        }
    }

    return 0;
}

int updater_finalize(updater_t *updater)
{
    ST_CHECK_PARAM(updater == NULL, -1);

    updater->finalized = true;

    return 0;
}

static int updater_forward_util_out(updater_t *updater)
{
    int c;

    ST_CHECK_PARAM(updater == NULL, -1);

    for (c = 0; c < updater->connlm->num_comp; c++) {
        if (comp_updater_forward_util_out(updater->comp_updaters[c],
                    updater->batches + c) < 0) {
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
        }
        node = out_updater_sample(updater->out_updater, node);
        if (node == OUTPUT_NODE_NONE) {
            ST_WARNING("Failed to out_updater_sample.");
            return -1;
        }

        if (out_updater_clear_node(updater->out_updater, node) < 0) {
            ST_WARNING("Failed to out_updater_clear_node.");
            return -1;
        }
    }

    return output_tree_leaf2word(output->tree, node);
}

static int wp_set_word(word_pool_t *wp, int word)
{
    ST_CHECK_PARAM(wp == NULL || word < 0, -1);

    if (word_pool_clear(wp) < 0) {
        ST_WARNING("Failed to word_pool_clear.");
        return -1;
    }
    if (word_pool_append(wp, word) < 0) {
        ST_WARNING("Failed to word_pool_resize.");
        return -1;
    }
    if (word_pool_build_mini_batch(wp, 1) < 0) {
        ST_WARNING("Failed to word_pool_build_mini_batch.");
        return -1;
    }

    return 0;
}

int updater_sampling(updater_t *updater, bool startover)
{
    int word;

    ST_CHECK_PARAM(updater == NULL, -1);

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Sampling");
#endif

    if (startover) { // feed with <s>
        word = vocab_get_id(updater->connlm->vocab, SENT_START);
        if (wp_set_word(&updater->tmp_wp, word) < 0) {
            ST_WARNING("Failed to wp_set_word.");
            return -1;
        }
        if (updater_feed(updater, &updater->tmp_wp) < 0) {
            ST_WARNING("Failed to updater_feed.");
            return -1;
        }

        if (updater_move_input(updater) < 0) {
            ST_WARNING("Failed to updater_move_input.");
            return -1;
        }
    }

    if (updater_prepare(updater) < 0) {
        ST_WARNING("updater_prepare.");
        return -1;
    }

    if (updater_forward_util_out(updater) < 0) {
        ST_WARNING("Failed to updater_forward_util_out.");
        return -1;
    }

    word = updater_sample_out(updater);
    if (word < 0) {
        ST_WARNING("Failed to updater_sample_out.");
        return -1;
    }

    if (wp_set_word(&updater->tmp_wp, word) < 0) {
        ST_WARNING("Failed to wp_set_word.");
        return -1;
    }
    if (updater_feed(updater, &updater->tmp_wp) < 0) {
        ST_WARNING("Failed to updater_feed.");
        return -1;
    }

    if (updater_save_state(updater) < 0) {
        ST_WARNING("updater_save_state.");
        return -1;
    }

    if (updater_reset(updater) < 0) {
        ST_WARNING("Failed to updater_reset.");
        return -1;
    }

    // Move cur_pos to the pos fed into later after sampled out
    if (updater_move_input(updater) < 0) {
        ST_WARNING("Failed to updater_move_input.");
        return -1;
    }

    return word;
}

int updater_state_size(updater_t *updater)
{
    int c;
    int size;
    int total_size;

    ST_CHECK_PARAM(updater == NULL, -1);

    total_size = 0;
    for (c = 0; c < updater->connlm->num_comp; c++) {
        size = comp_updater_state_size(updater->comp_updaters[c]);
        if (size < 0) {
            ST_WARNING("Failed to comp_updater_state_size[%s].",
                    updater->connlm->comps[c]->name);
            return -1;
        }
        total_size += size;
    }

    return total_size;
}

int updater_dump_state(updater_t *updater, mat_t *state)
{
    mat_t sub_state;

    int c;
    int size;
    int total_size;

    ST_CHECK_PARAM(updater == NULL || state == NULL, -1);

    total_size = 0;
    for (c = 0; c < updater->connlm->num_comp; c++) {
        size = comp_updater_state_size(updater->comp_updaters[c]);
        if (size < 0) {
            ST_WARNING("Failed to comp_updater_state_size[%s].",
                    updater->connlm->comps[c]->name);
            return -1;
        }

        if (mat_submat(state, 0, 0, total_size, 0, &sub_state) < 0) {
            ST_WARNING("Failed to mat_submat state.");
            return -1;
        }
        if (comp_updater_dump_state(updater->comp_updaters[c],
                    &sub_state) < 0) {
            ST_WARNING("Failed to comp_updater_dump_state[%s].",
                    updater->connlm->comps[c]->name);
            return -1;
        }
        total_size += size;
    }

    return 0;
}

int updater_dump_pre_ac_state(updater_t *updater, mat_t *state)
{
    mat_t sub_state;

    int c;
    int size;
    int total_size;

    ST_CHECK_PARAM(updater == NULL || state == NULL, -1);

    total_size = 0;
    for (c = 0; c < updater->connlm->num_comp; c++) {
        size = comp_updater_state_size(updater->comp_updaters[c]);
        if (size < 0) {
            ST_WARNING("Failed to comp_updater_state_size[%s].",
                    updater->connlm->comps[c]->name);
            return -1;
        }

        if (mat_submat(state, 0, 0, total_size, 0, &sub_state) < 0) {
            ST_WARNING("Failed to mat_submat state.");
            return -1;
        }
        if (comp_updater_dump_pre_ac_state(updater->comp_updaters[c],
                    &sub_state) < 0) {
            ST_WARNING("Failed to comp_updater_dump_pre_ac_state[%s].",
                    updater->connlm->comps[c]->name);
            return -1;
        }
        total_size += size;
    }

    return 0;
}

int updater_feed_state(updater_t *updater, mat_t *state)
{
    mat_t sub_state;

    int c;
    int size;
    int total_size;

    ST_CHECK_PARAM(updater == NULL || state == NULL, -1);

    total_size = 0;
    for (c = 0; c < updater->connlm->num_comp; c++) {
        size = comp_updater_state_size(updater->comp_updaters[c]);
        if (size < 0) {
            ST_WARNING("Failed to comp_updater_state_size[%s].",
                    updater->connlm->comps[c]->name);
            return -1;
        }

        if (mat_submat(state, 0, 0, total_size, 0, &sub_state) < 0) {
            ST_WARNING("Failed to mat_submat state.");
            return -1;
        }
        if (comp_updater_feed_state(updater->comp_updaters[c],
                    &sub_state) < 0) {
            ST_WARNING("Failed to comp_updater_feed_state[%s].",
                    updater->connlm->comps[c]->name);
            return -1;
        }
        total_size += size;
    }

    return 0;
}

int updater_random_state(updater_t *updater, mat_t *state)
{
    mat_t sub_state;

    int c;
    int size;
    int total_size;

    ST_CHECK_PARAM(updater == NULL || state == NULL, -1);

    total_size = 0;
    for (c = 0; c < updater->connlm->num_comp; c++) {
        size = comp_updater_state_size(updater->comp_updaters[c]);
        if (size < 0) {
            ST_WARNING("Failed to comp_updater_state_size[%s].",
                    updater->connlm->comps[c]->name);
            return -1;
        }

        if (mat_submat(state, 0, 0, total_size, 0, &sub_state) < 0) {
            ST_WARNING("Failed to mat_submat state.");
            return -1;
        }
        if (comp_updater_random_state(updater->comp_updaters[c],
                    &sub_state) < 0) {
            ST_WARNING("Failed to comp_updater_random_state[%s].",
                    updater->connlm->comps[c]->name);
            return -1;
        }
        total_size += size;
    }

    return 0;
}

static int updater_cleanup(updater_t *updater)
{
    int c;

    ST_CHECK_PARAM(updater == NULL, -1);

    if (input_updater_clear(updater->input_updater) < 0) {
        ST_WARNING("Failed to input_updater_clear.");
        return -1;
    }

    if (updater_reset(updater) < 0) {
        ST_WARNING("Failed to updater_reset.");
        return -1;
    }

    for (c = 0; c < updater->connlm->num_comp; c++) {
        if (comp_updater_prepare(updater->comp_updaters[c],
                    updater->batches + c) < 0) {
            ST_WARNING("Failed to comp_updater_prepare[%s].",
                    updater->connlm->comps[c]->name);
            return -1;
        }
    }

    return 0;
}

static int updater_set_hist(updater_t *updater, ivec_t *hists, int num_hists)
{
    int i, c;

    ST_CHECK_PARAM(updater == NULL || hists == NULL, -1);

    // build mini_batch with hists
    if (word_pool_clear(&updater->tmp_wp) < 0) {
        ST_WARNING("Failed to word_pool_clear.");
        return -1;
    }

    for (i = 0; i < num_hists; i++) {
        if (word_pool_append_ivec(&updater->tmp_wp, hists + i) < 0) {
            ST_WARNING("Failed to word_pool_append_ivec[%d].", i);
            return -1;
        }

        // set the sent_ends
        if (ivec_append(&updater->tmp_wp.sent_ends,
                    updater->tmp_wp.words.size) < 0) {
            ST_WARNING("Failed to ivec_append.");
            return -1;
        }
    }

    if (word_pool_build_mini_batch(&updater->tmp_wp, num_hists) < 0) {
        ST_WARNING("Failed to word_pool_build_mini_batch for hists.");
        return -1;
    }

    // feed the word_pool with hists
    if (updater_feed(updater, &updater->tmp_wp) < 0) {
        ST_WARNING("Failed to updater_feed.");
        return -1;
    }

    if (input_updater_move_to_end(updater->input_updater) < 0) {
        ST_WARNING("Failed to input_updater_move_to_end.");
        return -1;
    }

    for (c = 0; c < updater->connlm->num_comp; c++) {
        if (input_updater_update_batch(updater->input_updater,
                    updater->comp_updaters[c]->comp->input,
                    updater->batches + c) < 0) {
            ST_WARNING("Failed to input_updater_update_batch[%s].",
                    updater->connlm->comps[c]->name);
            return -1;
        }
    }

    return 0;
}

int updater_step_with_state(updater_t *updater, mat_t *state,
        ivec_t *hists, int num_hists)
{
    ST_CHECK_PARAM(updater == NULL || state == NULL, -1);

    if (updater_cleanup(updater) < 0) {
        ST_WARNING("Failed to updater_cleanup.");
        return -1;
    }

    if (hists != NULL && num_hists > 0) {
        if (num_hists != state->num_rows) {
            ST_WARNING("num_hists must be equal to state->num_rows.");
            return -1;
        }
        if (updater_set_hist(updater, hists, num_hists) < 0) {
            ST_WARNING("Failed to updater_set_hist.");
            return -1;
        }
    }

    if (state != NULL) {
        if (updater_feed_state(updater, state) < 0) {
            ST_WARNING("Failed to updater_feed_state.");
            return -1;
        }
    }

    if (updater_forward_util_out(updater) < 0) {
        ST_WARNING("Failed to updater_forward_util_out.");
        return -1;
    }

    if (updater_save_state(updater) < 0) {
        ST_WARNING("updater_save_state.");
        return -1;
    }

    return 0;
}

int updater_forward_out_words(updater_t *updater, ivec_t *words, dvec_t *logps)
{
    ST_CHECK_PARAM(updater == NULL || words == NULL, -1);

    int c;

    for (c = 0; c < updater->connlm->num_comp; c++) {
        if (comp_updater_forward_out_words(updater->comp_updaters[c], words) < 0) {
            ST_WARNING("Failed to comp_updater_forward_out_words[%s].",
                    updater->connlm->comps[c]->name);
            return -1;
        }
    }

    if (logps != NULL) {
        if (out_updater_activate(updater->out_updater, words, logps) < 0) {
            ST_WARNING("Failed to out_updater_activate.");
            return -1;
        }
    }

    return 0;
}

int updater_activate_state(updater_t *updater, mat_t *state)
{
    mat_t sub_state;

    int c;
    int size;
    int total_size;

    ST_CHECK_PARAM(updater == NULL || state == NULL, -1);

    total_size = 0;
    for (c = 0; c < updater->connlm->num_comp; c++) {
        size = comp_updater_state_size(updater->comp_updaters[c]);
        if (size < 0) {
            ST_WARNING("Failed to comp_updater_state_size[%s].",
                    updater->connlm->comps[c]->name);
            return -1;
        }

        if (mat_submat(state, 0, 0, total_size, 0, &sub_state) < 0) {
            ST_WARNING("Failed to mat_submat state.");
            return -1;
        }
        if (comp_updater_activate_state(updater->comp_updaters[c],
                    &sub_state) < 0) {
            ST_WARNING("Failed to comp_updater_activate_state[%s].",
                    updater->connlm->comps[c]->name);
            return -1;
        }
        total_size += size;
    }

    return 0;
}

int updater_setup_pre_ac_state(updater_t *updater)
{
    int c;

    ST_CHECK_PARAM(updater == NULL, -1);

    for (c = 0; c < updater->connlm->num_comp; c++) {
        if (comp_updater_setup_pre_ac_state(updater->comp_updaters[c]) < 0) {
            ST_WARNING("Failed to comp_updater_setup_pre_ac_state[%s].",
                    updater->connlm->comps[c]->name);
            return -1;
        }
    }

    return 0;
}

int updater_sampling_state(updater_t *updater, mat_t *state,
        mat_t *pre_ac_state, bool startover)
{
    int word;

    ST_CHECK_PARAM(updater == NULL
            || (state == NULL && pre_ac_state == NULL), -1);

    if ((state != NULL && state->num_rows != 1)
            || (pre_ac_state != NULL && pre_ac_state->num_rows != 1)) {
        ST_WARNING("num_rows of state/pre_ac_state must be 1.");
       return -1;
    }

    if (startover) { // feed with <s>
        word = vocab_get_id(updater->connlm->vocab, SENT_START);
        if (wp_set_word(&updater->tmp_wp, word) < 0) {
            ST_WARNING("Failed to wp_set_word.");
            return -1;
        }
        if (updater_feed(updater, &updater->tmp_wp) < 0) {
            ST_WARNING("Failed to updater_feed.");
            return -1;
        }
        if (updater_move_input(updater) < 0) {
            ST_WARNING("Failed to updater_move_input.");
            return -1;
        }
    }

    if (updater_prepare(updater) < 0) {
        ST_WARNING("updater_prepare.");
        return -1;
    }

    if (updater_forward_util_out(updater) < 0) {
        ST_WARNING("Failed to updater_forward_util_out.");
        return -1;
    }

    word = updater_sample_out(updater);
    if (word < 0) {
        ST_WARNING("Failed to updater_sample_out.");
        return -1;
    }

    if (wp_set_word(&updater->tmp_wp, word) < 0) {
        ST_WARNING("Failed to wp_set_word.");
        return -1;
    }
    if (updater_feed(updater, &updater->tmp_wp) < 0) {
        ST_WARNING("Failed to updater_feed.");
        return -1;
    }

    if (updater_save_state(updater) < 0) {
        ST_WARNING("updater_save_state.");
        return -1;
    }

    if (pre_ac_state != NULL) {
        if (updater_dump_pre_ac_state(updater, pre_ac_state) < 0) {
            ST_WARNING("updater_dump_pre_ac_state.");
            return -1;
        }
    }

    if (state != NULL) {
        if (updater_dump_state(updater, state) < 0) {
            ST_WARNING("updater_dump_state.");
            return -1;
        }
    }

    if (updater_reset(updater) < 0) {
        ST_WARNING("Failed to updater_reset.");
        return -1;
    }

    // Move cur_pos to the pos fed into later after sampled out
    if (updater_move_input(updater) < 0) {
        ST_WARNING("Failed to updater_move_input.");
        return -1;
    }

    return word;
}

int updater_step_state(updater_t *updater,
        mat_t *state, mat_t *pre_ac_state)
{
    ST_CHECK_PARAM(updater == NULL, -1);

    if (updater_move_input(updater) < 0) {
        ST_WARNING("Failed to updater_move_input.");
        return -1;
    }

    if (updater_prepare(updater) < 0) {
        ST_WARNING("updater_prepare.");
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

    if (updater_save_state(updater) < 0) {
        ST_WARNING("updater_save_state.");
        return -1;
    }

    if (pre_ac_state != NULL) {
        if (updater_dump_pre_ac_state(updater, pre_ac_state) < 0) {
            ST_WARNING("updater_dump_pre_ac_state.");
            return -1;
        }
    }

    if (state != NULL) {
        if (updater_dump_state(updater, state) < 0) {
            ST_WARNING("updater_dump_state.");
            return -1;
        }
    }

    if (updater_reset(updater) < 0) {
        ST_WARNING("Failed to updater_reset.");
        return -1;
    }

    if (updater->finalized) {
        if (updater_finish(updater) < 0) {
            ST_WARNING("Failed to updater_finish.");
            return -1;
        }
    }

    return 0;
}
