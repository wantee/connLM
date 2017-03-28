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

#include "ngram_hash.h"
#include "output.h"
#include "../../glues/direct_glue.h"
#include "../component_updater.h"

#include "direct_glue_updater.h"

typedef struct _dgu_data_t_ {
    hash_t *hash_vals;
    ngram_hash_t **nghashes;
    int num_features;
    int positive; /* beginning positon of positive context. */

    int hash_order;
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

    if (data->nghashes != NULL) {
        for (i = 0; i < data->num_features; i++) {
            safe_ngram_hash_destroy(data->nghashes[i]);
        }
        safe_st_free(data->nghashes);
    }

    safe_st_free(data->hash_vals);
}

dgu_data_t* dgu_data_init(glue_updater_t *glue_updater)
{
    dgu_data_t *data = NULL;

    data = (dgu_data_t *)st_malloc(sizeof(dgu_data_t));
    if (data == NULL) {
        ST_WARNING("Failed to st_malloc dgu_data.");
        goto ERR;
    }
    memset(data, 0, sizeof(dgu_data_t));

    return data;
ERR:
    safe_dgu_data_destroy(data);
    return NULL;
}

int dgu_data_setup(dgu_data_t *data, st_wt_int_t *features, int n_feat)
{
    int a;
    int b;

    int *context = NULL;
    int max_order, pos;
    int n_ctx;

    ST_CHECK_PARAM(data == NULL, -1);

    data->num_features = n_feat;
    max_order = 0;
    data->positive = n_feat;
    for (a = n_feat - 1; a >= 0; a--) {
        if (features[a].i > 0) {
            data->positive = a;
        }
        if (abs(features[a].i) > max_order) {
            max_order = abs(features[a].i);
        }
    }

    data->nghashes = (ngram_hash_t **)st_malloc(sizeof(ngram_hash_t*) * n_feat);
    if (data->nghashes == NULL) {
        ST_WARNING("Failed to st_malloc nghashes.");
        goto ERR;
    }
    context = (int *)st_malloc(sizeof(int) * max_order);
    if (context == NULL) {
        ST_WARNING("Failed to st_malloc context.");
        goto ERR;
    }

    for (a = 0; a < n_feat; a++) {
        pos = features[a].i;
        n_ctx = 0;
        if (pos < 0) {
            while (-pos > 0) {
                context[n_ctx] = pos;
                ++n_ctx;
                ++pos;
            }
        } else {
            b = 1;
            while (b <= pos) {
                context[n_ctx] = b;
                ++n_ctx;
                ++b;
            }
        }

        data->nghashes[a] = ngram_hash_create(context, n_ctx);
        if (data->nghashes[a] == NULL) {
            ST_WARNING("Failed to ngram_hash_create for [%d]th feature.", a);
            goto ERR;
        }
    }

    data->hash_vals = (hash_t *)st_malloc(sizeof(hash_t)*(n_feat + 1));
    if (data->hash_vals == NULL) {
        ST_WARNING("Failed to st_malloc hash_vals.");
        goto ERR;
    }
    data->hash_vals[0] = (hash_t)(108641969L * 116049371L); // PRIMES[0] * PRIMES[1] in ngram_hash

    safe_st_free(context);
    return 0;
ERR:
    dgu_data_destroy(data);
    safe_st_free(context);
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
        ST_WARNING("Not a direct glue_updater. [%s]", glue->type);
        return -1;
    }

    glue_updater->extra = (void *)dgu_data_init(glue_updater);
    if (glue_updater->extra == NULL) {
        ST_WARNING("Failed to dgu_data_init.");
        goto ERR;
    }

    return 0;

ERR:
    safe_dgu_data_destroy(glue_updater->extra);
    return -1;
}

