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
#include <stutils/st_mem.h>

#include "input_updater.h"

static void egs_input_destroy(egs_input_t *input)
{
    if (input == NULL) {
        return;
    }

    safe_st_free(input->words);
    safe_st_free(input->positions);
    safe_st_free(input->weights);

    input->num_words = 0;
    input->cap_words = 0;
}

static int egs_input_resize(egs_input_t *input, int n_ctx)
{
    ST_CHECK_PARAM(input == NULL, -1);

    if (n_ctx > input->cap_words) {
        input->words = (int *)st_realloc(input->words,
                sizeof(int) * n_ctx);
        if (input->words == NULL) {
            ST_WARNING("Failed to st_realloc input->words.");
            return -1;
        }
        memset(input->words, 0, sizeof(int) * (n_ctx - input->cap_words));

        input->positions = (int *)st_realloc(input->positions,
                sizeof(int) * n_ctx);
        if (input->positions == NULL) {
            ST_WARNING("Failed to st_realloc input->positions.");
            return -1;
        }
        memset(input->positions, 0, sizeof(int) * (n_ctx - input->cap_words));

        input->weights = (real_t *)st_realloc(input->weights,
                sizeof(real_t) * n_ctx);
        if (input->weights == NULL) {
            ST_WARNING("Failed to st_realloc input->weights.");
            return -1;
        }
        memset(input->weights, 0, sizeof(real_t) * (n_ctx - input->cap_words));

        input->cap_words = n_ctx;
    }

    return 0;
}

void egs_batch_destroy(egs_batch_t *batch)
{
    int i;

    if (batch == NULL) {
        return;
    }

    if (batch->inputs != NULL) {
        for (i = 0; i < batch->cap_egs; i++) {
            egs_input_destroy(batch->inputs + i);
        }
        safe_st_free(batch->inputs);
    }
    safe_st_free(batch->targets);

    batch->num_egs = 0;
    batch->cap_egs = 0;
}

static int egs_batch_resize(egs_batch_t *batch, int batch_size, int n_ctx)
{
    int i;

    ST_CHECK_PARAM(batch == NULL, -1);

    if (batch_size > batch->cap_egs) {
        batch->inputs = (egs_input_t *)st_realloc(batch->inputs,
                sizeof(egs_input_t) * batch_size);
        if (batch->inputs == NULL) {
            ST_WARNING("Failed to st_realloc batch->inputs.");
            return -1;
        }
        memset(batch->inputs, 0,
                sizeof(egs_input_t) * (batch_size - batch->cap_egs));

        batch->targets = (int *)st_realloc(batch->targets,
                sizeof(int) * batch_size);
        if (batch->targets == NULL) {
            ST_WARNING("Failed to st_realloc batch->targets.");
            return -1;
        }
        memset(batch->targets, 0,
                sizeof(int) * (batch_size - batch->cap_egs));

        batch->cap_egs = batch_size;
    }

    for (i = 0; i < batch->cap_egs; i++) {
        if (egs_input_resize(batch->inputs + i, n_ctx) < 0) {
            ST_WARNING("Failed to egs_input_resize [%d].", i);
            return -1;
        }
    }

    return 0;
}

void input_updater_destroy(input_updater_t *input_updater)
{
    if (input_updater == NULL) {
        return;
    }

    word_pool_destroy(&input_updater->wp);
    word_pool_destroy(&input_updater->tmp_wp);
    input_updater->cur_pos = 0;

    input_updater->ctx_leftmost = 0;
    input_updater->ctx_rightmost = 0;
}

input_updater_t* input_updater_create(int bos_id)
{
    input_updater_t *input_updater = NULL;

    input_updater = (input_updater_t *)st_malloc(sizeof(input_updater_t));
    if (input_updater == NULL) {
        ST_WARNING("Failed to st_malloc input_updater.");
        goto ERR;
    }
    memset(input_updater, 0, sizeof(input_updater_t));

    input_updater->bos_id = bos_id;

    return input_updater;

ERR:
    safe_input_updater_destroy(input_updater);
    return NULL;
}

int input_updater_clear(input_updater_t *input_updater)
{
    ST_CHECK_PARAM(input_updater == NULL, -1);

    input_updater->cur_pos = -1;

    return 0;
}

int input_updater_setup(input_updater_t *input_updater,
        int ctx_leftmost, int ctx_rightmost)
{
    ST_CHECK_PARAM(input_updater == NULL, -1);

    input_updater->cur_pos = -1;
    input_updater->ctx_leftmost = ctx_leftmost;
    input_updater->ctx_rightmost = ctx_rightmost;

    return 0;
}

