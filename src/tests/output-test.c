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
#include <stutils/st_log.h>

#include "output.h"

#define N 17
#define MAX_LEN 16384

static char *refs[MAX_LEN] = {
    "REF0",
};

static int check_output(output_t *output, const char* ref)
{
    char *buf = NULL;
    FILE *fp = NULL;
    size_t sz;
    int ret;

    fp = tmpfile();
    if (fp == NULL) {
        ST_WARNING("Failed to tmpfile.");
        goto ERR;
    }

    if (output_draw(output, fp) < 0) {
        ST_WARNING("Failed to output_draw.");
        goto ERR;
    }

    sz = ftell(fp);

    buf = malloc(sz);
    if (buf == NULL) {
        ST_WARNING("Failed to malloc buf.");
        goto ERR;
    }

    rewind(fp);

    if (fread(buf, 1, sz, fp) != sz) {
        ST_WARNING("Failed to read back buf.");
        goto ERR;
    }

    ret = strcmp(ref, buf);

    safe_fclose(fp);
    safe_free(buf);

    return ret;
ERR:
    safe_fclose(fp);
    safe_free(buf);

    return -1;
}

static int unit_test_output_generate()
{
    count_t word_cnts[N];
    int i;
    int ncase = 0;
    output_opt_t output_opt;
    output_t *output = NULL;

    for (i = 0; i < N; i++) {
        word_cnts[i] = N - i;
    }
    fprintf(stderr, "  Testing output generate...\n");
    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    output_opt.method = TOP_DOWN;
    output_opt.max_depth = 0;
    output_opt.max_branch = 3;
    output = output_generate(&output_opt, word_cnts, N);
    if (output == NULL) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    if (check_output(output, refs[ncase]) != 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    safe_output_destroy(output);
    fprintf(stderr, "Success\n");

    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    output_opt.method = BOTTOM_UP;
    output_opt.max_depth = 0;
    output_opt.max_branch = 3;
    output = output_generate(&output_opt, word_cnts, N);
    if (output == NULL) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    if (check_output(output, refs[ncase]) != 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    safe_output_destroy(output);
    fprintf(stderr, "Success\n");

    return 0;

ERR:
    safe_output_destroy(output);
    return -1;
}

static int run_all_tests()
{
    int ret = 0;

    if (unit_test_output_generate() != 0) {
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
