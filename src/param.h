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

#ifndef  _CONNLM_PARAM_H_
#define  _CONNLM_PARAM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stutils/st_opt.h>

#include <connlm/config.h>

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
    int l2_gap; /**< if biggen than 0, applying L2 penalty every l2_gap steps */
    real_t momentum; /**< value of momentum */
    int mini_batch; /**< size of mini-batch */
    int sync_size; /**< if bigger than 0, sync weight for threads every sync_size steps. */
} param_t;

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
 * Show param usage to stderr
 * @ingroup g_param
 */
void param_show_usage();

#ifdef __cplusplus
}
#endif

#endif
