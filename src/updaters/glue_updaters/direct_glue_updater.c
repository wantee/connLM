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
#include <stutils/st_rand.h>

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
    hash_t **hash_vals;
    int *hash_orders;
    int batch_size;
    int cap_batches;

    unsigned int **P; /**< coefficients of hash function, which is like
                           P0 + P0 * P1 * w1 + P0 * P1 * P2 * w2 + ... */
    int num_feats;
} dgu_data_t;

#define safe_dgu_data_destroy(ptr) do {\
    if((ptr) != NULL) {\
        dgu_data_destroy((dgu_data_t *)ptr);\
        safe_st_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)

void dgu_data_destroy(dgu_data_t *data)
{
    int i;

    if (data == NULL) {
        return;
    }

    if (data->hash_vals != NULL) {
        for (i = 0; i < data->cap_batches; i++) {
            safe_st_free(data->hash_vals[i]);
        }
        safe_st_free(data->hash_vals);
    }
    safe_st_free(data->hash_orders);
    data->batch_size = 0;
    data->cap_batches = 0;

    if (data->P != NULL) {
        for (i = 0; i < data->num_feats; i++) {
            safe_st_free(data->P[i]);
        }
        safe_st_free(data->P);
    }
    data->num_feats = 0;
}

dgu_data_t* dgu_data_init(glue_updater_t *glue_updater)
{
    dgu_data_t *data = NULL;

    data = (dgu_data_t *)st_malloc(sizeof(dgu_data_t));
    if (data == NULL) {
        ST_ERROR("Failed to st_malloc dgu_data.");
        goto ERR;
    }
    memset(data, 0, sizeof(dgu_data_t));

    return data;
ERR:
    safe_dgu_data_destroy(data);
    return NULL;
}

int dgu_data_resize_batch(dgu_data_t *data, int batch_size)
{
    int b;

    if (batch_size > data->cap_batches) {
        data->hash_orders = (int *)st_realloc(data->hash_orders,
                sizeof(int) * batch_size);
        if (data->hash_orders == NULL) {
            ST_ERROR("Failed to st_malloc hash_orders.");
            return -1;
        }

        data->hash_vals = (hash_t **)st_realloc(data->hash_vals,
                sizeof(hash_t *) * batch_size);
        if (data->hash_vals == NULL) {
            ST_ERROR("Failed to st_malloc hash_vals.");
            return -1;
        }

        for (b = data->cap_batches; b < batch_size; b++) {
            data->hash_vals[b] = (hash_t *)st_malloc(
                    sizeof(hash_t) * (data->num_feats + 1));
            if (data->hash_vals[b] == NULL) {
                ST_ERROR("Failed to st_malloc hash_vals[%d].", b);
                return -1;
            }

            data->hash_vals[b][0] = PRIMES[0] * PRIMES[1] * 1;
        }

        data->cap_batches = batch_size;
    }

    data->batch_size = batch_size;

    return 0;
}

