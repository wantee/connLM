/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Wang Jian
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
#include "vector.h"
#include "matrix.h"
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
    WT_UT_ONE_SHOT, /**< one-shot updated weight. e.g. embedding wt. */
} wt_update_type_t;

/**
 * Output updater.
 * @ingroup g_updater_wt
 */
typedef struct _weight_updater_t_ {
    param_t param; /**< the param. */

    mat_t wt; /**< local weight matrix of this updater. */
    mat_t delta_wt; /**< buffer for delta weight. used by momentum. */
    vec_t bias; /**< local bias of this updater. */
    vec_t delta_bias; /**< buffer for delta bias. used by momentum. */
    wt_update_type_t type; /**< updating type. */
} wt_updater_t;

/**
 * Destroy a wt_updater and set the pointer to NULL.
 * @ingroup g_updater_wt
 * @param[in] ptr pointer to updater_t.
 */
#define safe_wt_updater_destroy(ptr) do {\
    if((ptr) != NULL) {\
        wt_updater_destroy(ptr);\
        safe_st_free(ptr);\
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
 * @ingroup g_updater_wt
 * @param[in] param the param.
 * @param[in] wt the weight maxtrix.
 * @param[in] bias the bias vector.
 * @param[in] type updating type of weight maxtrix.
 * @return wt_updater on success, otherwise NULL.
 */
wt_updater_t* wt_updater_create(param_t *param, mat_t *wt, vec_t *bias,
        wt_update_type_t type);

/**
 * Update weights.
 * @ingroup g_updater_wt
 *
 * Denote Batch size by B
 * For WT_UT_FULL: in is [ B x col ]; er is [ B x row ];
 * For WT_UT_PART: in is NULL; er is [ 1 x part.n ];
 * For WT_UT_ONE_SHOT: in is NULL; er is [ B x row ];
 *
 * Note that for WT_UT_PART, we don't pass a Batch into this function,
 * since there is no MatXMat operation for this type of weight, and
 * the size of er is very different among the egs.
 *
 * @param[in] wt_updater the wt_updater.
 * @param[in] er the error vector.
 * @param[in] er_scale scale of error vector.
 * @param[in] in the input vector.
 * @param[in] in_scale scale of input vector.
 *
 * @param[in] part    for WT_UT_PART, specify the segment of row in weight.
 * @param[in] sp_mat  sparse matrix with num_rows == B. An egs is in a row.
 *                    for WT_UT_ONE_SHOT, every col index represents an id
 *                      of input and the value represents scale.
 * @return non-zero value if any error.
 */
int wt_update(wt_updater_t *wt_updater,
        mat_t *er, real_t er_scale,
        mat_t *in, real_t in_scale,
        st_size_seg_t* part, sp_mat_t *sp_mat);

#ifdef __cplusplus
}
#endif

#endif