static int direct_get_hash_pos(hash_t *hash_vals, hash_t init_val,
        ngram_hash_t **nghashes, int n_nghashes, int *words,
        int n_word, int tgt_pos)
{
    ngram_hash_t *nghash;
    int max_pos;

    int a;

    // assert all context[i] > 0
    for (a = 0; a < n_nghashes; a++) {
        nghash = nghashes[a];
        max_pos = tgt_pos + nghash->context[nghash->ctx_len - 1];
        if (max_pos >= n_word) {
            return a;
        }
        if (words[max_pos] < 0 || words[max_pos] == UNK_ID) {
            // if OOV was in future, do not use
            // this N-gram feature and higher orders
            return a;
        }

        hash_vals[a] = ngram_hash(nghash, words, n_word, tgt_pos, init_val);
    }

    return n_nghashes;
}

static int direct_get_hash_neg(hash_t *hash_vals, hash_t init_val,
        ngram_hash_t **nghashes, int n_nghashes, int *words,
        int n_word, int tgt_pos)
{
    ngram_hash_t *nghash;
    int max_pos;

    int a;

    // assert all context[i] < 0
    for (a = n_nghashes - 1; a >= 0; a--) {
        nghash = nghashes[a];
        max_pos = tgt_pos + nghash->context[0];
        if (max_pos < 0) {
            return n_nghashes - a - 1;
        }
        if (words[max_pos] < 0 || words[max_pos] == UNK_ID) {
            // if OOV was in history, do not use
            // this N-gram feature and higher orders
            return n_nghashes - a - 1;
        }

        hash_vals[n_nghashes - a - 1] = ngram_hash(nghash, words, n_word,
                tgt_pos, init_val);
    }

    return n_nghashes;
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
    real_t comp_scale;
    real_t *out_ac;
    real_t *out_er;
    hash_t h;

    wt_updater_t *wt_updater;
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

    scale = dw_args->comp_scale;
    hash_wt = dw_args->wt_updater->wt;

    if (output->norm == ON_SOFTMAX) {
        hash_sz = dw_args->wt_updater->row;

        h = dw_args->h + child_s;
        if (h > hash_sz) {
            h -= hash_sz;
        }

        if (h + child_e - child_s - 1 > hash_sz) {
            for (ch = child_s; h < hash_sz; ch++, h++) {
                dw_args->out_ac[ch] += scale * hash_wt[h];
            }
            for (h = 0; ch < child_e - 1; ch++, h++) {
                dw_args->out_ac[ch] += scale * hash_wt[h];
            }
        } else {
            for (ch = child_s; ch < child_e - 1; ch++, h++) {
                dw_args->out_ac[ch] += scale * hash_wt[h];
            }
        }
    }

    return 0;
}

static int direct_compute_hash(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, sent_t *input_sent)
{
    dgu_data_t *data;

    int future_order;
    int a;

    data = (dgu_data_t *)glue_updater->extra;

    /* history ngrams. */
    data->hash_order = direct_get_hash_neg(data->hash_vals + 1, 1,
            data->nghashes, data->positive,
            input_sent->words, input_sent->n_word, input_sent->tgt_pos);
    if (data->hash_order < 0) {
        ST_WARNING("Failed to direct_wt_get_hash history.");
        return -1;
    }

    if (data->num_features - data->positive > 0) {
        /* future ngrams. */
        future_order = direct_get_hash_pos(
                data->hash_vals + 1 + data->hash_order,
                1, data->nghashes + data->positive,
                data->num_features - data->positive,
                input_sent->words, input_sent->n_word, input_sent->tgt_pos);
        if (future_order < 0) {
            ST_WARNING("Failed to direct_wt_get_hash future.");
            return -1;
        }
        data->hash_order += future_order;
    }

    data->hash_order += 1/* for hash_vals[0]. */;

    for (a = 0; a < data->hash_order; a++) {
        data->hash_vals[a] = data->hash_vals[a] % glue_updater->wt_updater->row;
    }

    return 0;
}