int input_updater_update_batch(input_updater_t *input_updater,
        input_t *input, egs_batch_t *batch)
{
    egs_input_t *egs_input;
    word_pool_t *wp;
    int b, i;
    int cur_pos, row_start, idx;
    int ctx_leftmost = 0;
    int ctx_rightmost = 0;

    ST_CHECK_PARAM(input_updater == NULL || batch == NULL, -1);

    wp = &input_updater->wp;
    cur_pos = input_updater->cur_pos;

    if (input->context[0].i < 0) {
        ctx_leftmost = -input->context[0].i;
    }
    if (input->context[input->n_ctx - 1].i > 0) {
        ctx_rightmost = input->context[input->n_ctx - 1].i;
    }

    if (egs_batch_resize(batch, wp_batch_size(wp), input->n_ctx) < 0) {
        ST_WARNING("Failed to egs_batch_resize.");
        return -1;
    }

    batch->num_egs = 0;
    for (b = 0; b < wp_batch_size(wp); b++) {
        row_start = VEC_VAL(&wp->row_starts, b);

        // find sentence boundaries
        i = cur_pos - 1;
        while (i >= cur_pos - ctx_leftmost && i >= 0) {
            if (VEC_VAL(&wp->words, row_start + i) == SENT_END_ID) {
                break;
            }
            --i;
        }
        ctx_leftmost = -(i + 1 - cur_pos);

        i = cur_pos;
        while (i < cur_pos + ctx_rightmost
                && row_start + i + 1 < VEC_VAL(&wp->row_starts, b + 1)) {
            if (VEC_VAL(&wp->words, row_start + i) == SENT_END_ID) {
                break;
            }
            ++i;
        }
        ctx_rightmost = i - cur_pos;

        // add inputs
        egs_input = batch->inputs + batch->num_egs;
        egs_input->num_words = 0;
        for (i = 0; i < input->n_ctx; i++) {
            if (input->context[i].i < 0) {
                if (-input->context[i].i > ctx_leftmost) {
                    continue;
                }
            } else {
                if (input->context[i].i > ctx_rightmost) {
                    continue;
                }
            }

            assert(egs_input->num_words < egs_input->cap_words);

            idx = row_start + cur_pos + input->context[i].i;
            egs_input->words[egs_input->num_words] = VEC_VAL(&wp->words, idx);
            egs_input->weights[egs_input->num_words] = input->context[i].w;
            egs_input->positions[egs_input->num_words] = input->context[i].i;
            egs_input->num_words++;
        }

        // add target
        if (row_start + cur_pos >= VEC_VAL(&wp->row_starts, b + 1)
                // we never use <s> as target
                || VEC_VAL(&wp->words, row_start + cur_pos) == input_updater->bos_id) {
            batch->targets[batch->num_egs] = PADDING_ID;
        } else {
            batch->targets[batch->num_egs] = VEC_VAL(&wp->words, idx);
        }

        batch->num_egs++;
    }

    return 0;
}

int input_updater_feed(input_updater_t *input_updater, word_pool_t *new_wp)
{
    word_pool_t *wp, *tmp_wp;
    int b;
    int start, end;

    ST_CHECK_PARAM(input_updater == NULL || new_wp == NULL, -1);

    wp = &input_updater->wp;
    tmp_wp = &input_updater->tmp_wp;

    if (wp->words.size == 0) {
        if (word_pool_copy(wp, new_wp) < 0) {
            ST_WARNING("Failed to word_pool_copy.");
            return -1;
        }

        input_updater->cur_pos = 0;
    } else {
        // assert batch size is constanst, which is required by stateful network
        if (wp_batch_size(wp) != wp_batch_size(new_wp)) {
            ST_WARNING("Batch size can not be changed.");
            return -1;
        }

        if (word_pool_clear(tmp_wp) < 0) {
            ST_WARNING("Failed to word_pool_clear tmp_wp.");
            return -1;
        }
        if (ivec_append(&tmp_wp->row_starts, 0) < 0) {
            ST_WARNING("Failed to ivec_append first of tmp_wp->row_starts.");
            return -1;
        }
        for (b = 0; b < wp_batch_size(wp); b++) {
            start = VEC_VAL(&wp->row_starts, b) + input_updater->cur_pos
                - input_updater->ctx_leftmost;
            end = VEC_VAL(&wp->row_starts, b + 1);
            if (start < end) {
                if (ivec_extend(&tmp_wp->words, &wp->words, start, end) < 0) {
                    ST_WARNING("Failed to ivec_extend tmp_wp.words.");
                    return -1;
                }
            }
            if (ivec_append(&tmp_wp->row_starts, tmp_wp->words.size) < 0) {
                ST_WARNING("Failed to ivec_append tmp_wp->row_starts.");
                return -1;
            }
            // we do NOT take care of the sent_ends in word_pool any more
        }

        if (word_pool_swap(wp, tmp_wp) < 0) {
            ST_WARNING("Failed to word_pool_swap.");
            return -1;
        }

        input_updater->cur_pos = input_updater->ctx_leftmost;
    }

    return 0;
}

int input_updater_move(input_updater_t *input_updater)
{
    ST_CHECK_PARAM(input_updater == NULL, -1);

    input_updater->cur_pos++;

    return 0;
}

int input_updater_move_to_end(input_updater_t *input_updater)
{
    word_pool_t *wp;
    int b;

    ST_CHECK_PARAM(input_updater == NULL, -1);

    wp = &input_updater->wp;
    input_updater->cur_pos = 0;
    for (b = 0; b < wp_batch_size(wp); b++) {
        if (VEC_VAL(&wp->row_starts, b + 1) - VEC_VAL(&wp->row_starts, b)
                > input_updater->cur_pos) {
            input_updater->cur_pos = VEC_VAL(&wp->row_starts, b + 1)
                - VEC_VAL(&wp->row_starts, b);
        }
    }


    return 0;
}

bool input_updater_movable(input_updater_t *input_updater, bool finalized)
{
    word_pool_t *wp;
    int b, r, pos;

    ST_CHECK_PARAM(input_updater == NULL, false);

    wp = &input_updater->wp;

    if (finalized) {
        return true;
    }

    for (b = 0; b < wp_batch_size(wp); b++) {
        pos = VEC_VAL(&wp->row_starts, b) + input_updater->cur_pos;
        if (pos >= VEC_VAL(&wp->row_starts, b + 1)) {
            return false;
        }

        for (r = 1; r <= input_updater->ctx_rightmost; r++) {
            if (pos + r >= VEC_VAL(&wp->row_starts, b + 1)) {
                return false;
            }
            if (VEC_VAL(&wp->words, pos + r) == SENT_END_ID) {
                break;
            }
        }
    }


    return true;
}
