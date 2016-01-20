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

#include <st_log.h>
#include <st_io.h>

#include <connlm/utils.h>
#include <connlm/connlm.h>

bool g_binary;

st_opt_t *g_cmd_opt;

int connlm_copy_parse_opt(int *argc, const char *argv[])
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

    if (st_log_open(&log_opt) != 0) {
        ST_WARNING("Failed to open log");
        goto ST_OPT_ERR;
    }

    ST_OPT_SEC_GET_BOOL(g_cmd_opt, NULL, "BINARY", g_binary, true,
            "Save file as binary format");

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
            "Copy Models",
            "<model-in> <model-out-filter>",
            g_cmd_opt);
}

int main(int argc, const char *argv[])
{
    char fname[MAX_DIR_LEN];
    FILE *fp = NULL;
    connlm_t *connlm = NULL;
    model_filter_t mf;
    int ret;

    ret = connlm_copy_parse_opt(&argc, argv);
    if (ret < 0) {
        goto ERR;
    } if (ret == 1) {
        show_usage(argv[0]);
        goto ERR;
    }

    if (strcmp(connlm_revision(), CONNLM_COMMIT) != 0) {
        ST_WARNING("Binary revision[%s] not match with library[%s].",
                CONNLM_COMMIT, connlm_revision());
    }

    if (argc != 3) {
        show_usage(argv[0]);
        goto ERR;
    }

    st_opt_show(g_cmd_opt, "connLM Copy Options");
    ST_CLEAN("Model-in: %s, Model-out: %s", argv[1], argv[2]);

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

    mf = parse_model_filter(argv[2], fname, MAX_DIR_LEN);
    if (mf == MF_NONE) {
        ST_WARNING("Failed to parse_model_filter.");
        goto ERR;
    }

    fp = st_fopen(fname, "wb");
    if (fp == NULL) {
        ST_WARNING("Failed to st_fopen. [%s]", fname);
        goto ERR;
    }

    if (!(mf & MF_OUTPUT)) {
        safe_output_destroy(connlm->output);
    }
    if (!(mf & MF_VOCAB)) {
        safe_vocab_destroy(connlm->vocab);
    }
    if (!(mf & MF_MAXENT)) {
        safe_maxent_destroy(connlm->maxent);
    }
    if (!(mf & MF_RNN)) {
        safe_rnn_destroy(connlm->rnn);
    }
    if (!(mf & MF_LBL)) {
        safe_lbl_destroy(connlm->lbl);
    }
    if (!(mf & MF_FFNN)) {
        safe_ffnn_destroy(connlm->ffnn);
    }
    if (connlm_save(connlm, fp, g_binary) < 0) {
        ST_WARNING("Failed to connlm_save. [%s]", fname);
        goto ERR;
    }

    safe_st_fclose(fp);
    safe_st_opt_destroy(g_cmd_opt);
    safe_connlm_destroy(connlm);

    st_log_close(0);
    return 0;

ERR:
    safe_st_fclose(fp);
    safe_st_opt_destroy(g_cmd_opt);
    safe_connlm_destroy(connlm);

    st_log_close(1);
    return -1;
}
