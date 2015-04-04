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

int maxent_load_model_opt(maxent_opt_t *maxent_opt,
        st_opt_t *opt, const char *sec_name)
{
    float f;
    int d;

    ST_CHECK_PARAM(maxent_opt == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_FLOAT(opt, sec_name, "SCALE", f, 1.0, 
            "Scale of MaxEnt model output");
    maxent_opt->scale = (real_t)f;

    if (maxent_opt->scale <= 0) {
        return 0;
    }

    ST_OPT_SEC_GET_INT(opt, sec_name, "HASH_SIZE", d, 2,
            "Size of MaxEnt hash(in millions)");
    maxent_opt->size = d * 1000000;
    ST_OPT_SEC_GET_INT(opt, sec_name, "ORDER",
            maxent_opt->order, 3,
            "Order of MaxEnt");

    return 0;

ST_OPT_ERR:
    return -1;
}

int maxent_load_train_opt(maxent_opt_t *maxent_opt,
        st_opt_t *opt, const char *sec_name, nn_param_t *param)
{
    ST_CHECK_PARAM(maxent_opt == NULL || opt == NULL, -1);

    if (nn_param_load(&maxent_opt->param, opt, sec_name, param) < 0) {
        ST_WARNING("Failed to nn_param_load.");
        goto ST_OPT_ERR;
    }

    return 0;

ST_OPT_ERR:
    return -1;
}

int maxent_init(maxent_t **pmaxent, maxent_opt_t *maxent_opt)
{
    maxent_t *maxent = NULL;
    int i;

    ST_CHECK_PARAM(pmaxent == NULL || maxent_opt == NULL, -1);

    *pmaxent = NULL;

    if (maxent_opt->scale <= 0) {
        return 0;
    }

    maxent = (maxent_t *) malloc(sizeof(maxent_t));
    if (maxent == NULL) {
        ST_WARNING("Falied to malloc maxent_t.");
        goto ERR;
    }
    memset(maxent, 0, sizeof(maxent_t));

    maxent->maxent_opt = *maxent_opt;

    maxent->wt = (real_t *) malloc(sizeof(real_t)*maxent_opt->size);
    if (maxent->wt == NULL) {
        ST_WARNING("Failed to malloc wt.");
        goto ERR;
    }

    for (i = 0; i < maxent_opt->size; i++) {
        maxent->wt[i] = 0;
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

    safe_free(maxent->wt);
    safe_free(maxent->hist);
    safe_free(maxent->hash);
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
    maxent->maxent_opt = m->maxent_opt;

    maxent->wt = (real_t *) malloc(sizeof(real_t)*m->maxent_opt.size);
    if (maxent->wt == NULL) {
        ST_WARNING("Failed to malloc wt.");
        goto ERR;
    }
    memcpy(maxent->wt, m->wt, sizeof(real_t)*m->maxent_opt.size);

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
    long long size;
    int order;

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

        if (fread(&size, sizeof(long long), 1, fp) != 1) {
            ST_WARNING("Failed to read size");
            goto ERR;
        }

        if (fread(&order, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read order");
            goto ERR;
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

        if (st_readline(fp, "Size: %lld", &size) != 1) {
            ST_WARNING("Failed to read size");
            goto ERR;
        }

        if (st_readline(fp, "Order: %d", &order) != 1) {
            ST_WARNING("Failed to read order.");
            goto ERR;
        }
    }

    if (maxent != NULL) {
        *maxent = (maxent_t *)malloc(sizeof(maxent_t));
        if (*maxent == NULL) {
            ST_WARNING("Failed to malloc maxent_t");
            goto ERR;
        }
        memset(*maxent, 0, sizeof(maxent_t));

        (*maxent)->maxent_opt.scale = scale;
        (*maxent)->maxent_opt.size = size;
        (*maxent)->maxent_opt.order = order;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<MAXENT>: %g\n", scale);
        fprintf(fo_info, "Size: %lld\n", size);
        fprintf(fo_info, "Order: %d\n", order);
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

    sz = maxent->maxent_opt.size;
    maxent->wt = (real_t *) malloc(sizeof(real_t) * sz);
    if (maxent->wt == NULL) {
        ST_WARNING("Failed to malloc wt.");
        goto ERR;
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

        sz = maxent->maxent_opt.size;
        if (fread(maxent->wt, sizeof(real_t), sz, fp) != sz) {
            ST_WARNING("Failed to read weight.");
            goto ERR;
        }
    } else {
        if (st_readline(fp, "<MAXENT-DATA>") != 0) {
            ST_WARNING("body flag error.");
            goto ERR;
        }

        sz = maxent->maxent_opt.size;
        for (i = 0; i < sz; i++) {
            if (st_readline(fp, REAL_FMT, maxent->wt + i) != 1) {
                ST_WARNING("Failed to read weight.");
                goto ERR;
            }
        }
    }

    return 0;

ERR:
    safe_free(maxent->wt);
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

        if (fwrite(&maxent->maxent_opt.scale, sizeof(real_t), 1, fp) != 1) {
            ST_WARNING("Failed to write scale.");
            return -1;
        }
        if (fwrite(&maxent->maxent_opt.size, sizeof(long long),
                    1, fp) != 1) {
            ST_WARNING("Failed to write size.");
            return -1;
        }

        if (fwrite(&maxent->maxent_opt.order, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write order.");
            return -1;
        }
    } else {
        fprintf(fp, "    \n<MAXENT>\n");

        if (maxent == NULL) {
            fprintf(fp, "Scale: 0\n");

            return 0;
        } 

        fprintf(fp, "Scale: %g\n", maxent->maxent_opt.scale);
        fprintf(fp, "Size: %lld\n", maxent->maxent_opt.size);
        fprintf(fp, "Order: %d\n", maxent->maxent_opt.order);
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

        sz = maxent->maxent_opt.size;
        if (fwrite(maxent->wt, sizeof(real_t), sz, fp) != sz) {
            ST_WARNING("Failed to write weight.");
            return -1;
        }
    } else {
        fprintf(fp, "<MAXENT-DATA>\n");

        sz = maxent->maxent_opt.size;
        for (i = 0; i < sz; i++) {
            fprintf(fp, REAL_FMT "\n", maxent->wt[i]);
        }
    }

    return 0;
}

int maxent_setup_train(maxent_t *maxent, maxent_opt_t *maxent_opt)
{
    ST_CHECK_PARAM(maxent == NULL || maxent_opt == NULL, -1);

    if (maxent_opt->scale <= 0) {
        return 0;
    }

    maxent->hist = (int *) malloc(sizeof(int) * maxent_opt->order);
    if (maxent->hist == NULL) {
        ST_WARNING("Failed to malloc hist.");
        goto ERR;
    }

    maxent->hash = (hash_t *) malloc(sizeof(hash_t) * maxent_opt->order);
    if (maxent->hash == NULL) {
        ST_WARNING("Failed to malloc hash.");
        goto ERR;
    }

    return 0;
ERR:
    safe_free(maxent->hist);
    safe_free(maxent->hash);
    return -1;
}

int maxent_forward(maxent_t *maxent, int word)
{
    ST_CHECK_PARAM(maxent == NULL || word < 0, -1);

    return 0;
}

