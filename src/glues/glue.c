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

#include "sum_glue.h"
#include "append_glue.h"
#include "clone_glue.h"
#include "direct_glue.h"
#include "wt_glue.h"
#include "emb_wt_glue.h"
#include "out_wt_glue.h"
#include "glue.h"

static const int GLUE_MAGIC_NUM = 626140498 + 70;

typedef struct _glue_register_t_ {
    char type[MAX_NAME_LEN];
    int (*init)(glue_t *glue);
    void (*destroy)(glue_t *glue);
    int (*dup)(glue_t *dst, glue_t *src);
    int (*parse_topo)(glue_t *glue, const char *line);
    bool (*check)(glue_t *glue, layer_t **layers, layer_id_t n_layer);
    char* (*draw_label)(glue_t *glue, char *label, size_t label_len);
    int (*load_header)(void **extra, int version,
            FILE *fp, bool *binary, FILE *fo_info);
    int (*load_body)(void *extra, int version, FILE *fp, bool binary);
    int (*save_header)(void *extra, FILE *fp, bool binary);
    int (*save_body)(void *extra, FILE *fp, bool binary);
    int (*init_data)(glue_t *glue, input_t *input,
            layer_t **layers, output_t *output);
} glue_reg_t;

static glue_reg_t GLUE_REG[] = {
    {SUM_GLUE_NAME, sum_glue_init, sum_glue_destroy,
        sum_glue_dup, sum_glue_parse_topo, sum_glue_check,
        sum_glue_draw_label, NULL, NULL, NULL, NULL},
    {APPEND_GLUE_NAME, append_glue_init, append_glue_destroy,
        append_glue_dup, append_glue_parse_topo, append_glue_check,
        append_glue_draw_label, NULL, NULL, NULL, NULL},
    {CLONE_GLUE_NAME, clone_glue_init, clone_glue_destroy,
        clone_glue_dup, clone_glue_parse_topo, clone_glue_check,
        clone_glue_draw_label, NULL, NULL, NULL, NULL},
    {DIRECT_GLUE_NAME, direct_glue_init, direct_glue_destroy,
        direct_glue_dup, direct_glue_parse_topo, direct_glue_check,
        direct_glue_draw_label, NULL, NULL, NULL, NULL,
        direct_glue_init_data},
    {WT_GLUE_NAME, wt_glue_init, wt_glue_destroy,
        wt_glue_dup, wt_glue_parse_topo, wt_glue_check,
        wt_glue_draw_label, wt_glue_load_header, wt_glue_load_body,
        wt_glue_save_header, wt_glue_save_body,
        wt_glue_init_data},
    {EMB_WT_GLUE_NAME, emb_wt_glue_init, emb_wt_glue_destroy,
        emb_wt_glue_dup, emb_wt_glue_parse_topo, emb_wt_glue_check,
        emb_wt_glue_draw_label, emb_wt_glue_load_header,
        emb_wt_glue_load_body, emb_wt_glue_save_header,
        emb_wt_glue_save_body,
        emb_wt_glue_init_data},
    {OUT_WT_GLUE_NAME, out_wt_glue_init, out_wt_glue_destroy,
        out_wt_glue_dup, out_wt_glue_parse_topo, out_wt_glue_check,
        out_wt_glue_draw_label, out_wt_glue_load_header,
        out_wt_glue_load_body, out_wt_glue_save_header,
        out_wt_glue_save_body,
        out_wt_glue_init_data},
};

static glue_reg_t* glue_get_reg(const char *type)
{
    int t;

    for (t = 0; t < sizeof(GLUE_REG) / sizeof(GLUE_REG[0]); t++) {
        if (strcasecmp(type, GLUE_REG[t].type) == 0) {
            return GLUE_REG + t;
        }
    }

    return NULL;
}

static layer_id_t glue_get_layer(layer_t **layers, layer_id_t n_layer,
        const char *name)
{
    layer_id_t t;

    for (t = 0; t < n_layer; t++) {
        if (strcmp(layers[t]->name, name) == 0) {
            return t;
        }
    }

    return LAYER_ID_NONE;
}

void glue_destroy(glue_t *glue)
{
    glue_reg_t *reg;

    if (glue == NULL) {
        return;
    }

    glue->name[0] = '\0';

    reg = glue_get_reg(glue->type);
    if (reg != NULL) {
        reg->destroy(glue);
    }
    glue->type[0] = '\0';

    safe_free(glue->in_layers);
    safe_free(glue->in_offsets);
    safe_free(glue->in_scales);
    glue->num_in_layer = 0;
    safe_free(glue->out_layers);
    safe_free(glue->out_offsets);
    safe_free(glue->out_scales);
    glue->num_out_layer = 0;
}

