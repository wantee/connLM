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

#ifndef  _CONNLM_WEIGHT_UPDATER_H_
#define  _CONNLM_WEIGHT_UPDATER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stutils/st_int.h>

#include <connlm/config.h>

#include "utils.h"
#include "param.h"

/** @defgroup g_updater_wt Updater for Weight
 * @ingroup g_updater
 * Update values of a weight.
 */

/**
 * Type of weight.
 * @ingroup g_updater_wt
 */
typedef enum _weight_update_type_t_ {
    WT_UT_UNKNOWN = -1, /**< Unknown weight. */
    WT_UT_FULL = 0, /**< fully updated weight. */
    WT_UT_PART, /**< partly updated weight. e.g. hashed wt. */
    WT_UT_SEG, /**< segmently(non-overlap) updated weight. e.g. output wt. */
    WT_UT_ONE_SHOT, /**< one-shot updated weight. e.g. embedding wt. */
} wt_update_type_t;

/**
 * Store the dirty part of weight.
 * @ingroup g_updater_wt
 */
typedef struct _weight_dirty_buffer_t_ {
    // WT_UT_PART
    st_int_seg_t *segs; /**< buffer for segements. */
    int cap_seg; /**< capacity of segements buffer. */
    int n_seg; /**< size of segements buffer. */

    // WT_UT_ONE_SHOT or WT_UT_SEG
    int *ids; /**< buffer of ids. */
    int cap_id; /**< capacity of ids. */
    int n_id; /**< size of ids. */

    concat_mat_t *buf_in; /**< buffer for in_ac. sized by n_id and cap_id. */
    real_t in_scale; /**< scale of in_ac. */
    concat_mat_t *buf_er; /**< buffer for out_er. sized by n_id and cap_id. */
    real_t er_scale; /**< scale of out_er. */
    int n_buf;
} wt_dirty_buf_t;

/**
 * Output updater.
 * @ingroup g_updater_wt
 */
typedef struct _weight_updater_t_ {
    param_t param; /**< the param. */

    real_t *shared_wt; /**< shared weight maxtrix for all updaters. */
    real_t *wt; /**< local weight maxtrix of this updater. */
    real_t *ori_wt; /**< origin weight maxtrix for sync. */
    real_t *shadow_wt; /**< shadow weight maxtrix for mini-batch. */
    int row; /**< row of weight maxtrix. */
    int col; /**< col of weight maxtrix. */
    wt_update_type_t type; /**< updating type. */

    st_int_seg_t *segs; /**< segs for type == WT_UT_SEG. */
    int n_seg; /**< number of segs. */

    count_t n_step; /**< updating step. */
    count_t n_flush_step; /**< flushing step. */

    wt_dirty_buf_t mini_dirty; /**< dirty buffer for mini-batch. */
    wt_dirty_buf_t sync_dirty; /**< dirty buffer for sync. */

    real_t *delta_wt; /**< buffer for delta weight. */
} wt_updater_t;

/**
 * Destroy a wt_updater and set the pointer to NULL.
 * @ingroup g_updater_wt
 * @param[in] ptr pointer to updater_t.
 */
#define safe_wt_updater_destroy(ptr) do {\
    if((ptr) != NULL) {\
        wt_updater_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a wt_updater.
 * @ingroup g_updater_wt
 * @param[in] wt_updater wt_updater to be destroyed.
 */
void wt_updater_destroy(wt_updater_t *wt_updater);

/**
 * Create a wt_updater.
 *
 * row > 0 && col > 0: wt is [ row x col ];
 * row > 0 && col < 0: wt is hash based 1d vector [ row ];
 *
 * @ingroup g_updater_wt
 * @param[in] param the param.
 * @param[in] wt the weight maxtrix.
 * @param[in] row row of weight maxtrix.
 * @param[in] col col of weight maxtrix.
 * @param[in] type updating type of weight maxtrix.
 * @return wt_updater on success, otherwise NULL.
 */
wt_updater_t* wt_updater_create(param_t *param,
        real_t *wt, int row, int col, wt_update_type_t type);

/**
 * (Re)init a wt_updater.
 *
 * param, shared_wt, row, col and type must be already setted before.
 *
 * @ingroup g_updater_wt
 * @param[in] wt_updater the wt_updater.
 * @return non-zero value if any error.
 */
int wt_updater_init(wt_updater_t *wt_updater);

/**
 * Set segs for wt_updater. The segs is used by WT_UT_SEG.
 *
 * @ingroup g_updater_wt
 * @param[in] wt_updater the wt_updater.
 * @param[in] segs the segs.
 * @param[in] n_seg number of segs.
 * @return non-zero value if any error.
 */
int wt_updater_set_segs(wt_updater_t *wt_updater,
        st_int_seg_t *segs, int n_seg);

/**
 * Clear wt_updater.
 * @ingroup g_updater_wt
 * @param[in] wt_updater the wt_updater.
 */
void wt_updater_clear(wt_updater_t *wt_updater);

/**
 * Update weights.
 * @ingroup g_updater_wt
 *
 * For WT_UT_FULL: in is [ col x 1 ]; er is [ 1 x row ];
 * For WT_UT_PART: in is [ col x 1 ]; er is [ 1 x row_seg.n ];
 * For WT_UT_SEG: in is [ col x 1 ]; er is [ 1 x updater->segs[row_seg_id].n ];
 * For WT_UT_ONE_SHOT: in is NULL; er is [ 1 x row ]; updating cols in in_idx of wt;
 *
 * @param[in] wt_updater the wt_updater.
 * @param[in] row_seg segment of wt row corresponding to error vector.
 * @param[in] row_seg_id id of segment of wt row, used by WT_UT_SEG.
 * @param[in] er the error vector.
 * @param[in] er_scale scale of error vector.
 * @param[in] in the input vector.
 * @param[in] in_scale scale of input vector.
 * @param[in] in_idx input index (with scale) of input one-shot vector.
 * @return non-zero value if any error.
 */
int wt_update(wt_updater_t *wt_updater,
        st_int_seg_t* row_seg, int row_seg_id,
        real_t *er, real_t er_scale,
        real_t *in, real_t in_scale, st_wt_int_t *in_idx);

/**
 * Flush weights from mini-batch or sync buffer,
 * according to mini-batch size and sync_size.
 * @ingroup g_updater_wt
 *
 * @param[in] wt_updater the wt_updater.
 * @param[in] force force flushing, i.e. ignoring mini-batch size and sync size.
 * @return non-zero value if any error.
 */
int wt_flush(wt_updater_t *wt_updater, bool force);

#ifdef __cplusplus
}
#endif

#endif
