/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Wang Jian
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

#include "updaters/component_updater.h"
#include "updaters/output_updater.h"

#include "direct_glue.h"
#include "fc_glue.h"
#include "emb_glue.h"
#include "out_glue.h"
#include "glue.h"

static const int GLUE_MAGIC_NUM = 626140498 + 70;

static glue_impl_t GLUE_IMPL[] = {
    {DIRECT_GLUE_NAME, direct_glue_init, direct_glue_destroy, direct_glue_dup,
        direct_glue_parse_topo, direct_glue_check, direct_glue_draw_label,
        direct_glue_load_header, NULL, direct_glue_save_header, NULL,
        direct_glue_init_data,
        direct_glue_print_verbose_info},
    {FC_GLUE_NAME, NULL, NULL, NULL,
        fc_glue_parse_topo, fc_glue_check, NULL,
        NULL, NULL, NULL, NULL,
        fc_glue_init_data, NULL},
    {EMB_GLUE_NAME, emb_glue_init, emb_glue_destroy, emb_glue_dup,
        emb_glue_parse_topo, emb_glue_check, emb_glue_draw_label,
        emb_glue_load_header, NULL, emb_glue_save_header, NULL,
        emb_glue_init_data, NULL},
    {OUT_GLUE_NAME, NULL, NULL, NULL,
        out_glue_parse_topo, out_glue_check, NULL,
        NULL, NULL, NULL, NULL,
        out_glue_init_data, NULL},
};

static glue_impl_t* glue_get_impl(const char *type)
{
    int t;

    for (t = 0; t < sizeof(GLUE_IMPL) / sizeof(GLUE_IMPL[0]); t++) {
        if (strcasecmp(type, GLUE_IMPL[t].type) == 0) {
            return GLUE_IMPL + t;
        }
    }

    return NULL;
}

static int glue_get_layer(layer_t **layers, int n_layer, const char *name)
{
    int t;

    for (t = 0; t < n_layer; t++) {
        if (strcmp(layers[t]->name, name) == 0) {
            return t;
        }
    }

    return -1;
}

void glue_destroy(glue_t *glue)
{
    int i;

    if (glue == NULL) {
        return;
    }

    glue->name[0] = '\0';

    if (glue->impl != NULL && glue->impl->destroy != NULL) {
        glue->impl->destroy(glue);
    }
    glue->type[0] = '\0';
    glue->extra = NULL;

    glue->in_layer = -1;
    glue->in_offset = -1;
    glue->in_length = -1;
    glue->out_layer = -1;
    glue->out_offset = -1;
    glue->out_length = -1;

    if (glue->wts != NULL) {
        for (i = 0;i < glue->num_wts; i++) {
            safe_wt_destroy(glue->wts[i]);
        }
        safe_st_free(glue->wts);
    }
    glue->num_wts = 0;
}

bool glue_check(glue_t *glue, layer_t **layers,
        int n_layer, input_t *input, output_t *output)
{
    ST_CHECK_PARAM(glue == NULL, false);

    if (glue->in_layer < 0) {
        ST_WARNING("No in layer found.");
        return false;
    }
    if (glue->out_layer < 0) {
        ST_WARNING("No out layer found.");
        return false;
    }

    if (glue->in_offset < 0) {
        glue->in_offset = 0;
    }
    if (glue->in_length < 0) {
        glue->in_length = layers[glue->in_layer]->size - glue->in_offset;
    }

    if (glue->in_offset + glue->in_length > layers[glue->in_layer]->size) {
        ST_WARNING("in_length must less than in layer size.");
        return false;
    }

    if (glue->out_offset < 0) {
        glue->out_offset = 0;
    }
    if (glue->out_length < 0) {
        glue->out_length = layers[glue->out_layer]->size - glue->out_offset;
    }
    if (glue->out_offset + glue->out_length > layers[glue->out_layer]->size) {
        ST_WARNING("out_length must less than out layer size.");
        return false;
    }

    return true;
}