bool glue_check(glue_t *glue)
{
    layer_id_t l;

    ST_CHECK_PARAM(glue == NULL, false);

    if (glue->num_in_layer <= 0) {
        ST_WARNING("No in layers found.");
        goto ERR;
    }
    if (glue->in_offsets == NULL) {
        glue->in_offsets = (glue_offset_t *)malloc(sizeof(glue_offset_t)
                * glue->num_in_layer);
        if (glue->in_offsets == NULL) {
            ST_WARNING("Failed to malloc in_offsets.");
            goto ERR;
        }
        for (l = 0; l < glue->num_in_layer; l++) {
            glue->in_offsets[l] = 0;
        }
    }
    if (glue->in_scales == NULL) {
        glue->in_scales = (real_t *)malloc(sizeof(real_t)
                * glue->num_in_layer);
        if (glue->in_scales == NULL) {
            ST_WARNING("Failed to malloc in_scales.");
            goto ERR;
        }
        for (l = 0; l < glue->num_in_layer; l++) {
            glue->in_scales[l] = (real_t)1.0;
        }
    }
    if (glue->num_out_layer <= 0) {
        ST_WARNING("No out layers found.");
        goto ERR;
    }
    if (glue->out_offsets == NULL) {
        glue->out_offsets = (glue_offset_t *)malloc(sizeof(glue_offset_t)
                * glue->num_out_layer);
        if (glue->out_offsets == NULL) {
            ST_WARNING("Failed to malloc out_offsets.");
            goto ERR;
        }
        for (l = 0; l < glue->num_out_layer; l++) {
            glue->out_offsets[l] = 0;
        }
    }
    if (glue->out_scales == NULL) {
        glue->out_scales = (real_t *)malloc(sizeof(real_t)
                * glue->num_out_layer);
        if (glue->out_scales == NULL) {
            ST_WARNING("Failed to malloc out_scales.");
            goto ERR;
        }
        for (l = 0; l < glue->num_out_layer; l++) {
            glue->out_scales[l] = (real_t)1.0;
        }
    }

    return true;

ERR:
    safe_free(glue->in_offsets);
    safe_free(glue->in_scales);
    safe_free(glue->out_offsets);
    safe_free(glue->out_scales);

    return false;
}

