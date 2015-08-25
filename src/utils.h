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

#include <math.h>

#include <st_opt.h>

#include "config.h"

#define exp10(a) pow(10.0, a)

#define logn(a, base) ((base) == 0) ? log(a) : (log(a) / log(base))

void matXvec(real_t *dst, real_t *mat, real_t *vec,
        int mat_wid, int vec_size, real_t scale);

void vecXmat(real_t *dst, real_t *vec, real_t *mat,
        int mat_col, int vec_size, real_t scale);

void sigmoid(real_t *vec, int vec_size);

void softmax(real_t *vec, int vec_size);

void connlm_show_usage(const char *module_name, const char *header,
        const char *usage, st_opt_t *opt);

void int_sort(int *A, size_t n);

typedef enum _model_filter_t_ {
    MF_NONE       = 0x0000,
    MF_VOCAB      = 0x0001,
    MF_OUTPUT     = 0x0002,
    MF_MAXENT     = 0x0004,
    MF_RNN        = 0x0008,
    MF_LBL        = 0x0010,
    MF_FFNN       = 0x0020,
    MF_ALL        = 0x003F,
} model_filter_t;

/**
 * Parsing a model filter.
 * A model filter can be: mdl,[+-][ovmrlf]:file_name.
 * Any string not in such format will be treated as a model file name.
 *
 * @ingroup connlm
 * @param[in] mdl_filter string for the model filter.
 * @param[out] mdl_file name of model file.
 * @param[in] mdl_file_len max len of model file buffer.
 * @return filter type. MF_NONE if error.
 */
model_filter_t parse_model_filter(const char *mdl_filter,
        char *mdl_file, size_t mdl_file_len);

#ifdef __cplusplus
}
#endif

#endif
