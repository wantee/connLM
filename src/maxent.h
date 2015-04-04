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

#ifndef  _MAXENT_H_
#define  _MAXENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <st_opt.h>

#include "config.h"
#include "nn.h"

typedef unsigned long long hash_t;

typedef struct _maxent_opt_t {
    real_t scale;

    long long size;
    int order;

    nn_param_t param;
} maxent_opt_t;

typedef struct _maxent_t_ {
    maxent_opt_t maxent_opt;

    real_t *wt;
    int *hist;
    hash_t *hash;
} maxent_t;

int maxent_load_model_opt(maxent_opt_t *maxent_opt,
        st_opt_t *opt, const char *sec_name);
int maxent_load_train_opt(maxent_opt_t *maxent_opt,
        st_opt_t *opt, const char *sec_name, nn_param_t *param);

int maxent_init(maxent_t **pmaxent, maxent_opt_t *maxent_opt);
#define safe_maxent_destroy(ptr) do {\
    if((ptr) != NULL) {\
        maxent_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
void maxent_destroy(maxent_t *maxent);
maxent_t* maxent_dup(maxent_t *m);

int maxent_load_header(maxent_t **maxent, FILE *fp,
        bool *binary, FILE *fo_info);
int maxent_load_body(maxent_t *maxent, FILE *fp, bool binary);
int maxent_save_header(maxent_t *maxent, FILE *fp, bool binary);
int maxent_save_body(maxent_t *maxent, FILE *fp, bool binary);

int maxent_setup_train(maxent_t **maxent, maxent_opt_t *maxent_opt);
int maxent_forward(maxent_t *maxent, int word);

#ifdef __cplusplus
}
#endif

#endif
