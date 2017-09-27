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

#ifndef  _CONNLM_INPUT_UPDATER_H_
#define  _CONNLM_INPUT_UPDATER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <connlm/config.h>

#include "reader.h"
#include "input.h"

/** @defgroup g_updater_input Updater for Input Layer
 * @ingroup g_updater
 * Perform forward and backprop on input layer.
 */

/**
 * Input updater.
 * @ingroup g_updater_input
 */
typedef struct _input_updater_t_ {
    int bos_id; /**< word id of SENT_START. */

    int ctx_leftmost; /**< leftmost for all input contexts. */
    int ctx_rightmost; /**< rightmost for all input contexts. */

    word_pool_t wp; /**< buffer for input words. */
    int cur_pos; /**< position of current word in word buffer. */

    word_pool_t tmp_wp; /**< temp buffer for input words. */
} input_updater_t;

/**
 * Destroy a input_updater and set the pointer to NULL.
 * @ingroup g_updater_input
 * @param[in] ptr pointer to updater_t.
 */
#define safe_input_updater_destroy(ptr) do {\
    if((ptr) != NULL) {\
        input_updater_destroy(ptr);\
        safe_st_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a input_updater.
 * @ingroup g_updater_input
 * @param[in] input_updater input_updater to be destroyed.
 */
void input_updater_destroy(input_updater_t *input_updater);

/**
 * Create a input_updater.
 * @ingroup g_updater_input
 * @param[in] bos_id word id of SENT_START.
 * @return input_updater on success, otherwise NULL.
 */
input_updater_t* input_updater_create(int bos_id);

/**
 * Setup input_updater for running.
 * @ingroup g_updater_input
 * @param[in] input_updater input_updater.
 * @param[in] ctx_leftmost leftmost of input contexts.
 * @param[in] ctx_rightmost rightmost of input contexts.
 * @return non-zero value if any error.
 */
int input_updater_setup(input_updater_t *input_updater,
        int ctx_leftmost, int ctx_rightmost);

/**
 * Input of a training examples,
 * @ingroup g_updater_input
 */
typedef struct _egs_input_t_ {
    int num_words; /**< number of words in this batch. */
    int *words; /**< input words. */
    real_t *weights; /**< weight of input words. */
    int *positions; /**< relative position of words. */
    int cap_words; /**< capacity of words, equal to num_contexts of input. */
} egs_input_t;

/**
 * Batch of training examples,
 * consist of a batch of pairs of input words and target word.
 * @ingroup g_updater_input
 */
typedef struct _egs_batch_t_ {
    int num_egs; /**< number of egs in this batch. */
    egs_input_t *inputs; /**< input of batch. */
    int *targets; /**< target words. */
    int cap_egs; /**< capacity of egs. */
} egs_batch_t;

void egs_batch_destroy(egs_batch_t *batch);

/**
 * Update batch for a input layer with word pool in input_updater.
 * @ingroup g_updater_input
 * @param[in] input_updater input_updater.
 * @param[in] input input layer.
 * @param[out] batch egs batch.
 * @return non-zero value if any error.
 */
int input_updater_update_batch(input_updater_t *input_updater,
        input_t *input, egs_batch_t *batch);

/**
 * Clear input_updater.
 * @ingroup g_updater_input
 * @param[in] input_updater input_updater.
 * @return non-zero value if any error.
 */
int input_updater_clear(input_updater_t *input_updater);

/**
 * Feed input words to a input_updater.
 * @ingroup g_updater_input
 * @param[in] input_updater input_updater.
 * @param[in] wp word pool.
 * @return non-zero value if any error.
 */
int input_updater_feed(input_updater_t *input_updater, word_pool_t *wp);

/**
 * Move forward a input word and return the sentence buffer.
 * @ingroup g_updater_input
 * @param[in] input_updater input_updater.
 * @return non-zero value if any error.
 */
int input_updater_move(input_updater_t *input_updater);

/**
 * Move forward input word to the end and return the sentence buffer.
 * @ingroup g_updater_input
 * @param[in] input_updater input_updater.
 * @return non-zero value if any error.
 */
int input_updater_move_to_end(input_updater_t *input_updater);

/**
 * Determine whethre can move a word from input
 * @ingroup g_updater_input
 * @param[in] input_updater the input_updater.
 * @param[in] finalized whether should be finalized
 * @return true if movable, otherwise false.
 */
bool input_updater_movable(input_updater_t *input_updater, bool finalized);

#ifdef __cplusplus
}
#endif

#endif
