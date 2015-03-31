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

#include "lbl.h"

static const int LBL_MAGIC_NUM = 626140498 + 4;

int lbl_load_opt(lbl_opt_t *lbl_opt, st_opt_t *opt, const char *sec_name)
{
    float f;

    ST_CHECK_PARAM(lbl_opt == NULL || opt == NULL, -1);

    memset(lbl_opt, 0, sizeof(lbl_opt_t));

    ST_OPT_SEC_GET_FLOAT(opt, sec_name, "SCALE", f, 1.0,
            "Scale of LBL model output");
    lbl_opt->scale = (real_t)f;

    if (lbl_opt->scale <= 0) {
        return 0;
    }

    return 0;

ST_OPT_ERR:
    return -1;
}

lbl_t *lbl_create(lbl_opt_t *lbl_opt)
{
    lbl_t *lbl = NULL;

    ST_CHECK_PARAM(lbl_opt == NULL, NULL);

    lbl = (lbl_t *) malloc(sizeof(lbl_t));
    if (lbl == NULL) {
        ST_WARNING("Falied to malloc lbl_t.");
        goto ERR;
    }
    memset(lbl, 0, sizeof(lbl_t));

    return lbl;
  ERR:
    safe_lbl_destroy(lbl);
    return NULL;
}

void lbl_destroy(lbl_t *lbl)
{
    if (lbl == NULL) {
        return;
    }
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

static int lbl_load_header(lbl_t **lbl, FILE *fp, bool *binary)
{
    char str[MAX_LINE_LEN];
    long sz;
    int magic_num;
    int version;

    ST_CHECK_PARAM(lbl == NULL || fp == NULL
            || binary == NULL, -1);

    if (fread(&magic_num, sizeof(int), 1, fp) != 1) {
        ST_WARNING("NOT lbl format: Failed to load magic num.");
        return -1;
    }

    if (LBL_MAGIC_NUM != magic_num) {
        ST_WARNING("NOT lbl format, magic num wrong.");
        return -2;
    }
    fscanf(fp, "\n<LBL>\n");

    if (fread(&sz, sizeof(long), 1, fp) != 1) {
        ST_WARNING("Failed to read size.");
        return -1;
    }

    if (sz <= 0) {
        *lbl = NULL;
        return 0;
    }

    *lbl = (lbl_t *)malloc(sizeof(lbl_t));
    if (*lbl == NULL) {
        ST_WARNING("Failed to malloc lbl_t");
        goto ERR;
    }
    memset(*lbl, 0, sizeof(lbl_t));

    fscanf(fp, "Version: %d\n", &version);

    if (version > CONNLM_FILE_VERSION) {
        ST_WARNING("Too high file versoin[%d].");
        goto ERR;
    }

    fscanf(fp, "Binary: %s\n", str);
    *binary = str2bool(str);

    return 0;

ERR:
    safe_lbl_destroy(*lbl);
    return -1;
}

int lbl_load(lbl_t **lbl, FILE *fp)
{
    bool binary;

    ST_CHECK_PARAM(lbl == NULL || fp == NULL, -1);

    if (lbl_load_header(lbl, fp, &binary) < 0) {
        ST_WARNING("Failed to lbl_load_header.");
        goto ERR;
    }

    if (*lbl == NULL) {
        return 0;
    }

    if (binary) {
    } else {
    }
    return 0;

ERR:
    safe_lbl_destroy(*lbl);
    return -1;
}

static long lbl_save_header(lbl_t *lbl, FILE *fp, bool binary)
{
    long sz_pos;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (fwrite(&LBL_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
        ST_WARNING("Failed to write magic num.");
        return -1;
    }
    fprintf(fp, "\n<LBL>\n");

    if (lbl == NULL) {
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

int lbl_save(lbl_t *lbl, FILE *fp, bool binary)
{
    long sz;
    long sz_pos;
    long fpos;

    ST_CHECK_PARAM(fp == NULL, -1);

    sz_pos = lbl_save_header(lbl, fp, binary);
    if (sz_pos < 0) {
        ST_WARNING("Failed to lbl_save_header.");
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

int lbl_train(lbl_t *lbl)
{
    ST_CHECK_PARAM(lbl == NULL, -1);

    return 0;
}

