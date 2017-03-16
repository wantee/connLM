/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Wang Jian
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

#ifndef  _CONNLM_NGRAM_HASH_H_
#define  _CONNLM_NGRAM_HASH_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>

#include <connlm/config.h>

#include "vocab.h"

/** @defgroup g_ngram_hash N-gram Hash
 * A Hash function for n-gram.
 */

typedef uint64_t hash_t;
#define HASH_FMT "%llu"

/**
 * N-gram Hash
 * @ingroup g_ngram_hash
 */
typedef struct _ngram_hash_t_ {
    unsigned int *P; /**< coefficients of hash function, which is like
                         P0 + P0 * P1 * w1 + P0 * P1 * P2 * w2 + ... */
    int *context; /**< context of ngram. */
    int ctx_len; /**< length of context. */
} ngram_hash_t;

/**
 * Destroy a ngram hash and set the pointer to NULL.
 * @ingroup g_ngram_hash
 * @param[in] ptr pointer to ngram_hash_t.
 */
#define safe_ngram_hash_destroy(ptr) do {\
    if((ptr) != NULL) {\
        ngram_hash_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a ngram hash.
 * @ingroup g_ngram_hash
 * @param[in] nghash ngram hash to be destroyed.
 */
void ngram_hash_destroy(ngram_hash_t *nghash);

/**
 * Create a ngram hash.
 * @ingroup g_ngram_hash
 * @param[in] ctx context of ngram.
 * @param[in] ctx_len length of context.
 * @return ngram_hash on success, otherwise NULL.
 */
ngram_hash_t* ngram_hash_create(int *ctx, int ctx_len);

/**
 * Add an ngram a ngram hash.
 * @ingroup g_ngram_hash
 * @param[in] nghash the ngram hash.
 * @param[in] sent words in a sentence.
 * @param[in] sent_len number of words in a sentence.
 * @param[in] tgt_pos position of target word in sentence.
 * @param[in] init_val seed of hash.
 * @return non-zero value if any error.
 */
hash_t ngram_hash(ngram_hash_t *nghash, int *sent, int sent_len,
        int tgt_pos, hash_t init_val);

#ifdef __cplusplus
}
#endif

#endif
