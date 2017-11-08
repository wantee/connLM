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
    .dropout = 0.0,
    .l1_penalty = 0.0,
    .l2_penalty = 0.0,
    .momentum = 0.0,
    .er_cutoff = 50.0,

    .learn_rate_coef = 1.0,
    .bias_learn_rate_coef = 1.0,
    .momentum_coef = 1.0,
    .bias_momentum_coef = 1.0,
};

void param_show_usage()
{
    param_t param;
    st_opt_t *opt = NULL;

    opt = st_opt_create();
    if (opt == NULL) {
        ST_ERROR("Failed to st_opt_create.");
        goto ST_OPT_ERR;
    }

    if (param_load(&param, opt, NULL, NULL) < 0) {
        ST_ERROR("Failed to param_load");
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

    ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "DROPOUT", d,
            (double)param->dropout,
            "Dropout probability");
    param->dropout = (real_t)d;

    ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "L1_PENALTY", d,
            (double)param->l1_penalty,
            "L1 penalty (promote sparsity)");
    param->l1_penalty = (real_t)d;

    ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "L2_PENALTY", d,
            (double)param->l2_penalty,
            "L2 penalty (weight decay)");
    param->l2_penalty = (real_t)d;

    ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "MOMENTUM", d,
            (double)param->momentum,
            "Momentum. Note: to make the 'effective' learning rate remains "
            "the same, the learning rate will be multiplied by (1-momentum)");
    param->momentum = (real_t)d;

    ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "ERR_CUTOFF", d,
            (double)param->er_cutoff, "Cutoff of error");
    param->er_cutoff = (real_t)d;

    ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "LEARN_RATE_COEF", d,
            (double)param->learn_rate_coef,
            "Learning rate coefficient for weights.");
    param->learn_rate_coef = (real_t)d;

    ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "BIAS_LEARN_RATE_COEF", d,
            (double)param->bias_learn_rate_coef,
            "Learning rate coefficient for bias.");
    param->bias_learn_rate_coef = (real_t)d;

    ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "MOMENTUM_COEF", d,
            (double)param->momentum_coef,
            "Momentum coefficient for weights.");
    param->momentum_coef = (real_t)d;

    ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "BIAS_MOMENTUM_COEF", d,
            (double)param->bias_momentum_coef,
            "Momentum coefficient for bias.");
    param->bias_momentum_coef = (real_t)d;


    return 0;
ST_OPT_ERR:
    return -1;
}

bool param_equal(param_t *param1, param_t *param2)
{
    ST_CHECK_PARAM(param1 == NULL || param2 == NULL, false);

    if (param1->learn_rate != param2->learn_rate) {
        return false;
    }

    if (param1->dropout != param2->dropout) {
        return false;
    }

    if (param1->l1_penalty != param2->l1_penalty) {
        return false;
    }

    if (param1->l2_penalty != param2->l2_penalty) {
        return false;
    }

    if (param1->momentum != param2->momentum) {
        return false;
    }

    if (param1->er_cutoff != param2->er_cutoff) {
        return false;
    }

    if (param1->learn_rate_coef != param2->learn_rate_coef) {
        return false;
    }

    if (param1->bias_learn_rate_coef != param2->bias_learn_rate_coef) {
        return false;
    }

    if (param1->momentum_coef != param2->momentum_coef) {
        return false;
    }

    if (param1->bias_momentum_coef != param2->bias_momentum_coef) {
        return false;
    }

    return true;
}

static bptt_opt_t def_bptt_opt = {
    .bptt = 0,
};

void bptt_opt_show_usage()
{
    bptt_opt_t bptt_opt;
    st_opt_t *opt = NULL;

    opt = st_opt_create();
    if (opt == NULL) {
        ST_ERROR("Failed to st_opt_create.");
        goto ST_OPT_ERR;
    }

    if (bptt_opt_load(&bptt_opt, opt, NULL, NULL) < 0) {
        ST_ERROR("Failed to bptt_opt_load");
        goto ST_OPT_ERR;
    }

    fprintf(stderr, "\nBPTT options for recurrent glue:\n");
    st_opt_show_usage(opt, stderr, false);

ST_OPT_ERR:
    safe_st_opt_destroy(opt);
}

int bptt_opt_load(bptt_opt_t *bptt_opt, st_opt_t *opt, const char *sec_name,
        bptt_opt_t *parent_bptt_opt)
{
    ST_CHECK_PARAM(bptt_opt == NULL || opt == NULL, -1);

    if (parent_bptt_opt == NULL) {
        *bptt_opt = def_bptt_opt;
    } else {
        *bptt_opt = *parent_bptt_opt;
    }

    ST_OPT_SEC_GET_INT(opt, sec_name, "BPTT", bptt_opt->bptt,
            bptt_opt->bptt, "Time steps of BPTT.");
    if (bptt_opt->bptt < 1) {
        bptt_opt->bptt = 1;
    }

    return 0;
ST_OPT_ERR:
    return -1;
}

bool bptt_opt_equal(bptt_opt_t *bptt_opt1, bptt_opt_t *bptt_opt2)
{
    ST_CHECK_PARAM(bptt_opt1 == NULL || bptt_opt2 == NULL, false);

    if (bptt_opt1->bptt != bptt_opt2->bptt) {
        return false;
    }

    return true;
}