int direct_glue_updater_forward(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, sent_t *input_sent,
        real_t* in_ac, real_t *out_ac)
{
    dgu_data_t *data;
    out_updater_t *out_updater;

    direct_walker_args_t dw_args;
    int a;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || input_sent == NULL || out_ac == NULL, -1);

    data = (dgu_data_t *)glue_updater->extra;
    out_updater = comp_updater->out_updater;

    // TODO: instead of computing hash_vals here in advance,
    //       move this function inside direct_forward_walker
    //       with node_id as init_val, so that we can get different
    //       hash_vals on different tree nodes.
    if (direct_compute_hash(glue_updater, comp_updater,
                input_sent) < 0) {
        ST_WARNING("Failed to direct_compute_hash.");
        return -1;
    }

    dw_args.out_ac = out_ac;
    dw_args.comp_scale = comp_updater->comp->comp_scale;
    dw_args.wt_updater = glue_updater->wt_updater;
    for (a = 0; a < data->hash_order; a++) {
        dw_args.h = data->hash_vals[a];
        if (output_walk_through_path(out_updater->output,
                    input_sent->words[input_sent->tgt_pos],
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
        if (wt_update(dw_args->wt_updater, &seg, -1,
                    dw_args->out_er + child_s, dw_args->comp_scale,
                    NULL, 1.0, NULL) < 0) {
            ST_WARNING("Failed to wt_update.");
            return -1;
        }
    }

    return 0;
}

int direct_glue_updater_backprop(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, sent_t *input_sent,
        real_t *in_ac, real_t *out_er, real_t *in_er)
{
    dgu_data_t *data;
    out_updater_t *out_updater;

    direct_walker_args_t dw_args;
    int a;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || input_sent == NULL, -1);

    data = (dgu_data_t *)glue_updater->extra;
    out_updater = comp_updater->out_updater;

    dw_args.out_er = out_er;
    dw_args.comp_scale = comp_updater->comp->comp_scale;
    dw_args.wt_updater = glue_updater->wt_updater;
    for (a = 0; a < data->hash_order; a++) {
        dw_args.h = data->hash_vals[a];
        if (output_walk_through_path(out_updater->output,
                    input_sent->words[input_sent->tgt_pos],
                    direct_backprop_walker, (void *)&dw_args) < 0) {
            ST_WARNING("Failed to output_walk_through_path.");
            return -1;
        }
    }

    return 0;
}

int direct_glue_updater_forward_util_out(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, sent_t *input_sent,
        real_t* in_ac, real_t* out_ac)
{
    if (direct_compute_hash(glue_updater, comp_updater, input_sent) < 0) {
        ST_WARNING("Failed to direct_compute_hash.");
        return -1;
    }

    return 0;
}

int direct_glue_updater_forward_out(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, output_node_id_t node,
        real_t* in_ac, real_t *out_ac)
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
            || node == OUTPUT_NODE_NONE || out_ac == NULL, -1);

    data = (dgu_data_t *)glue_updater->extra;
    out_updater = comp_updater->out_updater;
    output = out_updater->output;

    child_s = s_children(output->tree, node);
    child_e = e_children(output->tree, node);

    if (child_s >= child_e) {
        return 0;
    }

    hash_sz = glue_updater->glue->wt->row;
    scale = comp_updater->comp->comp_scale;
    hash_wt = glue_updater->wt_updater->wt;

    if (output->norm == ON_SOFTMAX) {
        for (a = 0; a < data->hash_order; a++) {
            h = data->hash_vals[a] + child_s;
            if (h > hash_sz) {
                h -= hash_sz;
            }

            if (h + child_e - child_s - 1 > hash_sz) {
                for (ch = child_s; h < hash_sz; ch++, h++) {
                    out_ac[ch] += scale * hash_wt[h];
                }
                for (h = 0; ch < child_e - 1; ch++, h++) {
                    out_ac[ch] += scale * hash_wt[h];
                }
            } else {
                for (ch = child_s; ch < child_e - 1; ch++, h++) {
                    out_ac[ch] += scale * hash_wt[h];
                }
            }
        }
    }

    return 0;
}
