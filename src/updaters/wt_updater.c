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

#include <string.h>

#include <stutils/st_macro.h>
#include <stutils/st_log.h>
#include <stutils/st_utils.h>
#include <stutils/st_mem.h>

#include "blas.h"
#include "wt_updater.h"

#define REALLOC_NUM 100

void wt_dirty_destroy(wt_dirty_buf_t *dirty)
{
    if (dirty == NULL) {
        return;
    }

    safe_st_free(dirty->parts);
    dirty->cap_parts = 0;
    dirty->n_parts = 0;

    safe_st_free(dirty->segs);
    dirty->cap_seg = 0;
    dirty->n_seg = 0;

    safe_st_free(dirty->ids);
    safe_st_free(dirty->ids_hit);
    dirty->cap_id = 0;
    dirty->n_id = 0;

    if (dirty->buf_er != NULL) {
        int i;
        for (i = 0; i < dirty->n_buf; i++) {
            concat_mat_destroy(dirty->buf_er + i);
        }
        safe_st_free(dirty->buf_er);
    }
    if (dirty->buf_in != NULL) {
        int i;
        for (i = 0; i < dirty->n_buf; i++) {
            concat_mat_destroy(dirty->buf_in + i);
        }
        safe_st_free(dirty->buf_in);
    }
    dirty->n_buf = 0;

    safe_st_free(dirty->buf_grad);
}

void wt_updater_destroy(wt_updater_t *wt_updater)
{
    if (wt_updater == NULL) {
        return;
    }

    if (wt_updater->wt != wt_updater->shared_wt) {
        safe_st_aligned_free(wt_updater->wt);
    } else {
        wt_updater->wt = NULL;
    }
    wt_updater->shared_wt = NULL;
    safe_st_aligned_free(wt_updater->ori_wt);
    safe_st_aligned_free(wt_updater->delta_wt);
    if (wt_updater->bias != wt_updater->shared_bias) {
        safe_st_aligned_free(wt_updater->bias);
    } else {
        wt_updater->bias = NULL;
    }
    wt_updater->shared_bias = NULL;
    safe_st_aligned_free(wt_updater->ori_bias);
    safe_st_aligned_free(wt_updater->delta_bias);
    wt_updater->row = 0;
    wt_updater->col = 0;

    wt_dirty_destroy(&wt_updater->mini_dirty);
    wt_dirty_destroy(&wt_updater->sync_dirty);

    safe_st_free(wt_updater->segs);
    wt_updater->n_seg = 0;

    wt_updater_clear(wt_updater);
}

static int dirty_set_segs(wt_dirty_buf_t *dirty, size_t col,
        st_int_seg_t *segs, int n_seg)
{
    size_t sz;
    int i;

    ST_CHECK_PARAM(dirty == NULL || segs == NULL || n_seg <= 0, -1);

    safe_st_free(dirty->buf_er);
    safe_st_free(dirty->buf_in);

    sz = sizeof(concat_mat_t) * n_seg;
    dirty->buf_in = (concat_mat_t *)st_malloc(sz);
    if (dirty->buf_in == NULL) {
        ST_WARNING("Failed to mallco buf_in.");
        goto ERR;
    }
    memset(dirty->buf_in, 0, sz);

    dirty->buf_er = (concat_mat_t *)st_malloc(sz);
    if (dirty->buf_er == NULL) {
        ST_WARNING("Failed to mallco buf_er.");
        goto ERR;
    }
    memset(dirty->buf_er, 0, sz);

    for (i = 0; i < n_seg; i++) {
        dirty->buf_in[i].col = col;
        if (segs[i].n > 0) {
            dirty->buf_er[i].col = segs[i].n;
        } else {
            dirty->buf_er[i].col = 0;
        }
    }
    dirty->n_buf = n_seg;

    return 0;

ERR:
    safe_st_free(dirty->buf_er);
    safe_st_free(dirty->buf_in);
    dirty->n_buf = 0;
    return -1;
}

