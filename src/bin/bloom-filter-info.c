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

#include <string.h>

#include <stutils/st_log.h>
#include <stutils/st_io.h>

#include <connlm/utils.h>
#include <connlm/connlm.h>

#include <connlm/bloom_filter.h>

bool g_verbose;

st_opt_t *g_cmd_opt;

int bloom_filter_info_parse_opt(int *argc, const char *argv[])
{
    st_log_opt_t log_opt;
    bool b;

    g_cmd_opt = st_opt_create();
    if (g_cmd_opt == NULL) {
        ST_ERROR("Failed to st_opt_create.");
        goto ST_OPT_ERR;
    }

    if (st_opt_parse(g_cmd_opt, argc, argv) < 0) {
        ST_ERROR("Failed to st_opt_parse.");
        goto ST_OPT_ERR;
    }

    if (st_log_load_opt(&log_opt, g_cmd_opt, NULL) < 0) {
        ST_ERROR("Failed to st_log_load_opt");
        goto ST_OPT_ERR;
    }

    if (st_log_open(&log_opt) != 0) {
        ST_ERROR("Failed to open log");
        goto ST_OPT_ERR;
    }

    ST_OPT_GET_BOOL(g_cmd_opt, "verbose", g_verbose, false,
            "Print verbose info.");

    ST_OPT_GET_BOOL(g_cmd_opt, "help", b, false, "Print help");

    return (b ? 1 : 0);

ST_OPT_ERR:
    return -1;
}

void show_usage(const char *module_name)
{
    connlm_show_usage(module_name,
            "Print Information of Bloom Filter",
            "<model0> [<model1> <model2> ...]",
            "exp/train.blmflt",
            g_cmd_opt, NULL);
}

int main(int argc, const char *argv[])
{
    FILE *fp = NULL;
    bloom_filter_t *blm_flt = NULL;
    int ret;
    int i;

    ret = bloom_filter_info_parse_opt(&argc, argv);
    if (ret < 0) {
        goto ERR;
    } if (ret == 1) {
        show_usage(argv[0]);
        goto ERR;
    }

    if (strcmp(connlm_revision(), CONNLM_GIT_COMMIT) != 0) {
        ST_WARNING("Binary revision[%s] not match with library[%s].",
                CONNLM_GIT_COMMIT, connlm_revision());
    }

    if (argc < 2) {
        show_usage(argv[0]);
        goto ERR;
    }

    if (! st_opt_check(g_cmd_opt)) {
        show_usage(argv[0]);
        goto ERR;
    }

    st_opt_show(g_cmd_opt, "Bloom Filter Info Options");

    for (i = 1; i < argc; i++) {
        fp = st_fopen(argv[i], "rb");
        if (fp == NULL) {
            ST_ERROR("Failed to st_fopen. [%s]", argv[i]);
            goto ERR;
        }

        fprintf(stdout, "\nModel \"%s\":\n", argv[i]);
        if (bloom_filter_print_info(fp, stdout) < 0) {
            ST_ERROR("Failed to bloom_filter_print_info. [%s]", argv[i]);
            goto ERR;
        }

        if (g_verbose) {
            rewind(fp);

            blm_flt = bloom_filter_load(fp);
            if (blm_flt == NULL) {
                ST_ERROR("Failed to bloom_filter_load. [%s]", argv[i]);
                goto ERR;
            }

            fprintf(stdout, "\n<VERBOSE>\n");
            fprintf(stdout, "Load factor: %.3f\n",
                    bloom_filter_load_factor(blm_flt));
        }

        safe_st_fclose(fp);
    }

    safe_bloom_filter_destroy(blm_flt);
    safe_st_opt_destroy(g_cmd_opt);

    st_log_close(0);
    return 0;

ERR:
    safe_st_fclose(fp);

    safe_bloom_filter_destroy(blm_flt);
    safe_st_opt_destroy(g_cmd_opt);

    st_log_close(1);
    return -1;
}
