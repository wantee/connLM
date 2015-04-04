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
#include <st_utils.h>
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

int rnn_load_model_opt(rnn_opt_t *rnn_opt, st_opt_t *opt,
        const char *sec_name)
{
    float f;

    ST_CHECK_PARAM(rnn_opt == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_FLOAT(opt, sec_name, "SCALE", f, 1.0,
            "Scale of RNN model output");
    rnn_opt->scale = (real_t)f;

    if (rnn_opt->scale <= 0) {
        return 0;
    }

    ST_OPT_SEC_GET_INT(opt, sec_name, "HIDDEN_SIZE",
            rnn_opt->hidden_size, 15, "Size of hidden layer of RNN");

    return 0;

ST_OPT_ERR:
    return -1;
}

int rnn_load_train_opt(rnn_opt_t *rnn_opt, st_opt_t *opt,
        const char *sec_name, nn_param_t *param)
{
    ST_CHECK_PARAM(rnn_opt == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_INT(opt, sec_name, "BPTT", rnn_opt->bptt, 5,
            "Time step of BPTT");
    ST_OPT_SEC_GET_INT(opt, sec_name, "BPTT_BLOCK",
            rnn_opt->bptt_block, 10, "Block size of BPTT");

    if (nn_param_load(&rnn_opt->param, opt, sec_name, param) < 0) {
        ST_WARNING("Failed to nn_param_load.");
        goto ST_OPT_ERR;
    }

    return 0;

ST_OPT_ERR:
    return -1;
}

int rnn_init(rnn_t **prnn, rnn_opt_t *rnn_opt,
        int class_size, int vocab_size)
{
    rnn_t *rnn = NULL;

    size_t i;
    size_t j;
    size_t sz;

    ST_CHECK_PARAM(prnn == NULL || rnn_opt == NULL, -1);

    *prnn = NULL;

    if (rnn_opt->scale <= 0) {
        return 0;
    }

    rnn = (rnn_t *) malloc(sizeof(rnn_t));
    if (rnn == NULL) {
        ST_WARNING("Falied to malloc rnn_t.");
        goto ERR;
    }
    memset(rnn, 0, sizeof(rnn_t));

    rnn->rnn_opt = *rnn_opt;
    rnn->class_size = class_size;
    rnn->vocab_size = vocab_size;

    rnn->ac_i_h = (real_t *) malloc(sizeof(real_t)*rnn_opt->hidden_size);
    if (rnn->ac_i_h == NULL) {
        ST_WARNING("Failed to malloc ac_i_h.");
        goto ERR;
    }

    rnn->er_i_h = (real_t *) malloc(sizeof(real_t)*rnn_opt->hidden_size);
    if (rnn->er_i_h == NULL) {
        ST_WARNING("Failed to malloc er_i_h.");
        goto ERR;
    }

    rnn->ac_h = (real_t *) malloc(sizeof(real_t) * rnn_opt->hidden_size);
    if (rnn->ac_h == NULL) {
        ST_WARNING("Failed to malloc ac_h.");
        goto ERR;
    }

    rnn->er_h = (real_t *) malloc(sizeof(real_t) * rnn_opt->hidden_size);
    if (rnn->er_h == NULL) {
        ST_WARNING("Failed to malloc er_h.");
        goto ERR;
    }

    if (class_size > 0) {
        rnn->ac_o_c = (real_t *) malloc(sizeof(real_t) * class_size);
        if (rnn->ac_o_c == NULL) {
            ST_WARNING("Failed to malloc ac_o_c.");
            goto ERR;
        }

        rnn->er_o_c = (real_t *) malloc(sizeof(real_t) * class_size);
        if (rnn->er_o_c == NULL) {
            ST_WARNING("Failed to malloc er_o_c.");
            goto ERR;
        }

        sz = rnn_opt->hidden_size * class_size;
        rnn->wt_ho_c = (real_t *) malloc(sizeof(real_t) * sz);
        if (rnn->wt_ho_c == NULL) {
            ST_WARNING("Failed to malloc wt_ho_c.");
            goto ERR;
        }

        for (i = 0; i < class_size; i++) {
            rnn->ac_o_c[i] = 0;
            rnn->er_o_c[i] = 0;
        }

        for (i = 0; i < class_size; i++) {
            for (j = 0; j < rnn_opt->hidden_size; j++) {
                rnn->wt_ho_c[j + i*rnn_opt->hidden_size] =
                    rrandom(-0.1, 0.1) + rrandom(-0.1, 0.1) + rrandom(-0.1, 0.1);
            }
        }

    }

    rnn->ac_o_w = (real_t *) malloc(sizeof(real_t) * vocab_size);
    if (rnn->ac_o_w == NULL) {
        ST_WARNING("Failed to malloc ac_o_w.");
        goto ERR;
    }

    rnn->er_o_w = (real_t *) malloc(sizeof(real_t) * vocab_size);
    if (rnn->er_o_w == NULL) {
        ST_WARNING("Failed to malloc er_o_w.");
        goto ERR;
    }

    sz = vocab_size * rnn_opt->hidden_size;
    rnn->wt_ih_w = (real_t *) malloc(sizeof(real_t) * sz);
    if (rnn->wt_ih_w == NULL) {
        ST_WARNING("Failed to malloc wt_ih_w.");
        goto ERR;
    }

    sz = rnn_opt->hidden_size * rnn_opt->hidden_size;
    rnn->wt_ih_h = (real_t *) malloc(sizeof(real_t) * sz);
    if (rnn->wt_ih_h == NULL) {
        ST_WARNING("Failed to malloc wt_ih_h.");
        goto ERR;
    }

    sz = rnn_opt->hidden_size * vocab_size;
    rnn->wt_ho_w = (real_t *) malloc(sizeof(real_t) * sz);
    if (rnn->wt_ho_w == NULL) {
        ST_WARNING("Failed to malloc wt_ho_w.");
        goto ERR;
    }

    if (rnn->bptt > 1) {
        rnn->bptt_hist = (int *) malloc(sizeof(int)
                * (rnn_opt->bptt + rnn_opt->bptt_block));
        if (rnn->bptt_hist == NULL) {
            ST_WARNING("Failed to malloc bptt_hist.");
            goto ERR;
        }

        sz = rnn_opt->hidden_size * (rnn_opt->bptt + rnn_opt->bptt_block);
        rnn->ac_bptt_h = (real_t *) malloc(sizeof(real_t) * sz);
        if (rnn->ac_bptt_h == NULL) {
            ST_WARNING("Failed to malloc ac_bptt_h.");
            goto ERR;
        }

        sz = rnn_opt->hidden_size * (rnn_opt->bptt_block);
        rnn->er_bptt_h = (real_t *) malloc(sizeof(real_t) * sz);
        if (rnn->er_bptt_h == NULL) {
            ST_WARNING("Failed to malloc er_bptt_h.");
            goto ERR;
        }

        sz = vocab_size * rnn_opt->hidden_size;
        rnn->wt_bptt_ih_w = (real_t *) malloc(sizeof(real_t) * sz);
        if (rnn->wt_bptt_ih_w == NULL) {
            ST_WARNING("Failed to malloc wt_bptt_ih_w.");
            goto ERR;
        }

        sz = rnn_opt->hidden_size * rnn_opt->hidden_size;
        rnn->wt_bptt_ih_h = (real_t *) malloc(sizeof(real_t) * sz);
        if (rnn->wt_bptt_ih_h == NULL) {
            ST_WARNING("Failed to malloc wt_bptt_ih_h.");
            goto ERR;
        }

        for (i = 0; i < (rnn_opt->bptt + rnn_opt->bptt_block); i++) {
            rnn->bptt_hist[i] = -1;
        }

        sz = rnn_opt->hidden_size * (rnn_opt->bptt + rnn_opt->bptt_block);
        for (i = 0; i < sz; i++) {
            rnn->ac_bptt_h[i] = 0;
        }

        sz = rnn_opt->hidden_size * rnn_opt->bptt_block;
        for (i = 0; i < sz; i++) {
            rnn->er_bptt_h[i] = 0;
        }

        sz = vocab_size*rnn_opt->hidden_size;
        for (i = 0; i < sz; i++) {
            rnn->wt_bptt_ih_w[i] = 0;
        }

        sz = rnn_opt->hidden_size*rnn_opt->hidden_size;
        for (i = 0; i < sz; i++) {
            rnn->wt_bptt_ih_h[i] = 0;
        }
    }

    for (i = 0; i < rnn_opt->hidden_size; i++) {
        rnn->ac_i_h[i] = 0;
        rnn->er_i_h[i] = 0;
    }

    for (i = 0; i < rnn_opt->hidden_size; i++) {
        rnn->ac_h[i] = 0;
        rnn->er_h[i] = 0;
    }

    for (i = 0; i < vocab_size; i++) {
        rnn->ac_o_w[i] = 0;
        rnn->er_o_w[i] = 0;
    }

    for (i = 0; i < rnn_opt->hidden_size; i++) {
        for (j = 0; j < vocab_size; j++) {
            rnn->wt_ih_w[j + i*vocab_size] =
                rrandom(-0.1, 0.1) + rrandom(-0.1, 0.1) + rrandom(-0.1, 0.1);
        }

        for (j = 0; j < rnn_opt->hidden_size; j++) {
            rnn->wt_ih_h[j + i*rnn_opt->hidden_size] =
                rrandom(-0.1, 0.1) + rrandom(-0.1, 0.1) + rrandom(-0.1, 0.1);
        }
    }

    for (i = 0; i < vocab_size; i++) {
        for (j = 0; j < rnn_opt->hidden_size; j++) {
            rnn->wt_ho_w[j + i*rnn_opt->hidden_size] =
                rrandom(-0.1, 0.1) + rrandom(-0.1, 0.1) + rrandom(-0.1, 0.1);
        }
    }

    *prnn = rnn;

    return 0;

ERR:
    safe_rnn_destroy(rnn);
    return -1;
}

void rnn_destroy(rnn_t *rnn)
{
    if (rnn == NULL) {
        return;
    }

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

    safe_free(rnn->bptt_hist);
    safe_free(rnn->ac_bptt_h);
    safe_free(rnn->er_bptt_h);
    safe_free(rnn->wt_bptt_ih_w);
    safe_free(rnn->wt_bptt_ih_h);
}

rnn_t* rnn_dup(rnn_t *r)
{
    rnn_t *rnn = NULL;

    size_t sz;

    ST_CHECK_PARAM(r == NULL, NULL);

    rnn = (rnn_t *) malloc(sizeof(rnn_t));
    if (rnn == NULL) {
        ST_WARNING("Falied to malloc rnn_t.");
        goto ERR;
    }
    memset(rnn, 0, sizeof(rnn_t));

    rnn->rnn_opt = r->rnn_opt;
    *rnn = *r;

    sz = r->vocab_size * r->rnn_opt.hidden_size;
    rnn->wt_ih_w = (real_t *) malloc(sizeof(real_t) * sz);
    if (rnn->wt_ih_w == NULL) {
        ST_WARNING("Failed to malloc wt_ih_w.");
        goto ERR;
    }
    memcpy(rnn->wt_ih_w, r->wt_ih_w, sizeof(real_t) * sz);

    sz = r->rnn_opt.hidden_size * r->rnn_opt.hidden_size;
    rnn->wt_ih_h = (real_t *) malloc(sizeof(real_t) * sz);
    if (rnn->wt_ih_h == NULL) {
        ST_WARNING("Failed to malloc wt_ih_h.");
        goto ERR;
    }
    memcpy(rnn->wt_ih_h, r->wt_ih_h, sizeof(real_t) * sz);

    sz = r->rnn_opt.hidden_size * r->vocab_size;
    rnn->wt_ho_w = (real_t *) malloc(sizeof(real_t) * sz);
    if (rnn->wt_ho_w == NULL) {
        ST_WARNING("Failed to malloc wt_ho_w.");
        goto ERR;
    }
    memcpy(rnn->wt_ho_w, r->wt_ho_w, sizeof(real_t) * sz);

    if (r->class_size > 0) {
        sz = r->rnn_opt.hidden_size * r->class_size;
        rnn->wt_ho_c = (real_t *) malloc(sizeof(real_t) * sz);
        if (rnn->wt_ho_c == NULL) {
            ST_WARNING("Failed to malloc wt_ho_c.");
            goto ERR;
        }
        memcpy(rnn->wt_ho_c, r->wt_ho_c, sizeof(real_t) * sz);
    }

    return rnn;

ERR:
    safe_rnn_destroy(rnn);
    return NULL;
}

int rnn_load_header(rnn_t **rnn, FILE *fp, bool *binary, FILE *fo_info)
{
    union {
        char str[4];
        int magic_num;
    } flag;

    real_t scale;
    int hidden_size;
    int vocab_size;
    int class_size;

    ST_CHECK_PARAM((rnn == NULL && fo_info == NULL) || fp == NULL
            || binary == NULL, -1);

    if (fread(&flag.magic_num, sizeof(int), 1, fp) != 1) {
        ST_WARNING("Failed to load magic num.");
        return -1;
    }

    if (strncmp(flag.str, "    ", 4) == 0) {
        *binary = false;
    } else if (RNN_MAGIC_NUM != flag.magic_num) {
        ST_WARNING("magic num wrong.");
        return -2;
    } else {
        *binary = true;
    }

    if (rnn != NULL) {
        *rnn = NULL;
    }

    if (*binary) {
        if (fread(&scale, sizeof(real_t), 1, fp) != 1) {
            ST_WARNING("Failed to read scale.");
            goto ERR;
        }

        if (scale <= 0) {
            if (fo_info != NULL) {
                fprintf(fo_info, "\n<RNN>: None\n");
            }
            return 0;
        }

        if (fread(&hidden_size, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read hidden_size");
            goto ERR;
        }
        if (fread(&vocab_size, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read vocab_size");
            goto ERR;
        }
        if (fread(&class_size, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read class_size");
            goto ERR;
        }
    } else {
        if (st_readline(fp, "    ") != 0) {
            ST_WARNING("Failed to read tag.");
            goto ERR;
        }

        if (st_readline(fp, "<RNN>") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }

        if (st_readline(fp, "Scale: " REAL_FMT, &scale) != 1) {
            ST_WARNING("Failed to parse scale.");
            goto ERR;
        }

        if (scale <= 0) {
            if (fo_info != NULL) {
                fprintf(fo_info, "\n<RNN>: None\n");
            }
            return 0;
        }

        if (st_readline(fp, "Hidden Size: %d", &hidden_size) != 1) {
            ST_WARNING("Failed to read hidden_size.");
            goto ERR;
        }
        if (st_readline(fp, "Vocab Size: %d", &vocab_size) != 1) {
            ST_WARNING("Failed to read vocab_size.");
            goto ERR;
        }
        if (st_readline(fp, "Class Size: %d", &class_size) != 1) {
            ST_WARNING("Failed to read class_size.");
            goto ERR;
        }
    }

    if (rnn != NULL) {
        *rnn = (rnn_t *)malloc(sizeof(rnn_t));
        if (*rnn == NULL) {
            ST_WARNING("Failed to malloc rnn_t");
            goto ERR;
        }
        memset(*rnn, 0, sizeof(rnn_t));

        (*rnn)->rnn_opt.scale = scale;
        (*rnn)->rnn_opt.hidden_size = hidden_size;
        (*rnn)->vocab_size = vocab_size;
        (*rnn)->class_size = class_size;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<RNN>: %g\n", scale);
        fprintf(fo_info, "Hidden Size: %d\n", hidden_size);
        fprintf(fo_info, "Vocab Size: %d\n", vocab_size);
        fprintf(fo_info, "Class Size: %d\n", class_size);
    }


    return 0;

ERR:
    safe_rnn_destroy(*rnn);
    return -1;
}

int rnn_load_body(rnn_t *rnn, FILE *fp, bool binary)
{
    int n;

    size_t i;
    size_t sz;

    ST_CHECK_PARAM(rnn == NULL || fp == NULL, -1);

    sz = rnn->vocab_size * rnn->rnn_opt.hidden_size;
    rnn->wt_ih_w = (real_t *) malloc(sizeof(real_t) * sz);
    if (rnn->wt_ih_w == NULL) {
        ST_WARNING("Failed to malloc wt_ih_w.");
        goto ERR;
    }

    sz = rnn->rnn_opt.hidden_size * rnn->rnn_opt.hidden_size;
    rnn->wt_ih_h = (real_t *) malloc(sizeof(real_t) * sz);
    if (rnn->wt_ih_h == NULL) {
        ST_WARNING("Failed to malloc wt_ih_h.");
        goto ERR;
    }

    sz = rnn->rnn_opt.hidden_size * rnn->vocab_size;
    rnn->wt_ho_w = (real_t *) malloc(sizeof(real_t) * sz);
    if (rnn->wt_ho_w == NULL) {
        ST_WARNING("Failed to malloc wt_ho_w.");
        goto ERR;
    }

    if (rnn->class_size > 0) {
        sz = rnn->rnn_opt.hidden_size * rnn->class_size;
        rnn->wt_ho_c = (real_t *) malloc(sizeof(real_t) * sz);
        if (rnn->wt_ho_c == NULL) {
            ST_WARNING("Failed to malloc wt_ho_c.");
            goto ERR;
        }
    }

    if (binary) {
        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read magic num.");
            goto ERR;
        }

        if (n != -RNN_MAGIC_NUM) {
            ST_WARNING("Magic num error.");
            goto ERR;
        }

        sz = rnn->vocab_size * rnn->rnn_opt.hidden_size;
        if (fread(rnn->wt_ih_w, sizeof(real_t), sz, fp) != sz) {
            ST_WARNING("Failed to read wt_ih_w.");
            goto ERR;
        }

        sz = rnn->rnn_opt.hidden_size * rnn->rnn_opt.hidden_size;
        if (fread(rnn->wt_ih_h, sizeof(real_t), sz, fp) != sz) {
            ST_WARNING("Failed to read wt_ih_h.");
            goto ERR;
        }

        sz = rnn->rnn_opt.hidden_size * rnn->vocab_size;
        if (fread(rnn->wt_ho_w, sizeof(real_t), sz, fp) != sz) {
            ST_WARNING("Failed to read wt_ho_w.");
            goto ERR;
        }

        if (rnn->class_size > 0) {
            sz = rnn->rnn_opt.hidden_size * rnn->class_size;
            if (fread(rnn->wt_ho_c, sizeof(real_t), sz, fp) != sz) {
                ST_WARNING("Failed to read wt_ho_c.");
                goto ERR;
            }
        }
    } else {
        if (st_readline(fp, "<RNN-DATA>") != 0) {
            ST_WARNING("body tag error." );
            goto ERR;
        }

        if (st_readline(fp, "Weights input -> hidden (Word):") != 0) {
            ST_WARNING("weight flag error.");
            goto ERR;
        }

        for (i = 0; i < rnn->rnn_opt.hidden_size*rnn->vocab_size; i++) {
            if (st_readline(fp, REAL_FMT, rnn->wt_ih_w + i) != 1) {
                ST_WARNING("Failed to read weight.");
                goto ERR;
            }
        }

        if (st_readline(fp, "Weights input -> hidden (Hidden):") != 0) {
            ST_WARNING("weight flag error.");
            goto ERR;
        }
        sz = rnn->rnn_opt.hidden_size * rnn->rnn_opt.hidden_size;
        for (i = 0; i < sz; i++) {
            if (st_readline(fp, REAL_FMT, rnn->wt_ih_h + i) != 1) {
                ST_WARNING("Failed to parse weight.");
                goto ERR;
            }
        }

        if (st_readline(fp, "Weights hidden -> output:") != 0) {
            ST_WARNING("weight flag error.");
            goto ERR;
        }
        sz = rnn->rnn_opt.hidden_size * rnn->vocab_size;
        for (i = 0; i < sz; i++) {
            if (st_readline(fp, REAL_FMT, rnn->wt_ho_w + i) != 1) {
                ST_WARNING("Failed to parse weight.");
                goto ERR;
            }
        }

        if (rnn->class_size > 0) {
            if (st_readline(fp, "Weights hidden -> class:") != 0) {
                ST_WARNING("weight flag error.");
                goto ERR;
            }
            sz = rnn->rnn_opt.hidden_size * rnn->class_size;
            for (i = 0; i < sz; i++) {
                if (st_readline(fp, REAL_FMT, rnn->wt_ho_c + i) != 1) {
                    ST_WARNING("Failed to parse weight.");
                    goto ERR;
                }
            }
        }
    }

    return 0;

ERR:
    safe_free(rnn->wt_ih_w);
    safe_free(rnn->wt_ih_h);
    safe_free(rnn->wt_ho_w);
    safe_free(rnn->wt_ho_c);

    return -1;
}

int rnn_save_header(rnn_t *rnn, FILE *fp, bool binary)
{
    real_t scale;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (binary) { 
        if (fwrite(&RNN_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        if (rnn == NULL) {
            scale = 0;
            if (fwrite(&scale, sizeof(real_t), 1, fp) != 1) {
                ST_WARNING("Failed to write scale.");
                return -1;
            }
            return 0;
        }

        if (fwrite(&rnn->rnn_opt.scale, sizeof(real_t), 1, fp) != 1) {
            ST_WARNING("Failed to write scale.");
            return -1;
        }
        if (fwrite(&rnn->rnn_opt.hidden_size, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write hidden size.");
            return -1;
        }
        if (fwrite(&rnn->vocab_size, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write vocab size.");
            return -1;
        }
        if (fwrite(&rnn->class_size, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write class size.");
            return -1;
        }
    } else {
        fprintf(fp, "    \n<RNN>\n");

        if (rnn == NULL) {
            fprintf(fp, "Scale: 0\n");
            return 0;
        } 
            
        fprintf(fp, "Scale: %g\n", rnn->rnn_opt.scale);
        fprintf(fp, "Hidden Size: %d\n", rnn->rnn_opt.hidden_size);
        fprintf(fp, "Vocab Size: %d\n", rnn->vocab_size);
        fprintf(fp, "Class Size: %d\n", rnn->class_size);
    }

    return 0;
}

int rnn_save_body(rnn_t *rnn, FILE *fp, bool binary)
{
    int n;

    size_t i;
    size_t sz;

    ST_CHECK_PARAM(rnn == NULL || fp == NULL, -1);

    if (binary) {
        n = -RNN_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        sz = rnn->rnn_opt.hidden_size*rnn->vocab_size;
        if (fwrite(rnn->wt_ih_w, sizeof(real_t), sz, fp) != sz) {
            ST_WARNING("Failed to write wt_ih_w.");
            return -1;
        }

        sz = rnn->rnn_opt.hidden_size*rnn->rnn_opt.hidden_size;
        if (fwrite(rnn->wt_ih_h, sizeof(real_t), sz, fp) != sz) {
            ST_WARNING("Failed to write wt_ih_h.");
            return -1;
        }

        sz = rnn->rnn_opt.hidden_size*rnn->vocab_size;
        if (fwrite(rnn->wt_ho_w, sizeof(real_t), sz, fp) != sz) {
            ST_WARNING("Failed to write wt_ho_w.");
            return -1;
        }

        if (rnn->class_size > 0) {
            sz = rnn->rnn_opt.hidden_size*rnn->class_size;
            if (fwrite(rnn->wt_ho_c, sizeof(real_t), sz, fp) != sz) {
                ST_WARNING("Failed to write wt_ho_c.");
                return -1;
            }
        }
    } else {
        fprintf(fp, "<RNN-DATA>\n");

        fprintf(fp, "Weights input -> hidden (Word):\n");
        sz = rnn->rnn_opt.hidden_size*rnn->vocab_size;
        for (i = 0; i < sz; i++) {
            fprintf(fp, REAL_FMT "\n", rnn->wt_ih_w[i]);
        }

        fprintf(fp, "Weights input -> hidden (Hidden):\n");
        sz = rnn->rnn_opt.hidden_size*rnn->rnn_opt.hidden_size;
        for (i = 0; i < sz; i++) {
            fprintf(fp, REAL_FMT "\n", rnn->wt_ih_h[i]);
        }

        fprintf(fp, "Weights hidden -> output:\n");
        sz = rnn->rnn_opt.hidden_size*rnn->vocab_size;
        for (i = 0; i < sz; i++) {
            fprintf(fp, REAL_FMT "\n", rnn->wt_ho_w[i]);
        }

        if (rnn->class_size > 0) {
            fprintf(fp, "Weights hidden -> class:\n");
            sz = rnn->rnn_opt.hidden_size*rnn->class_size;
            for (i = 0; i < sz; i++) {
                fprintf(fp, REAL_FMT "\n", rnn->wt_ho_c[i]);
            }
        }
    }

    return 0;
}

int rnn_setup_train(rnn_t **rnn, rnn_opt_t *rnn_opt)
{
    ST_CHECK_PARAM(rnn == NULL || rnn_opt == NULL, -1);

    if (rnn_opt->scale <= 0) {
        return 0;
    }

    return 0;
}

int rnn_forward(rnn_t *rnn, int word)
{
    ST_CHECK_PARAM(rnn == NULL || word < 0, -1);

    return 0;
}

