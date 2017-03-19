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

void ngram_hash_destroy(ngram_hash_t *nghash)
{
    if (nghash == NULL) {
        return;
    }
    safe_free(nghash->P);
    safe_free(nghash->context);
    nghash->ctx_len = 0;
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

    nghash->context = (int *)malloc(sizeof(int) * ctx_len);
    if (nghash->context == NULL) {
        ST_WARNING("Failed to malloc context.");
        goto ERR;
    }
    memcpy(nghash->context, ctx, sizeof(int) * ctx_len);
    nghash->ctx_len = ctx_len;

    nghash->P = (unsigned int *)malloc(sizeof(unsigned int) * ctx_len);
    if (nghash->P == NULL) {
        ST_WARNING("Failed to malloc P.");
        goto ERR;
    }

    for (a = 0; a < ctx_len; a++) {
        idx = (unsigned)ctx[ctx_len - 1] * PRIMES[(unsigned)ctx[a] % PRIMES_SIZE];
        idx += (unsigned)ctx[a];
        nghash->P[a] = PRIMES[idx % PRIMES_SIZE];
        if (a > 0) {
            nghash->P[a] *= nghash->P[a - 1];
        } else {
            nghash->P[a] *= PRIMES[0] * PRIMES[1];
        }
    }

    return nghash;
ERR:
    safe_ngram_hash_destroy(nghash);
    return NULL;
}

hash_t ngram_hash(ngram_hash_t *nghash, int *sent, int sent_len,
        int tgt_pos, hash_t init_val)
{
    hash_t hash;
    int word;

    int a;

    hash = PRIMES[0] * PRIMES[1] * init_val;
    for (a = 0; a < nghash->ctx_len; a++) {
        if (tgt_pos + nghash->context[a] < 0
                || tgt_pos + nghash->context[a] >= sent_len) {
            continue;
        }
        word = sent[tgt_pos + nghash->context[a]];
        hash += nghash->P[a]*(hash_t)(word + 1);
    }

    return hash;
}
