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
#include <stutils/st_io.h>
#include <stutils/st_string.h>
#include <stutils/st_rand.h>
#include <stutils/st_mem.h>

#include "weight.h"

static const int WT_MAGIC_NUM = 626140498 + 90;

static const char *init_str[] = {
    "Undefined",
    "Constant",
    "Uniform",
    "Norm",
    "Trunc_Norm",
};

static const char* init2str(wt_init_type_t m)
{
    return init_str[m];
}

static wt_init_type_t str2init(const char *str)
{
    int i;

    for (i = 0; i < sizeof(init_str) / sizeof(init_str[0]); i++) {
        if (strcasecmp(init_str[i], str) == 0) {
            return (wt_init_type_t)i;
        }
    }

    return WT_INIT_UNKNOWN;
}

void wt_destroy(weight_t *wt)
{
    if (wt == NULL) {
        return;
    }

    safe_st_aligned_free(wt->mat);
    wt->row = 0;
    wt->col = 0;
}

static int wt_alloc(weight_t *wt, int row, int col)
{
    size_t sz;

    ST_CHECK_PARAM(wt == NULL || row < 0, -1);

    sz = row;
    if (col > 0) {
        sz *= col;
    }
    sz *= sizeof(real_t);

    wt->mat = st_aligned_malloc(sz, ALIGN_SIZE);
    if (wt->mat == NULL) {
        ST_WARNING("Failed to st_aligned_malloc mat.");
        goto ERR;
    }
    wt->row = row;
    wt->col = col;

    return 0;

ERR:
    safe_st_aligned_free(wt->mat);
    wt->row = 0;
    wt->col = 0;
    return -1;
}

weight_t* wt_dup(weight_t *src)
{
    weight_t *dst = NULL;
    size_t sz;

    ST_CHECK_PARAM(src == NULL, NULL);

    dst = (weight_t *)malloc(sizeof(weight_t));
    if (dst == NULL) {
        ST_WARNING("Failed to malloc weight_t.");
        return NULL;
    }
    memset(dst, 0, sizeof(weight_t));

    if (wt_alloc(dst, src->row, src->col) < 0) {
        ST_WARNING("Failed to wt_alloc.");
        goto ERR;
    }
    sz = src->row;
    if (src->col > 0) {
        sz *= src->col;
    }
    sz *= sizeof(real_t);

    memcpy(dst->mat, src->mat, sz);

    dst->init_type = src->init_type;
    dst->init_param = src->init_param;

    return dst;

ERR:
    safe_wt_destroy(dst);
    return NULL;
}

int wt_parse_topo(weight_t *wt, char *line, size_t line_len)
{
    char keyvalue[2*MAX_LINE_LEN];
    char token[MAX_LINE_LEN];
    char untouch_topo[MAX_LINE_LEN];

    const char *p;

    ST_CHECK_PARAM(wt == NULL || line == NULL, -1);

    wt->init_type = WT_INIT_UNKNOWN;
    wt->init_param = 0;

    p = line;
    untouch_topo[0] = '\0';
    while (p != NULL) {
        p = get_next_token(p, token);
        if (token[0] == '\0') {
            continue;
        }

        if (split_line(token, keyvalue, 2, MAX_LINE_LEN, "=") != 2) {
            ST_WARNING("Failed to split key/value. [%s]", token);
            goto ERR;
        }

        if (strcasecmp("init", keyvalue) == 0) {
            if (wt->init_type != WT_INIT_UNKNOWN) {
                ST_WARNING("Duplicated init tag");
                goto ERR;
            }
            wt->init_type = str2init(keyvalue + MAX_LINE_LEN);
            if (wt->init_type == WT_INIT_UNKNOWN) {
                ST_WARNING("Unknown init type.");
                goto ERR;
            }
        } else if (strcasecmp("init_param", keyvalue) == 0) {
            if (wt->init_param != 0) {
                ST_WARNING("Duplicated init_param tag");
                goto ERR;
            }
            wt->init_param = atof(keyvalue + MAX_LINE_LEN);
            if (wt->init_param <= 0) {
                ST_WARNING("Error init_param[%g]", wt->init_param);
                goto ERR;
            }
        } else {
            strncpy(untouch_topo + strlen(untouch_topo), token,
                    MAX_LINE_LEN - strlen(untouch_topo));
            if (strlen(untouch_topo) < MAX_LINE_LEN) {
                untouch_topo[strlen(untouch_topo)] = ' ';
            }
            untouch_topo[MAX_LINE_LEN - 1] = '\0';
        }
    }

    if (wt->init_type == WT_INIT_UNKNOWN) {
        wt->init_type = WT_INIT_UNIFORM;
    }

    if (wt->init_type == WT_INIT_CONST) {
        if (wt->init_param != 0) {
            ST_WARNING("WT_INIT_CONST should be 0.0.");
            wt->init_param = 0;
        }
    } else if (wt->init_type == WT_INIT_UNIFORM) {
        if (wt->init_param == 0) {
            wt->init_param = 0.1;
        }
    } else if (wt->init_type == WT_INIT_NORM) {
        if (wt->init_param == 0) {
            wt->init_param = 0.1;
        }
    } else if (wt->init_type == WT_INIT_TRUNC_NORM) {
        if (wt->init_param == 0) {
            wt->init_param = 0.1;
        }
    }

    strncpy(line, untouch_topo, line_len);
    line[line_len - 1] = '\0';

    return 0;

ERR:
    return -1;
}

