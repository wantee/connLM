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
    float f;
    int d;

    ST_CHECK_PARAM(model_opt == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_FLOAT(opt, sec_name, "SCALE", f, 1.0, 
            "Scale of MaxEnt model output");
    model_opt->scale = (real_t)f;

    if (model_opt->scale <= 0) {
        return 0;
    }

    ST_OPT_SEC_GET_INT(opt, sec_name, "ORDER",
            model_opt->order, 3,
            "Order of MaxEnt");

    ST_OPT_SEC_GET_INT(opt, sec_name, "WORD_HASH_SIZE", d, 1,
            "Size of MaxEnt hash for words(in millions)");
    model_opt->sz_w = d * 1000000;

    ST_OPT_SEC_GET_INT(opt, sec_name, "CLASS_HASH_SIZE", d, 1,
            "Size of MaxEnt hash for classes(in millions)");
    model_opt->sz_c = d * 1000000;

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

    maxent->wt_w = (real_t *) malloc(sizeof(real_t)*model_opt->sz_w);
    if (maxent->wt_w == NULL) {
        ST_WARNING("Failed to malloc wt_w.");
        goto ERR;
    }

    for (i = 0; i < model_opt->sz_w; i++) {
        maxent->wt_w[i] = 0;
    }

    if (maxent->class_size > 0) {
        maxent->wt_c = (real_t *) malloc(sizeof(real_t)*model_opt->sz_c);
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
    if (maxent == NULL) {
        return;
    }

    safe_free(maxent->wt_w);
    safe_free(maxent->wt_c);

    safe_free(maxent->hist);
    safe_free(maxent->hash_c);
    safe_free(maxent->hash_w);
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

    maxent->wt_w = (real_t *) malloc(sizeof(real_t)*m->model_opt.sz_w);
    if (maxent->wt_w == NULL) {
        ST_WARNING("Failed to malloc wt_w.");
        goto ERR;
    }
    memcpy(maxent->wt_w, m->wt_w, sizeof(real_t)*m->model_opt.sz_w);

    if (maxent->class_size > 0) {
        maxent->wt_c = (real_t *) malloc(sizeof(real_t)*m->model_opt.sz_c);
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
        if (st_readline(fp, "    ") != 0) {
            ST_WARNING("Failed to read tag.");
            goto ERR;
        }
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
    maxent->wt_w = (real_t *) malloc(sizeof(real_t) * sz);
    if (maxent->wt_w == NULL) {
        ST_WARNING("Failed to malloc wt_w.");
        goto ERR;
    }

    if (maxent->class_size > 0) {
        sz = maxent->model_opt.sz_c;
        maxent->wt_c = (real_t *) malloc(sizeof(real_t) * sz);
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

        hash[a] = PRIMES[0] * PRIMES[1] * init_val;
        for (b = 1; b <= a; b++) {
            hash[a] += PRIMES[(a*PRIMES[b] + b) % PRIMES_SIZE]
                * (hash_t)(hist[b-1] + 1);
        }
    }
    
    return order;
}

static int maxent_forward_class(maxent_t *maxent)
{
    hash_t h;
    int order;

    int a;
    int o;

    ST_CHECK_PARAM(maxent == NULL, -1);

    order = maxent_get_hash(maxent->hash_c, (hash_t)1,
            maxent->hist, maxent->model_opt.order);
    if (order < 0) {
        ST_WARNING("Failed to maxent_get_hash.");
        return -1;
    }

    for (a = 0; a < order; a++) {
        maxent->hash_c[a] = maxent->hash_c[a] % maxent->model_opt.sz_c;
    }

    for (a = 0; a < order; a++) {
        h = maxent->hash_c[a];
        for (o = 0; o < maxent->class_size; o++) {
            maxent->output->ac_o_c[o] += maxent->model_opt.scale 
                * maxent->wt_c[h];
            h++;
            h %= maxent->model_opt.sz_c;
        }
    }

    return 0;
}

static int maxent_forward_word(maxent_t *maxent, int c, int s, int e)
{
    hash_t h;
    int order;

    int a;
    int o;

    ST_CHECK_PARAM(maxent == NULL, -1);

    order = maxent_get_hash(maxent->hash_w, (hash_t)(c + 1),
            maxent->hist, maxent->model_opt.order);
    if (order < 0) {
        ST_WARNING("Failed to maxent_get_hash.");
        return -1;
    }

    for (a = 0; a < order; a++) {
        maxent->hash_w[a] %= maxent->model_opt.sz_w;
    }

    for (a = 0; a < order; a++) {
        h = maxent->hash_w[a];
        for (o = s; o < e; o++) {
            maxent->output->ac_o_w[o] += maxent->model_opt.scale 
                * maxent->wt_w[h];
            h++;
            h %= maxent->model_opt.sz_w;
        }
    }

    return 0;
}

int maxent_forward(maxent_t *maxent, int word)
{
    int c;
    int s;
    int e;

    int i;

    ST_CHECK_PARAM(maxent == NULL, -1);

    if (word >= 0) {
        if (maxent->class_size > 0) {
            c = maxent->output->w2c[word];
            s = maxent->output->c2w_s[c];
            e = maxent->output->c2w_e[c];

            if (maxent_forward_class(maxent) < 0) {
                ST_WARNING("Failed to maxent_forward_class.");
                return -1;
            }
        } else {
            c = 0;
            s = 0;
            e = maxent->vocab_size;
        }

        if (maxent_forward_word(maxent, c, s, e) < 0) {
            ST_WARNING("Failed to maxent_forward_word.");
            return -1;
        }
    }

    for (i = maxent->model_opt.order - 1; i > 0; i--) {
        maxent->hist[i] = maxent->hist[i - 1];
    }
    maxent->hist[0] = word;

    return 0;
}

static int maxent_backprop_class(maxent_t *maxent)
{
    int order;

    int a;

    ST_CHECK_PARAM(maxent == NULL, -1);

    order = maxent_get_hash(maxent->hash_c, (hash_t)1,
            maxent->hist, maxent->model_opt.order);
    if (order < 0) {
        ST_WARNING("Failed to maxent_get_hash.");
        return -1;
    }

    for (a = 0; a < order; a++) {
        maxent->hash_c[a] %= maxent->model_opt.sz_c;
    }

    for (a = 0; a < order; a++) {
        param_update(&maxent->train_opt.param,
                maxent->wt_c,
                maxent->output->er_o_c,
                NULL,
                maxent->class_size,
                -maxent->model_opt.sz_c,
                maxent->hash_c[a]);
    }

    return 0;
}

static int maxent_backprop_word(maxent_t *maxent, int c, int s, int e)
{
    int order;

    int a;

    order = maxent_get_hash(maxent->hash_w, (hash_t)(c + 1),
                maxent->hist, maxent->model_opt.order);
    if (order < 0) {
        ST_WARNING("Failed to maxent_get_hash.");
        return -1;
    }

    for (a = 0; a < order; a++) {
        maxent->hash_w[a] %= maxent->model_opt.sz_w;
    }

    for (a = 0; a < order; a++) {
        param_update(&maxent->train_opt.param,
                maxent->wt_w,
                maxent->output->er_o_w + s, 
                NULL,
                e - s, 
                -maxent->model_opt.sz_w,
                maxent->hash_w[a]);
    }

    return 0;
}

int maxent_backprop(maxent_t *maxent, int word)
{
    int c;
    int s;
    int e;

    ST_CHECK_PARAM(maxent == NULL, -1);

    if (word < 0) {
        return 0;
    }

    if (maxent->class_size > 0) {
        c = maxent->output->w2c[word];
        s = maxent->output->c2w_s[c];
        e = maxent->output->c2w_e[c];

        if (maxent_backprop_class(maxent) < 0) {
            ST_WARNING("Failed to maxent_backprop_class.");
            return -1;
        }
    } else {
        c = 0;
        s = 0;
        e = maxent->vocab_size;
    }

    if (maxent_backprop_word(maxent, c, s, e) < 0) {
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
        output_t *output)
{
    int order;
    int i;

    ST_CHECK_PARAM(maxent == NULL || train_opt == NULL
            || output == NULL, -1);

    if (!maxent_check_output(maxent, output)) {
        ST_WARNING("Output layer not match.");
        return -1;
    }

    maxent->train_opt = *train_opt;
    maxent->output = output;

    order = maxent->model_opt.order;
    maxent->hist = (int *) malloc(sizeof(int) * order);
    if (maxent->hist == NULL) {
        ST_WARNING("Failed to malloc hist.");
        goto ERR;
    }
    for (i = 0; i < order; i++) {
        maxent->hist[i] = -1;
    }

    maxent->hash_w = (hash_t *) malloc(sizeof(hash_t) * order);
    if (maxent->hash_w == NULL) {
        ST_WARNING("Failed to malloc hash_w.");
        goto ERR;
    }

    if (maxent->class_size > 0) {
        maxent->hash_c = (hash_t *) malloc(sizeof(hash_t) * order);
        if (maxent->hash_c == NULL) {
            ST_WARNING("Failed to malloc hash_c.");
            goto ERR;
        }
    }

    return 0;
ERR:
    safe_free(maxent->hist);
    safe_free(maxent->hash_w);
    safe_free(maxent->hash_c);
    return -1;
}

int maxent_reset_train(maxent_t *maxent)
{
    int order;
    int i;

    ST_CHECK_PARAM(maxent == NULL, -1);

    order = maxent->model_opt.order;
    for (i = 0; i < order; i++) {
        maxent->hist[i] = -1;
    }

    return 0;
}

int maxent_clear_train(maxent_t *maxent, int word)
{
    return 0;
}

int maxent_setup_test(maxent_t *maxent, output_t *output)
{
    int order;
    int i;

    ST_CHECK_PARAM(maxent == NULL || output == NULL, -1);

    if (!maxent_check_output(maxent, output)) {
        ST_WARNING("Output layer not match.");
        return -1;
    }

    maxent->output = output;

    order = maxent->model_opt.order;
    maxent->hist = (int *) malloc(sizeof(int) * order);
    if (maxent->hist == NULL) {
        ST_WARNING("Failed to malloc hist.");
        goto ERR;
    }
    for (i = 0; i < order; i++) {
        maxent->hist[i] = -1;
    }

    maxent->hash_w = (hash_t *) malloc(sizeof(hash_t) * order);
    if (maxent->hash_w == NULL) {
        ST_WARNING("Failed to malloc hash_w.");
        goto ERR;
    }

    if (maxent->class_size > 0) {
        maxent->hash_c = (hash_t *) malloc(sizeof(hash_t) * order);
        if (maxent->hash_c == NULL) {
            ST_WARNING("Failed to malloc hash_c.");
            goto ERR;
        }
    }

    return 0;
ERR:
    safe_free(maxent->hist);
    safe_free(maxent->hash_w);
    safe_free(maxent->hash_c);
    return -1;
}

int maxent_reset_test(maxent_t *maxent)
{
    int order;
    int i;

    ST_CHECK_PARAM(maxent == NULL, -1);

    order = maxent->model_opt.order;
    for (i = 0; i < order; i++) {
        maxent->hist[i] = -1;
    }

    return 0;
}

int maxent_clear_test(maxent_t *maxent, int word)
{
    return 0;
}

