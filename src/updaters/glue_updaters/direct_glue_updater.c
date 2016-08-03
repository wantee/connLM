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

#include "output.h"
#include "../../glues/direct_glue.h"
#include "../component_updater.h"

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
    int hash_order;
    unsigned int **P; /* coefficients of hash function.  the function is
                         P0 + P0 * P1 * w1 + P0 * P1 * P2 * w2 + ... */
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
    data->hash_order = 0;
    for (i = 0; i < data->n_ctx; i++) {
        safe_free(data->P[i]);
    }
    safe_free(data->P);
    data->n_ctx = 0;
    data->positive = 0;
}

dgu_data_t* dgu_data_init(glue_updater_t *glue_updater)
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
    glue_t *glue;

    ST_CHECK_PARAM(glue_updater == NULL, -1);

    glue= glue_updater->glue;
    if (strcasecmp(glue_updater->glue->type, DIRECT_GLUE_NAME) != 0) {
        ST_WARNING("Not a direct glue_updater. [%s]",
                glue_updater->glue->type);
        return -1;
    }

    glue_updater->wt_updater = wt_updater_create(&glue->param, glue->wt->mat,
            glue->wt->row, glue->wt->col, WT_UT_PART);
    if (glue_updater->wt_updater == NULL) {
        ST_WARNING("Failed to wt_updater_create.");
        goto ERR;
    }

    glue_updater->extra = (void *)dgu_data_init(glue_updater);
    if (glue_updater->extra == NULL) {
        ST_WARNING("Failed to dgu_data_init.");
        goto ERR;
    }

    return 0;

ERR:
    safe_dgu_data_destroy(glue_updater->extra);
    wt_updater_destroy(glue_updater->wt_updater);
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

static int direct_get_hash_neg(hash_t *hash, hash_t init_val,
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
        comp_updater_t *comp_updater, bool backprop)
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

typedef struct _direct_walker_args_t_ {
    out_updater_t *out_updater;
    real_t in_scale;
    real_t out_scale;
    hash_t h;

    wt_updater_t *wt_updater;
    count_t n_step;
} direct_walker_args_t;

static int direct_forward_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    direct_walker_args_t *dw_args;
    real_t *hash_wt;
    output_node_id_t ch;
    hash_t h;
    real_t scale;
    int hash_sz;

    dw_args = (direct_walker_args_t *) args;

    scale = dw_args->in_scale * dw_args->out_scale;
    hash_wt = dw_args->wt_updater->wt;

    if (output->norm == ON_SOFTMAX) {
        hash_sz = dw_args->wt_updater->row;

        h = dw_args->h + child_s;
        if (h > hash_sz) {
            h -= hash_sz;
        }

        if (h + child_e - child_s - 1 > hash_sz) {
            for (ch = child_s; h < hash_sz; ch++, h++) {
                dw_args->out_updater->ac[ch] += scale * hash_wt[h];
            }
            for (h = 0; ch < child_e - 1; ch++, h++) {
                dw_args->out_updater->ac[ch] += scale * hash_wt[h];
            }
        } else {
            for (ch = child_s; ch < child_e - 1; ch++, h++) {
                dw_args->out_updater->ac[ch] += scale * hash_wt[h];
            }
        }
    }

    return 0;
}

static int direct_compute_hash(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, int *words, int n_word, int tgt_pos)
{
    dgu_data_t *data;
    input_t *input;

    int future_order;
    int a;


    data = (dgu_data_t *)glue_updater->extra;
    input = comp_updater->comp->input;

    /* history ngrams. */
    data->hash_order = direct_get_hash_neg(data->hash + 1, data->hash[0],
            data->P, input->context, data->positive,
            words, n_word, tgt_pos);
    if (data->hash_order < 0) {
        ST_WARNING("Failed to direct_wt_get_hash history.");
        return -1;
    }

    if (input->n_ctx - data->positive > 0) {
        /* future ngrams. */
        future_order = direct_get_hash_pos(data->hash + 1 + data->hash_order,
                data->hash[0], data->P + data->positive,
                input->context + data->positive,
                input->n_ctx - data->positive, words, n_word, tgt_pos);
        if (future_order < 0) {
            ST_WARNING("Failed to direct_wt_get_hash future.");
            return -1;
        }
        data->hash_order += future_order;
    }

    data->hash_order += 1/* for hash[0]. */;

    for (a = 0; a < data->hash_order; a++) {
        data->hash[a] = data->hash[a] % glue_updater->wt_updater->row;
    }

    return 0;
}