int wt_load_header(weight_t **wt, int version,
        FILE *fp, bool *binary, FILE *fo_info)
{
    char sym[MAX_LINE_LEN];
    union {
        char str[4];
        int magic_num;
    } flag;

    int row;
    int col;
    int i;
    real_t init_param;

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
        if (fread(&i, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read init.");
            goto ERR;
        }
        if (fread(&init_param, sizeof(real_t), 1, fp) != 1) {
            ST_WARNING("Failed to read init_param.");
            goto ERR;
        }
    } else {
        if (st_readline(fp, "") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }
        if (st_readline(fp, "<WEIGHT>") != 0) {
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
        if (st_readline(fp, "Init: %"xSTR(MAX_LINE_LEN)"s", sym) != 1) {
            ST_WARNING("Failed to parse init.");
            goto ERR;
        }
        sym[MAX_LINE_LEN - 1] = '\0';
        i = (int)str2init(sym);
        if (i == (int)WT_INIT_UNKNOWN) {
            ST_WARNING("Unknown init[%s]", sym);
            goto ERR;
        }

        if (st_readline(fp, "Init param: "REAL_FMT, &init_param) != 1) {
            ST_WARNING("Failed to parse init param.");
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
        (*wt)->init_type = (wt_init_type_t)i;
        (*wt)->init_param = init_param;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<WEIGHT>\n");
        fprintf(fo_info, "Row: %d\n", row);
        fprintf(fo_info, "Col: %d\n", col);
        fprintf(fo_info, "Init: %s\n", init2str((wt_init_type_t)i));
        fprintf(fo_info, "Init param: %g\n", init_param);
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
    char name[MAX_NAME_LEN];
    int n;
    int i, j, col;
    int sz;

    char *line = NULL;
    size_t line_sz = 0;
    bool err;
    const char *p;
    char token[MAX_NAME_LEN];

    ST_CHECK_PARAM(wt == NULL || fp == NULL, -1);

    if (version < 5) {
        ST_WARNING("Too old version of connlm file");
        return -1;
    }

    if (wt->row <= 0) {
        return 0;
    }

    if (wt_alloc(wt, wt->row, wt->col) < 0) {
        ST_WARNING("Failed to wt_alloc.");
        return -1;
    }

    sz = wt->row;
    if (wt->col > 0) {
        sz *= wt->col;
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

        if (fread(wt->mat, sizeof(real_t), sz, fp) != sz) {
            ST_WARNING("Failed to read mat.");
            goto ERR;
        }
    } else {
        if (st_readline(fp, "<WEIGHT-DATA>") != 0) {
            ST_WARNING("body flag error.");
            goto ERR;
        }
        if (st_readline(fp, "%"xSTR(MAX_NAME_LEN)"s", name) != 1) {
            ST_WARNING("name error.");
            goto ERR;
        }

        if (wt->col <= 0) {
            col = 1;
        } else {
            col = wt->col;
        }
        for (i = 0; i < wt->row; i++) {
            if (st_fgets(&line, &line_sz, fp, &err) == NULL || err) {
                ST_WARNING("Failed to parse mat[%d].", i);
            }
            p = line;
            for (j = 0; j < col; j++) {
                p = get_next_token(p, token);
                if (p == NULL
                        || sscanf(token, REAL_FMT, wt->mat+i*col+j) != 1) {
                    ST_WARNING("Failed to parse mat[%d, %d].", i, j);
                    goto ERR;
                }
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
    int n;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (binary) {
        if (fwrite(&WT_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        if (fwrite(&wt->row, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write row.");
            return -1;
        }
        if (fwrite(&wt->col, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write col.");
            return -1;
        }
        n = (int)wt->init_type;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write init_type.");
            return -1;
        }
        if (fwrite(&wt->init_param, sizeof(real_t), 1, fp) != 1) {
            ST_WARNING("Failed to write init_param.");
            return -1;
        }
    } else {
        if (fprintf(fp, "    \n<WEIGHT>\n") < 0) {
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
        if (fprintf(fp, "Init: %s\n", init2str(wt->init_type)) < 0) {
            ST_WARNING("Failed to fprintf init type.");
            return -1;
        }
        if (fprintf(fp, "Init param: %f\n", wt->init_param) < 0) {
            ST_WARNING("Failed to fprintf init param.");
            return -1;
        }
    }

    return 0;
}

int wt_save_body(weight_t *wt, FILE *fp, bool binary, char *name)
{
    int n;
    int i, j, col;
    int sz;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (wt->row <= 0) {
        return 0;
    }

    sz = wt->row;
    if (wt->col > 0) {
        sz *= wt->col;
    }

    if (binary) {
        n = -WT_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        if (fwrite(wt->mat, sizeof(real_t), sz, fp) != sz) {
            ST_WARNING("Failed to write mat.");
            return -1;
        }
    } else {
        if (fprintf(fp, "<WEIGHT-DATA>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }
        if (fprintf(fp, "%s\n", name != NULL ? name : "") < 0) {
            ST_WARNING("Failed to fprintf name.");
            return -1;
        }

        if (wt->col <= 0) {
            col = 1;
        } else {
            col = wt->col;
        }
        for (i = 0; i < wt->row; i++) {
            for (j = 0; j < col - 1; j++) {
                if (fprintf(fp, REAL_FMT" ", wt->mat[i*col + j]) < 0) {
                    ST_WARNING("Failed to fprintf mat[%d,%d].", i, j);
                    return -1;
                }
            }
            if (fprintf(fp, REAL_FMT"\n", wt->mat[i*col + j]) < 0) {
                ST_WARNING("Failed to fprintf mat[%d/%d].", i, j);
                return -1;
            }
        }
    }

    return 0;
}

int wt_init(weight_t *wt, int row, int col)
{
    int i;
    int sz;

    ST_CHECK_PARAM(wt == NULL || row <= 0, -1);

    if (wt_alloc(wt, row, col) < 0) {
        ST_WARNING("Failed to wt_alloc.");
        goto ERR;
    }

    sz = wt->row;
    if (wt->col > 0) {
        sz *= wt->col;
    }

    switch(wt->init_type) {
        case WT_INIT_CONST:
            for (i = 0; i < sz; i++) {
                wt->mat[i] = 0;
            }
        case WT_INIT_UNIFORM:
            for (i = 0; i < sz; i++) {
                wt->mat[i] = st_random(-wt->init_param, wt->init_param);
            }
            break;
        case WT_INIT_NORM:
            for (i = 0; i < sz; i++) {
                wt->mat[i] = st_normrand(0, wt->init_param);
            }
        case WT_INIT_TRUNC_NORM:
            for (i = 0; i < sz; i++) {
                wt->mat[i] = st_trunc_normrand(0, wt->init_param, 2.0);
            }
            break;
        default:
            ST_WARNING("Unknown init type[%d]", wt->init_type);
            goto ERR;
    }

    return 0;

ERR:
    safe_st_aligned_free(wt->mat);
    return -1;
}

void wt_sanity_check(weight_t *wt, const char *name)
{
    int n;
    int i, sz;

    ST_CHECK_PARAM_VOID(wt == NULL);

    sz = wt->row;
    if (wt->col > 0) {
        sz *= wt->col;
    }

    n = 0;
    for (i = 0; i < sz; i++) {
        if (wt->mat[i] >= 100 || wt->mat[i] <= -100) {
            n++;
        }
    }

    if (n > 0) {
        ST_WARNING("There are %d extraordinary values in weight of [%s]",
                n, name);
    }
}
