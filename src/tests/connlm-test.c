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

#include "glues/sum_glue.h"
#include "glues/direct_glue.h"
#include "connlm.h"

#include "vocab-test.h"
#include "output-test.h"
#include "comp-test.h"

#define CONNLM_TEST_N 16

typedef struct _connlm_ref_t_ {
    output_ref_t output_ref;

    int num_comp;
    comp_ref_t comp_refs[CONNLM_TEST_N];
} connlm_ref_t;

static FILE* connlm_mk_topo_file(connlm_ref_t *ref)
{
    char lines[MAX_LINE_LEN * 5];
    FILE *fp;
    int c;

    fp = tmpfile();
    assert(fp != NULL);

    fprintf(fp, "foo foo\n");
    fprintf(fp, "<output>\n");
    output_test_mk_topo_line(lines, MAX_LINE_LEN * 5, &(ref->output_ref));
    fprintf(fp, "%s\n", lines);
    fprintf(fp, "</output>\n");

    for (c = 0; c < ref->num_comp; c++) {
        fprintf(fp, "<component>\n");
        comp_test_mk_topo_lines(lines, MAX_LINE_LEN * 5,
                ref->comp_refs + c, c);
        fprintf(fp, "%s", lines);
        fprintf(fp, "</component>\n");
        fprintf(fp, "foo\nbar\n");
    }
    rewind(fp);

#ifdef _CONNLM_TEST_PRINT_TOPO_
    {
        char line[MAX_LINE_LEN];
        while(fgets(line, MAX_LINE_LEN, fp)) {
            fprintf(stderr, "%s", line);
        }
        rewind(fp);
    }
#endif

    return fp;
}

static int check_connlm(connlm_t *connlm, connlm_ref_t *ref)
{
    int c;

    if (output_test_check_output(connlm->output, &(ref->output_ref)) != 0) {
        fprintf(stderr, "output not match\n");
        return -1;
    }

    if (connlm->num_comp != ref->num_comp) {
        fprintf(stderr, "num_comp not match\n");
        return -1;
    }
    for (c = 0; c < connlm->num_comp; c++) {
        if (comp_test_check_comp(connlm->comps[c],
                    connlm->comps[c]->input->input_size,
                    ref->comp_refs + c, c) != 0) {
            fprintf(stderr, "component not match\n");
            return -1;
        }
    }

    return 0;
}

static int unit_test_connlm_read_topo()
{
    FILE *fp = NULL;
    connlm_t *connlm = NULL;

    vocab_t *vocab = NULL;
    output_t *output = NULL;

    connlm_ref_t ref;
    int ncase = 0;
    connlm_ref_t std_ref = {
        .output_ref = {
            .norm = ON_SOFTMAX,
        },

        .num_comp = 1,
        .comp_refs = {
            {
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
            },
        },
    };

    fprintf(stderr, "  Testing Reading topology file...\n");
    vocab = vocab_test_new();
    assert(vocab != NULL);
    output = output_test_new(vocab);
    assert(output != NULL);

    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    ref = std_ref;
    fp = connlm_mk_topo_file(&ref);
    connlm = connlm_new(vocab, output, NULL, 0);
    assert(connlm != NULL);
    if (connlm_init(connlm, fp) < 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    if (check_connlm(connlm, &ref) != 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }

    safe_connlm_destroy(connlm);
    safe_fclose(fp);
    fprintf(stderr, "Success\n");

    safe_vocab_destroy(vocab);
    safe_output_destroy(output);
    return 0;

ERR:
    safe_connlm_destroy(connlm);
    safe_vocab_destroy(vocab);
    safe_output_destroy(output);
    safe_fclose(fp);
    return -1;
}

static int run_all_tests()
{
    int ret = 0;

    if (unit_test_connlm_read_topo() != 0) {
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
