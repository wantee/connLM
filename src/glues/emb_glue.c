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
#include <stutils/st_io.h>
#include <stutils/st_string.h>

#include "input.h"
#include "output.h"
#include "emb_glue.h"

static const int EMB_GLUE_MAGIC_NUM = 626140498 + 72;

static const char *combine_str[] = {
    "UnDefined",
    "Sum",
    "Avg",
    "Concat",
};

static const char* combine2str(emb_combine_t c)
{
    return combine_str[c];
}

static emb_combine_t str2combine(const char *str)
{
    int i;

    for (i = 0; i < sizeof(combine_str) / sizeof(combine_str[0]); i++) {
        if (strcasecmp(combine_str[i], str) == 0) {
            return (emb_combine_t)i;
        }
    }

    return EC_UNKNOWN;
}

#define safe_emb_glue_data_destroy(ptr) do {\
    if((ptr) != NULL) {\
        emb_glue_data_destroy((emb_glue_data_t *)ptr);\
        safe_st_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)

static void emb_glue_data_destroy(emb_glue_data_t *data)
{
    if (data == NULL) {
        return;
    }

    data->combine = EC_UNKNOWN;
}

static emb_glue_data_t* emb_glue_data_init()
{
    emb_glue_data_t *data = NULL;

    data = (emb_glue_data_t *)st_malloc(sizeof(emb_glue_data_t));
    if (data == NULL) {
        ST_WARNING("Failed to st_malloc emb_glue_data.");
        goto ERR;
    }
    memset(data, 0, sizeof(emb_glue_data_t));

    return data;
ERR:
    safe_emb_glue_data_destroy(data);
    return NULL;
}

static emb_glue_data_t* emb_glue_data_dup(emb_glue_data_t *src)
{
    emb_glue_data_t *dst = NULL;

    ST_CHECK_PARAM(src == NULL, NULL);

    dst = emb_glue_data_init();
    if (dst == NULL) {
        ST_WARNING("Failed to emb_glue_data_init.");
        goto ERR;
    }
    dst->combine = ((emb_glue_data_t *)src)->combine;

    return (void *)dst;
ERR:
    safe_emb_glue_data_destroy(dst);
    return NULL;
}

void emb_glue_destroy(glue_t *glue)
{
    if (glue == NULL) {
        return;
    }

    safe_emb_glue_data_destroy(glue->extra);
}

int emb_glue_init(glue_t *glue)
{
    ST_CHECK_PARAM(glue == NULL, -1);

    if (strcasecmp(glue->type, EMB_GLUE_NAME) != 0) {
        ST_WARNING("Not a sum glue. [%s]", glue->type);
        return -1;
    }

    glue->extra = (void *)emb_glue_data_init();
    if (glue->extra == NULL) {
        ST_WARNING("Failed to emb_glue_data_init.");
        goto ERR;
    }

    return 0;

ERR:
    safe_emb_glue_data_destroy(glue->extra);
    return -1;
}

int emb_glue_dup(glue_t *dst, glue_t *src)
{
    ST_CHECK_PARAM(dst == NULL || src == NULL, -1);

    if (strcasecmp(dst->type, EMB_GLUE_NAME) != 0) {
        ST_WARNING("dst is Not a emb glue. [%s]", dst->type);
        return -1;
    }

    if (strcasecmp(src->type, EMB_GLUE_NAME) != 0) {
        ST_WARNING("src is Not a emb glue. [%s]", src->type);
        return -1;
    }

    dst->extra = (void *)emb_glue_data_dup((emb_glue_data_t *)src->extra);
    if (dst->extra == NULL) {
        ST_WARNING("Failed to emb_glue_data_dup.");
        goto ERR;
    }

    return 0;

ERR:
    safe_emb_glue_data_destroy(dst->extra);
    return -1;
}

