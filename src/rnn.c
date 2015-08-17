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

#include "utils.h"
#include "rnn.h"

static const int RNN_MAGIC_NUM = 626140498 + 2;

int rnn_load_model_opt(rnn_model_opt_t *model_opt, st_opt_t *opt,
        const char *sec_name)
{
    double d;

    ST_CHECK_PARAM(model_opt == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "SCALE", d, 1.0,
            "Scale of RNN model output");
    model_opt->scale = (real_t)d;

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
    double d;

    ST_CHECK_PARAM(train_opt == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_INT(opt, sec_name, "BPTT", train_opt->bptt, 5,
            "Time step of BPTT");
    ST_OPT_SEC_GET_INT(opt, sec_name, "BPTT_BLOCK",
            train_opt->bptt_block, 10, "Block size of BPTT");

    ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "ERR_CUTOFF", d,
            50, "Cutoff of error");
    train_opt->er_cutoff = (real_t)d;

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
    (void)posix_memalign((void **)&rnn->wt_ih_w, ALIGN_SIZE, sizeof(real_t) * sz);
    if (rnn->wt_ih_w == NULL) {
        ST_WARNING("Failed to malloc wt_ih_w.");
        goto ERR;
    }

    sz = model_opt->hidden_size * model_opt->hidden_size;
    (void)posix_memalign((void **)&rnn->wt_ih_h, ALIGN_SIZE, sizeof(real_t) * sz);
    if (rnn->wt_ih_h == NULL) {
        ST_WARNING("Failed to malloc wt_ih_h.");
        goto ERR;
    }

    sz = model_opt->hidden_size * vocab_size;
    (void)posix_memalign((void **)&rnn->wt_ho_w, ALIGN_SIZE, sizeof(real_t) * sz);
    if (rnn->wt_ho_w == NULL) {
        ST_WARNING("Failed to malloc wt_ho_w.");
        goto ERR;
    }

    sz = model_opt->hidden_size * vocab_size;
    for (i = 0; i < sz; i++) {
        rnn->wt_ih_w[i] = st_random(-0.1, 0.1)
            + st_random(-0.1, 0.1) + st_random(-0.1, 0.1);
    }

    sz = model_opt->hidden_size * model_opt->hidden_size;
    for (i = 0; i < sz; i++) {
        rnn->wt_ih_h[i] = st_random(-0.1, 0.1)
            + st_random(-0.1, 0.1) + st_random(-0.1, 0.1);
    }

    sz = vocab_size * model_opt->hidden_size;
    for (i = 0; i < sz; i++) {
        rnn->wt_ho_w[i] = st_random(-0.1, 0.1)
            + st_random(-0.1, 0.1) + st_random(-0.1, 0.1);
    }

    if (class_size > 0) {
        sz = model_opt->hidden_size * class_size;
        (void)posix_memalign((void **)&rnn->wt_ho_c, ALIGN_SIZE, sizeof(real_t) * sz);
        if (rnn->wt_ho_c == NULL) {
            ST_WARNING("Failed to malloc wt_ho_c.");
            goto ERR;
        }

        sz = class_size * model_opt->hidden_size;
        for (i = 0; i < sz; i++) {
            rnn->wt_ho_c[i] = st_random(-0.1, 0.1)
                + st_random(-0.1, 0.1) + st_random(-0.1, 0.1);
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
    int i;

    if (rnn == NULL) {
        return;
    }

    safe_free(rnn->wt_ih_w);
    safe_free(rnn->wt_ih_h);

    safe_free(rnn->wt_ho_c);
    safe_free(rnn->wt_ho_w);

    if (rnn->neurons != NULL) {
        for (i = 0; i < rnn->num_thrs; i++) {
            safe_free(rnn->neurons[i].ac_i_h);
            safe_free(rnn->neurons[i].er_i_h);
            safe_free(rnn->neurons[i].ac_h);
            safe_free(rnn->neurons[i].er_h);

            safe_free(rnn->neurons[i].bptt_hist);
            safe_free(rnn->neurons[i].ac_bptt_h);
            safe_free(rnn->neurons[i].er_bptt_h);
            safe_free(rnn->neurons[i].wt_bptt_ih_w);
#ifdef _MINI_UPDATE_
            safe_free(rnn->neurons[i].er_h_buf);
            safe_free(rnn->neurons[i].ac_i_h_buf);
#else
            safe_free(rnn->neurons[i].wt_bptt_ih_h);
            safe_free(rnn->neurons[i].wt_ih_h);
#endif
            safe_free(rnn->neurons[i].dirty_word);

            safe_free(rnn->neurons[i].ho_w_hist);
            safe_free(rnn->neurons[i].ih_w_hist);
            safe_free(rnn->neurons[i].dirty_class);
            safe_free(rnn->neurons[i].wt_ih_w);
            safe_free(rnn->neurons[i].wt_ho_w);
#ifdef _MINI_UPDATE_
            safe_free(rnn->neurons[i].er_o_c_buf);
            safe_free(rnn->neurons[i].ac_h_buf);
#else
            safe_free(rnn->neurons[i].wt_ho_c);
#endif
        }
        safe_free(rnn->neurons);
    }
    rnn->num_thrs = 0;
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
    (void)posix_memalign((void **)&rnn->wt_ih_w, ALIGN_SIZE, sizeof(real_t) * sz);
    if (rnn->wt_ih_w == NULL) {
        ST_WARNING("Failed to malloc wt_ih_w.");
        goto ERR;
    }
    memcpy(rnn->wt_ih_w, r->wt_ih_w, sizeof(real_t) * sz);

    sz = r->model_opt.hidden_size * r->model_opt.hidden_size;
    (void)posix_memalign((void **)&rnn->wt_ih_h, ALIGN_SIZE, sizeof(real_t) * sz);
    if (rnn->wt_ih_h == NULL) {
        ST_WARNING("Failed to malloc wt_ih_h.");
        goto ERR;
    }
    memcpy(rnn->wt_ih_h, r->wt_ih_h, sizeof(real_t) * sz);

    sz = r->model_opt.hidden_size * r->vocab_size;
    (void)posix_memalign((void **)&rnn->wt_ho_w, ALIGN_SIZE, sizeof(real_t) * sz);
    if (rnn->wt_ho_w == NULL) {
        ST_WARNING("Failed to malloc wt_ho_w.");
        goto ERR;
    }
    memcpy(rnn->wt_ho_w, r->wt_ho_w, sizeof(real_t) * sz);

    if (r->class_size > 0) {
        sz = r->model_opt.hidden_size * r->class_size;
        (void)posix_memalign((void **)&rnn->wt_ho_c, ALIGN_SIZE, sizeof(real_t) * sz);
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
        if (st_readline(fp, "") != 0) {
            ST_WARNING("tag error.");
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
    if (rnn != NULL) {
        safe_rnn_destroy(*rnn);
    }
    return -1;
}

int rnn_load_body(rnn_t *rnn, FILE *fp, bool binary)
{
    int n;

    size_t i;
    size_t sz;

    ST_CHECK_PARAM(rnn == NULL || fp == NULL, -1);

    sz = rnn->vocab_size * rnn->model_opt.hidden_size;
    (void)posix_memalign((void **)&rnn->wt_ih_w, ALIGN_SIZE, sizeof(real_t) * sz);
    if (rnn->wt_ih_w == NULL) {
        ST_WARNING("Failed to malloc wt_ih_w.");
        goto ERR;
    }

    sz = rnn->model_opt.hidden_size * rnn->model_opt.hidden_size;
    (void)posix_memalign((void **)&rnn->wt_ih_h, ALIGN_SIZE, sizeof(real_t) * sz);
    if (rnn->wt_ih_h == NULL) {
        ST_WARNING("Failed to malloc wt_ih_h.");
        goto ERR;
    }

    sz = rnn->model_opt.hidden_size * rnn->vocab_size;
    (void)posix_memalign((void **)&rnn->wt_ho_w, ALIGN_SIZE, sizeof(real_t) * sz);
    if (rnn->wt_ho_w == NULL) {
        ST_WARNING("Failed to malloc wt_ho_w.");
        goto ERR;
    }

    if (rnn->class_size > 0) {
        sz = rnn->model_opt.hidden_size * rnn->class_size;
        (void)posix_memalign((void **)&rnn->wt_ho_c, ALIGN_SIZE, sizeof(real_t) * sz);
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

int rnn_forward_pre_layer(rnn_t *rnn, int tid)
{
    rnn_neuron_t *neu;
    output_neuron_t *output_neu;
    int hidden_size;

    int h;
    int i;

    ST_CHECK_PARAM(rnn == NULL || tid < 0, -1);

    neu = rnn->neurons + tid;
    output_neu = rnn->output->neurons + tid;
    hidden_size = rnn->model_opt.hidden_size;

    memset(neu->ac_h, 0, sizeof(real_t)*hidden_size);

    // PROPAGATE input -> hidden
    matXvec(neu->ac_h, rnn->wt_ih_h, neu->ac_i_h,
            hidden_size, hidden_size, 1.0);

    if (neu->last_word >= 0) {
        i = neu->last_word;
        for (h = 0; h < hidden_size; h++) {
            neu->ac_h[h] += rnn->wt_ih_w[i];
            i += rnn->vocab_size;
        }
    }

    // ACTIVATE hidden
    sigmoid(neu->ac_h, hidden_size);

    // PROPAGATE hidden -> output (class)
    if (rnn->class_size > 0) {
        matXvec(output_neu->ac_o_c, rnn->wt_ho_c, neu->ac_h,
                rnn->class_size, hidden_size, rnn->model_opt.scale);
    }

    return 0;
}

int rnn_forward_last_layer(rnn_t *rnn, int cls, int tid)
{
    rnn_neuron_t *neu;
    output_neuron_t *output_neu;
    int hidden_size;

    int s;
    int e;

    ST_CHECK_PARAM(rnn == NULL || tid < 0, -1);

    neu = rnn->neurons + tid;
    output_neu = rnn->output->neurons + tid;
    hidden_size = rnn->model_opt.hidden_size;

    if (rnn->class_size > 0 && cls >= 0) {
        s = rnn->output->c2w_s[cls];
        e = rnn->output->c2w_e[cls];
    } else {
        s = 0;
        e = rnn->vocab_size;
    }

    matXvec(output_neu->ac_o_w + s, rnn->wt_ho_w + s * hidden_size,
            neu->ac_h, e - s, hidden_size, rnn->model_opt.scale);

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

int rnn_backprop(rnn_t *rnn, int word, int tid)
{
    rnn_neuron_t *neu;
    output_neuron_t *output_neu;
    int bptt;
    int bptt_block;
    int hidden_size;

    int c;
    int s;
    int e;

    int h;
    int t;
    int i;
    int w;

    param_t param;

    ST_CHECK_PARAM(rnn == NULL || tid < 0, -1);

    if (word < 0) {
        return 0;
    }

    neu = rnn->neurons + tid;
    output_neu = rnn->output->neurons + tid;

    param = rnn->train_opt.param;
    bptt = rnn->train_opt.bptt;
    bptt_block = max(1, rnn->train_opt.bptt_block);
    hidden_size = rnn->model_opt.hidden_size;

    if (rnn->class_size > 0) {
        c = rnn->output->w2c[word];
        s = rnn->output->c2w_s[c];
        e = rnn->output->c2w_e[c];
    } else {
        s = 0;
        e = rnn->vocab_size;
    }

    memset(neu->er_h, 0, sizeof(real_t) * hidden_size);

    // BACK-PROPAGATE output -> hidden (word)
    propagate_error(neu->er_h, output_neu->er_o_w + s,
            rnn->wt_ho_w + s * hidden_size,
            hidden_size, e - s, rnn->train_opt.er_cutoff,
            rnn->model_opt.scale);

    if (param.mini_batch > 0) {
        param_acc_wt(neu->wt_ho_w + s * hidden_size,
                output_neu->er_o_w + s,
                e - s,
                neu->ac_h,
                hidden_size);
        neu->ho_w_hist[neu->ho_w_hist_num] = word;
        neu->ho_w_hist_num++;
    } else {
        // weight update (output -> hidden)
        param_update(&param,
                &neu->arg_ho_w, true,
                rnn->wt_ho_w + s * hidden_size,
                output_neu->er_o_w + s,
                rnn->model_opt.scale,
                e - s,
                neu->ac_h,
                hidden_size);
    }

    if (rnn->class_size > 0) {
        // BACK-PROPAGATE output -> hidden (class)
        propagate_error(neu->er_h, output_neu->er_o_c, rnn->wt_ho_c,
                hidden_size, rnn->class_size, rnn->train_opt.er_cutoff,
                rnn->model_opt.scale);

        if (param.mini_batch > 0) {
#ifdef _MINI_UPDATE_
            memcpy(neu->er_o_c_buf + neu->ho_c_buf_num * rnn->class_size,
                    output_neu->er_o_c, sizeof(real_t) * rnn->class_size);
            memcpy(neu->ac_h_buf + neu->ho_c_buf_num * hidden_size, neu->ac_h,
                    sizeof(real_t) * hidden_size);
            neu->ho_c_buf_num++;
#else
            param_acc_wt(neu->wt_ho_c,
                    output_neu->er_o_c,
                    rnn->class_size,
                    neu->ac_h,
                    hidden_size);
#endif
        } else {
            param_update(&param,
                    &neu->arg_ho_c, true,
                    rnn->wt_ho_c,
                    output_neu->er_o_c,
                    rnn->model_opt.scale,
                    rnn->class_size,
                    neu->ac_h,
                    hidden_size);
        }
    }

    // BACK-PROPAGATE hidden -> input
    if (bptt <= 1) { // normal BP
        // error derivation
        for (h = 0; h < hidden_size; h++) {
            neu->er_h[h] *= neu->ac_h[h] * (1 - neu->ac_h[h]);
        }

        // weight update (word -> hidden)
        if (neu->last_word >= 0) {
            if (param.mini_batch > 0) {
                param_acc_wt(neu->wt_ih_w + neu->last_word,
                        neu->er_h,
                        hidden_size,
                        NULL,
                        rnn->vocab_size);
                neu->ih_w_hist[neu->ih_w_hist_num] = neu->last_word;
                neu->ih_w_hist_num++;
            } else {
                param_update(&param,
                        &neu->arg_ih_w, true,
                        rnn->wt_ih_w + neu->last_word,
                        neu->er_h,
                        1.0,
                        hidden_size,
                        NULL,
                        rnn->vocab_size);
            }
        }

        // weight update (hidden -> hidden)
        if (param.mini_batch > 0) {
#ifdef _MINI_UPDATE_
            memcpy(neu->er_h_buf + neu->ih_buf_num * hidden_size,
                    neu->er_h, sizeof(real_t) * hidden_size);
            memcpy(neu->ac_i_h_buf + neu->ih_buf_num * hidden_size,
                    neu->ac_i_h, sizeof(real_t) * hidden_size);
#else
            param_acc_wt(neu->wt_ih_h,
                    neu->er_h,
                    hidden_size,
                    neu->ac_i_h,
                    hidden_size);
#endif
            neu->ih_buf_num++;
        } else {
            param_update(&param,
                    &neu->arg_ih_h, true,
                    rnn->wt_ih_h,
                    neu->er_h,
                    1.0,
                    hidden_size,
                    neu->ac_i_h,
                    hidden_size);
        }
    } else { // BPTT
        memcpy(neu->ac_bptt_h, neu->ac_h, sizeof(real_t) * hidden_size);
        memcpy(neu->er_bptt_h, neu->er_h, sizeof(real_t) * hidden_size);

        neu->block_step++;

        if (neu->step % bptt_block == 0 || word == 0) {
            bptt_block = min(bptt_block, neu->block_step);
            bptt = min(neu->step - bptt_block, bptt);
            if (bptt < 1) {
                bptt = 1;
            }
            neu->block_step = 0;
            for (t = 0; t < bptt + bptt_block - 1; t++) {
                // error derivation
                for (h = 0; h < hidden_size; h++) {
                    neu->er_h[h] *= neu->ac_h[h] * (1 - neu->ac_h[h]);
                }

                // back-propagate errors hidden -> input
                // weight update (word)
                if (neu->bptt_hist[t] != -1) {
                    i = neu->bptt_hist[t];
                    for (h = 0; h < hidden_size; h++) {
                        neu->wt_bptt_ih_w[i] += neu->er_h[h];
                        i += rnn->vocab_size;
                    }
                }

                memset(neu->er_i_h, 0, sizeof(real_t) * hidden_size);

                // error propagation (hidden)
                propagate_error(neu->er_i_h, neu->er_h,
                        rnn->wt_ih_h, hidden_size,
                        hidden_size, rnn->train_opt.er_cutoff, 1.0);

                // weight update (hidden)
#ifdef _MINI_UPDATE_
                memcpy(neu->er_h_buf + neu->ih_buf_num * hidden_size,
                        neu->er_h, sizeof(real_t) * hidden_size);
                memcpy(neu->ac_i_h_buf + neu->ih_buf_num * hidden_size,
                        neu->ac_i_h, sizeof(real_t) * hidden_size);
#else
                param_acc_wt(neu->wt_bptt_ih_h,
                        neu->er_h,
                        hidden_size,
                        neu->ac_i_h,
                        hidden_size);
#endif
                neu->ih_buf_num++;

                //propagate error from time T-n to T-n-1
                if (t < bptt_block - 1) {
                    i = (t + 1) * hidden_size;
                    for (h = 0; h < hidden_size; h++) {
                        neu->er_h[h] = neu->er_i_h[h]
                            + neu->er_bptt_h[i + h];
                    }
                } else {
                    memcpy(neu->er_h, neu->er_i_h,
                            sizeof(real_t) * hidden_size);
                }

                if (t < bptt + bptt_block - 2) {
                    i = (t + 1) * hidden_size;
                    memcpy(neu->ac_h, neu->ac_bptt_h + i,
                            sizeof(real_t) * hidden_size);
                    i = (t + 2) * hidden_size;
                    memcpy(neu->ac_i_h, neu->ac_bptt_h + i,
                            sizeof(real_t) * hidden_size);
                }
            }

            memcpy(neu->ac_h, neu->ac_bptt_h,
                    sizeof(real_t) * hidden_size);

            // update weight input -> hidden (hidden)
#ifdef _MINI_UPDATE_
            if (param.mini_batch > 0) {
                // update when finish mini batch
            } else {
                param_update_minibatch(&param,
                        &neu->arg_ih_h, true,
                        neu->ih_buf_num,
                        rnn->wt_ih_h,
                        neu->er_h_buf,
                        1.0,
                        hidden_size,
                        neu->ac_i_h_buf,
                        hidden_size);
                neu->ih_buf_num = 0;
            }
#else
            if (param.mini_batch > 0) {
                param_acc_wt(neu->wt_ih_h,
                        neu->wt_bptt_ih_h,
                        -hidden_size,
                        NULL,
                        hidden_size);
            } else {
                param_update(&param,
                        &neu->arg_ih_h, true,
                        rnn->wt_ih_h,
                        neu->wt_bptt_ih_h,
                        1.0,
                        -hidden_size,
                        NULL,
                        hidden_size);
            }

            memset(neu->wt_bptt_ih_h, 0,
                    sizeof(real_t) * hidden_size * hidden_size);
#endif

            // update weight input -> hidden (word)
            for (t = 0; t < bptt + bptt_block - 1; t++) {
                w = neu->bptt_hist[t];
                if (w == -1) {
                    continue;
                }

                if (neu->dirty_word[w]) {
                    continue;
                }

                neu->dirty_word[w] = true;

                if (param.mini_batch > 0) {
                    param_acc_wt(neu->wt_ih_w + w,
                            neu->wt_bptt_ih_w + w,
                            -hidden_size,
                            NULL,
                            -rnn->vocab_size);
                    neu->ih_w_hist[neu->ih_w_hist_num] = w;
                    neu->ih_w_hist_num++;
                } else {
                    param_update(&param,
                            &neu->arg_ih_w, true,
                            rnn->wt_ih_w + w,
                            neu->wt_bptt_ih_w + w,
                            1.0,
                            -hidden_size,
                            NULL,
                            -rnn->vocab_size);
                }

                i = w;
                for (h = 0; h < hidden_size; h++) {
                    neu->wt_bptt_ih_w[i] = 0;
                    i += rnn->vocab_size;
                }
            }

            for (t = 0; t < bptt + bptt_block - 1; t++) {
                w = neu->bptt_hist[t];
                if (w == -1) {
                    continue;
                }

                neu->dirty_word[w] = false;
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
        output_t *output, int num_thrs)
{
    rnn_neuron_t *neu;
    size_t sz;

    int hidden_size;
    int i;
    int t;

    ST_CHECK_PARAM(rnn == NULL || train_opt == NULL
            || output == NULL || num_thrs < 0, -1);

    if (!rnn_check_output(rnn, output)) {
        ST_WARNING("Output layer not match.");
        return -1;
    }

    rnn->output = output;
    rnn->train_opt = *train_opt;

    rnn->num_thrs = num_thrs;
    sz = sizeof(rnn_neuron_t) * num_thrs;
    rnn->neurons = (rnn_neuron_t *)malloc(sz);
    if (rnn->neurons == NULL) {
        ST_WARNING("Failed to malloc neurons.");
        goto ERR;
    }
    memset(rnn->neurons, 0, sz);

    hidden_size = rnn->model_opt.hidden_size;
    for (t = 0; t < num_thrs; t++) {
        neu = rnn->neurons + t;

        param_arg_clear(&neu->arg_ih_w);
        param_arg_clear(&neu->arg_ih_h);
        param_arg_clear(&neu->arg_ho_w);
        param_arg_clear(&neu->arg_ho_c);

        (void)posix_memalign((void **)&neu->ac_i_h, ALIGN_SIZE, sizeof(real_t) * hidden_size);
        if (neu->ac_i_h == NULL) {
            ST_WARNING("Failed to malloc ac_i_h.");
            goto ERR;
        }

        (void)posix_memalign((void **)&neu->ac_h, ALIGN_SIZE, sizeof(real_t) * hidden_size);
        if (neu->ac_h == NULL) {
            ST_WARNING("Failed to malloc ac_h.");
            goto ERR;
        }

        (void)posix_memalign((void **)&neu->er_h, ALIGN_SIZE, sizeof(real_t) * hidden_size);
        if (neu->er_h == NULL) {
            ST_WARNING("Failed to malloc er_h.");
            goto ERR;
        }

        if (train_opt->bptt > 1) {
            (void)posix_memalign((void **)&neu->er_i_h, ALIGN_SIZE, sizeof(real_t) * hidden_size);
            if (neu->er_i_h == NULL) {
                ST_WARNING("Failed to malloc er_i_h.");
                goto ERR;
            }

            sz = train_opt->bptt + train_opt->bptt_block - 1;
            neu->bptt_hist = (int *) malloc(sizeof(int) * sz);
            if (neu->bptt_hist == NULL) {
                ST_WARNING("Failed to malloc bptt_hist.");
                goto ERR;
            }

            sz = hidden_size * (train_opt->bptt + train_opt->bptt_block);
            (void)posix_memalign((void **)&neu->ac_bptt_h, ALIGN_SIZE, sizeof(real_t) * sz);
            if (neu->ac_bptt_h == NULL) {
                ST_WARNING("Failed to malloc ac_bptt_h.");
                goto ERR;
            }
            memset(neu->ac_bptt_h, 0, sizeof(real_t) * sz);

            sz = hidden_size * (train_opt->bptt_block);
            (void)posix_memalign((void **)&neu->er_bptt_h, ALIGN_SIZE, sizeof(real_t) * sz);
            if (neu->er_bptt_h == NULL) {
                ST_WARNING("Failed to malloc er_bptt_h.");
                goto ERR;
            }
            memset(neu->er_bptt_h, 0, sizeof(real_t) * sz);

            sz = rnn->vocab_size * hidden_size;
            (void)posix_memalign((void **)&neu->wt_bptt_ih_w, ALIGN_SIZE, sizeof(real_t) * sz);
            if (neu->wt_bptt_ih_w == NULL) {
                ST_WARNING("Failed to malloc wt_bptt_ih_w.");
                goto ERR;
            }
            memset(neu->wt_bptt_ih_w, 0, sizeof(real_t) * sz);

#ifdef _MINI_UPDATE_
            sz = (train_opt->bptt + train_opt->bptt_block - 1) * hidden_size;
            if (train_opt->param.mini_batch > 0) {
                sz *= train_opt->param.mini_batch;
            }
            (void)posix_memalign((void **)&neu->er_h_buf, ALIGN_SIZE, sizeof(real_t) * sz);
            if (neu->er_h_buf == NULL) {
                ST_WARNING("Failed to malloc er_h_buf.");
                goto ERR;
            }
            (void)posix_memalign((void **)&neu->ac_i_h_buf, ALIGN_SIZE, sizeof(real_t) * sz);
            if (neu->ac_i_h_buf == NULL) {
                ST_WARNING("Failed to malloc ac_i_h_buf.");
                goto ERR;
            }
            neu->ih_buf_num = 0;
#else
            sz = hidden_size * hidden_size;
            (void)posix_memalign((void **)&neu->wt_bptt_ih_h, ALIGN_SIZE, sizeof(real_t) * sz);
            if (neu->wt_bptt_ih_h == NULL) {
                ST_WARNING("Failed to malloc wt_bptt_ih_h.");
                goto ERR;
            }
            memset(neu->wt_bptt_ih_h, 0, sizeof(real_t) * sz);
#endif
        }

        if (train_opt->param.mini_batch > 0) {
            neu->mini_step = 0;

            sz = rnn->vocab_size * rnn->model_opt.hidden_size;
            (void)posix_memalign((void **)&neu->wt_ih_w, ALIGN_SIZE, sizeof(real_t) * sz);
            if (neu->wt_ih_w == NULL) {
                ST_WARNING("Failed to malloc wt_ih_w.");
                goto ERR;
            }
            memset(neu->wt_ih_w, 0, sizeof(real_t) * sz);

            sz = rnn->model_opt.hidden_size * rnn->vocab_size;
            (void)posix_memalign((void **)&neu->wt_ho_w, ALIGN_SIZE, sizeof(real_t) * sz);
            if (neu->wt_ho_w == NULL) {
                ST_WARNING("Failed to malloc wt_ho_w.");
                goto ERR;
            }
            memset(neu->wt_ho_w, 0, sizeof(real_t) * sz);

            if (rnn->class_size > 0) {
                sz = rnn->class_size;
                neu->dirty_class = (bool *) malloc(sizeof(bool) * sz);
                if (neu->dirty_class == NULL) {
                    ST_WARNING("Failed to malloc dirty_class.");
                    goto ERR;
                }
                memset(neu->dirty_class, 0, sizeof(bool)*sz);

                sz = train_opt->param.mini_batch;
                neu->ho_w_hist = (int *) malloc(sizeof(int) * sz);
                if (neu->ho_w_hist == NULL) {
                    ST_WARNING("Failed to malloc ho_w_hist.");
                    goto ERR;
                }
                neu->ho_w_hist_num = 0;

#ifdef _MINI_UPDATE_
                sz = train_opt->param.mini_batch * rnn->class_size;
                (void)posix_memalign((void **)&neu->er_o_c_buf, ALIGN_SIZE, sizeof(real_t) * sz);
                if (neu->er_o_c_buf == NULL) {
                    ST_WARNING("Failed to malloc er_o_c_buf.");
                    goto ERR;
                }
                sz = train_opt->param.mini_batch * hidden_size;
                (void)posix_memalign((void **)&neu->ac_h_buf, ALIGN_SIZE, sizeof(real_t) * sz);
                if (neu->ac_h_buf == NULL) {
                    ST_WARNING("Failed to malloc ac_h_buf.");
                    goto ERR;
                }
                neu->ho_c_buf_num = 0;
#else
                sz = rnn->model_opt.hidden_size * rnn->class_size;
                (void)posix_memalign((void **)&neu->wt_ho_c, ALIGN_SIZE, sizeof(real_t) * sz);
                if (neu->wt_ho_c == NULL) {
                    ST_WARNING("Failed to malloc wt_ho_c.");
                    goto ERR;
                }
                memset(neu->wt_ho_c, 0, sizeof(real_t) * sz);
#endif
            }

            sz = train_opt->param.mini_batch;
            if (train_opt->bptt > 1) {
                 sz *= (train_opt->bptt + train_opt->bptt_block - 1);
            }
            neu->ih_w_hist = (int *) malloc(sizeof(int) * sz);
            if (neu->ih_w_hist == NULL) {
                ST_WARNING("Failed to malloc ih_w_hist.");
                goto ERR;
            }
            neu->ih_w_hist_num = 0;

#ifdef _MINI_UPDATE_
            if (train_opt->bptt <= 1) {
                sz = train_opt->param.mini_batch * hidden_size;
                (void)posix_memalign((void **)&neu->er_h_buf, ALIGN_SIZE, sizeof(real_t) * sz);
                if (neu->er_h_buf == NULL) {
                    ST_WARNING("Failed to malloc er_h_buf.");
                    goto ERR;
                }
                sz = train_opt->param.mini_batch * hidden_size;
                (void)posix_memalign((void **)&neu->ac_i_h_buf, ALIGN_SIZE, sizeof(real_t) * sz);
                if (neu->ac_i_h_buf == NULL) {
                    ST_WARNING("Failed to malloc ac_i_h_buf.");
                    goto ERR;
                }
                neu->ih_buf_num = 0;
            }
#else
            sz = rnn->model_opt.hidden_size * rnn->model_opt.hidden_size;
            (void)posix_memalign((void **)&neu->wt_ih_h, ALIGN_SIZE, sizeof(real_t) * sz);
            if (neu->wt_ih_h == NULL) {
                ST_WARNING("Failed to malloc wt_ih_h.");
                goto ERR;
            }
            memset(neu->wt_ih_h, 0, sizeof(real_t) * sz);

#endif
        }

        if (train_opt->bptt > 1 || train_opt->param.mini_batch > 0) {
            sz = rnn->vocab_size;
            neu->dirty_word = (bool *) malloc(sizeof(bool) * sz);
            if (neu->dirty_word == NULL) {
                ST_WARNING("Failed to malloc dirty_word.");
                goto ERR;
            }
            memset(neu->dirty_word, 0, sizeof(bool)*sz);
        }
    }

    return 0;

ERR:
    if (rnn->neurons != NULL) {
        for (i = 0; i < rnn->num_thrs; i++) {
            safe_free(rnn->neurons[i].ac_i_h);
            safe_free(rnn->neurons[i].er_i_h);
            safe_free(rnn->neurons[i].ac_h);
            safe_free(rnn->neurons[i].er_h);

            safe_free(rnn->neurons[i].bptt_hist);
            safe_free(rnn->neurons[i].ac_bptt_h);
            safe_free(rnn->neurons[i].er_bptt_h);
            safe_free(rnn->neurons[i].wt_bptt_ih_w);
#ifdef _MINI_UPDATE_
            safe_free(rnn->neurons[i].er_h_buf);
            safe_free(rnn->neurons[i].ac_i_h_buf);
#else
            safe_free(rnn->neurons[i].wt_bptt_ih_h);
            safe_free(rnn->neurons[i].wt_ih_h);
#endif
            safe_free(rnn->neurons[i].dirty_word);

            safe_free(rnn->neurons[i].ho_w_hist);
            safe_free(rnn->neurons[i].ih_w_hist);
            safe_free(rnn->neurons[i].dirty_class);
            safe_free(rnn->neurons[i].wt_ih_w);
            safe_free(rnn->neurons[i].wt_ho_w);

#ifdef _MINI_UPDATE_
            safe_free(rnn->neurons[i].er_o_c_buf);
            safe_free(rnn->neurons[i].ac_h_buf);
#else
            safe_free(rnn->neurons[i].wt_ho_c);
#endif
        }
        safe_free(rnn->neurons);
    }
    rnn->num_thrs = 0;

    return -1;
}

int rnn_reset_train(rnn_t *rnn, int tid)
{
    rnn_neuron_t *neu;
    size_t i;

    rnn_train_opt_t *train_opt;
    int hidden_size;

    ST_CHECK_PARAM(rnn == NULL || tid < 0, -1);

    neu = rnn->neurons + tid;
    train_opt = &rnn->train_opt;

    hidden_size = rnn->model_opt.hidden_size;

    neu->step = 0;

    if (train_opt->bptt > 1) {
        neu->block_step = 0;
        for (i = 0; i < train_opt->bptt + train_opt->bptt_block - 1; i++) {
            neu->bptt_hist[i] = -1;
        }
        for (i = 0; i < hidden_size; i++) {
            neu->ac_bptt_h[i] = 0.1;
        }
    }

    for (i = 0; i < hidden_size; i++) {
        neu->ac_i_h[i] = 0.1;
    }

    neu->last_word = -1;

    return 0;
}

int rnn_start_train(rnn_t *rnn, int word, int tid)
{
    ST_CHECK_PARAM(rnn == NULL, -1);

    return 0;
}

int rnn_fwd_bp(rnn_t *rnn, int word, int tid)
{
    rnn_neuron_t *neu;
    int bptt;
    int bptt_block;
    int hidden_size;

    int a;
    int b;

    ST_CHECK_PARAM(rnn == NULL || tid < 0, -1);

    neu = rnn->neurons + tid;
    bptt = rnn->train_opt.bptt;
    bptt_block = rnn->train_opt.bptt_block;
    hidden_size = rnn->model_opt.hidden_size;

    if (bptt > 1) {
        a = min(neu->step, bptt + bptt_block - 2);
        for (; a > 0; a--) {
            neu->bptt_hist[a] = neu->bptt_hist[a - 1];
        }
        neu->bptt_hist[0] = neu->last_word;

        a = min(neu->step + 1, bptt + bptt_block - 1);
        for (; a > 0; a--) {
            for (b = 0; b < hidden_size; b++) {
                neu->ac_bptt_h[a*hidden_size + b] =
                    neu->ac_bptt_h[(a-1)*hidden_size + b];
            }
        }

        a = min(neu->step, bptt_block - 1);
        for (; a > 0; a--) {
            for (b = 0; b < hidden_size; b++) {
                neu->er_bptt_h[a*hidden_size + b] =
                    neu->er_bptt_h[(a-1)*hidden_size + b];
            }
        }
    }

    neu->step++;

    return 0;
}

static int rnn_update_minibatch(rnn_t *rnn, int tid)
{
    param_t param;
    rnn_neuron_t *neu;

    int hidden_size;

    int c;
    int s;
    int e;

    int w;
    int i;
    int a;
    int h;

    ST_CHECK_PARAM(rnn == NULL || tid < 0, -1);

    param = rnn->train_opt.param;
    neu = rnn->neurons + tid;
    hidden_size = rnn->model_opt.hidden_size;

    if (neu->ho_w_hist_num > 0) {
        if (rnn->class_size > 0) {
            for (i = 0; i < neu->ho_w_hist_num; i++) {
                if (neu->ho_w_hist[i] == -1) {
                    continue;
                }

                c = rnn->output->w2c[neu->ho_w_hist[i]];

                if (neu->dirty_class[c]) {
                    continue;
                }
                neu->dirty_class[c] = true;

                s = rnn->output->c2w_s[c];
                e = rnn->output->c2w_e[c];

                param_update(&param,
                        &neu->arg_ho_w, true,
                        rnn->wt_ho_w + s * hidden_size,
                        neu->wt_ho_w + s * hidden_size,
                        rnn->model_opt.scale,
                        -(e - s),
                        NULL,
                        hidden_size);

                memset(neu->wt_ho_w + s * hidden_size,
                        0,
                        sizeof(real_t) * (e-s) * hidden_size);
            }

            for (i = 0; i < neu->ho_w_hist_num; i++) {
                if (neu->ho_w_hist[i] == -1) {
                    continue;
                }

                c = rnn->output->w2c[neu->ho_w_hist[i]];
                neu->dirty_class[c] = false;
            }
            neu->ho_w_hist_num = 0;

#ifdef _MINI_UPDATE_
            param_update_minibatch(&param,
                    &neu->arg_ho_c, true,
                    neu->ho_c_buf_num,
                    rnn->wt_ho_c,
                    neu->er_o_c_buf,
                    rnn->model_opt.scale,
                    rnn->class_size,
                    neu->ac_h_buf,
                    hidden_size);

            neu->ho_c_buf_num = 0;
#else
            param_update(&param,
                    &neu->arg_ho_c, true,
                    rnn->wt_ho_c,
                    neu->wt_ho_c,
                    rnn->model_opt.scale,
                    -rnn->class_size,
                    NULL,
                    hidden_size);

            memset(neu->wt_ho_c, 0,
                    sizeof(real_t) * rnn->class_size * hidden_size);
#endif
        } else {
            param_update(&param,
                    &neu->arg_ho_w, true,
                    rnn->wt_ho_w,
                    neu->wt_ho_w,
                    rnn->model_opt.scale,
                    -rnn->vocab_size,
                    NULL,
                    hidden_size);

            memset(neu->wt_ho_w, 0,
                    sizeof(real_t) * rnn->vocab_size * hidden_size);
        }
    }

    if (neu->ih_w_hist_num > 0) {
        for (i = 0; i < neu->ih_w_hist_num; i++) {
            w = neu->ih_w_hist[i];

            if (w == -1) {
                continue;
            }

            if (neu->dirty_word[w]) {
                continue;
            }
            neu->dirty_word[w] = true;

            param_update(&param,
                    &neu->arg_ih_w, true,
                    rnn->wt_ih_w + w,
                    neu->wt_ih_w + w,
                    1.0,
                    -hidden_size,
                    NULL,
                    -rnn->vocab_size);

            a = w;
            for (h = 0; h < hidden_size; h++) {
                neu->wt_ih_w[a] = 0;
                a += rnn->vocab_size;
            }
        }

        for (i = 0; i < neu->ih_w_hist_num; i++) {
            w = neu->ih_w_hist[i];

            if (w == -1) {
                continue;
            }

            neu->dirty_word[w] = false;
        }
        neu->ih_w_hist_num = 0;
    }

    if (neu->ih_buf_num > 0) {
#ifdef _MINI_UPDATE_
        param_update_minibatch(&param,
                &neu->arg_ih_h, true,
                neu->ih_buf_num,
                rnn->wt_ih_h,
                neu->er_h_buf,
                1.0,
                hidden_size,
                neu->ac_i_h_buf,
                hidden_size);
#else
        param_update(&param,
                &neu->arg_ih_h, true,
                rnn->wt_ih_h,
                neu->wt_ih_h,
                1.0,
                -hidden_size,
                NULL,
                hidden_size);

        memset(neu->wt_ih_h, 0,
                sizeof(real_t) * hidden_size * hidden_size);
#endif
        neu->ih_buf_num = 0;
    }

    return 0;
}

int rnn_end_train(rnn_t *rnn, int word, int tid)
{
    rnn_neuron_t *neu;

    int mini_batch;

    ST_CHECK_PARAM(rnn == NULL || tid < 0, -1);

    neu = rnn->neurons + tid;
    mini_batch = rnn->train_opt.param.mini_batch;

    neu->last_word = word;

    // copy hidden layer to input
    memcpy(neu->ac_i_h, neu->ac_h,
            rnn->model_opt.hidden_size * sizeof(real_t));

    if (mini_batch > 0) {
        neu->mini_step++;
        if (neu->mini_step >= mini_batch) {// || word == 0) {
            if (rnn_update_minibatch(rnn, tid) < 0) {
                ST_WARNING("Failed to rnn_update_minibatch.");
                return -1;
            }
            neu->mini_step = 0;
        }
    }

    return 0;
}

int rnn_finish_train(rnn_t *rnn, int tid)
{
    ST_CHECK_PARAM(rnn == NULL || tid < 0, -1);

    if (rnn->train_opt.param.mini_batch > 0) {
        if (rnn_update_minibatch(rnn, tid) < 0) {
            ST_WARNING("Failed to rnn_update_minibatch.");
            return -1;
        }
    }

    return 0;
}

int rnn_setup_test(rnn_t *rnn, output_t *output, int num_thrs)
{
    rnn_neuron_t *neu;

    size_t sz;
    int i;
    int t;

    int hidden_size;

    ST_CHECK_PARAM(rnn == NULL || output == NULL || num_thrs < 0, -1);

    if (!rnn_check_output(rnn, output)) {
        ST_WARNING("Output layer not match.");
        return -1;
    }

    rnn->output = output;

    rnn->num_thrs = num_thrs;
    sz = sizeof(rnn_neuron_t) * num_thrs;
    rnn->neurons = (rnn_neuron_t *)malloc(sz);
    if (rnn->neurons == NULL) {
        ST_WARNING("Failed to malloc neurons.");
        goto ERR;
    }
    memset(rnn->neurons, 0, sz);

    hidden_size = rnn->model_opt.hidden_size;
    for (t = 0; t < num_thrs; t++) {
        neu = rnn->neurons + t;

        (void)posix_memalign((void **)&neu->ac_i_h, ALIGN_SIZE, sizeof(real_t) * hidden_size);
        if (neu->ac_i_h == NULL) {
            ST_WARNING("Failed to malloc ac_i_h.");
            goto ERR;
        }

        (void)posix_memalign((void **)&neu->ac_h, ALIGN_SIZE, sizeof(real_t) * hidden_size);
        if (neu->ac_h == NULL) {
            ST_WARNING("Failed to malloc ac_h.");
            goto ERR;
        }
    }

    return 0;

ERR:
    if (rnn->neurons != NULL) {
        for (i = 0; i < rnn->num_thrs; i++) {
            safe_free(rnn->neurons[i].ac_i_h);
            safe_free(rnn->neurons[i].ac_h);
        }
        safe_free(rnn->neurons);
    }
    rnn->num_thrs = 0;

    return -1;
}

int rnn_reset_test(rnn_t *rnn, int tid)
{
    rnn_neuron_t *neu;

    size_t i;

    int hidden_size;

    ST_CHECK_PARAM(rnn == NULL || tid < 0, -1);

    neu = rnn->neurons + tid;

    hidden_size = rnn->model_opt.hidden_size;

    for (i = 0; i < hidden_size; i++) {
        neu->ac_i_h[i] = 0.1;
    }

    neu->last_word = -1;

    return 0;
}

int rnn_start_test(rnn_t *rnn, int word, int tid)
{
    ST_CHECK_PARAM(rnn == NULL, -1);

    return 0;
}

int rnn_end_test(rnn_t *rnn, int word, int tid)
{
    rnn_neuron_t *neu;

    ST_CHECK_PARAM(rnn == NULL || tid < 0, -1);

    neu = rnn->neurons + tid;

    neu->last_word = word;

    // copy hidden layer to input
    memcpy(neu->ac_i_h, neu->ac_h,
            rnn->model_opt.hidden_size * sizeof(real_t));

    return 0;
}

int rnn_setup_gen(rnn_t *rnn, output_t *output)
{
    return rnn_setup_test(rnn, output, 1);
}

int rnn_reset_gen(rnn_t *rnn)
{
    return rnn_reset_test(rnn, 0);
}

int rnn_end_gen(rnn_t *rnn, int word)
{
    return rnn_end_test(rnn, word, 0);
}