int dgu_data_setup(dgu_data_t *data, st_wt_int_t *features, int num_feats)
{
    int i, j;
    unsigned int idx;
    int positive;

    ST_CHECK_PARAM(data == NULL, -1);

    data->num_feats = num_feats;

    data->P = (unsigned int **)st_malloc(sizeof(unsigned *)*num_feats);
    if (data->P == NULL) {
        ST_ERROR("Failed to st_malloc P.");
        goto ERR;
    }
    positive = num_feats;
    for (i = num_feats - 1; i >= 0; i--) {
        if (features[i].i > 0) {
            positive = i;
        }
    }

    // negtive context coefficients
    for (i = 0; i < positive; i++) {
        data->P[i] = (unsigned int *)st_malloc(sizeof(unsigned)*(i + 1));
        if (data->P[i] == NULL) {
            ST_ERROR("Failed to st_malloc P[%d].", i);
            goto ERR;
        }

        for (j = 0; j <= i; j++) {
            idx = (-features[i].i) * PRIMES[(-features[j].i) % PRIMES_SIZE];
            idx += (-features[j].i);
            data->P[i][j] = PRIMES[idx % PRIMES_SIZE];
            if (j > 0) {
                data->P[i][j] *= data->P[i][j - 1];
            } else {
                data->P[i][j] *= PRIMES[0] * PRIMES[1];
            }
        }
    }

    // postive context coefficients
    for (i = positive; i < num_feats; i++) {
        data->P[i] = (unsigned int *)st_malloc(sizeof(unsigned)
                * (i - positive + 1));
        if (data->P[i] == NULL) {
            ST_ERROR("Failed to st_malloc P[%d].", i);
            goto ERR;
        }

        for (j = positive; j <= i; j++) {
            idx = features[i].i * PRIMES[features[j].i % PRIMES_SIZE];
            idx += features[j].i;
            data->P[i][j] = PRIMES[idx % PRIMES_SIZE];
            if (j > positive) {
                data->P[i][j] *= data->P[i][j - positive - 1];
            } else {
                data->P[i][j] *= PRIMES[0] * PRIMES[1];
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

    if (strcasecmp(glue->type, DIRECT_GLUE_NAME) != 0) {
        ST_ERROR("Not a direct glue_updater. [%s]", glue->type);
        return -1;
    }

    glue_updater->extra = (void *)dgu_data_init(glue_updater);
    if (glue_updater->extra == NULL) {
        ST_ERROR("Failed to dgu_data_init.");
        goto ERR;
    }

    return 0;

ERR:
    safe_dgu_data_destroy(glue_updater->extra);
    return -1;
}

static int direct_get_hash_pos(hash_t *hash, hash_t init_val,
        unsigned int **P, int *words, int n_word)
{
    int i, j;

    // assert all context[i].i > 0
    for (i = 0; i < n_word; i++) {
        if (words[i] < 0 || words[i] == UNK_ID) {
            // if OOV was in future, do not use
            // this N-gram feature and higher orders
            return i;
        }

        hash[i] = init_val;
        for (j = 0; j <= i; j++) {
            hash[i] += P[i][j] * (hash_t)(words[j] + 1);
        }
    }

    return n_word;
}

static int direct_get_hash_neg(hash_t *hash, hash_t init_val,
        unsigned int **P, int *words, int n_word)
{
    int i, j;

    // assert all context[i].i < 0
    for (i = n_word - 1; i >= 0; i--) {
        if (words[i] < 0 || words[i] == UNK_ID) {
            // if OOV was in history, do not use
            // this N-gram feature and higher orders
            return n_word - i - 1;
        }

        hash[n_word - i - 1] = init_val;
        for (j = n_word - 1; j >= i; j--) {
            hash[n_word - i - 1] += P[n_word - i - 1][j - i]
                * (hash_t)(words[j] + 1);
        }
    }

    return n_word;
}

int direct_glue_updater_setup(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, bool backprop)
{
    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL, -1);

    if (dgu_data_setup((dgu_data_t *)glue_updater->extra,
                comp_updater->comp->input->context,
                comp_updater->comp->input->n_ctx) < 0) {
        ST_ERROR("Failed to dgu_data_setup");
        return -1;
    }

    return 0;
}

static int forward_one_node(output_norm_t norm, output_node_id_t node,
        output_node_id_t child_s, output_node_id_t child_e,
        real_t *hash_wt, size_t hash_sz,
        hash_t *hash_vals, int hash_order, real_t *out_ac, real_t scale,
        real_t *keep_mask, real_t keep_prob, unsigned int *rand_seed)
{
    hash_t h;
    output_node_id_t ch;
    double p;
    int a;

    ST_CHECK_PARAM(out_ac == NULL, -1);

    for (a = 0; a < hash_order; a++) {
        h = hash_vals[a] + child_s;
        if (h > hash_sz) {
            h -= hash_sz;
        }

        if (keep_mask != NULL) {
            for (ch = child_s; ch < child_e - 1; ch++) {
                p = st_random_r(0, 1, rand_seed);
                if (p < keep_prob) {
                    keep_mask[ch] = 1.0;
                } else {
                    keep_mask[ch] = 0.0;
                }
            }

            if (h + child_e - child_s - 1 > hash_sz) {
                for (ch = child_s; h < hash_sz; ch++, h++) {
                    if (keep_mask[ch] == 1.0) {
                        out_ac[ch - child_s] += scale * hash_wt[h] / keep_prob;
                    }
                }
                for (h = 0; ch < child_e - 1; ch++, h++) {
                    if (keep_mask[ch] == 1.0) {
                        out_ac[ch - child_s] += scale * hash_wt[h] / keep_prob;
                    }
                }
            } else {
                for (ch = child_s; ch < child_e - 1; ch++, h++) {
                    if (keep_mask[ch] == 1.0) {
                        out_ac[ch - child_s] += scale * hash_wt[h] / keep_prob;
                    }
                }
            }
        } else {
            if (h + child_e - child_s - 1 > hash_sz) {
                for (ch = child_s; h < hash_sz; ch++, h++) {
                    out_ac[ch - child_s] += scale * hash_wt[h];
                }
                for (h = 0; ch < child_e - 1; ch++, h++) {
                    out_ac[ch - child_s] += scale * hash_wt[h];
                }
            } else {
                for (ch = child_s; ch < child_e - 1; ch++, h++) {
                    out_ac[ch - child_s] += scale * hash_wt[h];
                }
            }
        }
    }

    return 0;
}

typedef struct _direct_forward_walker_args_t_ {
    real_t scale;

    mat_t *node_out_acs;
    int *node_iters;

    real_t *hash_wt;
    hash_t hash_sz;
    hash_t *hash_vals;
    int hash_order;

    real_t *keep_mask;
    real_t keep_prob;
    unsigned int *rand_seed;
} direct_fwd_walker_args_t;

static int direct_forward_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    direct_fwd_walker_args_t *dfw_args;

    dfw_args = (direct_fwd_walker_args_t *) args;

    if (child_e - child_s <= 1) {
        return 0;
    }

    if (forward_one_node(output->norm, node,
                child_s, child_e, dfw_args->hash_wt, dfw_args->hash_sz,
                dfw_args->hash_vals, dfw_args->hash_order,
                MAT_VALP(dfw_args->node_out_acs + node, dfw_args->node_iters[node], 0),
                dfw_args->scale, dfw_args->keep_mask, dfw_args->keep_prob,
                dfw_args->rand_seed) < 0) {
        ST_ERROR("Failed to forward_one_node");
        return -1;
    }

    dfw_args->node_iters[node]++;

    return 0;
}

static int direct_compute_hash(glue_updater_t *glue_updater, int batch_id,
        egs_input_t *input)
{
    dgu_data_t *data;

    int positive;
    int future_order;
    int i;

    data = (dgu_data_t *)glue_updater->extra;

    positive = input->num_words;
    for (i = input->num_words - 1; i >= 0; i--) {
        if (input->positions[i] > 0) {
            positive = i;
        }
    }
    /* history ngrams. */
    data->hash_orders[batch_id] = direct_get_hash_neg(
            data->hash_vals[batch_id] + 1, data->hash_vals[batch_id][0],
            data->P, input->words, positive);
    if (data->hash_orders < 0) {
        ST_ERROR("Failed to direct_wt_get_hash history.");
        return -1;
    }

    if (input->num_words - positive > 0) {
        /* future ngrams. */
        future_order = direct_get_hash_pos(
                data->hash_vals[batch_id] + 1 + data->hash_orders[batch_id],
                data->hash_vals[batch_id][0],
                data->P + positive,
                input->words + positive,
                input->num_words - positive);
        if (future_order < 0) {
            ST_ERROR("Failed to direct_wt_get_hash future.");
            return -1;
        }
        data->hash_orders[batch_id] += future_order;
    }

    data->hash_orders[batch_id] += 1/* for hash_vals[0]. */;

    for (i = 0; i < data->hash_orders[batch_id]; i++) {
        data->hash_vals[batch_id][i] %= glue_updater->wt_updaters[0]->wt.num_cols;
    }

    return 0;
}

static int reset_node_iters_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    int *node_iters;

    node_iters = (int *) args;

    node_iters[node] = 0;

    return 0;
}

