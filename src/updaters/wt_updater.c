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

#include "blas.h"
#include "utils.h"
#include "wt_updater.h"

#define N 8

void wt_dirty_destroy(wt_dirty_buf_t *dirty)
{
    if (dirty == NULL) {
        return;
    }

    safe_free(dirty->segs);
    dirty->cap_seg = 0;
    dirty->n_seg = 0;

    safe_free(dirty->in_idxs);
    dirty->cap_in_idx = 0;
    dirty->n_in_idx = 0;
}

void wt_updater_destroy(wt_updater_t *wt_updater)
{
    if (wt_updater == NULL) {
        return;
    }

    if (wt_updater->wt != wt_updater->shared_wt) {
        safe_free(wt_updater->wt);
    } else {
        wt_updater->wt = NULL;
    }
    wt_updater->shared_wt = NULL;
    safe_free(wt_updater->ori_wt);
    safe_free(wt_updater->delta_wt);
    wt_updater->row = -1;
    wt_updater->col = -1;

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
        if (posix_memalign((void **)&wt_updater->wt, ALIGN_SIZE, sz) != 0
                || wt_updater->wt == NULL) {
            ST_WARNING("Failed to malloc wt.");
            goto ERR;
        }
        memcpy(wt_updater->wt, wt, sz);

        if (posix_memalign((void **)&wt_updater->ori_wt, ALIGN_SIZE, sz) != 0
                || wt_updater->ori_wt == NULL) {
            ST_WARNING("Failed to malloc ori_wt.");
            goto ERR;
        }
        memcpy(wt_updater->ori_wt, wt_updater->wt, sz);
    } else {
        wt_updater->wt = wt;
    }

    if (wt_updater->param.mini_batch > 0) {
        if (posix_memalign((void **)&wt_updater->delta_wt,
                    ALIGN_SIZE, sz) != 0
                || wt_updater->delta_wt == NULL) {
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

void wt_dirty_clear(wt_dirty_buf_t *dirty)
{
    dirty->n_seg = 0;
    dirty->n_in_idx = 0;
}

void wt_updater_clear(wt_updater_t *wt_updater)
{
    wt_dirty_clear(&wt_updater->mini_dirty);
    wt_dirty_clear(&wt_updater->sync_dirty);
}

static int wt_updater_flush(wt_updater_t *wt_updater,
        real_t* dst_wt, real_t *src_wt, wt_dirty_buf_t *dirty, real_t *ori_wt)
{
    int row, col;
    int sz, i, a;

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
            dirty->n_seg = 0;

            break;

        case WT_UT_ONE_SHOT:
            if (ori_wt == NULL) {
                for (a = 0; a < dirty->n_in_idx; a++) {
                    i = dirty->in_idxs[a] * col;
                    for (; i < (dirty->in_idxs[a] + 1) * col; i++) {
                        dst_wt[i] += src_wt[i];
                        src_wt[i] = 0;
                    }
                }
            } else {
                for (a = 0; a < dirty->n_in_idx; a++) {
                    i = dirty->in_idxs[a] * col;
                    for (; i < (dirty->in_idxs[a] + 1) * col; i++) {
                        dst_wt[i] += src_wt[i] - ori_wt[i];
                    }
                    i = dirty->in_idxs[a] * col;
                    memcpy(src_wt + i, dst_wt + i, sizeof(int) * col);
                    memcpy(ori_wt + i, src_wt + i, sizeof(int) * col);
                }
            }
            dirty->n_in_idx = 0;
            break;
        default:
            ST_WARNING("Unknown updating type[%d].", wt_updater->type);
            return -1;
    }

    return 0;
}

static int wt_updater_acc_wt(wt_updater_t *wt_updater, count_t n_step,
        real_t *wt, int row_s,
        real_t *er, real_t er_scale, st_int_seg_t *er_seg,
        real_t *in, real_t in_scale, st_wt_int_t *in_idx)
{
    real_t lr;
    real_t l2;

    int row, col;

    int i, j, start, end;
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
            // needed: row_s, er_seg, er
            if (row_s + er_seg->n > row) {
                for (j = er_seg->s, i = row_s; i < row; j++, i++) {
                    wt[i] += lr * er[j] - l2 * wt[i];
                }
                for (i = 0; j < er_seg->n + er_seg->s; j++, i++) {
                    wt[i] += lr * er[j] - l2 * wt[i];
                }
            } else {
                for (j = er_seg->s, i = row_s;
                        j < er_seg->n + er_seg->s; j++, i++) {
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
            /* FALL THROUGH */
        case WT_UT_FULL:
            if (wt_updater->type == WT_UT_FULL) {
                start = 0;
                end = row;
            } else {
                start = er_seg->s;
                end = er_seg->e;
            }
#ifdef _USE_BLAS_
            cblas_gemm(CblasRowMajor, CblasTrans, CblasNoTrans,
                    end - start, col, 1,
                    lr, er, end - start, in, col,
                    1.0, wt, col);
#elif defined(_PARAM_UPDATE_UNROLL_)
            for (j = start; j < end; j++) {
                for (i = 0; i < col / N * N; i+=N) {
                    wt[j * col + i + 0] += lr * er[j] * in[i + 0]
                        - l2 * wt[j * col + i + 0];
                    wt[j * col + i + 1] += lr * er[j] * in[i + 1]
                        - l2 * wt[j * col + i + 1];
                    wt[j * col + i + 2] += lr * er[j] * in[i + 2]
                        - l2 * wt[j * col + i + 2];
                    wt[j * col + i + 3] += lr * er[j] * in[i + 3]
                        - l2 * wt[j * col + i + 3];

                    wt[j * col + i + 4] += lr * er[j] * in[i + 4]
                        - l2 * wt[j * col + i + 4];
                    wt[j * col + i + 5] += lr * er[j] * in[i + 5]
                        - l2 * wt[j * col + i + 5];
                    wt[j * col + i + 6] += lr * er[j] * in[i + 6]
                        - l2 * wt[j * col + i + 6];
                    wt[j * col + i + 7] += lr * er[j] * in[i + 7]
                        - l2 * wt[j * col + i + 7];
                }
                for (; i < col; i++) {
                    wt[j * col + i] += lr * er[j] * in[i]
                        - l2 * wt[j * col + i];
                }
            }
#else
            for (j = start; j < end; j++) {
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

static int wt_updater_dirty(wt_updater_t *wt_updater, wt_dirty_buf_t *dirty,
        int row_s, st_int_seg_t *er_seg, int in_idx)
{
    st_int_seg_t row_seg;
    size_t sz;

    switch (wt_updater->type) {
        case WT_UT_FULL:
            break;
        case WT_UT_PART:
            if (dirty->n_seg >= dirty->cap_seg) {
                dirty->cap_seg += 100;
                sz = dirty->cap_seg * sizeof(st_int_seg_t);
                dirty->segs = (st_int_seg_t *)realloc(dirty->segs, sz);
                if (dirty->segs == NULL) {
                    ST_WARNING("Failed to realloc segs");
                    return -1;
                }
            }
            row_seg.s = row_s;
            row_seg.n = er_seg->n;
            if (st_int_seg_union(dirty->segs, dirty->cap_seg, &dirty->n_seg,
                        &row_seg, 1, wt_updater->row) < 0) {
                ST_WARNING("Failed to st_int_seg_union.");
                return -1;
            }
            break;
        case WT_UT_ONE_SHOT:
            if (dirty->n_in_idx >= dirty->cap_in_idx) {
                dirty->cap_in_idx += 100;
                sz = dirty->cap_in_idx * sizeof(int);
                dirty->in_idxs = (int *)realloc(dirty->in_idxs, sz);
                if (dirty->in_idxs == NULL) {
                    ST_WARNING("Failed to realloc in_idxs");
                    return -1;
                }
            }
            if (st_int_insert(dirty->in_idxs, dirty->cap_in_idx,
                        &dirty->n_in_idx, in_idx) < 0) {
                ST_WARNING("Failed to st_int_insert.");
                return -1;
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
        case WT_UT_ONE_SHOT:
            if (dst->n_in_idx + src->n_in_idx > dst->cap_in_idx) {
                dst->cap_in_idx += src->n_in_idx;
                sz = dst->cap_in_idx * sizeof(int);
                dst->in_idxs = (int *)realloc(dst->in_idxs, sz);
                if (dst->in_idxs == NULL) {
                    ST_WARNING("Failed to realloc in_idxs");
                    return -1;
                }
            }
            for (i = 0; i < src->n_in_idx; i++) {
                if (st_int_insert(dst->in_idxs, dst->cap_in_idx,
                            &dst->n_in_idx, src->in_idxs[i]) < 0) {
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

int wt_update(wt_updater_t *wt_updater, count_t n_step, int row_s,
        real_t *er, real_t er_scale, st_int_seg_t *er_seg,
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

    if (wt_updater_acc_wt(wt_updater, n_step, wt, row_s,
                er, er_scale, er_seg,
                in, in_scale, in_idx) < 0) {
        ST_WARNING("Failed to wt_updater_acc_wt.");
        return -1;
    }

    if (wt_updater->param.mini_batch > 0) {
        if (wt_updater_dirty(wt_updater, &wt_updater->mini_dirty, row_s,
                    er_seg, in_idx != NULL ? in_idx->i : -1) < 0) {
            ST_WARNING("Failed to wt_updater_dirty for minibatch.");
            return -1;
        }
    }

    if (wt_updater->param.sync_size > 0) {
        if (wt_updater->param.mini_batch <= 0) {
            if (wt_updater_dirty(wt_updater, &wt_updater->sync_dirty, row_s,
                        er_seg, in_idx != NULL ? in_idx->i : -1) < 0) {
                ST_WARNING("Failed to wt_updater_dirty for sync.");
                return -1;
            }
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

#if 0
#ifdef _MINI_UPDATE_
int param_acc_wt_minibatch(int batch, real_t *wt, real_t *er, int er_size,
        real_t *in, int in_size)
{
    if (!(er_size > 0 && in_size > 0 && in != NULL)) {
        ST_WARNING("Do not support this type of update_minibatch.");
        return -1;
    }

    cblas_gemm(CblasRowMajor, CblasTrans, CblasNoTrans,
            er_size, in_size, batch,
            1.0, er, er_size, in, in_size,
            1.0, wt, in_size);
}

int param_update_minibatch(wt_updater_t *wt_updater, bool update_arg,
        int batch, real_t *wt, real_t *er, real_t er_scale,
        int er_size, real_t *in, int in_size)
{
    real_t lr;
    real_t l2;

    lr = wt_updater->param.learn_rate;
    l2 = wt_updater->param.l2_penalty;

    if (wt_updater->param.l2_gap > 1) {
        if (wt_updater->l2_step != 0) {
            l2 = 0.0;
        }

        if (update_arg) {
            wt_updater->l2_step++;
            if (wt_updater->l2_step >= wt_updater->param.l2_gap) {
                wt_updater->l2_step = 0;
            }
        }
    }

    if (!(er_size > 0 && in_size > 0 && in != NULL)) {
        ST_WARNING("Do not support this type of update_minibatch.");
        return;
    }

    cblas_gemm(CblasRowMajor, CblasTrans, CblasNoTrans,
            er_size, in_size, batch,
            lr*er_scale, er, er_size, in, in_size,
            1.0 - l2, wt, in_size);
}
#endif
#endif

#if 0
/*
 * accumulate weights
 *
 * in is [ in_size x 1 ];
 *
 *
 * er_size > 0 && in_size > 0: er is [ 1 x er_size ]; wt is [ er_size x in_size ]; if in == NULL: in is one-shot vector
 * er_size > 0 && in_size < 0: er is [ 1 x er_size ]; wt is hash based 1d vector; in is one-shot vector
 * er_size < 0 && in_size > 0: er is delta-weight matrix [ er_size x in_size ]; wt is [ er_size x in_size ];
 * er_size < 0 && in_size < 0: er is delta-weight matrix [ er_size x in_size ]; wt is [ er_size x in_size ]; in is one-shot vector
 */
void param_acc_wt(real_t *wt, real_t *er, int er_size, real_t *in, int in_size)
{
    real_t *w;
    real_t *delta_w;

    int i;
    int j;

    if (er_size > 0 && in_size > 0) {
        if (in == NULL) {
            i = 0;
            for (j = 0; j < er_size; j++) {
                wt[i] += er[j];
                i += in_size;
            }
        } else {
#ifdef _USE_BLAS_
            cblas_gemm(CblasRowMajor, CblasTrans, CblasNoTrans,
                    er_size, in_size, 1,
                    1.0, er, er_size, in, in_size,
                    1.0, wt, in_size);
#elif defined(_PARAM_UPDATE_UNROLL_)
            for (j = 0; j < er_size; j++) {
                for (i = 0; i < in_size / N * N; i+=N) {
                    wt[j * in_size + i + 0] += er[j] * in[i + 0];
                    wt[j * in_size + i + 1] += er[j] * in[i + 1];
                    wt[j * in_size + i + 2] += er[j] * in[i + 2];
                    wt[j * in_size + i + 3] += er[j] * in[i + 3];

                    wt[j * in_size + i + 4] += er[j] * in[i + 4];
                    wt[j * in_size + i + 5] += er[j] * in[i + 5];
                    wt[j * in_size + i + 6] += er[j] * in[i + 6];
                    wt[j * in_size + i + 7] += er[j] * in[i + 7];
                }

                for (; i < in_size; i++) {
                    wt[j * in_size + i] += er[j] * in[i];
                }
            }
#else
            w = wt;
            for (j = 0; j < er_size; j++) {
                for (i = 0; i < in_size; i++) {
                    w[i] += er[j] * in[i];
                }
                w += in_size;
            }
#endif
        }
    } else if (er_size > 0 && in_size < 0) {
        for (j = 0; j < er_size; j++) {
            wt[j] += er[j];
        }
    } else if (er_size < 0 && in_size > 0) {
        er_size = - er_size;
        w = wt;
        delta_w = er;

        for (j = 0; j < er_size; j++) {
            for (i = 0; i < in_size; i++) {
                w[i] += delta_w[i];
            }
            w += in_size;
            delta_w += in_size;
        }
    } else {
        in_size = - in_size;
        er_size = - er_size;
        w = wt;
        delta_w = er;

        i = 0;
        for (j = 0; j < er_size; j++) {
            w[i] += delta_w[i];
            i += in_size;
        }
    }
}

static void wt_updater_minibatch(wt_updater_t *wt_updater)
{
    if (er_size < 0 && in_size > 0) {
        er_size = - er_size;

#ifdef _PARAM_UPDATE_UNROLL_
        for (j = 0; j < er_size; j++) {
            for (i = 0; i < in_size / N * N; i+=N) {
                wt[j * in_size + i + 0] += lr * er[j * in_size + i + 0] * er_scale
                    - l2 * wt[j * in_size + i + 0];
                wt[j * in_size + i + 1] += lr * er[j * in_size + i + 1] * er_scale
                    - l2 * wt[j * in_size + i + 1];
                wt[j * in_size + i + 2] += lr * er[j * in_size + i + 2] * er_scale
                    - l2 * wt[j * in_size + i + 2];
                wt[j * in_size + i + 3] += lr * er[j * in_size + i + 3] * er_scale
                    - l2 * wt[j * in_size + i + 3];

                wt[j * in_size + i + 4] += lr * er[j * in_size + i + 4] * er_scale
                    - l2 * wt[j * in_size + i + 4];
                wt[j * in_size + i + 5] += lr * er[j * in_size + i + 5] * er_scale
                    - l2 * wt[j * in_size + i + 5];
                wt[j * in_size + i + 6] += lr * er[j * in_size + i + 6] * er_scale
                    - l2 * wt[j * in_size + i + 6];
                wt[j * in_size + i + 7] += lr * er[j * in_size + i + 7] * er_scale
                    - l2 * wt[j * in_size + i + 7];
            }

            for (; i < in_size; i++) {
                wt[j * in_size + i] += lr * er[j * in_size + i] * er_scale
                    - l2 * wt[j * in_size + i];
            }
        }
#else
        w = wt;
        delta_w = er;
        for (j = 0; j < er_size; j++) {
            for (i = 0; i < in_size; i++) {
                w[i] += lr * delta_w[i] * er_scale
                    - l2 * w[i];
            }
            w += in_size;
            delta_w += in_size;
        }
#endif
    } else {
        in_size = - in_size;
        er_size = - er_size;
        w = wt;
        delta_w = er;

        i = 0;
        for (j = 0; j < er_size; j++) {
            w[i] += lr * delta_w[i] * er_scale
                - l2 * w[i];
            i += in_size;
        }
    }
}
#endif
