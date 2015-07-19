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
#include <assert.h>
#include <time.h>

#include <st_utils.h>

#include "utils.h"
#include "blas.h"

#define LOOP_COUNT 1000

static void test_sgemm(int m, int n, int k, bool transa, bool transb)
{
    real_t *a;
    real_t *b;
    real_t *c;
    real_t *cp;

    real_t alpha = 0.1;
    real_t beta = 0.99;

    clock_t time_st;
    clock_t time_end;
    double time_avg;
    double gflop;

    real_t *aa, *bb, *cc;
    int i, j, t, l;

    fprintf(stderr, " Testing sgemm with m=%d, n=%d, k=%d, "
            "transa=%s, transb=%s...\n", m, n, k,
            bool2str(transa), bool2str(transb));

    posix_memalign((void **)&a, 128, sizeof(real_t) * m * k);
    assert(a != NULL);
    for (i = 0; i < m * k; i++) {
        a[i] = st_random(-0.1, 0.1);
    }

    posix_memalign((void **)&b, 128, sizeof(real_t) * k * n);
    assert(b != NULL);
    for (i = 0; i < k * n; i++) {
        b[i] = st_random(-0.1, 0.1);
    }

    posix_memalign((void **)&cp, 128, sizeof(real_t) * m * n);
    assert(cp != NULL);
    for (i = 0; i < m * n; i++) {
        cp[i] = st_random(-0.1, 0.1);
    }
    posix_memalign((void **)&c, 128, sizeof(real_t) * m * n);
    assert(c != NULL);

    fprintf(stderr, "Running clbas_segmm...\n");
    time_st = clock();
    for (l = 0; l < LOOP_COUNT; l++) {
        memcpy(c, cp, sizeof(real_t) * m * n);
        cblas_sgemm(CblasRowMajor, transa ? CblasTrans : CblasNoTrans,
                transb ? CblasTrans : CblasNoTrans,
                m, n, k,
                alpha, a, transa ? m : k,
                b, transb ? k : n,
                beta, c, n);
    }

    time_end = clock();
    time_avg = (double)(time_end - time_st)/CLOCKS_PER_SEC/LOOP_COUNT;
    gflop = (2.0*m*n*k)*1E-9;

    fprintf(stderr, "Average time: %e secs\n", time_avg);
    fprintf(stderr, "GFlop       : %.5f\n", gflop);
    fprintf(stderr, "GFlop/sec   : %.5f\n", gflop/time_avg); 

    fprintf(stderr, "Running raw code...\n");
    time_st = clock();
    for (l = 0; l < LOOP_COUNT; l++) {
        memset(c, 0, sizeof(real_t) * m * n);
        for (t = 0; t < k; t++) {
            cc = c;
            aa = a + t * m;
            bb = b + t * n;
            for (j = 0; j < m; j++) {
                for (i = 0; i < n; i++) {
                    cc[i] += alpha * aa[j] * bb[i];
                }
                cc += n;
            }
        }
        for(i = 0; i < m*n; i++) {
            c[i] += beta * cp[i];
        }
    }
    time_end = clock();
    time_avg = (double)(time_end - time_st)/CLOCKS_PER_SEC/LOOP_COUNT;
    gflop = (2.0*m*n*k)*1E-9;
    fprintf(stderr, "Average time: %e secs\n", time_avg);
    fprintf(stderr, "GFlop       : %.5f\n", gflop);
    fprintf(stderr, "GFlop/sec   : %.5f\n", gflop/time_avg); 

    free(a);
    free(b);
    free(c);
    free(cp);
}

static void run_all_tests(int m, int n, int k, bool transa, bool transb)
{
    test_sgemm(m, n, k, transa, transb);
}

int main(int argc, const char *argv[])
{
    int m = 250;
    int n = 150;
    int k = 20;
    bool transa = true;
    bool transb = false;

    int c = 0;

    if (--argc > 0) {
        m = atoi(argv[++c]);
    }

    if (--argc > 0) {
        n = atoi(argv[++c]);
    }

    if (--argc > 0) {
        k = atoi(argv[++c]);
    }

    if (--argc > 0) {
        transa = str2bool(argv[++c]);
    }

    if (--argc > 0) {
        transb = str2bool(argv[++c]);
    }

    run_all_tests(m, n, k, transa, transb);

    return 0;
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
