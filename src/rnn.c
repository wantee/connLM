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
#include <math.h>
#include <sys/time.h>

#include <st_macro.h>
#include <st_log.h>

#include "fastexp.h"
#include "utils.h"
#include "rnn.h"

static const int RNN_MAGIC_NUM = 626140498 + 2;

#ifdef _TIME_PROF_
extern long long matXvec_total;
extern long long vecXmat_total;
extern long long softmax_total;
long long direct_total = 0;
#endif

#define RNNLM_VERSION 10
#define REGULARIZATION_STEP 10

#define exp10(a) pow(10.0, a)

const unsigned int PRIMES[] = {
    108641969, 116049371, 125925907, 133333309, 145678979, 175308587,
    197530793, 234567803, 251851741, 264197411, 330864029, 399999781,
    407407183, 459258997, 479012069, 545678687, 560493491, 607407037,
    629629243, 656789717, 716048933, 718518067, 725925469, 733332871,
    753085943, 755555077, 782715551, 790122953, 812345159, 814814293,
    893826581, 923456189, 940740127, 953085797, 985184539, 990122807
};

const unsigned int PRIMES_SIZE = sizeof(PRIMES) / sizeof(PRIMES[0]);

int rnn_load_opt(rnn_opt_t *rnn_opt, st_opt_t *opt, const char *sec_name)
{
    float f;

    ST_CHECK_PARAM(rnn_opt == NULL || opt == NULL, -1);

    memset(rnn_opt, 0, sizeof(rnn_opt_t));

    ST_OPT_SEC_GET_FLOAT(opt, sec_name, "SCALE", f, 1.0,
            "Scale of RNN model output");
    rnn_opt->scale = (real_t)f;

    if (rnn_opt->scale <= 0) {
        return 0;
    }

    ST_OPT_SEC_GET_INT(opt, sec_name, "HIDDEN_SIZE",
            rnn_opt->hidden_size, 15, "Size of hidden layer of RNN");

    ST_OPT_SEC_GET_INT(opt, sec_name, "BPTT", rnn_opt->bptt, 5,
            "Time step of BPTT");
    ST_OPT_SEC_GET_INT(opt, sec_name, "BPTT_BLOCK",
            rnn_opt->bptt_block, 10, "Block size of BPTT");

    return 0;

ST_OPT_ERR:
    return -1;
}

rnn_t *rnn_create()
{
    rnn_t *rnn = NULL;

    rnn = (rnn_t *) malloc(sizeof(rnn_t));
    if (rnn == NULL) {
        ST_WARNING("Falied to malloc rnn_t.");
        goto ERR;
    }
    memset(rnn, 0, sizeof(rnn_t));

    return rnn;
  ERR:
    rnn_destroy(rnn);
    safe_free(rnn);
    return NULL;
}

void rnn_destroy(rnn_t *rnn)
{
    if (rnn == NULL) {
        return;
    }

    safe_free(rnn->w2c);
    safe_free(rnn->c2w_s);
    safe_free(rnn->c2w_e);
    safe_free(rnn->cnts);

    //safe_free(rnn->ac_i_w);
    //safe_free(rnn->er_i_w);
    safe_free(rnn->ac_i_h);
    safe_free(rnn->er_i_h);

    safe_free(rnn->ac_h);
    safe_free(rnn->er_h);

    safe_free(rnn->ac_o_c);
    safe_free(rnn->er_o_c);
    safe_free(rnn->ac_o_w);
    safe_free(rnn->er_o_w);

    safe_free(rnn->wt_ih_w);
    safe_free(rnn->wt_ih_h);

    safe_free(rnn->wt_ho_c);
    safe_free(rnn->wt_ho_w);

    safe_free(rnn->wt_d);
    safe_free(rnn->d_hist);
    safe_free(rnn->d_hash);

    safe_free(rnn->bptt_hist);
    safe_free(rnn->ac_bptt_h);
    safe_free(rnn->er_bptt_h);
    safe_free(rnn->wt_bptt_ih_w);
    safe_free(rnn->wt_bptt_ih_h);
}

rnn_t* rnn_dup(rnn_t *r)
{
    rnn_t *rnn = NULL;

    ST_CHECK_PARAM(r == NULL, NULL);

    rnn = (rnn_t *) malloc(sizeof(rnn_t));
    if (rnn == NULL) {
        ST_WARNING("Falied to malloc rnn_t.");
        goto ERR;
    }

    *rnn = *r;

    return rnn;

ERR:
    safe_rnn_destroy(rnn);
    return NULL;
}

long rnn_load_header(rnn_t **rnn, FILE *fp, bool *binary, FILE *fo)
{
    char str[MAX_LINE_LEN];
    long sz;
    int magic_num;
    int version;

    ST_CHECK_PARAM((rnn == NULL && fo == NULL) || fp == NULL
            || binary == NULL, -1);

    if (fread(&magic_num, sizeof(int), 1, fp) != 1) {
        ST_WARNING("NOT rnn format: Failed to load magic num.");
        return -1;
    }

    if (RNN_MAGIC_NUM != magic_num) {
        ST_WARNING("NOT rnn format, magic num wrong.");
        return -2;
    }

    fscanf(fp, "\n<RNN>\n");

    if (fread(&sz, sizeof(long), 1, fp) != 1) {
        ST_WARNING("Failed to read size.");
        return -1;
    }

    if (sz <= 0) {
        if (rnn != NULL) {
            *rnn = NULL;
        }
        if (fo != NULL) {
            fprintf(fo, "\n<RNN>: None\n");
        }
        return 0;
    }

    if (rnn != NULL) {
        *rnn = (rnn_t *)malloc(sizeof(rnn_t));
        if (*rnn == NULL) {
            ST_WARNING("Failed to malloc rnn_t");
            goto ERR;
        }
        memset(*rnn, 0, sizeof(rnn_t));
    }

    fscanf(fp, "Version: %d\n", &version);

    if (version > CONNLM_FILE_VERSION) {
        ST_WARNING("Too high file versoin[%d].");
        goto ERR;
    }

    fscanf(fp, "Binary: %s\n", str);
    *binary = str2bool(str);

    if (fo != NULL) {
        fprintf(fo, "\n<RNN>\n");
        fprintf(fo, "Version: %d\n", version);
        fprintf(fo, "Binary: %s\n", bool2str(*binary));
        fprintf(fo, "Size: %ldB\n", sz);
    }

    return sz;

ERR:
    safe_rnn_destroy(*rnn);
    return -1;
}

int rnn_load(rnn_t **rnn, FILE *fp)
{
    bool binary;

    ST_CHECK_PARAM(rnn == NULL || fp == NULL, -1);

    if (rnn_load_header(rnn, fp, &binary, NULL) < 0) {
        ST_WARNING("Failed to rnn_load_header.");
        goto ERR;
    }

    if (*rnn == NULL) {
        return 0;
    }

    if (binary) {
    } else {
    }

    return 0;

ERR:
    safe_rnn_destroy(*rnn);
    return -1;
}