int wt_updater_init(wt_updater_t *wt_updater)
{
    st_int_seg_t seg;

    size_t sz;
    size_t bias_sz;

    ST_CHECK_PARAM(wt_updater == NULL, -1);

    wt_updater->n_step = 0;
    wt_updater->n_flush_step = 1;

    sz =  wt_updater->row * wt_updater->col * sizeof(real_t);
    bias_sz = wt_updater->row * sizeof(real_t);

    if (wt_updater->param.sync_size > 0) {
        safe_st_aligned_free(wt_updater->wt);
        wt_updater->wt = st_aligned_malloc(sz, ALIGN_SIZE);
        if (wt_updater->wt == NULL) {
            ST_WARNING("Failed to st_aligned_malloc wt.");
            goto ERR;
        }
        memcpy(wt_updater->wt, wt_updater->shared_wt, sz);

        safe_st_aligned_free(wt_updater->ori_wt);
        wt_updater->ori_wt = st_aligned_malloc(sz, ALIGN_SIZE);
        if (wt_updater->ori_wt == NULL) {
            ST_WARNING("Failed to st_aligned_malloc ori_wt.");
            goto ERR;
        }
        memcpy(wt_updater->ori_wt, wt_updater->wt, sz);

        if (wt_updater->shared_bias != NULL) {
            safe_st_aligned_free(wt_updater->bias);
            wt_updater->bias = st_aligned_malloc(bias_sz, ALIGN_SIZE);
            if (wt_updater->bias == NULL) {
                ST_WARNING("Failed to st_aligned_malloc bias.");
                goto ERR;
            }
            memcpy(wt_updater->bias, wt_updater->shared_bias, bias_sz);

            safe_st_aligned_free(wt_updater->ori_bias);
            wt_updater->ori_bias = st_aligned_malloc(bias_sz, ALIGN_SIZE);
            if (wt_updater->ori_bias == NULL) {
                ST_WARNING("Failed to st_aligned_malloc ori_bias.");
                goto ERR;
            }
            memcpy(wt_updater->ori_bias, wt_updater->bias, bias_sz);
        }

    } else {
        wt_updater->wt = wt_updater->shared_wt;
        wt_updater->bias = wt_updater->shared_bias;
    }

    if (wt_updater->param.momentum != 0.0) {
        safe_st_aligned_free(wt_updater->delta_wt);
        wt_updater->delta_wt = st_aligned_malloc(sz, ALIGN_SIZE);
        if (wt_updater->delta_wt == NULL) {
            ST_WARNING("Failed to st_aligned_malloc delta_wt.");
            goto ERR;
        }
        memset(wt_updater->delta_wt, 0, sz);

        if (wt_updater->shared_bias != NULL) {
            safe_st_aligned_free(wt_updater->delta_bias);
            wt_updater->delta_bias = st_aligned_malloc(bias_sz, ALIGN_SIZE);
            if (wt_updater->delta_bias == NULL) {
                ST_WARNING("Failed to st_aligned_malloc delta_bias.");
                goto ERR;
            }
            memset(wt_updater->delta_bias, 0, bias_sz);
        }
    }

    switch (wt_updater->type) {
        case WT_UT_FULL:
            seg.s = 0;
            seg.n = wt_updater->row;

            if (wt_updater->param.mini_batch > 0) {
                if (dirty_set_segs(&wt_updater->mini_dirty,
                            wt_updater->col, &seg, 1) < 0) {
                    ST_WARNING("Failed to dirty_set_segs for mini-batch.");
                    goto ERR;
                }
            }
            break;
        case WT_UT_ONE_SHOT:
            /* FALL THROUGH */
        case WT_UT_PART:
            if (wt_updater->param.mini_batch > 0) {
                safe_st_free(wt_updater->mini_dirty.buf_grad);

                wt_updater->mini_dirty.buf_grad = (real_t *)st_malloc(sz);
                if (wt_updater->mini_dirty.buf_grad == NULL) {
                    ST_WARNING("Failed to mallco buf_grad.");
                    goto ERR;
                }
                memset(wt_updater->mini_dirty.buf_grad, 0, sz);
            }
            break;
        default:
            break;
    }

    return 0;

ERR:
    wt_updater_destroy(wt_updater);
    return -1;
}

wt_updater_t* wt_updater_create(param_t *param, real_t *wt, real_t *bias,
        size_t row, size_t col, wt_update_type_t type)
{
    wt_updater_t *wt_updater = NULL;

    ST_CHECK_PARAM(param == NULL || wt == NULL || row <= 0 || col <= 0, NULL);

    wt_updater = (wt_updater_t *)st_malloc(sizeof(wt_updater_t));
    if (wt_updater == NULL) {
        ST_WARNING("Failed to st_malloc wt_updater.");
        goto ERR;
    }
    memset(wt_updater, 0, sizeof(wt_updater_t));

    wt_updater->param = *param;
    wt_updater->row = row;
    wt_updater->col = col;
    wt_updater->shared_wt = wt;
    wt_updater->shared_bias = bias;
    wt_updater->type = type;

    if (wt_updater_init(wt_updater) < 0) {
        ST_WARNING("Failed to wt_updater_init.");
        goto ERR;
    }

    return wt_updater;

ERR:
    safe_wt_updater_destroy(wt_updater);
    return NULL;
}

int wt_updater_set_segs(wt_updater_t *wt_updater,
        st_int_seg_t *segs, int n_seg)
{
    ST_CHECK_PARAM(wt_updater == NULL || segs == NULL || n_seg <= 0, -1);

    safe_st_free(wt_updater->segs);
    wt_updater->segs = (st_int_seg_t *)st_malloc(sizeof(st_int_seg_t)*n_seg);
    if (wt_updater->segs == NULL) {
        ST_WARNING("Failed to st_malloc segs.");
        goto ERR;
    }
    memcpy(wt_updater->segs, segs, sizeof(st_int_seg_t) * n_seg);
    wt_updater->n_seg = n_seg;

    if (wt_updater->param.mini_batch > 0) {
        if (dirty_set_segs(&wt_updater->mini_dirty, wt_updater->col,
                    segs, n_seg) < 0) {
            ST_WARNING("Failed to dirty_set_segs for mini-batch.");
            goto ERR;
        }
    }

    return 0;

ERR:
    safe_st_free(wt_updater->segs);
    return -1;
}

