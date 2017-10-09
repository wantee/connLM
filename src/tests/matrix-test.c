/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Wang Jian
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

#include "matrix.h"

static void init_mat(mat_t *mat, size_t num_rows, size_t num_cols)
{
    size_t i, j, t;

    assert(mat_resize(mat, num_rows, num_cols, NAN) == 0);
    t = 0;
    for (i = 0; i < mat->num_rows; i++) {
        for (j = 0; j < mat->num_cols; j++) {
            MAT_VAL(mat, i, j) = t++;
        }
    }
}

static void fill_ref(mat_t *ref, real_t *ans, real_t alpha, real_t beta)
{
    size_t i, j, t;

    t = 0;
    for (i = 0; i < ref->num_rows; i++) {
        for (j = 0; j < ref->num_cols; j++) {
            MAT_VAL(ref, i, j) = alpha * ans[t++] + beta * MAT_VAL(ref, i, j);
        }
    }
}

// ref is calculated by following python script
// import numpy as np
// m = 2
// n = 4
// k = 4
// A = np.arange(m * k).reshape((m, k))
// B = np.arange(k * n).reshape((k, n))
// ref = np.matmul(A, B)
// # B = np.arange(k * n).reshape((n, k))
// # ref = np.matmul(A, np.transpose(B))
static void init_data(real_t alpha, real_t beta,
        mat_trans_t trans_A, mat_trans_t trans_B,
        mat_t *A, mat_t *B, mat_t *C, mat_t *ref, bool test_stride)
{
    size_t m, n, k;

    if (test_stride) {
        m = 2;
        n = 7;
        k = 5;
    } else {
        m = 2;
        n = 4;
        k = 4;
    }

    if (trans_A == MT_NoTrans && trans_B == MT_NoTrans) {
        init_mat(A, m, k);
        init_mat(B, k, n);
        init_mat(C, m, n);

        init_mat(ref, m, n);

        if (test_stride) {
            real_t ans[] = { 210, 220, 230, 240, 250, 260, 270,
                             560, 595, 630, 665, 700, 735, 770 };
            fill_ref(ref, ans, alpha, beta);
        } else {
            real_t ans[] = { 56,  62,  68,  74,
                             152, 174, 196, 218 };
            fill_ref(ref, ans, alpha, beta);
        }
    } else if (trans_A == MT_NoTrans && trans_B == MT_Trans) {
        init_mat(A, m, k);
        init_mat(B, n, k);
        init_mat(C, m, n);

        init_mat(ref, m, n);

        if (test_stride) {
            real_t ans[] = { 30,   80,  130,  180,  230,  280,  330,
                             80,  255,  430,  605,  780,  955, 1130 };
            fill_ref(ref, ans, alpha, beta);
        } else {
            real_t ans[] = { 14,  38,  62,  86,
                             38, 126, 214, 302 };
            fill_ref(ref, ans, alpha, beta);
        }
    } else if (trans_A == MT_Trans && trans_B == MT_NoTrans) {
        init_mat(A, k, m);
        init_mat(B, k, n);
        init_mat(C, m, n);

        init_mat(ref, m, n);

        if (test_stride) {
            real_t ans[] = { 420, 440, 460, 480, 500, 520, 540,
                             490, 515, 540, 565, 590, 615, 640 };
            fill_ref(ref, ans, alpha, beta);
        } else {
            real_t ans[] = { 112, 124, 136, 148,
                             136, 152, 168, 184 };
            fill_ref(ref, ans, alpha, beta);
        }
    } else if (trans_A == MT_Trans && trans_B == MT_Trans) {
        init_mat(A, k, m);
        init_mat(B, n, k);
        init_mat(C, m, n);

        init_mat(ref, m, n);

        if (test_stride) {
            real_t ans[] = { 60, 160, 260, 360, 460, 560, 660,
                             70, 195, 320, 445, 570, 695, 820 };
            fill_ref(ref, ans, alpha, beta);
        } else {
            real_t ans[] = { 28,  76, 124, 172,
                             34,  98, 162, 226 };
            fill_ref(ref, ans, alpha, beta);
        }
    }
}

