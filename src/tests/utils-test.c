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
#include <string.h>

#include "utils.h"

#define M 3
#define N 2

static int unit_test_matXvec()
{
    real_t mat[M*N];
    real_t vec[N];
    real_t dst[M];

    real_t ref[M] = {10, 22, 34};

    int i;

    fprintf(stderr, " Testing matXvec...");

    for (i = 0; i < N; i++) {
        vec[i] = (i + 1);
    }

    for (i = 0; i < M; i++) {
        dst[i] = 0;
    }

    for (i = 0; i < M*N; i++) {
        mat[i] = (i + 1);
    }

    matXvec(dst, mat, vec, M, N, 2.0);

    for (i = 0; i < M; i++) {
        if (dst[i] != ref[i]) {
            goto FAILED;
        }
    }

    fprintf(stderr, "Passed\n");
    return 0;

FAILED:
    fprintf(stderr, "Failed\n");
    return -1;
}

static int unit_test_vecXMat()
{
    real_t mat[M*N];
    real_t vec[M];
    real_t dst[N];

    real_t ref[N] = {22, 28};

    int i;

    fprintf(stderr, " Testing vecXmat...");

    for (i = 0; i < M; i++) {
        vec[i] = (i + 1);
    }

    for (i = 0; i < N; i++) {
        dst[i] = 0;
    }

    for (i = 0; i < M*N; i++) {
        mat[i] = (i + 1);
    }

    vecXmat(dst, vec, mat, N, M, 1.0);

    for (i = 0; i < N; i++) {
        if (dst[i] != ref[i]) {
            goto FAILED;
        }
    }

    fprintf(stderr, "Passed\n");
    return 0;

FAILED:
    fprintf(stderr, "Failed\n");
    return -1;
}

static int unit_test_dot_product()
{
    real_t v1[M];
    real_t v2[M];
    real_t ret;
    real_t ref = 16;

    int i;

    fprintf(stderr, " Testing dot_product...");

    for (i = 0; i < M; i++) {
        v1[i] = (i + 1);
    }

    for (i = 0; i < M; i++) {
        v2[i] = i * 2;
    }

    ret = dot_product(v1, v2, M);

    fprintf(stderr, "%g\n", ret);
    if (ret != ref) {
        goto FAILED;
    }

    fprintf(stderr, "Passed\n");
    return 0;

FAILED:
    fprintf(stderr, "Failed\n");
    return -1;
}

