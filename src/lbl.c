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

int lbl_load_opt(lbl_opt_t *lbl_opt, st_opt_t *opt, const char *sec_name,
        nn_param_t *param)
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

    if (nn_param_load(&lbl_opt->param, opt, sec_name, param) < 0) {
        ST_WARNING("Failed to nn_param_load.");
        goto ST_OPT_ERR;
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

int lbl_load_header(lbl_t **lbl, FILE *fp, bool *binary, FILE *fo_info)
{
    char line[MAX_LINE_LEN];
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
        if (fgets(line, MAX_LINE_LEN, fp) == NULL) {
            ST_WARNING("Failed to read flag.");
            goto ERR;
        }

        if (fgets(line, MAX_LINE_LEN, fp) == NULL) {
            ST_WARNING("Failed to read flag.");
            goto ERR;
        }
        if (strncmp(line, "<LBL>", 5) != 0) {
            ST_WARNING("flag error.[%s]", line);
            goto ERR;
        }

        if (fgets(line, MAX_LINE_LEN, fp) == NULL) {
            ST_WARNING("Failed to read scale.");
            goto ERR;
        }
        if (sscanf(line, "Scale: %f\n", &scale) != 1) {
            ST_WARNING("Failed to parse scale.[%s]", line);
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

        (*lbl)->lbl_opt.scale = scale;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<LBL>: %g\n", scale);
    }

    return 0;

ERR:
    safe_lbl_destroy(*lbl);
    return -1;
}

int lbl_load_body(lbl_t *lbl, FILE *fp, bool binary)
{
    char line[MAX_LINE_LEN];
    int n;

    ST_CHECK_PARAM(lbl == NULL || fp == NULL, -1);

    if (binary) {
        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read magic num.");
            goto ERR;
        }

        if (n != 2 * LBL_MAGIC_NUM) {
            ST_WARNING("Magic num error.");
            goto ERR;
        }

    } else {
        if (fgets(line, MAX_LINE_LEN, fp) == NULL) {
            ST_WARNING("Failed to read body flag.");
            goto ERR;
        }
        if (strncmp(line, "<LBL-DATA>", 10) != 0) {
            ST_WARNING("body flag error.[%s]", line);
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
    } else {
        fprintf(fp, "    \n<LBL>\n");

        if (lbl == NULL) {
            fprintf(fp, "Scale: 0\n");
            return 0;
        } 
            
        fprintf(fp, "Scale: %g\n", lbl->lbl_opt.scale);
    }

    return 0;
}

int lbl_save_body(lbl_t *lbl, FILE *fp, bool binary)
{
    int n;

    ST_CHECK_PARAM(lbl == NULL || fp == NULL, -1);

    if (binary) {
        n = 2 * LBL_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }
    } else {
        fprintf(fp, "<LBL-DATA>\n");
    }

    return 0;
}

int lbl_train(lbl_t *lbl)
{
    ST_CHECK_PARAM(lbl == NULL, -1);

    return 0;
}

