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

    safe_free(dirty->segs);
    dirty->cap_seg = 0;
    dirty->n_seg = 0;

    safe_free(dirty->ids);
    dirty->cap_id = 0;
    dirty->n_id = 0;

#ifdef _BATCH_UPDATE_
    if (dirty->buf_er != NULL) {
        int i;
        for (i = 0; i < dirty->n_buf; i++) {
            concat_mat_destroy(dirty->buf_er + i);
        }
        safe_free(dirty->buf_er);
    }
    if (dirty->buf_in != NULL) {
        int i;
        for (i = 0; i < dirty->n_buf; i++) {
            concat_mat_destroy(dirty->buf_in + i);
        }
        safe_free(dirty->buf_in);
    }
    dirty->n_buf = 0;
#endif
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
    wt_updater->row = -1;
    wt_updater->col = -1;

    wt_dirty_destroy(&wt_updater->mini_dirty);
    wt_dirty_destroy(&wt_updater->sync_dirty);

    safe_free(wt_updater->segs);
    wt_updater->n_seg = 0;

    wt_updater_clear(wt_updater);
}

#ifdef _BATCH_UPDATE_
int dirty_set_segs(wt_dirty_buf_t *dirty, int col,
        st_int_seg_t *segs, int  n_seg)
{
    size_t sz;
    int i;

    ST_CHECK_PARAM(dirty == NULL || segs == NULL || n_seg < 0, -1);

    safe_free(dirty->buf_er);
    safe_free(dirty->buf_in);

    sz = sizeof(concat_mat_t) * n_seg;
    dirty->buf_in = (concat_mat_t *)malloc(sz);
    if (dirty->buf_in == NULL) {
        ST_WARNING("Failed to mallco buf_in.");
        goto ERR;
    }
    memset(dirty->buf_in, 0, sz);

    dirty->buf_er = (concat_mat_t *)malloc(sz);
    if (dirty->buf_er == NULL) {
        ST_WARNING("Failed to mallco buf_er.");
        goto ERR;
    }
    memset(dirty->buf_er, 0, sz);

    for (i = 0; i < n_seg; i++) {
        dirty->buf_in[i].col = col;
        if (segs[i].s >= 0 && segs[i].n > 0) {
            dirty->buf_er[i].col = segs[i].n;
        } else {
            dirty->buf_er[i].col = 0;
        }
    }
    dirty->n_buf = n_seg;

    return 0;

ERR:
    safe_free(dirty->buf_er);
    safe_free(dirty->buf_in);
    dirty->n_buf = 0;
    return -1;
}
#endif

wt_updater_t* wt_updater_create(param_t *param,
        real_t *wt, int row, int col, wt_update_type_t type)
{
    wt_updater_t *wt_updater = NULL;

    size_t sz;

    ST_CHECK_PARAM(param == NULL || wt == NULL || row <= 0, NULL);

    wt_updater = (wt_updater_t *)malloc(sizeof(wt_updater_t));
    if (wt_updater == NULL) {
        ST_WARNING("Failed to malloc wt_updater.");
        goto ERR;
    }
    memset(wt_updater, 0, sizeof(wt_updater_t));

    wt_updater->param = *param;
    wt_updater->row = row;
    wt_updater->col = col;
    wt_updater->shared_wt = wt;
    wt_updater->type = type;
    wt_updater->n_step = 0;
    wt_updater->n_flush_step = 1;

    if (wt_updater->col > 0) {
        sz =  wt_updater->row * wt_updater->col;
    } else {
        sz = wt_updater->row;
    }
    sz *= sizeof(real_t);

    if (wt_updater->param.sync_size > 0) {
        wt_updater->wt = st_aligned_malloc(sz, ALIGN_SIZE);
        if (wt_updater->wt == NULL) {
            ST_WARNING("Failed to st_aligned_malloc wt.");
            goto ERR;
        }
        memcpy(wt_updater->wt, wt, sz);

        wt_updater->ori_wt = st_aligned_malloc(sz, ALIGN_SIZE);
        if (wt_updater->ori_wt == NULL) {
            ST_WARNING("Failed to malloc ori_wt.");
            goto ERR;
        }
        memcpy(wt_updater->ori_wt, wt_updater->wt, sz);
    } else {
        wt_updater->wt = wt;
    }

    if (wt_updater->param.mini_batch > 0) {
        wt_updater->delta_wt = st_aligned_malloc(sz, ALIGN_SIZE);
        if (wt_updater->delta_wt == NULL) {
            ST_WARNING("Failed to malloc delta_wt.");
            goto ERR;
        }
        memset(wt_updater->delta_wt, 0, sz);
    }

#ifdef _BATCH_UPDATE_
    if (wt_updater->type == WT_UT_FULL) {
        st_int_seg_t seg;

        seg.s = 0;
        seg.n = wt_updater->row;

        if (wt_updater->param.mini_batch > 0) {
            if (dirty_set_segs(&wt_updater->mini_dirty, col, &seg, 1) < 0) {
                ST_WARNING("Failed to dirty_set_segs for mini-batch.");
                goto ERR;
            }
        }
    }
#endif

    return wt_updater;

ERR:
    safe_wt_updater_destroy(wt_updater);
    return NULL;
}

