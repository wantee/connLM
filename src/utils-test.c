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

    fprintf(stderr, " Testing matXvec...\n");

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
            return -1;
        }
    }

    return 0;
}

static int unit_test_vecXMat()
{
    real_t mat[M*N];
    real_t vec[M];
    real_t dst[N];

    real_t ref[N] = {22, 28};

    int i;

    fprintf(stderr, " Testing vecXmat...\n");

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
            return -1;
        }
    }

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

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
