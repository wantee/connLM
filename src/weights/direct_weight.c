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

#include <stutils/st_macro.h>
#include <stutils/st_log.h>
#include <stutils/st_utils.h>

#include "direct_weight.h"

static const int DIRECT_WT_MAGIC_NUM = 626140498 + 100;

void direct_wt_destroy(direct_wt_t *direct_wt)
{
    if (direct_wt == NULL) {
        return;
    }

    safe_free(direct_wt->hash_wt);
    direct_wt->wt_sz = 0;
}

static int direct_wt_alloc(direct_wt_t *direct_wt, hash_size_t wt_sz)
{
    if (posix_memalign((void **)&(direct_wt->hash_wt), ALIGN_SIZE,
                sizeof(real_t) * wt_sz) != 0
            || direct_wt->hash_wt == NULL) {
        ST_WARNING("Failed to malloc hash_wt.");
        goto ERR;
    }
    direct_wt->wt_sz = wt_sz;

    return 0;

ERR:
    direct_wt_destroy(direct_wt);
    return -1;
}

direct_wt_t* direct_wt_dup(direct_wt_t *d)
{
    direct_wt_t *direct_wt = NULL;

    ST_CHECK_PARAM(d == NULL, NULL);

    direct_wt = (direct_wt_t *) malloc(sizeof(direct_wt_t));
    if (direct_wt == NULL) {
        ST_WARNING("Falied to malloc direct_wt_t.");
        goto ERR;
    }
    memset(direct_wt, 0, sizeof(direct_wt_t));

    if (direct_wt_alloc(direct_wt, d->wt_sz) < 0) {
        ST_WARNING("Failed to direct_wt_alloc.");
        goto ERR;
    }
    memcpy(direct_wt->hash_wt, d->hash_wt, sizeof(real_t) * d->wt_sz);

    direct_wt->forward = d->forward;
    direct_wt->backprop = d->backprop;

    return direct_wt;

ERR:
    safe_direct_wt_destroy(direct_wt);
    return NULL;
}

int direct_wt_load_header(direct_wt_t **direct_wt, int version,
        FILE *fp, bool *binary, FILE *fo_info)
{
    char buf[MAX_NAME_LEN];
    union {
        char str[4];
        int magic_num;
    } flag;

    hash_size_t wt_sz;

    ST_CHECK_PARAM((direct_wt == NULL && fo_info == NULL) || fp == NULL
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
    } else if (DIRECT_WT_MAGIC_NUM != flag.magic_num) {
        ST_WARNING("magic num wrong.");
        return -2;
    } else {
        *binary = true;
    }

    if (direct_wt != NULL) {
        *direct_wt = NULL;
    }

    if (*binary) {
        if (fread(&wt_sz, sizeof(hash_size_t), 1, fp) != 1) {
            ST_WARNING("Failed to read wt_sz.");
            return -1;
        }
    } else {
        if (st_readline(fp, "") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }
        if (st_readline(fp, "<DIRECT_WT>") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }

        if (st_readline(fp, "Weight size: "HASH_SIZE_FMT, &wt_sz) != 1) {
            ST_WARNING("Failed to parse wt_sz.");
            goto ERR;
        }
    }

    if (direct_wt != NULL) {
        *direct_wt = (direct_wt_t *)malloc(sizeof(direct_wt_t));
        if (*direct_wt == NULL) {
            ST_WARNING("Failed to malloc direct_wt_t");
            goto ERR;
        }
        memset(*direct_wt, 0, sizeof(direct_wt_t));

        (*direct_wt)->wt_sz = wt_sz;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<DIRECT_WT>\n");
        fprintf(fo_info, "Weight size: %s\n",
                st_ll2str(buf, MAX_NAME_LEN, (long long)wt_sz, false));
    }

    return 0;

ERR:
    if (direct_wt != NULL) {
        safe_direct_wt_destroy(*direct_wt);
    }
    return -1;
}

