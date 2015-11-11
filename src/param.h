/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Wang Jian
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

#ifndef  _PARAM_H_
#define  _PARAM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <st_opt.h>

#include "config.h"

/** @defgroup g_param Parameter
 * Data structures and functions for training Parameter.
 */

/**
 * Parameter.
 * @ingroup g_param
 */
typedef struct _param_t_ {
    real_t learn_rate; /**< value of learning rate */
    real_t l1_penalty; /**< value of L1 penalty */
    real_t l2_penalty; /**< value of L2 penalty */
    int l2_gap; /**< applying L2 penalty every l2_gap words */
    real_t momentum; /**< value of momentum */
    int mini_batch; /**< size of mini-batch */
} param_t;

/**
 * Arguments updated for one parameter.
 * @ingroup g_param
 */
typedef struct _param_arg_t_ {
    int l2_step; /**< step for L2 penalty */
} param_arg_t;

/**
 * Clear parameter argument
 * @ingroup g_param
 * @param[in] arg argument to be cleared.
 */
void param_arg_clear(param_arg_t *arg);

/**
 * Load param option.
 * @ingroup g_param
 * @param[out] param loaded.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @param[in] parent_param default value of parameters.
 * @return non-zero value if any error.
 */
int param_load(param_t *param, st_opt_t *opt, const char *sec_name,
        param_t *parent_param);

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
void param_update(param_t *param, param_arg_t *arg, bool update_arg,
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
void param_update_minibatch(param_t *param, param_arg_t *arg,
        bool update_arg, int batch, real_t *wt, real_t *er, real_t er_scale,
        int er_size, real_t *in, int in_size);
#endif

#ifdef __cplusplus
}
#endif

#endif
