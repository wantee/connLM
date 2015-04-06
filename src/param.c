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
    .momentum = 0.0,
    .gradient_cutoff = 0.0,
};

int param_load(param_t *param, st_opt_t *opt, const char *sec_name,
        param_t *parent_param)
{
    float f;

    ST_CHECK_PARAM(param == NULL || opt == NULL, -1);

    if (parent_param == NULL) {
        *param = def_param;
    } else {
        *param = *parent_param;
    }

    ST_OPT_SEC_GET_FLOAT(opt, sec_name, "LEARN_RATE", f,
            (float)param->learn_rate,
            "Learning rate");
    param->learn_rate = (real_t)f;

    ST_OPT_SEC_GET_FLOAT(opt, sec_name, "L1_PENALTY", f, 
            (float)param->l1_penalty,
            "L1 penalty (promote sparsity)");
    param->l1_penalty = (real_t)f;

    ST_OPT_SEC_GET_FLOAT(opt, sec_name, "L2_PENALTY", f,
            (float)param->l2_penalty,
            "L2 penalty (weight decay)");
    param->l2_penalty = (real_t)f;

    ST_OPT_SEC_GET_FLOAT(opt, sec_name, "MOMENTUM", f,
            (float)param->momentum,
            "Momentum");
    param->momentum = (real_t)f;

    ST_OPT_SEC_GET_FLOAT(opt, sec_name, "GRADIENT_CUTOFF", f,
            (float)param->gradient_cutoff,
            "Cutoff of gradient");
    param->gradient_cutoff = (real_t)f;

    return 0;
ST_OPT_ERR:
    return -1;
}

int param_update(param_t *param, real_t *wt, real_t *er, real_t *x,
        int er_size, int wt_row, int wt_start)
{
    real_t *w;

    int r;
    int i;
    int j;

    if (wt_row < 0) {
        j = wt_start;
        for (i = 0; i < er_size; i++) {
            wt[j] += param->learn_rate * er[i] * ((x == NULL) ? 1 : x[i])
                - param->l2_penalty * wt[j];
            j++;
            j %= (-wt_row);
        }
    } else {
        for (r = 0; r < wt_row; r++) {
            w = wt + r * er_size;
            for (i = 0; i < er_size; i++) {
                w[i] += param->learn_rate * er[i] * ((x == NULL) ? 1 : x[i])
                    - param->l2_penalty * w[i];
            }
        }
    }

    return 0;
}

