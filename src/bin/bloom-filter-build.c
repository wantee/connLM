/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Wang Jian
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <stutils/st_opt.h>
#include <stutils/st_log.h>
#include <stutils/st_string.h>
#include <stutils/st_io.h>
#include <stutils/st_mem.h>

#include <connlm/utils.h>
#include <connlm/connlm.h>
#include <connlm/bloom_filter.h>

bloom_filter_format_t g_fmt;

st_opt_t *g_cmd_opt;
bloom_filter_opt_t g_blm_flt_opt;

int bloom_filter_parse_opt(int *argc, const char *argv[])
{
    st_log_opt_t log_opt;

    char str[MAX_ST_CONF_LEN];
    bool b;

    g_cmd_opt = st_opt_create();
    if (g_cmd_opt == NULL) {
        ST_WARNING("Failed to st_opt_create.");
        goto ST_OPT_ERR;
    }

    if (st_opt_parse(g_cmd_opt, argc, argv) < 0) {
        ST_WARNING("Failed to st_opt_parse.");
        goto ST_OPT_ERR;
    }

    if (st_log_load_opt(&log_opt, g_cmd_opt, NULL) < 0) {
        ST_WARNING("Failed to st_log_load_opt");
        goto ST_OPT_ERR;
    }

    if (st_log_open_mt(&log_opt) != 0) {
        ST_WARNING("Failed to open log");
        goto ST_OPT_ERR;
    }

    if (bloom_filter_load_opt(&g_blm_flt_opt, g_cmd_opt, NULL) < 0) {
        ST_WARNING("Failed to bloom_filter_load_opt");
        goto ST_OPT_ERR;
    }

    ST_OPT_GET_STR(g_cmd_opt, "FORMAT", str, MAX_ST_CONF_LEN, "Compress",
            "storage format(Txt/Bin/Compress)");
    g_fmt = bloom_filter_format_parse(str);
    if (g_fmt == BF_FMT_UNKNOWN) {
        ST_WARNING("Unknown format[%s]", str);
        goto ST_OPT_ERR;
    }

    ST_OPT_GET_BOOL(g_cmd_opt, "help", b, false, "Print help");

    return (b ? 1 : 0);

ST_OPT_ERR:
    return -1;
}

void show_usage(const char *module_name)
{
    connlm_show_usage(module_name,
            "Build N-gram Bloom Filter",
            "<vocab> <text-file> <model-out>",
            "exp/vocab.clm data/train exp/train.blmflt",
            g_cmd_opt, NULL);
}

int main(int argc, const char *argv[])
{
    char args[1024] = "";
    FILE *fp = NULL;
    connlm_t *connlm = NULL;
    bloom_filter_t *blm_flt = NULL;
    int ret;

    if (st_mem_usage_init() < 0) {
        ST_WARNING("Failed to st_mem_usage_init.");
        goto ERR;
    }

    (void)st_escape_args(argc, argv, args, 1024);

    ret = bloom_filter_parse_opt(&argc, argv);
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

    if (argc < 4) {
        show_usage(argv[0]);
        goto ERR;
    }

    if (! st_opt_check(g_cmd_opt)) {
        show_usage(argv[0]);
        goto ERR;
    }

    ST_CLEAN("Command-line: %s", args);
    st_opt_show(g_cmd_opt, "Bloom Filter Build Options");
    ST_CLEAN("Vocab: '%s', Text: '%s', Model-out: '%s'", argv[1], argv[2], argv[3]);

    fp = st_fopen(argv[1], "rb");
    if (fp == NULL) {
        ST_WARNING("Failed to st_fopen vocab[%s]", argv[1]);
        goto ERR;
    }

    connlm = connlm_load(fp);
    if (connlm == NULL) {
        ST_WARNING("Failed to connlm_load from [%s]", argv[1]);
        goto ERR;
    }
    safe_st_fclose(fp);

    blm_flt = bloom_filter_create(&g_blm_flt_opt, connlm->vocab);
    if (blm_flt == NULL) {
        ST_WARNING("Failed to bloom_filter_create.");
        goto ERR;
    }

    safe_connlm_destroy(connlm);

    fp = st_fopen(argv[2], "rb");
    if (fp == NULL) {
        ST_WARNING("Failed to st_fopen text file[%s].", argv[2]);
        goto ERR;
    }

    if (bloom_filter_build(blm_flt, fp) < 0) {
        ST_WARNING("Failed to bloom_filter_build.");
        goto ERR;
    }
    safe_st_fclose(fp);

    fp = st_fopen(argv[3], "wb");
    if (fp == NULL) {
        ST_WARNING("Failed to st_fopen out model file[%s]", argv[3]);
        goto ERR;
    }

    if (bloom_filter_save(blm_flt, fp, g_fmt) < 0) {
        ST_WARNING("Failed to bloom_filter_save.");
        goto ERR;
    }

    safe_st_fclose(fp);

    safe_st_opt_destroy(g_cmd_opt);
    safe_bloom_filter_destroy(blm_flt);

    st_mem_usage_report();
    st_mem_usage_destroy();
    st_log_close(0);

    return 0;

ERR:
    safe_st_fclose(fp);
    safe_connlm_destroy(connlm);

    safe_st_opt_destroy(g_cmd_opt);
    safe_bloom_filter_destroy(blm_flt);

    st_mem_usage_destroy();
    st_log_close(1);
    return -1;
}
