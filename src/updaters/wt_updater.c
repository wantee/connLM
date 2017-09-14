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

    mat_destroy(&wt_updater->wt);
    mat_destroy(&wt_updater->delta_wt);
    vec_destroy(&wt_updater->bias);
    vec_destroy(&wt_updater->delta_bias);
}

wt_updater_t* wt_updater_create(param_t *param, mat_t *wt, vec_t *bias,
        wt_update_type_t type)
{
    wt_updater_t *wt_updater = NULL;

    ST_CHECK_PARAM(param == NULL || wt == NULL || bias == NULL, NULL);

    if (wt->num_cols != bias->size) {
        ST_WARNING("weight matrix and bias not match");
        return NULL;
    }

    wt_updater = (wt_updater_t *)st_malloc(sizeof(wt_updater_t));
    if (wt_updater == NULL) {
        ST_WARNING("Failed to st_malloc wt_updater.");
        goto ERR;
    }
    memset(wt_updater, 0, sizeof(wt_updater_t));

    wt_updater->param = *param;
    mat_assign(&wt_updater->wt, wt);
    vec_assign(&wt_updater->bias, bias);
    wt_updater->type = type;

    if (wt_updater->param.momentum != 0.0) {
        if (mat_resize(&wt_updater->delta_wt,
                    wt->num_rows, wt->num_cols, 0.0) < 0) {
            ST_WARNING("Failed to mat_resize delta_wt.");
            goto ERR;
        }

        if (wt_updater->bias.size > 0) {
            if (vec_resize(&wt_updater->delta_bias, bias->size, 0.0) < 0) {
                ST_WARNING("Failed to mat_resize delta_bias.");
                goto ERR;
            }
        }
    }

    return wt_updater;

ERR:
    safe_wt_updater_destroy(wt_updater);
    return NULL;
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
        mat_t *er, real_t er_scale,
        mat_t *in, real_t in_scale,
        st_size_seg_t* part, sp_mat_t *sp_mat)
{
    real_t *wt;
    real_t *delta_wt;
    real_t *bias;
    real_t *delta_bias;

    st_int_seg_t *seg;
    mat_t *buf_er, *buf_in;
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
    /* TODO: should we divide batch_size for WT_UT_ONE_SHOT
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
