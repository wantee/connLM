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
#include "input-test.h"
#include "glue-test.h"

#define GLUE_TEST_N 16

static int unit_test_glue_read_topo()
{
    vocab_t *vocab = NULL;
    output_t *output = NULL;
    input_t *input = NULL;
    layer_t *layers[GLUE_TEST_N];
    int n_layer;
    layer_t *input_layer = NULL;
    layer_t *output_layer = NULL;
    int input_size = 15;

    char line[MAX_LINE_LEN];
    int ncase = 0;
    glue_t *glue = NULL;
    glue_ref_t ref;
    glue_ref_t std_ref = {
        .type = "direct_wt",
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
    };
    int id = 0;

    fprintf(stderr, "  Testing Reading topology line...\n");
    vocab = vocab_test_new();
    assert(vocab != NULL);
    output = output_test_new(vocab);
    assert(output != NULL);
    input = input_test_new(input_size);
    assert(input != NULL);
    input_layer = input_get_layer(input);
    assert(input_layer != NULL);
    output_layer = output_get_layer(output);
    assert(output_layer != NULL);
    layers[0] = output_layer;
    layers[1] = input_layer;
    n_layer = 2;

    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    ref = std_ref;
    glue_test_mk_topo_line(line, MAX_LINE_LEN, &ref, id);
    glue = glue_parse_topo(line, layers, n_layer, input, output);
    if (glue == NULL) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    if (glue_test_check_glue(glue, &ref, id) != 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    safe_glue_destroy(glue);
    fprintf(stderr, "Success\n");

    safe_layer_destroy(input_layer);
    safe_layer_destroy(output_layer);
    safe_vocab_destroy(vocab);
    safe_output_destroy(output);
    safe_input_destroy(input);

    return 0;

ERR:
    safe_glue_destroy(glue);

    safe_layer_destroy(input_layer);
    safe_layer_destroy(output_layer);
    safe_vocab_destroy(vocab);
    safe_output_destroy(output);
    safe_input_destroy(input);

    return -1;
}

static int run_all_tests()
{
    int ret = 0;

    if (unit_test_glue_read_topo() != 0) {
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
