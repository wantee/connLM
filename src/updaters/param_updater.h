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

#ifndef  _CONNLM_PARAM_UPDATER_H_
#define  _CONNLM_PARAM_UPDATER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stutils/st_int.h>

#include <connlm/config.h>

#include "param.h"

/** @defgroup g_updater_param Updater for param.
 * @ingroup g_updater
 * Data structures and functions for param updater.
 */

/**
 * Type of weight.
 * @ingroup g_updater_param
 */
typedef enum _weight_update_type_t_ {
    WT_UT_UNKNOWN = -1, /**< Unknown weight. */
    WT_UT_FULL = 0, /**< fully updated weight. */
    WT_UT_PART, /**< partly updated weight. e.g. hashed wt. */
    WT_UT_SEG, /**< segmently updated weight. e.g. output wt. */
    WT_UT_ONE_SHOT, /**< one-shot updated weight. e.g. embedding wt. */
} wt_update_type_t;

/**
 * Output updater.
 * @ingroup g_updater_param
 */
typedef struct _param_updater_t_ {
    param_t param; /**< the param. */

    real_t *shared_wt; /**< shared weight maxtrix for all updaters. */
    real_t *wt; /**< local weight maxtrix of this updater. */
    real_t *delta_wt; /**< delta weight maxtrix for mini-batch. */
    int row; /**< row of weight maxtrix. */
    int col; /**< col of weight maxtrix. */
    wt_update_type_t type; /**< updating type. */

    count_t num_step; /**< update steps. */
} param_updater_t;

/**
 * Destroy a param_updater and set the pointer to NULL.
 * @ingroup g_updater_param
 * @param[in] ptr pointer to updater_t.
 */
#define safe_param_updater_destroy(ptr) do {\
    if((ptr) != NULL) {\
        param_updater_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a param_updater.
 * @ingroup g_updater_param
 * @param[in] param_updater param_updater to be destroyed.
 */
void param_updater_destroy(param_updater_t *param_updater);

/**
 * Create a param_updater.
 *
 * row > 0 && col > 0: wt is [ row x col ];
 * row > 0 && col < 0: wt is hash based 1d vector [ row ];
 *
 * @ingroup g_updater_param
 * @param[in] param the param.
 * @param[in] wt the weight maxtrix.
 * @param[in] row row of weight maxtrix.
 * @param[in] col col of weight maxtrix.
 * @param[in] type updating type of weight maxtrix.
 * @return param_updater on success, otherwise NULL.
 */
param_updater_t* param_updater_create(param_t *param,
        real_t *wt, int row, int col, wt_update_type_t type);

/**
 * Clear parameter argument
 * @ingroup g_param
 * @param[in] param_updater the param_updater.
 */
void param_updater_clear(param_updater_t *param_updater);

/**
 * Accumulate weights.
 * @ingroup g_param
 *
 * in is [ in_size x 1];
 *
 * er_size > 0 && in_size > 0: er is [ 1 x er_size ]; wt is [ er_size x in_size ]; if in == NULL: in is one-shot vector
 *
 * er_size > 0 && in_size < 0: er is [ 1 x er_size ]; wt is hash based 1d vector; in is one-shot vector
 *
 * er_size < 0 && in_size > 0: er is delta-weight matrix [ er_size x in_size ]; wt is [ er_size x in_size ];
 *
 * er_size < 0 && in_size < 0: er is delta-weight matrix [ er_size x in_size ]; wt is [ er_size x in_size ]; in is one-shot vector
 *
 * @return non-zero value if any error.
 * @see param_update
 */
void param_acc_wt(real_t *wt, real_t *er, int er_size, real_t *in,
        int in_size);

/**
 * Update weights.
 * @ingroup g_param
 *
 * For WT_UT_FULL: in is [ col x 1 ]; er is [ 1 x row ];
 * For WT_UT_PART: in is [ col x 1 ]; er is [ 1 x (segs[0].e - sges[0].s) ];
 * For WT_UT_SEG: in is [ col x 1 ]; er is [ 1 x (segs[seg_idx].e - sges[seg_idx].s) ];
 * For WT_UT_ONE_SHOT: in is NULL; er is [ 1 x row ]; updating cols in in_idx of wt;
 *
 * @param[in] param_updater the param_updater.
 * @param[in] er the error vector.
 * @param[in] er_scale scale of error vector.
 * @param[in] er_seg segmen of error vector.
 * @param[in] num_er_seg number of segment of error vector.
 * @param[in] seg_id id of segment.
 * @param[in] in the input vector.
 * @param[in] in_scale scale of input vector.
 * @param[in] in_idx input indexes (with scales) of input one-shot vector.
 * @param[in] num_in_idx number of input indexes of input one-shot vector.
 * @return non-zero value if any error.
 */
#if 0
int param_update(param_updater_t *param_updater,
        real_t *er, real_t er_scale, st_int_seg_t *er_seg, int num_er_seg, int seg_id,
        real_t *in, real_t in_scale,
        st_wt_int_t *in_idx, int num_in_idx);
#endif

#ifdef _MINI_UPDATE_
/**
 * Accumulate weights with mini-batch.
 * @ingroup g_param
 *
 * batch is the mini-batch size,
 * other arguments are the same as param_acc_wt.
 *
 * @return non-zero value if any error.
 * @see param_acc_wt
 */
int param_acc_wt_minibatch(int batch, real_t *wt, real_t *er, int er_size,
        real_t *in, int in_size);

/**
 * Update weight with mini-batch
 * @ingroup g_param
 *
 * batch is the mini-batch size,
 * other arguments are the same as param_update.
 *
 * @return non-zero value if any error.
 * @see param_update
 */
int param_update_minibatch(param_updater_t *param_updater, bool update_arg,
        int batch, real_t *wt, real_t *er, real_t er_scale,
        int er_size, real_t *in, int in_size);
#endif

#ifdef __cplusplus
}
#endif

#endif
