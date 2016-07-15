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
#include "param_updater.h"

void param_updater_destroy(param_updater_t *param_updater)
{
    if (param_updater == NULL) {
        return;
    }

    if (param_updater->wt != param_updater->shared_wt) {
        safe_free(param_updater->wt);
    } else {
        param_updater->wt = NULL;
    }
    param_updater->shared_wt = NULL;
    safe_free(param_updater->delta_wt);
    param_updater->row = -1;
    param_updater->col = -1;

    param_updater_clear(param_updater);
}

param_updater_t* param_updater_create(param_t *param,
        real_t *wt, int row, int col, wt_update_type_t type)
{
    param_updater_t *param_updater = NULL;

    size_t sz;

    ST_CHECK_PARAM(param == NULL || wt == NULL || row <= 0, NULL);

    param_updater = (param_updater_t *)malloc(sizeof(param_updater_t));
    if (param_updater == NULL) {
        ST_WARNING("Failed to malloc param_updater.");
        goto ERR;
    }
    memset(param_updater, 0, sizeof(param_updater_t));

    param_updater->param = *param;
    param_updater->row = row;
    param_updater->col = col;
    param_updater->shared_wt = wt;
    param_updater->type = type;

    if (param_updater->col > 0) {
        sz =  param_updater->row * param_updater->col;
    } else {
        sz = param_updater->row;
    }
    sz *= sizeof(real_t);

    if (param_updater->param.sync_size > 0) {
        param_updater->wt = (real_t *)malloc(sz);
        if (posix_memalign((void **)&param_updater->wt, ALIGN_SIZE, sz) != 0
                || param_updater->wt == NULL) {
            ST_WARNING("Failed to malloc wt.");
            goto ERR;
        }
        memcpy(param_updater->wt, wt, sz);
    } else {
        param_updater->wt = wt;
    }

    if (param_updater->param.mini_batch > 0) {
        param_updater->delta_wt = (real_t *)malloc(sz);
        if (posix_memalign((void **)&param_updater->delta_wt,
                    ALIGN_SIZE, sz) != 0
                || param_updater->delta_wt == NULL) {
            ST_WARNING("Failed to malloc delta_wt.");
            goto ERR;
        }
        memset(param_updater->delta_wt, 0, sz);
    }

    switch (type) {
        case WT_UT_FULL:
            break;
        case WT_UT_PART:
#if 0
            if (param_updater->param.mini_batch > 0) {
                sz = param_updater->param.mini_batch;
                param_updater->hist = (int *)malloc(sizeof(int) * sz);
                if (param_updater->hist == NULL) {
                    ST_WARNING("Failed to malloc hist");
                    goto ERR;
                }
            }
            break;
        case WT_UT_ONE_SHOT:
            if (param_updater->param.mini_batch > 0) {
                sz = param_updater->param.mini_batch;
                param_updater->hist = (int *)malloc(sizeof(int) * sz);
                if (param_updater->hist == NULL) {
                    ST_WARNING("Failed to malloc hist");
                    goto ERR;
                }
            }
#endif
            break;
        default:
            ST_WARNING("Unknown updating type[%d].", type);
            goto ERR;
    }

    return param_updater;

ERR:
    safe_param_updater_destroy(param_updater);
    return NULL;
}

void param_updater_clear(param_updater_t *param_updater)
{
//    param_updater->num_hist = 0;
}

#define N 8

static int param_updater_update_wt(param_updater_t *param_updater,
        real_t* dst_wt, real_t *src_wt, bool delta)
{
#if 0
    int row, col;
    int sz, i, j, a;

    row = param_updater->row;
    col = param_updater->col;

    switch (param_updater->type) {
        case WT_UT_FULL:
            if (col > 0) {
                sz = row * col;
            } else {
                sz = row;
            }
            sz *= sizeof(real_t);
            if (delta) {
                for (i = 0; i < sz; i++) {
                    dst_wt[i] += src_wt[i];
                }
            } else {
                memcpy(dst_wt, src_wt, sz);
            }
            break;

        case WT_UT_ONE_SHOT:
            // assert(col > 0);
            if (delta) {
                for (a = 0; a < num_idx; a++) {
                    i = idx_buf[a];
                    for (j = 0; j < row; j++) {
                        dst_wt[i] += src_wt[i];
                        i += col;
                    }
                }
            } else {
            }
            break;
        default:
            ST_WARNING("Unknown updating type[%d].", param_updater->type);
            return -1;
    }
#endif

    return 0;
}

