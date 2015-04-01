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

#include <st_io.h>

#include "utils.h"
#include "connlm.h"

#define DEFAULT_LOGFILE         "-"
#define DEFAULT_LOGLEVEL        8

char g_log_file[MAX_DIR_LEN];
int  g_log_level;
bool g_binary;

st_opt_t *g_cmd_opt;

connlm_t *g_connlm;
vocab_opt_t g_vocab_opt;
vocab_t *g_vocab;

int connlm_parse_opt(int *argc, const char *argv[])
{
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

    ST_OPT_GET_STR(g_cmd_opt, "LOG_FILE",
            g_log_file, MAX_DIR_LEN, DEFAULT_LOGFILE, "Log file");
    ST_OPT_GET_INT(g_cmd_opt, "LOG_LEVEL", g_log_level,
                     DEFAULT_LOGLEVEL, "Log level (1-8)");

    if (st_log_open(g_log_file, g_log_level) != 0) {
        ST_WARNING("Failed to open log connlm-vocab");
        goto ST_OPT_ERR;
    }

    if (vocab_load_opt(&g_vocab_opt, g_cmd_opt, NULL) < 0) {
        ST_WARNING("Failed to vocab_load_opt");
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
    fprintf(stderr, "\nConnectionist Language Modelling Toolkit\n");
    fprintf(stderr, "  -- Learn Vocabulary\n");
    fprintf(stderr, "Version  : %s", CONNLM_VERSION);
    fprintf(stderr, "File version: %d\n", CONNLM_FILE_VERSION);
    fprintf(stderr, "Usage    : %s [options] <train-file> <model-out>\n",
            module_name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options  : \n");
    st_opt_show_usage(g_cmd_opt, stderr);
}

int main(int argc, const char *argv[])
{
    char filename[MAX_DIR_LEN];
    FILE *fp = NULL;
    int ret;

    ret = connlm_parse_opt(&argc, argv);
    if (ret < 0) {
        show_usage(argv[0]);
        goto ERR;
    } if (ret == 1) {
        show_usage(argv[0]);
        goto ERR;
    }

    if (argc != 3) {
        show_usage(argv[0]);
        goto ERR;
    }

    st_opt_show(g_cmd_opt, "connLM Vocab Options");

    g_vocab = vocab_create(&g_vocab_opt);
    if (g_vocab == NULL) {
        ST_WARNING("Failed to vocab_create.");
        goto ERR;
    }

    fp = st_fopen(argv[1], "rb");
    if (fp == NULL) {
        ST_WARNING("Failed to st_fopen. [%s]", filename);
        goto ERR;
    }

    if (vocab_learn(g_vocab, fp) < 0) {
        ST_WARNING("Failed to vocab_learn.");
        goto ERR;
    }
    safe_st_fclose(fp);

    g_connlm = connlm_new(g_vocab, NULL, NULL, NULL, NULL);
    if (g_connlm == NULL) {
        ST_WARNING("Failed to connlm_new.");
        goto ERR;
    }

    fp = st_fopen(argv[2], "wb");
    if (fp == NULL) {
        ST_WARNING("Failed to st_fopen. [%s]", argv[2]);
        goto ERR;
    }

    if (connlm_save(g_connlm, fp, g_binary) < 0) {
        ST_WARNING("Failed to connlm_save.");
        goto ERR;
    }

    safe_st_fclose(fp);
    safe_st_opt_destroy(g_cmd_opt);
    safe_vocab_destroy(g_vocab);
    safe_connlm_destroy(g_connlm);

    st_log_close(0);
    return 0;

  ERR:
    safe_st_fclose(fp);
    safe_st_opt_destroy(g_cmd_opt);
    safe_vocab_destroy(g_vocab);
    safe_connlm_destroy(g_connlm);

    st_log_close(1);
    return -1;
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */