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
#include <stutils/st_string.h>

#include <connlm/config.h>

#include "input.h"

#define INPUT_TEST_N 16
typedef struct _input_ref_t_ {
    st_wt_int_t context[INPUT_TEST_N];
    int n_ctx;
} input_ref_t;

void input_test_mk_topo_line(char *line, size_t len, input_ref_t *ref)
{
    int i;

    assert(line != NULL && ref != NULL);

    snprintf(line, len, "input context=");
    for (i = 0; i < ref->n_ctx - 1; i++) {
        st_strncatf(line, len, "%d:%g,", ref->context[i].i, ref->context[i].w);
    }
    st_strncatf(line, len, "%d:%g", ref->context[i].i, ref->context[i].w);

#ifdef _INPUT_TEST_PRINT_TOPO_
    fprintf(stderr, "%s", line);
#endif
}

int input_test_check_input(input_t *input, int input_size, input_ref_t *ref)
{
    int i;

    assert(input != NULL && ref != NULL);

    if (input->input_size != input_size) {
        fprintf(stderr, "input size not match.\n");
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

input_t* input_test_new(int input_size)
{
    input_t *input = NULL;

    input = (input_t *)st_malloc(sizeof(input_t));
    assert (input != NULL);

    memset(input, 0, sizeof(input_t));

    input->input_size = input_size;

    return input;
}

#ifdef __cplusplus
}
#endif

#endif
