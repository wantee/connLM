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

#include <stutils/st_macro.h>
#include <stutils/st_log.h>
#include <stutils/st_opt.h>

#include "param.h"

static param_t def_param = {
    .learn_rate = 0.1,
    .l1_penalty = 0.0,
    .l2_penalty = 0.0,
    .l2_gap = 1,
    .momentum = 0.0,
    .mini_batch = 0,
};

void param_show_usage()
{
    param_t param;
    st_opt_t *opt = NULL;

    opt = st_opt_create();
    if (opt == NULL) {
        ST_WARNING("Failed to st_opt_create.");
        goto ST_OPT_ERR;
    }

    if (param_load(&param, opt, NULL, NULL) < 0) {
        ST_WARNING("Failed to param_load");
        goto ST_OPT_ERR;
    }

    fprintf(stderr, "\nUpdating Parameters:\n");
    st_opt_show_usage(opt, stderr, false);

ST_OPT_ERR:
    safe_st_opt_destroy(opt);
}

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

    ST_OPT_SEC_GET_INT(opt, sec_name, "MINI_BATCH", param->mini_batch,
            param->mini_batch,
            "Mini-batch size");

    return 0;
ST_OPT_ERR:
    return -1;
}