glue_t* glue_parse_topo(const char *line, layer_t **layers,
        int n_layer, input_t *input, output_t *output)
{
    glue_t *glue = NULL;

    char keyvalue[2*MAX_LINE_LEN];
    char token[MAX_LINE_LEN];
    char untouch_topo[MAX_LINE_LEN];

    const char *p;
    int i;

    ST_CHECK_PARAM(line == NULL, NULL);

    glue = (glue_t *)st_malloc(sizeof(glue_t));
    if (glue == NULL) {
        ST_WARNING("Failed to st_malloc glue_t.");
        goto ERR;
    }
    memset(glue, 0, sizeof(glue_t));

    p = get_next_token(line, token);
    if (strcasecmp("glue", token) != 0) {
        ST_WARNING("Not glue line.");
        goto ERR;
    }

    glue->in_layer = -1;
    glue->in_offset = -1;
    glue->in_length = -1;
    glue->out_layer = -1;
    glue->out_offset = -1;
    glue->out_length = -1;
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

        if (strcasecmp("name", keyvalue) == 0) {
            strncpy(glue->name, keyvalue + MAX_LINE_LEN, MAX_NAME_LEN);
            glue->name[MAX_NAME_LEN - 1] = '\0';
        } else if (strcasecmp("type", keyvalue) == 0) {
            strncpy(glue->type, keyvalue + MAX_LINE_LEN, MAX_NAME_LEN);
            glue->type[MAX_NAME_LEN - 1] = '\0';

            glue->impl = glue_get_impl(glue->type);
            if (glue->impl == NULL) {
                ST_WARNING("Unknown type of glue [%s].", glue->type);
                goto ERR;
            }
            if (glue->impl->init != NULL && glue->impl->init(glue) < 0) {
                ST_WARNING("Failed to init impl glue.");
                goto ERR;
            }
        } else if (strcasecmp("in", keyvalue) == 0) {
            if (glue->in_layer >= 0) {
                ST_WARNING("Duplicated in-layer.");
                goto ERR;
            }
            glue->in_layer = glue_get_layer(layers, n_layer,
                    keyvalue + MAX_LINE_LEN);
            if (glue->in_layer < 0) {
                ST_WARNING("No layer named [%s] is found.",
                        keyvalue + MAX_LINE_LEN);
                goto ERR;
            }
        } else if (strcasecmp("in_offset", keyvalue) == 0) {
            if (glue->in_offset >= 0) {
                ST_WARNING("Duplicated in-offset.");
                goto ERR;
            }
            glue->in_offset = atoi(keyvalue + MAX_LINE_LEN);
            if (glue->in_offset < 0) {
                ST_WARNING("Illegal in-offset value[%s].",
                        keyvalue + MAX_LINE_LEN);
                goto ERR;
            }
        } else if (strcasecmp("in_length", keyvalue) == 0) {
            if (glue->in_length >= 0) {
                ST_WARNING("Duplicated in-length.");
                goto ERR;
            }
            glue->in_length = atoi(keyvalue + MAX_LINE_LEN);
            if (glue->in_length <= 0) {
                ST_WARNING("Illegal in-length value[%s].",
                        keyvalue + MAX_LINE_LEN);
                goto ERR;
            }
        } else if (strcasecmp("out", keyvalue) == 0) {
            if (glue->out_layer >= 0) {
                ST_WARNING("Duplicated out-layer.");
                goto ERR;
            }
            glue->out_layer = glue_get_layer(layers, n_layer,
                    keyvalue + MAX_LINE_LEN);
            if (glue->out_layer < 0) {
                ST_WARNING("No layer named [%s] is found.",
                        keyvalue + MAX_LINE_LEN);
                goto ERR;
            }
        } else if (strcasecmp("out_offset", keyvalue) == 0) {
            if (glue->out_offset >= 0) {
                ST_WARNING("Duplicated out-offset.");
                goto ERR;
            }
            glue->out_offset = atoi(keyvalue + MAX_LINE_LEN);
            if (glue->out_offset < 0) {
                ST_WARNING("Illegal out-offset value[%s].",
                        keyvalue + MAX_LINE_LEN);
                goto ERR;
            }
        } else if (strcasecmp("out_length", keyvalue) == 0) {
            if (glue->out_length >= 0) {
                ST_WARNING("Duplicated out-length.");
                goto ERR;
            }
            glue->out_length = atoi(keyvalue + MAX_LINE_LEN);
            if (glue->out_length <= 0) {
                ST_WARNING("Illegal out-length value[%s].",
                        keyvalue + MAX_LINE_LEN);
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

    if (glue->name[0] == '\0') {
        ST_WARNING("No glue name found.");
        goto ERR;
    }

    if (strcasecmp(glue->type, OUT_GLUE_NAME) == 0) {
        glue->num_wts = output->tree->num_node;
    } else {
        glue->num_wts = 1;
    }

    glue->wts = (weight_t **)st_malloc(sizeof(weight_t *) * glue->num_wts);
    if (glue->wts == NULL) {
        ST_WARNING("Failed to st_malloc wts");
        goto ERR;
    }
    memset(glue->wts, 0, sizeof(weight_t *) * glue->num_wts);

    for (i = 0; i < glue->num_wts; i++) {
        glue->wts[i] = (weight_t *)st_malloc(sizeof(weight_t));
        if (glue->wts[i] == NULL) {
            ST_WARNING("Failed to st_malloc wts");
            goto ERR;
        }
        memset(glue->wts[i], 0, sizeof(weight_t));
    }

    if (wt_parse_topo(glue->wts[0], untouch_topo, MAX_LINE_LEN) < 0) {
        ST_WARNING("Failed to wt_parse_topo.");
        goto ERR;
    }
    for (i = 1; i < glue->num_wts; i++) {
        *(glue->wts[i]) = *(glue->wts[0]);
    }

    if (!glue_check(glue, layers, n_layer, input, output)) {
        ST_WARNING("Failed to glue_check.");
        return false;
    }

    if (glue->impl == NULL) {
        ST_WARNING("No type found.");
        goto ERR;
    }
    if (glue->impl->parse_topo != NULL) {
        if (glue->impl->parse_topo(glue, untouch_topo) < 0) {
            ST_WARNING("Failed to parse_topo for impl glue.");
            goto ERR;
        }
    }

    if (glue->impl->check != NULL) {
        if (!glue->impl->check(glue, layers, n_layer, input, output)) {
            ST_WARNING("check glue failed.");
            goto ERR;
        }
    }

    return glue;

ERR:
    safe_glue_destroy(glue);
    return NULL;
}

glue_t* glue_dup(glue_t *g)
{
    glue_t *glue = NULL;
    int i;

    ST_CHECK_PARAM(g == NULL, NULL);

    glue = (glue_t *)st_malloc(sizeof(glue_t));
    if (glue == NULL) {
        ST_WARNING("Failed to st_malloc glue_t.");
        goto ERR;
    }
    memset(glue, 0, sizeof(glue_t));


    strncpy(glue->name, g->name, MAX_NAME_LEN);
    glue->name[MAX_NAME_LEN - 1] = '\0';
    strncpy(glue->type, g->type, MAX_NAME_LEN);
    glue->type[MAX_NAME_LEN - 1] = '\0';

    glue->in_layer = g->in_layer;
    glue->out_layer = g->out_layer;
    glue->in_offset = g->in_offset;
    glue->in_length = g->in_length;
    glue->out_offset = g->out_offset;
    glue->out_length = g->out_length;

    glue->recur_type = g->recur_type;

    glue->bptt_opt = g->bptt_opt;
    glue->param = g->param;

    glue->num_wts = g->num_wts;
    glue->wts = (weight_t **)st_malloc(sizeof(weight_t *) * glue->num_wts);
    if (glue->wts == NULL) {
        ST_WARNING("Failed to st_malloc wts");
        goto ERR;
    }
    memset(glue->wts, 0, sizeof(weight_t *) * glue->num_wts);

    for (i = 0; i < glue->num_wts; i++) {
        glue->wts[i] = wt_dup(g->wts[i]);
        if (glue->wts[i] == NULL) {
            ST_WARNING("Failed to wt_dup wt[%d].", i);
            goto ERR;
        }
    }

    glue->impl = g->impl;
    if (glue->impl != NULL && glue->impl->dup != NULL) {
        if (glue->impl->dup(glue, g) < 0) {
            ST_WARNING("Failed to glue->impl->dup[%s].", glue->name);
            goto ERR;
        }
    }

    return glue;

ERR:
    safe_glue_destroy(glue);
    return NULL;
}

int glue_load_header(glue_t **glue, int version,
        FILE *fp, connlm_fmt_t *fmt, FILE *fo_info)
{
    union {
        char str[4];
        int magic_num;
    } flag;

    char name[MAX_NAME_LEN];
    char type[MAX_NAME_LEN];
    int in_layer;
    int in_offset;
    int in_length;
    int out_layer;
    int out_offset;
    int out_length;
    int num_wts;

    glue_impl_t *impl;
    connlm_fmt_t f;
    int i;

    ST_CHECK_PARAM((glue == NULL && fo_info == NULL) || fp == NULL
            || fmt == NULL, -1);

    if (version < 20) {
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
    } else if (GLUE_MAGIC_NUM != flag.magic_num) {
        ST_WARNING("magic num wrong.");
        return -2;
    }

    if (glue != NULL) {
        *glue = NULL;
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

        if (fread(name, sizeof(char), MAX_NAME_LEN, fp) != MAX_NAME_LEN) {
            ST_WARNING("Failed to read name.");
            return -1;
        }
        name[MAX_NAME_LEN - 1] = '\0';
        if (fread(type, sizeof(char), MAX_NAME_LEN, fp) != MAX_NAME_LEN) {
            ST_WARNING("Failed to read type.");
            return -1;
        }
        type[MAX_NAME_LEN - 1] = '\0';
        if (fread(&in_layer, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read in_layer.");
            return -1;
        }
        if (fread(&in_offset, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read in_offset.");
            return -1;
        }
        if (fread(&in_length, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read in_length.");
            return -1;
        }
        if (fread(&out_layer, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read out_layer.");
            return -1;
        }
        if (fread(&out_offset, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read out_offset.");
            return -1;
        }
        if (fread(&out_length, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read out_length.");
            return -1;
        }
        if (fread(&num_wts, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read num_wts.");
            return -1;
        }
    } else {
        if (st_readline(fp, "") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }
        if (st_readline(fp, "<GLUE>") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }

        if (st_readline(fp, "Name: %"xSTR(MAX_NAME_LEN)"s", name) != 1) {
            ST_WARNING("Failed to parse name.");
            goto ERR;
        }
        name[MAX_NAME_LEN - 1] = '\0';
        if (st_readline(fp, "Type: %"xSTR(MAX_NAME_LEN)"s", type) != 1) {
            ST_WARNING("Failed to parse type.");
            goto ERR;
        }
        type[MAX_NAME_LEN - 1] = '\0';

        if (st_readline(fp, "In layer: %d", &in_layer) != 1) {
            ST_WARNING("Failed to parse in_layer.");
            goto ERR;
        }
        if (st_readline(fp, "In layer offset: %d", &in_offset) != 1) {
            ST_WARNING("Failed to parse in_offset.");
            goto ERR;
        }
        if (st_readline(fp, "In layer length: %d", &in_length) != 1) {
            ST_WARNING("Failed to parse in_length.");
            goto ERR;
        }
        if (st_readline(fp, "Out layer: %d", &out_layer) != 1) {
            ST_WARNING("Failed to parse out_layer.");
            goto ERR;
        }
        if (st_readline(fp, "Out layer offset: %d", &out_offset) != 1) {
            ST_WARNING("Failed to parse out_offset.");
            goto ERR;
        }
        if (st_readline(fp, "Out layer length: %d", &out_length) != 1) {
            ST_WARNING("Failed to parse out_length.");
            goto ERR;
        }
        if (st_readline(fp, "Num weights: %d", &num_wts) != 1) {
            ST_WARNING("Failed to parse num_wts.");
            goto ERR;
        }
    }

    impl = glue_get_impl(type);
    if (impl == NULL) {
        ST_WARNING("Unknown glue type[%s].", type);
        goto ERR;
    }

    if (glue != NULL) {
        *glue = (glue_t *)st_malloc(sizeof(glue_t));
        if (*glue == NULL) {
            ST_WARNING("Failed to st_malloc glue_t");
            goto ERR;
        }
        memset(*glue, 0, sizeof(glue_t));

        strcpy((*glue)->name, name);
        strcpy((*glue)->type, type);
        (*glue)->in_layer = in_layer;
        (*glue)->in_offset = in_offset;
        (*glue)->in_length = in_length;
        (*glue)->out_layer = out_layer;
        (*glue)->out_offset = out_offset;
        (*glue)->out_length = out_length;
        (*glue)->num_wts = num_wts;
        (*glue)->impl = impl;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<GLUE>:%s\n", name);
        fprintf(fo_info, "Type: %s\n", type);
        fprintf(fo_info, "in layer: %d\n", in_layer);
        fprintf(fo_info, "in offset: %d\n", in_offset);
        fprintf(fo_info, "in length: %d\n", in_length);
        fprintf(fo_info, "out layer: %d\n", out_layer);
        fprintf(fo_info, "out offset: %d\n", out_offset);
        fprintf(fo_info, "out length: %d\n", out_length);
        fprintf(fo_info, "num weights: %d\n", num_wts);
    }

    if (glue != NULL) {
        (*glue)->num_wts = num_wts;
        (*glue)->wts = (weight_t **)st_malloc(sizeof(weight_t *) * num_wts);
        if ((*glue)->wts == NULL) {
            ST_WARNING("Failed to st_malloc wts");
            return -1;
        }
        memset((*glue)->wts, 0, sizeof(weight_t *) * num_wts);
    }

    if (wt_load_header(glue != NULL ? &((*glue)->wts[0]) : NULL,
            version, fp, &f, fo_info) < 0) {
        ST_WARNING("Failed to wt_load_header[0].");
        goto ERR;
    }
    if (connlm_fmt_is_bin(*fmt) != connlm_fmt_is_bin(f)) {
        ST_WARNING("Multiple formats in one file.");
        return -1;
    }

    for (i = 1; i < num_wts; i++) {
        // do not print info for other weights
        if (wt_load_header(glue != NULL ? &((*glue)->wts[i]) : NULL,
                    version, fp, &f, NULL) < 0) {
            ST_WARNING("Failed to wt_load_header[%d].", i);
            goto ERR;
        }
        if (connlm_fmt_is_bin(*fmt) != connlm_fmt_is_bin(f)) {
            ST_WARNING("Multiple formats in one file.");
            return -1;
        }
    }

    if (impl->load_header != NULL) {
        if (impl->load_header(glue != NULL ? &((*glue)->extra) : NULL,
                version, fp, &f, fo_info) < 0) {
            ST_WARNING("Failed to impl->load_header.");
            goto ERR;
        }
        if (*fmt != f) {
            ST_WARNING("Multiple formats in one file.");
            return -1;
        }
    }

    return 0;

ERR:
    if (glue != NULL) {
        safe_glue_destroy(*glue);
    }
    return -1;
}

int glue_load_body(glue_t *glue, int version, FILE *fp, connlm_fmt_t fmt)
{
    connlm_fmt_t wt_fmt;
    int n;
    int i;

    ST_CHECK_PARAM(glue == NULL || fp == NULL, -1);

    if (version < 5) {
        ST_WARNING("Too old version of connlm file");
        return -1;
    }

    if (connlm_fmt_is_bin(fmt)) {
        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read magic num.");
            goto ERR;
        }

        if (n != -GLUE_MAGIC_NUM) {
            ST_WARNING("Magic num error.");
            goto ERR;
        }
    } else {
        if (st_readline(fp, "<GLUE-DATA>") != 0) {
            ST_WARNING("body flag error.");
            goto ERR;
        }
    }

    if (strcasecmp(glue->type, DIRECT_GLUE_NAME) == 0) {
        wt_fmt = fmt;
    } else {
        if (connlm_fmt_is_bin(fmt)) {
            wt_fmt = CONN_FMT_BIN;
        } else {
            wt_fmt = CONN_FMT_TXT;
        }
    }

    for (i = 0; i < glue->num_wts; i++) {
        if (wt_load_body(glue->wts[i], version, fp, wt_fmt) < 0) {
            ST_WARNING("Failed to wt_load_body[%d].", i);
            goto ERR;
        }
    }

    if (glue->impl->load_body != NULL) {
        if (glue->impl->load_body(glue->extra, version, fp, fmt) < 0) {
            ST_WARNING("Failed to glue->impl->load_body.");
            goto ERR;
        }
    }

    return 0;

ERR:
    return -1;
}

int glue_save_header(glue_t *glue, FILE *fp, connlm_fmt_t fmt)
{
    int n;
    int i;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (connlm_fmt_is_bin(fmt)) {
        if (fwrite(&GLUE_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }
        if (fwrite(&fmt, sizeof(connlm_fmt_t), 1, fp) != 1) {
            ST_WARNING("Failed to write fmt.");
            return -1;
        }

        if (glue == NULL) {
            n = 0;
            if (fwrite(&n, sizeof(int), 1, fp) != 1) {
                ST_WARNING("Failed to write glue size.");
                return -1;
            }
            return 0;
        }

        if (fwrite(glue->name, sizeof(char), MAX_NAME_LEN,
                    fp) != MAX_NAME_LEN) {
            ST_WARNING("Failed to write name.");
            return -1;
        }
        if (fwrite(glue->type, sizeof(char), MAX_NAME_LEN,
                    fp) != MAX_NAME_LEN) {
            ST_WARNING("Failed to write type.");
            return -1;
        }
        if (fwrite(&glue->in_layer, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write in_layer.");
            return -1;
        }
        if (fwrite(&glue->in_offset, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write in_offset.");
            return -1;
        }
        if (fwrite(&glue->in_length, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write in_length.");
            return -1;
        }
        if (fwrite(&glue->out_layer, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write out_layer.");
            return -1;
        }
        if (fwrite(&glue->out_offset, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write out_offset.");
            return -1;
        }
        if (fwrite(&glue->out_length, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write out_length.");
            return -1;
        }
        if (fwrite(&glue->num_wts, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write num_wts.");
            return -1;
        }
    } else {
        if (fprintf(fp, "    \n<GLUE>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }

        if (fprintf(fp, "Name: %s\n", glue->name) < 0) {
            ST_WARNING("Failed to fprintf name.");
            return -1;
        }
        if (fprintf(fp, "Type: %s\n", glue->type) < 0) {
            ST_WARNING("Failed to fprintf type.");
            return -1;
        }
        if (fprintf(fp, "In layer: %d\n", glue->in_layer) < 0) {
            ST_WARNING("Failed to fprintf in_layer.");
            return -1;
        }
        if (fprintf(fp, "In layer offset: %d\n", glue->in_offset) < 0) {
            ST_WARNING("Failed to fprintf in_offset.");
            return -1;
        }
        if (fprintf(fp, "In layer length: %d\n", glue->in_length) < 0) {
            ST_WARNING("Failed to fprintf in_length.");
            return -1;
        }
        if (fprintf(fp, "Out layer: %d\n", glue->out_layer) < 0) {
            ST_WARNING("Failed to fprintf out_layer.");
            return -1;
        }
        if (fprintf(fp, "Out layer offset: %d\n", glue->out_offset) < 0) {
            ST_WARNING("Failed to fprintf out_offset.");
            return -1;
        }
        if (fprintf(fp, "Out layer length: %d\n", glue->out_length) < 0) {
            ST_WARNING("Failed to fprintf out_length.");
            return -1;
        }
        if (fprintf(fp, "Num weights: %d\n", glue->num_wts) < 0) {
            ST_WARNING("Failed to fprintf num_wts.");
            return -1;
        }
    }

    for (i = 0; i < glue->num_wts; i++) {
        if (wt_save_header(glue->wts[i], fp, fmt) < 0) {
            ST_WARNING("Failed to wt_save_header[%d].", i);
            return -1;
        }
    }

    if (glue->impl != NULL && glue->impl->save_header != NULL) {
        if (glue->impl->save_header(glue->extra, fp, fmt) < 0) {
            ST_WARNING("Failed to glue->impl->save_header.");
            return -1;
        }
    }

    return 0;
}

int glue_save_body(glue_t *glue, FILE *fp, connlm_fmt_t fmt)
{
    char name[MAX_NAME_LEN];
    connlm_fmt_t wt_fmt;
    int n;
    int i;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (connlm_fmt_is_bin(fmt)) {
        n = -GLUE_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }
    } else {
        if (fprintf(fp, "<GLUE-DATA>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }
    }

    if (strcasecmp(glue->type, DIRECT_GLUE_NAME) == 0) {
        wt_fmt = fmt;
    } else {
        if (connlm_fmt_is_bin(fmt)) {
            wt_fmt = CONN_FMT_BIN;
        } else {
            wt_fmt = CONN_FMT_TXT;
        }
    }

    for (i = 0; i < glue->num_wts; i++) {
        snprintf(name, MAX_NAME_LEN, "%s-%d", glue->name, i);
        if (wt_save_body(glue->wts[i], fp, wt_fmt, name) < 0) {
            ST_WARNING("Failed to wt_save_body[%d].", i);
            return -1;
        }
    }

    if (glue->impl != NULL && glue->impl->save_body != NULL) {
        if (glue->impl->save_body(glue->extra, fp, fmt) < 0) {
            ST_WARNING("Failed to glue->impl->save_body.");
            return -1;
        }
    }

    return 0;
}

char* glue_draw_label(glue_t *glue, char *label, size_t label_len,
        bool verbose)
{
    char buf[MAX_LINE_LEN];

    ST_CHECK_PARAM(glue == NULL || label == NULL, NULL);

    if (verbose) {
        snprintf(label, label_len, "%s/type=%s,in=(%d,%d),out=(%d,%d)",
                glue->name, glue->type, glue->in_offset, glue->in_length,
                glue->out_offset, glue->out_length);
        if (glue->impl != NULL && glue->impl->draw_label != NULL) {
            strncat(label, glue->impl->draw_label(glue, buf, MAX_LINE_LEN),
                    label_len - strlen(label) - 1);
        }
    } else {
        snprintf(label, label_len, "%s", glue->name);
    }

    return label;
}

char* glue_draw_label_one(glue_t *glue, int lid,
        char *label, size_t label_len, bool verbose)
{
    ST_CHECK_PARAM(glue == NULL || label == NULL, NULL);

    label[0] = '\0';

    return label;
}

int glue_init_data(glue_t *glue, input_t *input,
        layer_t **layers, output_t *output)
{
    ST_CHECK_PARAM(glue == NULL, -1);

    if (glue->impl != NULL && glue->impl->init_data != NULL) {
        if (glue->impl->init_data(glue, input, layers, output) < 0) {
            ST_WARNING("Failed to init_data for glue impl.");
            return -1;
        }
    }

    return 0;
}

wt_updater_t** glue_init_wt_updaters(glue_t *glue, param_t *param,
        wt_update_type_t wu_type, int *num_wt_updaters)
{
    wt_updater_t **wt_updaters = NULL;
    int i;

    ST_CHECK_PARAM(glue == NULL, NULL);

    *num_wt_updaters = glue->num_wts;
    wt_updaters = (wt_updater_t **)st_malloc(sizeof(wt_updater_t *)
            * (*num_wt_updaters));
    if (wt_updaters == NULL) {
        ST_WARNING("Failed to st_malloc wt_updaters.");
        return NULL;
    }
    memset(wt_updaters, 0, sizeof(wt_updater_t *) * (*num_wt_updaters));

    for (i = 0; i < *num_wt_updaters; i++) {
        wt_updaters[i] = wt_updater_create(param == NULL ? &glue->param : param,
                &glue->wts[i]->w, &glue->wts[i]->bias, wu_type);
        if (wt_updaters[i] == NULL) {
            ST_WARNING("Failed to wt_updater_create.");
            goto ERR;
        }
    }

    return wt_updaters;

ERR:
    if (wt_updaters != NULL) {
        for (i = 0; i < *num_wt_updaters; i++) {
            safe_wt_updater_destroy(wt_updaters[i]);
        }
        safe_st_free(wt_updaters);
    }
    *num_wt_updaters = 0;
    return NULL;
}

int glue_load_train_opt(glue_t *glue, st_opt_t *opt, const char *sec_name,
        param_t *parent_param, bptt_opt_t *parent_bptt_opt)
{
    char name[MAX_ST_CONF_LEN];

    ST_CHECK_PARAM(glue == NULL || opt == NULL, -1);

    if (glue->wts != NULL && glue->wts[0]->w.num_rows > 0) {
        if (sec_name == NULL || sec_name[0] == '\0') {
            snprintf(name, MAX_ST_CONF_LEN, "%s", glue->name);
        } else {
            snprintf(name, MAX_ST_CONF_LEN, "%s/%s", sec_name, glue->name);
        }
        if (param_load(&glue->param, opt, name, parent_param) < 0) {
            ST_WARNING("Failed to param_load.");
            goto ST_OPT_ERR;
        }

        if (glue->recur_type == RECUR_HEAD) {
            if (bptt_opt_load(&glue->bptt_opt, opt, name,
                        parent_bptt_opt) < 0) {
                ST_WARNING("Failed to bptt_opt_load");
                goto ST_OPT_ERR;
            }
        }
    }

    return 0;

ST_OPT_ERR:
    return -1;
}

void glue_sanity_check(glue_t *glue)
{
    char name[MAX_NAME_LEN];
    int i;

    ST_CHECK_PARAM_VOID(glue == NULL);

    if (glue->wts != NULL) {
        for (i = 0; i < glue->num_wts; i++) {
            snprintf(name, MAX_NAME_LEN, "%s-%d", glue->name, i);
            wt_sanity_check(glue->wts[i], name);
        }
    }
}

void glue_print_verbose_info(glue_t *glue, FILE *fo)
{
    ST_CHECK_PARAM_VOID(glue == NULL || fo == NULL);

    if (glue->impl != NULL && glue->impl->print_verbose_info != NULL) {
        glue->impl->print_verbose_info(glue, fo);
    }
}
