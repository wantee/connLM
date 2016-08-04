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
#include "utils.h"
#include "wt_updater.h"

#define N 8
#define REALLOC_NUM 100

#ifdef _BLAS_BATCH_UPDATE_
#define safe_concat_mat_destroy(ptr) do {\
    if((ptr) != NULL) {\
        concat_mat_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
void concat_mat_destroy(concat_mat_t *mat)
{
    if (mat == NULL) {
        return;
    }

    safe_free(mat->val);
    mat->col = 0;
    mat->n_row = 0;
    mat->cap_row = 0;
}
#endif

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

#ifdef _BLAS_BATCH_UPDATE_
    int i;
    safe_free(dirty->seg_ids);
    for (i = 0; i < dirty->n_seg_id; i++) {
        concat_mat_destroy(dirty->buf_in + i);
        concat_mat_destroy(dirty->buf_er + i);
    }
    dirty->n_seg_id = 0;
    dirty->cap_seg_id = 0;
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

    safe_free(wt_updater->segs);
    wt_updater->n_seg = 0;

    wt_dirty_destroy(&wt_updater->mini_dirty);
    wt_dirty_destroy(&wt_updater->sync_dirty);

    wt_updater_clear(wt_updater);
}

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

    return 0;

ERR:
    safe_free(wt_updater->segs);
    return -1;
}

void wt_dirty_clear(wt_dirty_buf_t *dirty)
{
    dirty->n_seg = 0;
    dirty->n_id = 0;

#ifdef _BLAS_BATCH_UPDATE_
    dirty->in_scale = 0.0;
    dirty->er_scale = 0.0;
#endif
}

void wt_updater_clear(wt_updater_t *wt_updater)
{
    wt_dirty_clear(&wt_updater->mini_dirty);
    wt_dirty_clear(&wt_updater->sync_dirty);
}