glue_t* glue_parse_topo(const char *line, layer_t **layers,
        layer_id_t n_layer)
{
    glue_t *glue = NULL;
    glue_reg_t *reg = NULL;

    char keyvalue[2*MAX_LINE_LEN];
    char token[MAX_LINE_LEN];
    char reg_topo[MAX_LINE_LEN];
    char *names = NULL;
    size_t name_cap = 16;

    const char *p;
    layer_id_t l;
    int n;

    ST_CHECK_PARAM(line == NULL, NULL);

    glue = (glue_t *)malloc(sizeof(glue_t));
    if (glue == NULL) {
        ST_WARNING("Failed to malloc glue_t.");
        goto ERR;
    }
    memset(glue, 0, sizeof(glue_t));

    names = (char *)malloc(name_cap * MAX_LINE_LEN);
    if (names == NULL) {
        ST_WARNING("Failed to malloc names");
        goto ERR;
    }

    p = line;

    p = get_next_token(p, token);
    if (strcasecmp("glue", token) != 0) {
        ST_WARNING("Not glue line.");
        goto ERR;
    }

    reg_topo[0] = '\0';
    while (p != NULL) {
        p = get_next_token(p, token);
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

            reg = glue_get_reg(glue->type);
            if (reg == NULL) {
                ST_WARNING("Unkown type of glue [%s].",
                        glue->type);
                goto ERR;
            }
            if (reg->init(glue) < 0) {
                ST_WARNING("Failed to init reg glue.");
                goto ERR;
            }
        } else if (strcasecmp("in", keyvalue) == 0) {
            if (glue->num_in_layer != 0) {
                ST_WARNING("Duplicated in-layers.");
                goto ERR;
            }
            glue->num_in_layer = split_line(keyvalue + MAX_LINE_LEN, names,
                    name_cap, MAX_LINE_LEN, ",");
            while (glue->num_in_layer < 0) {
                name_cap += 16;
                names = (char *)realloc(names, name_cap * MAX_LINE_LEN);
                if (names == NULL) {
                    ST_WARNING("Failed to malloc names");
                    goto ERR;
                }
                glue->num_in_layer = split_line(keyvalue + MAX_LINE_LEN,
                        names, name_cap, MAX_LINE_LEN, ",");
            }
            glue->in_layers = (layer_id_t *)malloc(sizeof(layer_id_t)
                    * glue->num_in_layer);
            if (glue->in_layers == NULL) {
                ST_WARNING("Failed to malloc in_layers.");
                goto ERR;
            }
            for (l = 0; l < glue->num_in_layer; l++) {
                glue->in_layers[l] = glue_get_layer(layers, n_layer,
                        names + l*MAX_LINE_LEN);
                if (glue->in_layers[l] == LAYER_ID_NONE) {
                    ST_WARNING("No layer named [%s] is found.",
                            names + l*MAX_LINE_LEN);
                    goto ERR;
                }
            }
        } else if (strcasecmp("in-offset", keyvalue) == 0) {
            if (glue->num_in_layer <= 0) {
                ST_WARNING("in-offset must after in-layers.");
                goto ERR;
            }
            if (glue->in_offsets != NULL) {
                ST_WARNING("Duplicated in-offset.");
                goto ERR;
            }
            n = split_line(keyvalue + MAX_LINE_LEN,
                    names, name_cap, MAX_LINE_LEN, ",");
            while (n < 0) {
                name_cap += 16;
                names = (char *)realloc(names, name_cap * MAX_LINE_LEN);
                if (names == NULL) {
                    ST_WARNING("Failed to malloc names");
                    goto ERR;
                }
                n = split_line(keyvalue + MAX_LINE_LEN,
                        names, name_cap, MAX_LINE_LEN, ",");
            }
            if (n > glue->num_in_layer) {
                ST_WARNING("Too many in-offset [%s]", token);
                goto ERR;
            }
            glue->in_offsets = (glue_offset_t *)malloc(sizeof(glue_offset_t)
                    * glue->num_in_layer);
            if (glue->in_offsets == NULL) {
                ST_WARNING("Failed to malloc in_offsets.");
                goto ERR;
            }
            for (l = 0; l < n; l++) {
                glue->in_offsets[l] = atoi(names + l*MAX_LINE_LEN);
                if (glue->in_offsets[l] < 0) {
                    ST_WARNING("Invalid offset[%s].",
                            names+ l*MAX_LINE_LEN);
                    goto ERR;
                }
            }
            for (; l < glue->num_in_layer; l++) {
                glue->in_offsets[l] = 0;
            }
        } else if (strcasecmp("in-scale", keyvalue) == 0) {
            if (glue->num_in_layer <= 0) {
                ST_WARNING("in-scale must after in-layers.");
                goto ERR;
            }
            if (glue->in_scales != NULL) {
                ST_WARNING("Duplicated in-scale.");
                goto ERR;
            }
            n = split_line(keyvalue + MAX_LINE_LEN,
                    names, name_cap, MAX_LINE_LEN, ",");
            while (n < 0) {
                name_cap += 16;
                names = (char *)realloc(names, name_cap * MAX_LINE_LEN);
                if (names == NULL) {
                    ST_WARNING("Failed to malloc names");
                    goto ERR;
                }
                n = split_line(keyvalue + MAX_LINE_LEN,
                        names, name_cap, MAX_LINE_LEN, ",");
            }
            if (n > glue->num_in_layer) {
                ST_WARNING("Too many in-scale [%s]", token);
                goto ERR;
            }
            glue->in_scales = (real_t *)malloc(sizeof(real_t)
                    * glue->num_in_layer);
            if (glue->in_scales == NULL) {
                ST_WARNING("Failed to malloc in_scales.");
                goto ERR;
            }
            for (l = 0; l < n; l++) {
                glue->in_scales[l] = (real_t)atof(names + l*MAX_LINE_LEN);
            }
            for (; l < glue->num_in_layer; l++) {
                glue->in_scales[l] = (real_t)1.0;
            }
        } else if (strcasecmp("out", keyvalue) == 0) {
            if (glue->num_out_layer != 0) {
                ST_WARNING("Duplicated out-layers.");
                goto ERR;
            }
            glue->num_out_layer = split_line(keyvalue + MAX_LINE_LEN,
                    names, name_cap, MAX_LINE_LEN, ",");
            while (glue->num_out_layer < 0) {
                name_cap += 16;
                names = (char *)realloc(names, name_cap * MAX_LINE_LEN);
                if (names == NULL) {
                    ST_WARNING("Failed to malloc names");
                    goto ERR;
                }
                glue->num_out_layer = split_line(keyvalue + MAX_LINE_LEN,
                        names, name_cap, MAX_LINE_LEN, ",");
            }
            glue->out_layers = (layer_id_t *)malloc(sizeof(layer_id_t)
                    * glue->num_out_layer);
            if (glue->out_layers == NULL) {
                ST_WARNING("Failed to malloc out_layers.");
                goto ERR;
            }
            for (l = 0; l < glue->num_out_layer; l++) {
                glue->out_layers[l] = glue_get_layer(layers, n_layer,
                        names + l*MAX_LINE_LEN);
                if (glue->out_layers[l] == LAYER_ID_NONE) {
                    ST_WARNING("No layer named [%s] is found.",
                            names + l*MAX_LINE_LEN);
                    goto ERR;
                }
            }
        } else if (strcasecmp("out-offset", keyvalue) == 0) {
            if (glue->num_out_layer <= 0) {
                ST_WARNING("out-offset must after out-layers.");
                goto ERR;
            }
            if (glue->out_offsets != NULL) {
                ST_WARNING("Duplicated out-offset.");
                goto ERR;
            }
            n = split_line(keyvalue + MAX_LINE_LEN,
                    names, name_cap, MAX_LINE_LEN, ",");
            while (n < 0) {
                name_cap += 16;
                names = (char *)realloc(names, name_cap * MAX_LINE_LEN);
                if (names == NULL) {
                    ST_WARNING("Failed to malloc names");
                    goto ERR;
                }
                n = split_line(keyvalue + MAX_LINE_LEN,
                        names, name_cap, MAX_LINE_LEN, ",");
            }
            if (n > glue->num_out_layer) {
                ST_WARNING("Too many out-offset [%s]", token);
                goto ERR;
            }
            glue->out_offsets = (glue_offset_t *)malloc(sizeof(glue_offset_t)
                    * glue->num_out_layer);
            if (glue->out_offsets == NULL) {
                ST_WARNING("Failed to malloc out_offsets.");
                goto ERR;
            }
            for (l = 0; l < n; l++) {
                glue->out_offsets[l] = atoi(names + l*MAX_LINE_LEN);
                if (glue->out_offsets[l] < 0) {
                    ST_WARNING("Invalid offset[%s].",
                            names + l*MAX_LINE_LEN);
                    goto ERR;
                }
            }
            for (; l < glue->num_out_layer; l++) {
                glue->out_offsets[l] = 0;
            }
        } else if (strcasecmp("out-scale", keyvalue) == 0) {
            if (glue->num_out_layer <= 0) {
                ST_WARNING("out-scale must after out-layers.");
                goto ERR;
            }
            if (glue->out_scales != NULL) {
                ST_WARNING("Duplicated out-scale.");
                goto ERR;
            }
            n = split_line(keyvalue + MAX_LINE_LEN,
                    names, name_cap, MAX_LINE_LEN, ",");
            while (n < 0) {
                name_cap += 16;
                names = (char *)realloc(names, name_cap * MAX_LINE_LEN);
                if (names == NULL) {
                    ST_WARNING("Failed to malloc names");
                    goto ERR;
                }
                n = split_line(keyvalue + MAX_LINE_LEN,
                        names, name_cap, MAX_LINE_LEN, ",");
            }
            if (n > glue->num_out_layer) {
                ST_WARNING("Too many out-scale [%s]", token);
                goto ERR;
            }
            glue->out_scales = (real_t *)malloc(sizeof(real_t)
                    * glue->num_out_layer);
            if (glue->out_scales == NULL) {
                ST_WARNING("Failed to malloc out_scales.");
                goto ERR;
            }
            for (l = 0; l < n; l++) {
                glue->out_scales[l] = (real_t)atof(names + l*MAX_LINE_LEN);
            }
            for (; l < glue->num_out_layer; l++) {
                glue->out_scales[l] = (real_t)1.0;
            }
        } else {
            strncpy(reg_topo + strlen(reg_topo), token,
                    MAX_LINE_LEN - strlen(reg_topo));
            if (strlen(reg_topo) < MAX_LINE_LEN) {
                reg_topo[strlen(reg_topo)] = ' ';
            }
            reg_topo[MAX_LINE_LEN - 1] = '\0';
        }
    }

    if (glue->name[0] == '\0') {
        ST_WARNING("No glue name found.");
        goto ERR;
    }

    if (!glue_check(glue)) {
        ST_WARNING("Failed to glue_check.");
        return false;
    }

    if (reg == NULL) {
        ST_WARNING("No type found.");
        goto ERR;
    }
    if (!reg->check(glue, layers, n_layer)) {
        ST_WARNING("check glue failed.");
        goto ERR;
    }
    if (reg->parse_topo(glue, reg_topo) < 0) {
        ST_WARNING("Failed to parse_topo for reg glue.");
        goto ERR;
    }

    safe_free(names);
    return glue;