int wt_updater_set_segs(wt_updater_t *wt_updater, st_int_seg_t *segs, int n_seg)
{
    ST_CHECK_PARAM(wt_updater == NULL || segs == NULL || n_seg <= 0, -1);

    safe_free(wt_updater->segs);
    wt_updater->segs = (st_int_seg_t *)malloc(sizeof(st_int_seg_t) * n_seg);
    if (wt_updater->segs == NULL) {
        ST_WARNING("Failed to malloc segs.");
        goto ERR;
    }
    memcpy(wt_updater->segs, segs, sizeof(st_int_seg_t) * n_seg);
    wt_updater->n_seg = n_seg;

#ifdef _BATCH_UPDATE_
    if (wt_updater->param.mini_batch > 0) {
        if (dirty_set_segs(&wt_updater->mini_dirty, wt_updater->col,
                    segs, n_seg) < 0) {
            ST_WARNING("Failed to dirty_set_segs for mini-batch.");
            goto ERR;
        }
    }
#endif
    return 0;

ERR:
    safe_free(wt_updater->segs);
    return -1;
}

void wt_dirty_clear(wt_dirty_buf_t *dirty)
{
#ifdef _BATCH_UPDATE_
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
#endif

    dirty->n_seg = 0;
    dirty->n_id = 0;
}

void wt_updater_clear(wt_updater_t *wt_updater)
{
    wt_dirty_clear(&wt_updater->mini_dirty);
    wt_dirty_clear(&wt_updater->sync_dirty);
}

static inline real_t get_l2(wt_updater_t *wt_updater)
{
    if (wt_updater->param.l2_delay > 0
            && wt_updater->n_flush_step % wt_updater->param.l2_delay == 0) {
        return wt_updater->param.l2_penalty;
    } else {
        return 0.0;
    }
}

