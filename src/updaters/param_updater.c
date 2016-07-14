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

/*
 * row > 0 && col > 0: wt is [ row x col ];
 * row > 0 && col < 0: wt is hash based 1d vector [ row ];
 */
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
    param_updater->num_step = 0;
    param_updater->num_hist = 0;
}

#define N 8

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

static void param_updater_sync(param_updater_t *param_updater)
{
    size_t sz;

    if (param_updater->col > 0) {
        sz =  param_updater->row * param_updater->col;
    } else {
        sz = param_updater->row;
    }
    sz *= sizeof(real_t);
    memcpy(param_updater->shared_wt, param_updater->wt, sz);
}

/*
 * update weight using parameters.
 *
 * in is [ in_size x 1 ];
 *
 *
 * er_size > 0 && in_size > 0: er is [ 1 x er_size ]; wt is [ er_size x in_size ]; if in == NULL: in is one-shot vector
 * er_size < 0 && in_size > 0: er is delta-weight matrix [ er_size x in_size ]; wt is [ er_size x in_size ];
 * er_size < 0 && in_size < 0: er is delta-weight matrix [ er_size x in_size ]; wt is [ er_size x in_size ]; in is one-shot vector
 */
void param_update(param_updater_t *param_updater,
        real_t *er, real_t er_scale,
        int er_s, int er_e, real_t *in, int in_idx)
{
    real_t *wt, *w;

    real_t lr;
    real_t l2;

    int row, col;

    int i, j;

    lr = param_updater->param.learn_rate;
    l2 = 0.0;
    row = param_updater->row;
    col = param_updater->col;

    param_updater->num_step++;

    if (param_updater->param.l2_gap > 0
        && param_updater->num_step % param_updater->param.l2_gap == 0) {
        l2 = param_updater->param.l2_penalty;
    }

    if (param_updater->param.mini_batch > 0) {
        wt = param_updater->delta_wt;
    } else {
        wt = param_updater->wt;
    }

    if (col < 0) { // Hash-based weight
        for (j = er_s; j < er_e; j++) {
            wt[j] += lr * er[j] * er_scale /* * 1.0 */ - l2 * wt[j];
        }
    } else { // matrix weight
        if (in == NULL) { // one-shot vector
            i = in_idx;
            for (j = er_s; j < er_e; j++) {
                wt[i] += lr * er[j] * er_scale - l2 * wt[i];
                i += col;
            }
        } else {
#ifdef _PARAM_UPDATE_UNROLL_
            for (j = er_s; j < er_e; j++) {
                for (i = 0; i < col / N * N; i+=N) {
                    wt[j * col + i + 0] += lr * er[j] * er_scale * in[i + 0]
                                           - l2 * wt[j * col + i + 0];
                    wt[j * col + i + 1] += lr * er[j] * er_scale * in[i + 1]
                                           - l2 * wt[j * col + i + 1];
                    wt[j * col + i + 2] += lr * er[j] * er_scale * in[i + 2]
                                           - l2 * wt[j * col + i + 2];
                    wt[j * col + i + 3] += lr * er[j] * er_scale * in[i + 3]
                                           - l2 * wt[j * col + i + 3];

                    wt[j * col + i + 4] += lr * er[j] * er_scale * in[i + 4]
                                           - l2 * wt[j * col + i + 4];
                    wt[j * col + i + 5] += lr * er[j] * er_scale * in[i + 5]
                                           - l2 * wt[j * col + i + 5];
                    wt[j * col + i + 6] += lr * er[j] * er_scale * in[i + 6]
                                           - l2 * wt[j * col + i + 6];
                    wt[j * col + i + 7] += lr * er[j] * er_scale * in[i + 7]
                                                 - l2 * wt[j * col + i + 7];
                }
                for (; i < col; i++) {
                    wt[j * col + i] += lr * er[j] * er_scale * in[i]
                                           - l2 * wt[j * col + i];
                }
            }
#else
            w = wt;
            for (j = er_s; j < er_e; j++) {
                for (i = 0; i < col; i++) {
                    w[i] += lr * er[j] * er_scale * in[i] - l2 * w[i];
                }
                w += col;
            }
#endif
        }
    }

    if (param_updater->param.mini_batch > 0
        && param_updater->num_step % param_updater->param.mini_batch == 0) {
        // update mini-batch
        param_updater_minibatch(param_updater);
    }

    if (param_updater->param.sync_size > 0
        && param_updater->num_step % param_updater->param.sync_size == 0) {
        // sync weight
        param_updater_sync(param_updater);
    }
}

#ifdef _MINI_UPDATE_
void param_acc_wt_minibatch(int batch, real_t *wt, real_t *er, int er_size,
        real_t *in, int in_size)
{
    if (!(er_size > 0 && in_size > 0 && in != NULL)) {
        ST_WARNING("Do not support this type of update_minibatch.");
        return;
    }

    cblas_gemm(CblasRowMajor, CblasTrans, CblasNoTrans,
            er_size, in_size, batch,
            1.0, er, er_size, in, in_size,
            1.0, wt, in_size);
}

void param_update_minibatch(param_updater_t *param_updater, bool update_arg,
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