static int param_updater_acc_wt(param_updater_t *param_updater, count_t n_step,
        real_t *wt, int row_s,
        real_t *er, real_t er_scale, st_int_seg_t *er_seg, int seg_id,
        real_t *in, real_t in_scale, st_wt_int_t *in_idx)
{
//    real_t *w;

    real_t lr;
    real_t l2;

    int row;//, col;

    int i, j;

    lr = param_updater->param.learn_rate;
    lr *= er_scale * in_scale;
    l2 = 0.0;
    row = param_updater->row;
//    col = param_updater->col;

    if (param_updater->param.l2_gap > 0
            && n_step % param_updater->param.l2_gap == 0) {
        l2 = param_updater->param.l2_penalty;
    }

    switch (param_updater->type) {
        case WT_UT_PART:
            // Hash-based weight
            if (er_seg == NULL) {
                ST_WARNING("no er_seg for WT_UT_PART");
                return -1;
            }

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

#if 0
        case WT_UT_ONE_SHOT:
            // ignore er_s & er_e
            i = in_idx;
            for (j = 0; j < row; j++) {
                wt[i] += lr * er[j] - l2 * wt[i];
                i += col;
            }
            break;
        case WT_UT_ONE_SEG:
            /* FALL THROUGH */
        case WT_UT_ONE_FULL:
            if (param_updater->type == WT_UT_ONE_FULL) {
                start = 0;
                end = row;
            } else {
                start = er_s;
                end = er_e;
            }
            lr *= in_scale;
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
            w = wt;
            for (j = start; j < end; j++) {
                for (i = 0; i < col; i++) {
                    w[i] += lr * er[j] * in[i] - l2 * w[i];
                }
                w += col;
            }
#endif
#endif
        default:
            ST_WARNING("Unknown updating type[%d].", param_updater->type);
            return -1;
    }

    return 0;
}

static int param_updater_record(param_updater_t *param_updater)
{
    return 0;
}

int param_update(param_updater_t *param_updater, count_t n_step, int row_s,
        real_t *er, real_t er_scale, st_int_seg_t *er_seg, int seg_id,
        real_t *in, real_t in_scale, st_wt_int_t *in_idx)
{
    real_t *wt;

    if (param_updater->param.mini_batch > 0) {
        wt = param_updater->delta_wt;
    } else {
        wt = param_updater->wt;
    }

    if (param_updater_acc_wt(param_updater, n_step, wt, row_s,
                er, er_scale, er_seg, seg_id,
                in, in_scale, in_idx) < 0) {
        ST_WARNING("Failed to param_updater_acc_wt.");
        return -1;
    }

    if (param_updater->param.mini_batch > 0) {
        if (param_updater_record(param_updater) < 0) {
            ST_WARNING("Failed to param_updater_record for minibatch.");
            return -1;
        }

        if (n_step % param_updater->param.mini_batch == 0) {
            if (param_updater_update_wt(param_updater, param_updater->wt,
                        param_updater->delta_wt, true) < 0) {
                ST_WARNING("Failed to param_updater_update_wt for minibatch.");
                return -1;
            }
        }
    }

    if (param_updater->param.sync_size > 0) {
        if (param_updater_record(param_updater) < 0) {
            ST_WARNING("Failed to param_updater_record for sync.");
            return -1;
        }

        if (n_step % param_updater->param.sync_size == 0) {
            if (param_updater_update_wt(param_updater, param_updater->shared_wt,
                        param_updater->wt, false) < 0) {
                ST_WARNING("Failed to param_updater_update_wt for sync.");
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

int param_update_minibatch(param_updater_t *param_updater, bool update_arg,
        int batch, real_t *wt, real_t *er, real_t er_scale,
        int er_size, real_t *in, int in_size)
{
    real_t lr;
    real_t l2;

    lr = param_updater->param.learn_rate;
    l2 = param_updater->param.l2_penalty;

    if (param_updater->param.l2_gap > 1) {
        if (param_updater->l2_step != 0) {
            l2 = 0.0;
        }

        if (update_arg) {
            param_updater->l2_step++;
            if (param_updater->l2_step >= param_updater->param.l2_gap) {
                param_updater->l2_step = 0;
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

static void param_updater_minibatch(param_updater_t *param_updater)
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
