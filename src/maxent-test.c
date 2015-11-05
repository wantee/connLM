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

#include <st_utils.h>

#include "maxent.h"

typedef struct _hash_range_arg_t_ {
    hash_range_t hunion[256];
    int num;
    hash_size_t sz;
} hash_range_arg_t;

int maxent_union(hash_range_t *range, int *n_range, hash_range_t *hash,
        int num_hash, hash_size_t sz_hash);

static int unit_test_maxent_union_one(void *base,
        size_t num_set, void *args)
{
    hash_range_t hunion[256];
    int n;
    int i;

    hash_range_t *hsets = (hash_range_t *)base;
    hash_range_arg_t *ref = (hash_range_arg_t *)args;

    n = 0;
    if (maxent_union(hunion, &n, hsets, num_set, ref->sz) < 0) {
        return -1;
    }
    if (n != ref->num) { return -1; }
    for (i = 0; i < n; i++) {
        if (hunion[i].s != ref->hunion[i].s) { return -1; }
        if (hunion[i].e != ref->hunion[i].e) { return -1; }
    }

    return 0;
}

static int unit_test_maxent_union()
{
    hash_range_arg_t ref;
    hash_range_t hsets[128];
    int num_set;
    int ncase = 0;

    ref.sz = 1000;

    fprintf(stderr, "  Testing maxent_union...\n");
    fprintf(stderr, "    Case %d...", ncase++);
    num_set = 2;
    hsets[0].s = 100; hsets[0].num = 200;
    hsets[1].s = 800; hsets[1].num = 200;
    ref.num = 2;
    ref.hunion[0].s = 100; ref.hunion[0].e = 300;
    ref.hunion[1].s = 800; ref.hunion[1].e = 1000;
    if (st_permutation(hsets, num_set, sizeof(hash_range_t),
            unit_test_maxent_union_one, &ref) < 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    num_set = 2;
    hsets[0].s = 100; hsets[0].num = 200;
    hsets[1].s = 800; hsets[1].num = 250;
    ref.num = 3;
    ref.hunion[0].s =   0; ref.hunion[0].e = 50;
    ref.hunion[1].s = 100; ref.hunion[1].e = 300;
    ref.hunion[2].s = 800; ref.hunion[2].e = 1000;
    if (st_permutation(hsets, num_set, sizeof(hash_range_t),
            unit_test_maxent_union_one, &ref) < 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    num_set = 4;
    hsets[0].s = 100; hsets[0].num = 100;
    hsets[1].s = 300; hsets[1].num = 100;
    hsets[2].s = 500; hsets[2].num = 100;
    hsets[3].s = 700; hsets[3].num = 100;
    ref.num = 4;
    ref.hunion[0].s = 100; ref.hunion[0].e = 200;
    ref.hunion[1].s = 300; ref.hunion[1].e = 400;
    ref.hunion[2].s = 500; ref.hunion[2].e = 600;
    ref.hunion[3].s = 700; ref.hunion[3].e = 800;
    if (st_permutation(hsets, num_set, sizeof(hash_range_t),
            unit_test_maxent_union_one, &ref) < 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    num_set = 5;
    hsets[0].s = 100; hsets[0].num = 100;
    hsets[1].s = 300; hsets[1].num = 100;
    hsets[2].s = 500; hsets[2].num = 100;
    hsets[3].s = 700; hsets[3].num = 100;
    hsets[4].s = 0; hsets[4].num = 50;
    ref.num = 5;
    ref.hunion[0].s =   0; ref.hunion[0].e = 50;
    ref.hunion[1].s = 100; ref.hunion[1].e = 200;
    ref.hunion[2].s = 300; ref.hunion[2].e = 400;
    ref.hunion[3].s = 500; ref.hunion[3].e = 600;
    ref.hunion[4].s = 700; ref.hunion[4].e = 800;
    if (st_permutation(hsets, num_set, sizeof(hash_range_t),
            unit_test_maxent_union_one, &ref) < 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    num_set = 5;
    hsets[0].s = 100; hsets[0].num = 100;
    hsets[1].s = 300; hsets[1].num = 100;
    hsets[2].s = 500; hsets[2].num = 100;
    hsets[3].s = 700; hsets[3].num = 100;
    hsets[4].s = 0; hsets[4].num = 100;
    ref.num = 4;
    ref.hunion[0].s =   0; ref.hunion[0].e = 200;
    ref.hunion[1].s = 300; ref.hunion[1].e = 400;
    ref.hunion[2].s = 500; ref.hunion[2].e = 600;
    ref.hunion[3].s = 700; ref.hunion[3].e = 800;
    if (st_permutation(hsets, num_set, sizeof(hash_range_t),
            unit_test_maxent_union_one, &ref) < 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    num_set = 5;
    hsets[0].s = 100; hsets[0].num = 100;
    hsets[1].s = 300; hsets[1].num = 100;
    hsets[2].s = 500; hsets[2].num = 100;
    hsets[3].s = 700; hsets[3].num = 100;
    hsets[4].s = 0; hsets[4].num = 150;
    ref.num = 4;
    ref.hunion[0].s =   0; ref.hunion[0].e = 200;
    ref.hunion[1].s = 300; ref.hunion[1].e = 400;
    ref.hunion[2].s = 500; ref.hunion[2].e = 600;
    ref.hunion[3].s = 700; ref.hunion[3].e = 800;
    if (st_permutation(hsets, num_set, sizeof(hash_range_t),
            unit_test_maxent_union_one, &ref) < 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    num_set = 5;
    hsets[0].s = 100; hsets[0].num = 100;
    hsets[1].s = 300; hsets[1].num = 100;
    hsets[2].s = 500; hsets[2].num = 100;
    hsets[3].s = 700; hsets[3].num = 100;
    hsets[4].s = 0; hsets[4].num = 250;
    ref.num = 4;
    ref.hunion[0].s =   0; ref.hunion[0].e = 250;
    ref.hunion[1].s = 300; ref.hunion[1].e = 400;
    ref.hunion[2].s = 500; ref.hunion[2].e = 600;
    ref.hunion[3].s = 700; ref.hunion[3].e = 800;
    if (st_permutation(hsets, num_set, sizeof(hash_range_t),
            unit_test_maxent_union_one, &ref) < 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    num_set = 5;
    hsets[0].s = 100; hsets[0].num = 100;
    hsets[1].s = 300; hsets[1].num = 100;
    hsets[2].s = 500; hsets[2].num = 100;
    hsets[3].s = 700; hsets[3].num = 100;
    hsets[4].s = 0; hsets[4].num = 300;
    ref.num = 3;
    ref.hunion[0].s =   0; ref.hunion[0].e = 400;
    ref.hunion[1].s = 500; ref.hunion[1].e = 600;
    ref.hunion[2].s = 700; ref.hunion[2].e = 800;
    if (st_permutation(hsets, num_set, sizeof(hash_range_t),
            unit_test_maxent_union_one, &ref) < 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    num_set = 5;
    hsets[0].s = 100; hsets[0].num = 100;
    hsets[1].s = 300; hsets[1].num = 100;
    hsets[2].s = 500; hsets[2].num = 100;
    hsets[3].s = 700; hsets[3].num = 100;
    hsets[4].s = 0; hsets[4].num = 450;
    ref.num = 3;
    ref.hunion[0].s =   0; ref.hunion[0].e = 450;
    ref.hunion[1].s = 500; ref.hunion[1].e = 600;
    ref.hunion[2].s = 700; ref.hunion[2].e = 800;
    if (st_permutation(hsets, num_set, sizeof(hash_range_t),
            unit_test_maxent_union_one, &ref) < 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    num_set = 5;
    hsets[0].s = 100; hsets[0].num = 100;
    hsets[1].s = 300; hsets[1].num = 100;
    hsets[2].s = 500; hsets[2].num = 100;
    hsets[3].s = 700; hsets[3].num = 100;
    hsets[4].s = 0; hsets[4].num = 500;
    ref.num = 2;
    ref.hunion[0].s =   0; ref.hunion[0].e = 600;
    ref.hunion[1].s = 700; ref.hunion[1].e = 800;
    if (st_permutation(hsets, num_set, sizeof(hash_range_t),
            unit_test_maxent_union_one, &ref) < 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    num_set = 5;
    hsets[0].s = 100; hsets[0].num = 100;
    hsets[1].s = 300; hsets[1].num = 100;
    hsets[2].s = 500; hsets[2].num = 100;
    hsets[3].s = 700; hsets[3].num = 100;
    hsets[4].s = 0; hsets[4].num = 550;
    ref.num = 2;
    ref.hunion[0].s =   0; ref.hunion[0].e = 600;
    ref.hunion[1].s = 700; ref.hunion[1].e = 800;
    if (st_permutation(hsets, num_set, sizeof(hash_range_t),
            unit_test_maxent_union_one, &ref) < 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    num_set = 5;
    hsets[0].s = 100; hsets[0].num = 100;
    hsets[1].s = 300; hsets[1].num = 100;
    hsets[2].s = 500; hsets[2].num = 100;
    hsets[3].s = 700; hsets[3].num = 100;
    hsets[4].s = 0; hsets[4].num = 650;
    ref.num = 2;
    ref.hunion[0].s =   0; ref.hunion[0].e = 650;
    ref.hunion[1].s = 700; ref.hunion[1].e = 800;
    if (st_permutation(hsets, num_set, sizeof(hash_range_t),
            unit_test_maxent_union_one, &ref) < 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    num_set = 5;
    hsets[0].s = 100; hsets[0].num = 100;
    hsets[1].s = 300; hsets[1].num = 100;
    hsets[2].s = 500; hsets[2].num = 100;
    hsets[3].s = 700; hsets[3].num = 100;
    hsets[4].s = 0; hsets[4].num = 700;
    ref.num = 1;
    ref.hunion[0].s =   0; ref.hunion[0].e = 800;
    if (st_permutation(hsets, num_set, sizeof(hash_range_t),
            unit_test_maxent_union_one, &ref) < 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    num_set = 5;
    hsets[0].s = 100; hsets[0].num = 100;
    hsets[1].s = 300; hsets[1].num = 100;
    hsets[2].s = 500; hsets[2].num = 100;
    hsets[3].s = 700; hsets[3].num = 100;
    hsets[4].s = 0; hsets[4].num = 750;
    ref.num = 1;
    ref.hunion[0].s =   0; ref.hunion[0].e = 800;
    if (st_permutation(hsets, num_set, sizeof(hash_range_t),
            unit_test_maxent_union_one, &ref) < 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    num_set = 5;
    hsets[0].s = 100; hsets[0].num = 100;
    hsets[1].s = 300; hsets[1].num = 100;
    hsets[2].s = 500; hsets[2].num = 100;
    hsets[3].s = 700; hsets[3].num = 100;
    hsets[4].s = 0; hsets[4].num = 850;
    ref.num = 1;
    ref.hunion[0].s =   0; ref.hunion[0].e = 850;
    if (st_permutation(hsets, num_set, sizeof(hash_range_t),
            unit_test_maxent_union_one, &ref) < 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    num_set = 5;
    hsets[0].s = 100; hsets[0].num = 100;
    hsets[1].s = 300; hsets[1].num = 100;
    hsets[2].s = 500; hsets[2].num = 100;
    hsets[3].s = 700; hsets[3].num = 100;
    hsets[4].s = 150; hsets[4].num = 50;
    ref.num = 4;
    ref.hunion[0].s = 100; ref.hunion[0].e = 200;
    ref.hunion[1].s = 300; ref.hunion[1].e = 400;
    ref.hunion[2].s = 500; ref.hunion[2].e = 600;
    ref.hunion[3].s = 700; ref.hunion[3].e = 800;
    if (st_permutation(hsets, num_set, sizeof(hash_range_t),
            unit_test_maxent_union_one, &ref) < 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    num_set = 5;
    hsets[0].s = 100; hsets[0].num = 100;
    hsets[1].s = 300; hsets[1].num = 100;
    hsets[2].s = 500; hsets[2].num = 100;
    hsets[3].s = 700; hsets[3].num = 100;
    hsets[4].s = 150; hsets[4].num = 100;
    ref.num = 4;
    ref.hunion[0].s = 100; ref.hunion[0].e = 250;
    ref.hunion[1].s = 300; ref.hunion[1].e = 400;
    ref.hunion[2].s = 500; ref.hunion[2].e = 600;
    ref.hunion[3].s = 700; ref.hunion[3].e = 800;
    if (st_permutation(hsets, num_set, sizeof(hash_range_t),
            unit_test_maxent_union_one, &ref) < 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    num_set = 5;
    hsets[0].s = 100; hsets[0].num = 100;
    hsets[1].s = 300; hsets[1].num = 100;
    hsets[2].s = 500; hsets[2].num = 100;
    hsets[3].s = 700; hsets[3].num = 100;
    hsets[4].s = 150; hsets[4].num = 200;
    ref.num = 3;
    ref.hunion[0].s = 100; ref.hunion[0].e = 400;
    ref.hunion[1].s = 500; ref.hunion[1].e = 600;
    ref.hunion[2].s = 700; ref.hunion[2].e = 800;
    if (st_permutation(hsets, num_set, sizeof(hash_range_t),
            unit_test_maxent_union_one, &ref) < 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    num_set = 5;
    hsets[0].s = 100; hsets[0].num = 100;
    hsets[1].s = 300; hsets[1].num = 100;
    hsets[2].s = 500; hsets[2].num = 100;
    hsets[3].s = 700; hsets[3].num = 100;
    hsets[4].s = 150; hsets[4].num = 300;
    ref.num = 3;
    ref.hunion[0].s = 100; ref.hunion[0].e = 450;
    ref.hunion[1].s = 500; ref.hunion[1].e = 600;
    ref.hunion[2].s = 700; ref.hunion[2].e = 800;
    if (st_permutation(hsets, num_set, sizeof(hash_range_t),
            unit_test_maxent_union_one, &ref) < 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    num_set = 5;
    hsets[0].s = 100; hsets[0].num = 100;
    hsets[1].s = 300; hsets[1].num = 100;
    hsets[2].s = 500; hsets[2].num = 100;
    hsets[3].s = 700; hsets[3].num = 100;
    hsets[4].s = 150; hsets[4].num = 400;
    ref.num = 2;
    ref.hunion[0].s = 100; ref.hunion[0].e = 600;
    ref.hunion[1].s = 700; ref.hunion[1].e = 800;
    if (st_permutation(hsets, num_set, sizeof(hash_range_t),
            unit_test_maxent_union_one, &ref) < 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    num_set = 5;
    hsets[0].s = 100; hsets[0].num = 100;
    hsets[1].s = 300; hsets[1].num = 100;
    hsets[2].s = 500; hsets[2].num = 100;
    hsets[3].s = 700; hsets[3].num = 100;
    hsets[4].s = 150; hsets[4].num = 500;
    ref.num = 2;
    ref.hunion[0].s = 100; ref.hunion[0].e = 650;
    ref.hunion[1].s = 700; ref.hunion[1].e = 800;
    if (st_permutation(hsets, num_set, sizeof(hash_range_t),
            unit_test_maxent_union_one, &ref) < 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    num_set = 5;
    hsets[0].s = 100; hsets[0].num = 100;
    hsets[1].s = 300; hsets[1].num = 100;
    hsets[2].s = 500; hsets[2].num = 100;
    hsets[3].s = 700; hsets[3].num = 100;
    hsets[4].s = 150; hsets[4].num = 600;
    ref.num = 1;
    ref.hunion[0].s = 100; ref.hunion[0].e = 800;
    if (st_permutation(hsets, num_set, sizeof(hash_range_t),
            unit_test_maxent_union_one, &ref) < 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    num_set = 5;
    hsets[0].s = 100; hsets[0].num = 100;
    hsets[1].s = 300; hsets[1].num = 100;
    hsets[2].s = 500; hsets[2].num = 100;
    hsets[3].s = 700; hsets[3].num = 100;
    hsets[4].s = 150; hsets[4].num = 700;
    ref.num = 1;
    ref.hunion[0].s = 100; ref.hunion[0].e = 850;
    if (st_permutation(hsets, num_set, sizeof(hash_range_t),
            unit_test_maxent_union_one, &ref) < 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    num_set = 5;
    hsets[0].s = 100; hsets[0].num = 100;
    hsets[1].s = 300; hsets[1].num = 100;
    hsets[2].s = 500; hsets[2].num = 100;
    hsets[3].s = 700; hsets[3].num = 100;
    hsets[4].s = 200; hsets[4].num = 50;
    ref.num = 4;
    ref.hunion[0].s = 100; ref.hunion[0].e = 250;
    ref.hunion[1].s = 300; ref.hunion[1].e = 400;
    ref.hunion[2].s = 500; ref.hunion[2].e = 600;
    ref.hunion[3].s = 700; ref.hunion[3].e = 800;
    if (st_permutation(hsets, num_set, sizeof(hash_range_t),
            unit_test_maxent_union_one, &ref) < 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    num_set = 5;
    hsets[0].s = 100; hsets[0].num = 100;
    hsets[1].s = 300; hsets[1].num = 100;
    hsets[2].s = 500; hsets[2].num = 100;
    hsets[3].s = 700; hsets[3].num = 100;
    hsets[4].s = 200; hsets[4].num = 100;
    ref.num = 3;
    ref.hunion[0].s = 100; ref.hunion[0].e = 400;
    ref.hunion[1].s = 500; ref.hunion[1].e = 600;
    ref.hunion[2].s = 700; ref.hunion[2].e = 800;
    if (st_permutation(hsets, num_set, sizeof(hash_range_t),
            unit_test_maxent_union_one, &ref) < 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    num_set = 5;
    hsets[0].s = 100; hsets[0].num = 100;
    hsets[1].s = 300; hsets[1].num = 100;
    hsets[2].s = 500; hsets[2].num = 100;
    hsets[3].s = 700; hsets[3].num = 100;
    hsets[4].s = 200; hsets[4].num = 250;
    ref.num = 3;
    ref.hunion[0].s = 100; ref.hunion[0].e = 450;
    ref.hunion[1].s = 500; ref.hunion[1].e = 600;
    ref.hunion[2].s = 700; ref.hunion[2].e = 800;
    if (st_permutation(hsets, num_set, sizeof(hash_range_t),
            unit_test_maxent_union_one, &ref) < 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    num_set = 5;
    hsets[0].s = 100; hsets[0].num = 100;
    hsets[1].s = 300; hsets[1].num = 100;
    hsets[2].s = 500; hsets[2].num = 100;
    hsets[3].s = 700; hsets[3].num = 100;
    hsets[4].s = 250; hsets[4].num = 25;
    ref.num = 5;
    ref.hunion[0].s = 100; ref.hunion[0].e = 200;
    ref.hunion[1].s = 250; ref.hunion[1].e = 275;
    ref.hunion[2].s = 300; ref.hunion[2].e = 400;
    ref.hunion[3].s = 500; ref.hunion[3].e = 600;
    ref.hunion[4].s = 700; ref.hunion[4].e = 800;
    if (st_permutation(hsets, num_set, sizeof(hash_range_t),
            unit_test_maxent_union_one, &ref) < 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    fprintf(stderr, "    Case %d...", ncase++);
    num_set = 5;
    hsets[0].s = 100; hsets[0].num = 100;
    hsets[1].s = 300; hsets[1].num = 100;
    hsets[2].s = 500; hsets[2].num = 100;
    hsets[3].s = 700; hsets[3].num = 100;
    hsets[4].s = 850; hsets[4].num = 50;
    ref.num = 5;
    ref.hunion[0].s = 100; ref.hunion[0].e = 200;
    ref.hunion[1].s = 300; ref.hunion[1].e = 400;
    ref.hunion[2].s = 500; ref.hunion[2].e = 600;
    ref.hunion[3].s = 700; ref.hunion[3].e = 800;
    ref.hunion[4].s = 850; ref.hunion[4].e = 900;
    if (st_permutation(hsets, num_set, sizeof(hash_range_t),
            unit_test_maxent_union_one, &ref) < 0) {
        fprintf(stderr, "Failed\n");
        return -1;
    }
    fprintf(stderr, "Success\n");

    return 0;
}

static int run_all_tests()
{
    int ret = 0;

    if (unit_test_maxent_union() != 0) {
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
