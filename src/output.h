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

#ifndef  _OUTPUT_H_
#define  _OUTPUT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <st_opt.h>
#include <st_alphabet.h>

#include "config.h"

typedef struct _output_opt_t_ {
    int class_size;
    bool hs;
} output_opt_t;

typedef struct _output_t_ {
    output_opt_t output_opt;

    int output_size;

    // neurons
    real_t *ac_o_w;             // word part
    real_t *er_o_w;
    real_t *ac_o_c;             // class part
    real_t *er_o_c;

    // for classes
    int *w2c;                   // word to class map
    int *c2w_s;                 // start for class to word map
    int *c2w_e;                 // end for class to word map

} output_t;

int output_load_opt(output_opt_t *output_opt, st_opt_t *opt,
        const char *sec_name);

output_t *output_create(output_opt_t *output_opt, int output_size);
#define safe_output_destroy(ptr) do {\
    if((ptr) != NULL) {\
        output_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
void output_destroy(output_t *output);
output_t* output_dup(output_t *v);

long output_load_header(output_t **output, FILE *fp,
        bool *binary, FILE *fo);
int output_load_body(output_t *output, FILE *fp, bool binary);
int output_save_header(output_t *output, FILE *fp, bool binary);
int output_save_body(output_t *output, FILE *fp, bool binary);

int output_generate(output_t *output, count_t *word_cnts);

int output_activate(output_t *output, int word);
int output_loss(output_t *output, int word);
real_t output_get_prob(output_t *output, int word);

int output_setup_train(output_t *output);
int output_reset_train(output_t *output);
int output_clear_train(output_t *output, int word);

int output_setup_test(output_t *output);
int output_reset_test(output_t *output);
int output_clear_test(output_t *output, int word);

#ifdef __cplusplus
}
#endif

#endif