static int unit_test_parse_model_filter()
{
    char in[MAX_DIR_LEN];
    char out[MAX_DIR_LEN];
    model_filter_t mf;

    int ncase;

    fprintf(stderr, " Testing parse_model_filter...\n");

    ncase = 1;
    fprintf(stderr, "    Case %d...", ncase++);
    strncpy(in, "model1.clm", MAX_DIR_LEN);
    in[MAX_DIR_LEN - 1] = '\0';
    mf = parse_model_filter(in, out, MAX_DIR_LEN);
    if (mf != MF_ALL || strcmp(in, out) != 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    strncpy(in, "mdl,model1.clm", MAX_DIR_LEN);
    in[MAX_DIR_LEN - 1] = '\0';
    mf = parse_model_filter(in, out, MAX_DIR_LEN);
    if (mf != MF_ALL || strcmp(in, out) != 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    strncpy(in, "mdl,oxt:model1.clm", MAX_DIR_LEN);
    in[MAX_DIR_LEN - 1] = '\0';
    mf = parse_model_filter(in, out, MAX_DIR_LEN);
    if (mf != MF_ALL || strcmp(in, out) != 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    strncpy(in, "mdl,o:model1.clm", MAX_DIR_LEN);
    in[MAX_DIR_LEN - 1] = '\0';
    mf = parse_model_filter(in, out, MAX_DIR_LEN);
    if ((strcmp(out, "model1.clm") != 0)
            || (!(mf & MF_OUTPUT))
            || (mf & ~MF_OUTPUT)) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    strncpy(in, "mdl,v:model1.clm", MAX_DIR_LEN);
    in[MAX_DIR_LEN - 1] = '\0';
    mf = parse_model_filter(in, out, MAX_DIR_LEN);
    if ((strcmp(out, "model1.clm") != 0)
            || (!(mf & MF_VOCAB))
            || (mf & ~MF_VOCAB)) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    strncpy(in, "mdl,m:model1.clm", MAX_DIR_LEN);
    in[MAX_DIR_LEN - 1] = '\0';
    mf = parse_model_filter(in, out, MAX_DIR_LEN);
    if ((strcmp(out, "model1.clm") != 0)
            || (!(mf & MF_MAXENT))
            || (mf & ~MF_MAXENT)) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    strncpy(in, "mdl,r:model1.clm", MAX_DIR_LEN);
    in[MAX_DIR_LEN - 1] = '\0';
    mf = parse_model_filter(in, out, MAX_DIR_LEN);
    if ((strcmp(out, "model1.clm") != 0)
            || (!(mf & MF_RNN))
            || (mf & ~MF_RNN)) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    strncpy(in, "mdl,l:model1.clm", MAX_DIR_LEN);
    in[MAX_DIR_LEN - 1] = '\0';
    mf = parse_model_filter(in, out, MAX_DIR_LEN);
    if ((strcmp(out, "model1.clm") != 0)
            || (!(mf & MF_LBL))
            || (mf & ~MF_LBL)) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    strncpy(in, "mdl,f:model1.clm", MAX_DIR_LEN);
    in[MAX_DIR_LEN - 1] = '\0';
    mf = parse_model_filter(in, out, MAX_DIR_LEN);
    if ((strcmp(out, "model1.clm") != 0)
            || (!(mf & MF_FFNN))
            || (mf & ~MF_FFNN)) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    strncpy(in, "mdl,-o:model1.clm", MAX_DIR_LEN);
    in[MAX_DIR_LEN - 1] = '\0';
    mf = parse_model_filter(in, out, MAX_DIR_LEN);
    if ((strcmp(out, "model1.clm") != 0)
            || (mf & MF_OUTPUT)
            || !(mf & MF_VOCAB)
            || !(mf & MF_MAXENT)
            || !(mf & MF_RNN)
            || !(mf & MF_LBL)
            || !(mf & MF_FFNN)) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    strncpy(in, "mdl,-v:model1.clm", MAX_DIR_LEN);
    in[MAX_DIR_LEN - 1] = '\0';
    mf = parse_model_filter(in, out, MAX_DIR_LEN);
    if ((strcmp(out, "model1.clm") != 0)
            || !(mf & MF_OUTPUT)
            || (mf & MF_VOCAB)
            || !(mf & MF_MAXENT)
            || !(mf & MF_RNN)
            || !(mf & MF_LBL)
            || !(mf & MF_FFNN)) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    strncpy(in, "mdl,-m:model1.clm", MAX_DIR_LEN);
    in[MAX_DIR_LEN - 1] = '\0';
    mf = parse_model_filter(in, out, MAX_DIR_LEN);
    if ((strcmp(out, "model1.clm") != 0)
            || !(mf & MF_OUTPUT)
            || !(mf & MF_VOCAB)
            || (mf & MF_MAXENT)
            || !(mf & MF_RNN)
            || !(mf & MF_LBL)
            || !(mf & MF_FFNN)) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    strncpy(in, "mdl,-r:model1.clm", MAX_DIR_LEN);
    in[MAX_DIR_LEN - 1] = '\0';
    mf = parse_model_filter(in, out, MAX_DIR_LEN);
    if ((strcmp(out, "model1.clm") != 0)
            || !(mf & MF_OUTPUT)
            || !(mf & MF_VOCAB)
            || !(mf & MF_MAXENT)
            || (mf & MF_RNN)
            || !(mf & MF_LBL)
            || !(mf & MF_FFNN)) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    strncpy(in, "mdl,-l:model1.clm", MAX_DIR_LEN);
    in[MAX_DIR_LEN - 1] = '\0';
    mf = parse_model_filter(in, out, MAX_DIR_LEN);
    if ((strcmp(out, "model1.clm") != 0)
            || !(mf & MF_OUTPUT)
            || !(mf & MF_VOCAB)
            || !(mf & MF_MAXENT)
            || !(mf & MF_RNN)
            || (mf & MF_LBL)
            || !(mf & MF_FFNN)) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    strncpy(in, "mdl,-f:model1.clm", MAX_DIR_LEN);
    in[MAX_DIR_LEN - 1] = '\0';
    mf = parse_model_filter(in, out, MAX_DIR_LEN);
    if ((strcmp(out, "model1.clm") != 0)
            || !(mf & MF_OUTPUT)
            || !(mf & MF_VOCAB)
            || !(mf & MF_MAXENT)
            || !(mf & MF_RNN)
            || !(mf & MF_LBL)
            || (mf & MF_FFNN)) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    strncpy(in, "mdl,fv:model1.clm", MAX_DIR_LEN);
    in[MAX_DIR_LEN - 1] = '\0';
    mf = parse_model_filter(in, out, MAX_DIR_LEN);
    if ((strcmp(out, "model1.clm") != 0)
            || (mf & MF_OUTPUT)
            || !(mf & MF_VOCAB)
            || (mf & MF_MAXENT)
            || (mf & MF_RNN)
            || (mf & MF_LBL)
            || !(mf & MF_FFNN)) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    strncpy(in, "mdl,olr:model1.clm", MAX_DIR_LEN);
    in[MAX_DIR_LEN - 1] = '\0';
    mf = parse_model_filter(in, out, MAX_DIR_LEN);
    if ((strcmp(out, "model1.clm") != 0)
            || !(mf & MF_OUTPUT)
            || (mf & MF_VOCAB)
            || (mf & MF_MAXENT)
            || !(mf & MF_RNN)
            || !(mf & MF_LBL)
            || (mf & MF_FFNN)) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    strncpy(in, "mdl,-rl:model1.clm", MAX_DIR_LEN);
    in[MAX_DIR_LEN - 1] = '\0';
    mf = parse_model_filter(in, out, MAX_DIR_LEN);
    if ((strcmp(out, "model1.clm") != 0)
            || !(mf & MF_OUTPUT)
            || !(mf & MF_VOCAB)
            || !(mf & MF_MAXENT)
            || (mf & MF_RNN)
            || (mf & MF_LBL)
            || !(mf & MF_FFNN)) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    strncpy(in, "mdl,-vof:model1.clm", MAX_DIR_LEN);
    in[MAX_DIR_LEN - 1] = '\0';
    mf = parse_model_filter(in, out, MAX_DIR_LEN);
    if ((strcmp(out, "model1.clm") != 0)
            || (mf & MF_OUTPUT)
            || (mf & MF_VOCAB)
            || !(mf & MF_MAXENT)
            || !(mf & MF_RNN)
            || !(mf & MF_LBL)
            || (mf & MF_FFNN)) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    strncpy(in, "mdl,vofmlr:model1.clm", MAX_DIR_LEN);
    in[MAX_DIR_LEN - 1] = '\0';
    mf = parse_model_filter(in, out, MAX_DIR_LEN);
    if ((strcmp(out, "model1.clm") != 0)
            || (mf != MF_ALL)) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    strncpy(in, "mdl,-vofmlr:model1.clm", MAX_DIR_LEN);
    in[MAX_DIR_LEN - 1] = '\0';
    mf = parse_model_filter(in, out, MAX_DIR_LEN);
    if ((strcmp(out, "model1.clm") != 0)
            || (mf != MF_NONE)) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    return 0;
}

static int run_all_tests()
{
    int ret = 0;

    if (unit_test_matXvec() != 0) {
        ret = -1;
    }

    if (unit_test_vecXMat() != 0) {
        ret = -1;
    }

    if (unit_test_dot_product() != 0) {
        ret = -1;
    }

    if (unit_test_parse_model_filter() != 0) {
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
