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

#include "ffnn.h"

static const int FFNN_MAGIC_NUM = 626140498 + 5;

int ffnn_load_opt(ffnn_opt_t *ffnn_opt, st_opt_t *opt,
        const char *sec_name)
{
    float f;

    ST_CHECK_PARAM(ffnn_opt == NULL || opt == NULL, -1);

    memset(ffnn_opt, 0, sizeof(ffnn_opt_t));

    ST_OPT_SEC_GET_FLOAT(opt, sec_name, "SCALE", f, 1.0,
            "Scale of FFNN model output");
    ffnn_opt->scale = (real_t)f;

    if (ffnn_opt->scale <= 0) {
        return 0;
    }

    return 0;

ST_OPT_ERR:
    return -1;
}

ffnn_t *ffnn_create(ffnn_opt_t *ffnn_opt)
{
    ffnn_t *ffnn = NULL;

    ST_CHECK_PARAM(ffnn_opt == NULL, NULL);

    ffnn = (ffnn_t *) malloc(sizeof(ffnn_t));
    if (ffnn == NULL) {
        ST_WARNING("Falied to malloc ffnn_t.");
        goto ERR;
    }
    memset(ffnn, 0, sizeof(ffnn_t));

    return ffnn;
  ERR:
    safe_ffnn_destroy(ffnn);
    return NULL;
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

static int ffnn_load_header(ffnn_t **ffnn, FILE *fp, bool *binary)
{
    char str[MAX_LINE_LEN];
    long sz;
    int magic_num;
    int version;

    ST_CHECK_PARAM(ffnn == NULL || fp == NULL
            || binary == NULL, -1);

    if (fread(&magic_num, sizeof(int), 1, fp) != 1) {
        ST_WARNING("NOT ffnn format: Failed to load magic num.");
        return -1;
    }

    if (FFNN_MAGIC_NUM != magic_num) {
        ST_WARNING("NOT ffnn format, magic num wrong.");
        return -2;
    }
    fscanf(fp, "\n<FFNN>\n");

    if (fread(&sz, sizeof(long), 1, fp) != 1) {
        ST_WARNING("Failed to read size.");
        return -1;
    }

    if (sz <= 0) {
        *ffnn = NULL;
        return 0;
    }

    *ffnn = (ffnn_t *)malloc(sizeof(ffnn_t));
    if (*ffnn == NULL) {
        ST_WARNING("Failed to malloc ffnn_t");
        goto ERR;
    }
    memset(*ffnn, 0, sizeof(ffnn_t));

    fscanf(fp, "Version: %d\n", &version);

    if (version > CONNLM_FILE_VERSION) {
        ST_WARNING("Too high file versoin[%d].");
        goto ERR;
    }

    fscanf(fp, "Binary: %s\n", str);
    *binary = str2bool(str);

    return 0;

ERR:
    safe_ffnn_destroy(*ffnn);
    return -1;
}

int ffnn_load(ffnn_t **ffnn, FILE *fp)
{
    bool binary;

    ST_CHECK_PARAM(ffnn == NULL || fp == NULL, -1);

    if (ffnn_load_header(ffnn, fp, &binary) < 0) {
        ST_WARNING("Failed to ffnn_load_header.");
        goto ERR;
    }

    if (*ffnn == NULL) {
        return 0;
    }

    if (binary) {
    } else {
    }
    return 0;

ERR:
    safe_ffnn_destroy(*ffnn);
    return -1;
}

static long ffnn_save_header(ffnn_t *ffnn, FILE *fp, bool binary)
{
    long sz_pos;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (fwrite(&FFNN_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
        ST_WARNING("Failed to write magic num.");
        return -1;
    }
    fprintf(fp, "\n<FFNN>\n");

    if (ffnn == NULL) {
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

int ffnn_save(ffnn_t *ffnn, FILE *fp, bool binary)
{
    long sz;
    long sz_pos;
    long fpos;

    ST_CHECK_PARAM(fp == NULL, -1);

    sz_pos = ffnn_save_header(ffnn, fp, binary);
    if (sz_pos < 0) {
        ST_WARNING("Failed to ffnn_save_header.");
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

int ffnn_train(ffnn_t *ffnn)
{
    ST_CHECK_PARAM(ffnn == NULL, -1);

    return 0;
}

