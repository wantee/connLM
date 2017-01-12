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
        direct_glue_init_data, direct_glue_init_wt_updater,
        direct_glue_generate_wildcard_repr},
    {FC_GLUE_NAME, NULL, NULL, NULL,
        fc_glue_parse_topo, fc_glue_check, NULL,
        NULL, NULL, NULL, NULL,
        fc_glue_init_data, fc_glue_init_wt_updater,
        NULL},
    {EMB_GLUE_NAME, NULL, NULL, NULL,
        emb_glue_parse_topo, emb_glue_check, NULL,
        NULL, NULL, NULL, NULL,
        emb_glue_init_data, emb_glue_init_wt_updater,
        emb_glue_generate_wildcard_repr},
    {OUT_GLUE_NAME, NULL, NULL, NULL,
        out_glue_parse_topo, out_glue_check, NULL,
        NULL, NULL, NULL, NULL,
        out_glue_init_data, out_glue_init_wt_updater,
        NULL},
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
    glue->out_layer = -1;
    safe_wt_destroy(glue->wt);

    safe_free(glue->wildcard_repr);
}

bool glue_check(glue_t *glue)
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

    ST_CHECK_PARAM(line == NULL, NULL);

    glue = (glue_t *)malloc(sizeof(glue_t));
    if (glue == NULL) {
        ST_WARNING("Failed to malloc glue_t.");
        goto ERR;
    }
    memset(glue, 0, sizeof(glue_t));

    p = get_next_token(line, token);
    if (strcasecmp("glue", token) != 0) {
        ST_WARNING("Not glue line.");
        goto ERR;
    }

    glue->in_layer = -1;
    glue->out_layer = -1;
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

    glue->wt = (weight_t *)malloc(sizeof(weight_t));
    if (glue->wt == NULL) {
        ST_WARNING("Failed to malloc weight_t");
        goto ERR;
    }
    memset(glue->wt, 0, sizeof(weight_t));

    if (wt_parse_topo(glue->wt, untouch_topo, MAX_LINE_LEN) < 0) {
        ST_WARNING("Failed to wt_parse_topo.");
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

    glue->in_layer = g->in_layer;
    glue->out_layer = g->out_layer;

    glue->recur_type = g->recur_type;

    glue->bptt_opt = g->bptt_opt;
    glue->param = g->param;

    glue->wt = wt_dup(g->wt);
    if (glue->wt == NULL) {
        ST_WARNING("Failed to wt_dup.");
        goto ERR;
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
        FILE *fp, bool *binary, FILE *fo_info)
{
    union {
        char str[4];
        int magic_num;
    } flag;

    char name[MAX_NAME_LEN];
    char type[MAX_NAME_LEN];
    int in_layer;
    int out_layer;

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
        if (fread(&in_layer, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read in_layer.");
            return -1;
        }
        if (fread(&out_layer, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read out_layer.");
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
        if (st_readline(fp, "Out layer: %d", &out_layer) != 1) {
            ST_WARNING("Failed to parse out_layer.");
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
        (*glue)->in_layer = in_layer;
        (*glue)->out_layer = out_layer;
        (*glue)->impl = impl;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<GLUE>:%s\n", name);
        fprintf(fo_info, "Type: %s\n", type);
        fprintf(fo_info, "in layer: %d\n", in_layer);
        fprintf(fo_info, "out layer: %d\n", out_layer);
    }

    if (wt_load_header(glue != NULL ? &((*glue)->wt) : NULL,
            version, fp, &b, fo_info) < 0) {
        ST_WARNING("Failed to wt_load_header.");
        goto ERR;
    }

    if (b != *binary) {
        ST_WARNING("binary not match");
        goto ERR;
    }

    if (impl->load_header != NULL) {
        if (impl->load_header(glue != NULL ? &((*glue)->extra) : NULL,
                version, fp, &b, fo_info) < 0) {
            ST_WARNING("Failed to impl->load_header.");
            goto ERR;
        }

        if (b != *binary) {
            ST_WARNING("binary not match");
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
    int n;

    ST_CHECK_PARAM(glue == NULL || fp == NULL, -1);

    if (version < 5) {
        ST_WARNING("Too old version of connlm file");
        return -1;
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
    } else {
        if (st_readline(fp, "<GLUE-DATA>") != 0) {
            ST_WARNING("body flag error.");
            goto ERR;
        }
    }

    if (wt_load_body(glue->wt, version, fp, binary) < 0) {
        ST_WARNING("Failed to wt_load_body.");
        goto ERR;
    }

    if (glue->impl->load_body != NULL) {
        if (glue->impl->load_body(glue->extra, version, fp, binary) < 0) {
            ST_WARNING("Failed to glue->impl->load_body.");
            goto ERR;
        }
    }

    return 0;

ERR:
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
        if (fwrite(&glue->in_layer, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write in_layer.");
            return -1;
        }
        if (fwrite(&glue->out_layer, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write out_layer.");
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
        if (fprintf(fp, "Out layer: %d\n", glue->out_layer) < 0) {
            ST_WARNING("Failed to fprintf out_layer.");
            return -1;
        }
    }

    if (wt_save_header(glue->wt, fp, binary) < 0) {
        ST_WARNING("Failed to wt_save_header.");
        return -1;
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

    ST_CHECK_PARAM(fp == NULL, -1);

    if (binary) {
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

    if (wt_save_body(glue->wt, fp, binary, glue->name) < 0) {
        ST_WARNING("Failed to wt_save_body.");
        return -1;
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

wt_updater_t* glue_init_wt_updater(glue_t *glue, param_t *param)
{
    wt_updater_t *wt_updater = NULL;

    ST_CHECK_PARAM(glue == NULL, NULL);

    if (glue->impl != NULL && glue->impl->init_wt_updater != NULL) {
        wt_updater = glue->impl->init_wt_updater(glue, param);
        if (wt_updater == NULL) {
            ST_WARNING("Failed to glue->impl->init_wt_updater.[%s]",
                    glue->name);
            goto ERR;
        }
    }

    return wt_updater;

ERR:
    return NULL;
}

int glue_load_train_opt(glue_t *glue, st_opt_t *opt, const char *sec_name,
        param_t *parent_param, bptt_opt_t *parent_bptt_opt)
{
    char name[MAX_ST_CONF_LEN];

    ST_CHECK_PARAM(glue == NULL || opt == NULL, -1);

    if (glue->wt != NULL && glue->wt->row > 0) {
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

int glue_generate_wildcard_repr(glue_t *glue)
{
    ST_CHECK_PARAM(glue == NULL, -1);

    if (glue->impl != NULL && glue->impl->generate_wildcard_repr != NULL) {
        if (glue->impl->generate_wildcard_repr(glue) < 0) {
            ST_WARNING("Failed to generate_wildcard_repr for glue impl.");
            return -1;
        }
    }

    return 0;
}
