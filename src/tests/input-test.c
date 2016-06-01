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

#include <stutils/st_utils.h>

#include "input.h"

#define N_CTX 16
typedef struct _ref_t_ {
    int context[N_CTX];
    int n_ctx;
} ref_t;

static void mk_topo_line(char *line, ref_t *ref)
{
    char array[1024];
    char buf[20];
    int i;

    assert(line != NULL && ref != NULL);

    array[0] = '\0';
    for (i = 0; i < ref->n_ctx - 1; i++) {
        snprintf(buf, 20, "%d,", ref->context[i]);
        strcat(array, buf);
    }
    snprintf(buf, 20, "%d", ref->context[i]);
    strcat(array, buf);

    sprintf(line, "input context=%s", array);
#ifdef _CONNLM_TEST_PRINT_TOPO_
    {
        fprintf(stderr, "%s", line);
    }
#endif
}

static int check_input(input_t *input, ref_t *ref)
{
    int i;

    assert(input != NULL && ref != NULL);

    if (input->num_ctx != ref->n_ctx) {
        fprintf(stderr, "n_ctx not match[%d/%d]\n",
                input->num_ctx, ref->n_ctx);
        return -1;
    }

    for (i = 0; i < ref->n_ctx; i++) {
        if (input->context[i] != ref->context[i]) {
            fprintf(stderr, "context not match[%d:%d/%d]\n",
                    i, input->context[i], ref->context[i]);
            return -1;
        }
    }

    return 0;
}

static int unit_test_input_read_topo()
{
    char line[1024];
    int ncase = 0;
    input_t *input = NULL;
    int input_sz = 15;
    ref_t ref;
    ref_t std_ref = {
        .n_ctx = 3,
        .context = {-1,0,1},
    };

    fprintf(stderr, "  Testing Reading topology line...\n");
    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    ref = std_ref;
    line[0] = '\0';
    mk_topo_line(line, &ref);
    input = input_parse_topo(line, input_sz);
    if (input == NULL) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    if (check_input(input, &ref) != 0) {
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