ERR:
    safe_free(names);
    safe_glue_destroy(glue);
    return NULL;
}

glue_t* glue_dup(glue_t *g)
{
    glue_t *glue = NULL;
    size_t sz;

    ST_CHECK_PARAM(g == NULL, NULL);

    glue = (glue_t *) malloc(sizeof(glue_t));
    if (glue == NULL) {
        ST_WARNING("Falied to malloc glue_t.");
        goto ERR;
    }
    memset(glue, 0, sizeof(glue_t));

    strncpy(glue->name, g->name, MAX_NAME_LEN);
    glue->name[MAX_NAME_LEN - 1] = '\0';
    strncpy(glue->type, g->type, MAX_NAME_LEN);
    glue->type[MAX_NAME_LEN - 1] = '\0';

    if (glue->num_in_layer > 0) {
        sz = sizeof(layer_id_t)*glue->num_in_layer;
        glue->in_layers = (layer_id_t *)malloc(sz);
        if (glue->in_layers == NULL) {
            ST_WARNING("Failed to malloc in_layers");
            goto ERR;
        }
        memcpy(glue->in_layers, g->in_layers, sz);

        sz = sizeof(glue_offset_t)*glue->num_in_layer;
        glue->in_offsets = (glue_offset_t *)malloc(sz);
        if (glue->in_offsets == NULL) {
            ST_WARNING("Failed to malloc in_offsets");
            goto ERR;
        }
        memcpy(glue->in_offsets, g->in_offsets, sz);

        sz = sizeof(real_t)*glue->num_in_layer;
        glue->in_scales = (real_t *)malloc(sz);
        if (glue->in_scales == NULL) {
            ST_WARNING("Failed to malloc in_scales");
            goto ERR;
        }
        memcpy(glue->in_scales, g->in_scales, sz);
    }

    if (glue->num_out_layer > 0) {
        sz = sizeof(layer_id_t)*glue->num_out_layer;
        glue->out_layers = (layer_id_t *)malloc(sz);
        if (glue->out_layers == NULL) {
            ST_WARNING("Failed to malloc out_layers");
            goto ERR;
        }
        memcpy(glue->out_layers, g->out_layers, sz);

        sz = sizeof(glue_offset_t)*glue->num_out_layer;
        glue->out_offsets = (glue_offset_t *)malloc(sz);
        if (glue->out_offsets == NULL) {
            ST_WARNING("Failed to malloc out_offsets");
            goto ERR;
        }
        memcpy(glue->out_offsets, g->out_offsets, sz);

        sz = sizeof(real_t)*glue->num_out_layer;
        glue->out_scales = (real_t *)malloc(sz);
        if (glue->out_scales == NULL) {
            ST_WARNING("Failed to malloc out_scales");
            goto ERR;
        }
        memcpy(glue->out_scales, g->out_scales, sz);
    }
    glue->recur = g->recur;

    glue->forward = g->forward;
    glue->backprop = g->backprop;

    return glue;

ERR:
    safe_glue_destroy(glue);
    return NULL;
}

