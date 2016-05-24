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

#include <string.h>

#include <stutils/st_log.h>
#include <stutils/st_utils.h>

#include "weight.h"

static const int WT_MAGIC_NUM = 626140498 + 90;

void wt_destroy(weight_t *wt)
{
    if (wt == NULL) {
        return;
    }

    wt->name[0] = '\0';
    wt->id = -1;
}

weight_t* wt_parse_topo(const char *line)
{
    weight_t *wt = NULL;

    ST_CHECK_PARAM(line == NULL, NULL);

    return wt;
}

weight_t* wt_dup(weight_t *w)
{
    weight_t *wt = NULL;

    ST_CHECK_PARAM(w == NULL, NULL);

    wt = (weight_t *) malloc(sizeof(weight_t));
    if (wt == NULL) {
        ST_WARNING("Falied to malloc weight_t.");
        goto ERR;
    }
    memset(wt, 0, sizeof(weight_t));

    strncpy(wt->name, w->name, MAX_NAME_LEN);
    wt->name[MAX_NAME_LEN - 1] = '\0';
    wt->id = w->id;

    wt->forward = w->forward;
    wt->backprop = w->backprop;

    return wt;

ERR:
    safe_wt_destroy(wt);
    return NULL;
}

int wt_load_header(weight_t **wt, int version,
        FILE *fp, bool *binary, FILE *fo_info)
{
    union {
        char str[4];
        int magic_num;
    } flag;

    ST_CHECK_PARAM((wt == NULL && fo_info == NULL) || fp == NULL
            || binary == NULL, -1);

    if (version < 3) {
        ST_WARNING("Too old version of connlm file");
        return -1;
    }

    if (fread(&flag.magic_num, sizeof(int), 1, fp) != 1) {
        ST_WARNING("Failed to load magic num.");
        return -1;
    }

    if (strncmp(flag.str, "    ", 4) == 0) {
        *binary = false;
    } else if (WT_MAGIC_NUM != flag.magic_num) {
        ST_WARNING("magic num wrong.");
        return -2;
    } else {
        *binary = true;
    }

    if (wt != NULL) {
        *wt = NULL;
    }

    if (*binary) {
    } else {
        if (st_readline(fp, "") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }
        if (st_readline(fp, "<WT>") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }
    }

    if (wt != NULL) {
        *wt = (weight_t *)malloc(sizeof(weight_t));
        if (*wt == NULL) {
            ST_WARNING("Failed to malloc weight_t");
            goto ERR;
        }
        memset(*wt, 0, sizeof(weight_t));
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<WT>\n");
    }

    return 0;

ERR:
    if (wt != NULL) {
        safe_wt_destroy(*wt);
    }
    return -1;
}

int wt_load_body(weight_t *wt, int version, FILE *fp, bool binary)
{
    int n;

    ST_CHECK_PARAM(wt == NULL || fp == NULL, -1);

    if (version < 3) {
        ST_WARNING("Too old version of connlm file");
        return -1;
    }

    if (binary) {
        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read magic num.");
            goto ERR;
        }

        if (n != -WT_MAGIC_NUM) {
            ST_WARNING("Magic num error.[%d]", n);
            goto ERR;
        }
    } else {
        if (st_readline(fp, "<WT-DATA>") != 0) {
            ST_WARNING("body flag error.");
            goto ERR;
        }
    }

    return 0;
ERR:

    return -1;
}

int wt_save_header(weight_t *wt, FILE *fp, bool binary)
{
    ST_CHECK_PARAM(fp == NULL, -1);

    if (binary) {
        if (fwrite(&WT_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }
    } else {
        if (fprintf(fp, "    \n<WT>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }
    }

    return 0;
}

int wt_save_body(weight_t *wt, FILE *fp, bool binary)
{
    int n;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (binary) {
        n = -WT_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }
    } else {
        if (fprintf(fp, "<WT-DATA>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }
    }

    return 0;
}
