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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <st_macro.h>
#include <st_log.h>

#include "param.h"

static param_t def_param = {
    .learn_rate = 0.1,
    .l1_penalty = 0.0,
    .l2_penalty = 0.0,
    .l2_gap = 1,
    .momentum = 0.0,
};

int param_load(param_t *param, st_opt_t *opt, const char *sec_name,
        param_t *parent_param)
{
    double d;

    ST_CHECK_PARAM(param == NULL || opt == NULL, -1);

    if (parent_param == NULL) {
        *param = def_param;
    } else {
        *param = *parent_param;
    }

    ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "LEARN_RATE", d,
            (double)param->learn_rate,
            "Learning rate");
    param->learn_rate = (real_t)d;

    ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "L1_PENALTY", d, 
            (double)param->l1_penalty,
            "L1 penalty (promote sparsity)");
    param->l1_penalty = (real_t)d;

    ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "L2_PENALTY", d,
            (double)param->l2_penalty,
            "L2 penalty (weight decay)");
    param->l2_penalty = (real_t)d;

    ST_OPT_SEC_GET_INT(opt, sec_name, "L2_GAP", param->l2_gap,
            param->l2_gap,
            "Number of words between two consecutive L2 regularization");

    ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "MOMENTUM", d,
            (double)param->momentum,
            "Momentum");
    param->momentum = (real_t)d;

    return 0;
ST_OPT_ERR:
    return -1;
}

void param_arg_clear(param_arg_t *arg)
{
    arg->l2_step = 0;
}

/**
 * update weight using parameters.
 * 
 * in is [ in_size x 1 ];
 *  
 *
 * er_size > 0 && in_size > 0: er is [ 1 x er_size ]; wt is [ er_size x in_size ]; if in == NULL: in is one-shot vector
 * er_size > 0 && in_size < 0: er is [ 1 x er_size ]; wt is hash based 1d vector start from hash_start; in is one-shot vector
 * er_size < 0 && in_size > 0: er is delta-weight matrix [ er_size x in_size ]; wt is [ er_size x in_size ]; 
 * er_size < 0 && in_size < 0: er is delta-weight matrix [ er_size x in_size ]; wt is [ er_size x in_size ]; in is one-shot vector
 */
void param_update(param_t *param, param_arg_t *arg,
        real_t *wt, real_t *er, real_t er_scale,
        int er_size, real_t *in, hash_size_t in_size,
        hash_size_t hash_start)
{
    real_t *w;
    real_t *delta_w;

    real_t lr;
    real_t l2;

    hash_size_t i;
    int j;

    lr = param->learn_rate;
    l2 = param->l2_penalty;

    if (param->l2_gap > 1 && arg != NULL) {
        if (arg->l2_step != 0) {
            l2 = 0.0;
        }
        arg->l2_step++;
        if (arg->l2_step >= param->l2_gap) {
            arg->l2_step = 0;
        }
    }

    if (er_size > 0 && in_size > 0) {
        if (in == NULL) {
            i = 0;
            for (j = 0; j < er_size; j++) {
                wt[i] += lr * er[j] * er_scale
                    - l2 * wt[i];
                i += in_size;
            }
        } else {
            w = wt;
            for (j = 0; j < er_size; j++) {
                for (i = 0; i < in_size; i++) {
                    w[i] += lr * er[j] * er_scale * in[i]
                        - l2 * w[i];
                }
                w += in_size;
            }
        }
    } else if (er_size > 0 && in_size < 0) {
        in_size = - in_size;
        i = hash_start;
        for (j = 0; j < er_size; j++) {
            wt[i] += lr * er[j] * er_scale // * 1.0
                - l2 * wt[i];
            i++;
            if (i >= in_size) {
                i = 0;
            }
        }
    } else if (er_size < 0 && in_size > 0) {
        er_size = - er_size;
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