static int unit_test_one_add_mat_mat(real_t alpha, real_t beta,
        mat_trans_t trans_A, mat_trans_t trans_B, bool test_stride)
{
    mat_t A = {0},
          B = {0},
          C = {0};

    mat_t ref = {0};

    init_data(alpha, beta, trans_A, trans_B, &A, &B, &C, &ref, test_stride);

    if (add_mat_mat(alpha, &A, trans_A,
                &B, trans_B, beta, &C) < 0) {
        return -1;
    }

    if (!mat_eq(&C, &ref)) {
#ifdef _MATRIX_TEST_DEBUG_
        fprintf(stdout, "\n");
        assert (mat_save_body(&A, stdout, CONN_FMT_TXT, "A") == 0);
        fprintf(stdout, "\n");
        assert (mat_save_body(&B, stdout, CONN_FMT_TXT, "B") == 0);
        fprintf(stdout, "\n");
        assert (mat_save_body(&C, stdout, CONN_FMT_TXT, "C") == 0);
        fprintf(stdout, "\n");
        assert (mat_save_body(&ref, stdout, CONN_FMT_TXT, "REF") == 0);
#endif
        return -1;
    }

    return 0;
}

static int unit_test_add_mat_mat()
{
    real_t alpha, beta;
    mat_trans_t trans_A, trans_B;

    int ncase = 0;

    fprintf(stderr, " Testing add_mat_mat...");

    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    alpha = 2.0;
    beta = 1.0;
    trans_A = MT_NoTrans;
    trans_B = MT_NoTrans;
    if (unit_test_one_add_mat_mat(alpha, beta, trans_A, trans_B, false) < 0) {
        goto FAILED;
    }

    fprintf(stderr, "    Case %d...", ncase++);
    if (unit_test_one_add_mat_mat(alpha, beta, trans_A, trans_B, true) < 0) {
        goto FAILED;
    }

    fprintf(stderr, "    Case %d...", ncase++);
    beta = 0.0;
    if (unit_test_one_add_mat_mat(alpha, beta, trans_A, trans_B, false) < 0) {
        goto FAILED;
    }

    fprintf(stderr, "    Case %d...", ncase++);
    beta = 2.0;
    if (unit_test_one_add_mat_mat(alpha, beta, trans_A, trans_B, false) < 0) {
        goto FAILED;
    }

    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    alpha = 2.0;
    beta = 1.0;
    trans_A = MT_Trans;
    trans_B = MT_NoTrans;
    if (unit_test_one_add_mat_mat(alpha, beta, trans_A, trans_B, false) < 0) {
        goto FAILED;
    }

    fprintf(stderr, "    Case %d...", ncase++);
    if (unit_test_one_add_mat_mat(alpha, beta, trans_A, trans_B, true) < 0) {
        goto FAILED;
    }

    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    alpha = 2.0;
    beta = 1.0;
    trans_A = MT_NoTrans;
    trans_B = MT_Trans;
    if (unit_test_one_add_mat_mat(alpha, beta, trans_A, trans_B, false) < 0) {
        goto FAILED;
    }

    fprintf(stderr, "    Case %d...", ncase++);
    if (unit_test_one_add_mat_mat(alpha, beta, trans_A, trans_B, true) < 0) {
        goto FAILED;
    }

    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    alpha = 2.0;
    beta = 1.0;
    trans_A = MT_Trans;
    trans_B = MT_Trans;
    if (unit_test_one_add_mat_mat(alpha, beta, trans_A, trans_B, false) < 0) {
        goto FAILED;
    }

    fprintf(stderr, "    Case %d...", ncase++);
    if (unit_test_one_add_mat_mat(alpha, beta, trans_A, trans_B, true) < 0) {
        goto FAILED;
    }

    fprintf(stderr, "Passed\n");
    return 0;

FAILED:
    fprintf(stderr, "Failed\n");
    return -1;
}

static int run_all_tests()
{
    int ret = 0;

    if (unit_test_add_mat_mat() != 0) {
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