void wt_dirty_clear(wt_dirty_buf_t *dirty)
{
    if (dirty->buf_in != NULL) {
        int i;
        if (dirty->n_id > 0) {
            for (i = 0; i < dirty->n_id; i++) {
                dirty->buf_in[dirty->ids[i]].n_row = 0;
            }
        } else {
            for (i = 0; i < dirty->n_buf; i++) {
                dirty->buf_in[i].n_row = 0;
            }
        }
    }
    dirty->in_scale = 0.0;
    if (dirty->buf_er != NULL) {
        int i;
        if (dirty->n_id > 0) {
            for (i = 0; i < dirty->n_id; i++) {
                dirty->buf_er[dirty->ids[i]].n_row = 0;
            }
        } else {
            for (i = 0; i < dirty->n_buf; i++) {
                dirty->buf_er[i].n_row = 0;
            }
        }
        dirty->er_scale = 0.0;
    }

    dirty->n_parts = 0;
    dirty->n_seg = 0;
    dirty->n_id = 0;
}

void wt_updater_clear(wt_updater_t *wt_updater)
{
    wt_dirty_clear(&wt_updater->mini_dirty);
    wt_dirty_clear(&wt_updater->sync_dirty);
}

static inline real_t get_lr(param_t *param)
{
    if (param->momentum != 0.0) {
        return param->learn_rate * (1.0 - param->momentum);
    } else {
        return param->learn_rate;
    }
}

static inline real_t get_l2(wt_updater_t *wt_updater)
{
    if (wt_updater->param.l2_delay > 0
        && wt_updater->n_flush_step % (wt_updater->param.l2_delay + 1) == 0) {
        return wt_updater->param.l2_penalty;
    } else {
        return 0.0;
    }
}

static int wt_dirty_insert_ids(wt_dirty_buf_t *dirty, int id, int num_hit)
{
    int pos;
    int n;

    ST_CHECK_PARAM(dirty == NULL || id < 0, -1);

    n = dirty->n_id;
    pos = st_int_insert(dirty->ids, dirty->cap_id, &dirty->n_id, id);
    if (pos < 0) {
        ST_WARNING("Failed to st_int_insert.");
        return -1;
    }

    if (dirty->ids_hit != NULL) {
        if (num_hit <= 0) {
            ST_WARNING("Invalid num_hit[%d]", num_hit);
            return -1;
        }

        if (n == dirty->n_id) { // not new elem
            dirty->ids_hit[pos] += num_hit;
        } else {
            if (pos < n) {
                memmove(dirty->ids_hit + pos + 1, dirty->ids_hit + pos,
                        sizeof(int) * (n - pos));
            }
            dirty->ids_hit[pos] = num_hit;
        }
    }

    return 0;
}