int emb_glue_parse_topo(glue_t *glue, const char *line)
{
    char keyvalue[2*MAX_LINE_LEN];
    char token[MAX_LINE_LEN];

    emb_glue_data_t *data;
    const char *p;

    ST_CHECK_PARAM(glue == NULL || line == NULL, -1);

    data = (emb_glue_data_t *)glue->extra;
    p = line;

    while (p != NULL) {
        p = get_next_token(p, token);
        if (token[0] == '\0') {
            continue;
        }

        if (split_line(token, keyvalue, 2, MAX_LINE_LEN, "=") != 2) {
            ST_WARNING("Failed to split key/value. [%s]", token);
            goto ERR;
        }

        if (strcasecmp("combine", keyvalue) == 0) {
            if (data->combine != EC_UNDEFINED) {
                ST_WARNING("Duplicated combine.");
            }
            data->combine = str2combine(keyvalue + MAX_LINE_LEN);
            if (data->combine == EC_UNKNOWN) {
                ST_WARNING("Failed to parse combine string.[%s]",
                        keyvalue + MAX_LINE_LEN);
                goto ERR;
            }
        } else {
            ST_WARNING("Unknown key/value[%s]", token);
        }
    }

    return 0;

ERR:
    return -1;
}

bool emb_glue_check(glue_t *glue, layer_t **layers, int n_layer,
        input_t *input, output_t *output)
{
    emb_glue_data_t *data;

    ST_CHECK_PARAM(glue == NULL || input == NULL, false);

    data = (emb_glue_data_t *)glue->extra;

    if (strcasecmp(glue->type, EMB_GLUE_NAME) != 0) {
        ST_WARNING("Not a emb glue. [%s]", glue->type);
        return false;
    }

    if (layers == NULL) {
        ST_WARNING("No layers.");
        return false;
    }

    if (strcasecmp(layers[glue->in_layer]->type,
                INPUT_LAYER_NAME) != 0) {
        ST_WARNING("emb glue: in layer should be input layer.");
        return false;
    }

    if (strcasecmp(layers[glue->out_layer]->type,
                OUTPUT_LAYER_NAME) == 0) {
        ST_WARNING("emb glue: out layer should not be output layer.");
        return false;
    }

    if (input->n_ctx > 1) {
        if (data->combine == EC_UNDEFINED) {
            ST_WARNING("emb glue: No combine specified in input.");
            return false;
        }
    } else {
        if (data->combine == EC_UNDEFINED) {
            data->combine = EC_CONCAT;
        }
    }

    if (data->combine == EC_CONCAT) {
        if (glue->out_length % input->n_ctx != 0) {
            ST_WARNING("emb glue: can not apply CONCAT combination. "
                    "out layer length can not divided by context number.");
            return false;
        }
    }

    glue->wt->init_bias = INFINITY; // direct weight will not have bias

    return true;
}

char* emb_glue_draw_label(glue_t *glue, char *label, size_t label_len)
{
    emb_glue_data_t *data;

    ST_CHECK_PARAM(glue == NULL || label == NULL, NULL);

    data = (emb_glue_data_t *)glue->extra;

    snprintf(label, label_len, ",combine=%s", combine2str(data->combine));

    return label;
}

