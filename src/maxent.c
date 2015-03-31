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

#include "maxent.h"

static const int MAXENT_MAGIC_NUM = 626140498 + 3;

int maxent_load_opt(maxent_opt_t *maxent_opt,
        st_opt_t *opt, const char *sec_name)
{
    float f;
    int d;

    ST_CHECK_PARAM(maxent_opt == NULL || opt == NULL, -1);

    memset(maxent_opt, 0, sizeof(maxent_opt_t));

    ST_OPT_SEC_GET_FLOAT(opt, sec_name, "SCALE", f, 1.0, 
            "Scale of MaxEnt model output");
    maxent_opt->scale = (real_t)f;

    if (maxent_opt->scale <= 0) {
        return 0;
    }

    ST_OPT_SEC_GET_INT(opt, sec_name, "HASH_SIZE", d, 2,
            "Size of MaxEnt hash(in millions)");
    maxent_opt->direct_size = d * 1000000;
    ST_OPT_SEC_GET_INT(opt, sec_name, "ORDER",
            maxent_opt->direct_order, 3,
            "Order of MaxEnt");

    return 0;

ST_OPT_ERR:
    return -1;
}

maxent_t *maxent_create(maxent_opt_t *maxent_opt)
{
    maxent_t *maxent = NULL;

    ST_CHECK_PARAM(maxent_opt == NULL, NULL);

    maxent = (maxent_t *) malloc(sizeof(maxent_t));
    if (maxent == NULL) {
        ST_WARNING("Falied to malloc maxent_t.");
        goto ERR;
    }
    memset(maxent, 0, sizeof(maxent_t));

    return maxent;
  ERR:
    safe_maxent_destroy(maxent);
    return NULL;
}

void maxent_destroy(maxent_t *maxent)
{
    if (maxent == NULL) {
        return;
    }
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

    return maxent;

ERR:
    safe_maxent_destroy(maxent);
    return NULL;
}

static int maxent_load_header(maxent_t **maxent, FILE *fp, bool *binary)
{
    char str[MAX_LINE_LEN];
    long sz;
    int magic_num;
    int version;

    ST_CHECK_PARAM(maxent == NULL || fp == NULL
            || binary == NULL, -1);

    if (fread(&magic_num, sizeof(int), 1, fp) != 1) {
        ST_WARNING("NOT maxent format: Failed to load magic num.");
        return -1;
    }

    if (MAXENT_MAGIC_NUM != magic_num) {
        ST_WARNING("NOT maxent format, magic num wrong.");
        return -2;
    }
    fscanf(fp, "\n<MAXENT>\n");

    if (fread(&sz, sizeof(long), 1, fp) != 1) {
        ST_WARNING("Failed to read size.");
        return -1;
    }

    if (sz <= 0) {
        *maxent = NULL;
        return 0;
    }

    *maxent = (maxent_t *)malloc(sizeof(maxent_t));
    if (*maxent == NULL) {
        ST_WARNING("Failed to malloc maxent_t");
        goto ERR;
    }
    memset(*maxent, 0, sizeof(maxent_t));

    fscanf(fp, "Version: %d\n", &version);

    if (version > CONNLM_FILE_VERSION) {
        ST_WARNING("Too high file versoin[%d].");
        goto ERR;
    }

    fscanf(fp, "Binary: %s\n", str);
    *binary = str2bool(str);

    return 0;

ERR:
    safe_maxent_destroy(*maxent);
    return -1;
}

int maxent_load(maxent_t **maxent, FILE *fp)
{
    bool binary;

    ST_CHECK_PARAM(maxent == NULL || fp == NULL, -1);

    if (maxent_load_header(maxent, fp, &binary) < 0) {
        ST_WARNING("Failed to maxent_load_header.");
        goto ERR;
    }

    if (*maxent == NULL) {
        return 0;
    }

    if (binary) {
    } else {
    }
    return 0;

ERR:
    safe_maxent_destroy(*maxent);
    return -1;
}

static long maxent_save_header(maxent_t *maxent, FILE *fp, bool binary)
{
    long sz_pos;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (fwrite(&MAXENT_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
        ST_WARNING("Failed to write magic num.");
        return -1;
    }
    fprintf(fp, "\n<MAXENT>\n");

    if (maxent == NULL) {
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

int maxent_save(maxent_t *maxent, FILE *fp, bool binary)
{
    long sz;
    long sz_pos;
    long fpos;

    ST_CHECK_PARAM(fp == NULL, -1);

    sz_pos = maxent_save_header(maxent, fp, binary);
    if (sz_pos < 0) {
        ST_WARNING("Failed to maxent_save_header.");
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

int maxent_train(maxent_t *maxent)
{
    ST_CHECK_PARAM(maxent == NULL, -1);

    return 0;
}

