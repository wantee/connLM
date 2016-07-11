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

#ifndef  _CONNLM_OUTPUT_TEST_H_
#define  _CONNLM_OUTPUT_TEST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stutils/st_macro.h>

#include <connlm/config.h>

#include "output.h"

#define OUTPUT_TEST_N 16
typedef struct _output_ref_t_ {
    output_norm_t norm;
} output_ref_t;

static const char *norm_str[] = {
    "Undefined",
    "Softmax",
    "NCE",
};

void output_test_mk_topo_line(char *line, size_t len, output_ref_t *ref)
{
    assert(line != NULL && ref != NULL);

    snprintf(line, len, "property norm=%s", norm_str[ref->norm]);

#ifdef _OUTPUT_TEST_PRINT_TOPO_
    fprintf(stderr, "%s\n", line);
#endif
}

int output_test_check_output(output_t *output, output_ref_t *ref)
{
    assert(output != NULL && ref != NULL);

    if (output->norm != ref->norm) {
        fprintf(stderr, "norm not match[%d/%d]\n",
                output->norm, ref->norm);
        return -1;
    }

    return 0;
}

#ifdef __cplusplus
}
#endif

#endif
