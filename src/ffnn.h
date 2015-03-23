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

#include <stconf.h>

#include "config.h"

typedef struct _ffnn_opt_t {
    real_t scale;
} ffnn_opt_t;

typedef struct _ffnn_t_ {
    ffnn_opt_t ffnn_opt;
} ffnn_t;

int ffnn_load_opt(ffnn_opt_t *ffnn_opt, stconf_t *pconf, const char *sec_name);

ffnn_t *ffnn_create(ffnn_opt_t *ffnn_opt);
#define safe_ffnn_destroy(ptr) do {\
    if(ptr != NULL) {\
        ffnn_destroy(ptr);\
        safe_free(ptr);\
        ptr = NULL;\
    }\
    } while(0)
void ffnn_destroy(ffnn_t *ffnn);
int ffnn_save(ffnn_t *ffnn, bool binary, FILE *fo);

int ffnn_train(ffnn_t *ffnn);

#ifdef __cplusplus
}
#endif

#endif