int glue_load_header(glue_t **glue, int version,
        FILE *fp, bool *binary, FILE *fo_info)
{
    union {
        char str[4];
        int magic_num;
    } flag;

    char name[MAX_NAME_LEN];
    char type[MAX_NAME_LEN];
    layer_id_t num_in_layer;
    layer_id_t num_out_layer;

    glue_reg_t *reg;
    bool b;

    ST_CHECK_PARAM((glue == NULL && fo_info == NULL) || fp == NULL
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
    } else if (GLUE_MAGIC_NUM != flag.magic_num) {
        ST_WARNING("magic num wrong.");
        return -2;
    } else {
        *binary = true;
    }

    if (glue != NULL) {
        *glue = NULL;
    }

    if (*binary) {
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
        if (fread(&num_in_layer, sizeof(layer_id_t), 1, fp) != 1) {
            ST_WARNING("Failed to read num_in_layer.");
            return -1;
        }
        if (fread(&num_out_layer, sizeof(layer_id_t), 1, fp) != 1) {
            ST_WARNING("Failed to read num_out_layer.");
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

        if (st_readline(fp, "Num in layer: "LAYER_ID_FMT,
                    &num_in_layer) != 1) {
            ST_WARNING("Failed to parse num_in_layer.");
            goto ERR;
        }
        if (st_readline(fp, "Num out layer: "LAYER_ID_FMT,
                    &num_out_layer) != 1) {
            ST_WARNING("Failed to parse num_out_layer.");
            goto ERR;
        }
    }

    if (glue != NULL) {
        *glue = (glue_t *)malloc(sizeof(glue_t));
        if (*glue == NULL) {
            ST_WARNING("Failed to malloc glue_t");
            goto ERR;
        }
        memset(*glue, 0, sizeof(glue_t));

        strcpy((*glue)->name, name);
        strcpy((*glue)->type, type);
        (*glue)->num_in_layer = num_in_layer;
        (*glue)->num_out_layer = num_out_layer;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<GLUE>\n");
        fprintf(fo_info, "Name: %s\n", name);
        fprintf(fo_info, "Type: %s\n", type);
        fprintf(fo_info, "Num in layer: "LAYER_ID_FMT"\n", num_in_layer);
        fprintf(fo_info, "Num out layer: "LAYER_ID_FMT"\n", num_out_layer);
    }

    reg = glue_get_reg(type);
    if (reg->load_header != NULL) {
        if (reg->load_header(glue != NULL ? &((*glue)->extra) : NULL,
                    version, fp, &b, fo_info) < 0) {
            ST_WARNING("Failed to reg->load_header.");
            goto ERR;
        }

        if (b != *binary) {
            ST_WARNING("Tree binary not match with output binary");
            goto ERR;
        }
    }

    return 0;

ERR:
    if (glue != NULL) {
        safe_glue_destroy(*glue);
    }
    return -1;
}

int glue_load_body(glue_t *glue, int version, FILE *fp, bool binary)
{
    size_t sz;
    int n;
    layer_id_t l;
    layer_id_t tmp;

    glue_reg_t *reg;

    ST_CHECK_PARAM(glue == NULL || fp == NULL, -1);

    if (version < 3) {
        ST_WARNING("Too old version of connlm file");
        return -1;
    }

    safe_free(glue->in_layers);
    safe_free(glue->in_offsets);
    safe_free(glue->in_scales);
    if (glue->num_in_layer > 0) {
        sz = sizeof(layer_id_t)*glue->num_in_layer;
        glue->in_layers = (layer_id_t *)malloc(sz);
        if (glue->in_layers == NULL) {
            ST_WARNING("Failed to malloc in_layers");
            goto ERR;
        }

        sz = sizeof(glue_offset_t)*glue->num_in_layer;
        glue->in_offsets = (glue_offset_t *)malloc(sz);
        if (glue->in_offsets == NULL) {
            ST_WARNING("Failed to malloc in_offsets");
            goto ERR;
        }

        sz = sizeof(real_t)*glue->num_in_layer;
        glue->in_scales = (real_t *)malloc(sz);
        if (glue->in_scales == NULL) {
            ST_WARNING("Failed to malloc in_scales");
            goto ERR;
        }
    }

    safe_free(glue->out_layers);
    safe_free(glue->out_offsets);
    safe_free(glue->out_scales);
    if (glue->num_out_layer > 0) {
        sz = sizeof(layer_id_t)*glue->num_out_layer;
        glue->out_layers = (layer_id_t *)malloc(sz);
        if (glue->out_layers == NULL) {
            ST_WARNING("Failed to malloc out_layers");
            goto ERR;
        }

        sz = sizeof(glue_offset_t)*glue->num_out_layer;
        glue->out_offsets = (glue_offset_t *)malloc(sz);
        if (glue->out_offsets == NULL) {
            ST_WARNING("Failed to malloc out_offsets");
            goto ERR;
        }

        sz = sizeof(real_t)*glue->num_out_layer;
        glue->out_scales = (real_t *)malloc(sz);
        if (glue->out_scales == NULL) {
            ST_WARNING("Failed to malloc out_scales");
            goto ERR;
        }
    }

    if (binary) {
        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read magic num.");
            goto ERR;
        }

        if (n != -GLUE_MAGIC_NUM) {
            ST_WARNING("Magic num error.");
            goto ERR;
        }

        if (glue->num_in_layer > 0){
            if (fread(glue->in_layers, sizeof(layer_id_t),
                        glue->num_in_layer, fp) != glue->num_in_layer) {
                ST_WARNING("Failed to read in_layers.");
                goto ERR;
            }
            if (fread(glue->in_offsets, sizeof(glue_offset_t),
                        glue->num_in_layer, fp) != glue->num_in_layer) {
                ST_WARNING("Failed to read in_offsets.");
                goto ERR;
            }
            if (fread(glue->in_scales, sizeof(real_t),
                        glue->num_in_layer, fp) != glue->num_in_layer) {
                ST_WARNING("Failed to read in_scales.");
                goto ERR;
            }
        }

        if (glue->num_out_layer > 0){
            if (fread(glue->out_layers, sizeof(layer_id_t),
                        glue->num_out_layer, fp) != glue->num_out_layer) {
                ST_WARNING("Failed to read out_layers.");
                goto ERR;
            }
            if (fread(glue->out_offsets, sizeof(glue_offset_t),
                        glue->num_out_layer, fp) != glue->num_out_layer) {
                ST_WARNING("Failed to read out_offsets.");
                goto ERR;
            }
            if (fread(glue->out_scales, sizeof(real_t),
                        glue->num_out_layer, fp) != glue->num_out_layer) {
                ST_WARNING("Failed to read out_scales.");
                goto ERR;
            }
        }
    } else {
        if (st_readline(fp, "<GLUE-DATA>") != 0) {
            ST_WARNING("body flag error.");
            goto ERR;
        }

        if (glue->num_in_layer > 0){
            if (st_readline(fp, "In layers:") != 0) {
                ST_WARNING("in_layers flag error.");
                goto ERR;
            }
            for (l = 0; l < glue->num_in_layer; l++) {
                if (st_readline(fp, "\t"LAYER_ID_FMT"\t"LAYER_ID_FMT,
                            &tmp, glue->in_layers + l) != 2) {
                    ST_WARNING("Failed to parse in_layers.");
                    goto ERR;
                }
            }
            if (st_readline(fp, "In offsets:") != 0) {
                ST_WARNING("in_offsets flag error.");
                goto ERR;
            }
            for (l = 0; l < glue->num_in_layer; l++) {
                if (st_readline(fp, "\t"LAYER_ID_FMT"\t"GLUE_OFFSET_FMT,
                            &tmp, glue->in_offsets + l) != 2) {
                    ST_WARNING("Failed to parse in_offsets.");
                    goto ERR;
                }
            }
            if (st_readline(fp, "In scales:") != 0) {
                ST_WARNING("in_scales flag error.");
                goto ERR;
            }
            for (l = 0; l < glue->num_in_layer; l++) {
                if (st_readline(fp, "\t"LAYER_ID_FMT"\t"REAL_FMT,
                            &tmp, glue->in_scales + l) != 2) {
                    ST_WARNING("Failed to parse in_scales.");
                    goto ERR;
                }
            }
        }

        if (glue->num_out_layer > 0){
            if (st_readline(fp, "Out layers:") != 0) {
                ST_WARNING("out_layers flag error.");
                goto ERR;
            }
            for (l = 0; l < glue->num_out_layer; l++) {
                if (st_readline(fp, "\t"LAYER_ID_FMT"\t"LAYER_ID_FMT,
                            &tmp, glue->out_layers + l) != 2) {
                    ST_WARNING("Failed to parse out_layers.");
                    goto ERR;
                }
            }
            if (st_readline(fp, "Out offsets:") != 0) {
                ST_WARNING("out_offsets flag error.");
                goto ERR;
            }
            for (l = 0; l < glue->num_out_layer; l++) {
                if (st_readline(fp, "\t"LAYER_ID_FMT"\t"GLUE_OFFSET_FMT,
                            &tmp, glue->out_offsets + l) != 2) {
                    ST_WARNING("Failed to parse out_offsets.");
                    goto ERR;
                }
            }
            if (st_readline(fp, "Out scales:") != 0) {
                ST_WARNING("out_scales flag error.");
                goto ERR;
            }
            for (l = 0; l < glue->num_out_layer; l++) {
                if (st_readline(fp, "\t"LAYER_ID_FMT"\t"REAL_FMT,
                            &tmp, glue->out_scales + l) != 2) {
                    ST_WARNING("Failed to parse out_scales.");
                    goto ERR;
                }
            }
        }
    }

    reg = glue_get_reg(glue->type);
    if (reg->load_body != NULL) {
        if (reg->load_body(glue->extra, version, fp, binary) < 0) {
            ST_WARNING("Failed to reg->load_body.");
            goto ERR;
        }
    }

    return 0;
ERR:

    safe_free(glue->in_layers);
    safe_free(glue->in_offsets);
    safe_free(glue->in_scales);

    safe_free(glue->out_layers);
    safe_free(glue->out_offsets);
    safe_free(glue->out_scales);

    return -1;
}

