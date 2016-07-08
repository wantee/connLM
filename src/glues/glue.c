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

static glue_impl_t GLUE_IMPL[] = {
    {SUM_GLUE_NAME, sum_glue_init, sum_glue_destroy,
        sum_glue_dup, sum_glue_parse_topo, sum_glue_check,
        sum_glue_draw_label, NULL, NULL, NULL, NULL, NULL},
    {APPEND_GLUE_NAME, append_glue_init, append_glue_destroy,
        append_glue_dup, append_glue_parse_topo, append_glue_check,
        append_glue_draw_label, NULL, NULL, NULL, NULL, NULL},
    {CLONE_GLUE_NAME, clone_glue_init, clone_glue_destroy,
        clone_glue_dup, clone_glue_parse_topo, clone_glue_check,
        clone_glue_draw_label, NULL, NULL, NULL, NULL, NULL},
    {DIRECT_GLUE_NAME, direct_glue_init, direct_glue_destroy,
        direct_glue_dup, direct_glue_parse_topo, direct_glue_check,
        direct_glue_draw_label, direct_glue_load_header,
        direct_glue_load_body, direct_glue_save_header,
        direct_glue_save_body, direct_glue_init_data,
        direct_glue_load_train_opt, direct_glue_forward},
    {WT_GLUE_NAME, wt_glue_init, wt_glue_destroy,
        wt_glue_dup, wt_glue_parse_topo, wt_glue_check,
        wt_glue_draw_label, wt_glue_load_header, wt_glue_load_body,
        wt_glue_save_header, wt_glue_save_body,
        wt_glue_init_data, wt_glue_load_train_opt},
    {EMB_WT_GLUE_NAME, emb_wt_glue_init, emb_wt_glue_destroy,
        emb_wt_glue_dup, emb_wt_glue_parse_topo, emb_wt_glue_check,
        emb_wt_glue_draw_label, emb_wt_glue_load_header,
        emb_wt_glue_load_body, emb_wt_glue_save_header,
        emb_wt_glue_save_body, emb_wt_glue_init_data,
        emb_wt_glue_load_train_opt},
    {OUT_WT_GLUE_NAME, out_wt_glue_init, out_wt_glue_destroy,
        out_wt_glue_dup, out_wt_glue_parse_topo, out_wt_glue_check,
        out_wt_glue_draw_label, out_wt_glue_load_header,
        out_wt_glue_load_body, out_wt_glue_save_header,
        out_wt_glue_save_body, out_wt_glue_init_data,
        out_wt_glue_load_train_opt},
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

static int glue_get_layer(layer_t **layers, int n_layer,
        const char *name)
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
    if (glue == NULL) {
        return;
    }

    glue->name[0] = '\0';

    if (glue->impl != NULL && glue->impl->destroy != NULL) {
        glue->impl->destroy(glue);
    }
    glue->type[0] = '\0';
    glue->extra = NULL;

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
    int l;

    ST_CHECK_PARAM(glue == NULL, false);

    if (glue->num_in_layer <= 0) {
        ST_WARNING("No in layers found.");
        goto ERR;
    }
    if (glue->in_offsets == NULL) {
        glue->in_offsets = (int *)malloc(sizeof(int)
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
        glue->out_offsets = (int *)malloc(sizeof(int)
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
        int n_layer, input_t *input, output_t *output)
{
    glue_t *glue = NULL;

    char keyvalue[2*MAX_LINE_LEN];
    char token[MAX_LINE_LEN];
    char impl_topo[MAX_LINE_LEN];
    char *names = NULL;
    size_t name_cap = 16;

    const char *p;
    int l;
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

    impl_topo[0] = '\0';
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

            glue->impl = glue_get_impl(glue->type);
            if (glue->impl == NULL) {
                ST_WARNING("Unknown type of glue [%s].", glue->type);
                goto ERR;
            }
            if (glue->impl->init(glue) < 0) {
                ST_WARNING("Failed to init impl glue.");
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
            glue->in_layers = (int *)malloc(sizeof(int)
                    * glue->num_in_layer);
            if (glue->in_layers == NULL) {
                ST_WARNING("Failed to malloc in_layers.");
                goto ERR;
            }
            for (l = 0; l < glue->num_in_layer; l++) {
                glue->in_layers[l] = glue_get_layer(layers, n_layer,
                        names + l*MAX_LINE_LEN);
                if (glue->in_layers[l] < 0) {
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
            glue->in_offsets = (int *)malloc(sizeof(int)
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
            glue->out_layers = (int *)malloc(sizeof(int)
                    * glue->num_out_layer);
            if (glue->out_layers == NULL) {
                ST_WARNING("Failed to malloc out_layers.");
                goto ERR;
            }
            for (l = 0; l < glue->num_out_layer; l++) {
                glue->out_layers[l] = glue_get_layer(layers, n_layer,
                        names + l*MAX_LINE_LEN);
                if (glue->out_layers[l] < 0) {
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
            glue->out_offsets = (int *)malloc(sizeof(int)
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
            strncpy(impl_topo + strlen(impl_topo), token,
                    MAX_LINE_LEN - strlen(impl_topo));
            if (strlen(impl_topo) < MAX_LINE_LEN) {
                impl_topo[strlen(impl_topo)] = ' ';
            }
            impl_topo[MAX_LINE_LEN - 1] = '\0';
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

    if (glue->impl == NULL) {
        ST_WARNING("No type found.");
        goto ERR;
    }
    if (glue->impl->check != NULL) {
        if (!glue->impl->check(glue, layers, n_layer, input, output)) {
            ST_WARNING("check glue failed.");
            goto ERR;
        }
    }
    if (glue->impl->parse_topo != NULL) {
        if (glue->impl->parse_topo(glue, impl_topo) < 0) {
            ST_WARNING("Failed to parse_topo for impl glue.");
            goto ERR;
        }
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
        sz = sizeof(int)*glue->num_in_layer;
        glue->in_layers = (int *)malloc(sz);
        if (glue->in_layers == NULL) {
            ST_WARNING("Failed to malloc in_layers");
            goto ERR;
        }
        memcpy(glue->in_layers, g->in_layers, sz);

        sz = sizeof(int)*glue->num_in_layer;
        glue->in_offsets = (int *)malloc(sz);
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
        sz = sizeof(int)*glue->num_out_layer;
        glue->out_layers = (int *)malloc(sz);
        if (glue->out_layers == NULL) {
            ST_WARNING("Failed to malloc out_layers");
            goto ERR;
        }
        memcpy(glue->out_layers, g->out_layers, sz);

        sz = sizeof(int)*glue->num_out_layer;
        glue->out_offsets = (int *)malloc(sz);
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
    int num_in_layer;
    int num_out_layer;

    glue_impl_t *impl;
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
        if (fread(&num_in_layer, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read num_in_layer.");
            return -1;
        }
        if (fread(&num_out_layer, sizeof(int), 1, fp) != 1) {
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

        if (st_readline(fp, "Num in layer: %d",
                    &num_in_layer) != 1) {
            ST_WARNING("Failed to parse num_in_layer.");
            goto ERR;
        }
        if (st_readline(fp, "Num out layer: %d",
                    &num_out_layer) != 1) {
            ST_WARNING("Failed to parse num_out_layer.");
            goto ERR;
        }
    }

    impl = glue_get_impl(type);
    if (impl == NULL) {
        ST_WARNING("Unknown glue type[%s].", type);
        goto ERR;
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
        (*glue)->impl = impl;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<GLUE>\n");
        fprintf(fo_info, "Name: %s\n", name);
        fprintf(fo_info, "Type: %s\n", type);
        fprintf(fo_info, "Num in layer: %d\n", num_in_layer);
        fprintf(fo_info, "Num out layer: %d\n", num_out_layer);
    }

    if (impl->load_header != NULL) {
        if (impl->load_header(glue != NULL ? &((*glue)->extra) : NULL,
                version, fp, &b, fo_info) < 0) {
            ST_WARNING("Failed to impl->load_header.");
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
    int l;
    int tmp;

    ST_CHECK_PARAM(glue == NULL || fp == NULL, -1);

    if (version < 3) {
        ST_WARNING("Too old version of connlm file");
        return -1;
    }

    safe_free(glue->in_layers);
    safe_free(glue->in_offsets);
    safe_free(glue->in_scales);
    if (glue->num_in_layer > 0) {
        sz = sizeof(int)*glue->num_in_layer;
        glue->in_layers = (int *)malloc(sz);
        if (glue->in_layers == NULL) {
            ST_WARNING("Failed to malloc in_layers");
            goto ERR;
        }

        sz = sizeof(int)*glue->num_in_layer;
        glue->in_offsets = (int *)malloc(sz);
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
        sz = sizeof(int)*glue->num_out_layer;
        glue->out_layers = (int *)malloc(sz);
        if (glue->out_layers == NULL) {
            ST_WARNING("Failed to malloc out_layers");
            goto ERR;
        }

        sz = sizeof(int)*glue->num_out_layer;
        glue->out_offsets = (int *)malloc(sz);
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
            if (fread(glue->in_layers, sizeof(int),
                        glue->num_in_layer, fp) != glue->num_in_layer) {
                ST_WARNING("Failed to read in_layers.");
                goto ERR;
            }
            if (fread(glue->in_offsets, sizeof(int),
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
            if (fread(glue->out_layers, sizeof(int),
                        glue->num_out_layer, fp) != glue->num_out_layer) {
                ST_WARNING("Failed to read out_layers.");
                goto ERR;
            }
            if (fread(glue->out_offsets, sizeof(int),
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
                if (st_readline(fp, "\t%d\t%d",
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
                if (st_readline(fp, "\t%d\t%d",
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
                if (st_readline(fp, "\t%d\t"REAL_FMT,
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
                if (st_readline(fp, "\t%d\t%d",
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
                if (st_readline(fp, "\t%d\t%d",
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
                if (st_readline(fp, "\t%d\t"REAL_FMT,
                            &tmp, glue->out_scales + l) != 2) {
                    ST_WARNING("Failed to parse out_scales.");
                    goto ERR;
                }
            }
        }
    }

    if (glue->impl->load_body != NULL) {
        if (glue->impl->load_body(glue->extra, version, fp, binary) < 0) {
            ST_WARNING("Failed to glue->impl->load_body.");
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
        if (fwrite(&glue->num_in_layer, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write num_in_layer.");
            return -1;
        }
        if (fwrite(&glue->num_out_layer, sizeof(int), 1, fp) != 1) {
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
        if (fprintf(fp, "Num in layer: %d\n",
                    glue->num_in_layer) < 0) {
            ST_WARNING("Failed to fprintf num_in_layer.");
            return -1;
        }
        if (fprintf(fp, "Num out layer: %d\n",
                    glue->num_out_layer) < 0) {
            ST_WARNING("Failed to fprintf num_out_layer.");
            return -1;
        }
    }

    if (glue->impl != NULL && glue->impl->save_header != NULL) {
        if (glue->impl->save_header(glue->extra, fp, binary) < 0) {
            ST_WARNING("Failed to glue->impl->save_header.");
            return -1;
        }
    }

    return 0;
}

int glue_save_body(glue_t *glue, FILE *fp, bool binary)
{
    int n;
    int l;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (binary) {
        n = -GLUE_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        if (glue->num_in_layer > 0){
            if (fwrite(glue->in_layers, sizeof(int),
                        glue->num_in_layer, fp) != glue->num_in_layer) {
                ST_WARNING("Failed to write in_layers.");
                return -1;
            }
            if (fwrite(glue->in_offsets, sizeof(int),
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
            if (fwrite(glue->out_layers, sizeof(int),
                        glue->num_out_layer, fp) != glue->num_out_layer) {
                ST_WARNING("Failed to write out_layers.");
                return -1;
            }
            if (fwrite(glue->out_offsets, sizeof(int),
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
                if (fprintf(fp, "\t%d\t%d\n",
                            l, glue->in_layers[l]) < 0) {
                    ST_WARNING("Failed to fprintf in_layers[%d].", l);
                    return -1;
                }
            }
            if (fprintf(fp, "In offsets:\n") < 0) {
                ST_WARNING("Failed to fprintf in_offsets.");
                return -1;
            }
            for (l = 0; l < glue->num_in_layer; l++) {
                if (fprintf(fp, "\t%d\t%d\n",
                            l, glue->in_offsets[l]) < 0) {
                    ST_WARNING("Failed to fprintf in_offsets[%d].", l);
                    return -1;
                }
            }
            if (fprintf(fp, "In scales:\n") < 0) {
                ST_WARNING("Failed to fprintf in_scales.");
                return -1;
            }
            for (l = 0; l < glue->num_in_layer; l++) {
                if (fprintf(fp, "\t%d\t"REAL_FMT"\n",
                            l, glue->in_scales[l]) < 0) {
                    ST_WARNING("Failed to fprintf in_scales[%d].", l);
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
                if (fprintf(fp, "\t%d\t%d\n",
                            l, glue->out_layers[l]) < 0) {
                    ST_WARNING("Failed to fprintf out_layers[%d].", l);
                    return -1;
                }
            }
            if (fprintf(fp, "Out offsets:\n") < 0) {
                ST_WARNING("Failed to fprintf out_offsets.");
                return -1;
            }
            for (l = 0; l < glue->num_out_layer; l++) {
                if (fprintf(fp, "\t%d\t%d\n",
                            l, glue->out_offsets[l]) < 0) {
                    ST_WARNING("Failed to fprintf out_offsets[%d].", l);
                    return -1;
                }
            }
            if (fprintf(fp, "Out scales:\n") < 0) {
                ST_WARNING("Failed to fprintf out_scales.");
                return -1;
            }
            for (l = 0; l < glue->num_out_layer; l++) {
                if (fprintf(fp, "\t%d\t"REAL_FMT"\n",
                            l, glue->out_scales[l]) < 0) {
                    ST_WARNING("Failed to fprintf out_scales[%d].", l);
                    return -1;
                }
            }
        }
    }

    if (glue->impl != NULL && glue->impl->save_body != NULL) {
        if (glue->impl->save_body(glue->extra, fp, binary) < 0) {
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
        snprintf(label, label_len, "%s/type=%s", glue->name, glue->type);
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
    char buf[MAX_LINE_LEN];

    ST_CHECK_PARAM(glue == NULL || label == NULL, NULL);

    label[0] = '\0';
    if (glue->in_offsets != NULL && glue->in_offsets[lid] != 0) {
        snprintf(buf, MAX_LINE_LEN, ",off=%d",
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
    ST_CHECK_PARAM(glue == NULL, -1);

    if (glue->impl != NULL && glue->impl->init_data != NULL) {
        if (glue->impl->init_data(glue, input, layers, output) < 0) {
            ST_WARNING("Failed to init_data for glue impl.");
            return -1;
        }
    }

    return 0;
}

int glue_load_train_opt(glue_t *glue, st_opt_t *opt,
        const char *sec_name, param_t *parent)
{
    char name[MAX_ST_CONF_LEN];

    ST_CHECK_PARAM(glue == NULL || opt == NULL, -1);

    if (glue->impl == NULL || glue->impl->load_train_opt == NULL) {
        return 0;
    }

    if (sec_name == NULL || sec_name[0] == '\0') {
        snprintf(name, MAX_ST_CONF_LEN, "%s", glue->name);
    } else {
        snprintf(name, MAX_ST_CONF_LEN, "%s/%s", sec_name,
                glue->name);
    }
    if (glue->impl->load_train_opt(glue, opt, name, parent) < 0) {
        ST_WARNING("Failed to glue->impl load_train_opt.");
        goto ST_OPT_ERR;
    }

    return 0;

ST_OPT_ERR:
    return -1;
}

int glue_forward(glue_t *glue, input_t *input, output_t *output,
        layer_t **layers, int n_layer, int tid)
{
    int l;
    int lid;
    int off;

    ST_CHECK_PARAM(glue == NULL || layers == NULL, -1);

    for (l = 0; l < glue->num_in_layer; l++) {
        lid = glue->in_layers[l];
        off = glue->in_offsets[l];
        if (!layers[lid]->activated) {
            if (layer_activate(layers[lid], off, tid) < 0) {
                ST_WARNING("Failed to layer_activate.[%s]",
                        layers[lid]->name);
                return -1;
            }

            layers[lid]->activated = true;
        }
    }

    for (l = 0; l < glue->num_out_layer; l++) {
        lid = glue->out_layers[l];
        off = glue->out_offsets[l];
        if (!layers[lid]->cleared) {
            if (layer_clear(layers[lid], off, tid) < 0) {
                ST_WARNING("Failed to layer_clear.[%s]",
                        layers[lid]->name);
                return -1;
            }

            layers[lid]->cleared = true;
        }
    }

    if (glue->impl != NULL && glue->impl->forward != NULL) {
        if (glue->impl->forward(glue, input, output,
                    layers, n_layer, tid) < 0) {
            ST_WARNING("Failed to glue->impl->forward.[%s]",
                    glue->name);
            return -1;
        }
    }

    return 0;
}

int glue_backprop(glue_t *glue, int tid)
{
    ST_CHECK_PARAM(glue == NULL, -1);

    return 0;
}
