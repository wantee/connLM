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

#include <connlm/config.h>

#include "param.h"

/** @defgroup g_updater_param Updater for param.
 * @ingroup g_updater
 * Data structures and functions for param updater.
 */

/**
 * Output updater.
 * @ingroup g_updater_param
 */
typedef struct _param_updater_t_ {
    param_t param; /**< the param. */

    int l2_step; /**< step for L2 penalty */
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
 * @ingroup g_updater_param
 * @param[in] connlm the connlm model.
 * @return param_updater on success, otherwise NULL.
 */
param_updater_t* param_updater_create(param_t *param);

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
 * @see param_update
 */
void param_acc_wt(real_t *wt, real_t *er, int er_size, real_t *in,
        int in_size);

/**
 * Update weight using parameters.
 * @ingroup g_param
 *
 * in is [ in_size x 1 ];
 *
 *
 * er_size > 0 && in_size > 0: er is [ 1 x er_size ]; wt is [ er_size x in_size ]; if in == NULL: in is one-shot vector
 *
 * er_size > 0 && in_size < 0: er is [ 1 x er_size ]; wt is hash based 1d vector; in is one-shot vector
 *
 * er_size < 0 && in_size > 0: er is delta-weight matrix [ er_size x in_size ]; wt is [ er_size x in_size ];
 *
 * er_size < 0 && in_size < 0: er is delta-weight matrix [ er_size x in_size ]; wt is [ er_size x in_size ]; in is one-shot vector
 *
 * @see param_acc_wt
 */
void param_update(param_updater_t *param_updater, bool update_arg,
        real_t *wt, real_t *er, real_t er_scale,
        int er_size, real_t *in, int in_size);

#ifdef _MINI_UPDATE_
/**
 * Accumulate weights with mini-batch.
 * @ingroup g_param
 *
 * batch is the mini-batch size,
 * other arguments are the same as param_acc_wt.
 *
 * @see param_acc_wt
 */
void param_acc_wt_minibatch(int batch, real_t *wt, real_t *er, int er_size,
        real_t *in, int in_size);

/**
 * Update weight with mini-batch
 * @ingroup g_param
 *
 * batch is the mini-batch size,
 * other arguments are the same as param_update.
 *
 * @see param_update
 */
void param_update_minibatch(param_updater_t *param_updater, bool update_arg,
        int batch, real_t *wt, real_t *er, real_t er_scale,
        int er_size, real_t *in, int in_size);
#endif

#ifdef __cplusplus
}
#endif

#endif
