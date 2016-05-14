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

#include "embedding_weight.h"

static const int EMB_WT_MAGIC_NUM = 626140498 + 50;

void emb_wt_destroy(emb_wt_t *emb_wt)
{
    if (emb_wt == NULL) {
        return;
    }
}

emb_wt_t* emb_wt_dup(emb_wt_t *e)
{
    emb_wt_t *emb_wt = NULL;

    ST_CHECK_PARAM(e == NULL, NULL);

    emb_wt = (emb_wt_t *) malloc(sizeof(emb_wt_t));
    if (emb_wt == NULL) {
        ST_WARNING("Falied to malloc emb_wt_t.");
        goto ERR;
    }
    memset(emb_wt, 0, sizeof(emb_wt_t));

    emb_wt->forward = e->forward;
    emb_wt->backprop = e->backprop;

    return emb_wt;

ERR:
    safe_emb_wt_destroy(emb_wt);
    return NULL;
}

int emb_wt_load_header(emb_wt_t **emb_wt, int version,
        FILE *fp, bool *binary, FILE *fo_info)
{
    union {
        char str[4];
        int magic_num;
    } flag;

    ST_CHECK_PARAM((emb_wt == NULL && fo_info == NULL) || fp == NULL
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
    } else if (EMB_WT_MAGIC_NUM != flag.magic_num) {
        ST_WARNING("magic num wrong.");
        return -2;
    } else {
        *binary = true;
    }

    if (emb_wt != NULL) {
        *emb_wt = NULL;
    }

    if (*binary) {
    } else {
        if (st_readline(fp, "") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }
        if (st_readline(fp, "<EMB_WT>") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }
    }

    if (emb_wt != NULL) {
        *emb_wt = (emb_wt_t *)malloc(sizeof(emb_wt_t));
        if (*emb_wt == NULL) {
            ST_WARNING("Failed to malloc emb_wt_t");
            goto ERR;
        }
        memset(*emb_wt, 0, sizeof(emb_wt_t));
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<EMB_WT>\n");
    }

    return 0;

ERR:
    if (emb_wt != NULL) {
        safe_emb_wt_destroy(*emb_wt);
    }
    return -1;
}

int emb_wt_load_body(emb_wt_t *emb_wt, int version, FILE *fp, bool binary)
{
    int n;

    ST_CHECK_PARAM(emb_wt == NULL || fp == NULL, -1);

    if (version < 3) {
        ST_WARNING("Too old version of connlm file");
        return -1;
    }

    if (binary) {
        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read magic num.");
            goto ERR;
        }

        if (n != -EMB_WT_MAGIC_NUM) {
            ST_WARNING("Magic num error.[%d]", n);
            goto ERR;
        }
    } else {
        if (st_readline(fp, "<EMB_WT-DATA>") != 0) {
            ST_WARNING("body flag error.");
            goto ERR;
        }
    }

    return 0;
ERR:

    return -1;
}

int emb_wt_save_header(emb_wt_t *emb_wt, FILE *fp, bool binary)
{
    ST_CHECK_PARAM(fp == NULL, -1);

    if (binary) {
        if (fwrite(&EMB_WT_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }
    } else {
        if (fprintf(fp, "    \n<EMB_WT>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }
    }

    return 0;
}

int emb_wt_save_body(emb_wt_t *emb_wt, FILE *fp, bool binary)
{
    int n;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (binary) {
        n = -EMB_WT_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }
    } else {
        if (fprintf(fp, "<EMB_WT-DATA>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }
    }

    return 0;
}
