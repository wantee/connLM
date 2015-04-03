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

#include "nn.h"

static nn_param_t def_param = {
    .learn_rate = 0.1,
    .l1_penalty = 0.0,
    .l2_penalty = 0.0,
    .momentum = 0.0,
    .gradient_cutoff = 0.0,
};

int nn_param_load(nn_param_t *nn_param, 
        st_opt_t *opt, const char *sec_name, nn_param_t *parent_param)
{
    float f;

    ST_CHECK_PARAM(nn_param == NULL || opt == NULL, -1);

    if (parent_param == NULL) {
        *nn_param = def_param;
    } else {
        *nn_param = *parent_param;
    }

    ST_OPT_SEC_GET_FLOAT(opt, sec_name, "LEARN_RATE", f,
            (float)nn_param->learn_rate,
            "Learning rate");
    nn_param->learn_rate = (real_t)f;

    ST_OPT_SEC_GET_FLOAT(opt, sec_name, "L1_PENALTY", f, 
            (float)nn_param->l1_penalty,
            "L1 penalty (promote sparsity)");
    nn_param->l1_penalty = (real_t)f;

    ST_OPT_SEC_GET_FLOAT(opt, sec_name, "L2_PENALTY", f,
            (float)nn_param->l2_penalty,
            "L2 penalty (weight decay)");
    nn_param->l2_penalty = (real_t)f;

    ST_OPT_SEC_GET_FLOAT(opt, sec_name, "MOMENTUM", f,
            (float)nn_param->momentum,
            "Momentum");
    nn_param->momentum = (real_t)f;

    ST_OPT_SEC_GET_FLOAT(opt, sec_name, "GRADIENT_CUTOFF", f,
            (float)nn_param->gradient_cutoff,
            "Cutoff of gradient");
    nn_param->gradient_cutoff = (real_t)f;

    return 0;
ST_OPT_ERR:
    return -1;
}

int nn_forward(nn_t *nn)
{
    ST_CHECK_PARAM(nn == NULL, -1);

    return 0;
}

int nn_backprop(nn_t *nn)
{
    ST_CHECK_PARAM(nn == NULL, -1);

    return 0;
}