static int wt_updater_mini_update(wt_updater_t *wt_updater)
{
    wt_dirty_buf_t *dirty;
    real_t *wt;
    real_t *delta_wt;
    real_t *bias;
    real_t *delta_bias;

    st_int_seg_t *seg;
    size_t row, col, sz, i, j, a;
    int idx;
    real_t sum;

    real_t lr, l2, llr;
    real_t momentum;

    dirty = &wt_updater->mini_dirty;
    wt = wt_updater->wt;
    delta_wt = wt_updater->delta_wt;
    bias = wt_updater->bias;
    delta_bias = wt_updater->delta_bias;

    row = wt_updater->row;
    col = wt_updater->col;

    lr = get_lr(&(wt_updater->param));
    lr *= dirty->er_scale * dirty->in_scale;
    l2 = get_l2(wt_updater);
    momentum = wt_updater->param.momentum;

    switch (wt_updater->type) {
        case WT_UT_FULL:
            sz = row * col;

            if (dirty->buf_er != NULL && dirty->buf_er[0].n_row > 0) {
                lr /= dirty->buf_er[0].n_row;

                if (momentum != 0.0) {
                    matXmat(delta_wt, dirty->buf_er[0].val,
                            dirty->buf_in[0].val, row, col,
                            dirty->buf_er[0].n_row,
                            lr, momentum);
                    for (i = 0; i < sz; i++) {
                        delta_wt[i] -= l2 * wt[i];
                        wt[i] += delta_wt[i];
                    }
                    if (bias != NULL) {
                        for (i = 0; i < row; i++) {
                            sum = 0.0;
                            for (j = 0; j < dirty->buf_er[0].n_row; j++) {
                                sum += dirty->buf_er[0].val[j * row + i];
                            }
                            delta_bias[i] = lr * sum - l2 * bias[i] + momentum * delta_bias[i];
                            bias[i] += delta_bias[i];
                        }
                    }
                } else {
                    matXmat(wt, dirty->buf_er[0].val,
                            dirty->buf_in[0].val, row, col,
                            dirty->buf_er[0].n_row,
                            lr, 1.0 - l2);
                    if (bias != NULL) {
                        for (i = 0; i < row; i++) {
                            sum = 0.0;
                            for (j = 0; j < dirty->buf_er[0].n_row; j++) {
                                sum += dirty->buf_er[0].val[j * row + i];
                            }
                            bias[i] += lr * sum - l2 * bias[i];
                        }
                    }
                }
            }
            break;

        case WT_UT_SEG:
            if (dirty->buf_er != NULL && dirty->n_id > 0) {
                for (a = 0; a < dirty->n_id; a++) {
                    idx = dirty->ids[a];
                    seg = wt_updater->segs + idx;
                    if (dirty->buf_er[idx].n_row > 0) {
                        llr = lr / dirty->buf_er[idx].n_row;
                        if (momentum != 0.0) {
                            matXmat(delta_wt + seg->s * col,
                                    dirty->buf_er[idx].val,
                                    dirty->buf_in[idx].val, seg->n, col,
                                    dirty->buf_er[idx].n_row,
                                    llr, momentum);
                            i = seg->s * col;
                            for (; i < (seg->s + seg->n) * col; i++) {
                                delta_wt[i] -= l2 * wt[i];
                                wt[i] += delta_wt[i];
                            }
                            if (bias != NULL) {
                                for (i = seg->s; i < (seg->s + seg->n); i++) {
                                    sum = 0.0;
                                    for (j = 0; j < dirty->buf_er[idx].n_row; j++) {
                                        sum += dirty->buf_er[idx].val[j * seg->n + i - seg->s];
                                    }
                                    delta_bias[i] = llr * sum - l2 * bias[i] + momentum * delta_bias[i];
                                    bias[i] += delta_bias[i];
                                }
                            }
                        } else {
                            matXmat(wt + seg->s * col,
                                    dirty->buf_er[idx].val,
                                    dirty->buf_in[idx].val, seg->n, col,
                                    dirty->buf_er[idx].n_row,
                                    llr, 1.0 - l2);
                            if (bias != NULL) {
                                for (i = seg->s; i < (seg->s + seg->n); i++) {
                                    sum = 0.0;
                                    for (j = 0; j < dirty->buf_er[idx].n_row; j++) {
                                        sum += dirty->buf_er[idx].val[j * seg->n + i - seg->s];
                                    }
                                    bias[i] += llr * sum - l2 * bias[i];
                                }
                            }
                        }
                    }
                }
            }

            break;

        case WT_UT_PART:
            if (momentum != 0.0) {
                for (a = 0; a < dirty->n_parts; a++) {
                    for (i = dirty->parts[a].s; i < dirty->parts[a].e; i++) {
                        delta_wt[i] = lr * dirty->buf_grad[i] - l2 * wt[i]
                            + momentum * delta_wt[i];
                        wt[i] += delta_wt[i];
                        dirty->buf_grad[i] = 0.0;
                    }
                }
            } else {
                for (a = 0; a < dirty->n_parts; a++) {
                    for (i = dirty->parts[a].s; i < dirty->parts[a].e; i++) {
                        wt[i] += lr * dirty->buf_grad[i] - l2 * wt[i];
                        dirty->buf_grad[i] = 0.0;
                    }
                }
            }
            break;

        case WT_UT_ONE_SHOT:
            if (momentum != 0.0) {
                for (a = 0; a < dirty->n_id; a++) {
                    i = dirty->ids[a] * col;
                    llr = lr / dirty->ids_hit[a];
                    for (; i < (dirty->ids[a] + 1) * col; i++) {
                        delta_wt[i] = llr * dirty->buf_grad[i] - l2 * wt[i]
                            + momentum * delta_wt[i];
                        wt[i] += delta_wt[i];
                        dirty->buf_grad[i] = 0.0;
                    }
                }
            } else {
                for (a = 0; a < dirty->n_id; a++) {
                    i = dirty->ids[a] * col;
                    llr = lr / dirty->ids_hit[a];
                    for (; i < (dirty->ids[a] + 1) * col; i++) {
                        wt[i] += llr * dirty->buf_grad[i] - l2 * wt[i];
                        dirty->buf_grad[i] = 0.0;
                    }
                }
            }
            break;

        default:
            ST_WARNING("Unknown updating type[%d].", wt_updater->type);
            return -1;
    }

    wt_dirty_clear(dirty);

    return 0;
}

