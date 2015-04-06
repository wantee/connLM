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

#ifndef  _LBL_H_
#define  _LBL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <st_opt.h>

#include "config.h"
#include "param.h"
#include "output.h"

typedef struct _lbl_model_opt_t {
    real_t scale;
} lbl_model_opt_t;

typedef struct _lbl_train_opt_t {
    param_t param;
} lbl_train_opt_t;

typedef struct _lbl_t_ {
    lbl_model_opt_t model_opt;
    output_t *output;

    lbl_train_opt_t train_opt;
} lbl_t;

int lbl_load_model_opt(lbl_model_opt_t *model_opt, st_opt_t *opt,
        const char *sec_name);
int lbl_load_train_opt(lbl_train_opt_t *train_opt, st_opt_t *opt,
        const char *sec_name, param_t *param);

int lbl_init(lbl_t **plbl, lbl_model_opt_t *model_opt, output_t *output);
#define safe_lbl_destroy(ptr) do {\
    if((ptr) != NULL) {\
        lbl_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
void lbl_destroy(lbl_t *lbl);
lbl_t* lbl_dup(lbl_t *l);

int lbl_load_header(lbl_t **lbl, FILE *fp, bool *binary, FILE *fo_info);
int lbl_load_body(lbl_t *lbl, FILE *fp, bool binary);
int lbl_save_header(lbl_t *lbl, FILE *fp, bool binary);
int lbl_save_body(lbl_t *lbl, FILE *fp, bool binary);

int lbl_setup_train(lbl_t *lbl, lbl_train_opt_t *train_opt,
        output_t *output);
int lbl_forward(lbl_t *lbl, int word);
int lbl_backprop(lbl_t *lbl, int word);

#ifdef __cplusplus
}
#endif

#endif
