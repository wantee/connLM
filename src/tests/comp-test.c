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

#include <stutils/st_macro.h>

#include "vocab-test.h"
#include "output-test.h"
#include "comp-test.h"

static int unit_test_comp_read_topo()
{
    vocab_t *vocab = NULL;
    output_t *output = NULL;
    int input_size = 15;

    char lines[MAX_LINE_LEN * 5];
    int ncase = 0;
    component_t *comp = NULL;
    comp_ref_t ref;
    comp_ref_t std_ref = {
        .input_ref = {
            .n_ctx = 1,
            .context = {{-1,1.0}},
            .combine = IC_AVG,
        },

        .num_layer = 0,

        .num_glue = 1,
        .glue_refs = {
            {
                .type = "direct",
                .num_in_layer = 1,
                .in_layers = {1},
                .in_offsets = {0},
                .in_scales = {1.0},
                .num_out_layer = 1,
                .out_layers = {0},
                .out_offsets = {0},
                .out_scales = {1.0},
                .sum_glue = {true, true},
                .direct_glue = {2000000},
            },
        },
    };
    int id = 0;

    fprintf(stderr, "  Testing Reading topology lines...\n");
    vocab = vocab_test_new();
    assert(vocab != NULL);
    output = output_test_new(vocab);
    assert(output != NULL);

    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    ref = std_ref;
    comp_test_mk_topo_lines(lines, MAX_LINE_LEN * 5, &ref, id);
    comp = comp_init_from_topo(lines, output, input_size);
    if (comp == NULL) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    if (comp_test_check_comp(comp, input_size, &ref, id) != 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    safe_comp_destroy(comp);
    fprintf(stderr, "Success\n");

    safe_vocab_destroy(vocab);
    safe_output_destroy(output);

    return 0;

ERR:
    safe_comp_destroy(comp);

    safe_vocab_destroy(vocab);
    safe_output_destroy(output);

    return -1;
}

static int run_all_tests()
{
    int ret = 0;

    if (unit_test_comp_read_topo() != 0) {
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