static int wt_updater_flush(wt_updater_t *wt_updater, real_t* dst_wt,
        real_t *src_wt, wt_dirty_buf_t *dirty, real_t *ori_wt)
{
    st_int_seg_t *seg;
    int row, col;
    int sz, i, a, j;
#ifdef _BATCH_UPDATE_
    real_t lr, l2;
#endif

    row = wt_updater->row;
    col = wt_updater->col;

    switch (wt_updater->type) {
        case WT_UT_FULL:
#ifdef _BATCH_UPDATE_
            if (dirty->buf_er != NULL) {
                lr = wt_updater->param.learn_rate;
                lr *= dirty->er_scale * dirty->in_scale;
                l2 = get_l2(wt_updater);

                matXmat(src_wt, dirty->buf_er[0].val,
                        dirty->buf_in[0].val, row, col,
                        dirty->buf_er[0].n_row,
                        lr, 1.0 - l2);
            }
#endif
            if (col > 0) {
                sz = row * col;
            } else {
                sz = row;
            }
            if (ori_wt == NULL) {
                for (i = 0; i < sz; i++) {
                    dst_wt[i] += src_wt[i];
                    src_wt[i] = 0;
                }
            } else {
                for (i = 0; i < sz; i++) {
                    dst_wt[i] += src_wt[i] - ori_wt[i];
                }
                memcpy(src_wt, dst_wt, sz * sizeof(real_t));
                memcpy(ori_wt, src_wt, sz * sizeof(real_t));
            }
            break;

        case WT_UT_SEG:
#ifdef _BATCH_UPDATE_
            if (dirty->buf_er != NULL) {
                lr = wt_updater->param.learn_rate;
                lr *= dirty->er_scale * dirty->in_scale;
                l2 = get_l2(wt_updater);

                for (a = 0; a < dirty->n_id; a++) {
                    i = dirty->ids[a];
                    seg = wt_updater->segs + i;
                    matXmat(src_wt + seg->s * col, dirty->buf_er[i].val,
                            dirty->buf_in[i].val, seg->n, col,
                            dirty->buf_er[i].n_row,
                            lr, 1.0 - l2);
                }
            }
#endif
            if (ori_wt == NULL) {
                for (a = 0; a < dirty->n_id; a++) {
                    seg = wt_updater->segs + dirty->ids[a];
                    for (i = seg->s; i < seg->s + seg->n; i++) {
                        for (j = 0; j < col; j++) {
                            dst_wt[i * col + j] += src_wt[i * col + j];
                            src_wt[i * col + j] = 0;
                        }
                    }
                }
            } else {
                for (a = 0; a < dirty->n_id; a++) {
                    seg = wt_updater->segs + dirty->ids[a];
                    for (i = seg->s; i < seg->s + seg->n; i++) {
                        for (j = 0; j < col; j++) {
                            dst_wt[i * col + j] += src_wt[i * col + j]
                                - ori_wt[i * col + j];
                        }
                    }
                    sz = col * seg->n * sizeof(real_t);
                    memcpy(src_wt + seg->s * col, dst_wt + seg->s * col, sz);
                    memcpy(ori_wt + seg->s * col, src_wt + seg->s * col, sz);
                }
            }
            break;

        case WT_UT_PART:
            if (ori_wt == NULL) {
                for (a = 0; a < dirty->n_seg; a++) {
                    for (i = dirty->segs[a].s; i < dirty->segs[a].e; i++) {
                        dst_wt[i] += src_wt[i];
                        src_wt[i] = 0;
                    }
                }
            } else {
                for (a = 0; a < dirty->n_seg; a++) {
                    for (i = dirty->segs[a].s; i < dirty->segs[a].e; i++) {
                        dst_wt[i] += src_wt[i] - ori_wt[i];
                    }
                    sz = dirty->segs[a].e - dirty->segs[a].s;
                    memcpy(src_wt + dirty->segs[a].s,
                            dst_wt + dirty->segs[a].s, sizeof(real_t) * sz);
                    memcpy(ori_wt + dirty->segs[a].s,
                            src_wt + dirty->segs[a].s, sizeof(real_t) * sz);
                }
            }
            break;

        case WT_UT_ONE_SHOT:
            if (ori_wt == NULL) {
                for (a = 0; a < dirty->n_id; a++) {
                    i = dirty->ids[a] * col;
                    for (; i < (dirty->ids[a] + 1) * col; i++) {
                        dst_wt[i] += src_wt[i];
                        src_wt[i] = 0;
                    }
                }
            } else {
                for (a = 0; a < dirty->n_id; a++) {
                    i = dirty->ids[a] * col;
                    for (; i < (dirty->ids[a] + 1) * col; i++) {
                        dst_wt[i] += src_wt[i] - ori_wt[i];
                    }
                    i = dirty->ids[a] * col;
                    memcpy(src_wt + i, dst_wt + i, sizeof(int) * col);
                    memcpy(ori_wt + i, src_wt + i, sizeof(int) * col);
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

static int wt_updater_acc_wt(wt_updater_t *wt_updater,
        real_t *wt, st_int_seg_t *row_seg, int row_seg_id,
        real_t *er, real_t er_scale,
        real_t *in, real_t in_scale, st_wt_int_t *in_idx)
{
    real_t lr;
    real_t l2;

    int row, col;

    int i, j, row_start, row_end;
    real_t scale;

    lr = wt_updater->param.learn_rate;
    lr *= er_scale * in_scale;
    row = wt_updater->row;
    col = wt_updater->col;

    l2 = get_l2(wt_updater);

    switch (wt_updater->type) {
        case WT_UT_PART:
            // Hash-based weight
            // needed: row_seg, er
            if (row_seg->s + row_seg->n > row) {
                for (j = 0, i = row_seg->s; i < row; j++, i++) {
                    wt[i] += lr * er[j] - l2 * wt[i];
                }
                for (i = 0; j < row_seg->n; j++, i++) {
                    wt[i] += lr * er[j] - l2 * wt[i];
                }
            } else {
                for (j = 0, i = row_seg->s; j < row_seg->n; j++, i++) {
                    wt[i] += lr * er[j] - l2 * wt[i];
                }
            }
            break;

        case WT_UT_ONE_SHOT:
            // needed: in_idx, er
            i = in_idx->i * col;
            scale = lr * in_idx->w;
            for (j = 0; j < col; j++, i++) {
                wt[i] += scale * er[j] - l2 * wt[i];
            }
            break;
        case WT_UT_SEG:
            // needed: in, er, row_seg_id
            /* FALL THROUGH */
        case WT_UT_FULL:
            // needed: in, er
#ifdef _BATCH_UPDATE_
            if (wt_updater->param.mini_batch > 0) {
                break; /* Do nothing. */
            }
#endif
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
            matXmat(wt + row_start * col, er, in, row_end - row_start, col, 1,
                    lr, 1.0 - l2);
            break;
        default:
            ST_WARNING("Unknown updating type[%d].", wt_updater->type);
            return -1;
    }

    return 0;
}

static int wt_updater_dirty(wt_updater_t *wt_updater, wt_dirty_buf_t *dirty,
        st_int_seg_t *row_seg, int row_seg_id, int in_idx,
        real_t *er, real_t er_scale, real_t *in, real_t in_scale)
{
    size_t sz;

    switch (wt_updater->type) {
        case WT_UT_FULL:
#ifdef _BATCH_UPDATE_
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
#endif
            break;
        case WT_UT_PART:
            if (dirty->n_seg >= dirty->cap_seg) {
                dirty->cap_seg += REALLOC_NUM;
                sz = dirty->cap_seg * sizeof(st_int_seg_t);
                dirty->segs = (st_int_seg_t *)realloc(dirty->segs, sz);
                if (dirty->segs == NULL) {
                    ST_WARNING("Failed to realloc segs");
                    return -1;
                }
            }
            if (st_int_seg_union(dirty->segs, dirty->cap_seg, &dirty->n_seg,
                        row_seg, 1, wt_updater->row) < 0) {
                ST_WARNING("Failed to st_int_seg_union.");
                return -1;
            }
            break;
        case WT_UT_ONE_SHOT:
            if (dirty->n_id >= dirty->cap_id) {
                dirty->cap_id += 100;
                sz = dirty->cap_id * sizeof(int);
                dirty->ids = (int *)realloc(dirty->ids, sz);
                if (dirty->ids == NULL) {
                    ST_WARNING("Failed to realloc ids");
                    return -1;
                }
            }
            if (st_int_insert(dirty->ids, dirty->cap_id,
                        &dirty->n_id, in_idx) < 0) {
                ST_WARNING("Failed to st_int_insert.");
                return -1;
            }
            break;

        case WT_UT_SEG:
            if (dirty->n_id >= dirty->cap_id) {
                dirty->cap_id += 100;
                sz = dirty->cap_id * sizeof(int);
                dirty->ids = (int *)realloc(dirty->ids, sz);
                if (dirty->ids == NULL) {
                    ST_WARNING("Failed to realloc ids");
                    return -1;
                }
            }
            if (st_int_insert(dirty->ids, dirty->cap_id,
                        &dirty->n_id, row_seg_id) < 0) {
                ST_WARNING("Failed to st_int_insert.");
                return -1;
            }
#ifdef _BATCH_UPDATE_
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
#endif
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
    int i;

    switch (wt_updater->type) {
        case WT_UT_FULL:
#ifdef _BATCH_UPDATE_
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
#endif
            break;
        case WT_UT_PART:
            if (dst->n_seg + src->n_seg > dst->cap_seg) {
                dst->cap_seg += src->n_seg;
                sz = dst->cap_seg * sizeof(st_int_seg_t);
                dst->segs = (st_int_seg_t *)realloc(dst->segs, sz);
                if (dst->segs == NULL) {
                    ST_WARNING("Failed to realloc segs");
                    return -1;
                }
            }
            for (i = 0; i < src->n_seg; i++) {
                src->segs[i].n = src->segs[i].e - src->segs[i].s;
            }
            if (st_int_seg_union(dst->segs, dst->cap_seg,
                        &dst->n_seg, src->segs, src->n_seg,
                        wt_updater->row) < 0) {
                ST_WARNING("Failed to st_int_seg_union.");
                return -1;
            }
            for (i = 0; i < src->n_seg; i++) {
                src->segs[i].e = src->segs[i].s + src->segs[i].n;
            }
            break;
        case WT_UT_SEG:
#ifdef _BATCH_UPDATE_
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
#endif
            /* FALL THROUGH */
        case WT_UT_ONE_SHOT:
            if (dst->n_id + src->n_id > dst->cap_id) {
                dst->cap_id += src->n_id;
                sz = dst->cap_id * sizeof(int);
                dst->ids = (int *)realloc(dst->ids, sz);
                if (dst->ids == NULL) {
                    ST_WARNING("Failed to realloc ids");
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
        default:
            ST_WARNING("Unknown updating type[%d].", wt_updater->type);
            return -1;
    }

    return 0;
}

int wt_update(wt_updater_t *wt_updater,
        st_int_seg_t *row_seg, int row_seg_id,
        real_t *er, real_t er_scale,
        real_t *in, real_t in_scale, st_wt_int_t *in_idx)
{
    real_t *wt;

    ST_CHECK_PARAM(wt_updater == NULL, -1);

#if _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Update weight");
#endif

    if (wt_updater->param.mini_batch > 0) {
        wt = wt_updater->delta_wt;
    } else {
        wt = wt_updater->wt;
    }

    if (wt_updater_acc_wt(wt_updater, wt, row_seg, row_seg_id,
                er, er_scale, in, in_scale, in_idx) < 0) {
        ST_WARNING("Failed to wt_updater_acc_wt.");
        return -1;
    }

    if (wt_updater->param.mini_batch > 0) {
        if (wt_updater_dirty(wt_updater, &wt_updater->mini_dirty,
                    row_seg, row_seg_id, in_idx != NULL ? in_idx->i : -1,
                    er, er_scale, in, in_scale) < 0) {
            ST_WARNING("Failed to wt_updater_dirty for minibatch.");
            return -1;
        }
    }

    if (wt_updater->param.sync_size > 0) {
        if (wt_updater->param.mini_batch <= 0) {
            if (wt_updater_dirty(wt_updater, &wt_updater->sync_dirty,
                        row_seg, row_seg_id, in_idx != NULL ? in_idx->i : -1,
                        er, er_scale, in, in_scale) < 0) {
                ST_WARNING("Failed to wt_updater_dirty for sync.");
                return -1;
            }
        }
    }

    return 0;
}

int wt_flush(wt_updater_t *wt_updater)
{
    ST_CHECK_PARAM(wt_updater == NULL, -1);

#if _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Flush weight");
#endif

    wt_updater->n_step++;

    if (wt_updater->param.mini_batch > 0) {
        if (wt_updater->n_step % wt_updater->param.mini_batch == 0) {
            if (wt_updater->param.sync_size > 0) {
                if (wt_updater_dirty_cpy(wt_updater,
                        &wt_updater->sync_dirty, &wt_updater->mini_dirty) < 0) {
                    ST_WARNING("Failed to wt_updater_dirty_cpy.");
                    return -1;
                }
            }

            if (wt_updater_flush(wt_updater, wt_updater->wt,
                        wt_updater->delta_wt, &wt_updater->mini_dirty,
                        NULL) < 0) {
                ST_WARNING("Failed to wt_updater_flush for minibatch.");
                return -1;
            }

            wt_updater->n_flush_step++;
        }
    } else {
        wt_updater->n_flush_step++;
    }

    if (wt_updater->param.sync_size > 0) {
        if (wt_updater->n_step % wt_updater->param.sync_size == 0) {
            if (wt_updater_flush(wt_updater, wt_updater->shared_wt,
                        wt_updater->wt, &wt_updater->sync_dirty,
                        wt_updater->ori_wt) < 0) {
                ST_WARNING("Failed to wt_updater_flush for sync.");
                return -1;
            }
        }
    }

    return 0;
}
