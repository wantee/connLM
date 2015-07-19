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

#include "ffnn.h"

static const int FFNN_MAGIC_NUM = 626140498 + 5;

int ffnn_load_model_opt(ffnn_model_opt_t *model_opt, st_opt_t *opt,
        const char *sec_name)
{
    double d;

    ST_CHECK_PARAM(model_opt == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "SCALE", d, 1.0,
            "Scale of FFNN model output");
    model_opt->scale = (real_t)d;

    if (model_opt->scale <= 0) {
        return 0;
    }

    return 0;

ST_OPT_ERR:
    return -1;
}

int ffnn_load_train_opt(ffnn_train_opt_t *train_opt, st_opt_t *opt,
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

int ffnn_init(ffnn_t **pffnn, ffnn_model_opt_t *model_opt, output_t *output)
{
    ffnn_t *ffnn = NULL;

    ST_CHECK_PARAM(pffnn == NULL || model_opt == NULL, -1);

    *pffnn = NULL;

    if (model_opt->scale <= 0) {
        return 0;
    }

    ffnn = (ffnn_t *) malloc(sizeof(ffnn_t));
    if (ffnn == NULL) {
        ST_WARNING("Falied to malloc ffnn_t.");
        goto ERR;
    }
    memset(ffnn, 0, sizeof(ffnn_t));

    ffnn->model_opt = *model_opt;
    ffnn->output = output;


    *pffnn = ffnn;

    return 0;

ERR:
    safe_ffnn_destroy(ffnn);
    return -1;
}

void ffnn_destroy(ffnn_t *ffnn)
{
    if (ffnn == NULL) {
        return;
    }
}

ffnn_t* ffnn_dup(ffnn_t *f)
{
    ffnn_t *ffnn = NULL;

    ST_CHECK_PARAM(f == NULL, NULL);

    ffnn = (ffnn_t *) malloc(sizeof(ffnn_t));
    if (ffnn == NULL) {
        ST_WARNING("Falied to malloc ffnn_t.");
        goto ERR;
    }

    *ffnn = *f;

    return ffnn;

ERR:
    safe_ffnn_destroy(ffnn);
    return NULL;
}

int ffnn_load_header(ffnn_t **ffnn, FILE *fp, bool *binary, FILE *fo_info)
{
    union {
        char str[4];
        int magic_num;
    } flag;

    real_t scale;

    ST_CHECK_PARAM((ffnn == NULL && fo_info == NULL) || fp == NULL
            || binary == NULL, -1);

    if (fread(&flag.magic_num, sizeof(int), 1, fp) != 1) {
        ST_WARNING("Failed to load magic num.");
        return -1;
    }

    if (strncmp(flag.str, "    ", 4) == 0) {
        *binary = false;
    } else if (FFNN_MAGIC_NUM != flag.magic_num) {
        ST_WARNING("magic num wrong.");
        return -2;
    } else {
        *binary = true;
    }

    if (ffnn != NULL) {
        *ffnn = NULL;
    }

    if (*binary) {
        if (fread(&scale, sizeof(real_t), 1, fp) != 1) {
            ST_WARNING("Failed to read scale.");
            goto ERR;
        }

        if (scale <= 0) {
            if (fo_info != NULL) {
                fprintf(fo_info, "\n<FFNN>: None\n");
            }
            return 0;
        }
    } else {
        if (st_readline(fp, "<FFNN>") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }

        if (st_readline(fp, "Scale: " REAL_FMT, &scale) != 1) {
            ST_WARNING("Failed to parse scale.");
            goto ERR;
        }

        if (scale <= 0) {
            if (fo_info != NULL) {
                fprintf(fo_info, "\n<FFNN>: None\n");
            }
            return 0;
        }
    }

    if (ffnn != NULL) {
        *ffnn = (ffnn_t *)malloc(sizeof(ffnn_t));
        if (*ffnn == NULL) {
            ST_WARNING("Failed to malloc ffnn_t");
            goto ERR;
        }
        memset(*ffnn, 0, sizeof(ffnn_t));

        (*ffnn)->model_opt.scale = scale;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<FFNN>: %g\n", scale);
    }

    return 0;

ERR:
    safe_ffnn_destroy(*ffnn);
    return -1;
}

int ffnn_load_body(ffnn_t *ffnn, FILE *fp, bool binary)
{
    int n;

    ST_CHECK_PARAM(ffnn == NULL || fp == NULL, -1);

    if (binary) {
        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read magic num.");
            goto ERR;
        }

        if (n != -FFNN_MAGIC_NUM) {
            ST_WARNING("Magic num error.");
            goto ERR;
        }

    } else {
        if (st_readline(fp, "<FFNN-DATA>") != 0) {
            ST_WARNING("body flag error.");
            goto ERR;
        }

    }

    return 0;

ERR:
    return -1;
}