static int reset_node_iters(out_updater_t *out_updater, egs_batch_t *batch)
{
    int b;

    for (b = 0; b < batch->num_egs; b++) {
        if (batch->targets[b] == PADDING_ID) {
            continue;
        }
        if (output_walk_through_path(out_updater->output, batch->targets[b],
                    reset_node_iters_walker, (void *)out_updater->node_iters) < 0) {
            ST_ERROR("Failed to output_walk_through_path.");
            return -1;
        }
    }

    return 0;
}

int direct_glue_updater_prepare(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater /* unused */, egs_batch_t *batch)
{
    dgu_data_t *data;

    ST_CHECK_PARAM(glue_updater == NULL || batch == NULL, -1);

    data = (dgu_data_t *)glue_updater->extra;

    if (dgu_data_resize_batch(data, batch->num_egs) < 0) {
        ST_ERROR("Failed to dgu_data_resize_batch.");
        return -1;
    }

    return 0;
}

int direct_glue_updater_forward(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, egs_batch_t *batch,
        mat_t* in_ac /* unused */, mat_t *out_ac /* unused */)
{
    dgu_data_t *data;
    out_updater_t *out_updater;

    direct_fwd_walker_args_t dfw_args;
    int b;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || batch == NULL, -1);

    data = (dgu_data_t *)glue_updater->extra;
    out_updater = comp_updater->out_updater;

    if (reset_node_iters(out_updater, batch) < 0) {
        ST_ERROR("Failed to reset_node_iters.");
        return -1;
    }

    dfw_args.scale = comp_updater->comp->comp_scale;

    dfw_args.node_out_acs = comp_updater->out_updater->node_acs;
    dfw_args.node_iters = comp_updater->out_updater->node_iters;

    dfw_args.hash_wt = MAT_VALP(&glue_updater->wt_updaters[0]->wt, 0, 0);
    dfw_args.hash_sz = glue_updater->wt_updaters[0]->wt.num_cols;

    dfw_args.keep_mask = NULL;
    dfw_args.keep_prob = glue_updater->keep_prob;
    dfw_args.rand_seed = glue_updater->rand_seed;
    for (b = 0; b < batch->num_egs; b++) {
        if (batch->targets[b] == PADDING_ID) {
            continue;
        }

        // TODO: instead of computing hash_vals here in advance,
        //       move this function inside direct_forward_walker
        //       with node_id as init_val, so that we can get different
        //       hash_vals on different tree nodes.
        if (direct_compute_hash(glue_updater, b, batch->inputs + b) < 0) {
            ST_ERROR("Failed to direct_compute_hash.");
            return -1;
        }

        dfw_args.hash_vals = data->hash_vals[b];
        dfw_args.hash_order = data->hash_orders[b];
        if (glue_updater->keep_mask.num_rows > 0) {
            dfw_args.keep_mask = MAT_VALP(&glue_updater->keep_mask, b, 0);
        }
        if (output_walk_through_path(out_updater->output,
                    batch->targets[b],
                    direct_forward_walker, (void *)&dfw_args) < 0) {
            ST_ERROR("Failed to output_walk_through_path.");
            return -1;
        }
    }

    return 0;
}

