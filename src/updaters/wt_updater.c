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

    if (wt_updater->param.momentum * wt_updater->param.momentum_coef != 0.0) {
        if (mat_resize(&wt_updater->delta_wt,
                    wt->num_rows, wt->num_cols, 0.0) < 0) {
            ST_WARNING("Failed to mat_resize delta_wt.");
            goto ERR;
        }
    }

    if (wt_updater->param.momentum * wt_updater->param.bias_momentum_coef != 0.0) {
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

static int mat_regularize_l1_l2(mat_t *mat, real_t l1, real_t l2)
{
    real_t *vals;
    real_t l1_term;
    size_t i, j;

    if (l1 == 0.0 && l2 == 0.0) {
        return 0;
    } else if (l1 == 0.0) {
        vals = mat->vals;
        for (i = 0; i < mat->num_rows; i++) {
            for (j = 0; j < mat->num_cols; j++) {
                vals[j] = vals[j] - l2 * vals[j];
            }
            vals += mat->stride;
        }
    } else if (l2 == 0.0) {
        vals = mat->vals;
        for (i = 0; i < mat->num_rows; i++) {
            for (j = 0; j < mat->num_cols; j++) {
                if (vals[j] > 0.0) {
                    l1_term = -l1;
                } else if (vals[j] < 0.0) {
                    l1_term = l1;
                } else {
                    l1_term = 0.0;
                }

                vals[j] = vals[j] + l1_term;
            }
            vals += mat->stride;
        }
    } else {
        vals = mat->vals;
        for (i = 0; i < mat->num_rows; i++) {
            for (j = 0; j < mat->num_cols; j++) {
                if (vals[j] > 0.0) {
                    l1_term = -l1;
                } else if (vals[j] < 0.0) {
                    l1_term = l1;
                } else {
                    l1_term = 0.0;
                }

                vals[j] = vals[j] - l2 * vals[j] + l1_term;
            }
            vals += mat->stride;
        }
    }

    return 0;
}

static int update_part(size_t num_rows, size_t num_cols,
        mat_t *wt, size_t wt_row, size_t wt_col,
        mat_t *er, size_t er_row, size_t er_col,
        real_t lr, real_t l1, real_t l2,
        real_t mmt, mat_t *delta_wt)
{
    mat_t sub_wt = {0};
    mat_t sub_delta_wt = {0};
    mat_t sub_er = {0};

    if (mmt != 0.0) {
        if (mat_submat(wt, wt_row, num_rows, wt_col, num_cols, &sub_wt) < 0) {
            ST_WARNING("Failed to mat_submat wt.");
            return -1;
        }

        if (mat_regularize_l1_l2(&sub_wt, l1, l2) < 0) {
            ST_WARNING("Failed to mat_regularize_l1_l2 sub_wt.");
            return -1;
        }

        if (mat_submat(er, er_row, num_rows, er_col, num_cols, &sub_er) < 0) {
            ST_WARNING("Failed to mat_submat er.");
            return -1;
        }

        if (mat_submat(delta_wt, wt_row, num_rows, wt_col, num_cols, &sub_delta_wt) < 0) {
            ST_WARNING("Failed to mat_submat delta_wt.");
            return -1;
        }
        if (mat_add_elems(&sub_delta_wt, mmt, &sub_er, lr, &sub_delta_wt) < 0) {
            ST_WARNING("Failed to mat_add_elems sub_er to sub_delta_wt.");
            return -1;
        }

        if (mat_add_elems(&sub_delta_wt, 1.0, &sub_wt, 1.0, &sub_wt) < 0) {
            ST_WARNING("Failed to mat_add_elems sub_delta_wt to sub_wt.");
            return -1;
        }
    } else {
        if (mat_submat(wt, wt_row, num_rows, wt_col, num_cols, &sub_wt) < 0) {
            ST_WARNING("Failed to mat_submat wt.");
            return -1;
        }

        if (mat_regularize_l1_l2(&sub_wt, l1, l2) < 0) {
            ST_WARNING("Failed to mat_regularize_l1_l2 sub_wt.");
            return -1;
        }

        if (mat_submat(er, er_row, num_rows, er_col, num_cols, &sub_er) < 0) {
            ST_WARNING("Failed to mat_submat er.");
            return -1;
        }

        if (mat_add_elems(&sub_wt, 1.0, &sub_er, lr, &sub_wt) < 0) {
            ST_WARNING("Failed to mat_add_elems sub_wt vs sub_er.");
            return -1;
        }
    }

    return 0;
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

    size_t a;
    int batch_size;

    real_t lr, lr_bias, l1, l2;
    real_t mmt, mmt_bias;

    ST_CHECK_PARAM(wt_updater == NULL, -1);

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Update weight[%s].", wutype2str(wt_updater->type));
#endif

    wt = &wt_updater->wt;
    delta_wt = &wt_updater->delta_wt;
    bias = &wt_updater->bias;
    delta_bias = &wt_updater->delta_bias;

    batch_size = er->num_rows;

    lr = get_lr(&(wt_updater->param));
    lr *= er_scale * in_scale;
    lr_bias = lr * wt_updater->param.bias_learn_rate_coef;
    lr = lr * wt_updater->param.learn_rate_coef;

    l1 = lr * wt_updater->param.l1_penalty;
    l2 = lr * wt_updater->param.l2_penalty;

    mmt = wt_updater->param.momentum * wt_updater->param.momentum_coef;
    mmt_bias = wt_updater->param.momentum * wt_updater->param.bias_momentum_coef;

    switch (wt_updater->type) {
        case WT_UT_FULL:
            if (er->num_cols != wt->num_rows) {
                ST_WARNING("Error size of er mat.[%zux%zu]",
                        er->num_rows, er->num_cols);
                return -1;
            }

            l1 *= batch_size;
            l2 *= batch_size;

            if (mmt != 0.0) {
                if (add_mat_mat(lr, er, MT_Trans, in, MT_NoTrans,
                            mmt, delta_wt) < 0) {
                    ST_WARNING("Failed to add_mat_mat for in and er");
                    return -1;
                }

                if (mat_regularize_l1_l2(wt, l1, l2) < 0) {
                    ST_WARNING("Failed to mat_regularize_l1_l2 wt.");
                    return -1;
                }

                if (mat_add_elems(wt, 1.0, delta_wt, 1.0, wt) < 0) {
                    ST_WARNING("Failed to mat_add_elems for wt.");
                    return -1;
                }
            } else {
                if (mat_regularize_l1_l2(wt, l1, l2) < 0) {
                    ST_WARNING("Failed to mat_regularize_l1_l2 wt.");
                    return -1;
                }

                if (add_mat_mat(lr, er, MT_Trans, in, MT_NoTrans,
                            1.0, wt) < 0) {
                    ST_WARNING("Failed to add_mat_mat for in and er");
                    return -1;
                }
            }

            if (bias->size > 0) {
                if (mmt_bias != 0.0) {
                    if (vec_add_col_sum_mat(delta_bias, lr_bias,
                                er, mmt_bias) < 0) {
                        ST_WARNING("Failed to vec_add_col_sum_mat bias");
                        return -1;
                    }

                    if (vec_add_elems(bias, 1.0, delta_bias, 1.0, bias) < 0) {
                        ST_WARNING("Failed to vec_add_elems bias");
                        return -1;
                    }
                } else {
                    if (vec_add_col_sum_mat(bias, lr_bias, er, 1.0) < 0) {
                        ST_WARNING("Failed to vec_add_col_sum_mat bias");
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

            if (part->s + part->n > wt->num_cols) {
                a = wt->num_cols - part->s;
                if (update_part(1, a, wt, 0, part->s, er, 0, 0, lr, l1, l2,
                            mmt, delta_wt) < 0) {
                    ST_WARNING("Failed to update_part first part");
                    return -1;
                }
                if (update_part(1, part->n - a, wt, 0, 0,
                            er, 0, a, lr, l1, l2, mmt, delta_wt) < 0) {
                    ST_WARNING("Failed to update_part second part");
                    return -1;
                }
            } else {
                if (update_part(1, part->n, wt, 0, part->s, er, 0, 0,
                            lr, l1, l2, mmt, delta_wt) < 0) {
                    ST_WARNING("Failed to update_part whole part");
                    return -1;
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
            for (a = 0; a < sp_mat->size; a++) {
                // sp_mat->coo.cols[a] is word_id
                // sp_mat->coo.rows[a] is batch_id
                if (update_part(1, wt->num_cols,
                            wt, sp_mat->coo.cols[a], 0,
                            er, sp_mat->coo.rows[a], 0,
                            lr * sp_mat->vals[a], l1, l2,
                            mmt, delta_wt) < 0) {
                    ST_WARNING("Failed to update_part one-shot");
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