static long rnn_save_header(rnn_t *rnn, FILE *fp, bool binary)
{
    long sz_pos;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (fwrite(&RNN_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
        ST_WARNING("Failed to write magic num.");
        return -1;
    }
    fprintf(fp, "\n<RNN>\n");

    if (rnn == NULL) {
        sz_pos = 0;
        if (fwrite(&sz_pos, sizeof(long), 1, fp) != 1) {
            ST_WARNING("Failed to write size.");
            return -1;
        }
        return 0;
    }

    sz_pos = ftell(fp);
    fseek(fp, sizeof(long), SEEK_CUR);

    fprintf(fp, "Version: %d\n", CONNLM_FILE_VERSION);
    fprintf(fp, "Binary: %s\n", bool2str(binary));

    return sz_pos;
}

int rnn_save(rnn_t *rnn, FILE *fp, bool binary)
{
    long sz;
    long sz_pos;
    long fpos;

    ST_CHECK_PARAM(fp == NULL, -1);

    sz_pos = rnn_save_header(rnn, fp, binary);
    if (sz_pos < 0) {
        ST_WARNING("Failed to rnn_save_header.");
        return -1;
    } else if (sz_pos == 0) {
        return 0;
    }

    fpos = ftell(fp);

    if (binary) {
    } else {
    }

    sz = ftell(fp) - fpos;
    fpos = ftell(fp);
    fseek(fp, sz_pos, SEEK_SET);
    if (fwrite(&sz, sizeof(long), 1, fp) != 1) {
        ST_WARNING("Failed to write size.");
        return -1;
    }
    fseek(fp, fpos, SEEK_SET);

    return 0;
}

/*
int rnn_get_version()
{
    return RNNLM_VERSION;
}

static void rnn_restore_wts(rnn_t * rnn)
{
    memcpy(rnn->wt_ih_w, rnn->wt_ih_w_bak,
            sizeof(real_t) * rnn->vocab_size * rnn->hidden_size);

    memcpy(rnn->wt_ih_h, rnn->wt_ih_h_bak,
            sizeof(real_t) * rnn->hidden_size * rnn->hidden_size);

    memcpy(rnn->wt_ho_c, rnn->wt_ho_c_bak,
            sizeof(real_t) * rnn->hidden_size * rnn->class_size);

    memcpy(rnn->wt_ho_w, rnn->wt_ho_w_bak,
            sizeof(real_t) * rnn->hidden_size * rnn->vocab_size);

    memcpy(rnn->wt_d, rnn->wt_d_bak,
            sizeof(real_t) * rnn->direct_size);
}

static void rnn_backup_wts(rnn_t * rnn)
{
    memcpy(rnn->wt_ih_w_bak, rnn->wt_ih_w,
            sizeof(real_t) * rnn->vocab_size * rnn->hidden_size);

    memcpy(rnn->wt_ih_h_bak, rnn->wt_ih_h,
            sizeof(real_t) * rnn->hidden_size * rnn->hidden_size);

    memcpy(rnn->wt_ho_c_bak, rnn->wt_ho_c,
            sizeof(real_t) * rnn->hidden_size * rnn->class_size);

    memcpy(rnn->wt_ho_w_bak, rnn->wt_ho_w,
            sizeof(real_t) * rnn->hidden_size * rnn->vocab_size);

    memcpy(rnn->wt_d_bak, rnn->wt_d,
            sizeof(real_t) * rnn->direct_size);
}

static real_t rrandom(real_t min, real_t max)
{
    return rand() / (real_t) RAND_MAX *(max - min) + min;
}

static int rnn_init(rnn_t * rnn)
{
    int i;
    int j;
    long long d;

    ST_CHECK_PARAM(rnn == NULL, -1);

    rnn->llogp = -100000000;
    rnn->logp = 0.0;
    rnn->alpha_divide = 0;

#if 0
    safe_free(rnn->ac_i_w);
    rnn->ac_i_w = (real_t *) malloc(sizeof(real_t) * rnn->vocab_size);
    if (rnn->ac_i_w == NULL) {
        ST_WARNING("Failed to malloc ac_i_w.");
        goto ERR;
    }

    safe_free(rnn->er_i_w);
    rnn->er_i_w = (real_t *) malloc(sizeof(real_t) * rnn->vocab_size);
    if (rnn->er_i_w == NULL) {
        ST_WARNING("Failed to malloc er_i_w.");
        goto ERR;
    }
#endif

    safe_free(rnn->ac_i_h);
    rnn->ac_i_h = (real_t *) malloc(sizeof(real_t) * rnn->hidden_size);
    if (rnn->ac_i_h == NULL) {
        ST_WARNING("Failed to malloc ac_i_h.");
        goto ERR;
    }

    safe_free(rnn->er_i_h);
    rnn->er_i_h = (real_t *) malloc(sizeof(real_t) * rnn->hidden_size);
    if (rnn->er_i_h == NULL) {
        ST_WARNING("Failed to malloc er_i_h.");
        goto ERR;
    }

    safe_free(rnn->ac_h);
    rnn->ac_h = (real_t *) malloc(sizeof(real_t) * rnn->hidden_size);
    if (rnn->ac_h == NULL) {
        ST_WARNING("Failed to malloc ac_h.");
        goto ERR;
    }

    safe_free(rnn->er_h);
    rnn->er_h = (real_t *) malloc(sizeof(real_t) * rnn->hidden_size);
    if (rnn->er_h == NULL) {
        ST_WARNING("Failed to malloc er_h.");
        goto ERR;
    }

    safe_free(rnn->ac_o_c);
    rnn->ac_o_c = (real_t *) malloc(sizeof(real_t) * rnn->class_size);
    if (rnn->ac_o_c == NULL) {
        ST_WARNING("Failed to malloc ac_o_c.");
        goto ERR;
    }

    safe_free(rnn->er_o_c);
    rnn->er_o_c = (real_t *) malloc(sizeof(real_t) * rnn->class_size);
    if (rnn->er_o_c == NULL) {
        ST_WARNING("Failed to malloc er_o_c.");
        goto ERR;
    }

    safe_free(rnn->ac_o_w);
    rnn->ac_o_w = (real_t *) malloc(sizeof(real_t) * rnn->vocab_size);
    if (rnn->ac_o_w == NULL) {
        ST_WARNING("Failed to malloc ac_o_w.");
        goto ERR;
    }

    safe_free(rnn->er_o_w);
    rnn->er_o_w = (real_t *) malloc(sizeof(real_t) * rnn->vocab_size);
    if (rnn->er_o_w == NULL) {
        ST_WARNING("Failed to malloc er_o_w.");
        goto ERR;
    }

    safe_free(rnn->wt_ih_w);
    rnn->wt_ih_w = (real_t *) malloc(sizeof(real_t)
            * rnn->vocab_size * rnn->hidden_size);
    if (rnn->wt_ih_w == NULL) {
        ST_WARNING("Failed to malloc wt_ih_w.");
        goto ERR;
    }

    safe_free(rnn->wt_ih_w_bak);
    rnn->wt_ih_w_bak = (real_t *) malloc(sizeof(real_t)
            * rnn->vocab_size * rnn->hidden_size);
    if (rnn->wt_ih_w_bak == NULL) {
        ST_WARNING("Failed to malloc wt_ih_w_bak.");
        goto ERR;
    }

    safe_free(rnn->wt_ih_h);
    rnn->wt_ih_h = (real_t *) malloc(sizeof(real_t)
            * rnn->hidden_size * rnn->hidden_size);
    if (rnn->wt_ih_h == NULL) {
        ST_WARNING("Failed to malloc wt_ih_h.");
        goto ERR;
    }

    safe_free(rnn->wt_ih_h_bak);
    rnn->wt_ih_h_bak = (real_t *) malloc(sizeof(real_t)
            * rnn->hidden_size * rnn->hidden_size);
    if (rnn->wt_ih_h_bak == NULL) {
        ST_WARNING("Failed to malloc wt_ih_h_bak.");
        goto ERR;
    }

    safe_free(rnn->wt_ho_c);
    rnn->wt_ho_c = (real_t *) malloc(sizeof(real_t)
            * rnn->hidden_size * rnn->class_size);
    if (rnn->wt_ho_c == NULL) {
        ST_WARNING("Failed to malloc wt_ho_c.");
        goto ERR;
    }

    safe_free(rnn->wt_ho_c_bak);
    rnn->wt_ho_c_bak = (real_t *) malloc(sizeof(real_t)
            * rnn->hidden_size * rnn->class_size);
    if (rnn->wt_ho_c_bak == NULL) {
        ST_WARNING("Failed to malloc wt_ho_c_bak.");
        goto ERR;
    }

    safe_free(rnn->wt_ho_w);
    rnn->wt_ho_w = (real_t *) malloc(sizeof(real_t)
            * rnn->hidden_size * rnn->vocab_size);
    if (rnn->wt_ho_w == NULL) {
        ST_WARNING("Failed to malloc wt_ho_w.");
        goto ERR;
    }

    safe_free(rnn->wt_ho_w_bak);
    rnn->wt_ho_w_bak = (real_t *) malloc(sizeof(real_t)
            * rnn->hidden_size * rnn->vocab_size);
    if (rnn->wt_ho_w_bak == NULL) {
        ST_WARNING("Failed to malloc wt_ho_w_bak.");
        goto ERR;
    }

    safe_free(rnn->wt_d);
    rnn->wt_d = (real_t *) malloc(sizeof(real_t) * rnn->direct_size);
    if (rnn->wt_d == NULL) {
        ST_WARNING("Failed to malloc wt_d.");
        goto ERR;
    }

    safe_free(rnn->wt_d_bak);
    rnn->wt_d_bak = (real_t *) malloc(sizeof(real_t)
            * rnn->direct_size);
    if (rnn->wt_d_bak == NULL) {
        ST_WARNING("Failed to malloc wt_d_bak.");
        goto ERR;
    }

    if (rnn->direct_order > 0) {
        safe_free(rnn->d_hist);
        rnn->d_hist = (int *) malloc(sizeof(int) * rnn->direct_order);
        if (rnn->d_hist == NULL) {
            ST_WARNING("Failed to malloc d_hist.");
            goto ERR;
        }

        safe_free(rnn->d_hash);
        rnn->d_hash = (hash_t *) malloc(sizeof(hash_t)
                * rnn->direct_order);
        if (rnn->d_hash == NULL) {
            ST_WARNING("Failed to malloc d_hash.");
            goto ERR;
        }
    }

    if (rnn->bptt > 1) {
        safe_free(rnn->bptt_hist);
        rnn->bptt_hist = (int *) malloc(sizeof(int)
                * (rnn->bptt + rnn->bptt_block));
        if (rnn->bptt_hist == NULL) {
            ST_WARNING("Failed to malloc bptt_hist.");
            goto ERR;
        }

        safe_free(rnn->ac_bptt_h);
        rnn->ac_bptt_h = (real_t *) malloc(sizeof(real_t)
                * rnn->hidden_size * (rnn->bptt + rnn->bptt_block));
        if (rnn->ac_bptt_h == NULL) {
            ST_WARNING("Failed to malloc ac_bptt_h.");
            goto ERR;
        }

        safe_free(rnn->er_bptt_h);
        rnn->er_bptt_h = (real_t *) malloc(sizeof(real_t)
                * rnn->hidden_size * (rnn->bptt_block));
        if (rnn->er_bptt_h == NULL) {
            ST_WARNING("Failed to malloc er_bptt_h.");
            goto ERR;
        }

        safe_free(rnn->wt_bptt_ih_w);
        rnn->wt_bptt_ih_w = (real_t *) malloc(sizeof(real_t)
                * rnn->vocab_size * rnn->hidden_size);
        if (rnn->wt_bptt_ih_w == NULL) {
            ST_WARNING("Failed to malloc wt_bptt_ih_w.");
            goto ERR;
        }

        safe_free(rnn->wt_bptt_ih_h);
        rnn->wt_bptt_ih_h = (real_t *) malloc(sizeof(real_t)
                * rnn->hidden_size * rnn->hidden_size);
        if (rnn->wt_bptt_ih_h == NULL) {
            ST_WARNING("Failed to malloc wt_bptt_ih_h.");
            goto ERR;
        }

        for (i = 0; i < (rnn->bptt + rnn->bptt_block); i++) {
            rnn->bptt_hist[i] = -1;
        }

        for (i = 0; i < rnn->hidden_size
                * (rnn->bptt + rnn->bptt_block); i++) {
            rnn->ac_bptt_h[i] = 0;
        }

        for (i = 0; i < rnn->hidden_size * rnn->bptt_block; i++) {
            rnn->er_bptt_h[i] = 0;
        }

        for (i = 0; i < rnn->vocab_size*rnn->hidden_size; i++) {
            rnn->wt_bptt_ih_w[i] = 0;
        }

        for (i = 0; i < rnn->hidden_size*rnn->hidden_size; i++) {
            rnn->wt_bptt_ih_h[i] = 0;
        }
    }

#if 0
    for (i = 0; i < rnn->vocab_size; i++) {
        rnn->ac_i_w[i] = 0;
        rnn->er_i_w[i] = 0;
    }
#endif

    for (i = 0; i < rnn->hidden_size; i++) {
        rnn->ac_i_h[i] = 0;
        rnn->er_i_h[i] = 0;
    }

    for (i = 0; i < rnn->hidden_size; i++) {
        rnn->ac_h[i] = 0;
        rnn->er_h[i] = 0;
    }

    for (i = 0; i < rnn->class_size; i++) {
        rnn->ac_o_c[i] = 0;
        rnn->er_o_c[i] = 0;
    }

    for (i = 0; i < rnn->vocab_size; i++) {
        rnn->ac_o_w[i] = 0;
        rnn->er_o_w[i] = 0;
    }

#if 0
    for (i = 0; i < rnn->vocab_size * rnn->hidden_size; i++) {
        rnn->wt_ih_w[i] = rrandom(-0.1, 0.1) + rrandom(-0.1, 0.1) + rrandom(-0.1, 0.1);
    }

    for (i = 0; i < rnn->hidden_size * rnn->hidden_size; i++) {
        rnn->wt_ih_h[i] = rrandom(-0.1, 0.1) + rrandom(-0.1, 0.1) + rrandom(-0.1, 0.1);
    }


    for (i = 0; i < rnn->hidden_size * rnn->vocab_size; i++) {
        rnn->wt_ho_w[i] = rrandom(-0.1, 0.1) + rrandom(-0.1, 0.1) + rrandom(-0.1, 0.1);
    }

    for (i = 0; i < rnn->hidden_size * rnn->class_size; i++) {
        rnn->wt_ho_c[i] = rrandom(-0.1, 0.1) + rrandom(-0.1, 0.1) + rrandom(-0.1, 0.1);
    }
#else
    for (i = 0; i < rnn->hidden_size; i++) {
        for (j = 0; j < rnn->vocab_size; j++) {
            rnn->wt_ih_w[j + i*rnn->vocab_size] =
                rrandom(-0.1, 0.1) + rrandom(-0.1, 0.1) + rrandom(-0.1, 0.1);
        }

        for (j = 0; j < rnn->hidden_size; j++) {
            rnn->wt_ih_h[j + i*rnn->hidden_size] =
                rrandom(-0.1, 0.1) + rrandom(-0.1, 0.1) + rrandom(-0.1, 0.1);
        }
    }

    for (i = 0; i < rnn->vocab_size; i++) {
        for (j = 0; j < rnn->hidden_size; j++) {
            rnn->wt_ho_w[j + i*rnn->hidden_size] =
                rrandom(-0.1, 0.1) + rrandom(-0.1, 0.1) + rrandom(-0.1, 0.1);
        }
    }

    for (i = 0; i < rnn->class_size; i++) {
        for (j = 0; j < rnn->hidden_size; j++) {
            rnn->wt_ho_c[j + i*rnn->hidden_size] =
                rrandom(-0.1, 0.1) + rrandom(-0.1, 0.1) + rrandom(-0.1, 0.1);
        }
    }
#endif

    for (d = 0; d < rnn->direct_size; d++) {
        rnn->wt_d[d] = 0;
    }

    return 0;

  ERR:
    rnn_destroy(rnn);
    safe_free(rnn);
    return -1;
}

int rnn_save(rnn_t * rnn, bool binary, FILE *fo)
{
    long long aa;
    int a, b;
    float f;

    ST_CHECK_PARAM(rnn == NULL || fo == NULL, -1);

    fprintf(fo, "version: %d\n", RNNLM_VERSION);
    fprintf(fo, "file format: %d\n\n", binary);

    fprintf(fo, "training data file: %s\n", rnn->train_file);
    fprintf(fo, "validation data file: %s\n\n", rnn->valid_file);

    fprintf(fo, "last probability of validation data: %f\n", rnn->llogp);
    fprintf(fo, "number of finished iterations: %d\n", rnn->iter);

    fprintf(fo, "current position in training data: %ld\n", rnn->train_cur_pos);
    fprintf(fo, "current probability of training data: %f\n", rnn->logp);
    fprintf(fo, "save after processing # words: %ld\n", rnn->anti_k);
    fprintf(fo, "# of training words: %ld\n", rnn->train_words);

    fprintf(fo, "input layer size: %d\n", rnn->hidden_size + rnn->vocab_size);
    fprintf(fo, "hidden layer size: %d\n", rnn->hidden_size);
    fprintf(fo, "compression layer size: %d\n", 0);
    fprintf(fo, "output layer size: %d\n", rnn->vocab_size + rnn->class_size);

    fprintf(fo, "direct connections: %lld\n", rnn->direct_size);
    fprintf(fo, "direct order: %d\n", rnn->direct_order);

    fprintf(fo, "bptt: %d\n", rnn->bptt);
    fprintf(fo, "bptt block: %d\n", rnn->bptt_block);

    fprintf(fo, "vocabulary size: %d\n", rnn->vocab_size);
    fprintf(fo, "class size: %d\n", rnn->class_size);

    fprintf(fo, "old classes: %d\n", 0);
    fprintf(fo, "independent sentences mode: %d\n", rnn->independent);

    fprintf(fo, "starting learning rate: %f\n", rnn->starting_alpha);
    fprintf(fo, "current learning rate: %f\n", rnn->alpha);
    fprintf(fo, "learning rate decrease: %d\n", rnn->alpha_divide);
    fprintf(fo, "\n");

    fprintf(fo, "\nVocabulary:\n");
    for (a = 0; a < rnn->vocab_size; a++) {
        fprintf(fo, "%6d\t%10d\t%s\t%d\n", a, rnn->cnts[a], 
                st_alphabet_get_label(rnn->vocab, a),
                rnn->w2c[a]);
    }

#define WEIGHT_FMT "%f\n"
#define DIRECT_FMT "%f\n"

    if (filetype == TEXT) {
        fprintf(fo, "\nHidden layer activation:\n");
        for (a = 0; a < rnn->hidden_size; a++) {
            fprintf(fo, WEIGHT_FMT, rnn->ac_h[a]);
        }
    } else if (filetype == BINARY) {
        for (a = 0; a < rnn->hidden_size; a++) {
            fwrite(&rnn->ac_h[a], sizeof(real_t), 1, fo);
        }
    }

    if (filetype == TEXT) {
        fprintf(fo, "\nWeights 0->1:\n");

        for (a = 0; a < rnn->hidden_size; a++) {
            for (b = 0; b < rnn->vocab_size; b++) {
                fprintf(fo, WEIGHT_FMT, 
                        rnn->wt_ih_w[b + a*rnn->vocab_size]);
            }

            for (b = 0; b < rnn->hidden_size; b++) {
                fprintf(fo, WEIGHT_FMT, 
                        rnn->wt_ih_h[b + a*rnn->hidden_size]);
            }
        }
    } else if (filetype == BINARY) {
        for (a = 0; a < rnn->hidden_size; a++) {
            for (b = 0; b < rnn->vocab_size; b++) {
                f = rnn->wt_ih_w[b + a*rnn->vocab_size];
                fwrite(&f, sizeof(float), 1, fo);
            }

            for (b = 0; b < rnn->hidden_size; b++) {
                f = rnn->wt_ih_h[b + a*rnn->hidden_size];
                fwrite(&f, sizeof(float), 1, fo);
            }
        }
    }

    if (filetype == TEXT) {
        fprintf(fo, "\n\nWeights 1->2:\n");

        for (a = 0; a < rnn->vocab_size; a++) {
            for (b = 0; b < rnn->hidden_size; b++) {
                fprintf(fo, WEIGHT_FMT, 
                        rnn->wt_ho_w[b + a*rnn->hidden_size]);
            }
        }

        for (a = 0; a < rnn->class_size; a++) {
            for (b = 0; b < rnn->hidden_size; b++) {
                fprintf(fo, WEIGHT_FMT, 
                        rnn->wt_ho_c[b + a*rnn->hidden_size]);
            }
        }
    } else if (filetype == BINARY) {
        for (a = 0; a < rnn->vocab_size; a++) {
            for (b = 0; b < rnn->hidden_size; b++) {
                f = rnn->wt_ho_w[b + a*rnn->hidden_size];
                fwrite(&f, sizeof(float), 1, fo);
            }
        }

        for (a = 0; a < rnn->class_size; a++) {
            for (b = 0; b < rnn->hidden_size; b++) {
                f = rnn->wt_ho_c[b + a*rnn->hidden_size];
                fwrite(&f, sizeof(float), 1, fo);
            }
        }
    }

    if (filetype == TEXT) {
        fprintf(fo, "\nDirect connections:\n");
        for (aa = 0; aa < rnn->direct_size; aa++) {
            fprintf(fo, DIRECT_FMT, rnn->wt_d[aa]);
        }
    } else if (filetype == BINARY) {
        for (aa = 0; aa < rnn->direct_size; aa++) {
            f = rnn->wt_d[aa];
            fwrite(&f, sizeof(float), 1, fo);
        }
    }

    return 0;
}

int rnn_load(rnn_t * rnn, FILE *fi)
{
    char word[MAX_SYM_LEN];
    long long aa;
    int a, b, ver;
    float fl;
    double d;

    ST_CHECK_PARAM(rnn == NULL || fi == NULL, -1);

    goto_delimiter(':', fi);
    fscanf(fi, "%d", &ver);
    if (ver != RNNLM_VERSION) {
        ST_WARNING("Unknown version of file\n");
        return -1;
    }

    goto_delimiter(':', fi);
    fscanf(fi, "%d", &rnn->filetype);

    goto_delimiter(':', fi);
    fscanf(fi, "%s", rnn->train_file);

    goto_delimiter(':', fi);
    fscanf(fi, "%s", rnn->valid_file);
    goto_delimiter(':', fi);
    fscanf(fi, "%lf", &rnn->llogp);
    goto_delimiter(':', fi);
    fscanf(fi, "%d", &rnn->iter);
    goto_delimiter(':', fi);
    fscanf(fi, "%ld", &rnn->train_cur_pos);
    goto_delimiter(':', fi);
    fscanf(fi, "%lf", &rnn->logp);
    goto_delimiter(':', fi);
    fscanf(fi, "%ld", &rnn->anti_k);
    goto_delimiter(':', fi);
    fscanf(fi, "%ld", &rnn->train_words);
    goto_delimiter(':', fi);
    fscanf(fi, "%d", &a);
    goto_delimiter(':', fi);
    fscanf(fi, "%d", &rnn->hidden_size);
    rnn->vocab_size = a - rnn->hidden_size;
    goto_delimiter(':', fi);
    fscanf(fi, "%d", &a);
    goto_delimiter(':', fi);
    fscanf(fi, "%d", &a);
    rnn->class_size = a - rnn->vocab_size;

    goto_delimiter(':', fi);
    fscanf(fi, "%lld", &rnn->direct_size);
    goto_delimiter(':', fi);
    fscanf(fi, "%d", &rnn->direct_order);
    goto_delimiter(':', fi);
    fscanf(fi, "%d", &rnn->bptt);
    goto_delimiter(':', fi);
    fscanf(fi, "%d", &rnn->bptt_block);
    goto_delimiter(':', fi);
    fscanf(fi, "%d", &a);
    if (a != rnn->vocab_size) {
        ST_WARNING("Vocab size not equal.");
        return -1;
    }
    goto_delimiter(':', fi);
    fscanf(fi, "%d", &a);
    if (a != rnn->class_size) {
        ST_WARNING("Class size not equal.");
        return -1;
    }
    goto_delimiter(':', fi);
    fscanf(fi, "%d", &a);
    goto_delimiter(':', fi);
    fscanf(fi, "%d", &rnn->independent);
    goto_delimiter(':', fi);
    fscanf(fi, "%lf", &d);
    rnn->starting_alpha = d;
    goto_delimiter(':', fi);
    fscanf(fi, "%lf", &d);
    rnn->alpha = d;
    goto_delimiter(':', fi);
    fscanf(fi, "%d", &rnn->alpha_divide);

    safe_st_alphabet_destroy(rnn->vocab);
    rnn->vocab = st_alphabet_create(rnn->vocab_size);
    if (rnn->vocab == NULL) {
        ST_WARNING("Failed to st_alphabet_create vocab.");
        return -1;
    }

    safe_free(rnn->cnts);
    rnn->cnts = (int *) malloc(sizeof(int) * rnn->vocab_size);
    if (rnn->cnts == NULL) {
        ST_WARNING("Failed to malloc cnts.");
        return -1;
    }
    memset(rnn->cnts, 0, sizeof(int) * rnn->vocab_size);

    safe_free(rnn->w2c);
    rnn->w2c = (int *) malloc(sizeof(int) * rnn->vocab_size);
    if (rnn->w2c == NULL) {
        ST_WARNING("Failed to malloc w2c.");
        return -1;
    }
    memset(rnn->w2c, 0, sizeof(int) * rnn->vocab_size);

    goto_delimiter(':', fi);
    for (a = 0; a < rnn->vocab_size; a++) {
        fscanf(fi, "%d%d", &b, rnn->cnts + a);
        if (rnn_read_word(word, MAX_SYM_LEN, fi) < 0) {
            ST_WARNING("Failed to rnn_read_word.");
            return -1;
        }
        if (st_alphabet_add_label(rnn->vocab, word) < 0) {
            ST_WARNING("Failed to st_alphabet_add_sym. word[%s] id[%d].",
                    word, a);
            return -1;
        }
        fscanf(fi, "%d", rnn->w2c + a);
    }

    if (rnn_init(rnn) < 0) {
        ST_WARNING("Failed to rnn_init.");
        return -1;
    }

    if (rnn->filetype == TEXT) {
        goto_delimiter(':', fi);
        for (a = 0; a < rnn->hidden_size; a++) {
            fscanf(fi, "%lf", &d);
            rnn->ac_h[a] = d;
        }
    } else if (rnn->filetype == BINARY) {
        for (a = 0; a < rnn->hidden_size; a++) {
            fread(&fl, sizeof(float), 1, fi);
            rnn->ac_h[a] = fl;
        }
    }

    if (rnn->filetype == TEXT) {
        goto_delimiter(':', fi);
        for (a = 0; a < rnn->hidden_size; a++) {
            for (b = 0; b < rnn->vocab_size; b++) {
                fscanf(fi, "%lf", &d);
                rnn->wt_ih_w[b + a*rnn->vocab_size] = d;
            }

            for (b = 0; b < rnn->hidden_size; b++) {
                fscanf(fi, "%lf", &d);
                rnn->wt_ih_h[b + a*rnn->hidden_size] = d;
            }
        }
    } else if (rnn->filetype == BINARY) {
        for (a = 0; a < rnn->hidden_size; a++) {
            for (b = 0; b < rnn->vocab_size; b++) {
                fread(&fl, sizeof(float), 1, fi);
                rnn->wt_ih_w[b + a*rnn->vocab_size] = fl;
            }

            for (b = 0; b < rnn->hidden_size; b++) {
                fread(&fl, sizeof(float), 1, fi);
                rnn->wt_ih_h[b + a*rnn->hidden_size] = fl;
            }
        }
    }

    if (rnn->filetype == TEXT) {
        goto_delimiter(':', fi);
        for (a = 0; a < rnn->vocab_size; a++) {
            for (b = 0; b < rnn->hidden_size; b++) {
                fscanf(fi, "%lf", &d);
                rnn->wt_ho_w[b + a*rnn->hidden_size] = d;
            }
        }

        for (a = 0; a < rnn->class_size; a++) {
            for (b = 0; b < rnn->hidden_size; b++) {
                fscanf(fi, "%lf", &d);
                rnn->wt_ho_c[b + a*rnn->hidden_size] = d;
            }
        }
    } else if (rnn->filetype == BINARY) {
        for (a = 0; a < rnn->vocab_size; a++) {
            for (b = 0; b < rnn->hidden_size; b++) {
                fread(&fl, sizeof(float), 1, fi);
                rnn->wt_ho_w[b + a*rnn->hidden_size] = fl;
            }
        }

        for (a = 0; a < rnn->class_size; a++) {
            for (b = 0; b < rnn->hidden_size; b++) {
                fread(&fl, sizeof(float), 1, fi);
                rnn->wt_ho_c[b + a*rnn->hidden_size] = fl;
            }
        }
    }

    if (rnn->filetype == TEXT) {
        goto_delimiter(':', fi);
        for (aa = 0; aa < rnn->direct_size; aa++) {
            fscanf(fi, "%lf", &d);
            rnn->wt_d[aa] = d;
        }
    } else if (rnn->filetype == BINARY) {
        for (aa = 0; aa < rnn->direct_size; aa++) {
            fread(&fl, sizeof(float), 1, fi);
            rnn->wt_d[aa] = fl;
        }
    }

    rnn_backup_wts(rnn);

    return 0;
}

static int rnn_save_net(rnn_t * rnn)
{
    FILE *fp = NULL;

    fp = fopen(rnn->out_rnn_file, "wb");
    if (fp == NULL) {
        ST_WARNING("Failed to open rnn file[%s].", rnn->out_rnn_file);
        goto ERR;
    }

    if (rnn_save(rnn, rnn->filetype, fp) < 0) {
        ST_WARNING("Failed to rnn_save.");
        goto ERR;
    }

    safe_fclose(fp);
    return 0;

ERR:
    safe_fclose(fp);
    return -1;
}

int rnn_load_net(rnn_t * rnn)
{
    FILE *fp = NULL;

    fp = fopen(rnn->in_rnn_file, "rb");
    if (fp == NULL) {
        ST_WARNING("Failed to open in rnn file[%s].", 
                rnn->in_rnn_file);
        goto ERR;
    }

    if (rnn_load(rnn, fp) < 0) {
        ST_WARNING("Failed to rnn_load.");
        goto ERR;
    }

    safe_fclose(fp);
    return 0;

ERR:
    safe_fclose(fp);
    return -1;
}

static int rnn_flush_net(rnn_t * rnn)
{
    int i;

    ST_CHECK_PARAM(rnn == NULL, -1);

#if 0
    for (i = 0; i < rnn->vocab_size; i++) {
        rnn->ac_i_w[i] = 0;
        rnn->er_i_w[i] = 0;
    }
#endif

    for (i = 0; i < rnn->hidden_size; i++) {
        rnn->ac_i_h[i] = 0.1;
        rnn->er_i_h[i] = 0;
    }

    for (i = 0; i < rnn->hidden_size; i++) {
        rnn->ac_h[i] = 0;
        rnn->er_h[i] = 0;
    }

    for (i = 0; i < rnn->class_size; i++) {
        rnn->ac_o_c[i] = 0;
        rnn->er_o_c[i] = 0;
    }

    for (i = 0; i < rnn->vocab_size; i++) {
        rnn->ac_o_w[i] = 0;
        rnn->er_o_w[i] = 0;
    }

    for (i = 0; i < rnn->direct_order; i++) {
        rnn->d_hist[i] = -1;
    }

    for (i = 0; i < (rnn->bptt + rnn->bptt_block); i++) {
        rnn->bptt_hist[i] = -1;
    }

    return 0;
}

static int rnn_reset_net(rnn_t * rnn)
{
    ST_CHECK_PARAM(rnn == NULL, -1);
    return 0;
}

static void rnn_get_hash(hash_t *hash, hash_t init_val,
        int *hist, int order)
{
    int a;
    int b;

    for (a = 0; a < order; a++) {
        if (a > 0) {
            if (hist[a - 1] < 0) { // if OOV was in history, do not use
                                   //this N-gram feature and higher orders
                for ( ; a < order; a++) {
                    hash[a] = 0;
                }
                break;
            }
        }

        hash[a] = PRIMES[0] * PRIMES[1] * init_val;
        for (b = 1; b <= a; b++) {
            hash[a] += PRIMES[(a*PRIMES[b] + b) % PRIMES_SIZE]
                * (hash_t)(hist[b-1] + 1);
        }
    }
}

static void rnn_compute_direct_class(rnn_t *rnn)
{
    int a;
    int o;

#ifdef _TIME_PROF_
    struct timeval tts;
    struct timeval tte;

    gettimeofday(&tts, NULL);
#endif

    rnn_get_hash(rnn->d_hash, (hash_t)1,
            rnn->d_hist, rnn->direct_order);

    for (a = 0; a < rnn->direct_order; a++) {
        if (rnn->d_hash[a] == 0) {
            break;
        }
        rnn->d_hash[a] = rnn->d_hash[a] % (rnn->direct_size / 2);
    }

    for (o = 0; o < rnn->class_size; o++) {
        for (a = 0; a < rnn->direct_order; a++) {
            if (!rnn->d_hash[a]) {
                break;
            }

            rnn->ac_o_c[o] += rnn->wt_d[rnn->d_hash[a]];
            rnn->d_hash[a]++;
            rnn->d_hash[a] = rnn->d_hash[a] % (rnn->direct_size / 2);
        }
    }

#ifdef _TIME_PROF_
    gettimeofday(&tte, NULL);
    direct_total += UTIMEDIFF(tts, tte);
#endif
}

static void rnn_compute_direct_word(rnn_t *rnn, int c, int s, int e)
{
    int a;
    int o;

#ifdef _TIME_PROF_
    struct timeval tts;
    struct timeval tte;

    gettimeofday(&tts, NULL);
#endif

    rnn_get_hash(rnn->d_hash, (hash_t)(c + 1), rnn->d_hist,
            rnn->direct_order);

    for (a = 0; a < rnn->direct_order; a++) {
        if (rnn->d_hash[a] == 0) {
            break;
        }
        rnn->d_hash[a] = (rnn->d_hash[a] % (rnn->direct_size / 2))
            + rnn->direct_size / 2;
    }

    for (o = s; o < e; o++) {
        for (a = 0; a < rnn->direct_order; a++) {
            if (!rnn->d_hash[a]) {
                break;
            }
            rnn->ac_o_w[o] += rnn->wt_d[rnn->d_hash[a]];
            rnn->d_hash[a]++;
            rnn->d_hash[a] = (rnn->d_hash[a] % (rnn->direct_size/2))
                + rnn->direct_size / 2;
        }
    }

#ifdef _TIME_PROF_
    gettimeofday(&tte, NULL);
    direct_total += UTIMEDIFF(tts, tte);
#endif
}

static int rnn_compute_net(rnn_t * rnn, int last_word, int word)
{
    int h;

    int c;
    int s;
    int e;

    ST_CHECK_PARAM(rnn == NULL, -1);

    // PROPAGATE input -> hidden
    matXvec(rnn->ac_h, rnn->wt_ih_h, rnn->ac_i_h,
            rnn->hidden_size, rnn->hidden_size);

    if (last_word >= 0) {
        for (h = 0; h < rnn->hidden_size; h++) {
            rnn->ac_h[h] +=
                rnn->wt_ih_w[last_word + h*rnn->vocab_size];
        }
    }

    // ACTIVATE hidden
    sigmoid(rnn->ac_h, rnn->hidden_size);

    // PROPAGATE hidden -> output (class)
    matXvec(rnn->ac_o_c, rnn->wt_ho_c, rnn->ac_h,
            rnn->class_size, rnn->hidden_size);

    // PROPAGATE input -> output (class)
    if (rnn->direct_size > 0) {
        rnn_compute_direct_class(rnn);
    }

    // ACTIVATE output(class)
    softmax(rnn->ac_o_c, rnn->class_size);

    // PROPAGATE hidden -> output (word)
    if (word < 0) {
        return 0;
    }

    c = rnn->w2c[word];
    s = rnn->c2w_s[c];
    e = rnn->c2w_e[c];
    matXvec(rnn->ac_o_w + s, rnn->wt_ho_w + s * rnn->hidden_size,
            rnn->ac_h, e - s, rnn->hidden_size);

    // PROPAGATE input -> output (word)
    if (rnn->direct_size > 0) {
        rnn_compute_direct_word(rnn, c, s, e);
    }

    // ACTIVATE output(word)
    softmax(rnn->ac_o_w + s, e - s);

    return 0;
}

static void rnn_learn_direct_class(rnn_t *rnn, real_t beta3)
{
    int a;
    int o;

#ifdef _TIME_PROF_
    struct timeval tts;
    struct timeval tte;

    gettimeofday(&tts, NULL);
#endif

    rnn_get_hash(rnn->d_hash, (hash_t)1,
            rnn->d_hist, rnn->direct_order);

    for (a = 0; a < rnn->direct_order; a++) {
        if (rnn->d_hash[a] == 0) {
            break;
        }
        rnn->d_hash[a] = rnn->d_hash[a] % (rnn->direct_size / 2);
    }

    for (o = 0; o < rnn->class_size; o++) {
        for (a = 0; a < rnn->direct_order; a++) {
            if (!rnn->d_hash[a]) {
                break;
            }

            rnn->wt_d[rnn->d_hash[a]] += rnn->er_o_c[o]
                - beta3 * rnn->wt_d[rnn->d_hash[a]];
            rnn->d_hash[a]++;
            rnn->d_hash[a] = rnn->d_hash[a] % (rnn->direct_size / 2);
        }
    }

#ifdef _TIME_PROF_
    gettimeofday(&tte, NULL);
    direct_total += UTIMEDIFF(tts, tte);
#endif
}

static void rnn_learn_direct_word(rnn_t *rnn, int c, int s, int e,
        real_t beta3)
{
    int a;
    int o;

#ifdef _TIME_PROF_
    struct timeval tts;
    struct timeval tte;

    gettimeofday(&tts, NULL);
#endif

    rnn_get_hash(rnn->d_hash, (hash_t)(c + 1), rnn->d_hist,
            rnn->direct_order);

    for (a = 0; a < rnn->direct_order; a++) {
        if (rnn->d_hash[a] == 0) {
            break;
        }
        rnn->d_hash[a] = (rnn->d_hash[a] % (rnn->direct_size / 2))
            + rnn->direct_size / 2;
    }

    for (o = s; o < e; o++) {
        for (a = 0; a < rnn->direct_order; a++) {
            if (!rnn->d_hash[a]) {
                break;
            }

            rnn->wt_d[rnn->d_hash[a]] += rnn->er_o_w[o]
                - beta3 * rnn->wt_d[rnn->d_hash[a]];
            rnn->d_hash[a]++;
            rnn->d_hash[a] = (rnn->d_hash[a] % (rnn->direct_size/2))
                + rnn->direct_size / 2;
        }
    }

#ifdef _TIME_PROF_
    gettimeofday(&tte, NULL);
    direct_total += UTIMEDIFF(tts, tte);
#endif
}

static void propagate_error(real_t *dst, real_t *vec, real_t *mat,
        int mat_col, int in_vec_size, real_t gradient_cutoff)
{
    int a;

    vecXmat(dst, vec, mat, mat_col, in_vec_size);

    if (gradient_cutoff > 0) {
        for (a = 0; a < mat_col; a++) {
            if (dst[a] > gradient_cutoff) {
                dst[a] = gradient_cutoff;
            } else if (dst[a] < -gradient_cutoff) {
                dst[a] = -gradient_cutoff;
            }
        }
    }
}

static int rnn_learn_net(rnn_t * rnn, int last_word, int word)
{
    real_t beta2;
    real_t beta3;

    int o;
    int c;
    int s;
    int e;

    int a;
    int h;
    int t;
    int i;

    ST_CHECK_PARAM(rnn == NULL, -1);

    if (word < 0) {
        return 0;
    }

    beta2 = rnn->alpha * rnn->beta;
    beta3 = beta2 * 1;

    // compute ERROR (word)
    c = rnn->w2c[word];
    s = rnn->c2w_s[c];
    e = rnn->c2w_e[c];

    for (o = s; o < e; o++) {
        rnn->er_o_w[o] = (0 - rnn->ac_o_w[o]) * rnn->alpha;
    }
    rnn->er_o_w[word] = (1 - rnn->ac_o_w[word]) * rnn->alpha;

    // compute ERROR (class)
    for (o = 0; o < rnn->class_size; o++) {
        rnn->er_o_c[o] = (0 - rnn->ac_o_c[o]) * rnn->alpha;
    }
    rnn->er_o_c[c] = (1 - rnn->ac_o_c[c]) * rnn->alpha;

    if (rnn->direct_size > 0) {
        if (word >= 0) {
            // LEARN output -> input (word)
            rnn_learn_direct_word(rnn, c, s, e, beta3);
        }

        // LEARN output -> input (class)
        rnn_learn_direct_class(rnn, beta3);
    }


    for (h = 0; h < rnn->hidden_size; h++) {
        rnn->er_h[h] = 0;
    }

    // BACK-PROPAGATE output -> hidden (word)
    propagate_error(rnn->er_h, rnn->er_o_w + s,
            rnn->wt_ho_w + s * rnn->hidden_size,
            rnn->hidden_size, e - s, rnn->gradient_cutoff);

    if (rnn->counter % REGULARIZATION_STEP == 0) {
        a = s * rnn->hidden_size;
        for (o = s; o < e; o++) {
            for (h = 0; h < rnn->hidden_size; h++) {
                rnn->wt_ho_w[a + h] += rnn->er_o_w[o] * rnn->ac_h[h]
                    - rnn->wt_ho_w[a + h]*beta2;
            }
            a += rnn->hidden_size;
        }
    } else {
        a = s * rnn->hidden_size;
        for (o = s; o < e; o++) {
            for (h = 0; h < rnn->hidden_size; h++) {
                rnn->wt_ho_w[a + h] += rnn->er_o_w[o] * rnn->ac_h[h];
            }
            a += rnn->hidden_size;
        }
    }

    // BACK-PROPAGATE output -> hidden (class)
    propagate_error(rnn->er_h, rnn->er_o_c, rnn->wt_ho_c,
            rnn->hidden_size, rnn->class_size, rnn->gradient_cutoff);

    if (rnn->counter % REGULARIZATION_STEP == 0) {
        a = 0;
        for (o = 0; o < rnn->class_size; o++) {
            for (h = 0; h < rnn->hidden_size; h++) {
                rnn->wt_ho_c[a+h] += rnn->er_o_c[o] * rnn->ac_h[h]
                    - rnn->wt_ho_c[a+h]*beta2;
            }
            a += rnn->hidden_size;
        }
    } else {
        a = 0;
        for (o = 0; o < rnn->class_size; o++) {
            for (h = 0; h < rnn->hidden_size; h++) {
                rnn->wt_ho_c[a+h] += rnn->er_o_c[o] * rnn->ac_h[h];
            }
            a += rnn->hidden_size;
        }
    }

    // BACK-PROPAGATE hidden -> input
    if (rnn->bptt <= 1) { // normal BP
        // error derivation
        for (h = 0; h < rnn->hidden_size; h++) {
            rnn->er_h[h] *= rnn->ac_h[h]*(1-rnn->ac_h[h]);
        }

        // weight update (word -> hidden)
        a = last_word;
        if (a >= 0) {
            if ((rnn->counter % 10) == 0) {
                for (h = 0; h < rnn->hidden_size; h++) {
                    rnn->wt_ih_w[a+h*rnn->vocab_size] +=
                        rnn->er_h[h]// *rnn->ac_i_w[a]
                        - rnn->wt_ih_w[a+h*rnn->vocab_size]*beta2;
                }
            } else {
                for (h = 0; h < rnn->hidden_size; h++) {
                    rnn->wt_ih_w[a+h*rnn->vocab_size] +=
                        rnn->er_h[h];// *rnn->ac_i_w[a];  ==> 1
                }
            }
        }

        // weight update (hidden -> hidden)
        if ((rnn->counter % 10) == 0) {
            for (h = 0; h < rnn->hidden_size; h++) {
                for (a = 0; a < rnn->hidden_size; a++) {
                    rnn->wt_ih_h[a+h*rnn->hidden_size] +=
                        rnn->er_h[h]*rnn->ac_i_h[a]
                        - rnn->wt_ih_h[a+h*rnn->hidden_size]*beta2;
                }
            }
        } else {
            for (h = 0; h < rnn->hidden_size; h++) {
                for (a = 0; a < rnn->hidden_size; a++) {
                    rnn->wt_ih_h[a+h*rnn->hidden_size] +=
                        rnn->er_h[h]*rnn->ac_i_h[a];
                }
            }
        }

    } else { // BPTT
        for (h = 0; h < rnn->hidden_size; h++) {
            rnn->ac_bptt_h[h] = rnn->ac_h[h];
            rnn->er_bptt_h[h] = rnn->er_h[h];
        }

        if (((rnn->counter % rnn->bptt_block) == 0)
                || (rnn->independent && (word == 0))) {
            for (t = 0; t < rnn->bptt + rnn->bptt_block - 2; t++) {
                // error derivation
                for (h = 0; h < rnn->hidden_size; h++) {
                    rnn->er_h[h] *= rnn->ac_h[h]*(1 - rnn->ac_h[h]);
                }

                // back-propagate errors hidden -> input
                // weight update (word)
                a = rnn->bptt_hist[t];
                if (a != -1) {
                    for (h = 0; h < rnn->hidden_size; h++) {
                        rnn->wt_bptt_ih_w[a + h*rnn->vocab_size] +=
                            rnn->er_h[h];// *rnn->ac_i_w[a]; should be 1
                    }
                }

                for(i = 0; i < rnn->hidden_size; i++) {
                    rnn->er_i_h[i] = 0;
                }

                // error propagation (hidden)
                propagate_error(rnn->er_i_h, rnn->er_h,
                        rnn->wt_ih_h, rnn->hidden_size,
                        rnn->hidden_size, rnn->gradient_cutoff);
                // weight update (hidden)
                for (a = 0; a < rnn->hidden_size; a++) {
                    for (h = 0; h < rnn->hidden_size; h++) {
                        rnn->wt_bptt_ih_h[a + h*rnn->hidden_size] +=
                            rnn->er_h[h]*rnn->ac_i_h[a];
                    }
                }

                //propagate error from time T-n to T-n-1
                if (t < rnn->bptt_block - 1) {
                    for (h = 0; h < rnn->hidden_size; h++) {
                        rnn->er_h[h] = rnn->er_i_h[h]
                            + rnn->er_bptt_h[(t + 1)*rnn->hidden_size + h];
                    }
                } else {
                    for (h = 0; h < rnn->hidden_size; h++) {
                        rnn->er_h[h] = rnn->er_i_h[h];
                    }
                }

                if (t < rnn->bptt + rnn->bptt_block - 3) {
                    for (h = 0; h < rnn->hidden_size; h++) {
                        rnn->ac_h[h] = rnn->ac_bptt_h[(t + 1)
                            * rnn->hidden_size + h];
                        rnn->ac_i_h[h] = rnn->ac_bptt_h[(t + 2)
                            * rnn->hidden_size + h];
                    }
                }
            }

            // Clear buffers
            a = rnn->bptt_block;
            for (h = 0; h < a * rnn->hidden_size; h++) {
                rnn->er_bptt_h[h] = 0;
            }

            for (h = 0; h < rnn->hidden_size; h++) {
                rnn->ac_h[h] = rnn->ac_bptt_h[h];
            }

            // update weight input -> hidden (hidden)
            if (rnn->counter % 10 == 0) {
                for (a = 0; a < rnn->hidden_size; a++) {
                    for (h = 0; h < rnn->hidden_size; h++) {
                        i = a + h * rnn->hidden_size;
                        rnn->wt_ih_h[i] += rnn->wt_bptt_ih_h[i]
                            - rnn->wt_ih_h[i] * beta2;

                        rnn->wt_bptt_ih_h[i] = 0;
                    }
                }
            } else {
                for (a = 0; a < rnn->hidden_size; a++) {
                    for (h = 0; h < rnn->hidden_size; h++) {
                        i = a + h * rnn->hidden_size;
                        rnn->wt_ih_h[i] += rnn->wt_bptt_ih_h[i];

                        rnn->wt_bptt_ih_h[i] = 0;
                    }
                }
            }

            // update weight input -> hidden (word)
            if (rnn->counter % 10 == 0) {
                for (t = 0; t < rnn->bptt + rnn->bptt_block - 2; t++) {
                    a = rnn->bptt_hist[t];
                    if (a != -1) {
                        for (h = 0; h < rnn->hidden_size; h++) {
                            i = a + h*rnn->vocab_size;
                            rnn->wt_ih_w[i] += rnn->wt_bptt_ih_w[i]
                                - rnn->wt_ih_w[i] * beta2;

                            rnn->wt_bptt_ih_w[i] = 0;
                        }
                    }
                }
            } else {
                for (t = 0; t < rnn->bptt + rnn->bptt_block-2; t++) {
                    a = rnn->bptt_hist[t];
                    if (a != -1) {
                        for (h = 0; h < rnn->hidden_size; h++) {
                            i = a + h*rnn->vocab_size;
                            rnn->wt_ih_w[i] += rnn->wt_bptt_ih_w[i];

                            rnn->wt_bptt_ih_w[i] = 0;
                        }
                    }
                }
            }
        }
    }

    return 0;
}

int rnn_train(rnn_t * rnn)
{
    struct timeval tts;
    struct timeval tte;

    FILE *fp = NULL;
    int word;
    int last_word;
    int word_cnt;

    int a;
    int b;

    ST_CHECK_PARAM(rnn == NULL, -1);

    if (rnn->in_rnn_file[0] != '\0') {
        if (rnn_load_net(rnn) < 0) {
            ST_WARNING("Failed to rnn_load_net.");
            goto ERR;
        }
    } else {
        if (rnn_learn_vocab(rnn) < 0) {
            ST_WARNING("Failed to rnn_learn_vocab.");
            goto ERR;
        }

        if (rnn_init(rnn) < 0) {
            ST_WARNING("Failed to rnn_init.");
            goto ERR;
        }
        rnn->iter = 0;
    }

    rnn->alpha = rnn->starting_alpha;
    rnn->counter = rnn->train_cur_pos;
    while (rnn->max_iterations <= 0
            || rnn->iter < rnn->max_iterations) {
        ST_NOTICE("Iter: %3d, Alpha: %f", rnn->iter, rnn->alpha);

        if (rnn_flush_net(rnn) < 0) {
            ST_WARNING("Failed to rnn_flush_net.");
            goto ERR;
        }

        fp = fopen(rnn->train_file, "rb");
        if (fp == NULL) {
            ST_WARNING("Failed to open train file[%s].",
                       rnn->train_file);
            goto ERR;
        }

        last_word = 0;

        if (rnn->counter > 0) {
            for (a = 0; a < rnn->counter; a++) {
                word = rnn_read_word_id(rnn->vocab, fp);
                if (word == -3) {
                    ST_WARNING("Failed to rnn_read_word_id.");
                    goto ERR;
                }
            }
        }

        gettimeofday(&tts, NULL);
        while (1) {
            rnn->counter++;

            if (rnn->report_progress > 0) {
                if ((rnn->counter % rnn->report_progress) == 0) {
                    gettimeofday(&tte, NULL);

                    ST_NOTICE("Iter: %3d  Alpha: %f  TRAIN entropy: %.4f  "
                            "Progress: %.2f%%   Words/sec: %.1f",
                            rnn->iter, rnn->alpha,
                            -rnn->logp/log10(2)/rnn->counter,
                            rnn->counter / (real_t)rnn->train_words*100,
                            rnn->counter / ((double)(TIMEDIFF(tts, tte) / 1000.0)));
                }
            }

            if (rnn->anti_k > 0 && (rnn->counter % rnn->anti_k) == 0) {
                rnn->train_cur_pos = rnn->counter;
                rnn_save_net(rnn);
            }

            word = rnn_read_word_id(rnn->vocab, fp);
            if (word == -3) {
                ST_WARNING("Failed to rnn_read_word_id.");
                goto ERR;
            }

#if 0
            if (last_word >= 0) {
                rnn->ac_i_w[last_word] = 1;
            }
#endif

            if (rnn_compute_net(rnn, last_word, word) < 0) {
                ST_WARNING("Failed to rnn_compute_net.");
                goto ERR;
            }

            if (word == -2) {
                break;
            }

            if (word >= 0) {
                rnn->logp += log10(rnn->ac_o_w[word])
                    + log10(rnn->ac_o_c[rnn->w2c[word]]);

                if(isnan(rnn->logp) || isinf(rnn->logp)) {
                    ST_WARNING("Numerical error. word: %d, "
                            "P(w|c)=%g, P(c|s)=%g",
                            word, rnn->ac_o_w[word],
                            rnn->ac_o_c[rnn->w2c[word]]);
                    goto ERR;
                }
            }

            if (rnn->bptt > 1) {
                for (a = rnn->bptt + rnn->bptt_block - 1; a > 0; a--) {
                    rnn->bptt_hist[a] = rnn->bptt_hist[a - 1];
                }
                rnn->bptt_hist[0] = last_word;

                for (a = rnn->bptt + rnn->bptt_block - 1; a > 0; a--) {
                    for (b = 0; b < rnn->hidden_size; b++) {
                        rnn->ac_bptt_h[a*rnn->hidden_size + b] =
                            rnn->ac_bptt_h[(a-1)*rnn->hidden_size + b];
                    }
                }

                for (a = rnn->bptt_block - 1; a > 0; a--) {
                    for (b = 0; b < rnn->hidden_size; b++) {
                        rnn->er_bptt_h[a*rnn->hidden_size + b] =
                            rnn->er_bptt_h[(a-1)*rnn->hidden_size + b];
                    }
                }
            }

            if (rnn_learn_net(rnn, last_word, word) < 0) {
                ST_WARNING("Failed to rnn_compute_net.");
                goto ERR;
            }

            // copy hidden layer to input
            memcpy(rnn->ac_i_h, rnn->ac_h,
                    rnn->hidden_size * sizeof(real_t));

#if 0
            if (last_word >= 0) {
                rnn->ac_i_w[last_word] = 0;
            }
#endif

            last_word = word;

            for (a = rnn->direct_order - 1; a > 0; a--) {
                rnn->d_hist[a] = rnn->d_hist[a - 1];
            }
            rnn->d_hist[0] = word;

            if (rnn->independent && word == 0) {
                if (rnn_reset_net(rnn) < 0) {
                    ST_WARNING("Failed to rnn_reset_net.");
                    goto ERR;
                }
            }
        }
        gettimeofday(&tte, NULL);

        safe_fclose(fp);

        ST_NOTICE("Iter: %3d\tAlpha: %f\t   TRAIN entropy: %.4f    "
                  "Words/sec: %.1f   ", rnn->iter, rnn->alpha,
                  -rnn->logp / log10(2) / rnn->counter,
                  rnn->counter / ((double) (TIMEDIFF(tts, tte) / 1000.0)));

#ifdef _TIME_PROF_
        fprintf(stderr, "Train: %ld, MatXVec: %lld, VecXMat: %lld, Softmax: %lld, "
                "Direct: %lld\n",
                UTIMEDIFF(tts, tte), matXvec_total, vecXmat_total, softmax_total,
                direct_total);
#endif

        //VALIDATION PHASE
        fp = fopen(rnn->valid_file, "rb");
        if (fp == NULL) {
            ST_WARNING("Failed to open valid file[%s].",
                       rnn->valid_file);
            goto ERR;
        }

        if (rnn_flush_net(rnn) < 0) {
            ST_WARNING("Failed to rnn_flush_net.");
            goto ERR;
        }

        last_word = 0;
        word_cnt = 0;
        rnn->logp = 0;

        while (1) {
            word = rnn_read_word_id(rnn->vocab, fp);
            if (word == -3) {
                ST_WARNING("Failed to rnn_read_word_id.");
                goto ERR;
            }

#if 0
            if (last_word >= 0) {
                rnn->ac_i_w[last_word] = 1;
            }
#endif

            if (rnn_compute_net(rnn, last_word, word) < 0) {
                ST_WARNING("Failed to rnn_compute_net.");
                goto ERR;
            }

            if (word == -2) {
                break;
            }

            if (word >= 0) {
                rnn->logp += log10(rnn->ac_o_w[word])
                    + log10(rnn->ac_o_c[rnn->w2c[word]]);
                word_cnt++;
            }

            // copy hidden layer to input
            memcpy(rnn->ac_i_h, rnn->ac_h,
                    rnn->hidden_size * sizeof(real_t));

#if 0
            if (last_word >= 0) {
                rnn->ac_i_w[last_word] = 0;
            }
#endif

            last_word = word;

            for (a = rnn->direct_order - 1; a > 0; a--) {
                rnn->d_hist[a] = rnn->d_hist[a - 1];
            }
            rnn->d_hist[0] = word;

            if (rnn->independent && word == 0) {
                if (rnn_reset_net(rnn) < 0) {
                    ST_WARNING("Failed to rnn_reset_net.");
                    goto ERR;
                }
            }
        }
        safe_fclose(fp);

        ST_NOTICE("Iter: %3d\t   VALID entropy: %.4f    "
                  "logP: %.4f    PPL: %.4f", rnn->iter,
                  -rnn->logp / log10(2) / word_cnt,
                  rnn->logp,
                  exp10(-rnn->logp / (real_t) word_cnt));

        if (rnn->logp < rnn->llogp) {
            rnn_restore_wts(rnn);
        } else {
            rnn_backup_wts(rnn);
        }

        if (rnn->logp * rnn->min_improvement < rnn->llogp) {
            if (rnn->alpha_divide == 0) {
                rnn->alpha_divide = 1;
            } else {
                break;
            }
        }

        if (rnn->alpha_divide) {
            rnn->alpha /= 2;
        }

        rnn->counter = 0;
        rnn->train_cur_pos = 0;
        rnn->llogp = rnn->logp;
        rnn->logp = 0;

        rnn->iter++;
        if (rnn->auto_save) {
            if (rnn_save_net(rnn) < 0) {
                ST_WARNING("Failed to rnn_save_net.");
            }
        }
    }

    if (rnn_save_net(rnn) < 0) {
        ST_WARNING("Failed to rnn_save_net.");
    }

    return 0;

  ERR:
    safe_fclose(fp);
    return -1;
}
*/