typedef struct _direct_backprop_walker_args_t_ {
    real_t scale;

    mat_t *node_out_ers;
    int *node_iters;

    wt_updater_t *wt_updater;

    hash_t *hash_vals;
    int hash_order;

    real_t *keep_mask;
    real_t *dropout_val;
} direct_bp_walker_args_t;

static int direct_backprop_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    direct_bp_walker_args_t *dbw_args;
    real_t *out_er;
    real_t *keep_mask;
    real_t *node_out_er;

    mat_t out_er_mat = {0};
    st_size_seg_t seg;
    hash_t h;
    size_t hash_sz;
    int a, ch;

    dbw_args = (direct_bp_walker_args_t *) args;

    if (child_e - child_s <= 1) {
        return 0;
    }

    node_out_er = MAT_VALP(dbw_args->node_out_ers + node, dbw_args->node_iters[node], 0);

    hash_sz = dbw_args->wt_updater->wt.num_cols;

    if (dbw_args->keep_mask != NULL) {
        out_er = dbw_args->dropout_val + child_s;
        keep_mask = dbw_args->keep_mask + child_s;
        for (ch = 0; ch < child_e - child_s - 1; ch++) {
            if (keep_mask[ch]) {
                out_er[ch] = node_out_er[ch];
            } else {
                out_er[ch] = 0.0;
            }
        }
    } else {
        out_er = node_out_er;
    }

    for (a = 0; a < dbw_args->hash_order; a++) {
        h = dbw_args->hash_vals[a] + child_s;
        if (h >= hash_sz) {
            h -= hash_sz;
        }

        seg.s = h;
        seg.n = child_e - child_s - 1;
        mat_from_array(&out_er_mat, out_er, seg.n, true);
        if (wt_update(dbw_args->wt_updater, &out_er_mat, dbw_args->scale,
                    NULL, 1.0, &seg, NULL) < 0) {
            ST_ERROR("Failed to wt_update.");
            return -1;
        }
    }

    dbw_args->node_iters[node]++;

    return 0;
}