static int wt_updater_sync(wt_updater_t *wt_updater)
{
    wt_dirty_buf_t *dirty;
    real_t *shared_wt;
    real_t *wt;
    real_t *ori_wt;
    real_t *shared_bias;
    real_t *bias;
    real_t *ori_bias;

    st_int_seg_t *seg;
    size_t row, col;
    size_t sz, i, a;

    row = wt_updater->row;
    col = wt_updater->col;

    dirty = &wt_updater->sync_dirty;
    shared_wt = wt_updater->shared_wt;
    wt = wt_updater->wt;
    ori_wt = wt_updater->ori_wt;
    shared_bias = wt_updater->shared_bias;
    bias = wt_updater->bias;
    ori_bias = wt_updater->ori_bias;

    switch (wt_updater->type) {
        case WT_UT_FULL:
            sz = row * col;

            for (i = 0; i < sz; i++) {
                shared_wt[i] += wt[i] - ori_wt[i];
            }
            memcpy(wt, shared_wt, sz * sizeof(real_t));
            memcpy(ori_wt, wt, sz * sizeof(real_t));

            if (bias != NULL) {
                for (i = 0; i < row; i++) {
                    shared_bias[i] += bias[i] - ori_bias[i];
                }
                memcpy(bias, shared_bias, row * sizeof(real_t));
                memcpy(ori_bias, bias, row * sizeof(real_t));
            }

            break;

        case WT_UT_SEG:
            for (a = 0; a < dirty->n_id; a++) {
                seg = wt_updater->segs + dirty->ids[a];
                for (i = seg->s * col; i < (seg->s + seg->n) * col; i++) {
                    shared_wt[i] += wt[i] - ori_wt[i];
                }
                sz = col * seg->n * sizeof(real_t);
                memcpy(wt + seg->s * col, shared_wt + seg->s * col, sz);
                memcpy(ori_wt + seg->s * col, wt + seg->s * col, sz);

                if (bias != NULL) {
                    for (i = seg->s; i < (seg->s + seg->n); i++) {
                        shared_bias[i] += bias[i] - ori_bias[i];
                    }
                    sz = seg->n * sizeof(real_t);
                    memcpy(bias + seg->s, shared_bias + seg->s, sz);
                    memcpy(ori_bias + seg->s, bias + seg->s, sz);
                }
            }
            break;

        case WT_UT_PART:
            for (a = 0; a < dirty->n_parts; a++) {
                for (i = dirty->parts[a].s; i < dirty->parts[a].e; i++) {
                    shared_wt[i] += wt[i] - ori_wt[i];
                }
                sz = dirty->parts[a].e - dirty->parts[a].s;
                memcpy(wt + dirty->parts[a].s,
                        shared_wt + dirty->parts[a].s, sizeof(real_t) * sz);
                memcpy(ori_wt + dirty->parts[a].s,
                        wt + dirty->parts[a].s, sizeof(real_t) * sz);
            }
            break;

        case WT_UT_ONE_SHOT:
            for (a = 0; a < dirty->n_id; a++) {
                i = dirty->ids[a] * col;
                for (; i < (dirty->ids[a] + 1) * col; i++) {
                    shared_wt[i] += wt[i] - ori_wt[i];
                }
                i = dirty->ids[a] * col;
                memcpy(wt + i, shared_wt + i, sizeof(real_t) * col);
                memcpy(ori_wt + i, wt + i, sizeof(real_t) * col);
            }
            break;
        default:
            ST_WARNING("Unknown updating type[%d].", wt_updater->type);
            return -1;
    }

    wt_dirty_clear(dirty);

    return 0;
}

static int wt_updater_update(wt_updater_t *wt_updater,
        st_size_seg_t *part, int row_seg_id,
        real_t *er, real_t er_scale,
        real_t *in, real_t in_scale, st_wt_int_t *in_idx)
{
    real_t *wt;
    real_t *delta_wt;
    real_t *bias;
    real_t *delta_bias;

    real_t lr;
    real_t l2;
    real_t momentum;

    size_t row, col;

    size_t i, j;
    int row_start, row_end;
    real_t scale;

    lr = get_lr(&(wt_updater->param));
    lr *= er_scale * in_scale;
    row = wt_updater->row;
    col = wt_updater->col;

    l2 = get_l2(wt_updater);
    momentum = wt_updater->param.momentum;

    wt = wt_updater->wt;
    delta_wt = wt_updater->delta_wt;
    bias = wt_updater->bias;
    delta_bias = wt_updater->delta_bias;

    switch (wt_updater->type) {
        case WT_UT_PART:
            // Hash-based weight
            // needed: part, er
            if (momentum != 0.0) {
                if (part->s + part->n > row) {
                    for (j = 0, i = part->s; i < row; j++, i++) {
                        delta_wt[i] = lr * er[j] - l2 * wt[i]
                            + momentum * delta_wt[i];
                        wt[i] += delta_wt[i];
                    }
                    for (i = 0; j < part->n; j++, i++) {
                        delta_wt[i] = lr * er[j] - l2 * wt[i]
                            + momentum * delta_wt[i];
                        wt[i] += delta_wt[i];
                    }
                } else {
                    for (j = 0, i = part->s; j < part->n; j++, i++) {
                        delta_wt[i] = lr * er[j] - l2 * wt[i]
                            + momentum * delta_wt[i];
                        wt[i] += delta_wt[i];
                    }
                }
            } else {
                if (part->s + part->n > row) {
                    for (j = 0, i = part->s; i < row; j++, i++) {
                        wt[i] += lr * er[j] - l2 * wt[i];
                    }
                    for (i = 0; j < part->n; j++, i++) {
                        wt[i] += lr * er[j] - l2 * wt[i];
                    }
                } else {
                    for (j = 0, i = part->s; j < part->n; j++, i++) {
                        wt[i] += lr * er[j] - l2 * wt[i];
                    }
                }
            }
            break;

        case WT_UT_ONE_SHOT:
            // needed: in_idx, er
            i = in_idx->i * col;
            scale = lr * in_idx->w;
            if (momentum != 0.0) {
                for (j = 0; j < col; j++, i++) {
                    delta_wt[i] = scale * er[j] - l2 * wt[i]
                            + momentum * delta_wt[i];
                    wt[i] += delta_wt[i];
                }
            } else {
                for (j = 0; j < col; j++, i++) {
                    wt[i] += scale * er[j] - l2 * wt[i];
                }
            }
            break;
        case WT_UT_SEG:
            // needed: in, er, row_seg_id
            /* FALL THROUGH */
        case WT_UT_FULL:
            // needed: in, er
            if (wt_updater->type == WT_UT_FULL) {
                row_start = 0;
                row_end = row;
            } else {
                row_start = wt_updater->segs[row_seg_id].s;
                row_end = row_start + wt_updater->segs[row_seg_id].n;
            }

            if (row_start < 0 || row_start >= row_end) {
                ST_WARNING("Error row_seg_id[%d], lead to a invalid seg.",
                        row_seg_id);
                return -1;
            }
            if (momentum != 0.0) {
                matXmat(delta_wt + row_start * col, er, in,
                        row_end - row_start, col, 1, lr, momentum);
                for (i = row_start * col; i < row_end * col; i++) {
                    delta_wt[i] -= l2 * wt[i];
                    wt[i] += delta_wt[i];
                }
                if (bias != NULL) {
                    for (i = row_start; i < row_end; i++) {
                        delta_bias[i] = lr * er[i - row_start] - l2 * bias[i] + momentum * delta_bias[i];
                        bias[i] += delta_bias[i];
                    }
                }
            } else {
                matXmat(wt + row_start * col, er, in, row_end - row_start,
                        col, 1, lr, 1.0 - l2);
                if (bias != NULL) {
                    for (i = row_start; i < row_end; i++) {
                        bias[i] += lr * er[i - row_start] - l2 * bias[i];
                    }
                }
            }
            break;
        default:
            ST_WARNING("Unknown updating type[%d].", wt_updater->type);
            return -1;
    }

    return 0;
}