static int wt_updater_flush(wt_updater_t *wt_updater,
        real_t* dst_wt, real_t *src_wt, wt_dirty_buf_t *dirty, real_t *ori_wt)
{
    st_int_seg_t *seg;
    int row, col;
    int sz, i, a, j;

    row = wt_updater->row;
    col = wt_updater->col;

    switch (wt_updater->type) {
        case WT_UT_FULL:
            if (col > 0) {
                sz = row * col;
            } else {
                sz = row;
            }
            sz *= sizeof(real_t);
            if (ori_wt == NULL) {
                for (i = 0; i < sz; i++) {
                    dst_wt[i] += src_wt[i];
                }
            } else {
                for (i = 0; i < sz; i++) {
                    dst_wt[i] += src_wt[i] - ori_wt[i];
                }
                memcpy(src_wt, dst_wt, sz);
                memcpy(ori_wt, src_wt, sz);
            }
            break;

        case WT_UT_SEG:
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

static int wt_updater_acc_wt(wt_updater_t *wt_updater, count_t n_step,
        real_t *wt, st_int_seg_t *row_seg, int row_seg_id,
        real_t *er, real_t er_scale,
        real_t *in, real_t in_scale, st_wt_int_t *in_idx)
{
    real_t lr;
    real_t l2;

    int row, col;

    int i, j, row_start, row_end, er_start, er_end;
    real_t scale;

    lr = wt_updater->param.learn_rate;
    lr *= er_scale * in_scale;
    l2 = 0.0;
    row = wt_updater->row;
    col = wt_updater->col;

    if (wt_updater->param.l2_gap > 0
            && n_step % wt_updater->param.l2_gap == 0) {
        l2 = wt_updater->param.l2_penalty;
    }

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
#ifdef _BLAS_BATCH_UPDATE_
            break; /* Do nothing. */
#else
            // needed: in, er, row_seg_id
            /* FALL THROUGH */
#endif
        case WT_UT_FULL:
            // needed: in, er
            if (wt_updater->type == WT_UT_FULL) {
                row_start = er_start = 0;
                row_end = er_end = row;
            } else {
                row_start = wt_updater->segs[row_seg_id].s;
                row_end = row_start + wt_updater->segs[row_seg_id].n;
                er_start = 0;
                er_end = wt_updater->segs[row_seg_id].n;
            }

            if (row_start < 0 || row_start >= row_end) {
                ST_WARNING("Error row_seg_id[%d], lead to a invalid seg.",
                        row_seg_id);
                return -1;
            }
            wt += row_start * col;
#ifdef _USE_BLAS_
            cblas_gemm(CblasRowMajor, CblasTrans, CblasNoTrans,
                    row_end - row_start, col, 1,
                    lr, er, er_end - er_start, in, col,
                    1.0 - l2, wt, col);
#elif defined(_PARAM_UPDATE_UNROLL_)
            for (j = er_start; j < er_end; j++) {
                for (i = 0; i < col / N * N; i+=N) {
                    wt[i + 0] += lr * er[j] * in[i + 0] - l2 * wt[i + 0];
                    wt[i + 1] += lr * er[j] * in[i + 1] - l2 * wt[i + 1];
                    wt[i + 2] += lr * er[j] * in[i + 2] - l2 * wt[i + 2];
                    wt[i + 3] += lr * er[j] * in[i + 3] - l2 * wt[i + 3];

                    wt[i + 4] += lr * er[j] * in[i + 4] - l2 * wt[i + 4];
                    wt[i + 5] += lr * er[j] * in[i + 5] - l2 * wt[i + 5];
                    wt[i + 6] += lr * er[j] * in[i + 6] - l2 * wt[i + 6];
                    wt[i + 7] += lr * er[j] * in[i + 7] - l2 * wt[i + 7];
                }
                for (; i < col; i++) {
                    wt[i] += lr * er[j] * in[i] - l2 * wt[i];
                }
                wt += col;
            }
#else
            for (j = er_start; j < er_end; j++) {
                for (i = 0; i < col; i++) {
                    wt[i] += lr * er[j] * in[i] - l2 * wt[i];
                }
                wt += col;
            }
#endif
            break;
        default:
            ST_WARNING("Unknown updating type[%d].", wt_updater->type);
            return -1;
    }

    return 0;
}

#ifdef _BLAS_BATCH_UPDATE_

static int dirty_ensure_capacity(wt_dirty_buf_t *dirty, int extra)
{
    size_t sz;

    ST_CHECK_PARAM(dirty == NULL || extra <= 0, -1);

    if (dirty->n_seg_id + extra <= dirty->cap_seg_id) {
        return 0;
    }

    dirty->cap_seg_id += extra + REALLOC_NUM;

    sz = dirty->cap_seg_id * sizeof(seg_with_id_t);
    dirty->seg_ids = (seg_with_id_t *)realloc(dirty->seg_ids, sz);
    if (dirty->seg_ids == NULL) {
        ST_WARNING("Failed to realloc seg_ids");
        return -1;
    }

    sz = dirty->cap_seg_id * sizeof(concat_mat_t);
    dirty->buf_in = (concat_mat_t *)realloc(dirty->buf_in, sz);
    if (dirty->buf_in == NULL) {
        ST_WARNING("Failed to realloc buf_in");
        return -1;
    }

    dirty->buf_er = (concat_mat_t *)realloc(dirty->buf_er, sz);
    if (dirty->buf_er == NULL) {
        ST_WARNING("Failed to realloc buf_er");
        return -1;
    }

    return 0;
}

static int concat_mat_add_row(concat_mat_t *mat, real_t *vec, int vec_size)
{
    size_t sz;

    ST_CHECK_PARAM(mat == NULL || vec == NULL || vec_size <= 0, -1);

    if (mat->col == 0) {
        mat->col = vec_size;
    } else if (mat->col != vec_size) {
        ST_WARNING("col not match.");
        return -1;
    }

    if (mat->n_row >= mat->cap_row) {
        mat->cap_row += REALLOC_NUM;
        sz = mat->cap_row * mat->col * sizeof(real_t);
        mat->val = (real_t *)st_aligned_realloc(dirty->val, sz, ALIGN_SIZE);
        if (mat->val == NULL) {
            ST_WARNING("Failed to realloc val");
            return -1;
        }
    }

    memcpy(mat->val + mat->n_row * mat->col, vec, sizeof(real_t) * mat->col);
    mat->n_row++;

    return 0;
}

static int wt_updater_dirty(wt_updater_t *wt_updater, wt_dirty_buf_t *dirty,
        st_int_seg_t *row_seg, int row_seg_id, int in_idx,
        real_t *er, real_t er_scale, real_t *in, real_t in_scale)
#else
static int wt_updater_dirty(wt_updater_t *wt_updater, wt_dirty_buf_t *dirty,
        st_int_seg_t *row_seg, int row_seg_id, int in_idx)
#endif
{
    size_t sz;

#ifdef _BLAS_BATCH_UPDATE_
    seg_with_id_t seg_id;
#endif

    switch (wt_updater->type) {
        case WT_UT_FULL:
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
#ifdef _BLAS_BATCH_UPDATE_
            if (dirty_ensure_capacity(dirty, 1) < 0) {
                ST_WARNING("Failed to dirty_ensure_capacity");
                return -1;
            }

            seg_id.seg = *er_seg;
            seg_id.id = st_insert((void *)dirty->seg_ids, dirty->cap_seg_id,
                    sizeof(seg_with_id_t), &dirty->n_seg_id,
                    &seg_id, cmp_seg_id, NULL);
            if (seg_id.id < 0) {
                ST_WARNING("Failed to st_insert.");
                return -1;
            }

            if (seg_id.id == dirty->n_seg_id - 1) { // fresh added
                memset(dirty->buf_er + seg_id.id, 0, sizeof(concat_mat_t));
                memset(dirty->buf_in + seg_id.id, 0, sizeof(concat_mat_t));
            }

            if (dirty->er_scale == 0.0) {
                dirty->er_scale = er_scale;
            } else if (dirty->er_scale != er_scale) {
                ST_WARNING("er_scale changed.");
                return -1;
            }
            if (concat_mat_add_row(dirty->buf_er + seg_id.id, er + er_seg->s,
                        er_seg->e - er_seg->s) < 0) {
                ST_WARNING("Failed to concat_mat_add_row for buf_er.");
                return -1;
            }

            if (dirty->in_scale == 0.0) {
                dirty->in_scale = in_scale;
            } else if (dirty->in_scale != in_scale) {
                ST_WARNING("in_scale changed.");
                return -1;
            }
            if (concat_mat_add_row(dirty->buf_in + seg_id.id, in,
                        wt_updater->col) < 0) {
                ST_WARNING("Failed to concat_mat_add_row for buf_in.");
                return -1;
            }
#else
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
#ifdef _BLAS_BATCH_UPDATE_
            if (dst->n_seg_id + src->n_seg_id >= dst->cap_seg_id) {
                dst->cap_seg_id += src->n_seg_id;
                sz = dst->cap_seg_id * sizeof(seg_with_id_t);
                dst->seg_ids = (seg_with_id_t *)realloc(dst->seg_ids, sz);
                if (dst->seg_ids == NULL) {
                    ST_WARNING("Failed to realloc seg_ids");
                    return -1;
                }
            }

            /* TODO: We could use merge sort here. */
            for (i = 0; i < src->n_seg_id; i++) {
                id = st_insert((void *)dst->seg_ids, dst->cap_seg_id,
                        sizeof(seg_with_id_t), &dst->n_seg_id,
                        &src->seg_ids + i, cmp_seg_id, NULL);
                if (id < 0) {
                    ST_WARNING("Failed to st_insert.");
                    return -1;
                }

                if (id >= dst->cap_buf) {
                    dst->cap_buf += REALLOC_NUM;
                    sz = dst->cap_buf * wt_updater->col * sizeof(real_t);
                    dst->buf_in = (real_t *)st_aligned_realloc(dst->buf_in,
                            sz, ALIGN_SIZE);
                    if (dst->buf_in == NULL) {
                        ST_WARNING("Failed to realloc buf_in");
                        return -1;
                    }

                    sz = dst->cap_buf * sizeof(real_t)
                        * (src->seg_ids[i].seg.e - src->seg_ids[i].seg.s);
                    dst->buf_er = (real_t *)st_aligned_realloc(dst->buf_er,
                            sz, ALIGN_SIZE);
                    if (dst->buf_er == NULL) {
                        ST_WARNING("Failed to realloc buf_er");
                        return -1;
                    }
                }

                if (dst->er_scale == 0.0) {
                    dst->er_scale = src->er_scale;
                } else if (dst->er_scale != src->er_scale) {
                    ST_WARNING("er_scale changed.");
                    return -1;
                }
                memcpy(dst->buf_er + id, src->buf_er + src->seg_ids[i].id,
                        sizeof(real_t)
                        * (src->seg_ids[i].seg.e - src->seg_ids[i].seg.s));

                if (dst->in_scale == 0.0) {
                    dst->in_scale = src->in_scale;
                } else if (dst->in_scale != src->in_scale) {
                    ST_WARNING("in_scale changed.");
                    return -1;
                }
                memcpy(dst->buf_in + id, in,
                        sizeof(real_t) * wt_updater->col);
            }
            break;
#else
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
#endif
            break;
        default:
            ST_WARNING("Unknown updating type[%d].", wt_updater->type);
            return -1;
    }

    return 0;
}

int wt_update(wt_updater_t *wt_updater, count_t n_step,
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

    if (wt_updater_acc_wt(wt_updater, n_step, wt, row_seg, row_seg_id,
                er, er_scale, in, in_scale, in_idx) < 0) {
        ST_WARNING("Failed to wt_updater_acc_wt.");
        return -1;
    }

    if (wt_updater->param.mini_batch > 0) {
#ifdef _BLAS_BATCH_UPDATE_
        if (wt_updater_dirty(wt_updater, &wt_updater->mini_dirty,
                    row_seg, in_idx != NULL ? in_idx->i : -1,
                    er, er_scale, in, in_scale) < 0) {
            ST_WARNING("Failed to wt_updater_dirty for minibatch.");
            return -1;
        }
#else
        if (wt_updater_dirty(wt_updater, &wt_updater->mini_dirty,
                row_seg, row_seg_id, in_idx != NULL ? in_idx->i : -1) < 0) {
            ST_WARNING("Failed to wt_updater_dirty for minibatch.");
            return -1;
        }
#endif
    }

    if (wt_updater->param.sync_size > 0) {
        if (wt_updater->param.mini_batch <= 0) {
#ifdef _BLAS_BATCH_UPDATE_
            if (wt_updater_dirty(wt_updater, &wt_updater->sync_dirty,
                        row_seg, in_idx != NULL ? in_idx->i : -1,
                        er, er_scale, in, in_scale) < 0) {
                ST_WARNING("Failed to wt_updater_dirty for sync.");
                return -1;
            }
#else
            if (wt_updater_dirty(wt_updater, &wt_updater->sync_dirty,
                    row_seg, row_seg_id, in_idx != NULL ? in_idx->i : -1) < 0) {
                ST_WARNING("Failed to wt_updater_dirty for sync.");
                return -1;
            }
#endif
        }
    }

    return 0;
}

int wt_flush(wt_updater_t *wt_updater, count_t n_step)
{
    ST_CHECK_PARAM(wt_updater == NULL, -1);

#if _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Flush weight");
#endif

    if (wt_updater->param.mini_batch > 0) {
        if (n_step % wt_updater->param.mini_batch == 0) {
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
        }
    }

    if (wt_updater->param.sync_size > 0) {
        if (n_step % wt_updater->param.sync_size == 0) {
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