int emb_glue_load_header(void **extra, int version,
        FILE *fp, connlm_fmt_t *fmt, FILE *fo_info)
{
    emb_glue_data_t *data = NULL;

    union {
        char str[4];
        int magic_num;
    } flag;

    char sym[MAX_LINE_LEN];
    int c;

    ST_CHECK_PARAM((extra == NULL && fo_info == NULL) || fp == NULL
            || fmt == NULL, -1);

    if (version < 15) {
        ST_WARNING("Too old version of connlm file");
        return -1;
    }

    if (fread(&flag.magic_num, sizeof(int), 1, fp) != 1) {
        ST_WARNING("Failed to load magic num.");
        return -1;
    }

    *fmt = CONN_FMT_UNKNOWN;
    if (strncmp(flag.str, "    ", 4) == 0) {
        *fmt = CONN_FMT_TXT;
    } else if (EMB_GLUE_MAGIC_NUM != flag.magic_num) {
        ST_WARNING("magic num wrong.");
        return -2;
    }

    if (extra != NULL) {
        *extra = NULL;
    }

    if (*fmt != CONN_FMT_TXT) {
        if (version >= 12) {
            if (fread(fmt, sizeof(connlm_fmt_t), 1, fp) != 1) {
                ST_WARNING("Failed to read fmt.");
                goto ERR;
            }
        } else {
            *fmt = CONN_FMT_BIN;
        }

        if (fread(&c, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read combine.");
            return -1;
        }
    } else {
        if (st_readline(fp, "") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }
        if (st_readline(fp, "<EMB-GLUE>") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }

        if (st_readline(fp, "Combine: %"xSTR(MAX_LINE_LEN)"s", sym) != 1) {
            ST_WARNING("Failed to parse combine.");
            goto ERR;
        }
        sym[MAX_LINE_LEN - 1] = '\0';
        c = (int)str2combine(sym);
        if (c == (int)EC_UNKNOWN) {
            ST_WARNING("Unknown combine[%s]", sym);
            goto ERR;
        }
    }

    if (extra != NULL) {
        data = emb_glue_data_init();
        if (data == NULL) {
            ST_WARNING("Failed to emb_glue_data_init.");
            goto ERR;
        }

        *extra = (void *)data;
        data->combine = (emb_combine_t)c;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<EMB-GLUE>\n");
        fprintf(fo_info, "Combine: %s\n", combine2str((emb_combine_t)c));
    }

    return 0;

ERR:
    if (extra != NULL) {
        safe_emb_glue_data_destroy(*extra);
    }
    return -1;
}

int emb_glue_save_header(void *extra, FILE *fp, connlm_fmt_t fmt)
{
    emb_glue_data_t *data;
    int i;

    ST_CHECK_PARAM(extra == NULL || fp == NULL, -1);

    data = (emb_glue_data_t *)extra;

    if (connlm_fmt_is_bin(fmt)) {
        if (fwrite(&EMB_GLUE_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }
        if (fwrite(&fmt, sizeof(connlm_fmt_t), 1, fp) != 1) {
            ST_WARNING("Failed to write fmt.");
            return -1;
        }

        i = (int)data->combine;
        if (fwrite(&i, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write combine.");
            return -1;
        }
    } else {
        if (fprintf(fp, "    \n<EMB-GLUE>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }

        if (fprintf(fp, "Combine: %s\n", combine2str(data->combine)) < 0) {
            ST_WARNING("Failed to fprintf combine.");
            return -1;
        }
    }

    return 0;
}

int emb_glue_init_data(glue_t *glue, input_t *input,
        layer_t **layers, output_t *output)
{
    emb_glue_data_t *data;
    int col;

    ST_CHECK_PARAM(glue == NULL || glue->wt == NULL
            || input == NULL, -1);

    if (strcasecmp(glue->type, EMB_GLUE_NAME) != 0) {
        ST_WARNING("Not a emb glue. [%s]", glue->type);
        return -1;
    }

    data = (emb_glue_data_t *)glue->extra;
    if (data->combine == EC_CONCAT) {
        col = glue->out_length / input->n_ctx;
    } else {
        col = glue->out_length;
    }

    if (wt_init(glue->wt, input->input_size, col) < 0) {
        ST_WARNING("Failed to wt_init.");
        return -1;
    }

    return 0;
}

wt_updater_t* emb_glue_init_wt_updater(glue_t *glue, param_t *param)
{
    wt_updater_t *wt_updater = NULL;

    ST_CHECK_PARAM(glue == NULL, NULL);

    if (strcasecmp(glue->type, EMB_GLUE_NAME) != 0) {
        ST_WARNING("Not a emb glue. [%s]", glue->type);
        return NULL;
    }

    wt_updater = wt_updater_create(param == NULL ? &glue->param : param,
            glue->wt->mat, glue->wt->bias,
            glue->wt->row, glue->wt->col, WT_UT_ONE_SHOT);
    if (wt_updater == NULL) {
        ST_WARNING("Failed to wt_updater_create.");
        goto ERR;
    }

    return wt_updater;

ERR:
    safe_wt_updater_destroy(wt_updater);
    return NULL;
}
