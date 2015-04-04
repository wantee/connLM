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

#ifndef  _CONNLM_UTILS_H_
#define  _CONNLM_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"

void matXvec(real_t *dst, real_t *mat, real_t *vec, int mat_wid, int vec_size);

void vecXmat(real_t *dst, real_t *vec, real_t *mat, int mat_col, int vec_size);

void sigmoid(real_t *vec, int vec_size);

void softmax(real_t *vec, int vec_size);

real_t rrandom(real_t min, real_t max);

void connlm_show_usage(const char *module_name, const char *header,
        const char *usage, st_opt_t *opt);

#ifdef __cplusplus
}
#endif

#endif
