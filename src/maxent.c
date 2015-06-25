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

#include "maxent.h"

static const int MAXENT_MAGIC_NUM = 626140498 + 3;

const unsigned int PRIMES[] = {
    108641969, 116049371, 125925907, 133333309, 145678979, 175308587,
    197530793, 234567803, 251851741, 264197411, 330864029, 399999781,
    407407183, 459258997, 479012069, 545678687, 560493491, 607407037,
    629629243, 656789717, 716048933, 718518067, 725925469, 733332871,
    753085943, 755555077, 782715551, 790122953, 812345159, 814814293,
    893826581, 923456189, 940740127, 953085797, 985184539, 990122807
};

const unsigned int PRIMES_SIZE = sizeof(PRIMES) / sizeof(PRIMES[0]);

int maxent_load_model_opt(maxent_model_opt_t *model_opt,
        st_opt_t *opt, const char *sec_name)
{
    double d;
    int n;

    ST_CHECK_PARAM(model_opt == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "SCALE", d, 1.0,
            "Scale of MaxEnt model output");
    model_opt->scale = (real_t)d;

    if (model_opt->scale <= 0) {
        return 0;
    }

    ST_OPT_SEC_GET_INT(opt, sec_name, "ORDER",
            model_opt->order, 3,
            "Order of MaxEnt");

    ST_OPT_SEC_GET_INT(opt, sec_name, "WORD_HASH_SIZE", n, 1,
            "Size of MaxEnt hash for words(in millions)");
    model_opt->sz_w = n * 1000000;

    ST_OPT_SEC_GET_INT(opt, sec_name, "CLASS_HASH_SIZE", n, 1,
            "Size of MaxEnt hash for classes(in millions)");
    model_opt->sz_c = n * 1000000;

    return 0;

ST_OPT_ERR:
    return -1;
}

int maxent_load_train_opt(maxent_train_opt_t *train_opt,
        st_opt_t *opt, const char *sec_name, param_t *param)
{
    ST_CHECK_PARAM(train_opt == NULL || opt == NULL, -1);

    if (param_load(&train_opt->param, opt, sec_name, param) < 0) {
        ST_WARNING("Failed to param_load.");
        goto ST_OPT_ERR;
    }

    return 0;

ST_OPT_ERR:
    return -1;
}

int maxent_init(maxent_t **pmaxent, maxent_model_opt_t *model_opt,
        output_t *output)
{
    maxent_t *maxent = NULL;
    hash_size_t i;

    int class_size;
    int vocab_size;

    ST_CHECK_PARAM(pmaxent == NULL || model_opt == NULL, -1);

    *pmaxent = NULL;

    if (model_opt->scale <= 0) {
        return 0;
    }

    class_size = output->output_opt.class_size;
    vocab_size = output->output_size;

    maxent = (maxent_t *) malloc(sizeof(maxent_t));
    if (maxent == NULL) {
        ST_WARNING("Falied to malloc maxent_t.");
        goto ERR;
    }
    memset(maxent, 0, sizeof(maxent_t));

    maxent->model_opt = *model_opt;
    maxent->output = output;
    maxent->class_size = class_size;
    maxent->vocab_size = vocab_size;

    posix_memalign((void **)&maxent->wt_w, ALIGN_SIZE, sizeof(real_t) * model_opt->sz_w);
    if (maxent->wt_w == NULL) {
        ST_WARNING("Failed to malloc wt_w.");
        goto ERR;
    }

    for (i = 0; i < model_opt->sz_w; i++) {
        maxent->wt_w[i] = 0;
    }

    if (maxent->class_size > 0) {
        posix_memalign((void **)&maxent->wt_c, ALIGN_SIZE, sizeof(real_t) * model_opt->sz_c);
        if (maxent->wt_c == NULL) {
            ST_WARNING("Failed to malloc wt_c.");
            goto ERR;
        }

        for (i = 0; i < model_opt->sz_c; i++) {
            maxent->wt_c[i] = 0;
        }
    }

    *pmaxent = maxent;

    return 0;

ERR:
    safe_maxent_destroy(maxent);
    return -1;
}

void maxent_destroy(maxent_t *maxent)
{
    int i;
    int j;

    if (maxent == NULL) {
        return;
    }

    safe_free(maxent->wt_w);
    safe_free(maxent->wt_c);

    for (i = 0; i < maxent->num_thrs; i++) {
        safe_free(maxent->neurons[i].hist);
        safe_free(maxent->neurons[i].hash_w);
        safe_free(maxent->neurons[i].hash_c);
        if (maxent->neurons[i].hash_hist_w != NULL) {
            for (j = 0; j < maxent->train_opt.param.mini_batch; j++) {
                safe_free(maxent->neurons[i].hash_hist_w[j].range);
            }
            safe_free(maxent->neurons[i].hash_hist_w);
        }
        if (maxent->neurons[i].hash_hist_c != NULL) {
            for (j = 0; j < maxent->train_opt.param.mini_batch; j++) {
                safe_free(maxent->neurons[i].hash_hist_c[j].range);
            }
            safe_free(maxent->neurons[i].hash_hist_c);
        }
        safe_free(maxent->neurons[i].hash_union);
        safe_free(maxent->neurons[i].wt_w);
        safe_free(maxent->neurons[i].wt_c);
    }
    safe_free(maxent->neurons);
}

