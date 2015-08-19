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

#include "lbl.h"

static const int LBL_MAGIC_NUM = 626140498 + 4;

int lbl_load_model_opt(lbl_model_opt_t *model_opt, st_opt_t *opt,
        const char *sec_name)
{
    double d;

    ST_CHECK_PARAM(model_opt == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "SCALE", d, 1.0,
            "Scale of LBL model output");
    model_opt->scale = (real_t)d;

    if (model_opt->scale <= 0) {
        return 0;
    }

    return 0;

ST_OPT_ERR:
    return -1;
}

int lbl_load_train_opt(lbl_train_opt_t *train_opt, st_opt_t *opt,
        const char *sec_name, param_t *param)
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

int lbl_init(lbl_t **plbl, lbl_model_opt_t *model_opt, output_t *output)
{
    lbl_t *lbl = NULL;

    ST_CHECK_PARAM(plbl == NULL || model_opt == NULL, -1);

    *plbl = NULL;

    if (model_opt->scale <= 0) {
        return 0;
    }

    lbl = (lbl_t *) malloc(sizeof(lbl_t));
    if (lbl == NULL) {
        ST_WARNING("Falied to malloc lbl_t.");
        goto ERR;
    }
    memset(lbl, 0, sizeof(lbl_t));

    lbl->model_opt = *model_opt;
    lbl->output = output;


    *plbl = lbl;

    return 0;

ERR:
    safe_lbl_destroy(lbl);
    return -1;
}

void lbl_destroy(lbl_t *lbl)
{
    if (lbl == NULL) {
        return;
    }

    safe_free(lbl->neurons);
    lbl->num_thrs = 0;
}

lbl_t* lbl_dup(lbl_t *l)
{
    lbl_t *lbl = NULL;

    ST_CHECK_PARAM(l == NULL, NULL);

    lbl = (lbl_t *) malloc(sizeof(lbl_t));
    if (lbl == NULL) {
        ST_WARNING("Falied to malloc lbl_t.");
        goto ERR;
    }

    *lbl = *l;

    return lbl;

ERR:
    safe_lbl_destroy(lbl);
    return NULL;
}

int lbl_load_header(lbl_t **lbl, FILE *fp, bool *binary, FILE *fo_info)
{
    union {
        char str[4];
        int magic_num;
    } flag;

    real_t scale;

    ST_CHECK_PARAM((lbl == NULL && fo_info == NULL) || fp == NULL
            || binary == NULL, -1);

    if (fread(&flag.magic_num, sizeof(int), 1, fp) != 1) {
        ST_WARNING("Failed to load magic num.");
        return -1;
    }

    if (strncmp(flag.str, "    ", 4) == 0) {
        *binary = false;
    } else if (LBL_MAGIC_NUM != flag.magic_num) {
        ST_WARNING("magic num wrong.");
        return -2;
    } else {
        *binary = true;
    }

    if (lbl != NULL) {
        *lbl = NULL;
    }

    if (*binary) {
        if (fread(&scale, sizeof(real_t), 1, fp) != 1) {
            ST_WARNING("Failed to read scale.");
            goto ERR;
        }

        if (scale <= 0) {
            if (fo_info != NULL) {
                fprintf(fo_info, "\n<LBL>: None\n");
            }
            return 0;
        }
    } else {
        if (st_readline(fp, "") != 0) {
            ST_WARNING("tag error");
            goto ERR;
        }
        if (st_readline(fp, "<LBL>") != 0) {
            ST_WARNING("tag error");
            goto ERR;
        }

        if (st_readline(fp, "Scale: " REAL_FMT, &scale) != 1) {
            ST_WARNING("Failed to read scale.");
            goto ERR;
        }

        if (scale <= 0) {
            if (fo_info != NULL) {
                fprintf(fo_info, "\n<LBL>: None\n");
            }
            return 0;
        }
    }

    if (lbl != NULL) {
        *lbl = (lbl_t *)malloc(sizeof(lbl_t));
        if (*lbl == NULL) {
            ST_WARNING("Failed to malloc lbl_t");
            goto ERR;
        }
        memset(*lbl, 0, sizeof(lbl_t));

        (*lbl)->model_opt.scale = scale;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<LBL>: %g\n", scale);
    }

    return 0;

ERR:
    if (lbl != NULL) {
        safe_lbl_destroy(*lbl);
    }
    return -1;
}

int lbl_load_body(lbl_t *lbl, FILE *fp, bool binary)
{
    int n;

    ST_CHECK_PARAM(lbl == NULL || fp == NULL, -1);

    if (binary) {
        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read magic num.");
            goto ERR;
        }

        if (n != -LBL_MAGIC_NUM) {
            ST_WARNING("Magic num error.");
            goto ERR;
        }

    } else {
        if (st_readline(fp, "<LBL-DATA>") != 0) {
            ST_WARNING("body flag error.");
            goto ERR;
        }

    }

    return 0;