static int wt_updater_dirty(wt_updater_t *wt_updater, wt_dirty_buf_t *dirty,
        st_size_seg_t *part, int row_seg_id, st_wt_int_t *in_idx,
        real_t *er, real_t er_scale, real_t *in, real_t in_scale)
{
    size_t sz;

    size_t i, j;

    switch (wt_updater->type) {
        case WT_UT_FULL:
            if (dirty->buf_er != NULL) {
                if (dirty->er_scale == 0.0) {
                    dirty->er_scale = er_scale;
                } else if (dirty->er_scale != er_scale) {
                    ST_WARNING("er_scale changed.");
                    return -1;
                }
                if (concat_mat_add_row(dirty->buf_er, er,
                            wt_updater->row) < 0) {
                    ST_WARNING("Failed to concat_mat_add_row for buf_er.");
                    return -1;
                }

                if (dirty->in_scale == 0.0) {
                    dirty->in_scale = in_scale;
                } else if (dirty->in_scale != in_scale) {
                    ST_WARNING("in_scale changed.");
                    return -1;
                }
                if (concat_mat_add_row(dirty->buf_in, in,
                            wt_updater->col) < 0) {
                    ST_WARNING("Failed to concat_mat_add_row for buf_in.");
                    return -1;
                }
            }
            break;
        case WT_UT_PART:
            if (dirty->n_parts >= dirty->cap_parts) {
                dirty->cap_parts += REALLOC_NUM;
                sz = dirty->cap_parts * sizeof(st_size_seg_t);
                dirty->parts = (st_size_seg_t *)st_realloc(dirty->parts, sz);
                if (dirty->parts == NULL) {
                    ST_WARNING("Failed to st_realloc parts");
                    return -1;
                }
            }
            if (st_size_seg_union(dirty->parts, dirty->cap_parts,
                        &dirty->n_parts, part, 1, wt_updater->row) < 0) {
                ST_WARNING("Failed to st_size_seg_union.");
                return -1;
            }

            if (dirty->buf_grad != NULL) {
                if (dirty->er_scale == 0.0) {
                    dirty->er_scale = er_scale;
                } else if (dirty->er_scale != er_scale) {
                    ST_WARNING("er_scale changed.");
                    return -1;
                }
                if (dirty->in_scale == 0.0) {
                    dirty->in_scale = in_scale;
                } else if (dirty->in_scale != in_scale) {
                    ST_WARNING("in_scale changed.");
                    return -1;
                }
                if (part->s + part->n > wt_updater->row) {
                    for (j = 0, i = part->s; i < wt_updater->row; j++, i++) {
                        dirty->buf_grad[i] += er[j];
                    }
                    for (i = 0; j < part->n; j++, i++) {
                        dirty->buf_grad[i] += er[j];
                    }
                } else {
                    for (j = 0, i = part->s; j < part->n; j++, i++) {
                        dirty->buf_grad[i] += er[j];
                    }
                }
            }
            break;
        case WT_UT_ONE_SHOT:
            if (dirty->n_id >= dirty->cap_id) {
                dirty->cap_id += 100;
                sz = dirty->cap_id * sizeof(int);
                dirty->ids = (int *)st_realloc(dirty->ids, sz);
                if (dirty->ids == NULL) {
                    ST_WARNING("Failed to st_realloc ids");
                    return -1;
                }
                dirty->ids_hit = (int *)st_realloc(dirty->ids_hit, sz);
                if (dirty->ids_hit == NULL) {
                    ST_WARNING("Failed to st_realloc ids_hit");
                    return -1;
                }
            }
            if (wt_dirty_insert_ids(dirty, in_idx->i, 1) < 0) {
                ST_WARNING("Failed to wt_dirty_insert_ids.");
                return -1;
            }

            if (dirty->buf_grad != NULL) {
                if (dirty->er_scale == 0.0) {
                    dirty->er_scale = er_scale;
                } else if (dirty->er_scale != er_scale) {
                    ST_WARNING("er_scale changed.");
                    return -1;
                }
                if (dirty->in_scale == 0.0) {
                    dirty->in_scale = in_scale;
                } else if (dirty->in_scale != in_scale) {
                    ST_WARNING("in_scale changed.");
                    return -1;
                }
                i = in_idx->i * wt_updater->col;
                for (j = 0; j < wt_updater->col; j++, i++) {
                    dirty->buf_grad[i] += in_idx->w * er[j];
                }
            }
            break;

        case WT_UT_SEG:
            if (dirty->n_id >= dirty->cap_id) {
                dirty->cap_id += 100;
                sz = dirty->cap_id * sizeof(int);
                dirty->ids = (int *)st_realloc(dirty->ids, sz);
                if (dirty->ids == NULL) {
                    ST_WARNING("Failed to st_realloc ids");
                    return -1;
                }
            }
            if (st_int_insert(dirty->ids, dirty->cap_id,
                        &dirty->n_id, row_seg_id) < 0) {
                ST_WARNING("Failed to st_int_insert.");
                return -1;
            }
            if (dirty->buf_er != NULL) {
                if (dirty->er_scale == 0.0) {
                    dirty->er_scale = er_scale;
                } else if (dirty->er_scale != er_scale) {
                    ST_WARNING("er_scale changed.");
                    return -1;
                }
                if (concat_mat_add_row(dirty->buf_er + row_seg_id, er,
                            wt_updater->segs[row_seg_id].n) < 0) {
                    ST_WARNING("Failed to concat_mat_add_row for buf_er.");
                    return -1;
                }

                if (dirty->in_scale == 0.0) {
                    dirty->in_scale = in_scale;
                } else if (dirty->in_scale != in_scale) {
                    ST_WARNING("in_scale changed.");
                    return -1;
                }
                if (concat_mat_add_row(dirty->buf_in + row_seg_id, in,
                            wt_updater->col) < 0) {
                    ST_WARNING("Failed to concat_mat_add_row for buf_in.");
                    return -1;
                }
            }
            break;
        default:
            ST_WARNING("Unknown updating type[%d].", wt_updater->type);
            return -1;
    }

    return 0;
}

