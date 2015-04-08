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

#include <st_opt.h>
#include <st_log.h>
#include <st_io.h>

#include "utils.h"
#include "connlm.h"

bool g_binary;
bool g_dry_run;

st_opt_t *g_cmd_opt;

connlm_train_opt_t g_train_opt;
connlm_t *g_connlm;

int connlm_train_parse_opt(int *argc, const char *argv[])
{
    st_log_opt_t log_opt;
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

    if (connlm_load_train_opt(&g_train_opt, g_cmd_opt, NULL) < 0) {
        ST_WARNING("Failed to connlm_load_train_opt");
        goto ST_OPT_ERR;
    }

    ST_OPT_SEC_GET_BOOL(g_cmd_opt, NULL, "BINARY", g_binary, true,
            "Save file as binary format");

    ST_OPT_SEC_GET_BOOL(g_cmd_opt, NULL, "DRY_RUN", g_dry_run, false,
            "Read config and exit");

    if (st_opt_get_bool(g_cmd_opt, NULL, "help", &b, false,
                "Print help") < 0) {
        ST_WARNING("Failed to st_opt_get_bool for help");
        goto ST_OPT_ERR;
    }

    return (b ? 1 : 0);

ST_OPT_ERR:
    return -1;
}

void show_usage(const char *module_name)
{
    connlm_show_usage(module_name,
            "Train Model",
            "<train-file> <model-in> <model-out>",
            g_cmd_opt);
}

int main(int argc, const char *argv[])
{
    FILE *fp = NULL;
    int ret;

    ret = connlm_train_parse_opt(&argc, argv);
    if (ret < 0) {
        goto ERR;
    } if (ret == 1) {
        show_usage(argv[0]);
        goto ERR;
    }

    if (g_dry_run) {
        st_opt_show(g_cmd_opt, "connLM Train Options");
        return 0;
    }

    if (argc != 4) {
        show_usage(argv[0]);
        goto ERR;
    }

    st_opt_show(g_cmd_opt, "connLM Train Options");
    ST_CLEAN("Train: %s, Model-in: %s, Model-out: %s", 
            argv[1], argv[2], argv[3]);

    fp = st_fopen(argv[2], "rb");
    if (fp == NULL) {
        ST_WARNING("Failed to st_fopen. [%s]", argv[2]);
        goto ERR;
    }

    g_connlm = connlm_load(fp);
    if (g_connlm == NULL) {
        ST_WARNING("Failed to connlm_load. [%s]", argv[2]);
        goto ERR;
    }
    safe_st_fclose(fp);

    if (connlm_setup_train(g_connlm, &g_train_opt, argv[1]) < 0) {
        ST_WARNING("Failed to connlm_setup_train.");
        goto ERR;
    }

    if (connlm_train(g_connlm) < 0) {
        ST_WARNING("Failed to connlm_train.");
        goto ERR;
    }

    fp = st_fopen(argv[3], "wb");
    if (fp == NULL) {
        ST_WARNING("Failed to st_fopen. [%s]", argv[3]);
        goto ERR;
    }

    if (connlm_save(g_connlm, fp, g_binary) < 0) {
        ST_WARNING("Failed to connlm_save. [%s]", argv[3]);
        goto ERR;
    }
    safe_st_fclose(fp);

    safe_st_opt_destroy(g_cmd_opt);
    safe_connlm_destroy(g_connlm);

    st_log_close(0);
    return 0;

ERR:
    safe_st_fclose(fp);
    safe_st_opt_destroy(g_cmd_opt);
    safe_connlm_destroy(g_connlm);

    st_log_close(1);
    return -1;
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