maxent_t* maxent_dup(maxent_t *m)
{
    maxent_t *maxent = NULL;

    ST_CHECK_PARAM(m == NULL, NULL);

    maxent = (maxent_t *) malloc(sizeof(maxent_t));
    if (maxent == NULL) {
        ST_WARNING("Falied to malloc maxent_t.");
        goto ERR;
    }

    *maxent = *m;
    maxent->model_opt = m->model_opt;

    posix_memalign((void **)&maxent->wt_w, ALIGN_SIZE, sizeof(real_t) * m->model_opt.sz_w);
    if (maxent->wt_w == NULL) {
        ST_WARNING("Failed to malloc wt_w.");
        goto ERR;
    }
    memcpy(maxent->wt_w, m->wt_w, sizeof(real_t)*m->model_opt.sz_w);

    if (maxent->class_size > 0) {
        posix_memalign((void **)&maxent->wt_c, ALIGN_SIZE, sizeof(real_t) * m->model_opt.sz_c);
        if (maxent->wt_c == NULL) {
            ST_WARNING("Failed to malloc wt_c.");
            goto ERR;
        }
        memcpy(maxent->wt_c, m->wt_c, sizeof(real_t)*m->model_opt.sz_c);
    }

    return maxent;

ERR:
    safe_maxent_destroy(maxent);
    return NULL;
}

