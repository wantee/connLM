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

/** @defgroup g_updater_input Updater for Input Layer
 * @ingroup g_updater
 * Perform forward and backprop on input layer.
 */

/**
 * Input updater.
 * @ingroup g_updater_input
 */
typedef struct _input_updater_t_ {
    int ctx_leftmost; /**< leftmost for all input contexts. */
    int ctx_rightmost; /**< rightmost for all input contexts. */

    int *words; /**< buffer for input words. */
    int n_word; /**< number of input words. */
    int cap_word; /**< capacity of input words buffer. */
    int cur_pos; /**< position of current word in word buffer. */

    int sent_head; /**< head position of current sentence. */
    int sent_tail; /**< tail position of current sentence. */
} input_updater_t;

/**
 * Destroy a input_updater and set the pointer to NULL.
 * @ingroup g_updater_input
 * @param[in] ptr pointer to updater_t.
 */
#define safe_input_updater_destroy(ptr) do {\
    if((ptr) != NULL) {\
        input_updater_destroy(ptr);\
        safe_free(ptr);\
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
 * @return input_updater on success, otherwise NULL.
 */
input_updater_t* input_updater_create();

/**
 * Setup input_updater for running.
 * @ingroup g_updater_input
 * @param[in] input_updater input_updater.
 * @param[in] ctx_leftmost leftmost of input contexts.
 * @param[in] ctx_rightmost rightmost of input contexts.
 * @return non-zero value if any error.
 */
int input_updater_setup(input_updater_t *input_updater, int ctx_leftmost,
        int ctx_rightmost);

/**
 * Buffer of a (partial) sentence with the position of target word.
 * There must be no SENT_END except the last word.
 * @ingroup g_updater_input
 */
typedef struct _sentence_t_ {
    int *words; /**< words buffer. */
    int n_word; /**< number of words. */
    int tgt_pos; /**< position of target predicted word in sentence. */
} sent_t;

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
 * @param[in] words input words buffer.
 * @param[in] n_word number of input words.
 * @param[out] sent current sentence.
 * @return non-zero value if any error.
 */
int input_updater_feed(input_updater_t *input_updater, int *words, int n_word,
        sent_t *sent);

/**
 * Move forward a input word and return the sentence buffer.
 * @ingroup g_updater_input
 * @param[in] input_updater input_updater.
 * @param[out] sent sentence buffer.
 * @return non-zero value if any error.
 */
int input_updater_move(input_updater_t *input_updater, sent_t *sent);

#ifdef __cplusplus
}
#endif

#endif