int direct_glue_updater_forward(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, int *words, int n_word, int tgt_pos)
{
    dgu_data_t *data;
    out_updater_t *out_updater;

    direct_walker_args_t dw_args;
    int a;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || words == NULL, -1);

    data = (dgu_data_t *)glue_updater->extra;
    out_updater = comp_updater->out_updater;

    if (direct_compute_hash(glue_updater, comp_updater,
                words, n_word, tgt_pos) < 0) {
        ST_WARNING("Failed to direct_compute_hash.");
        return -1;
    }

    dw_args.out_updater = out_updater;
    dw_args.in_scale = glue_updater->glue->in_scales[0];
    dw_args.out_scale = glue_updater->glue->out_scales[0];
    dw_args.wt_updater = glue_updater->wt_updater;
    dw_args.n_step = -1;
    for (a = 0; a < data->hash_order; a++) {
        dw_args.h = data->hash[a];
        if (output_walk_through_path(out_updater->output, words[tgt_pos],
                    direct_forward_walker, (void *)&dw_args) < 0) {
            ST_WARNING("Failed to output_walk_through_path.");
            return -1;
        }
    }

    return 0;
}

static int direct_backprop_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    direct_walker_args_t *dw_args;
    st_int_seg_t seg;
    hash_t h;
    int hash_sz;

    dw_args = (direct_walker_args_t *) args;

    if (output->norm == ON_SOFTMAX) {
        if (child_e <= child_s + 1) {
            return 0;
        }

        hash_sz = dw_args->wt_updater->row;
        h = dw_args->h + child_s;
        if (h >= hash_sz) {
            h -= hash_sz;
        }

        seg.s = h;
        seg.n = child_e - child_s - 1;
        if (wt_update(dw_args->wt_updater, dw_args->n_step, &seg, -1,
                    dw_args->out_updater->er + child_s, dw_args->out_scale,
                    NULL, dw_args->in_scale, NULL) < 0) {
            ST_WARNING("Failed to wt_update.");
            return -1;
        }
    }

    return 0;
}

int direct_glue_updater_backprop(glue_updater_t *glue_updater, count_t n_step,
        comp_updater_t *comp_updater, int *words, int n_word, int tgt_pos)
{
    dgu_data_t *data;
    out_updater_t *out_updater;

    direct_walker_args_t dw_args;
    int a;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || words == NULL, -1);

    data = (dgu_data_t *)glue_updater->extra;
    out_updater = comp_updater->out_updater;

    dw_args.out_updater = out_updater;
    dw_args.in_scale = glue_updater->glue->in_scales[0];
    dw_args.out_scale = glue_updater->glue->out_scales[0];
    dw_args.wt_updater = glue_updater->wt_updater;
    dw_args.n_step = n_step;
    for (a = 0; a < data->hash_order; a++) {
        dw_args.h = data->hash[a];
        if (output_walk_through_path(out_updater->output, words[tgt_pos],
                    direct_backprop_walker, (void *)&dw_args) < 0) {
            ST_WARNING("Failed to output_walk_through_path.");
            return -1;
        }
    }

    if (wt_flush(glue_updater->wt_updater, n_step) < 0) {
        ST_WARNING("Failed to wt_flush.");
        return -1;
    }

    return 0;
}

int direct_glue_updater_forward_util_out(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, int *words, int n_word, int tgt_pos)
{
    if (direct_compute_hash(glue_updater, comp_updater,
                words, n_word, tgt_pos) < 0) {
        ST_WARNING("Failed to direct_compute_hash.");
        return -1;
    }

    return 0;
}

int direct_glue_updater_forward_out(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, output_node_id_t node)
{
    dgu_data_t *data;
    out_updater_t *out_updater;
    output_t *output;

    real_t *hash_wt;
    real_t scale;
    int hash_sz, a;
    hash_t h;

    output_node_id_t ch, child_s, child_e;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || node == OUTPUT_NODE_NONE, -1);

    data = (dgu_data_t *)glue_updater->extra;
    out_updater = comp_updater->out_updater;
    output = out_updater->output;

    child_s = s_children(output->tree, node);
    child_e = e_children(output->tree, node);

    if (child_s >= child_e) {
        return 0;
    }

    hash_sz = glue_updater->glue->wt->row;
    scale = glue_updater->glue->in_scales[0] * glue_updater->glue->out_scales[0];
    hash_wt = glue_updater->wt_updater->wt;

    if (output->norm == ON_SOFTMAX) {
        for (a = 0; a < data->hash_order; a++) {
            h = data->hash[a] + child_s;
            if (h > hash_sz) {
                h -= hash_sz;
            }

            if (h + child_e - child_s - 1 > hash_sz) {
                for (ch = child_s; h < hash_sz; ch++, h++) {
                    out_updater->ac[ch] += scale * hash_wt[h];
                }
                for (h = 0; ch < child_e - 1; ch++, h++) {
                    out_updater->ac[ch] += scale * hash_wt[h];
                }
            } else {
                for (ch = child_s; ch < child_e - 1; ch++, h++) {
                    out_updater->ac[ch] += scale * hash_wt[h];
                }
            }
        }
    }

    return 0;
}