int maxent_load_header(maxent_t **maxent, FILE *fp,
        bool *binary, FILE *fo_info)
{
    union {
        char str[4];
        int magic_num;
    } flag;

    real_t scale;
    int vocab_size;
    int class_size;

    int order;
    hash_size_t sz_w;
    hash_size_t sz_c;

    ST_CHECK_PARAM((maxent == NULL && fo_info == NULL) || fp == NULL
            || binary == NULL, -1);

    if (fread(&flag.magic_num, sizeof(int), 1, fp) != 1) {
        ST_WARNING("Failed to load magic num.");
        return -1;
    }

    if (strncmp(flag.str, "    ", 4) == 0) {
        *binary = false;
    } else if (MAXENT_MAGIC_NUM != flag.magic_num) {
        ST_WARNING("magic num wrong.");
        return -2;
    } else {
        *binary = true;
    }

    if (maxent != NULL) {
        *maxent = NULL;
    }

    if (*binary) {
        if (fread(&scale, sizeof(real_t), 1, fp) != 1) {
            ST_WARNING("Failed to read scale.");
            goto ERR;
        }

        if (scale <= 0) {
            if (fo_info != NULL) {
                fprintf(fo_info, "\n<MAXENT>: None\n");
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

        if (fread(&order, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read order");
            goto ERR;
        }

        if (fread(&sz_w, sizeof(hash_size_t), 1, fp) != 1) {
            ST_WARNING("Failed to read sz_w");
            goto ERR;
        }

        if (class_size > 0) {
            if (fread(&sz_c, sizeof(hash_size_t), 1, fp) != 1) {
                ST_WARNING("Failed to read sz_c");
                goto ERR;
            }
        }
    } else {
        if (st_readline(fp, "<MAXENT>") != 0) {
            ST_WARNING("tag error");
            goto ERR;
        }

        if (st_readline(fp, "Scale: " REAL_FMT, &scale) != 1) {
            ST_WARNING("Failed to read scale.");
            goto ERR;
        }

        if (scale <= 0) {
            if (fo_info != NULL) {
                fprintf(fo_info, "\n<MAXENT>: None\n");
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

        if (st_readline(fp, "Order: %d", &order) != 1) {
            ST_WARNING("Failed to read order.");
            goto ERR;
        }

        if (st_readline(fp, "Word Hash Size: " HASH_SIZE_FMT, &sz_w) != 1) {
            ST_WARNING("Failed to read sz_w");
            goto ERR;
        }

        if (class_size > 0) {
            if (st_readline(fp, "Class Hash Size: " HASH_SIZE_FMT,
                        &sz_c) != 1) {
                ST_WARNING("Failed to read sz_c");
                goto ERR;
            }
        }
    }

    if (maxent != NULL) {
        *maxent = (maxent_t *)malloc(sizeof(maxent_t));
        if (*maxent == NULL) {
            ST_WARNING("Failed to malloc maxent_t");
            goto ERR;
        }
        memset(*maxent, 0, sizeof(maxent_t));

        (*maxent)->model_opt.scale = scale;
        (*maxent)->vocab_size = vocab_size;
        (*maxent)->class_size = class_size;

        (*maxent)->model_opt.order = order;
        (*maxent)->model_opt.sz_w = sz_w;
        (*maxent)->model_opt.sz_c = sz_c;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<MAXENT>: %g\n", scale);
        fprintf(fo_info, "Vocab Size: %d\n", vocab_size);
        fprintf(fo_info, "Class Size: %d\n", class_size);

        fprintf(fo_info, "Order: %d\n", order);
        fprintf(fo_info, "Word Hash Size: " HASH_SIZE_FMT "\n", sz_w);
        if (class_size > 0) {
            fprintf(fo_info, "Class Hash Size: " HASH_SIZE_FMT "\n", sz_c);
        }
    }

    return 0;

ERR:
    safe_maxent_destroy(*maxent);
    return -1;
}

int maxent_load_body(maxent_t *maxent, FILE *fp, bool binary)
{
    int n;

    size_t i;
    size_t sz;

    ST_CHECK_PARAM(maxent == NULL || fp == NULL, -1);

    sz = maxent->model_opt.sz_w;
    posix_memalign((void **)&maxent->wt_w, ALIGN_SIZE, sizeof(real_t) * sz);
    if (maxent->wt_w == NULL) {
        ST_WARNING("Failed to malloc wt_w.");
        goto ERR;
    }

    if (maxent->class_size > 0) {
        sz = maxent->model_opt.sz_c;
        posix_memalign((void **)&maxent->wt_c, ALIGN_SIZE, sizeof(real_t) * sz);
        if (maxent->wt_c == NULL) {
            ST_WARNING("Failed to malloc wt_c.");
            goto ERR;
        }
    }

    if (binary) {
        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read magic num.");
            goto ERR;
        }

        if (n != -MAXENT_MAGIC_NUM) {
            ST_WARNING("Magic num error.");
            goto ERR;
        }

        sz = maxent->model_opt.sz_w;
        if (fread(maxent->wt_w, sizeof(real_t), sz, fp) != sz) {
            ST_WARNING("Failed to read wt_w.");
            goto ERR;
        }
        if (maxent->class_size > 0) {
            sz = maxent->model_opt.sz_c;
            if (fread(maxent->wt_c, sizeof(real_t), sz, fp) != sz) {
                ST_WARNING("Failed to read wt_c.");
                goto ERR;
            }
        }
    } else {
        if (st_readline(fp, "<MAXENT-DATA>") != 0) {
            ST_WARNING("body tag error.");
            goto ERR;
        }

        if (st_readline(fp, "Weights input -> output (word):") != 0) {
            ST_WARNING("wt_w tag error.");
            goto ERR;
        }

        sz = maxent->model_opt.sz_w;
        for (i = 0; i < sz; i++) {
            if (st_readline(fp, REAL_FMT, maxent->wt_w + i) != 1) {
                ST_WARNING("Failed to read wt_w.");
                goto ERR;
            }
        }

        if (maxent->class_size > 0) {
            if (st_readline(fp, "Weights input -> output (class):") != 0) {
                ST_WARNING("wt_c tag error.");
                goto ERR;
            }
            sz = maxent->model_opt.sz_c;
            for (i = 0; i < sz; i++) {
                if (st_readline(fp, REAL_FMT, maxent->wt_c + i) != 1) {
                    ST_WARNING("Failed to read wt_c.");
                    goto ERR;
                }
            }
        }
    }

    return 0;

ERR:
    safe_free(maxent->wt_w);
    safe_free(maxent->wt_c);
    return -1;
}

int maxent_save_header(maxent_t *maxent, FILE *fp, bool binary)
{
    real_t scale;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (binary) {
        if (fwrite(&MAXENT_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }
        if (maxent == NULL) {
            scale = 0;
            if (fwrite(&scale, sizeof(real_t), 1, fp) != 1) {
                ST_WARNING("Failed to write scale.");
                return -1;
            }
            return 0;
        }

        if (fwrite(&maxent->model_opt.scale, sizeof(real_t), 1, fp) != 1) {
            ST_WARNING("Failed to write scale.");
            return -1;
        }
        if (fwrite(&maxent->vocab_size, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write vocab size.");
            return -1;
        }
        if (fwrite(&maxent->class_size, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write class size.");
            return -1;
        }

        if (fwrite(&maxent->model_opt.order, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write order.");
            return -1;
        }

        if (fwrite(&maxent->model_opt.sz_w, sizeof(hash_size_t),
                    1, fp) != 1) {
            ST_WARNING("Failed to write sz_w.");
            return -1;
        }

        if (maxent->class_size > 0) {
            if (fwrite(&maxent->model_opt.sz_c, sizeof(hash_size_t),
                        1, fp) != 1) {
                ST_WARNING("Failed to write sz_c.");
                return -1;
            }
        }
    } else {
        fprintf(fp, "    \n<MAXENT>\n");

        if (maxent == NULL) {
            fprintf(fp, "Scale: 0\n");

            return 0;
        }

        fprintf(fp, "Scale: %g\n", maxent->model_opt.scale);
        fprintf(fp, "Vocab Size: %d\n", maxent->vocab_size);
        fprintf(fp, "Class Size: %d\n", maxent->class_size);

        fprintf(fp, "Order: %d\n", maxent->model_opt.order);
        fprintf(fp, "Word Hash Size: " HASH_SIZE_FMT "\n",
                maxent->model_opt.sz_w);
        if (maxent->class_size > 0) {
            fprintf(fp, "Class Hash Size: " HASH_SIZE_FMT "\n",
                    maxent->model_opt.sz_c);
        }
    }

    return 0;
}

int maxent_save_body(maxent_t *maxent, FILE *fp, bool binary)
{
    int n;

    size_t i;
    size_t sz;

    ST_CHECK_PARAM(maxent == NULL || fp == NULL, -1);

    if (binary) {
        n = -MAXENT_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        sz = maxent->model_opt.sz_w;
        if (fwrite(maxent->wt_w, sizeof(real_t), sz, fp) != sz) {
            ST_WARNING("Failed to write wt_w.");
            return -1;
        }

        if (maxent->class_size) {
            sz = maxent->model_opt.sz_c;
            if (fwrite(maxent->wt_c, sizeof(real_t), sz, fp) != sz) {
                ST_WARNING("Failed to write wt_c.");
                return -1;
            }
        }
    } else {
        fprintf(fp, "<MAXENT-DATA>\n");

        fprintf(fp, "Weights input -> output (word):\n");
        sz = maxent->model_opt.sz_w;
        for (i = 0; i < sz; i++) {
            fprintf(fp, REAL_FMT "\n", maxent->wt_w[i]);
        }
        if (maxent->class_size) {
            fprintf(fp, "Weights input -> output (class):\n");
            sz = maxent->model_opt.sz_c;
            for (i = 0; i < sz; i++) {
                fprintf(fp, REAL_FMT "\n", maxent->wt_c[i]);
            }
        }
    }

    return 0;
}

static int maxent_get_hash(hash_t *hash, hash_t init_val,
        int *hist, int order)
{
    int a;
    int b;

    hash[0] = PRIMES[0] * PRIMES[1] * init_val;

    for (a = 1; a < order; a++) {
        if (hist[a - 1] < 0) {
            // if OOV was in history, do not use
            // this N-gram feature and higher orders
            return a;
        }

        hash[a] = hash[0];
        for (b = 1; b <= a; b++) {
            hash[a] += PRIMES[(a*PRIMES[b] + b) % PRIMES_SIZE]
                * (hash_t)(hist[b-1] + 1);
        }
    }

    return order;
}

static int maxent_forward_class(maxent_t *maxent, int tid)
{
    maxent_neuron_t *neu;
    output_neuron_t *output_neu;

    hash_t h;
    int order;

    int a;
    int o;

    ST_CHECK_PARAM(maxent == NULL || tid < 0, -1);

    neu = maxent->neurons + tid;
    output_neu = maxent->output->neurons + tid;

    order = maxent_get_hash(neu->hash_c, (hash_t)1,
            neu->hist, maxent->model_opt.order);
    if (order < 0) {
        ST_WARNING("Failed to maxent_get_hash.");
        return -1;
    }
#ifndef _MAXENT_BP_CALC_HASH_
    neu->hash_order_c = order;
#endif

    for (a = 0; a < order; a++) {
        neu->hash_c[a] = neu->hash_c[a] % maxent->model_opt.sz_c;
    }

    for (a = 0; a < order; a++) {
        h = neu->hash_c[a];
        for (o = 0; o < maxent->class_size; o++) {
            output_neu->ac_o_c[o] += maxent->model_opt.scale
                * maxent->wt_c[h];
            h++;
            if (h >= maxent->model_opt.sz_c) {
                h = 0;
            }
        }
    }

    return 0;
}

static int maxent_forward_word(maxent_t *maxent, int c, int s, int e,
        int tid)
{
    maxent_neuron_t *neu;
    output_neuron_t *output_neu;

    hash_t h;
    int order;

    int a;
    int o;

    ST_CHECK_PARAM(maxent == NULL || tid < 0, -1);

    neu = maxent->neurons + tid;
    output_neu = maxent->output->neurons + tid;

    order = maxent_get_hash(neu->hash_w, (hash_t)(c + 1),
            neu->hist, maxent->model_opt.order);
    if (order < 0) {
        ST_WARNING("Failed to maxent_get_hash.");
        return -1;
    }
#ifndef _MAXENT_BP_CALC_HASH_
    neu->hash_order_w = order;
#endif

    for (a = 0; a < order; a++) {
        neu->hash_w[a] %= maxent->model_opt.sz_w;
    }

    for (a = 0; a < order; a++) {
        h = neu->hash_w[a];
        for (o = s; o < e; o++) {
            output_neu->ac_o_w[o] += maxent->model_opt.scale
                * maxent->wt_w[h];
            h++;
            if (h >= maxent->model_opt.sz_w) {
                h = 0;
            }
        }
    }

    return 0;
}

int maxent_forward(maxent_t *maxent, int word, int tid)
{
    int c;
    int s;
    int e;

    ST_CHECK_PARAM(maxent == NULL || tid < 0, -1);

    if (word < 0) {
        return 0;
    }

    if (maxent->class_size > 0) {
        c = maxent->output->w2c[word];
        s = maxent->output->c2w_s[c];
        e = maxent->output->c2w_e[c];

        if (maxent_forward_class(maxent, tid) < 0) {
            ST_WARNING("Failed to maxent_forward_class.");
            return -1;
        }
    } else {
        c = 0;
        s = 0;
        e = maxent->vocab_size;
    }

    if (maxent_forward_word(maxent, c, s, e, tid) < 0) {
        ST_WARNING("Failed to maxent_forward_word.");
        return -1;
    }

    return 0;
}

static int maxent_backprop_class(maxent_t *maxent, int tid)
{
    maxent_neuron_t *neu;
    output_neuron_t *output_neu;

    hash_size_t sz;
    int order;

    int a;

    ST_CHECK_PARAM(maxent == NULL || tid < 0, -1);

    neu = maxent->neurons + tid;
    output_neu = maxent->output->neurons + tid;

#ifdef _MAXENT_BP_CALC_HASH_
    order = maxent_get_hash(neu->hash_c, (hash_t)1,
            neu->hist, maxent->model_opt.order);
    if (order < 0) {
        ST_WARNING("Failed to maxent_get_hash.");
        return -1;
    }

    for (a = 0; a < order; a++) {
        neu->hash_c[a] %= maxent->model_opt.sz_c;
    }
#else
    order = neu->hash_order_c;
#endif

    for (a = 0; a < order; a++) {
        if (neu->hash_c[a] + maxent->class_size > maxent->model_opt.sz_c) {
            sz = maxent->model_opt.sz_c - neu->hash_c[a];
            if (maxent->train_opt.param.mini_batch > 0) {
                param_acc_wt(neu->wt_c + neu->hash_c[a],
                        output_neu->er_o_c,
                        sz,
                        NULL,
                        -1);
                param_acc_wt(neu->wt_c,
                        output_neu->er_o_c + sz,
                        maxent->class_size - sz,
                        NULL,
                        -1);
            } else {
                param_update(&maxent->train_opt.param,
                        &neu->param_arg_c, false,
                        maxent->wt_c + neu->hash_c[a],
                        output_neu->er_o_c,
                        maxent->model_opt.scale,
                        sz,
                        NULL,
                        -1);
                param_update(&maxent->train_opt.param,
                        &neu->param_arg_c, true,
                        maxent->wt_c,
                        output_neu->er_o_c + sz,
                        maxent->model_opt.scale,
                        maxent->class_size - sz,
                        NULL,
                        -1);
            }
        } else {
            if (maxent->train_opt.param.mini_batch > 0) {
                param_acc_wt(neu->wt_c + neu->hash_c[a],
                        output_neu->er_o_c,
                        maxent->class_size,
                        NULL,
                        -1);
            } else {
                param_update(&maxent->train_opt.param,
                        &neu->param_arg_c, true,
                        maxent->wt_c + neu->hash_c[a],
                        output_neu->er_o_c,
                        maxent->model_opt.scale,
                        maxent->class_size,
                        NULL,
                        -1);
            }
        }
        if (maxent->train_opt.param.mini_batch > 0) {
            neu->hash_hist_c[neu->hash_hist_c_num].range[a].s = neu->hash_c[a];
            neu->hash_hist_c[neu->hash_hist_c_num].range[a].num = maxent->class_size;
        }
    }
    if (maxent->train_opt.param.mini_batch > 0 && order > 0) {
        neu->hash_hist_c[neu->hash_hist_c_num].order = order;
        neu->hash_hist_c_num++;
    }

    return 0;
}

static int maxent_backprop_word(maxent_t *maxent, int c, int s, int e,
        int tid)
{
    maxent_neuron_t *neu;
    output_neuron_t *output_neu;

    hash_size_t sz;
    int order;

    int a;

    ST_CHECK_PARAM(maxent == NULL || tid < 0, -1);

    neu = maxent->neurons + tid;
    output_neu = maxent->output->neurons + tid;

#ifdef _MAXENT_BP_CALC_HASH_
    order = maxent_get_hash(neu->hash_w, (hash_t)(c + 1),
                neu->hist, maxent->model_opt.order);
    if (order < 0) {
        ST_WARNING("Failed to maxent_get_hash.");
        return -1;
    }

    for (a = 0; a < order; a++) {
        neu->hash_w[a] %= maxent->model_opt.sz_w;
    }
#else
    order = neu->hash_order_w;
#endif

    for (a = 0; a < order; a++) {
        if (neu->hash_w[a] + (e - s) > maxent->model_opt.sz_w) {
            sz = maxent->model_opt.sz_w - neu->hash_w[a];
            if (maxent->train_opt.param.mini_batch > 0) {
                param_acc_wt(neu->wt_w + neu->hash_w[a],
                        output_neu->er_o_w + s,
                        sz,
                        NULL,
                        -1);
                param_acc_wt(neu->wt_w,
                        output_neu->er_o_w + s + sz,
                        (e - s) - sz,
                        NULL,
                        -1);
            } else {
                param_update(&maxent->train_opt.param,
                        &neu->param_arg_w, false,
                        maxent->wt_w + neu->hash_w[a],
                        output_neu->er_o_w + s,
                        maxent->model_opt.scale,
                        sz,
                        NULL,
                        -1);
                param_update(&maxent->train_opt.param,
                        &neu->param_arg_w, true,
                        maxent->wt_w,
                        output_neu->er_o_w + s + sz,
                        maxent->model_opt.scale,
                        (e - s) - sz,
                        NULL,
                        -1);
            }
        } else {
            if (maxent->train_opt.param.mini_batch > 0) {
                param_acc_wt(neu->wt_w + neu->hash_w[a],
                        output_neu->er_o_w + s,
                        e - s,
                        NULL,
                        -1);
            } else {
                param_update(&maxent->train_opt.param,
                        &neu->param_arg_w, true,
                        maxent->wt_w + neu->hash_w[a],
                        output_neu->er_o_w + s,
                        maxent->model_opt.scale,
                        e - s,
                        NULL,
                        -1);
            }
        }
        if (maxent->train_opt.param.mini_batch > 0) {
            neu->hash_hist_w[neu->hash_hist_w_num].range[a].s = neu->hash_w[a];
            neu->hash_hist_w[neu->hash_hist_w_num].range[a].num = e - s;
        }
    }
    if (maxent->train_opt.param.mini_batch > 0 && order > 0) {
        neu->hash_hist_w[neu->hash_hist_w_num].order = order;
        neu->hash_hist_w_num++;
    }

    return 0;
}

int maxent_backprop(maxent_t *maxent, int word, int tid)
{
    int c;
    int s;
    int e;

    ST_CHECK_PARAM(maxent == NULL || tid < 0, -1);

    if (word < 0) {
        return 0;
    }

    if (maxent->class_size > 0) {
        c = maxent->output->w2c[word];
        s = maxent->output->c2w_s[c];
        e = maxent->output->c2w_e[c];

        if (maxent_backprop_class(maxent, tid) < 0) {
            ST_WARNING("Failed to maxent_backprop_class.");
            return -1;
        }
    } else {
        c = 0;
        s = 0;
        e = maxent->vocab_size;
    }

    if (maxent_backprop_word(maxent, c, s, e, tid) < 0) {
        ST_WARNING("Failed to maxent_backprop_word.");
        return -1;
    }

    return 0;
}

static bool maxent_check_output(maxent_t *maxent, output_t *output)
{
    ST_CHECK_PARAM(maxent == NULL || output == NULL, false);

    if (maxent->class_size != output->output_opt.class_size) {
        ST_WARNING("Class size not match");
        return false;
    } else if (maxent->vocab_size != output->output_size) {
        ST_WARNING("Vocab size not match");
        return false;
    }

    return true;
}

int maxent_setup_train(maxent_t *maxent, maxent_train_opt_t *train_opt,
        output_t *output, int num_thrs)
{
    maxent_neuron_t *neu;

    size_t sz;

    int order;
    int i;
    int j;
    int t;

    ST_CHECK_PARAM(maxent == NULL || train_opt == NULL
            || output == NULL || num_thrs < 0, -1);

    if (!maxent_check_output(maxent, output)) {
        ST_WARNING("Output layer not match.");
        return -1;
    }

    maxent->train_opt = *train_opt;
    maxent->output = output;

    maxent->num_thrs = num_thrs;
    sz = sizeof(maxent_neuron_t) * num_thrs;
    maxent->neurons = (maxent_neuron_t *)malloc(sz);
    if (maxent->neurons == NULL) {
        ST_WARNING("Failed to malloc neurons.");
        goto ERR;
    }
    memset(maxent->neurons, 0, sz);

    order = maxent->model_opt.order;
    for (t = 0; t < num_thrs; t++) {
        neu = maxent->neurons + t;

        param_arg_clear(&neu->param_arg_w);

        neu->hist = (int *) malloc(sizeof(int) * order);
        if (neu->hist == NULL) {
            ST_WARNING("Failed to malloc hist.");
            goto ERR;
        }
        for (i = 0; i < order; i++) {
            neu->hist[i] = -1;
        }

        neu->hash_w = (hash_t *) malloc(sizeof(hash_t) * order);
        if (neu->hash_w == NULL) {
            ST_WARNING("Failed to malloc hash_w.");
            goto ERR;
        }

        if (maxent->class_size > 0) {
            param_arg_clear(&neu->param_arg_c);

            neu->hash_c = (hash_t *) malloc(sizeof(hash_t) * order);
            if (neu->hash_c == NULL) {
                ST_WARNING("Failed to malloc hash_c.");
                goto ERR;
            }
        }

        if (train_opt->param.mini_batch > 0) {
            sz = maxent->model_opt.sz_w;
            posix_memalign((void **)&neu->wt_w, ALIGN_SIZE, sizeof(real_t) * sz);
            if (neu->wt_w == NULL) {
                ST_WARNING("Failed to malloc wt_w.");
                goto ERR;
            }
            memset(neu->wt_w, 0, sizeof(real_t)*sz);

            sz = train_opt->param.mini_batch * sizeof(hash_hist_t);
            neu->hash_hist_w = (hash_hist_t *) malloc(sz);
            if (neu->hash_hist_w == NULL) {
                ST_WARNING("Failed to malloc hash_hist_w.");
                goto ERR;
            }
            memset(neu->hash_hist_w, 0, sz);

            sz = sizeof(hash_range_t) * order;
            for (i = 0; i < train_opt->param.mini_batch; i++) {
                neu->hash_hist_w[i].range = (hash_range_t *)malloc(sz);
                if (neu->hash_hist_w[i].range == NULL) {
                    ST_WARNING("Failed to malloc hash_hist_w->range.");
                    goto ERR;
                }
                memset(neu->hash_hist_w[i].range, 0, sz);
            }

            sz = 2 * train_opt->param.mini_batch * order * sizeof(hash_range_t);
            neu->hash_union = (hash_range_t *) malloc(sz);
            if (neu->hash_union == NULL) {
                ST_WARNING("Failed to malloc hash_union.");
                goto ERR;
            }
            memset(neu->hash_union, 0, sz);

            if (maxent->class_size > 0) {
                sz = maxent->model_opt.sz_c;
                posix_memalign((void **)&neu->wt_c, ALIGN_SIZE, sizeof(real_t) * sz);
                if (neu->wt_c == NULL) {
                    ST_WARNING("Failed to malloc wt_c.");
                    goto ERR;
                }
                memset(neu->wt_c, 0, sizeof(real_t)*sz);

                sz = train_opt->param.mini_batch*sizeof(hash_hist_t);
                neu->hash_hist_c = (hash_hist_t *) malloc(sz);
                if (neu->hash_hist_c == NULL) {
                    ST_WARNING("Failed to malloc hash_hist_c.");
                    goto ERR;
                }
                memset(neu->hash_hist_c, 0, sz);

                sz = sizeof(hash_range_t) * order;
                for (i = 0; i < train_opt->param.mini_batch; i++) {
                    neu->hash_hist_c[i].range = (hash_range_t *)malloc(sz);
                    if (neu->hash_hist_c[i].range == NULL) {
                        ST_WARNING("Failed to malloc hash_hist_c->range.");
                        goto ERR;
                    }
                    memset(neu->hash_hist_c[i].range, 0, sz);
                }
            }
        }
    }

    return 0;
ERR:
    for (i = 0; i < maxent->num_thrs; i++) {
        safe_free(maxent->neurons[i].hist);
        safe_free(maxent->neurons[i].hash_w);
        safe_free(maxent->neurons[i].hash_c);
        if (maxent->neurons[i].hash_hist_w != NULL) {
            for (j = 0; j < maxent->train_opt.param.mini_batch; j++) {
                safe_free(maxent->neurons[i].hash_hist_w[j].range);
            }
            safe_free(maxent->neurons[i].hash_hist_w);
        }
        if (maxent->neurons[i].hash_hist_c != NULL) {
            for (j = 0; j < maxent->train_opt.param.mini_batch; j++) {
                safe_free(maxent->neurons[i].hash_hist_c[j].range);
            }
            safe_free(maxent->neurons[i].hash_hist_c);
        }
        safe_free(maxent->neurons[i].hash_union);
        safe_free(maxent->neurons[i].wt_w);
        safe_free(maxent->neurons[i].wt_c);
    }
    safe_free(maxent->neurons);

    return -1;
}

int maxent_reset_train(maxent_t *maxent, int tid)
{
    maxent_neuron_t *neu;

    int order;
    int i;

    ST_CHECK_PARAM(maxent == NULL || tid < 0, -1);

    neu = maxent->neurons + tid;

    order = maxent->model_opt.order;
    for (i = 0; i < order; i++) {
        neu->hist[i] = -1;
    }

    return 0;
}

int maxent_start_train(maxent_t *maxent, int word, int tid)
{
    return 0;
}

static int maxent_union_insert(hash_range_t *range, int *n_range,
        hash_t s, hash_t e)
{
    int i;
    int j;
    int n;

    ST_CHECK_PARAM_EX(range == NULL || n_range == NULL
            || s < 0 || e < 0 || s >= e, -1, "%lld, %lld", s, e);

    n = *n_range;

    if (e < range[0].s) {
        memmove(range + 1, range, sizeof(hash_range_t)*n);
        range[0].s = s;
        range[0].e = e;
        (*n_range)++;
        return 0;
    } else if (e == range[0].s) {
        range[0].s = s;
        return 0;
    }

    // skip non-intersected sets
    for (i = 0; i < n; i++) {
        if (s <= range[i].e) {
            break;
        }
    }

    if (i == n) {
        range[n].s = s;
        range[n].e = e;

        (*n_range)++;
        return 0;
    }

    if (e < range[i].s) { // insert new set before ith
        memmove(range + i + 1, range + i,
                sizeof(hash_range_t) * (n - i));
        range[i].s = s;
        range[i].e = e;
        (*n_range)++;
    } else {// forward extend the ith set
        for (j = i + 1; j < n; j++) {
            if (e <= range[j].s) {
                break;
            }
        }

        range[i].s = min(s, range[i].s);
        if (j == n || e < range[j].s) {
            range[i].e = max(e, range[j - 1].e);
            if (j < n) {
                memmove(range + i + 1, range + j,
                        sizeof(hash_range_t) * (n - j));
            }
            *n_range -= (j - i - 1);
        } else {
            range[i].e = max(e, range[j].e);

            if (j + 1 < n) {
                memmove(range + i + 1, range + j + 1,
                        sizeof(hash_range_t) * (n - (j + 1)));
            }
            *n_range -= (j - i);
        }
    }

    return 0;
}

/*
 * Find union of num_hash sets of hash
 */
int maxent_union(hash_range_t *range, int *n_range, hash_range_t *hash,
        int num_hash, hash_size_t sz_hash)
{
    hash_t s;
    hash_t e;

    int i;

    ST_CHECK_PARAM(range == NULL || n_range == NULL
            || *n_range < 0  || hash == NULL, -1);

    for (i = 0; i < num_hash; i++) {
        if (hash[i].s + hash[i].num > sz_hash) {
            s = hash[i].s;
            e = sz_hash;
            if (maxent_union_insert(range, n_range, s, e) < 0) {
                ST_WARNING("Failed to maxent_union_insert.");
                return -1;
            }

            s = 0;
            e = hash[i].num - (sz_hash - hash[i].s);
            if (maxent_union_insert(range, n_range, s, e) < 0) {
                ST_WARNING("Failed to maxent_union_insert.");
                return -1;
            }
        } else {
            s = hash[i].s;
            e = hash[i].s + hash[i].num;
            if (maxent_union_insert(range, n_range, s, e) < 0) {
                ST_WARNING("Failed to maxent_union_insert.");
                return -1;
            }
        }
    }

    return 0;
}

static int maxent_update_minibatch(maxent_t *maxent, int tid)
{
    maxent_neuron_t *neu;

    hash_t s;
    hash_t e;
    hash_size_t sz;

    int i;
    int n;
    int h;

    ST_CHECK_PARAM(maxent == NULL || tid < 0, -1);

    neu = maxent->neurons + tid;

    if (maxent->class_size > 0 && neu->hash_hist_c_num > 0) {
        sz = maxent->model_opt.sz_c;
        n = 0;
        for (h = 0; h < neu->hash_hist_c_num; h++) {
            if (maxent_union(neu->hash_union, &n,
                        neu->hash_hist_c[h].range,
                        neu->hash_hist_c[h].order, sz) < 0) {
                ST_WARNING("Failed to maxent_union.");
                return -1;
            }
        }

        for (i = 0; i < n; i++) {
            s = neu->hash_union[i].s;
            e = neu->hash_union[i].e;
            param_update(&maxent->train_opt.param,
                    &neu->param_arg_c, (i == n - 1),
                    maxent->wt_c + s,
                    neu->wt_c + s,
                    maxent->model_opt.scale,
                    e - s,
                    NULL,
                    -1);
            memset(neu->wt_c + s, 0, sizeof(real_t) * (e - s));
        }
        neu->hash_hist_c_num = 0;
    }

    if (neu->hash_hist_w_num > 0) {
        sz = maxent->model_opt.sz_w;
        n = 0;
        for (h = 0; h < neu->hash_hist_w_num; h++) {
            if (maxent_union(neu->hash_union, &n,
                        neu->hash_hist_w[h].range,
                        neu->hash_hist_w[h].order,
                        sz) < 0) {
                ST_WARNING("Failed to maxent_union.");
                return -1;
            }
        }

        for (i = 0; i < n; i++) {
            s = neu->hash_union[i].s;
            e = neu->hash_union[i].e;
            param_update(&maxent->train_opt.param,
                    &neu->param_arg_w, (i == n - 1),
                    maxent->wt_w + s,
                    neu->wt_w + s,
                    maxent->model_opt.scale,
                    e - s,
                    NULL,
                    -1);
            memset(neu->wt_w + s, 0, sizeof(real_t) * (e - s));
        }
        neu->hash_hist_w_num = 0;
    }

    return 0;
}

int maxent_end_train(maxent_t *maxent, int word, int tid)
{
    maxent_neuron_t *neu;

    int mini_batch;
    int i;

    ST_CHECK_PARAM(maxent == NULL || tid < 0, -1);

    mini_batch = maxent->train_opt.param.mini_batch;
    neu = maxent->neurons + tid;

    for (i = maxent->model_opt.order - 1; i > 0; i--) {
        neu->hist[i] = neu->hist[i - 1];
    }
    neu->hist[0] = word;
    neu->mini_step++;

    if (mini_batch > 0) {
        if (neu->mini_step >= mini_batch) {// || word == 0) {
            if (maxent_update_minibatch(maxent, tid) < 0) {
                ST_WARNING("Failed to maxent_update_minibatch.");
                return -1;
            }
            neu->mini_step = 0;
        }
    }

    return 0;
}

int maxent_finish_train(maxent_t *maxent, int tid)
{
    ST_CHECK_PARAM(maxent == NULL || tid < 0, -1);

    if (maxent->train_opt.param.mini_batch > 0) {
        if (maxent_update_minibatch(maxent, tid) < 0) {
            ST_WARNING("Failed to maxent_update_minibatch.");
            return -1;
        }
    }

    return 0;
}

int maxent_setup_test(maxent_t *maxent, output_t *output, int num_thrs)
{
    maxent_neuron_t *neu;

    size_t sz;

    int order;
    int i;
    int t;

    ST_CHECK_PARAM(maxent == NULL || output == NULL || num_thrs < 0, -1);

    if (!maxent_check_output(maxent, output)) {
        ST_WARNING("Output layer not match.");
        return -1;
    }

    maxent->output = output;

    maxent->num_thrs = num_thrs;
    sz = sizeof(maxent_neuron_t) * num_thrs;
    maxent->neurons = (maxent_neuron_t *)malloc(sz);
    if (maxent->neurons == NULL) {
        ST_WARNING("Failed to malloc neurons.");
        goto ERR;
    }
    memset(maxent->neurons, 0, sz);

    order = maxent->model_opt.order;
    for (t = 0; t < num_thrs; t++) {
        neu = maxent->neurons + t;

        neu->hist = (int *) malloc(sizeof(int) * order);
        if (neu->hist == NULL) {
            ST_WARNING("Failed to malloc hist.");
            goto ERR;
        }
        for (i = 0; i < order; i++) {
            neu->hist[i] = -1;
        }

        neu->hash_w = (hash_t *) malloc(sizeof(hash_t) * order);
        if (neu->hash_w == NULL) {
            ST_WARNING("Failed to malloc hash_w.");
            goto ERR;
        }

        if (maxent->class_size > 0) {
            neu->hash_c = (hash_t *) malloc(sizeof(hash_t) * order);
            if (neu->hash_c == NULL) {
                ST_WARNING("Failed to malloc hash_c.");
                goto ERR;
            }
        }
    }

    return 0;
ERR:
    for (i = 0; i < maxent->num_thrs; i++) {
        safe_free(maxent->neurons[i].hist);
        safe_free(maxent->neurons[i].hash_w);
        safe_free(maxent->neurons[i].hash_c);
    }
    safe_free(maxent->neurons);

    return -1;
}

int maxent_reset_test(maxent_t *maxent, int tid)
{
    maxent_neuron_t *neu;

    int order;
    int i;

    ST_CHECK_PARAM(maxent == NULL || tid < 0, -1);

    neu = maxent->neurons + tid;

    order = maxent->model_opt.order;
    for (i = 0; i < order; i++) {
        neu->hist[i] = -1;
    }

    if (maxent->train_opt.param.mini_batch > 0) {
        neu->hash_hist_c_num = 0;
        neu->hash_hist_w_num = 0;
    }

    return 0;
}

int maxent_start_test(maxent_t *maxent, int word, int tid)
{
    return 0;
}

int maxent_end_test(maxent_t *maxent, int word, int tid)
{
    maxent_neuron_t *neu;

    int i;

    ST_CHECK_PARAM(maxent == NULL || tid < 0, -1);

    neu = maxent->neurons + tid;

    for (i = maxent->model_opt.order - 1; i > 0; i--) {
        neu->hist[i] = neu->hist[i - 1];
    }
    neu->hist[0] = word;

    return 0;
}

