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

void wt_updater_destroy(wt_updater_t *wt_updater)
{
    if (wt_updater == NULL) {
        return;
    }

    wt_updater->wt = NULL;
    safe_st_aligned_free(wt_updater->delta_wt);
    wt_updater->bias = NULL;
    safe_st_aligned_free(wt_updater->delta_bias);
    wt_updater->row = 0;
    wt_updater->col = 0;

    safe_st_free(wt_updater->segs);
    wt_updater->n_seg = 0;
}

wt_updater_t* wt_updater_create(param_t *param, real_t *wt, real_t *bias,
        size_t row, size_t col, wt_update_type_t type)
{
    size_t sz;
    size_t bias_sz;

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
    wt_updater->wt = wt;
    wt_updater->bias = bias;
    wt_updater->type = type;

    if (wt_updater->param.momentum != 0.0) {
        sz =  wt_updater->row * wt_updater->col * sizeof(real_t);
        bias_sz = wt_updater->row * sizeof(real_t);

        safe_st_aligned_free(wt_updater->delta_wt);
        wt_updater->delta_wt = st_aligned_malloc(sz, ALIGN_SIZE);
        if (wt_updater->delta_wt == NULL) {
            ST_WARNING("Failed to st_aligned_malloc delta_wt.");
            goto ERR;
        }
        memset(wt_updater->delta_wt, 0, sz);

        if (wt_updater->bias != NULL) {
            safe_st_aligned_free(wt_updater->delta_bias);
            wt_updater->delta_bias = st_aligned_malloc(bias_sz, ALIGN_SIZE);
            if (wt_updater->delta_bias == NULL) {
                ST_WARNING("Failed to st_aligned_malloc delta_bias.");
                goto ERR;
            }
            memset(wt_updater->delta_bias, 0, bias_sz);
        }
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

    return 0;

ERR:
    safe_st_free(wt_updater->segs);
    return -1;
}

static inline real_t get_lr(param_t *param)
{
    if (param->momentum != 0.0) {
        return param->learn_rate * (1.0 - param->momentum);
    } else {
        return param->learn_rate;
    }
}

int wt_update(wt_updater_t *wt_updater,
        matrix_t *er, real_t er_scale,
        matrix_t *in, real_t in_scale,
        st_size_seg_t* part, sparse_matrix_t *sp_mat)
{
    real_t *wt;
    real_t *delta_wt;
    real_t *bias;
    real_t *delta_bias;

    st_int_seg_t *seg;
    matrix_t *buf_er, *buf_in;
    size_t row, col, sz, i, j;
    int idx;
    real_t sum;

    real_t lr, l2;
    real_t momentum;

    ST_CHECK_PARAM(wt_updater == NULL, -1);

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Update weight");
#endif

    wt = wt_updater->wt;
    delta_wt = wt_updater->delta_wt;
    bias = wt_updater->bias;
    delta_bias = wt_updater->delta_bias;

    row = wt_updater->row;
    col = wt_updater->col;

    lr = get_lr(&(wt_updater->param));
    lr *= er_scale * in_scale;
    l2 = wt_updater->param.l2_penalty;
    momentum = wt_updater->param.momentum;

    batch_size = er->num_rows;
    /* TODO: should we divide batch_size for WT_UT_ONE_SHOT and WT_UT_SEG,
       since they usually update only a subset of rows. */
    lr /= batch_size;

    switch (wt_updater->type) {
        case WT_UT_FULL:
            if (er->num_cols != row) {
                ST_WARNING("Error size of er mat.[%zux%zu]",
                        er->num_rows, er->num_cols);
                return -1;
            }
            sz = row * col;

            if (momentum != 0.0) {
                matXmat(delta_wt, er->vals, in->vals, row, col,
                        batch_size, lr, momentum);
                for (i = 0; i < sz; i++) {
                    delta_wt[i] -= l2 * wt[i];
                    wt[i] += delta_wt[i];
                }
                if (bias != NULL) {
                    for (i = 0; i < row; i++) {
                        sum = 0.0;
                        for (b = 0; b < batch_size; b++) {
                            sum += MAT_VAL(er, b, i);
                        }
                        delta_bias[i] = lr * sum - l2 * bias[i] + momentum * delta_bias[i];
                        bias[i] += delta_bias[i];
                    }
                }
            } else {
                matXmat(wt, er->vals, in->vals, row, col,
                        batch_size, lr, 1.0 - l2);
                if (bias != NULL) {
                    for (i = 0; i < row; i++) {
                        sum = 0.0;
                        for (b = 0; b < batch_size; b++) {
                            sum += MAT_VAL(er, b, i);
                        }
                        bias[i] += lr * sum - l2 * bias[i];
                    }
                }
            }
            break;

        case WT_UT_SEG:
            if (er->num_cols != row) {
                ST_WARNING("Error size of er mat.[%zux%zu]",
                        er->num_rows, er->num_cols);
                return -1;
            }
            if (sp_mat->fmt != SP_MAT_CSC) {
                ST_WARNING("Error format of sp_mat.[%d]", sp_mat->fmt);
                return -1;
            }

            // construct maxtrix for segs
            for (a = 0; a < sp_mat->csc.num_cols; a++) {
                for (b = sp_mat->csc.col_s; b < sp_mat->csc.col_e; b++) {
                    idx = sp_mat->csc.rows[b];
                    seg = wt_updater->segs + idx;

                    buf_er = wt_updater->buf_ers + idx;
                    if (matrix_resize(buf_er, row, seg->n) < 0) {
                        ST_WARNING("Failed to matrix_resize for buf_er.");
                        return -1;
                    }
                    if (matrix_append_row(buf_er,
                                MAT_VALP(er, idx, seg->s), seg->n) < 0) {
                        ST_WARNING("Failed to matrix_append_row for buf_er.");
                        return -1;
                    }

                    buf_in = wt_updater->buf_ins + idx;
                    if (matrix_resize(buf_in, row, seg->n) < 0) {
                        ST_WARNING("Failed to matrix_resize for buf_in.");
                        return -1;
                    }
                    if (matrix_append_row(buf_in,
                                MAT_VALP(in, idx, seg->s), seg->n) < 0) {
                        ST_WARNING("Failed to matrix_append_row for buf_in.");
                        return -1;
                    }
                }
            }

            // do update weight
            for (a = 0; a < sp_mat->csc.num_cols; a++) {
                for (b = sp_mat->csc.col_s; b < sp_mat->csc.col_e; b++) {
                    idx = sp_mat->csc.rows[b];
                    seg = wt_updater->segs + idx;
                    buf_er = wt_updater->buf_ers + idx;
                    buf_in = wt_updater->buf_ins + idx;

                    if (buf_er->num_rows == 0) {
                        continue;
                    }

                    if (momentum != 0.0) {
                        matXmat(delta_wt + seg->s * col,
                                buf_er->vals,
                                buf_in->vals, seg->n, col,
                                buf_er->num_rows, lr, momentum);
                        i = seg->s * col;
                        for (; i < (seg->s + seg->n) * col; i++) {
                            delta_wt[i] -= l2 * wt[i];
                            wt[i] += delta_wt[i];
                        }
                        if (bias != NULL) {
                            for (i = seg->s; i < (seg->s + seg->n); i++) {
                                sum = 0.0;
                                for (j = 0; j < buf_er->num_rows; j++) {
                                    sum += MAT_VAL(buf_er, j, i - seg->s);
                                }
                                delta_bias[i] = lr * sum - l2 * bias[i] + momentum * delta_bias[i];
                                bias[i] += delta_bias[i];
                            }
                        }
                    } else {
                        matXmat(wt + seg->s * col,
                                buf_er->vals,
                                buf_in->vals, seg->n, col,
                                buf_er->num_rows,
                                lr, 1.0 - l2);
                        if (bias != NULL) {
                            for (i = seg->s; i < (seg->s + seg->n); i++) {
                                sum = 0.0;
                                for (j = 0; j < buf_er->num_rows; j++) {
                                    sum += MAT_VAL(buf_er, j, i - seg->s);
                                }
                                bias[i] += lr * sum - l2 * bias[i];
                            }
                        }
                    }

                    if (matrix_clear(buf_er) < 0) {
                        ST_WARNING("Failed to matrix_clear for buf_er.");
                        return -1;
                    }
                    if (matrix_clear(buf_in) < 0) {
                        ST_WARNING("Failed to matrix_clear for buf_in.");
                        return -1;
                    }
                }
            }

            break;

        case WT_UT_PART:
            if (er->num_rows != 1 || er->num_cols != part->n) {
                ST_WARNING("Error size of er mat.[%zux%zu]",
                        er->num_rows, er->num_cols);
                return -1;
            }

            if (momentum != 0.0) {
                if (part->s + part->n > row) {
                    for (j = 0, i = part->s; i < row; j++, i++) {
                        delta_wt[i] = lr * er->vals[j] - l2 * wt[i]
                            + momentum * delta_wt[i];
                        wt[i] += delta_wt[i];
                    }
                    for (i = 0; j < part->n; j++, i++) {
                        delta_wt[i] = lr * er->vals[j] - l2 * wt[i]
                            + momentum * delta_wt[i];
                        wt[i] += delta_wt[i];
                    }
                } else {
                    for (j = 0, i = part->s; j < part->n; j++, i++) {
                        delta_wt[i] = lr * er->vals[j] - l2 * wt[i]
                            + momentum * delta_wt[i];
                        wt[i] += delta_wt[i];
                    }
                }
            } else {
                if (part->s + part->n > row) {
                    for (j = 0, i = part->s; i < row; j++, i++) {
                        wt[i] += lr * er->vals[j] - l2 * wt[i];
                    }
                    for (i = 0; j < part->n; j++, i++) {
                        wt[i] += lr * er->vals[j] - l2 * wt[i];
                    }
                } else {
                    for (j = 0, i = part->s; j < part->n; j++, i++) {
                        wt[i] += lr * er->vals[j] - l2 * wt[i];
                    }
                }
            }
            break;

        case WT_UT_ONE_SHOT:
            if (er->num_cols != row) {
                ST_WARNING("Error size of er mat.[%zux%zu]",
                        er->num_rows, er->num_cols);
                return -1;
            }
            if (sp_mat->fmt != SP_MAT_COO) {
                ST_WARNING("Error format of sp_mat.[%d]", sp_mat->fmt);
                return -1;
            }
            if (momentum != 0.0) {
                for (a = 0; a < sp_mat->size; a++) {
                    i = sp_mat->cols[a] * col; // sp_mat->cols[a] is word_id
                    for (j = 0; j < col; j++, i++) {
                        delta_wt[i] += lr * sp_mat->vals[a]
                            * MAT_VAL(er, sp_mat->rows[a], j) - l2 * wt[i]
                            + momentum * delta_wt[i];
                        wt[i] += delta_wt[i];
                    }
                }
            } else {
                for (a = 0; a < sp_mat->size; a++) {
                    i = sp_mat->cols[a] * col; // sp_mat->cols[a] is word_id
                    for (j = 0; j < col; j++, i++) {
                        wt[i] += lr * sp_mat->vals[a]
                            * MAT_VAL(er, sp_mat->rows[a], j) - l2 * wt[i];
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
