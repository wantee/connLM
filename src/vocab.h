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

#ifndef  _VOCAB_H_
#define  _VOCAB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <st_opt.h>
#include <st_alphabet.h>

#include "config.h"

typedef struct _vocab_opt_t_ {
    int max_word_num;
} vocab_opt_t;

typedef struct _vocab_t_ {
    vocab_opt_t vocab_opt;

    int vocab_size;

    st_alphabet_t *alphabet;
    count_t *cnts;
} vocab_t;

int vocab_load_opt(vocab_opt_t *vocab_opt, st_opt_t *opt,
        const char *sec_name);
vocab_t *vocab_create(vocab_opt_t *vocab_opt);
#define safe_vocab_destroy(ptr) do {\
    if((ptr) != NULL) {\
        vocab_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
void vocab_destroy(vocab_t *vocab);
vocab_t* vocab_dup(vocab_t *v);

long vocab_load_header(vocab_t **vocab, FILE *fp, bool *binary, FILE *fo);
int vocab_load_body(vocab_t *vocab, FILE *fp, bool binary);
int vocab_save_header(vocab_t *vocab, FILE *fp, bool binary);
int vocab_save_body(vocab_t *vocab, FILE *fp, bool binary);

int vocab_learn(vocab_t *vocab, FILE *fp);

int vocab_get_id(vocab_t *vocab, const char *word);
char* vocab_get_word(vocab_t *vocab, int id);
int vocab_add_word(vocab_t *vocab, const char* word);

#ifdef __cplusplus
}
#endif

#endif