ERR:
    return -1;
}

int lbl_save_header(lbl_t *lbl, FILE *fp, bool binary)
{
    real_t scale;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (binary) { 
        if (fwrite(&LBL_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        if (lbl == NULL) {
            scale = 0;
            if (fwrite(&scale, sizeof(real_t), 1, fp) != 1) {
                ST_WARNING("Failed to write scale.");
                return -1;
            }
            return 0;
        }
        if (fwrite(&lbl->model_opt.scale, sizeof(real_t), 1, fp) != 1) {
            ST_WARNING("Failed to write scale.");
            return -1;
        }
    } else {
        fprintf(fp, "    \n<LBL>\n");

        if (lbl == NULL) {
            fprintf(fp, "Scale: 0\n");
            return 0;
        } 
            
        fprintf(fp, "Scale: %g\n", lbl->model_opt.scale);
    }

    return 0;
}

int lbl_save_body(lbl_t *lbl, FILE *fp, bool binary)
{
    int n;

    ST_CHECK_PARAM(lbl == NULL || fp == NULL, -1);

    if (binary) {
        n = -LBL_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }
    } else {
        fprintf(fp, "<LBL-DATA>\n");
    }

    return 0;
}

int lbl_forward_pre_layer(lbl_t *lbl, int tid)
{
    ST_CHECK_PARAM(lbl == NULL || tid < 0, -1);

    return 0;
}

int lbl_forward_last_layer(lbl_t *lbl, int cls, int tid)
{
    ST_CHECK_PARAM(lbl == NULL || tid < 0, -1);

    return 0;
}

int lbl_backprop(lbl_t *lbl, int word, int tid)
{
    ST_CHECK_PARAM(lbl == NULL || tid < 0 || word < 0, -1);

    return 0;
}

int lbl_setup_train(lbl_t *lbl, lbl_train_opt_t *train_opt,
        output_t *output, int num_thrs)
{
    size_t sz;

    ST_CHECK_PARAM(lbl == NULL || train_opt == NULL
            || output == NULL || num_thrs < 0, -1);

    lbl->train_opt = *train_opt;
    lbl->output = output;

    lbl->num_thrs = num_thrs;
    sz = sizeof(lbl_neuron_t) * num_thrs;
    lbl->neurons = (lbl_neuron_t *)malloc(sz);
    if (lbl->neurons == NULL) {
        ST_WARNING("Failed to malloc neurons.");
        goto ERR;
    }
    memset(lbl->neurons, 0, sz);

    return 0;

ERR:
    safe_free(lbl->neurons);
    lbl->num_thrs = 0;
    return -1;
}

int lbl_reset_train(lbl_t *lbl, int tid)
{
    ST_CHECK_PARAM(lbl == NULL || tid < 0, -1);

    return 0;
}

int lbl_start_train(lbl_t *lbl, int word, int tid)
{
    ST_CHECK_PARAM(lbl == NULL || tid < 0, -1);

    return 0;
}

int lbl_end_train(lbl_t *lbl, int word, int tid)
{
    ST_CHECK_PARAM(lbl == NULL || tid < 0, -1);

    return 0;
}

int lbl_finish_train(lbl_t *lbl, int tid)
{
    ST_CHECK_PARAM(lbl == NULL || tid < 0, -1);

    return 0;
}

int lbl_setup_test(lbl_t *lbl, output_t *output, int num_thrs)
{
    size_t sz;

    ST_CHECK_PARAM(lbl == NULL || output == NULL || num_thrs < 0, -1);

    lbl->output = output;

    lbl->num_thrs = num_thrs;
    sz = sizeof(lbl_neuron_t) * num_thrs;
    lbl->neurons = (lbl_neuron_t *)malloc(sz);
    if (lbl->neurons == NULL) {
        ST_WARNING("Failed to malloc neurons.");
        goto ERR;
    }
    memset(lbl->neurons, 0, sz);

    return 0;

ERR:
    safe_free(lbl->neurons);
    lbl->num_thrs = 0;
    return -1;
}

int lbl_reset_test(lbl_t *lbl, int tid)
{
    ST_CHECK_PARAM(lbl == NULL || tid < 0, -1);

    return 0;
}

int lbl_start_test(lbl_t *lbl, int word, int tid)
{
    ST_CHECK_PARAM(lbl == NULL || tid < 0, -1);

    return 0;
}

int lbl_end_test(lbl_t *lbl, int word, int tid)
{
    ST_CHECK_PARAM(lbl == NULL || tid < 0, -1);

    return 0;
}

int lbl_setup_gen(lbl_t *lbl, output_t *output)
{
    return lbl_setup_test(lbl, output, 1);
}

int lbl_reset_gen(lbl_t *lbl)
{
    return lbl_reset_test(lbl, 0);
}

int lbl_end_gen(lbl_t *lbl, int word)
{
    return lbl_end_test(lbl, word, 0);
}