int direct_glue_updater_backprop(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, egs_batch_t *batch,
        mat_t *in_ac, mat_t *out_er, mat_t *in_er)
{
    dgu_data_t *data;
    out_updater_t *out_updater;

    direct_bp_walker_args_t dbw_args;
    int b;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || batch == NULL, -1);

    data = (dgu_data_t *)glue_updater->extra;
    out_updater = comp_updater->out_updater;

    if (reset_node_iters(out_updater, batch) < 0) {
        ST_ERROR("Failed to reset_node_iters.");
        return -1;
    }

    dbw_args.scale = comp_updater->comp->comp_scale;
    dbw_args.node_out_ers = out_updater->node_ers;
    dbw_args.node_iters = out_updater->node_iters;
    dbw_args.wt_updater = glue_updater->wt_updaters[0];
    dbw_args.keep_mask = NULL;
    dbw_args.dropout_val = NULL;
    for (b = 0; b < batch->num_egs; b++) {
        if (batch->targets[b] == PADDING_ID) {
            continue;
        }

        dbw_args.hash_vals = data->hash_vals[b];
        dbw_args.hash_order = data->hash_orders[b];
        if (glue_updater->keep_mask.num_rows > 0) {
            dbw_args.keep_mask = MAT_VALP(&glue_updater->keep_mask, b, 0);
            dbw_args.dropout_val = MAT_VALP(&glue_updater->dropout_val, b, 0);
        }
        if (output_walk_through_path(out_updater->output,
                    batch->targets[b],
                    direct_backprop_walker, (void *)&dbw_args) < 0) {
            ST_ERROR("Failed to output_walk_through_path.");
            return -1;
        }
    }

    return 0;
}

int direct_glue_updater_forward_util_out(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, egs_batch_t *batch,
        mat_t* in_ac /* unused */, mat_t* out_ac /* unused */)
{
    out_updater_t *out_updater;
    int b;

    out_updater = comp_updater->out_updater;

    for (b = 0; b < batch->num_egs; b++) {
        if (direct_compute_hash(glue_updater, b, batch->inputs + b) < 0) {
            ST_ERROR("Failed to direct_compute_hash.");
            return -1;
        }
    }

    if (reset_node_iters(out_updater, batch) < 0) {
        ST_ERROR("Failed to reset_node_iters.");
        return -1;
    }

    return 0;
}

