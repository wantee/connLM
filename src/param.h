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

typedef struct _param_t_ {
    real_t learn_rate;
    real_t l1_penalty;
    real_t l2_penalty;
    int l2_gap;
    real_t momentum;
    int mini_batch;
} param_t;

typedef struct _param_arg_t_ {
    int l2_step;
} param_arg_t;

void param_arg_clear(param_arg_t *arg);

int param_load(param_t *param, st_opt_t *opt, const char *sec_name,
        param_t *parent_param);

void param_acc_wt(real_t *wt, real_t *er, int er_size, real_t *in,
        hash_size_t in_size, hash_size_t hash_start);

void param_update(param_t *param, param_arg_t *arg,
        real_t *wt, real_t *er, real_t er_scale,
        int er_size, real_t *in, hash_size_t in_size,
        hash_size_t hash_start);

#ifdef __cplusplus
}
#endif

#endif
