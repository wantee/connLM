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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <stutils/st_opt.h>
#include <stutils/st_log.h>
#include <stutils/st_io.h>
#include <stutils/st_string.h>
#include <stutils/st_mem.h>

#include <connlm/utils.h>
#include <connlm/connlm.h>
#include <connlm/reader.h>
#include <connlm/driver.h>

connlm_fmt_t g_fmt;
bool g_dry_run;
int g_num_thr;

st_opt_t *g_cmd_opt;

reader_opt_t g_reader_opt;
driver_train_opt_t g_train_opt;

int connlm_train_parse_opt(int *argc, const char *argv[])
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

    if (reader_load_opt(&g_reader_opt, g_cmd_opt, NULL) < 0) {
        ST_WARNING("Failed to reader_load_opt");
        goto ST_OPT_ERR;
    }

    if (driver_load_train_opt(&g_train_opt, g_cmd_opt, NULL) < 0) {
        ST_WARNING("Failed to driver_load_train_opt");
        goto ST_OPT_ERR;
    }

    ST_OPT_GET_INT(g_cmd_opt, "NUM_THREAD", g_num_thr, 1,
            "Number of working threads");

    ST_OPT_GET_STR(g_cmd_opt, "FORMAT", str, MAX_ST_CONF_LEN, "Bin",
            "storage format(Txt/Bin/Zeros-Compress/Short-Q)");
    g_fmt = connlm_format_parse(str);
    if (g_fmt == CONN_FMT_UNKNOWN) {
        ST_WARNING("Unknown format[%s]", str);
        goto ST_OPT_ERR;
    }

    ST_OPT_GET_BOOL(g_cmd_opt, "DRY_RUN", g_dry_run, false,
            "Read config and exit");

    ST_OPT_GET_BOOL(g_cmd_opt, "help", b, false, "Print help");

    return (b ? 1 : 0);

ST_OPT_ERR:
    return -1;
}

void show_usage(const char *module_name)
{
    connlm_show_usage(module_name,
            "Train Model",
            "<train-file> <model-in> <model-out>",
            "exp/init.clm data/train ext/01.clm",
            g_cmd_opt, NULL);
    param_show_usage();
    bptt_opt_show_usage();
    fprintf(stderr, "Individual parameter or bptt options can be set with "
            "component and/or glue name,\ne.g.:\n");
    fprintf(stderr, "  --comp-name.param "
            "set param/bptt_opt for a component.\n");
    fprintf(stderr, "  --comp-name.glue-name.param "
            "set param/bptt_opt for a glue.\n");
}

int main(int argc, const char *argv[])
{
    char args[1024] = "";
    FILE *fp = NULL;
    connlm_t *connlm = NULL;
    reader_t *reader = NULL;
    driver_t *driver = NULL;
    int ret;

    if (st_mem_usage_init() < 0) {
        ST_WARNING("Failed to st_mem_usage_init.");
        goto ERR;
    }

    (void)st_escape_args(argc, argv, args, 1024);

    ret = connlm_train_parse_opt(&argc, argv);
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

    if (g_dry_run) {
        if (argc != 2) {
            show_usage(argv[0]);
            goto ERR;
        }
    } else {
        if (argc != 4) {
            show_usage(argv[0]);
            goto ERR;
        }
    }

    ST_CLEAN("Command-line: %s", args);
    if (g_dry_run) {
        ST_CLEAN("Model-in: '%s'", argv[1]);
    } else {
        ST_CLEAN("Model-in: '%s', Train: '%s', Model-out: '%s'",
                argv[1], argv[2], argv[3]);
    }

    fp = st_fopen(argv[1], "rb");
    if (fp == NULL) {
        ST_WARNING("Failed to st_fopen. [%s]", argv[1]);
        goto ERR;
    }

    connlm = connlm_load(fp);
    if (connlm == NULL) {
        ST_WARNING("Failed to connlm_load. [%s]", argv[1]);
        goto ERR;
    }
    safe_st_fclose(fp);

    if (connlm_load_train_opt(connlm, g_cmd_opt, NULL) < 0) {
        ST_WARNING("Failed to connlm_load_train_opt");
        goto ERR;
    }

    st_opt_show(g_cmd_opt, "connLM Train Options");

    if (g_dry_run) {
        goto RET;
    }

    reader = reader_create(&g_reader_opt, g_num_thr, connlm->vocab, argv[2]);
    if (reader == NULL) {
        ST_WARNING("Failed to reader_create.");
        goto ERR;
    }

    driver = driver_create(connlm, reader, g_num_thr);
    if (driver == NULL) {
        ST_WARNING("Failed to driver_create.");
        goto ERR;
    }

    if (driver_set_train(driver, &g_train_opt) < 0) {
        ST_WARNING("Failed to driver_set_train.");
        goto ERR;
    }

    if (driver_setup(driver, DRIVER_TRAIN) < 0) {
        ST_WARNING("Failed to driver_setup.");
        goto ERR;
    }

    if (driver_run(driver) < 0) {
        ST_WARNING("Failed to driver_run.");
        goto ERR;
    }

    safe_driver_destroy(driver);
    safe_reader_destroy(reader);

    connlm_sanity_check(connlm);

    fp = st_fopen(argv[3], "wb");
    if (fp == NULL) {
        ST_WARNING("Failed to st_fopen. [%s]", argv[3]);
        goto ERR;
    }

    if (connlm_save(connlm, fp, g_fmt) < 0) {
        ST_WARNING("Failed to connlm_save. [%s]", argv[3]);
        goto ERR;
    }

RET:
    safe_st_fclose(fp);

    safe_st_opt_destroy(g_cmd_opt);
    safe_connlm_destroy(connlm);

    st_mem_usage_report();
    st_mem_usage_destroy();
    st_log_close(0);
    return 0;

ERR:
    safe_st_opt_destroy(g_cmd_opt);
    safe_st_fclose(fp);
    safe_driver_destroy(driver);
    safe_reader_destroy(reader);
    safe_connlm_destroy(connlm);

    st_mem_usage_destroy();
    st_log_close(1);
    return -1;
}
