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

#ifdef _CONNLM_TRACE_PROCEDURE_
static const char *wt_update_type_str[] = {
    "Full",
    "Part",
    "One-shot",
};

static const char* wutype2str(wt_update_type_t u)
{
    return wt_update_type_str[u];
}

#if 0
static wt_update_type_t str2wutype(const char *str)
{
    int i;

    for (i = 0; i < sizeof(wt_update_type_str) / sizeof(wt_update_type_str[0]); i++) {
        if (strcasecmp(wt_update_type_str[i], str) == 0) {
            return (wt_update_type_t)i;
        }
    }

    return WT_UT_UNKNOWN;
}
#endif
#endif

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

    if (bias->size > 0 && wt->num_rows != bias->size) {
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
    mat_t *wt;
    mat_t *delta_wt;
    vec_t *bias;
    vec_t *delta_bias;

    size_t a, i, j;
    int b, batch_size;
    real_t sum;

    real_t lr, l2;
    real_t momentum;

    ST_CHECK_PARAM(wt_updater == NULL, -1);

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Update weight[%s].", wutype2str(wt_updater->type));
#endif

    wt = &wt_updater->wt;
    delta_wt = &wt_updater->delta_wt;
    bias = &wt_updater->bias;
    delta_bias = &wt_updater->delta_bias;

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
            if (er->num_cols != wt->num_rows) {
                ST_WARNING("Error size of er mat.[%zux%zu]",
                        er->num_rows, er->num_cols);
                return -1;
            }

            if (momentum != 0.0) {
                if (add_mat_mat(lr, er, MT_Trans, in, MT_NoTrans,
                            momentum, delta_wt) < 0) {
                    ST_WARNING("Failed to add_mat_mat for in and er");
                    return -1;
                }

                if (mat_add_elems(delta_wt, 1.0, wt, -l2, delta_wt) < 0) {
                    ST_WARNING("Failed to mat_add_elems for delta_wt.");
                    return -1;
                }

                if (mat_add_elems(wt, 1.0, delta_wt, 1.0, wt) < 0) {
                    ST_WARNING("Failed to mat_add_elems for wt.");
                    return -1;
                }

                if (bias->size > 0) {
                    for (i = 0; i < er->num_cols; i++) {
                        sum = 0.0;
                        for (b = 0; b < batch_size; b++) {
                            sum += MAT_VAL(er, b, i);
                        }
                        VEC_VAL(delta_bias, i) = lr * sum - l2 * VEC_VAL(bias, i)
                            + momentum * VEC_VAL(delta_bias, i);
                        VEC_VAL(bias, i) += VEC_VAL(delta_bias, i);
                    }
                }
            } else {
                if (add_mat_mat(lr, er, MT_Trans, in, MT_NoTrans,
                            1.0 - l2, wt) < 0) {
                    ST_WARNING("Failed to add_mat_mat for in and er");
                    return -1;
                }
                if (bias->size > 0) {
                    for (i = 0; i < er->num_cols; i++) {
                        sum = 0.0;
                        for (b = 0; b < batch_size; b++) {
                            sum += MAT_VAL(er, b, i);
                        }
                        VEC_VAL(bias, i) += lr * sum - l2 * VEC_VAL(bias, i);
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
                if (part->s + part->n > wt->num_cols) {
                    for (j = 0, i = part->s; i < wt->num_cols; j++, i++) {
                        MAT_VAL(delta_wt, 0, i) = lr * MAT_VAL(er, 0, j)
                            - l2 * MAT_VAL(wt, 0, i)
                            + momentum * MAT_VAL(delta_wt, 0, i);
                        MAT_VAL(wt, 0, i) += MAT_VAL(delta_wt, 0, i);
                    }
                    for (i = 0; j < part->n; j++, i++) {
                        MAT_VAL(delta_wt, 0, i) = lr * MAT_VAL(er, 0, j)
                            - l2 * MAT_VAL(wt, 0, i)
                            + momentum * MAT_VAL(delta_wt, 0, i);
                        MAT_VAL(wt, 0, i) += MAT_VAL(delta_wt, 0, i);
                    }
                } else {
                    for (j = 0, i = part->s; j < part->n; j++, i++) {
                        MAT_VAL(delta_wt, 0, i) = lr * MAT_VAL(er, 0, j)
                            - l2 * MAT_VAL(wt, 0, i)
                            + momentum * MAT_VAL(delta_wt, 0, i);
                        MAT_VAL(wt, 0, i) += MAT_VAL(delta_wt, 0, i);
                    }
                }
            } else {
                if (part->s + part->n > wt->num_cols) {
                    for (j = 0, i = part->s; i < wt->num_cols; j++, i++) {
                        MAT_VAL(wt, 0, i) += lr * MAT_VAL(er, 0, j) - l2 * MAT_VAL(wt, 0, i);
                    }
                    for (i = 0; j < part->n; j++, i++) {
                        MAT_VAL(wt, 0, i) += lr * MAT_VAL(er, 0, j) - l2 * MAT_VAL(wt, 0, i);
                    }
                } else {
                    for (j = 0, i = part->s; j < part->n; j++, i++) {
                        MAT_VAL(wt, 0, i) += lr * MAT_VAL(er, 0, j) - l2 * MAT_VAL(wt, 0, i);
                    }
                }
            }
            break;

        case WT_UT_ONE_SHOT:
            if (er->num_cols != wt->num_cols) {
                ST_WARNING("Error size of er mat[%zux%zu], wt[%zux%zu]",
                        er->num_rows, er->num_cols,
                        wt->num_rows, wt->num_cols);
                return -1;
            }
            if (sp_mat->fmt != SP_MAT_COO) {
                ST_WARNING("Error format of sp_mat.[%d]", sp_mat->fmt);
                return -1;
            }
            if (momentum != 0.0) {
                for (a = 0; a < sp_mat->size; a++) {
                    i = sp_mat->coo.cols[a]; // sp_mat->coo.cols[a] is word_id
                    for (j = 0; j < wt->num_cols; j++) {
                        MAT_VAL(delta_wt, i, j) = lr * sp_mat->vals[a]
                            * MAT_VAL(er, sp_mat->coo.rows[a], j)
                            - l2 * MAT_VAL(wt, i, j)
                            + momentum * MAT_VAL(delta_wt, i, j);
                        MAT_VAL(wt, i, j) += MAT_VAL(delta_wt, i, j);
                    }
                }
            } else {
                for (a = 0; a < sp_mat->size; a++) {
                    i = sp_mat->coo.cols[a]; // sp_mat->coo.cols[a] is word_id
                    for (j = 0; j < wt->num_cols; j++) {
                        MAT_VAL(wt, i, j) += lr * sp_mat->vals[a]
                            * MAT_VAL(er, sp_mat->coo.rows[a], j)
                            - l2 * MAT_VAL(wt, i, j);
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
