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

#include <string.h>

#include <stutils/st_macro.h>
#include <stutils/st_utils.h>
#include <stutils/st_log.h>

#include "ngram_hash.h"

const unsigned int PRIMES[] = {
    108641969, 116049371, 125925907, 133333309, 145678979, 175308587,
    197530793, 234567803, 251851741, 264197411, 330864029, 399999781,
    407407183, 459258997, 479012069, 545678687, 560493491, 607407037,
    629629243, 656789717, 716048933, 718518067, 725925469, 733332871,
    753085943, 755555077, 782715551, 790122953, 812345159, 814814293,
    893826581, 923456189, 940740127, 953085797, 985184539, 990122807
};

const unsigned int PRIMES_SIZE = sizeof(PRIMES) / sizeof(PRIMES[0]);

void ngram_hash_destroy(ngram_hash_t *nghash);
{
    int i;

    if (nghash == NULL) {
        return;
    }
    for (i = 0; i < nghash->n_ctx; i++) {
        safe_free(nghash->P[i]);
    }
    safe_free(nghash->P);
    nghash->n_ctx = 0;
}

ngram_hash_t* ngram_hash_create(int *ctx, int ctx_len)
{
    ngram_hash_t *nghash = NULL;
    int a;
    unsigned idx;

    ST_CHECK_PARAM(ctx == NULL || ctx_len <= 0, NULL);

    nghash = (ngram_hash_t *)malloc(sizeof(ngram_hash_t));
    if (nghash == NULL) {
        ST_WARNING("Failed to malloc ngram_hash_t.");
        goto ERR;
    }

    nghash->ctx_len = ctx_len;

    nghash->P = (unsigned int *)malloc(sizeof(unsigned int) * ctx_len);
    if (nghash->P == NULL) {
        ST_WARNING("Failed to malloc P.");
        goto ERR;
    }

    for (a = 0; a < nghash->ctx_len; a++) {
        idx = abs(context[nghash->ctx_len-1]) * PRIMES[abs(context[a]) % PRIMES_SIZE];
        idx += abs(context[a]);
        data->P[a] = PRIMES[idx % PRIMES_SIZE];
        if (a > 0) {
            data->P[a] *= data->P[a - 1];
        } else {
            data->P[a] *= PRIMES[0] * PRIMES[1] * 1;
        }
    }

    return 0;
ERR:
    safe_ngram_hash_destroy(nghash);
    return -1;
}

static int direct_get_hash_pos(hash_t *hash, hash_t init_val,
        unsigned int **P, st_wt_int_t *context, int n_ctx, int *words,
        int n_word, int tgt_pos)
{
    int a;
    int b;

    // assert all context[i].i > 0
    for (a = 0; a < n_ctx; a++) {
        if (tgt_pos + context[a].i >= n_word) {
            return a;
        }
        if (words[tgt_pos + context[a].i] < 0
                || words[tgt_pos + context[a].i] == UNK_ID) {
            // if OOV was in future, do not use
            // this N-gram feature and higher orders
            return a;
        }

        hash[a] = init_val;
        for (b = 0; b <= a; b++) {
            hash[a] += P[a][b] * (hash_t)(words[tgt_pos+context[b].i] + 1);
        }
    }

    return n_ctx;
}

uint64_t ngram_hash(ngram_hash_t *nghash, int *sent, int sent_len,
        int tgt_pos, uint64_t init_val)
{
    uint64_t hash;
    int word;

    hash = nghash->P[0] * init_val;
    for (a = 0; a < nghash->ctx_len; a++) {
        if (tgt_pos + nghash->context[a] >= sent_len) {
            continue;
        } if (tgt_pos + nghash->context[a] >= 0) {
            word = sent[tgt_pos + nghash->context[a]];
        } else {
            word = 0; // <s>
        }
        hash += P[a]*(hash_t)(word + 1);
    }

    return hash;
}

static int direct_get_hash_neg(hash_t *hash, hash_t init_val,
        unsigned int **P, st_wt_int_t *context, int n_ctx, int *words,
        int n_word, int tgt_pos)
{
    int a;
    int b;
    int word;

    // assert all context[i].i < 0
    for (a = n_ctx - 1; a >= 0; a--) {
        if (tgt_pos + context[a].i < -1) {
            return n_ctx - a - 1;
        }
        if (tgt_pos + context[a].i >= 0) {
            if (words[tgt_pos + context[a].i] < 0
                    || words[tgt_pos + context[a].i] == UNK_ID) {
                // if OOV was in history, do not use
                // this N-gram feature and higher orders
                return n_ctx - a - 1;
            }
        }

        hash[n_ctx - a - 1] = init_val;
        for (b = n_ctx - 1; b >= a; b--) {
            if (tgt_pos + context[b].i >= 0) {
                word = words[tgt_pos + context[b].i];
            } else {
                word = 0; // <s>
            }
            hash[n_ctx - a - 1] += P[n_ctx - a - 1][b - a]*(hash_t)(word + 1);
        }
    }

    return n_ctx;
}
