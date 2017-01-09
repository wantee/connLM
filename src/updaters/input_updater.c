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
#include <stutils/st_mem.h>

#include "input_updater.h"

void input_updater_destroy(input_updater_t *input_updater)
{
    if (input_updater == NULL) {
        return;
    }

    safe_free(input_updater->words);
    input_updater->n_word = 0;
    input_updater->cap_word = 0;
    input_updater->cur_pos = 0;

    input_updater->ctx_leftmost = 0;
    input_updater->ctx_rightmost = 0;

    input_updater->sent_head = -1;
    input_updater->sent_tail = -1;
}

input_updater_t* input_updater_create()
{
    input_updater_t *input_updater = NULL;

    input_updater = (input_updater_t *)malloc(sizeof(input_updater_t));
    if (input_updater == NULL) {
        ST_WARNING("Failed to malloc input_updater.");
        goto ERR;
    }
    memset(input_updater, 0, sizeof(input_updater_t));

    return input_updater;

ERR:
    safe_input_updater_destroy(input_updater);
    return NULL;
}

int input_updater_clear(input_updater_t *input_updater)
{
    ST_CHECK_PARAM(input_updater == NULL, -1);

    input_updater->n_word = 0;
    input_updater->cur_pos = 0;
    input_updater->sent_head = -1;
    input_updater->sent_tail = -1;

    return 0;
}

int input_updater_setup(input_updater_t *input_updater, int ctx_leftmost,
        int ctx_rightmost)
{
    size_t sz;

    ST_CHECK_PARAM(input_updater == NULL, -1);

    input_updater->ctx_leftmost = ctx_leftmost;
    input_updater->ctx_rightmost = ctx_rightmost;

    input_updater->cap_word = ctx_leftmost + ctx_rightmost + 1;

    sz = sizeof(int) * input_updater->cap_word;
    input_updater->words = (int *)malloc(sz);
    if (input_updater->words == NULL) {
        ST_WARNING("Failed to malloc words.");
        goto ERR;
    }
    memset(input_updater->words, 0, sz);
    input_updater->n_word = 0;
    input_updater->cur_pos = 0;
    input_updater->sent_head = -1;
    input_updater->sent_tail = -1;

    return 0;

ERR:
    safe_free(input_updater->words);
    input_updater->n_word = 0;
    input_updater->cap_word = 0;
    input_updater->cur_pos = 0;
    input_updater->ctx_leftmost = 0;
    input_updater->ctx_rightmost = 0;
    input_updater->sent_head = -1;
    input_updater->sent_tail = -1;

    return -1;
}

static int input_updater_update_sent(input_updater_t *input_updater,
        sent_t *sent)
{
    int i;
    int next;

    ST_CHECK_PARAM(input_updater == NULL || sent == NULL, -1);

    if (input_updater->sent_head >= 0) {
        if (input_updater->words[input_updater->sent_tail-1] != SENT_END_ID) {
            // move util sentence end
            next = input_updater->sent_tail;
            input_updater->sent_tail = input_updater->n_word;
            for (i = next; i < input_updater->n_word; i++) {
                if (input_updater->words[i] == SENT_END_ID) {
                    input_updater->sent_tail = i + 1;
                    break;
                }
            }
        } else if (input_updater->cur_pos >= input_updater->sent_tail) {
            // move to next sentence
            input_updater->sent_head = input_updater->sent_tail;
            // move util sentence end
            next = input_updater->sent_tail;
            input_updater->sent_tail = input_updater->n_word;
            for (i = next; i < input_updater->n_word; i++) {
                if (input_updater->words[i] == SENT_END_ID) {
                    input_updater->sent_tail = i + 1;
                    break;
                }
            }
        }
    } else {
        input_updater->sent_head = 0;
        input_updater->sent_tail = input_updater->n_word;
        for (i = 0; i < input_updater->n_word; i++) {
            if (input_updater->words[i] == SENT_END_ID) {
                input_updater->sent_tail = i + 1;
                break;
            }
        }
    }

    sent->words = input_updater->words + input_updater->sent_head;
    sent->n_word = input_updater->sent_tail - input_updater->sent_head;
    if (input_updater->cur_pos < input_updater->n_word - 1
         && input_updater->words[input_updater->cur_pos] == SENT_START_ID) {
        // we never use <s> as target word, since P(<s>) should be equal to 1
        input_updater->cur_pos++;
    }
    sent->tgt_pos = input_updater->cur_pos - input_updater->sent_head;

    return 0;
}

int input_updater_feed(input_updater_t *input_updater, int *words, int n_word,
        sent_t *sent)
{
    int drop;

    ST_CHECK_PARAM(input_updater == NULL || words == NULL, -1);

    // drop words already done
    drop = input_updater->cur_pos - input_updater->ctx_leftmost;
    if (drop > 0) {
        memmove(input_updater->words, input_updater->words + drop,
                sizeof(int) * (input_updater->n_word - drop));
        input_updater->n_word -= drop;
        input_updater->cur_pos -= drop;
        input_updater->sent_head -= drop;
        input_updater->sent_tail -= drop;
    }

    // append new words
    if (input_updater->n_word + n_word > input_updater->cap_word) {
        input_updater->cap_word = input_updater->n_word + n_word;
        input_updater->words = (int *)realloc(input_updater->words,
                sizeof(int) * input_updater->cap_word);
        if (input_updater->words == NULL) {
            ST_WARNING("Failed to realloc words.");
            return -1;
        }
        //memset(input_updater->words + input_updater->n_word, 0,
        //    sizeof(int) * (input_updater->cap_word - input_updater->n_word));
    }
    memcpy(input_updater->words + input_updater->n_word, words,
            sizeof(int)*n_word);
    input_updater->n_word += n_word;

    if (input_updater_update_sent(input_updater, sent) < 0) {
        ST_WARNING("Failed to input_updater_update_sent.");
        return -1;
    }

    return 0;
}

int input_updater_move(input_updater_t *input_updater, sent_t *sent)
{
    ST_CHECK_PARAM(input_updater == NULL, -1);

    input_updater->cur_pos++;

    if (input_updater_update_sent(input_updater, sent) < 0) {
        ST_WARNING("Failed to input_updater_update_sent.");
        return -1;
    }

    return 0;
}

int input_updater_move_to_end(input_updater_t *input_updater, sent_t *sent)
{
    ST_CHECK_PARAM(input_updater == NULL, -1);

    input_updater->cur_pos = input_updater->n_word;

    if (input_updater_update_sent(input_updater, sent) < 0) {
        ST_WARNING("Failed to input_updater_update_sent.");
        return -1;
    }

    return 0;
}