int glue_save_header(glue_t *glue, FILE *fp, bool binary)
{
    int n;

    glue_reg_t *reg;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (binary) {
        if (fwrite(&GLUE_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
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
        if (fwrite(&glue->num_in_layer, sizeof(layer_id_t), 1, fp) != 1) {
            ST_WARNING("Failed to write num_in_layer.");
            return -1;
        }
        if (fwrite(&glue->num_out_layer, sizeof(layer_id_t), 1, fp) != 1) {
            ST_WARNING("Failed to write num_out_layer.");
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
        if (fprintf(fp, "Num in layer: "LAYER_ID_FMT"\n",
                    glue->num_in_layer) < 0) {
            ST_WARNING("Failed to fprintf num_in_layer.");
            return -1;
        }
        if (fprintf(fp, "Num out layer: "LAYER_ID_FMT"\n",
                    glue->num_out_layer) < 0) {
            ST_WARNING("Failed to fprintf num_out_layer.");
            return -1;
        }
    }

    reg = glue_get_reg(glue->type);
    if (reg->save_header != NULL) {
        if (reg->save_header(glue->extra, fp, binary) < 0) {
            ST_WARNING("Failed to reg->save_header.");
            return -1;
        }
    }

    return 0;
}

int glue_save_body(glue_t *glue, FILE *fp, bool binary)
{
    int n;
    layer_id_t l;

    glue_reg_t *reg;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (binary) {
        n = -GLUE_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        if (glue->num_in_layer > 0){
            if (fwrite(glue->in_layers, sizeof(layer_id_t),
                        glue->num_in_layer, fp) != glue->num_in_layer) {
                ST_WARNING("Failed to write in_layers.");
                return -1;
            }
            if (fwrite(glue->in_offsets, sizeof(glue_offset_t),
                        glue->num_in_layer, fp) != glue->num_in_layer) {
                ST_WARNING("Failed to write in_offsets.");
                return -1;
            }
            if (fwrite(glue->in_scales, sizeof(real_t),
                        glue->num_in_layer, fp) != glue->num_in_layer) {
                ST_WARNING("Failed to write in_scales.");
                return -1;
            }
        }

        if (glue->num_out_layer > 0){
            if (fwrite(glue->out_layers, sizeof(layer_id_t),
                        glue->num_out_layer, fp) != glue->num_out_layer) {
                ST_WARNING("Failed to write out_layers.");
                return -1;
            }
            if (fwrite(glue->out_offsets, sizeof(glue_offset_t),
                        glue->num_out_layer, fp) != glue->num_out_layer) {
                ST_WARNING("Failed to write out_offsets.");
                return -1;
            }
            if (fwrite(glue->out_scales, sizeof(real_t),
                        glue->num_out_layer, fp) != glue->num_out_layer) {
                ST_WARNING("Failed to write out_scales.");
                return -1;
            }
        }
    } else {
        if (fprintf(fp, "<GLUE-DATA>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }

        if (glue->num_in_layer > 0){
            if (fprintf(fp, "In layers:\n") < 0) {
                ST_WARNING("Failed to fprintf in_layers.");
                return -1;
            }
            for (l = 0; l < glue->num_in_layer; l++) {
                if (fprintf(fp, "\t"LAYER_ID_FMT"\t"LAYER_ID_FMT"\n",
                            l, glue->in_layers[l]) < 0) {
                    ST_WARNING("Failed to fprintf in_layers["
                            LAYER_ID_FMT"].", l);
                    return -1;
                }
            }
            if (fprintf(fp, "In offsets:\n") < 0) {
                ST_WARNING("Failed to fprintf in_offsets.");
                return -1;
            }
            for (l = 0; l < glue->num_in_layer; l++) {
                if (fprintf(fp, "\t"LAYER_ID_FMT"\t"GLUE_OFFSET_FMT"\n",
                            l, glue->in_offsets[l]) < 0) {
                    ST_WARNING("Failed to fprintf in_offsets["
                            LAYER_ID_FMT"].", l);
                    return -1;
                }
            }
            if (fprintf(fp, "In scales:\n") < 0) {
                ST_WARNING("Failed to fprintf in_scales.");
                return -1;
            }
            for (l = 0; l < glue->num_in_layer; l++) {
                if (fprintf(fp, "\t"LAYER_ID_FMT"\t"REAL_FMT"\n",
                            l, glue->in_scales[l]) < 0) {
                    ST_WARNING("Failed to fprintf in_scales["
                            LAYER_ID_FMT"].", l);
                    return -1;
                }
            }
        }

        if (glue->num_out_layer > 0){
            if (fprintf(fp, "Out layers:\n") < 0) {
                ST_WARNING("Failed to fprintf out_layers.");
                return -1;
            }
            for (l = 0; l < glue->num_out_layer; l++) {
                if (fprintf(fp, "\t"LAYER_ID_FMT"\t"LAYER_ID_FMT"\n",
                            l, glue->out_layers[l]) < 0) {
                    ST_WARNING("Failed to fprintf out_layers["
                            LAYER_ID_FMT"].", l);
                    return -1;
                }
            }
            if (fprintf(fp, "Out offsets:\n") < 0) {
                ST_WARNING("Failed to fprintf out_offsets.");
                return -1;
            }
            for (l = 0; l < glue->num_out_layer; l++) {
                if (fprintf(fp, "\t"LAYER_ID_FMT"\t"GLUE_OFFSET_FMT"\n",
                            l, glue->out_offsets[l]) < 0) {
                    ST_WARNING("Failed to fprintf out_offsets["
                            LAYER_ID_FMT"].", l);
                    return -1;
                }
            }
            if (fprintf(fp, "Out scales:\n") < 0) {
                ST_WARNING("Failed to fprintf out_scales.");
                return -1;
            }
            for (l = 0; l < glue->num_out_layer; l++) {
                if (fprintf(fp, "\t"LAYER_ID_FMT"\t"REAL_FMT"\n",
                            l, glue->out_scales[l]) < 0) {
                    ST_WARNING("Failed to fprintf out_scales["
                            LAYER_ID_FMT"].", l);
                    return -1;
                }
            }
        }
    }

    reg = glue_get_reg(glue->type);
    if (reg->save_body != NULL) {
        if (reg->save_body(glue->extra, fp, binary) < 0) {
            ST_WARNING("Failed to reg->save_body.");
            return -1;
        }
    }

    return 0;
}

char* glue_draw_label(glue_t *glue, char *label, size_t label_len,
        bool verbose)
{
    char buf[MAX_LINE_LEN];
    glue_reg_t *reg;

    ST_CHECK_PARAM(glue == NULL || label == NULL, NULL);

    reg = glue_get_reg(glue->type);
    if (verbose) {
        snprintf(label, label_len, "%s/type=%s", glue->name, glue->type);
        strncat(label, reg->draw_label(glue, buf, MAX_LINE_LEN),
                label_len - strlen(label) - 1);
    } else {
        snprintf(label, label_len, "%s", glue->name);
    }

    return label;
}

char* glue_draw_label_one(glue_t *glue, layer_id_t lid,
        char *label, size_t label_len, bool verbose)
{
    char buf[MAX_LINE_LEN];

    ST_CHECK_PARAM(glue == NULL || label == NULL, NULL);

    label[0] = '\0';
    if (glue->in_offsets != NULL && glue->in_offsets[lid] != 0) {
        snprintf(buf, MAX_LINE_LEN, ",off="GLUE_OFFSET_FMT,
                glue->in_offsets[lid]);
        strncat(label, buf, label_len - strlen(label) - 1);
    }
    if (glue->in_offsets != NULL && glue->in_scales[lid] != 1.0) {
        snprintf(buf, MAX_LINE_LEN, ",scale="REAL_FMT,
                glue->in_scales[lid]);
        strncat(label, buf, label_len - strlen(label) - 1);
    }

    return label;
}

int glue_init_data(glue_t *glue, input_t *input,
        layer_t **layers, output_t *output)
{
    glue_reg_t *reg = NULL;

    reg = glue_get_reg(glue->type);
    if (reg == NULL) {
        ST_WARNING("Unkown type of glue [%s].", glue->type);
        return -1;
    }

    if (reg->init_data != NULL &&
            reg->init_data(glue, input, layers, output) < 0) {
        ST_WARNING("Failed to init_data for reg glue.");
        return -1;
    }

    return 0;
}