int ffnn_save_header(ffnn_t *ffnn, FILE *fp, bool binary)
{
    real_t scale;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (binary) { 
        if (fwrite(&FFNN_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        if (ffnn == NULL) {
            scale = 0;
            if (fwrite(&scale, sizeof(real_t), 1, fp) != 1) {
                ST_WARNING("Failed to write scale.");
                return -1;
            }
            return 0;
        }

        if (fwrite(&ffnn->model_opt.scale, sizeof(real_t), 1, fp) != 1) {
            ST_WARNING("Failed to write scale.");
            return -1;
        }
    } else {
        fprintf(fp, "    \n<FFNN>\n");

        if (ffnn == NULL) {
            fprintf(fp, "Scale: 0\n");
            return 0;
        } 
            
        fprintf(fp, "Scale: %g\n", ffnn->model_opt.scale);
    }

    return 0;
}

int ffnn_save_body(ffnn_t *ffnn, FILE *fp, bool binary)
{
    int n;

    ST_CHECK_PARAM(ffnn == NULL || fp == NULL, -1);

    if (binary) {
        n = -FFNN_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }
    } else {
        fprintf(fp, "<FFNN-DATA>\n");
    }

    return 0;
}

int ffnn_forward_pre_layer(ffnn_t *ffnn, int tid)
{
    ST_CHECK_PARAM(ffnn == NULL || tid < 0, -1);

    return 0;
}

int ffnn_forward_last_layer(ffnn_t *ffnn, int cls, int tid)
{
    ST_CHECK_PARAM(ffnn == NULL || tid < 0, -1);

    return 0;
}

int ffnn_backprop(ffnn_t *ffnn, int word, int tid)
{
    ST_CHECK_PARAM(ffnn == NULL || tid < 0, -1);

    if (word < 0) {
        return 0;
    }

    return 0;
}

int ffnn_setup_train(ffnn_t *ffnn, ffnn_train_opt_t *train_opt,
        output_t *output, int num_thrs)
{
    size_t sz;

    ST_CHECK_PARAM(ffnn == NULL || train_opt == NULL
            || output == NULL || num_thrs < 0, -1);

    ffnn->train_opt = *train_opt;
    ffnn->output = output;

    ffnn->num_thrs = num_thrs;
    sz = sizeof(ffnn_neuron_t) * num_thrs;
    ffnn->neurons = (ffnn_neuron_t *)malloc(sz);
    if (ffnn->neurons == NULL) {
        ST_WARNING("Failed to malloc neurons.");
        goto ERR;
    }
    memset(ffnn->neurons, 0, sz);

    return 0;

ERR:
    safe_free(ffnn->neurons);
    return -1;
}

int ffnn_reset_train(ffnn_t *ffnn, int tid)
{
    ST_CHECK_PARAM(ffnn == NULL || tid < 0, -1);

    return 0;
}

int ffnn_start_train(ffnn_t *ffnn, int word, int tid)
{
    ST_CHECK_PARAM(ffnn == NULL || tid < 0, -1);

    return 0;
}

int ffnn_end_train(ffnn_t *ffnn, int word, int tid)
{
    ST_CHECK_PARAM(ffnn == NULL || tid < 0, -1);

    return 0;
}

int ffnn_finish_train(ffnn_t *ffnn, int tid)
{
    ST_CHECK_PARAM(ffnn == NULL || tid < 0, -1);

    return 0;
}

int ffnn_setup_test(ffnn_t *ffnn, output_t *output, int num_thrs)
{
    size_t sz;

    ST_CHECK_PARAM(ffnn == NULL || output == NULL || num_thrs < 0, -1);

    ffnn->output = output;

    ffnn->num_thrs = num_thrs;
    sz = sizeof(ffnn_neuron_t) * num_thrs;
    ffnn->neurons = (ffnn_neuron_t *)malloc(sz);
    if (ffnn->neurons == NULL) {
        ST_WARNING("Failed to malloc neurons.");
        goto ERR;
    }
    memset(ffnn->neurons, 0, sz);

    return 0;

ERR:
    safe_free(ffnn->neurons);
    return -1;
}

int ffnn_reset_test(ffnn_t *ffnn, int tid)
{
    ST_CHECK_PARAM(ffnn == NULL || tid < 0, -1);

    return 0;
}

int ffnn_start_test(ffnn_t *ffnn, int word, int tid)
{
    ST_CHECK_PARAM(ffnn == NULL || tid < 0, -1);

    return 0;
}

int ffnn_end_test(ffnn_t *ffnn, int word, int tid)
{
    ST_CHECK_PARAM(ffnn == NULL || tid < 0, -1);

    return 0;
}

int ffnn_setup_gen(ffnn_t *ffnn, output_t *output)
{
    return ffnn_setup_test(ffnn, output, 1);
}

int ffnn_reset_gen(ffnn_t *ffnn)
{
    return ffnn_reset_test(ffnn, 0);
}

int ffnn_end_gen(ffnn_t *ffnn, int word)
{
    return ffnn_end_test(ffnn, word, 0);
}

