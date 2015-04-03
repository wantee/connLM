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

int rnn_load_opt(rnn_opt_t *rnn_opt, st_opt_t *opt, const char *sec_name,
        nn_param_t *param)
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

    if (nn_param_load(&rnn_opt->param, opt, sec_name, param) < 0) {
        ST_WARNING("Failed to nn_param_load.");
        goto ST_OPT_ERR;
    }

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

int rnn_load_header(rnn_t **rnn, FILE *fp, bool *binary, FILE *fo_info)
{
    char line[MAX_LINE_LEN];
    union {
        char str[4];
        int magic_num;
    } flag;

    real_t scale;
    int hidden_size;

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
    } else {
        if (fgets(line, MAX_LINE_LEN, fp) == NULL) {
            ST_WARNING("Failed to read flag.");
            goto ERR;
        }

        if (fgets(line, MAX_LINE_LEN, fp) == NULL) {
            ST_WARNING("Failed to read flag.");
            goto ERR;
        }
        if (strncmp(line, "<RNN>", 5) != 0) {
            ST_WARNING("flag error.[%s]", line);
            goto ERR;
        }

        if (fgets(line, MAX_LINE_LEN, fp) == NULL) {
            ST_WARNING("Failed to read scale.");
            goto ERR;
        }
        if (sscanf(line, "Scale: %f\n", &scale) != 1) {
            ST_WARNING("Failed to parse scale.[%s]", line);
            goto ERR;
        }

        if (scale <= 0) {
            if (fo_info != NULL) {
                fprintf(fo_info, "\n<RNN>: None\n");
            }
            return 0;
        }

        if (fgets(line, MAX_LINE_LEN, fp) == NULL) {
            ST_WARNING("Failed to read hidden_size.");
            goto ERR;
        }
        if (sscanf(line, "Hidden Size: %d", &hidden_size) != 1) {
            ST_WARNING("Failed to parse hidden_size.[%s]", line);
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
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<RNN>: %g\n", scale);
        fprintf(fo_info, "Hidden Size: %d\n", hidden_size);
    }


    return 0;

ERR:
    safe_rnn_destroy(*rnn);
    return -1;
}

int rnn_load_body(rnn_t *rnn, FILE *fp, bool binary)
{
    char line[MAX_LINE_LEN];
    int n;

    ST_CHECK_PARAM(rnn == NULL || fp == NULL, -1);

    if (binary) {
        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read magic num.");
            goto ERR;
        }

        if (n != 2 * RNN_MAGIC_NUM) {
            ST_WARNING("Magic num error.");
            goto ERR;
        }

    } else {
        if (fgets(line, MAX_LINE_LEN, fp) == NULL) {
            ST_WARNING("Failed to read body flag.");
            goto ERR;
        }
        if (strncmp(line, "<RNN-DATA>", 10) != 0) {
            ST_WARNING("body flag error.[%s]", line);
            goto ERR;
        }

    }

    return 0;

ERR:
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

        if (fwrite(&rnn->rnn_opt.hidden_size, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write hidden size.");
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
    }

    return 0;
}

int rnn_save_body(rnn_t *rnn, FILE *fp, bool binary)
{
    int n;

    ST_CHECK_PARAM(rnn == NULL || fp == NULL, -1);

    if (binary) {
        n = 2 * RNN_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }
    } else {
        fprintf(fp, "<RNN-DATA>\n");
    }

    return 0;
}

int rnn_setup_train(rnn_t **rnn, rnn_opt_t *rnn_opt)
{
    ST_CHECK_PARAM(rnn == NULL || rnn_opt == NULL, -1);

    if (rnn_opt->scale <= 0) {
        safe_rnn_destroy(*rnn);
        return 0;
    }

    return 0;
}