int direct_glue_updater_forward_out(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, output_node_id_t node,
        mat_t* in_ac /* unused */, mat_t *out_ac /* unused */)
{
    out_updater_t *out_updater;
    output_t *output;
    dgu_data_t *data;

    output_node_id_t child_s, child_e;
    int b;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || node == OUTPUT_NODE_NONE, -1);

    out_updater = comp_updater->out_updater;
    output = out_updater->output;

    child_s = s_children(output->tree, node);
    child_e = e_children(output->tree, node);

    if (child_e - child_s <= 1) {
        return 0;
    }

    data = (dgu_data_t *)glue_updater->extra;

    for (b = 0; b < data->batch_size; b++) {
        if (forward_one_node(output->norm, node,
                    child_s, child_e,
                    MAT_VALP(&glue_updater->wt_updaters[0]->wt, 0, 0),
                    glue_updater->wt_updaters[0]->wt.num_cols,
                    data->hash_vals[b], data->hash_orders[b],
                    MAT_VALP(out_updater->node_acs + node,
                        out_updater->node_iters[node], 0),
                    comp_updater->comp->comp_scale,
                    NULL, 0.0, NULL) < 0) {
            ST_ERROR("Failed to forward_one_node");
            return -1;
        }
    }
    out_updater->node_iters[node]++;

    return 0;
}

static int forward_out_words_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    direct_fwd_walker_args_t *dfw_args;

    dfw_args = (direct_fwd_walker_args_t *)args;

    if (child_e - child_s <= 1) {
        return 0;
    }

    if (dfw_args->node_iters[node] != 0) {
        // already forwarded
        return 0;
    }

    if (forward_one_node(output->norm, node, child_s, child_e,
                dfw_args->hash_wt, dfw_args->hash_sz,
                dfw_args->hash_vals, dfw_args->hash_order,
                MAT_VALP(dfw_args->node_out_acs + node, 0, 0),
                dfw_args->scale, NULL, 0.0, NULL) < 0) {
        ST_ERROR("Failed to forward_one_node["OUTPUT_NODE_FMT"]", node);
        return -1;
    }

    dfw_args->node_iters[node] = 1;

    return 0;
}

int direct_glue_updater_forward_out_words(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, ivec_t *words,
        mat_t* in_ac /* unused */, mat_t *out_ac /* unused */)
{
    direct_fwd_walker_args_t dfw_args;

    out_updater_t *out_updater;
    dgu_data_t *data;

    int i;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || words == NULL, -1);

    out_updater = comp_updater->out_updater;
    data = (dgu_data_t *)glue_updater->extra;

    if (in_ac->num_rows != 1) {
        ST_ERROR("Only support batch_size == 1.");
        return -1;
    }

    if (out_updater_reset_iters(out_updater, words) < 0) {
        ST_ERROR("Failed to out_updater_reset_iters.");
        return -1;
    }

    dfw_args.scale = comp_updater->comp->comp_scale;
    dfw_args.node_out_acs = out_updater->node_acs;
    dfw_args.node_iters = out_updater->node_iters;

    dfw_args.hash_wt = MAT_VALP(&glue_updater->wt_updaters[0]->wt, 0, 0);
    dfw_args.hash_sz = glue_updater->wt_updaters[0]->wt.num_cols;
    dfw_args.hash_vals = data->hash_vals[0];
    dfw_args.hash_order = data->hash_orders[0];
    for (i = 0; i < words->size; i++) {
        if (VEC_VAL(words, i) == PADDING_ID) {
            continue;
        }
        if (output_walk_through_path(out_updater->output, VEC_VAL(words, i),
                    forward_out_words_walker, (void *)&dfw_args) < 0) {
            ST_ERROR("Failed to output_walk_through_path for forward_out_words.");
            return -1;
        }
    }

    return 0;
}

int direct_glue_updater_gen_keep_mask(glue_updater_t *glue_updater, int batch_size)
{
    if (mat_resize_row(&glue_updater->keep_mask, batch_size, NAN) < 0) {
        ST_ERROR("Failed to mat_resize_row for keep_mask.");
        return -1;
    }
    if (mat_resize(&glue_updater->dropout_val,
                glue_updater->keep_mask.num_rows,
                glue_updater->keep_mask.num_cols, NAN) < 0) {
        ST_ERROR("Failed to mat_resize for dropout_val.");
        return -1;
    }
    /* keep_mask will be generated on the fly. */

    return 0;
}
