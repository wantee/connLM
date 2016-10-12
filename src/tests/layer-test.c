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

#include "layer-test.h"

static int unit_test_layer_read_topo()
{
    char line[MAX_LINE_LEN];
    int ncase = 0;
    layer_t *layer = NULL;
    layer_ref_t ref;
    layer_ref_t std_ref = {
        .type = "sigmoid",
        .size = 15,
    };
    int id = 0;

    fprintf(stderr, "  Testing Reading topology line...\n");
    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    ref = std_ref;
    layer_test_mk_topo_line(line, MAX_LINE_LEN, &ref, id);
    layer = layer_parse_topo(line);
    if (layer == NULL) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    if (layer_test_check_layer(layer, &ref, id+2) != 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    safe_layer_destroy(layer);
    fprintf(stderr, "Success\n");

    return 0;

ERR:
    safe_layer_destroy(layer);
    return -1;
}

static int run_all_tests()
{
    int ret = 0;

    if (unit_test_layer_read_topo() != 0) {
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
