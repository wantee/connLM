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

int rnn_load_model_opt(rnn_model_opt_t *model_opt, st_opt_t *opt,
        const char *sec_name)
{
    float f;

    ST_CHECK_PARAM(model_opt == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_FLOAT(opt, sec_name, "SCALE", f, 1.0,
            "Scale of RNN model output");
    model_opt->scale = (real_t)f;

    if (model_opt->scale <= 0) {
        return 0;
    }

    ST_OPT_SEC_GET_INT(opt, sec_name, "HIDDEN_SIZE",
            model_opt->hidden_size, 15, "Size of hidden layer of RNN");

    return 0;

ST_OPT_ERR:
    return -1;
}

int rnn_load_train_opt(rnn_train_opt_t *train_opt, st_opt_t *opt,
        const char *sec_name, param_t *param)
{
    float f;

    ST_CHECK_PARAM(train_opt == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_INT(opt, sec_name, "BPTT", train_opt->bptt, 5,
            "Time step of BPTT");
    ST_OPT_SEC_GET_INT(opt, sec_name, "BPTT_BLOCK",
            train_opt->bptt_block, 10, "Block size of BPTT");

    ST_OPT_SEC_GET_FLOAT(opt, sec_name, "ERR_CUTOFF", f,
            50, "Cutoff of error");
    train_opt->er_cutoff = (real_t)f;

    if (param_load(&train_opt->param, opt, sec_name, param) < 0) {
        ST_WARNING("Failed to nn_param_load.");
        goto ST_OPT_ERR;
    }

    return 0;

ST_OPT_ERR:
    return -1;
}

int rnn_init(rnn_t **prnn, rnn_model_opt_t *model_opt, output_t *output)
{
    rnn_t *rnn = NULL;

    size_t i;
    size_t sz;

    int class_size;
    int vocab_size;

    ST_CHECK_PARAM(prnn == NULL || model_opt == NULL || output == NULL, -1);

    *prnn = NULL;

    if (model_opt->scale <= 0) {
        return 0;
    }
    class_size = output->output_opt.class_size;
    vocab_size = output->output_size;

    rnn = (rnn_t *) malloc(sizeof(rnn_t));
    if (rnn == NULL) {
        ST_WARNING("Falied to malloc rnn_t.");
        goto ERR;
    }
    memset(rnn, 0, sizeof(rnn_t));

    rnn->model_opt = *model_opt;
    rnn->output = output;
    rnn->class_size = class_size;
    rnn->vocab_size = vocab_size;

    sz = vocab_size * model_opt->hidden_size;
    rnn->wt_ih_w = (real_t *) malloc(sizeof(real_t) * sz);
    if (rnn->wt_ih_w == NULL) {
        ST_WARNING("Failed to malloc wt_ih_w.");
        goto ERR;
    }

    sz = model_opt->hidden_size * model_opt->hidden_size;
    rnn->wt_ih_h = (real_t *) malloc(sizeof(real_t) * sz);
    if (rnn->wt_ih_h == NULL) {
        ST_WARNING("Failed to malloc wt_ih_h.");
        goto ERR;
    }

    sz = model_opt->hidden_size * vocab_size;
    rnn->wt_ho_w = (real_t *) malloc(sizeof(real_t) * sz);
    if (rnn->wt_ho_w == NULL) {
        ST_WARNING("Failed to malloc wt_ho_w.");
        goto ERR;
    }

    sz = model_opt->hidden_size * vocab_size;
    for (i = 0; i < sz; i++) {
        rnn->wt_ih_w[i] = rrandom(-0.1, 0.1) 
            + rrandom(-0.1, 0.1) + rrandom(-0.1, 0.1);
    }

    sz = model_opt->hidden_size * model_opt->hidden_size;
    for (i = 0; i < sz; i++) {
        rnn->wt_ih_h[i] = rrandom(-0.1, 0.1) 
            + rrandom(-0.1, 0.1) + rrandom(-0.1, 0.1);
    }

    sz = vocab_size * model_opt->hidden_size;
    for (i = 0; i < sz; i++) {
        rnn->wt_ho_w[i] = rrandom(-0.1, 0.1) 
            + rrandom(-0.1, 0.1) + rrandom(-0.1, 0.1);
    }

    if (class_size > 0) {
        param_arg_clear(&rnn->arg_ho_c);

        sz = model_opt->hidden_size * class_size;
        rnn->wt_ho_c = (real_t *) malloc(sizeof(real_t) * sz);
        if (rnn->wt_ho_c == NULL) {
            ST_WARNING("Failed to malloc wt_ho_c.");
            goto ERR;
        }

        sz = class_size * model_opt->hidden_size;
        for (i = 0; i < sz; i++) {
            rnn->wt_ho_c[i] = rrandom(-0.1, 0.1) 
                + rrandom(-0.1, 0.1) + rrandom(-0.1, 0.1);
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

    safe_free(rnn->ac_i_h);
    safe_free(rnn->er_i_h);

    safe_free(rnn->ac_h);
    safe_free(rnn->er_h);

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

    rnn->model_opt = r->model_opt;
    *rnn = *r;

    sz = r->vocab_size * r->model_opt.hidden_size;
    rnn->wt_ih_w = (real_t *) malloc(sizeof(real_t) * sz);
    if (rnn->wt_ih_w == NULL) {
        ST_WARNING("Failed to malloc wt_ih_w.");
        goto ERR;
    }
    memcpy(rnn->wt_ih_w, r->wt_ih_w, sizeof(real_t) * sz);

    sz = r->model_opt.hidden_size * r->model_opt.hidden_size;
    rnn->wt_ih_h = (real_t *) malloc(sizeof(real_t) * sz);
    if (rnn->wt_ih_h == NULL) {
        ST_WARNING("Failed to malloc wt_ih_h.");
        goto ERR;
    }
    memcpy(rnn->wt_ih_h, r->wt_ih_h, sizeof(real_t) * sz);

    sz = r->model_opt.hidden_size * r->vocab_size;
    rnn->wt_ho_w = (real_t *) malloc(sizeof(real_t) * sz);
    if (rnn->wt_ho_w == NULL) {
        ST_WARNING("Failed to malloc wt_ho_w.");
        goto ERR;
    }
    memcpy(rnn->wt_ho_w, r->wt_ho_w, sizeof(real_t) * sz);

    if (r->class_size > 0) {
        sz = r->model_opt.hidden_size * r->class_size;
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
    int vocab_size;
    int class_size;

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

        if (fread(&vocab_size, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read vocab_size");
            goto ERR;
        }
        if (fread(&class_size, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read class_size");
            goto ERR;
        }

        if (fread(&hidden_size, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read hidden_size");
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

        if (st_readline(fp, "Vocab Size: %d", &vocab_size) != 1) {
            ST_WARNING("Failed to read vocab_size.");
            goto ERR;
        }
        if (st_readline(fp, "Class Size: %d", &class_size) != 1) {
            ST_WARNING("Failed to read class_size.");
            goto ERR;
        }

        if (st_readline(fp, "Hidden Size: %d", &hidden_size) != 1) {
            ST_WARNING("Failed to read hidden_size.");
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

        (*rnn)->model_opt.scale = scale;
        (*rnn)->vocab_size = vocab_size;
        (*rnn)->class_size = class_size;

        (*rnn)->model_opt.hidden_size = hidden_size;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<RNN>: %g\n", scale);
        fprintf(fo_info, "Vocab Size: %d\n", vocab_size);
        fprintf(fo_info, "Class Size: %d\n", class_size);

        fprintf(fo_info, "Hidden Size: %d\n", hidden_size);
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

    sz = rnn->vocab_size * rnn->model_opt.hidden_size;
    rnn->wt_ih_w = (real_t *) malloc(sizeof(real_t) * sz);
    if (rnn->wt_ih_w == NULL) {
        ST_WARNING("Failed to malloc wt_ih_w.");
        goto ERR;
    }

    sz = rnn->model_opt.hidden_size * rnn->model_opt.hidden_size;
    rnn->wt_ih_h = (real_t *) malloc(sizeof(real_t) * sz);
    if (rnn->wt_ih_h == NULL) {
        ST_WARNING("Failed to malloc wt_ih_h.");
        goto ERR;
    }

    sz = rnn->model_opt.hidden_size * rnn->vocab_size;
    rnn->wt_ho_w = (real_t *) malloc(sizeof(real_t) * sz);
    if (rnn->wt_ho_w == NULL) {
        ST_WARNING("Failed to malloc wt_ho_w.");
        goto ERR;
    }

    if (rnn->class_size > 0) {
        sz = rnn->model_opt.hidden_size * rnn->class_size;
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

        sz = rnn->vocab_size * rnn->model_opt.hidden_size;
        if (fread(rnn->wt_ih_w, sizeof(real_t), sz, fp) != sz) {
            ST_WARNING("Failed to read wt_ih_w.");
            goto ERR;
        }

        sz = rnn->model_opt.hidden_size * rnn->model_opt.hidden_size;
        if (fread(rnn->wt_ih_h, sizeof(real_t), sz, fp) != sz) {
            ST_WARNING("Failed to read wt_ih_h.");
            goto ERR;
        }

        sz = rnn->model_opt.hidden_size * rnn->vocab_size;
        if (fread(rnn->wt_ho_w, sizeof(real_t), sz, fp) != sz) {
            ST_WARNING("Failed to read wt_ho_w.");
            goto ERR;
        }

        if (rnn->class_size > 0) {
            sz = rnn->model_opt.hidden_size * rnn->class_size;
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

        for (i = 0; i < rnn->model_opt.hidden_size*rnn->vocab_size; i++) {
            if (st_readline(fp, REAL_FMT, rnn->wt_ih_w + i) != 1) {
                ST_WARNING("Failed to read weight.");
                goto ERR;
            }
        }

        if (st_readline(fp, "Weights input -> hidden (Hidden):") != 0) {
            ST_WARNING("weight flag error.");
            goto ERR;
        }
        sz = rnn->model_opt.hidden_size * rnn->model_opt.hidden_size;
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
        sz = rnn->model_opt.hidden_size * rnn->vocab_size;
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
            sz = rnn->model_opt.hidden_size * rnn->class_size;
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

        if (fwrite(&rnn->model_opt.scale, sizeof(real_t), 1, fp) != 1) {
            ST_WARNING("Failed to write scale.");
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

        if (fwrite(&rnn->model_opt.hidden_size, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write hidden size.");
            return -1;
        }
    } else {
        fprintf(fp, "    \n<RNN>\n");

        if (rnn == NULL) {
            fprintf(fp, "Scale: 0\n");
            return 0;
        } 
            
        fprintf(fp, "Scale: %g\n", rnn->model_opt.scale);
        fprintf(fp, "Vocab Size: %d\n", rnn->vocab_size);
        fprintf(fp, "Class Size: %d\n", rnn->class_size);

        fprintf(fp, "Hidden Size: %d\n", rnn->model_opt.hidden_size);
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

        sz = rnn->model_opt.hidden_size*rnn->vocab_size;
        if (fwrite(rnn->wt_ih_w, sizeof(real_t), sz, fp) != sz) {
            ST_WARNING("Failed to write wt_ih_w.");
            return -1;
        }

        sz = rnn->model_opt.hidden_size*rnn->model_opt.hidden_size;
        if (fwrite(rnn->wt_ih_h, sizeof(real_t), sz, fp) != sz) {
            ST_WARNING("Failed to write wt_ih_h.");
            return -1;
        }

        sz = rnn->model_opt.hidden_size*rnn->vocab_size;
        if (fwrite(rnn->wt_ho_w, sizeof(real_t), sz, fp) != sz) {
            ST_WARNING("Failed to write wt_ho_w.");
            return -1;
        }

        if (rnn->class_size > 0) {
            sz = rnn->model_opt.hidden_size*rnn->class_size;
            if (fwrite(rnn->wt_ho_c, sizeof(real_t), sz, fp) != sz) {
                ST_WARNING("Failed to write wt_ho_c.");
                return -1;
            }
        }
    } else {
        fprintf(fp, "<RNN-DATA>\n");

        fprintf(fp, "Weights input -> hidden (Word):\n");
        sz = rnn->model_opt.hidden_size*rnn->vocab_size;
        for (i = 0; i < sz; i++) {
            fprintf(fp, REAL_FMT "\n", rnn->wt_ih_w[i]);
        }

        fprintf(fp, "Weights input -> hidden (Hidden):\n");
        sz = rnn->model_opt.hidden_size*rnn->model_opt.hidden_size;
        for (i = 0; i < sz; i++) {
            fprintf(fp, REAL_FMT "\n", rnn->wt_ih_h[i]);
        }

        fprintf(fp, "Weights hidden -> output:\n");
        sz = rnn->model_opt.hidden_size*rnn->vocab_size;
        for (i = 0; i < sz; i++) {
            fprintf(fp, REAL_FMT "\n", rnn->wt_ho_w[i]);
        }

        if (rnn->class_size > 0) {
            fprintf(fp, "Weights hidden -> class:\n");
            sz = rnn->model_opt.hidden_size*rnn->class_size;
            for (i = 0; i < sz; i++) {
                fprintf(fp, REAL_FMT "\n", rnn->wt_ho_c[i]);
            }
        }
    }

    return 0;
}

int rnn_forward(rnn_t *rnn, int word)
{
    int hidden_size;

    int h;
    int i;

    int c;
    int s;
    int e;

    ST_CHECK_PARAM(rnn == NULL, -1);

    hidden_size = rnn->model_opt.hidden_size;

    memset(rnn->ac_h, 0, sizeof(real_t)*hidden_size);

    // PROPAGATE input -> hidden
    matXvec(rnn->ac_h, rnn->wt_ih_h, rnn->ac_i_h,
            hidden_size, hidden_size, 1.0);

    if (rnn->last_word >= 0) {
        i = rnn->last_word;
        for (h = 0; h < hidden_size; h++) {
            rnn->ac_h[h] += rnn->wt_ih_w[i];
            i += rnn->vocab_size;
        }
    }

    // ACTIVATE hidden
    sigmoid(rnn->ac_h, hidden_size);

    // PROPAGATE hidden -> output (class)
    if (rnn->class_size > 0) {
        matXvec(rnn->output->ac_o_c, rnn->wt_ho_c, rnn->ac_h,
                rnn->class_size, hidden_size, rnn->model_opt.scale);
    }

    // PROPAGATE hidden -> output (word)
    if (word < 0) {
        return 0;
    }

    c = rnn->output->w2c[word];
    s = rnn->output->c2w_s[c];
    e = rnn->output->c2w_e[c];

    matXvec(rnn->output->ac_o_w + s, rnn->wt_ho_w + s * hidden_size,
            rnn->ac_h, e - s, hidden_size, rnn->model_opt.scale);

    return 0;
}

static void propagate_error(real_t *dst, real_t *vec, real_t *mat,
        int mat_col, int in_vec_size, real_t er_cutoff, real_t scale)
{
    int a;

    vecXmat(dst, vec, mat, mat_col, in_vec_size, scale);

    if (er_cutoff > 0) {
        for (a = 0; a < mat_col; a++) {
            if (dst[a] > er_cutoff) {
                dst[a] = er_cutoff;
            } else if (dst[a] < -er_cutoff) {
                dst[a] = -er_cutoff;
            }
        }
    }
}

int rnn_backprop(rnn_t *rnn, int word)
{
    int bptt;
    int bptt_block;
    int hidden_size;

    int c;
    int s;
    int e;

    int h;
    int t;
    int i;

    ST_CHECK_PARAM(rnn == NULL, -1);

    if (word < 0) {
        return 0;
    }

    bptt = rnn->train_opt.bptt;
    bptt_block = max(1, rnn->train_opt.bptt_block);
    hidden_size = rnn->model_opt.hidden_size;

    c = rnn->output->w2c[word];
    s = rnn->output->c2w_s[c];
    e = rnn->output->c2w_e[c];

    memset(rnn->er_h, 0, sizeof(real_t) * hidden_size);

    // BACK-PROPAGATE output -> hidden (word)
    propagate_error(rnn->er_h, rnn->output->er_o_w + s,
            rnn->wt_ho_w + s * hidden_size,
            hidden_size, e - s, rnn->train_opt.er_cutoff,
            rnn->model_opt.scale);

    // weight update (output -> hidden)
    param_update(&rnn->train_opt.param,
            &rnn->arg_ho_w,
            rnn->wt_ho_w + s * hidden_size,
            rnn->output->er_o_w + s, 
            1.0, 
            e - s, 
            rnn->ac_h,
            hidden_size,
            -1);

    if (rnn->class_size > 0) {
        // BACK-PROPAGATE output -> hidden (class)
        propagate_error(rnn->er_h, rnn->output->er_o_c, rnn->wt_ho_c,
                hidden_size, rnn->class_size, rnn->train_opt.er_cutoff,
                rnn->model_opt.scale);

        param_update(&rnn->train_opt.param,
                &rnn->arg_ho_c,
                rnn->wt_ho_c,
                rnn->output->er_o_c, 
                1.0, 
                rnn->class_size, 
                rnn->ac_h,
                hidden_size,
                -1);
    }

    // BACK-PROPAGATE hidden -> input
    if (bptt <= 1) { // normal BP
        // error derivation
        for (h = 0; h < hidden_size; h++) {
            rnn->er_h[h] *= rnn->ac_h[h] * (1 - rnn->ac_h[h]);
        }

        // weight update (word -> hidden)
        if (rnn->last_word >= 0) {
            param_update(&rnn->train_opt.param,
                    &rnn->arg_ih_w,
                    rnn->wt_ih_w + rnn->last_word,
                    rnn->er_h, 
                    1.0, 
                    hidden_size, 
                    NULL,
                    rnn->vocab_size,
                    -1);
        }

        // weight update (hidden -> hidden)
        param_update(&rnn->train_opt.param,
                &rnn->arg_ih_h,
                rnn->wt_ih_h,
                rnn->er_h, 
                1.0, 
                hidden_size, 
                rnn->ac_i_h,
                hidden_size,
                -1);
    } else { // BPTT
        memcpy(rnn->ac_bptt_h, rnn->ac_h, sizeof(real_t) * hidden_size);
        memcpy(rnn->er_bptt_h, rnn->er_h, sizeof(real_t) * hidden_size);

        rnn->block_step++;

        if (rnn->step % bptt_block == 0 || word == 0) {
            bptt_block = min(bptt_block, rnn->block_step);
            bptt = min(rnn->step - bptt_block, bptt);
            if (bptt < 1) {
                bptt = 1;
            }
            rnn->block_step = 0;
            for (t = 0; t < bptt + bptt_block - 1; t++) {
                // error derivation
                for (h = 0; h < hidden_size; h++) {
                    rnn->er_h[h] *= rnn->ac_h[h] * (1 - rnn->ac_h[h]);
                }

                // back-propagate errors hidden -> input
                // weight update (word)
                if (rnn->bptt_hist[t] != -1) {
                    i = rnn->bptt_hist[t];
                    for (h = 0; h < hidden_size; h++) {
                        rnn->wt_bptt_ih_w[i] += rnn->er_h[h];
                        i += rnn->vocab_size;
                    }
                }

                memset(rnn->er_i_h, 0, sizeof(real_t) * hidden_size);

                // error propagation (hidden)
                propagate_error(rnn->er_i_h, rnn->er_h,
                        rnn->wt_ih_h, hidden_size,
                        hidden_size, rnn->train_opt.er_cutoff, 1.0);

                // weight update (hidden)
                for (i = 0; i < hidden_size; i++) {
                    for (h = 0; h < hidden_size; h++) {
                        rnn->wt_bptt_ih_h[i + h*hidden_size] +=
                            rnn->er_h[h]*rnn->ac_i_h[i];
                    }
                }

                //propagate error from time T-n to T-n-1
                if (t < bptt_block - 1) {
                    i = (t + 1) * hidden_size;
                    for (h = 0; h < hidden_size; h++) {
                        rnn->er_h[h] = rnn->er_i_h[h]
                            + rnn->er_bptt_h[i + h];
                    }
                } else {
                    memcpy(rnn->er_h, rnn->er_i_h,
                            sizeof(real_t) * hidden_size);
                }

                if (t < bptt + bptt_block - 2) {
                    i = (t + 1) * hidden_size;
                    memcpy(rnn->ac_h, rnn->ac_bptt_h + i,
                            sizeof(real_t) * hidden_size);
                    i = (t + 2) * hidden_size;
                    memcpy(rnn->ac_i_h, rnn->ac_bptt_h + i,
                            sizeof(real_t) * hidden_size);
                }
            }

            memcpy(rnn->ac_h, rnn->ac_bptt_h,
                    sizeof(real_t) * hidden_size);

            // update weight input -> hidden (hidden)
            param_update(&rnn->train_opt.param,
                    &rnn->arg_ih_h,
                    rnn->wt_ih_h,
                    rnn->wt_bptt_ih_h,
                    1.0,
                    -hidden_size, 
                    NULL,
                    hidden_size,
                    -1);

            memset(rnn->wt_bptt_ih_h, 0,
                    sizeof(real_t) * hidden_size * hidden_size);

            // update weight input -> hidden (word)
            for (t = 0; t < bptt + bptt_block - 1; t++) {
                if (rnn->bptt_hist[t] != -1) {
                    param_update(&rnn->train_opt.param,
                            &rnn->arg_ih_w,
                            rnn->wt_ih_w + rnn->bptt_hist[t],
                            rnn->wt_bptt_ih_w + rnn->bptt_hist[t],
                            1.0,
                            -hidden_size, 
                            NULL,
                            -rnn->vocab_size,
                            -1);

                    i = rnn->bptt_hist[t];
                    for (h = 0; h < hidden_size; h++) {
                        rnn->wt_bptt_ih_w[i] = 0;
                        i += rnn->vocab_size;
                    }
                }
            }
        }
    }

    return 0;
}

static bool rnn_check_output(rnn_t *rnn, output_t *output)
{
    ST_CHECK_PARAM(rnn == NULL || output == NULL, false);

    if (rnn->class_size != output->output_opt.class_size) {
        ST_WARNING("Class size not match");
        return false;
    } else if (rnn->vocab_size != output->output_size) {
        ST_WARNING("Vocab size not match");
        return false;
    }

    return true;
}

int rnn_setup_train(rnn_t *rnn, rnn_train_opt_t *train_opt,
        output_t *output)
{
    size_t sz;

    int hidden_size;

    ST_CHECK_PARAM(rnn == NULL || train_opt == NULL
            || output == NULL, -1);

    if (!rnn_check_output(rnn, output)) {
        ST_WARNING("Output layer not match.");
        return -1;
    }

    rnn->output = output;
    rnn->train_opt = *train_opt;

    param_arg_clear(&rnn->arg_ih_w);
    param_arg_clear(&rnn->arg_ih_h);
    param_arg_clear(&rnn->arg_ho_w);

    hidden_size = rnn->model_opt.hidden_size;

    rnn->ac_i_h = (real_t *) malloc(sizeof(real_t) * hidden_size);
    if (rnn->ac_i_h == NULL) {
        ST_WARNING("Failed to malloc ac_i_h.");
        goto ERR;
    }

    rnn->ac_h = (real_t *) malloc(sizeof(real_t) * hidden_size);
    if (rnn->ac_h == NULL) {
        ST_WARNING("Failed to malloc ac_h.");
        goto ERR;
    }

    rnn->er_h = (real_t *) malloc(sizeof(real_t) * hidden_size);
    if (rnn->er_h == NULL) {
        ST_WARNING("Failed to malloc er_h.");
        goto ERR;
    }

    if (train_opt->bptt > 1) {
        rnn->er_i_h = (real_t *) malloc(sizeof(real_t) * hidden_size);
        if (rnn->er_i_h == NULL) {
            ST_WARNING("Failed to malloc er_i_h.");
            goto ERR;
        }

        sz = train_opt->bptt + train_opt->bptt_block - 1;
        rnn->bptt_hist = (int *) malloc(sizeof(int) * sz);
        if (rnn->bptt_hist == NULL) {
            ST_WARNING("Failed to malloc bptt_hist.");
            goto ERR;
        }

        sz = hidden_size * (train_opt->bptt + train_opt->bptt_block);
        rnn->ac_bptt_h = (real_t *) malloc(sizeof(real_t) * sz);
        if (rnn->ac_bptt_h == NULL) {
            ST_WARNING("Failed to malloc ac_bptt_h.");
            goto ERR;
        }
        memset(rnn->ac_bptt_h, 0, sizeof(real_t) * sz);

        sz = hidden_size * (train_opt->bptt_block);
        rnn->er_bptt_h = (real_t *) malloc(sizeof(real_t) * sz);
        if (rnn->er_bptt_h == NULL) {
            ST_WARNING("Failed to malloc er_bptt_h.");
            goto ERR;
        }
        memset(rnn->er_bptt_h, 0, sizeof(real_t) * sz);

        sz = rnn->vocab_size * hidden_size;
        rnn->wt_bptt_ih_w = (real_t *) malloc(sizeof(real_t) * sz);
        if (rnn->wt_bptt_ih_w == NULL) {
            ST_WARNING("Failed to malloc wt_bptt_ih_w.");
            goto ERR;
        }
        memset(rnn->wt_bptt_ih_w, 0, sizeof(real_t) * sz);

        sz = hidden_size * hidden_size;
        rnn->wt_bptt_ih_h = (real_t *) malloc(sizeof(real_t) * sz);
        if (rnn->wt_bptt_ih_h == NULL) {
            ST_WARNING("Failed to malloc wt_bptt_ih_h.");
            goto ERR;
        }
        memset(rnn->wt_bptt_ih_h, 0, sizeof(real_t) * sz);
    }

    return 0;

ERR:
    safe_free(rnn->ac_i_h);
    safe_free(rnn->er_i_h);
    safe_free(rnn->ac_h);
    safe_free(rnn->er_h);

    safe_free(rnn->bptt_hist);
    safe_free(rnn->ac_bptt_h);
    safe_free(rnn->er_bptt_h);
    safe_free(rnn->wt_bptt_ih_w);
    safe_free(rnn->wt_bptt_ih_h);

    return -1;
}

int rnn_reset_train(rnn_t *rnn)
{
    size_t i;

    rnn_train_opt_t *train_opt;
    int hidden_size;

    ST_CHECK_PARAM(rnn == NULL, -1);

    train_opt = &rnn->train_opt;

    hidden_size = rnn->model_opt.hidden_size;

    if (train_opt->bptt > 1) {
        rnn->step = 0;
        rnn->block_step = 0;
        for (i = 0; i < train_opt->bptt + train_opt->bptt_block - 1; i++) {
            rnn->bptt_hist[i] = -1;
        }
        for (i = 0; i < hidden_size; i++) {
            rnn->ac_bptt_h[i] = 0.1;
        }
    }

    for (i = 0; i < hidden_size; i++) {
        rnn->ac_i_h[i] = 0.1;
    }

    rnn->last_word = -1;

    return 0;
}

int rnn_start_train(rnn_t *rnn, int word)
{
    ST_CHECK_PARAM(rnn == NULL, -1);

    return 0;
}

int rnn_fwd_bp(rnn_t *rnn, int word) 
{
    int bptt;
    int bptt_block;
    int hidden_size;

    int a;
    int b;

    ST_CHECK_PARAM(rnn == NULL, -1);

    bptt = rnn->train_opt.bptt;
    bptt_block = rnn->train_opt.bptt_block;
    hidden_size = rnn->model_opt.hidden_size;

    if (bptt > 1) {
        a = min(rnn->step, bptt + bptt_block - 2);
        for (; a > 0; a--) {
            rnn->bptt_hist[a] = rnn->bptt_hist[a - 1];
        }
        rnn->bptt_hist[0] = rnn->last_word;

        a = min(rnn->step + 1, bptt + bptt_block - 1);
        for (; a > 0; a--) {
            for (b = 0; b < hidden_size; b++) {
                rnn->ac_bptt_h[a*hidden_size + b] =
                    rnn->ac_bptt_h[(a-1)*hidden_size + b];
            }
        }

        a = min(rnn->step, bptt_block - 1);
        for (; a > 0; a--) {
            for (b = 0; b < hidden_size; b++) {
                rnn->er_bptt_h[a*hidden_size + b] =
                    rnn->er_bptt_h[(a-1)*hidden_size + b];
            }
        }

        rnn->step++;
    }

    return 0;
}

int rnn_end_train(rnn_t *rnn, int word)
{
    ST_CHECK_PARAM(rnn == NULL, -1);

    rnn->last_word = word;

    // copy hidden layer to input
    memcpy(rnn->ac_i_h, rnn->ac_h,
            rnn->model_opt.hidden_size * sizeof(real_t));

    return 0;
}

int rnn_setup_test(rnn_t *rnn, output_t *output)
{
    int hidden_size;

    ST_CHECK_PARAM(rnn == NULL || output == NULL, -1);

    if (!rnn_check_output(rnn, output)) {
        ST_WARNING("Output layer not match.");
        return -1;
    }

    rnn->output = output;

    hidden_size = rnn->model_opt.hidden_size;

    rnn->ac_i_h = (real_t *) malloc(sizeof(real_t) * hidden_size);
    if (rnn->ac_i_h == NULL) {
        ST_WARNING("Failed to malloc ac_i_h.");
        goto ERR;
    }

    rnn->ac_h = (real_t *) malloc(sizeof(real_t) * hidden_size);
    if (rnn->ac_h == NULL) {
        ST_WARNING("Failed to malloc ac_h.");
        goto ERR;
    }

    return 0;

ERR:
    safe_free(rnn->ac_i_h);
    safe_free(rnn->ac_h);

    return -1;
}

int rnn_reset_test(rnn_t *rnn)
{
    size_t i;

    int hidden_size;

    ST_CHECK_PARAM(rnn == NULL, -1);

    hidden_size = rnn->model_opt.hidden_size;

    for (i = 0; i < hidden_size; i++) {
        rnn->ac_i_h[i] = 0.1;
    }

    rnn->last_word = -1;

    return 0;
}

int rnn_start_test(rnn_t *rnn, int word)
{
    ST_CHECK_PARAM(rnn == NULL, -1);

    return 0;
}

int rnn_end_test(rnn_t *rnn, int word)
{
    ST_CHECK_PARAM(rnn == NULL, -1);

    rnn->last_word = word;

    // copy hidden layer to input
    memcpy(rnn->ac_i_h, rnn->ac_h,
            rnn->model_opt.hidden_size * sizeof(real_t));

    return 0;
}

