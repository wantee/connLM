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

#include <stconf.h>
#include <stlog.h>

#include "utils.h"
#include "connlm.h"

#define DEFAULT_CONF_FILE       "./conf/connlm.conf"
#define DEFAULT_LOGFILE         "./log/connlm.log"
#define DEFAULT_LOGLEVEL        8

typedef enum _connlm_mode_t_ {
    MODE_TRAIN,
    MODE_TEST,
} connlm_mode_t;

typedef struct _connlm_conf_t_ {
    char log_file[MAX_DIR_LEN];
    int  log_level;
    int  mode;
} connlm_conf_t;

typedef struct _connlm_train_conf_t {
    connlm_opt_t connlm_opt;
} connlm_train_conf_t;

typedef struct _connlm_test_conf_t {
    char test_file[MAX_DIR_LEN];

    char model_file[MAX_DIR_LEN];
} connlm_test_conf_t;

char g_connlm_conf_file[MAX_DIR_LEN];
connlm_conf_t g_connlm_conf;
connlm_train_conf_t g_connlm_train_conf;
connlm_test_conf_t g_connlm_test_conf;
connlm_t *g_connlm;

int connlm_load_conf()
{
    stconf_t *pconf = NULL;
    char s[MAX_STCONF_LEN];

    if (g_connlm_conf_file[0] == '\0') {
        snprintf(g_connlm_conf_file, MAX_DIR_LEN, "%s", DEFAULT_CONF_FILE);
    }

    pconf = init_stconf(g_connlm_conf_file);
    if (NULL == pconf) {
        ST_WARNING("Failed to init log configure structure.");
        return -1;
    }

    GET_STR_CONF_DEF(pconf, "LOG_FILE",
            g_connlm_conf.log_file, MAX_DIR_LEN, DEFAULT_LOGFILE);
    GET_INT_CONF_DEF(pconf, "LOG_LEVEL", g_connlm_conf.log_level,
                     DEFAULT_LOGLEVEL);

    if (st_openlog(g_connlm_conf.log_file,
                g_connlm_conf.log_level) != 0) {
        ST_WARNING("Failed to open log connlm");
        goto STCONF_ERR;
    }

    GET_STR_CONF(pconf, "MODE", s, MAX_STCONF_LEN);
    if (strncasecmp(s, "Train", 6) == 0) {
        g_connlm_conf.mode = MODE_TRAIN;
    } else if (strncasecmp(s, "Test", 5) == 0) {
        g_connlm_conf.mode = MODE_TEST;
    } else {
        ST_WARNING("Unkown mode[%s]", s);
        goto STCONF_ERR;
    }

    if (g_connlm_conf.mode == MODE_TRAIN) {
        if (connlm_load_opt(&g_connlm_train_conf.connlm_opt, 
                    pconf, "TRAIN") < 0) {
            ST_WARNING("Failed to connlm_load_opt");
            goto STCONF_ERR;
        }
    } else if (g_connlm_conf.mode == MODE_TEST){
        GET_STR_SEC_CONF(pconf, "TEST", "MODEL_FILE",
                g_connlm_test_conf.model_file, MAX_DIR_LEN);
        GET_STR_SEC_CONF(pconf, "TEST", "TEST_FILE",
                g_connlm_test_conf.test_file, MAX_DIR_LEN);
    }

    show_stconf(pconf, "connLM Config");
    free_stconf(pconf);

    return 0;

STCONF_ERR:
    free_stconf(pconf);
    return -1;
}

int init_connlm()
{
    if (connlm_load_conf() < 0) {
        ST_WARNING("Failed to load conf from file[%s].",
                g_connlm_conf_file);
        goto FAILED;
    }

    g_connlm = connlm_create(&g_connlm_train_conf.connlm_opt);
    if (g_connlm == NULL) {
        ST_WARNING("Failed to connlm_create.");
        goto FAILED;
    }

    return 0;

  FAILED:
    safe_connlm_destroy(g_connlm);
    return -1;
}

void show_usage(char *module_name)
{
    fprintf(stderr, "Connectionist Language Modelling Toolkit\n");
    fprintf(stderr, "Version    : %s\n", VERSION);
    fprintf(stderr, "Usage: %s [-h] \n", module_name);
    fprintf(stderr, "\t-c\tconfig file, default is %s.\n", DEFAULT_CONF_FILE);
    fprintf(stderr, "\t-h\tshow help and version, i.e. this page\n");


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

int main(int argc, char *argv[])
{
    int c;

    while ((c = getopt(argc, argv, "c:hv")) != -1) {
        switch (c) {
            case 'c':
                strncpy(g_connlm_conf_file, optarg, MAX_DIR_LEN);
                g_connlm_conf_file[MAX_DIR_LEN - 1] = 0;
                break;
            case 'v':
            case 'h':
            case '?':
                show_usage(argv[0]);
                exit(-1);
        }
    }

    if (init_connlm() < 0) {
        ST_WARNING("Failed to init_connlm.");
        goto ERR;
    }

    if (g_connlm_conf.mode == MODE_TRAIN) {
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

    safe_connlm_destroy(g_connlm);

    st_closelog(0);
    return 0;

  ERR:
    safe_connlm_destroy(g_connlm);

    st_closelog(1);
    return -1;
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
