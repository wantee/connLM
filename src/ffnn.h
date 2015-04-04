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

#ifndef  _FFNN_H_
#define  _FFNN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <st_opt.h>

#include "config.h"
#include "nn.h"

typedef struct _ffnn_opt_t {
    real_t scale;

    nn_param_t param;
} ffnn_opt_t;

typedef struct _ffnn_t_ {
    ffnn_opt_t ffnn_opt;
} ffnn_t;

int ffnn_load_model_opt(ffnn_opt_t *ffnn_opt, st_opt_t *opt,
        const char *sec_name);
int ffnn_load_train_opt(ffnn_opt_t *ffnn_opt, st_opt_t *opt,
        const char *sec_name, nn_param_t *param);

int ffnn_init(ffnn_t **pffnn, ffnn_opt_t *ffnn_opt);
#define safe_ffnn_destroy(ptr) do {\
    if((ptr) != NULL) {\
        ffnn_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
void ffnn_destroy(ffnn_t *ffnn);
ffnn_t* ffnn_dup(ffnn_t *f);

int ffnn_load_header(ffnn_t **ffnn, FILE *fp, bool *binary, FILE *fo_info);
int ffnn_load_body(ffnn_t *ffnn, FILE *fp, bool binary);
int ffnn_save_header(ffnn_t *ffnn, FILE *fp, bool binary);
int ffnn_save_body(ffnn_t *ffnn, FILE *fp, bool binary);

int ffnn_setup_train(ffnn_t **ffnn, ffnn_opt_t *ffnn_opt);
int ffnn_forward(ffnn_t *ffnn, int word);

#ifdef __cplusplus
}
#endif

#endif
