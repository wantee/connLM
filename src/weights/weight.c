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

    safe_free(wt->matrix);
    wt->row = 0;
    wt->col = 0;
}

static int wt_alloc(weight_t *wt, int row, int col)
{
    size_t sz;

    sz = row * col;
    if (posix_memalign((void **)&(wt->matrix), ALIGN_SIZE,
                sizeof(real_t) * sz) != 0
            || wt->matrix == NULL) {
        ST_WARNING("Failed to malloc matrix.");
        goto ERR;
    }
    wt->row = row;
    wt->col = col;

    return 0;

ERR:
    wt_destroy(wt);
    return -1;
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

    if (wt_alloc(wt, w->row, w->col) < 0) {
        ST_WARNING("Failed to wt_alloc.");
        goto ERR;
    }
    memcpy(wt->matrix, w->matrix, sizeof(real_t) * w->row * w->col);

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

    int row;
    int col;

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
        if (fread(&row, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read row.");
            return -1;
        }
        if (fread(&col, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read col.");
            return -1;
        }
    } else {
        if (st_readline(fp, "") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }
        if (st_readline(fp, "<WT>") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }

        if (st_readline(fp, "Row: %d", &row) != 1) {
            ST_WARNING("Failed to parse row.");
            goto ERR;
        }
        if (st_readline(fp, "Col: %d", &col) != 1) {
            ST_WARNING("Failed to parse col.");
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

        (*wt)->row = row;
        (*wt)->col = col;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<WT>\n");
        fprintf(fo_info, "Row: %d\n", row);
        fprintf(fo_info, "Col: %d\n", col);
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

    int i;

    ST_CHECK_PARAM(wt == NULL || fp == NULL, -1);

    if (version < 3) {
        ST_WARNING("Too old version of connlm file");
        return -1;
    }

    if (wt_alloc(wt, wt->row, wt->col) < 0) {
        ST_WARNING("Failed to wt_alloc.");
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

        if (fread(wt->matrix, sizeof(real_t), wt->row * wt->col, fp)
                != wt->row * wt->col) {
            ST_WARNING("Failed to read matrix.");
            goto ERR;
        }
    } else {
        if (st_readline(fp, "<WT-DATA>") != 0) {
            ST_WARNING("body flag error.");
            goto ERR;
        }
        for (i = 0; i < wt->row * wt->col; i++) {
            if (st_readline(fp, REAL_FMT, wt->matrix + i) != 1) {
                ST_WARNING("Failed to parse matrix[%d].", i);
                goto ERR;
            }
        }
    }

    return 0;
ERR:

    wt_destroy(wt);
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

        if (fwrite(&wt->row, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read row.");
            return -1;
        }
        if (fwrite(&wt->col, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read col.");
            return -1;
        }
    } else {
        if (fprintf(fp, "    \n<WT>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }

        if (fprintf(fp, "Row: %d\n", wt->row) < 0) {
            ST_WARNING("Failed to fprintf row.");
            return -1;
        }
        if (fprintf(fp, "Col: %d\n", wt->col) < 0) {
            ST_WARNING("Failed to fprintf col.");
            return -1;
        }
    }

    return 0;
}

int wt_save_body(weight_t *wt, FILE *fp, bool binary)
{
    int n;

    int i;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (binary) {
        n = -WT_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        if (fwrite(wt->matrix, sizeof(real_t), wt->row * wt->col, fp)
                != wt->row * wt->col) {
            ST_WARNING("Failed to write matrix.");
            return -1;
        }
    } else {
        if (fprintf(fp, "<WT-DATA>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }

        for (i = 0; i < wt->row * wt->col; i++) {
            if (fprintf(fp, REAL_FMT"\n", wt->matrix[i]) < 0) {
                ST_WARNING("Failed to fprintf matrix.");
                return -1;
            }
        }
    }

    return 0;
}

weight_t* wt_init(int row, int col)
{
    weight_t *wt = NULL;
    size_t i;

    ST_CHECK_PARAM(row <= 0 || col <= 0, NULL);

    wt = (weight_t *) malloc(sizeof(weight_t));
    if (wt == NULL) {
        ST_WARNING("Falied to malloc weight_t.");
        goto ERR;
    }
    memset(wt, 0, sizeof(weight_t));

    if (wt_alloc(wt, row, col) < 0) {
        ST_WARNING("Failed to wt_alloc.");
        goto ERR;
    }

    for (i = 0; i < row * col; i++) {
        wt->matrix[i] = st_random(-0.1, 0.1)
            + st_random(-0.1, 0.1) + st_random(-0.1, 0.1);
    }

    return wt;

ERR:
    safe_wt_destroy(wt);
    return NULL;
}