int direct_wt_load_body(direct_wt_t *direct_wt, int version,
        FILE *fp, bool binary)
{
    int n;
    size_t i;

    ST_CHECK_PARAM(direct_wt == NULL || fp == NULL, -1);

    if (version < 3) {
        ST_WARNING("Too old version of connlm file");
        return -1;
    }

    if (direct_wt_alloc(direct_wt, direct_wt->wt_sz) < 0) {
        ST_WARNING("Failed to direct_wt_alloc.");
        return -1;
    }

    if (binary) {
        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read magic num.");
            goto ERR;
        }

        if (n != -DIRECT_WT_MAGIC_NUM) {
            ST_WARNING("Magic num error.");
            goto ERR;
        }

        if (fread(direct_wt->hash_wt, sizeof(real_t),
                direct_wt->wt_sz, fp) != direct_wt->wt_sz) {
            ST_WARNING("Failed to read hash_wt.");
            goto ERR;
        }
    } else {
        if (st_readline(fp, "<DIRECT_WT-DATA>") != 0) {
            ST_WARNING("body flag error.");
            goto ERR;
        }

        for (i = 0; i < direct_wt->wt_sz; i++) {
            if (st_readline(fp, REAL_FMT, direct_wt->hash_wt + i) != 1) {
                ST_WARNING("Failed to parse hash_wt[%zu].", i);
                goto ERR;
            }
        }
    }

    return 0;
ERR:

    return -1;
}

int direct_wt_save_header(direct_wt_t *direct_wt, FILE *fp, bool binary)
{
    ST_CHECK_PARAM(fp == NULL, -1);

    if (binary) {
        if (fwrite(&DIRECT_WT_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        if (fwrite(&direct_wt->wt_sz, sizeof(hash_size_t), 1, fp) != 1) {
            ST_WARNING("Failed to read wt_sz.");
            return -1;
        }
    } else {
        if (fprintf(fp, "    \n<DIRECT_WT>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }

        if (fprintf(fp, "Weight size: "HASH_SIZE_FMT"\n",
                    direct_wt->wt_sz) < 0) {
            ST_WARNING("Failed to fprintf wt_sz.");
            return -1;
        }
    }

    return 0;
}

int direct_wt_save_body(direct_wt_t *direct_wt, FILE *fp, bool binary)
{
    int n;
    size_t i;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (binary) {
        n = -DIRECT_WT_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        if (fwrite(direct_wt->hash_wt, sizeof(real_t),
                direct_wt->wt_sz, fp) != direct_wt->wt_sz) {
            ST_WARNING("Failed to write hash_wt.");
            return -1;
        }
    } else {
        if (fprintf(fp, "<DIRECT_WT-DATA>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }

        for (i = 0; i < direct_wt->wt_sz; i++) {
            if (fprintf(fp, REAL_FMT"\n", direct_wt->hash_wt[i]) < 0) {
                ST_WARNING("Failed to fprintf hash_wt.");
                return -1;
            }
        }
    }

    return 0;
}

direct_wt_t* direct_wt_init(hash_size_t wt_sz)
{
    direct_wt_t *direct_wt = NULL;
    size_t i;

    ST_CHECK_PARAM(wt_sz <= 0, NULL);

    direct_wt = (direct_wt_t *) malloc(sizeof(direct_wt_t));
    if (direct_wt == NULL) {
        ST_WARNING("Falied to malloc direct_wt_t.");
        goto ERR;
    }
    memset(direct_wt, 0, sizeof(direct_wt_t));

    if (direct_wt_alloc(direct_wt, wt_sz) < 0) {
        ST_WARNING("Failed to direct_wt_alloc.");
        goto ERR;
    }

    for (i = 0; i < wt_sz; i++) {
        direct_wt->hash_wt[i] = 0.0;
    }

    return direct_wt;

ERR:
    safe_direct_wt_destroy(direct_wt);
    return NULL;
}
