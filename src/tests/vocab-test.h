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

#ifndef  _CONNLM_VOCAB_TEST_H_
#define  _CONNLM_VOCAB_TEST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stutils/st_macro.h>

#include <connlm/config.h>

#include "vocab.h"

#define VOCAB_TEST_SIZE 17

vocab_t* vocab_test_new()
{
    vocab_opt_t vocab_opt;
    vocab_t *vocab = NULL;

    char word[16];
    int i;

    memset(&vocab_opt, 0, sizeof(vocab_opt_t));
    vocab_opt.max_alphabet_size = VOCAB_TEST_SIZE + 10;
    vocab = vocab_create(&vocab_opt);
    assert(vocab != NULL);
    for (i = 2; i < VOCAB_TEST_SIZE; i++) {
        word[0] = 'A' + i;
        word[1] = 'A' + i;
        word[2] = 'A' + i;
        word[3] = '\0';
        vocab_add_word(vocab, word);
    }
    vocab->vocab_size = VOCAB_TEST_SIZE;
    vocab->cnts = (count_t *)malloc(sizeof(count_t) * vocab->vocab_size);
    assert(vocab->cnts != NULL);
    for (i = 0; i < vocab->vocab_size; i++) {
        vocab->cnts[i] = vocab->vocab_size - i;
    }

    return vocab;
}

#ifdef __cplusplus
}
#endif

#endif
