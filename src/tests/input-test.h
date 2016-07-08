/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Wang Jian
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

#ifndef  _CONNLM_INPUT_TEST_H_
#define  _CONNLM_INPUT_TEST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stutils/st_macro.h>
#include <stutils/st_utils.h>

#include <connlm/config.h>

#include "input.h"

#define N_CTX 16
typedef struct _input_ref_t_ {
    st_wt_int_t context[N_CTX];
    int n_ctx;
    input_combine_t combine;
} input_ref_t;

static const char *combine_str[] = {
    "UnDefined",
    "Sum",
    "Avg",
    "Concat",
};

void input_test_mk_topo_line(char *line, input_ref_t *ref)
{
    char array[1024];
    char buf[20];
    int i;

    assert(line != NULL && ref != NULL);

    array[0] = '\0';
    for (i = 0; i < ref->n_ctx - 1; i++) {
        snprintf(buf, 20, "%d:%g,", ref->context[i].i, ref->context[i].w);
        strcat(array, buf);
    }
    snprintf(buf, 20, "%d:%g", ref->context[i].i, ref->context[i].w);
    strcat(array, buf);

    sprintf(line, "input context=%s combine=%s", array,
            combine_str[ref->combine]);
#ifdef _CONNLM_TEST_PRINT_TOPO_
    {
        fprintf(stderr, "%s", line);
    }
#endif
}

int input_test_check_input(input_t *input, input_ref_t *ref)
{
    int i;

    assert(input != NULL && ref != NULL);

    if (input->combine != ref->combine) {
        fprintf(stderr, "combine not match[%d/%d]\n",
                input->combine, ref->combine);
        return -1;
    }

    if (input->n_ctx != ref->n_ctx) {
        fprintf(stderr, "n_ctx not match[%d/%d]\n",
                input->n_ctx, ref->n_ctx);
        return -1;
    }

    for (i = 0; i < ref->n_ctx; i++) {
        if (input->context[i].i != ref->context[i].i) {
            fprintf(stderr, "context not match[%d:%d/%d]\n",
                    i, input->context[i].i, ref->context[i].i);
            return -1;
        }
        if (!APPROX_EQUAL(input->context[i].w, ref->context[i].w)) {
            fprintf(stderr, "context weight not match[%d:%f/%f]\n",
                    i, input->context[i].w, ref->context[i].w);
            return -1;
        }
    }

    return 0;
}

#ifdef __cplusplus
}
#endif

#endif
