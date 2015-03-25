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

#include "utils.h"
#include "connlm.h"

#define DEFAULT_LOGFILE         "-"
#define DEFAULT_LOGLEVEL        8

typedef struct _connlm_conf_t_ {
    char log_file[MAX_DIR_LEN];
    int  log_level;
} connlm_conf_t;

st_opt_t *g_cmd_opt;

connlm_conf_t g_connlm_conf;
connlm_opt_t g_connlm_opt;
connlm_t *g_connlm;

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
            g_connlm_conf.log_file, MAX_DIR_LEN, DEFAULT_LOGFILE,
            "Log file");
    ST_OPT_GET_INT(g_cmd_opt, "LOG_LEVEL", g_connlm_conf.log_level,
                     DEFAULT_LOGLEVEL, "Log level (1-8)");

    if (st_log_open_mt(g_connlm_conf.log_file,
                g_connlm_conf.log_level) != 0) {
        ST_WARNING("Failed to open log connlm");
        goto ST_OPT_ERR;
    }

    if (connlm_load_opt(&g_connlm_opt, g_cmd_opt, NULL) < 0) {
        ST_WARNING("Failed to connlm_load_opt");
        goto ST_OPT_ERR;
    }

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
    fprintf(stderr, "\nConnectionist Language Modelling Training Tool\n");
    fprintf(stderr, "Version  : %s\n", VERSION);
    fprintf(stderr, "Usage    : %s [options] <train-file> <valid-file> <model-in> <model-out>\n",
            module_name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options  : \n");
    st_opt_show_usage(g_cmd_opt, stderr);


#if _MAT_X_VEC_DEBUG_
#define M 3
#define N 2
    real_t *mat = NULL;
    real_t *vec = NULL;
    real_t *dst = NULL;
    int i;

    mat = (real_t *)malloc(sizeof(real_t)*M*N);
    assert(mat != NULL);

    vec = (real_t *)malloc(sizeof(real_t)*N);
    assert(vec != NULL);

    dst = (real_t *)malloc(sizeof(real_t)*M);
    assert(dst != NULL);

    for (i = 0; i < N; i++) {
        vec[i] = (i + 1);
    }

    for (i = 0; i < M*N; i++) {
        mat[i] = (i + 1);
    }

    matXvec(dst, mat, vec, M, N);

    safe_free(mat);
    safe_free(vec);
    safe_free(dst);

    vec = (real_t *)malloc(sizeof(real_t)*M);
    assert(vec != NULL);

    mat = (real_t *)malloc(sizeof(real_t)*M*N);
    assert(mat != NULL);

    dst = (real_t *)malloc(sizeof(real_t)*N);
    assert(dst != NULL);

    for (i = 0; i < M; i++) {
        vec[i] = (i + 1);
    }

    for (i = 0; i < N; i++) {
        dst[i] = 0;
    }

    for (i = 0; i < M*N; i++) {
        mat[i] = (i + 1);
    }

    vecXmat(dst, vec, mat, N, M);

    safe_free(mat);
    safe_free(vec);
    safe_free(dst);
#endif
}

int main(int argc, const char *argv[])
{
    int ret;

    ret = connlm_parse_opt(&argc, argv);
    if (ret < 0) {
        show_usage(argv[0]);
        goto ERR;
    } if (ret == 1) {
        show_usage(argv[0]);
        goto ERR;
    }

    st_opt_show(g_cmd_opt, "connLM Train Config");
    if (argc != 5) {
        show_usage(argv[0]);
        goto ERR;
    }

    st_opt_show(g_cmd_opt, "connLM Train Config");

    g_connlm = connlm_create(&g_connlm_opt);
    if (g_connlm == NULL) {
        ST_WARNING("Failed to connlm_create.");
        goto ERR;
    }

    if (g_connlm_opt.mode == MODE_TRAIN) {
        if (connlm_train(g_connlm) < 0) {
            ST_WARNING("Failed to connlm_train.");
            goto ERR;
        }
    } else {
        if (connlm_test(g_connlm) < 0) {
            ST_WARNING("Failed to connlm_test.");
            goto ERR;
        }
    }

    safe_st_opt_destroy(g_cmd_opt);
    safe_connlm_destroy(g_connlm);

    st_log_close(0);
    return 0;

  ERR:
    safe_st_opt_destroy(g_cmd_opt);
    safe_connlm_destroy(g_connlm);

    st_log_close(1);
    return -1;
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