static int wt_updater_dirty_cpy(wt_updater_t *wt_updater,
        wt_dirty_buf_t *dst, wt_dirty_buf_t *src)
{
    size_t sz;
    size_t i;

    switch (wt_updater->type) {
        case WT_UT_FULL:
            if (dst->buf_er != NULL) {
                if (dst->er_scale == 0.0) {
                    dst->er_scale = src->er_scale;
                } else if (dst->er_scale != src->er_scale) {
                    ST_WARNING("er_scale changed.");
                    return -1;
                }
                if (dst->in_scale == 0.0) {
                    dst->in_scale = src->in_scale;
                } else if (dst->in_scale != src->in_scale) {
                    ST_WARNING("in_scale changed.");
                    return -1;
                }
                if (concat_mat_add_mat(dst->buf_er, src->buf_er) < 0) {
                    ST_WARNING("Failed to concat_mat_add_mat for buf_er.");
                    return -1;
                }

                if (concat_mat_add_mat(dst->buf_in, src->buf_in) < 0) {
                    ST_WARNING("Failed to concat_mat_add_mat for buf_in.");
                    return -1;
                }
            }
            break;
        case WT_UT_PART:
            if (dst->n_parts + src->n_parts > dst->cap_parts) {
                dst->cap_parts += src->n_parts;
                sz = dst->cap_parts * sizeof(st_size_seg_t);
                dst->parts = (st_size_seg_t *)st_realloc(dst->parts, sz);
                if (dst->parts == NULL) {
                    ST_WARNING("Failed to st_realloc parts");
                    return -1;
                }
            }
            for (i = 0; i < src->n_parts; i++) {
                src->parts[i].n = src->parts[i].e - src->parts[i].s;
            }
            if (st_size_seg_union(dst->parts, dst->cap_parts,
                        &dst->n_parts, src->parts, src->n_parts,
                        wt_updater->row) < 0) {
                ST_WARNING("Failed to st_size_seg_union.");
                return -1;
            }
            for (i = 0; i < src->n_parts; i++) {
                src->parts[i].e = src->parts[i].s + src->parts[i].n;
            }
            break;
        case WT_UT_SEG:
            if (dst->buf_er != NULL) {
                if (dst->er_scale == 0.0) {
                    dst->er_scale = src->er_scale;
                } else if (dst->er_scale != src->er_scale) {
                    ST_WARNING("er_scale changed.");
                    return -1;
                }
                if (dst->in_scale == 0.0) {
                    dst->in_scale = src->in_scale;
                } else if (dst->in_scale != src->in_scale) {
                    ST_WARNING("in_scale changed.");
                    return -1;
                }
                for (i = 0; i < src->n_id; i++) {
                    if (concat_mat_add_mat(dst->buf_er + src->ids[i],
                                src->buf_er + src->ids[i]) < 0) {
                        ST_WARNING("Failed to concat_mat_add_mat for buf_er.");
                        return -1;
                    }

                    if (concat_mat_add_mat(dst->buf_in + src->ids[i],
                                src->buf_in + src->ids[i]) < 0) {
                        ST_WARNING("Failed to concat_mat_add_mat for buf_in.");
                        return -1;
                    }
                }
            }

            if (dst->n_id + src->n_id > dst->cap_id) {
                dst->cap_id += src->n_id;
                sz = dst->cap_id * sizeof(int);
                dst->ids = (int *)st_realloc(dst->ids, sz);
                if (dst->ids == NULL) {
                    ST_WARNING("Failed to st_realloc ids");
                    return -1;
                }
            }
            /* TODO: We could use merge sort here. */
            for (i = 0; i < src->n_id; i++) {
                if (st_int_insert(dst->ids, dst->cap_id,
                            &dst->n_id, src->ids[i]) < 0) {
                    ST_WARNING("Failed to st_int_insert.");
                    return -1;
                }
            }
            break;

        case WT_UT_ONE_SHOT:
            if (dst->n_id + src->n_id > dst->cap_id) {
                dst->cap_id += src->n_id;
                sz = dst->cap_id * sizeof(int);
                dst->ids = (int *)st_realloc(dst->ids, sz);
                if (dst->ids == NULL) {
                    ST_WARNING("Failed to st_realloc ids");
                    return -1;
                }
                dst->ids_hit = (int *)st_realloc(dst->ids_hit, sz);
                if (dst->ids_hit == NULL) {
                    ST_WARNING("Failed to st_realloc ids_hit");
                    return -1;
                }
            }
            /* TODO: We could use merge sort here. */
            for (i = 0; i < src->n_id; i++) {
                if (wt_dirty_insert_ids(dst, src->ids[i],
                            src->ids_hit[i]) < 0) {
                    ST_WARNING("Failed to wt_dirty_insert_ids.");
                    return -1;
                }
            }
            break;
        default:
            ST_WARNING("Unknown updating type[%d].", wt_updater->type);
            return -1;
    }

    return 0;
}

int wt_update(wt_updater_t *wt_updater,
        st_size_seg_t *part, int row_seg_id,
        real_t *er, real_t er_scale,
        real_t *in, real_t in_scale, st_wt_int_t *in_idx)
{
    ST_CHECK_PARAM(wt_updater == NULL, -1);

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Update weight");
#endif

    if (wt_updater->param.mini_batch > 0) {
        if (wt_updater_dirty(wt_updater, &wt_updater->mini_dirty,
                    part, row_seg_id, in_idx,
                    er, er_scale, in, in_scale) < 0) {
            ST_WARNING("Failed to wt_updater_dirty for minibatch.");
            return -1;
        }
    } else {
        if (wt_updater_update(wt_updater, part, row_seg_id,
                    er, er_scale, in, in_scale, in_idx) < 0) {
            ST_WARNING("Failed to wt_updater_update.");
            return -1;
        }

        if (wt_updater->param.sync_size > 0) {
            if (wt_updater_dirty(wt_updater, &wt_updater->sync_dirty,
                        part, row_seg_id, in_idx,
                        er, er_scale, in, in_scale) < 0) {
                ST_WARNING("Failed to wt_updater_dirty for sync.");
                return -1;
            }
        }
    }

    return 0;
}

int wt_flush(wt_updater_t *wt_updater, bool force)
{
    ST_CHECK_PARAM(wt_updater == NULL, -1);

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Flush weight");
#endif

    wt_updater->n_step++;

    if (wt_updater->param.mini_batch > 0) {
        if (wt_updater->n_step % wt_updater->param.mini_batch == 0 || force) {
            if (wt_updater->param.sync_size > 0) {
                if (wt_updater_dirty_cpy(wt_updater, &wt_updater->sync_dirty,
                            &wt_updater->mini_dirty) < 0) {
                    ST_WARNING("Failed to wt_updater_dirty_cpy.");
                    return -1;
                }
            }

            if (wt_updater_mini_update(wt_updater) < 0) {
                ST_WARNING("Failed to wt_updater_mini_update.");
                return -1;
            }

            wt_updater->n_flush_step++;
        }
    } else {
        wt_updater->n_flush_step++;
    }

    if (wt_updater->param.sync_size > 0) {
        if (wt_updater->n_step % wt_updater->param.sync_size == 0 || force) {
            if (wt_updater_sync(wt_updater) < 0) {
                ST_WARNING("Failed to wt_updater_sync for sync.");
                return -1;
            }
        }
    }

    return 0;
}
