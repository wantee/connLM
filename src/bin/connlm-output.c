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
#include <stutils/st_string.h>
#include <stutils/st_mem.h>

#include <connlm/utils.h>
#include <connlm/connlm.h>

connlm_fmt_t g_fmt;

st_opt_t *g_cmd_opt;

output_opt_t g_output_opt;

int connlm_output_parse_opt(int *argc, const char *argv[])
{
    st_log_opt_t log_opt;

    char str[MAX_ST_CONF_LEN];
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

    if (output_load_opt(&g_output_opt, g_cmd_opt, NULL) < 0) {
        ST_ERROR("Failed to output_load_opt");
        goto ST_OPT_ERR;
    }

    ST_OPT_GET_STR(g_cmd_opt, "FORMAT", str, MAX_ST_CONF_LEN, "Bin",
            "storage format(Txt/Bin/Zeros-Compress/Short-Q)");
    g_fmt = connlm_format_parse(str);
    if (g_fmt == CONN_FMT_UNKNOWN) {
        ST_ERROR("Unknown format[%s]", str);
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
            "Initialise output layer",
            "<model-in> <model-out>",
            "exp/vocab.clm exp/output.clm",
            g_cmd_opt, NULL);
}

int main(int argc, const char *argv[])
{
    char args[1024] = "";

    FILE *fp = NULL;
    int ret;

    connlm_t *connlm_in = NULL;
    connlm_t *connlm_out = NULL;
    output_t *output = NULL;

    if (st_mem_usage_init() < 0) {
        ST_ERROR("Failed to st_mem_usage_init.");
        goto ERR;
    }

    (void)st_escape_args(argc, argv, args, 1024);

    ret = connlm_output_parse_opt(&argc, argv);
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

    if (argc != 3) {
        show_usage(argv[0]);
        goto ERR;
    }

    if (! st_opt_check(g_cmd_opt)) {
        show_usage(argv[0]);
        goto ERR;
    }

    ST_CLEAN("Command-line: %s", args);
    st_opt_show(g_cmd_opt, "connLM Output Options");
    ST_CLEAN("Model-in: '%s', Model-out: '%s'", argv[1], argv[2]);

    ST_NOTICE("Loading vocab model..");
    fp = st_fopen(argv[1], "rb");
    if (fp == NULL) {
        ST_ERROR("Failed to st_fopen. [%s]", argv[1]);
        goto ERR;
    }

    connlm_in = connlm_load(fp);
    if (connlm_in == NULL) {
        ST_ERROR("Failed to connlm_load. [%s]", argv[1]);
        goto ERR;
    }
    safe_st_fclose(fp);

    if (connlm_in->vocab == NULL) {
        ST_ERROR("No vocab loaded from [%s].", argv[1]);
        goto ERR;
    }

    ST_NOTICE("Generating Output Layer...");
    output = output_generate(&g_output_opt, connlm_in->vocab->cnts,
            connlm_in->vocab->vocab_size - 1/* <s> */);
    if (output == NULL) {
        ST_ERROR("Failed to output_generate.");
        goto ERR;
    }

    connlm_out = connlm_new(connlm_in->vocab, output, NULL, -1);
    if (connlm_out == NULL) {
        ST_ERROR("Failed to connlm_new.");
        goto ERR;
    }

    fp = st_fopen(argv[2], "wb");
    if (fp == NULL) {
        ST_ERROR("Failed to st_fopen. [%s]", argv[2]);
        goto ERR;
    }

    if (connlm_save(connlm_out, fp, g_fmt) < 0) {
        ST_ERROR("Failed to connlm_save. [%s]", argv[2]);
        goto ERR;
    }

    safe_st_fclose(fp);
    safe_st_opt_destroy(g_cmd_opt);
    safe_output_destroy(output);
    safe_connlm_destroy(connlm_in);
    safe_connlm_destroy(connlm_out);

    st_mem_usage_report();
    st_mem_usage_destroy();
    st_log_close(0);
    return 0;

ERR:
    safe_st_fclose(fp);
    safe_st_opt_destroy(g_cmd_opt);
    safe_output_destroy(output);
    safe_connlm_destroy(connlm_in);
    safe_connlm_destroy(connlm_out);

    st_mem_usage_destroy();
    st_log_close(1);
    return -1;
}
