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

#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "input-test.h"

static int unit_test_input_read_topo()
{
    char line[1024];
    int ncase = 0;
    input_t *input = NULL;
    int input_sz = 15;
    input_ref_t ref;
    input_ref_t std_ref = {
        .n_ctx = 2,
        .context = {{-1,1.0}, {1, 0.5}},
        .combine = IC_SUM,
    };

    fprintf(stderr, "  Testing Reading topology line...\n");
    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    ref = std_ref;
    line[0] = '\0';
    input_test_mk_topo_line(line, &ref);
    input = input_parse_topo(line, input_sz);
    if (input == NULL) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    if (input_test_check_input(input, &ref) != 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    safe_input_destroy(input);
    fprintf(stderr, "Success\n");

    return 0;

ERR:
    safe_input_destroy(input);
    return -1;
}

static int run_all_tests()
{
    int ret = 0;

    if (unit_test_input_read_topo() != 0) {
        ret = -1;
    }

    return ret;
}

int main(int argc, const char *argv[])
{
    int ret;

    fprintf(stderr, "Start testing...\n");
    ret = run_all_tests();
    if (ret != 0) {
        fprintf(stderr, "Tests failed.\n");
    } else {
        fprintf(stderr, "Tests succeeded.\n");
    }

    return ret;
}
