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
#include <float.h>

#include <st_log.h>
#include <st_utils.h>

#include "config.h"
#include "utils.h"
#include "fastexp.h"
#include "blas.h"

void matXvec(real_t *dst, real_t *mat, real_t *vec,
        int mat_row, int in_vec_size, real_t scale)
{
#ifndef _USE_BLAS_
    int i;
    int j;
#endif

#ifdef _USE_BLAS_
    cblas_sgemv(CblasRowMajor, CblasNoTrans, mat_row, in_vec_size, scale,
            mat, in_vec_size, vec, 1, 1.0, dst, 1);
#elif defined(_MAT_X_VEC_RAW_)
    for (i = 0; i < mat_row; i++) {
        for (j = 0; j < in_vec_size; j++) {
            dst[i] += vec[j] * mat[i*in_vec_size + j] * scale;
        }
    }
#else

#define N 8
    real_t t0;
    real_t t1;
    real_t t2;
    real_t t3;

    real_t t4;
    real_t t5;
    real_t t6;
    real_t t7;

    for (i = 0; i < mat_row / N; i++) {
        t0 = 0;
        t1 = 0;
        t2 = 0;
        t3 = 0;

        t4 = 0;
        t5 = 0;
        t6 = 0;
        t7 = 0;

        for (j = 0; j < in_vec_size; j++) {
            t0 += vec[j] * mat[(i * N + 0)*in_vec_size + j];
            t1 += vec[j] * mat[(i * N + 1)*in_vec_size + j];
            t2 += vec[j] * mat[(i * N + 2)*in_vec_size + j];
            t3 += vec[j] * mat[(i * N + 3)*in_vec_size + j];

            t4 += vec[j] * mat[(i * N + 4)*in_vec_size + j];
            t5 += vec[j] * mat[(i * N + 5)*in_vec_size + j];
            t6 += vec[j] * mat[(i * N + 6)*in_vec_size + j];
            t7 += vec[j] * mat[(i * N + 7)*in_vec_size + j];
        }

        dst[i * N + 0] += t0 * scale;
        dst[i * N + 1] += t1 * scale;
        dst[i * N + 2] += t2 * scale;
        dst[i * N + 3] += t3 * scale;

        dst[i * N + 4] += t4 * scale;
        dst[i * N + 5] += t5 * scale;
        dst[i * N + 6] += t6 * scale;
        dst[i * N + 7] += t7 * scale;
    }

    for (i = i * N; i < mat_row; i++) {
        for (j = 0; j < in_vec_size; j++) {
            dst[i] += vec[j] * mat[i*in_vec_size + j] * scale;
        }
    }
#endif
}

void vecXmat(real_t *dst, real_t *vec, real_t *mat,
        int mat_col, int in_vec_size, real_t scale)
{
#ifndef _USE_BLAS_
    int i;
    int j;
#endif

#ifdef _USE_BLAS_
    cblas_sgemv(CblasRowMajor, CblasTrans, in_vec_size, mat_col, scale,
            mat, mat_col, vec, 1, 1.0, dst, 1);
#elif defined(_MAT_X_VEC_RAW_)
    for (i = 0; i < mat_col; i++) {
        for (j = 0; j < in_vec_size; j++) {
            dst[i] += vec[j] * mat[j*mat_col + i] * scale;
        }
    }
#else

#define N 8
    real_t t0;
    real_t t1;
    real_t t2;
    real_t t3;

    real_t t4;
    real_t t5;
    real_t t6;
    real_t t7;

    for (i = 0; i < mat_col / N; i++) {
        t0 = 0;
        t1 = 0;
        t2 = 0;
        t3 = 0;

        t4 = 0;
        t5 = 0;
        t6 = 0;
        t7 = 0;

        for (j = 0; j < in_vec_size; j++) {
            t0 += vec[j] * mat[(i * N + 0) + j*mat_col];
            t1 += vec[j] * mat[(i * N + 1) + j*mat_col];
            t2 += vec[j] * mat[(i * N + 2) + j*mat_col];
            t3 += vec[j] * mat[(i * N + 3) + j*mat_col];

            t4 += vec[j] * mat[(i * N + 4) + j*mat_col];
            t5 += vec[j] * mat[(i * N + 5) + j*mat_col];
            t6 += vec[j] * mat[(i * N + 6) + j*mat_col];
            t7 += vec[j] * mat[(i * N + 7) + j*mat_col];
        }

        dst[i * N + 0] += t0 * scale;
        dst[i * N + 1] += t1 * scale;
        dst[i * N + 2] += t2 * scale;
        dst[i * N + 3] += t3 * scale;

        dst[i * N + 4] += t4 * scale;
        dst[i * N + 5] += t5 * scale;
        dst[i * N + 6] += t6 * scale;
        dst[i * N + 7] += t7 * scale;
    }

    for (i = i * N; i < mat_col; i++) {
        for (j = 0; j < in_vec_size; j++) {
            dst[i] += vec[j] * mat[i + j*mat_col] * scale;
        }
    }
#endif
}

void sigmoid(real_t *vec, int vec_size)
{
    int a;

    for (a = 0; a < vec_size; a++) {
        if (vec[a] > 50) {
            vec[a] = 50;
        }
        if (vec[a] < -50) {
            vec[a] = -50;
        }

        vec[a] = fastersigmoid(vec[a]);
    }
}

void softmax(real_t *vec, int vec_size)
{
    double sum;
    real_t max;

    int i;

    max = -FLT_MAX;
    sum = 0;

    for (i = 0; i < vec_size; i++) {
        if (vec[i] > max) {
            max = vec[i]; //this prevents the need to check for overflow
        }
    }

    for (i = 0; i < vec_size; i++) {
        vec[i] = fasterexp(vec[i] - max);
        sum += vec[i];
    }

    for (i = 0; i < vec_size; i++) {
        vec[i] = vec[i] / sum;
    }
}

real_t rrandom(real_t min, real_t max)
{
    return st_rand() / (real_t) ST_RAND_MAX *(max - min) + min;
}

void connlm_show_usage(const char *module_name, const char *header,
        const char *usage, st_opt_t *opt)
{
    fprintf(stderr, "\nConnectionist Language Modelling Toolkit\n");
    fprintf(stderr, "  -- %s\n", header);
    fprintf(stderr, "Version  : %s\n", CONNLM_VERSION);
    fprintf(stderr, "File version: %d\n", CONNLM_FILE_VERSION);
    fprintf(stderr, "Real type: %s\n",
            (sizeof(real_t) == sizeof(double)) ? "double" : "float");
    fprintf(stderr, "Usage    : %s [options] %s\n", module_name, usage);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options  : \n");
    st_opt_show_usage(opt, stderr);
}

int int_comp(const void *elem1, const void *elem2) 
{
    int f = *((int*)elem1);
    int s = *((int*)elem2);
    if (f > s) return  1;
    if (f < s) return -1;
    return 0;
}

void int_sort(int *A, size_t n)
{
    qsort(A, n, sizeof(int), int_comp);
}
