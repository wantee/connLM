/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Wang Jian
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

#include "../../glues/direct_glue.h"
#include "direct_glue_updater.h"

const unsigned int PRIMES[] = {
    108641969, 116049371, 125925907, 133333309, 145678979, 175308587,
    197530793, 234567803, 251851741, 264197411, 330864029, 399999781,
    407407183, 459258997, 479012069, 545678687, 560493491, 607407037,
    629629243, 656789717, 716048933, 718518067, 725925469, 733332871,
    753085943, 755555077, 782715551, 790122953, 812345159, 814814293,
    893826581, 923456189, 940740127, 953085797, 985184539, 990122807
};

const unsigned int PRIMES_SIZE = sizeof(PRIMES) / sizeof(PRIMES[0]);

typedef struct _dgu_data_t_ {
    hash_t *hash;
    unsigned int **P; /* coefficients of hash function. */
    int n_ctx;
    int positive; /* beginning positon of positive context. */
} dgu_data_t;

#define safe_dgu_data_destroy(ptr) do {\
    if((ptr) != NULL) {\
        dgu_data_destroy((dgu_data_t *)ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)

void dgu_data_destroy(dgu_data_t *data)
{
    int i;

    if (data == NULL) {
        return;
    }
    safe_free(data->hash);
    for (i = 0; i < data->n_ctx; i++) {
        safe_free(data->P[i]);
    }
    safe_free(data->P);
    data->n_ctx = 0;
    data->positive = 0;
}

dgu_data_t* dgu_data_init()
{
    dgu_data_t *data = NULL;

    data = (dgu_data_t *)malloc(sizeof(dgu_data_t));
    if (data == NULL) {
        ST_WARNING("Failed to malloc dgu_data.");
        goto ERR;
    }
    memset(data, 0, sizeof(dgu_data_t));

    return data;
ERR:
    safe_dgu_data_destroy(data);
    return NULL;
}

int dgu_data_setup(dgu_data_t *data, st_wt_int_t *context, int n_ctx)
{
    int a;
    int b;
    unsigned idx;

    ST_CHECK_PARAM(data == NULL, -1);

    data->n_ctx = n_ctx;

    data->hash = (hash_t *)malloc(sizeof(hash_t)*(n_ctx + 1));
    if (data->hash == NULL) {
        ST_WARNING("Failed to malloc hash.");
        goto ERR;
    }

    data->hash[0] = PRIMES[0] * PRIMES[1] * 1;

    data->P = (unsigned int **)malloc(sizeof(unsigned *)*n_ctx);
    if (data->P == NULL) {
        ST_WARNING("Failed to malloc P.");
        goto ERR;
    }

    data->positive = n_ctx;
    for (a = 0; a < n_ctx; a++) {
        if (context[a].i > 0) {
            data->positive = a;
        }
    }

    // negtive context coefficients
    for (a = 0; a < data->positive; a++) {
        data->P[a] = (unsigned int *)malloc(sizeof(unsigned)*(a + 1));
        if (data->P[a] == NULL) {
            ST_WARNING("Failed to malloc P[%d].", a);
            goto ERR;
        }

        for (b = 0; b <= a; b++) {
            idx = (-context[a].i) * PRIMES[(-context[b].i) % PRIMES_SIZE];
            idx += (-context[b].i);
            data->P[a][b] = PRIMES[idx % PRIMES_SIZE];
            if (b > 0) {
                data->P[a][b] *= data->P[a][b - 1];
            } else {
                data->P[a][b] *= data->hash[0];
            }
        }
    }

    // negtive context coefficients
    for (a = data->positive; a < n_ctx; a++) {
        data->P[a] = (unsigned int *)malloc(sizeof(unsigned)
                * (a - data->positive + 1));
        if (data->P[a] == NULL) {
            ST_WARNING("Failed to malloc P[%d].", a);
            goto ERR;
        }

        for (b = data->positive; b <= a; b++) {
            idx = context[a].i * PRIMES[context[b].i % PRIMES_SIZE];
            idx += context[b].i;
            data->P[a][b] = PRIMES[idx % PRIMES_SIZE];
            if (b > data->positive) {
                data->P[a][b] *= data->P[a][b - data->positive - 1];
            } else {
                data->P[a][b] *= data->hash[0];
            }
        }
    }

    return 0;
ERR:
    dgu_data_destroy(data);
    return -1;
}

void direct_glue_updater_destroy(glue_updater_t *glue_updater)
{
    if (glue_updater == NULL) {
        return;
    }

    safe_dgu_data_destroy(glue_updater->extra);
}

int direct_glue_updater_init(glue_updater_t *glue_updater)
{
    ST_CHECK_PARAM(glue_updater == NULL, -1);

    if (strcasecmp(glue_updater->glue->type, DIRECT_GLUE_NAME) != 0) {
        ST_WARNING("Not a direct glue_updater. [%s]",
                glue_updater->glue->type);
        return -1;
    }

    glue_updater->extra = (void *)dgu_data_init();
    if (glue_updater->extra == NULL) {
        ST_WARNING("Failed to dgu_data_init.");
        goto ERR;
    }

    return 0;

ERR:
    safe_dgu_data_destroy(glue_updater->extra);
    return -1;
}

static int direct_wt_get_hash_pos(hash_t *hash, hash_t init_val,
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

static int direct_wt_get_hash_neg(hash_t *hash, hash_t init_val,
        unsigned int **P, st_wt_int_t *context, int n_ctx, int *words,
        int n_word, int tgt_pos)
{
    int a;
    int b;

    // assert all context[i].i < 0
    for (a = n_ctx - 1; a >= 0; a--) {
        if (tgt_pos + context[a].i < 0) {
            return n_ctx - a - 1;
        }
        if (words[tgt_pos + context[a].i] < 0
                || words[tgt_pos + context[a].i] == UNK_ID) {
            // if OOV was in history, do not use
            // this N-gram feature and higher orders
            return n_ctx - a - 1;
        }

        hash[n_ctx - a - 1] = init_val;
        for (b = n_ctx - 1; b >= a; b--) {
            hash[n_ctx - a - 1] += P[n_ctx - a - 1][b - a]
                * (hash_t)(words[tgt_pos + context[b].i] + 1);
        }
    }

    return n_ctx;
}

int direct_glue_updater_setup(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, bool backprob)
{
    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL, -1);

    if (dgu_data_setup((dgu_data_t *)glue_updater->extra,
                comp_updater->comp->input->context,
                comp_updater->comp->input->n_ctx) < 0) {
        ST_WARNING("Failed to dgu_data_setup");
        return -1;
    }

    return 0;
}

int direct_glue_updater_forward(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, int *words, int n_word, int tgt_pos)
{
    dgu_data_t *data;
    direct_glue_data_t *glue_data;
    out_updater_t *out_updater;
    input_t *input;

    real_t *hash_wt;
    real_t scale;

    hash_t h;
    int future_order;
    int order;

    output_path_t *path;
    output_node_id_t node;
    output_node_id_t n;
    output_node_id_t ch;

    int a;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || words == NULL, -1);

    data = (dgu_data_t *)glue_updater->extra;
    glue_data = (direct_glue_data_t *)glue_updater->glue->extra;
    out_updater = comp_updater->out_updater;
    input = comp_updater->comp->input;

    hash_wt = glue_data->direct_wt->hash_wt;
    scale = glue_updater->glue->out_scales[0];

    /* history ngrams. */
    order = direct_wt_get_hash_neg(data->hash + 1, data->hash[0],
            data->P, input->context, data->positive,
            words, n_word, tgt_pos);
    if (order < 0) {
        ST_WARNING("Failed to direct_wt_get_hash history.");
        return -1;
    }

    if (input->n_ctx - data->positive > 0) {
        /* future ngrams. */
        future_order = direct_wt_get_hash_pos(data->hash + 1 + order,
                data->hash[0], data->P + data->positive,
                input->context + data->positive,
                input->n_ctx - data->positive, words, n_word, tgt_pos);
        if (future_order < 0) {
            ST_WARNING("Failed to direct_wt_get_hash future.");
            return -1;
        }
        order += future_order;
    }

    order += 1/* for hash[0]. */;

    for (a = 0; a < order; a++) {
        data->hash[a] = data->hash[a] % glue_data->hash_sz;
    }

    path = out_updater->output->paths + words[tgt_pos];
    for (a = 0; a < order; a++) {
        node = out_updater->output->tree->root;
        ch = s_children(out_updater->output->tree, node);
        h = data->hash[a] + ch;
        for (; ch < e_children(out_updater->output->tree, node); ch++) {
            if (h >= glue_data->hash_sz) {
                h = 0;
            }
            out_updater->ac[ch] += scale * hash_wt[h];
            h++;
        }
        for (n = 0; n < path->num_node; n++) {
            node = path->nodes[n];
            ch = s_children(out_updater->output->tree, node);
            h = data->hash[a] + ch;
            for (; ch < e_children(out_updater->output->tree, node); ch++) {
                if (h >= glue_data->hash_sz) {
                    h = 0;
                }
                out_updater->ac[ch] += scale * hash_wt[h];
                h++;
            }
        }
    }

    return 0;
}
