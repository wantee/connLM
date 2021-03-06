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

#include "param.h"
#include "graph.h"
#include "component.h"

static const int COMP_MAGIC_NUM = 626140498 + 30;

static void comp_destroy_glue_cycles(component_t *comp)
{
    int i;

    if (comp == NULL) {
        return;
    }

    for (i = 0; i < comp->num_glue_cycle; i++) {
        safe_st_free(comp->glue_cycles[i]);
    }
    safe_st_free(comp->glue_cycles);
    comp->num_glue_cycle = 0;
}

void comp_destroy(component_t *comp)
{
    int l;
    int g;

    if (comp == NULL) {
        return;
    }

    comp->name[0] = 0;

    safe_input_destroy(comp->input);

    for (l = 0; l < comp->num_layer; l++) {
        safe_layer_destroy(comp->layers[l]);
    }
    safe_st_free(comp->layers);
    comp->num_layer = 0;

    for (g = 0; g < comp->num_glue; g++) {
        safe_glue_destroy(comp->glues[g]);
    }
    safe_st_free(comp->glues);
    comp->num_glue = 0;

    safe_st_free(comp->fwd_order);

    comp_destroy_glue_cycles(comp);
}

static int comp_dup_glue_cycles(component_t *comp, int **cycles, int num_cycle)
{
    int i, n;

    comp->num_glue_cycle = num_cycle;
    if (comp->num_glue_cycle > 0) {
        comp->glue_cycles = (int **)st_malloc(sizeof(int *)*comp->num_glue_cycle);
        if (comp->glue_cycles == NULL) {
            ST_ERROR("Failed to st_malloc comp->glue_cycles.");
            goto ERR;
        }

        for (i = 0; i < comp->num_glue_cycle; i++) {
            n = cycles[i][0] + 1;
            comp->glue_cycles[i] = (int *)st_malloc(sizeof(int) * n);
            if (comp->glue_cycles[i] == NULL) {
                ST_ERROR("Failed to st_malloc comp->glue_cycles[%d].", i);
                goto ERR;
            }
            memcpy(comp->glue_cycles[i], cycles[i], sizeof(int) * n);
        }
    }

    return 0;

ERR:
    comp_destroy_glue_cycles(comp);
    return -1;
}

component_t* comp_dup(component_t *c)
{
    component_t *comp = NULL;

    int l;
    int g;

    ST_CHECK_PARAM(c == NULL, NULL);

    comp = (component_t *)st_malloc(sizeof(component_t));
    if (comp == NULL) {
        ST_ERROR("Failed to st_malloc component_t.");
        goto ERR;
    }
    memset(comp, 0, sizeof(component_t));

    strncpy(comp->name, c->name, MAX_NAME_LEN);
    comp->name[MAX_NAME_LEN - 1] = '\0';
    comp->comp_scale = c->comp_scale;
    comp->train_opt = c->train_opt;

    comp->input = input_dup(c->input);
    if (comp->input == NULL) {
        ST_ERROR("Failed to input_dup.");
        goto ERR;
    }

    if (c->num_layer > 0) {
        comp->layers = (layer_t **)st_malloc(sizeof(layer_t *)
                * c->num_layer);
        if (comp->layers == NULL) {
            ST_ERROR("Failed to st_malloc layers.");
            goto ERR;
        }

        for (l = 0; l < c->num_layer; l++) {
            comp->layers[l] = layer_dup(c->layers[l]);
            if (comp->layers[l] == NULL) {
                ST_ERROR("Failed to layer_dup.");
                goto ERR;
            }
        }
        comp->num_layer = c->num_layer;
    } else {
        comp->layers = NULL;
        comp->num_layer = 0;
    }

    if (c->num_glue > 0) {
        comp->glues = (glue_t **)st_malloc(sizeof(glue_t *)
                * c->num_glue);
        if (comp->glues == NULL) {
            ST_ERROR("Failed to st_malloc glues.");
            goto ERR;
        }

        for (g = 0; g < c->num_glue; g++) {
            comp->glues[g] = glue_dup(c->glues[g]);
            if (comp->glues[g] == NULL) {
                ST_ERROR("Failed to glue_dup.");
                goto ERR;
            }
        }
        comp->num_glue = c->num_glue;
    } else {
        comp->glues = NULL;
        comp->num_glue = 0;
    }

    if (comp_dup_glue_cycles(comp, c->glue_cycles, c->num_glue_cycle) < 0) {
        ST_ERROR("Failed to comp_dup_glue_cycles.");
        goto ERR;
    }

    return comp;
ERR:
    safe_comp_destroy(comp);
    return NULL;
}

static int comp_parse_topo(component_t *comp, const char *line)
{
    char keyvalue[2*MAX_LINE_LEN];
    char token[MAX_LINE_LEN];
    const char *p;

    ST_CHECK_PARAM(comp == NULL || line == NULL, -1);

    p = line;

    p = get_next_token(p, token);
    if (strcasecmp("property", token) != 0) {
        ST_ERROR("Not property line.");
        return -1;
    }

    while (p != NULL) {
        p = get_next_token(p, token);
        if (token[0] == '\0') {
            continue;
        }

        if (split_line(token, keyvalue, 2, MAX_LINE_LEN, "=") < 0) {
            ST_ERROR("Failed to split key/value. [%s]", token);
            return -1;
        }

        if (strcasecmp("name", keyvalue) == 0) {
            strncpy(comp->name, keyvalue + MAX_LINE_LEN, MAX_NAME_LEN);
            comp->name[MAX_NAME_LEN - 1] = '\0';
        } else if (strcasecmp("scale", keyvalue) == 0) {
            comp->comp_scale = atof(keyvalue + MAX_LINE_LEN);
            if (comp->comp_scale == 0) {
                ST_ERROR("Error comp_scale[%s]", keyvalue + MAX_LINE_LEN);
                return -1;
            }
        } else {
            ST_ERROR("Unknown key[%s].", keyvalue);
        }
    }

    return 0;
}

static int comp_sort_glue(component_t *comp)
{
    graph_t *graph = NULL;

    ST_CHECK_PARAM(comp == NULL, -1);

    graph = graph_construct(comp->layers, comp->num_layer,
            comp->glues, comp->num_glue);
    if (graph == NULL) {
        ST_ERROR("Failed to graph_construct.");
        goto ERR;
    }

    comp->fwd_order = graph_sort(graph);
    if (comp->fwd_order == NULL) {
        ST_ERROR("Failed to graph_sort.");
        goto ERR;
    }

    if (comp_dup_glue_cycles(comp, graph->cycles, graph->num_cycle) < 0) {
        ST_ERROR("Failed to comp_dup_cycles.");
        goto ERR;
    }

    safe_graph_destroy(graph);

    return 0;

ERR:
    safe_graph_destroy(graph);
    return -1;
}

component_t *comp_init_from_topo(const char* topo_content,
        output_t *output, int input_size)
{
    component_t *comp = NULL;

    char token[MAX_LINE_LEN];
    char *line = NULL;
    size_t line_sz = 0;
    size_t line_cap = 0;

    const char *p;

    int l, m;
    int g, h;

    ST_CHECK_PARAM(topo_content == NULL || output == NULL, NULL);

    comp = (component_t *)st_malloc(sizeof(component_t));
    if (comp == NULL) {
        ST_ERROR("Failed to st_malloc comp.");
        goto ERR;
    }
    memset(comp, 0, sizeof(component_t));
    comp->comp_scale = 1.0;

    comp->layers = (layer_t **)st_realloc(comp->layers,
            sizeof(layer_t *) * (comp->num_layer + 1));
    if (comp->layers == NULL) {
        ST_ERROR("Failed to alloc layers.");
        goto ERR;
    }

    if (comp->num_layer != OUTPUT_LAYER_ID) {
        ST_ERROR("output layer must be the 0-layer.");
        goto ERR;
    }
    comp->layers[comp->num_layer] = output_get_layer(output);
    if (comp->layers[comp->num_layer] == NULL) {
        ST_ERROR("Failed to output_get_layer.");
        goto ERR;
    }
    comp->num_layer++;

    p = topo_content;
    while (*p != '\0') {
        line_sz = 0;
        while (*p != '\n' && *p != '\0') {
            if (line_sz >= line_cap) {
                line_cap += 1024;
                line = (char *)st_realloc(line, line_cap);
                if (line == NULL) {
                    ST_ERROR("Failed to st_realloc line.");
                    goto ERR;
                }
            }
            line[line_sz++] = *p;
            p++;
        }
        line[line_sz] = '\0';

        if (*p == '\n') {
            p++;
        }

        (void)get_next_token(line, token);
        if (strcasecmp("property", token) == 0) {
            if(comp_parse_topo(comp, line) < 0) {
                ST_ERROR("Failed to comp_parse_prop.");
                goto ERR;
            }
        } else if (strcasecmp("input", token) == 0) {
            if (comp->input != NULL) {
                ST_ERROR("Too many input layers.");
                goto ERR;
            }
            comp->input = input_parse_topo(line, input_size);
            if (comp->input == NULL) {
                ST_ERROR("Failed to input_parse_topo.");
                goto ERR;
            }
            comp->layers = (layer_t **)st_realloc(comp->layers,
                    sizeof(layer_t *) * (comp->num_layer + 1));
            if (comp->layers == NULL) {
                ST_ERROR("Failed to alloc layers.");
                goto ERR;
            }

            if (comp->num_layer != INPUT_LAYER_ID) {
                ST_ERROR("input layer must be the 1-layer.");
                goto ERR;
            }
            comp->layers[comp->num_layer] = input_get_layer(comp->input);
            if (comp->layers[comp->num_layer] == NULL) {
                ST_ERROR("Failed to input_get_layer.");
                goto ERR;
            }
            comp->num_layer++;
        } else if (strcasecmp("layer", token) == 0) {
            if (comp->input == NULL) {
                ST_ERROR("input definition should place before layer.");
                goto ERR;
            }
            comp->layers = (layer_t **)st_realloc(comp->layers,
                    sizeof(layer_t *) * (comp->num_layer + 1));
            if (comp->layers == NULL) {
                ST_ERROR("Failed to alloc layers.");
                goto ERR;
            }

            comp->layers[comp->num_layer] = layer_parse_topo(line);
            if (comp->layers[comp->num_layer] == NULL) {
                ST_ERROR("Failed to layer_parse_topo.");
                goto ERR;
            }
            comp->num_layer++;
        } else if (strcasecmp("glue", token) == 0) {
            if (comp->input == NULL) {
                ST_ERROR("input definition should place before glue.");
                goto ERR;
            }
            comp->glues = (glue_t **)st_realloc(comp->glues,
                    sizeof(glue_t *) * (comp->num_glue + 1));
            if (comp->glues == NULL) {
                ST_ERROR("Failed to alloc glues.");
                goto ERR;
            }

            comp->glues[comp->num_glue] = glue_parse_topo(line,
                    comp->layers, comp->num_layer, comp->input, output);
            if (comp->glues[comp->num_glue] == NULL) {
                ST_ERROR("Failed to glue_parse_topo.");
                goto ERR;
            }
            comp->num_glue++;
        }
    }

    safe_st_free(line);

    if (comp->input == NULL) {
        ST_ERROR("NO input layer.");
        goto ERR;
    }

    if (comp->num_glue == 0) {
        ST_ERROR("NO glues.");
        goto ERR;
    }

    if (comp->num_layer == 0) {
        ST_ERROR("NO layers.");
        goto ERR;
    }

    for (l = 0; l < comp->num_layer - 1; l++) {
        for (m = l+1; m < comp->num_layer; m++) {
            if (strcmp(comp->layers[l]->name, comp->layers[m]->name) == 0) {
                ST_ERROR("Duplicated layer name[%s]",
                        comp->layers[l]->name);
                goto ERR;
            }
        }
    }

    for (g = 0; g < comp->num_glue - 1; g++) {
        for (h = g+1; h < comp->num_glue; h++) {
            if (strcmp(comp->glues[g]->name, comp->glues[h]->name) == 0) {
                ST_ERROR("Duplicated glue name[%s]",
                        comp->glues[g]->name);
                goto ERR;
            }
        }
    }

    if (comp_sort_glue(comp) < 0) {
        ST_ERROR("Failed to comp_sort_glue component [%s]", comp->name);
        goto ERR;
    }

    for (g = 0; g < comp->num_glue; g++) {
        if (glue_init_data(comp->glues[g], comp->input,
                    comp->layers, output) < 0) {
            ST_ERROR("Failed to glue_init_data.");
            goto ERR;
        }
    }

    return comp;

ERR:
    safe_st_free(line);

    safe_comp_destroy(comp);
    return NULL;
}

int comp_load_train_opt(component_t *comp, st_opt_t *opt, const char *sec_name,
        param_t *parent_param, bptt_opt_t *parent_bptt_opt)
{
    char name[MAX_ST_CONF_LEN];
    param_t param;
    bptt_opt_t bptt_opt;
    int i, c, g;

    ST_CHECK_PARAM(comp == NULL || opt == NULL, -1);

    if (sec_name == NULL || sec_name[0] == '\0') {
        snprintf(name, MAX_ST_CONF_LEN, "%s", comp->name);
    } else {
        snprintf(name, MAX_ST_CONF_LEN, "%s/%s", sec_name,
                comp->name);
    }
    if (param_load(&param, opt, name, parent_param) < 0) {
        ST_ERROR("Failed to param_load.");
        goto ST_OPT_ERR;
    }
    if (bptt_opt_load(&bptt_opt, opt, name, parent_bptt_opt) < 0) {
        ST_ERROR("Failed to param_load.");
        goto ST_OPT_ERR;
    }

    for (g = 0; g < comp->num_glue; g++) {
        if (glue_load_train_opt(comp->glues[g], opt, name,
                    &param, &bptt_opt) < 0) {
            ST_ERROR("Failed to glue_load_train_opt.");
            goto ST_OPT_ERR;
        }
    }

    // Set bptt_opt for glues in cycles
    for (c = 0; c < comp->num_glue_cycle; c++) {
        g = comp->glue_cycles[c][1]; // RECUR_HEAD
        bptt_opt = comp->glues[g]->bptt_opt;
        param = comp->glues[g]->param;
        for (i = 2; i <= comp->glue_cycles[c][0]; i++) {
            g = comp->glue_cycles[c][i];
            if (comp->glues[g]->bptt_opt.bptt > 0) {
                // bptt already be set, which implies this glue lies on
                // multiple cycles
                if (!bptt_opt_equal(&comp->glues[g]->bptt_opt, &bptt_opt)) {
                    ST_ERROR("recur glue[%s] in multiple cycles must have "
                            "same bptt hyperparames.", comp->glues[g]);
                    goto ST_OPT_ERR;
                }
                if (!param_equal(&comp->glues[g]->param, &param)) {
                    ST_ERROR("recur glue[%s] in multiple cycles must have "
                            "same hyperparames.", comp->glues[g]);
                    goto ST_OPT_ERR;
                }
            } else {
                comp->glues[g]->param = param;
                comp->glues[g]->bptt_opt= bptt_opt;
            }
        }
    }

    return 0;

ST_OPT_ERR:
    return -1;
}

int comp_load_header(component_t **comp, int version,
        FILE *fp, connlm_fmt_t *fmt, FILE *fo_info)
{
    union {
        char str[4];
        int magic_num;
    } flag;
    size_t sz;

    char name[MAX_NAME_LEN];
    int num_layer;
    int num_glue;
    real_t scale;
    int l;
    int g;

    connlm_fmt_t f;

    ST_CHECK_PARAM((comp == NULL && fo_info == NULL) || fp == NULL
            || fmt == NULL, -1);

    if (version < 3) {
        ST_ERROR("Too old version of connlm file");
        return -1;
    }

    if (fread(&flag.magic_num, sizeof(int), 1, fp) != 1) {
        ST_ERROR("Failed to load magic num.");
        return -1;
    }

    *fmt = CONN_FMT_UNKNOWN;
    if (strncmp(flag.str, "    ", 4) == 0) {
        *fmt = CONN_FMT_TXT;
    } else if (COMP_MAGIC_NUM != flag.magic_num) {
        ST_ERROR("magic num wrong.");
        return -2;
    }

    if (comp != NULL) {
        *comp = NULL;
    }

    if (*fmt != CONN_FMT_TXT) {
        if (version >= 12) {
            if (fread(fmt, sizeof(connlm_fmt_t), 1, fp) != 1) {
                ST_ERROR("Failed to read fmt.");
                goto ERR;
            }
        } else {
            *fmt = CONN_FMT_BIN;
        }

        if (fread(name, sizeof(char), MAX_NAME_LEN, fp) != MAX_NAME_LEN) {
            ST_ERROR("Failed to read name.");
            return -1;
        }
        name[MAX_NAME_LEN - 1] = '\0';

        if (fread(&num_layer, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to read num_layer.");
            return -1;
        }

        if (fread(&num_glue, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to read num_glue.");
            return -1;
        }

        if (fread(&scale, sizeof(real_t), 1, fp) != 1) {
            ST_ERROR("Failed to read scale.");
            return -1;
        }
    } else {
        if (st_readline(fp, "") != 0) {
            ST_ERROR("tag error.");
            goto ERR;
        }
        if (st_readline(fp, "<COMPONENT>") != 0) {
            ST_ERROR("tag error.");
            goto ERR;
        }

        if (st_readline(fp, "Name: %"xSTR(MAX_NAME_LEN)"s", name) != 1) {
            ST_ERROR("Failed to parse name.");
            goto ERR;
        }
        name[MAX_NAME_LEN - 1] = '\0';

        if (st_readline(fp, "Num layer: %d", &num_layer) != 1) {
            ST_ERROR("Failed to parse num_layer.");
            return -1;
        }

        if (st_readline(fp, "Num glue: %d", &num_glue) != 1) {
            ST_ERROR("Failed to parse num_glue.");
            return -1;
        }

        if (st_readline(fp, "Scale: "REAL_FMT, &scale) != 1) {
            ST_ERROR("Failed to parse scale.");
            return -1;
        }
    }

    if (comp != NULL) {
        *comp = (component_t *)st_malloc(sizeof(component_t));
        if (*comp == NULL) {
            ST_ERROR("Failed to st_malloc component_t");
            goto ERR;
        }
        memset(*comp, 0, sizeof(component_t));

        strncpy((*comp)->name, name, MAX_NAME_LEN);
        (*comp)->name[MAX_NAME_LEN - 1] = '\0';

        sz = sizeof(layer_t *)*num_layer;
        (*comp)->layers = (layer_t **)st_malloc(sz);
        if ((*comp)->layers == NULL) {
            ST_ERROR("Failed to alloc layers.");
            goto ERR;
        }
        memset((*comp)->layers, 0, sz);
        (*comp)->num_layer = num_layer;

        sz = sizeof(glue_t *)*num_glue;
        (*comp)->glues = (glue_t **)st_malloc(sz);
        if ((*comp)->glues == NULL) {
            ST_ERROR("Failed to alloc glues.");
            goto ERR;
        }
        memset((*comp)->glues, 0, sz);
        (*comp)->num_glue = num_glue;

        (*comp)->comp_scale = scale;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<COMPONENT>:%s\n", name);
        fprintf(fo_info, "Num layer: %d\n", num_layer);
        fprintf(fo_info, "Num glue: %d\n", num_glue);
        fprintf(fo_info, "Scale: "REAL_FMT"\n", scale);
    }

    if (input_load_header(comp != NULL ? &((*comp)->input) : NULL,
                version, fp, &f, fo_info) < 0) {
        ST_ERROR("Failed to input_load_header.");
        goto ERR;
    }
    if (*fmt != f) {
        ST_ERROR("Multiple formats in one file.");
        return -1;
    }

    for (l = 0; l < num_layer; l++) {
        if (layer_load_header(comp != NULL ? &((*comp)->layers[l]) : NULL,
                    version, fp, &f, fo_info) < 0) {
            ST_ERROR("Failed to layer_load_header[%d].", l);
            goto ERR;
        }
        if (*fmt != f) {
            ST_ERROR("Multiple formats in one file.");
            return -1;
        }
    }

    for (g = 0; g < num_glue; g++) {
        if (glue_load_header(comp != NULL ? &((*comp)->glues[g]) : NULL,
                    version, fp, &f, fo_info) < 0) {
            ST_ERROR("Failed to glue_load_header[%d].", g);
            goto ERR;
        }
        if (*fmt != f) {
            ST_ERROR("Multiple formats in one file.");
            return -1;
        }
    }

    return 0;

ERR:
    if (comp != NULL) {
        safe_comp_destroy(*comp);
    }
    return -1;
}

int comp_load_body(component_t *comp, int version, FILE *fp, connlm_fmt_t fmt)
{
    int n;
    int l;
    int g;

    ST_CHECK_PARAM(comp == NULL || fp == NULL, -1);

    if (version < 3) {
        ST_ERROR("Too old version of connlm file");
        return -1;
    }

    if (connlm_fmt_is_bin(fmt)) {
        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to read magic num.");
            goto ERR;
        }

        if (n != -COMP_MAGIC_NUM) {
            ST_ERROR("Magic num error.");
            goto ERR;
        }

    } else {
        if (st_readline(fp, "<COMPONENT-DATA>") != 0) {
            ST_ERROR("body flag error.");
            goto ERR;
        }
    }

    if (input_load_body(comp->input, version, fp, fmt) < 0) {
        ST_ERROR("Failed to input_load_body.");
        goto ERR;
    }

    for (l = 0; l < comp->num_layer; l++) {
        if (layer_load_body(comp->layers[l], version, fp, fmt) < 0) {
            ST_ERROR("Failed to layer_load_body[%d].", l);
            goto ERR;
        }
    }

    for (g = 0; g < comp->num_glue; g++) {
        if (glue_load_body(comp->glues[g], version, fp, fmt) < 0) {
            ST_ERROR("Failed to glue_load_body[%d].", g);
            goto ERR;
        }
    }

    if (comp_sort_glue(comp) < 0) {
        ST_ERROR("Failed to comp_sort_glue component [%s]", comp->name);
        goto ERR;
    }

    return 0;

ERR:
    safe_input_destroy(comp->input);

    for (l = 0; l < comp->num_layer; l++) {
        safe_layer_destroy(comp->layers[l]);
    }

    for (g = 0; g < comp->num_glue; g++) {
        safe_glue_destroy(comp->glues[g]);
    }

    return -1;
}

int comp_save_header(component_t *comp, FILE *fp, connlm_fmt_t fmt)
{
    int l;
    int g;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (connlm_fmt_is_bin(fmt)) {
        if (fwrite(&COMP_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to write magic num.");
            return -1;
        }
        if (fwrite(&fmt, sizeof(connlm_fmt_t), 1, fp) != 1) {
            ST_ERROR("Failed to write fmt.");
            return -1;
        }

        if (fwrite(comp->name, sizeof(char), MAX_NAME_LEN, fp)
                != MAX_NAME_LEN) {
            ST_ERROR("Failed to write name.");
            return -1;
        }

        if (fwrite(&comp->num_layer, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to write num_layer.");
            return -1;
        }

        if (fwrite(&comp->num_glue, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to write num_glue.");
            return -1;
        }

        if (fwrite(&comp->comp_scale, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to write scale.");
            return -1;
        }
    } else {
        if (fprintf(fp, "    \n<COMPONENT>\n") < 0) {
            ST_ERROR("Failed to fprintf header.");
            return -1;
        }

        if (fprintf(fp, "Name: %s\n", comp->name) < 0) {
            ST_ERROR("Failed to fprintf name.");
            return -1;
        }
        if (fprintf(fp, "Num layer: %d\n", comp->num_layer) < 0) {
            ST_ERROR("Failed to fprintf num layer.");
            return -1;
        }
        if (fprintf(fp, "Num glue: %d\n", comp->num_glue) < 0) {
            ST_ERROR("Failed to fprintf num glue.");
            return -1;
        }
        if (fprintf(fp, "Scale: "REAL_FMT"\n", comp->comp_scale) < 0) {
            ST_ERROR("Failed to fprintf scale.");
            return -1;
        }
    }

    if (input_save_header(comp->input, fp, fmt) < 0) {
        ST_ERROR("Failed to input_save_header.");
        return -1;
    }

    for (l = 0; l < comp->num_layer; l++) {
        if (layer_save_header(comp->layers[l], fp, fmt) < 0) {
            ST_ERROR("Failed to layer_save_header[%d].", l);
            return -1;
        }
    }

    for (g = 0; g < comp->num_glue; g++) {
        if (glue_save_header(comp->glues[g], fp, fmt) < 0) {
            ST_ERROR("Failed to glue_save_header[%d].", g);
            return -1;
        }
    }

    return 0;
}

int comp_save_body(component_t *comp, FILE *fp, connlm_fmt_t fmt)
{
    int n;
    int l;
    int g;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (comp == NULL) {
        return 0;
    }

    if (connlm_fmt_is_bin(fmt)) {
        n = -COMP_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to write magic num.");
            return -1;
        }
    } else {
        if (fprintf(fp, "<COMPONENT-DATA>\n") < 0) {
            ST_ERROR("Failed to fprintf header.");
            return -1;
        }
    }

    if (input_save_body(comp->input, fp, fmt) < 0) {
        ST_ERROR("Failed to input_save_body.");
        return -1;
    }

    for (l = 0; l < comp->num_layer; l++) {
        if (layer_save_body(comp->layers[l], fp, fmt) < 0) {
            ST_ERROR("Failed to layer_save_body[%d].", l);
            return -1;
        }
    }

    for (g = 0; g < comp->num_glue; g++) {
        if (glue_save_body(comp->glues[g], fp, fmt) < 0) {
            ST_ERROR("Failed to glue_save_body[%d].", g);
            return -1;
        }
    }

    return 0;
}

char* comp_input_nodename(component_t *comp, char *nodename,
        size_t name_len)
{
    snprintf(nodename, name_len, "%s_%s", INPUT_LAYER_NAME, comp->name);
    return nodename;
}

static inline char* layer2nodename(component_t *comp, int l,
        char *node, size_t node_len)
{
    if (strcasecmp(comp->layers[l]->type, INPUT_LAYER_NAME) == 0) {
        (void)comp_input_nodename(comp, node, node_len);
    } else if (strcasecmp(comp->layers[l]->type, OUTPUT_LAYER_NAME) == 0) {
        snprintf(node, node_len, OUTPUT_LAYER_NAME);
    } else {
        snprintf(node, node_len, "layer_%s_%s", comp->name,
                comp->layers[l]->name);
    }
    return node;
}

static inline char* glue2nodename(component_t *comp, int g,
        char *node, size_t node_len)
{
    snprintf(node, node_len, "glue_%s_%s", comp->name,
            comp->glues[g]->name);
    return node;
}

int comp_draw(component_t *comp, FILE *fp, bool verbose)
{
    char label[MAX_NAME_LEN];
    char nodename[MAX_NAME_LEN];
    char gluenodename[MAX_NAME_LEN];
    int l;
    int g;
    int gg, order;

    ST_CHECK_PARAM(comp == NULL || fp == NULL, -1);

    fprintf(fp, "    node[shape=box,fixedsize=false];\n");
    for (l = 0; l < comp->num_layer; l++) {
        if (strcasecmp(comp->layers[l]->type, OUTPUT_LAYER_NAME) == 0) {
            continue;
        }
        if (strcasecmp(comp->layers[l]->type, INPUT_LAYER_NAME) == 0) {
            fprintf(fp, "    %s [label=\"%s/%s\"];\n",
                    layer2nodename(comp, l, nodename, MAX_NAME_LEN),
                    comp->name,
                    input_draw_label(comp->input, label, MAX_NAME_LEN));
            continue;
        }
        fprintf(fp, "    %s [label=\"%s\"];\n",
                layer2nodename(comp, l, nodename, MAX_NAME_LEN),
                layer_draw_label(comp->layers[l], label, MAX_NAME_LEN));
    }
    fprintf(fp, "\n");

    fprintf(fp, "    node[shape=circle");
    if (!verbose) {
        fprintf(fp, ",fixedsize=true");
    }
    fprintf(fp, "];\n");
    for (g = 0; g < comp->num_glue; g++) {
        if (comp->glues[g]->recur_type == RECUR_HEAD) {
            fprintf(fp, "    node[color=red,fontcolor=red];\n");
            fprintf(fp, "    edge[color=red];\n");
        } else if (comp->glues[g]->recur_type == RECUR_BODY) {
            fprintf(fp, "    node[color=blue,fontcolor=blue];\n");
            fprintf(fp, "    edge[color=blue];\n");
        }
        (void)glue2nodename(comp, g, gluenodename, MAX_NAME_LEN),
        fprintf(fp, "    %s [label=\"%s", gluenodename,
                glue_draw_label(comp->glues[g], label, MAX_NAME_LEN,
                    verbose));
        if (verbose) {
            order = -1;
            for (gg = 0; gg < comp->num_glue; gg++) {
                if (comp->fwd_order[gg] == g) {
                    order = gg;
                    break;
                }
            }
            fprintf(fp, "\\n%d", order);
        }
        fprintf(fp, "\"];\n");
        fprintf(fp, "    %s -> %s [label=\"%s\"];\n",
                layer2nodename(comp,
                    comp->glues[g]->in_layer,
                    nodename, MAX_NAME_LEN),
                gluenodename,
                glue_draw_label_one(comp->glues[g], l,
                    label, MAX_NAME_LEN, verbose));
        fprintf(fp, "    %s -> %s [label=\"%s\"];\n",
                gluenodename,
                layer2nodename(comp,
                    comp->glues[g]->out_layer,
                    nodename, MAX_NAME_LEN),
                glue_draw_label_one(comp->glues[g], l,
                    label, MAX_NAME_LEN, verbose));

        if (comp->glues[g]->recur_type != RECUR_NON) {
            fprintf(fp, "    node[color=black,fontcolor=black];\n");
            fprintf(fp, "    edge[color=black];\n");
        }
        fprintf(fp, "\n");
    }

    return 0;
}

void comp_sanity_check(component_t *comp)
{
    int g;

    ST_CHECK_PARAM_VOID(comp == NULL);

    for (g = 0; g < comp->num_glue; g++) {
        glue_sanity_check(comp->glues[g]);
    }
}

void comp_print_verbose_info(component_t *comp, FILE *fo)
{
    int i;

    ST_CHECK_PARAM_VOID(comp == NULL || fo == NULL);

    input_print_verbose_info(comp->input, fo);

    for (i = 0; i < comp->num_layer; i++) {
        layer_print_verbose_info(comp->layers[i], fo);
    }

    for (i = 0; i < comp->num_glue; i++) {
        glue_print_verbose_info(comp->glues[i], fo);
    }
}

bool comp_check_glue_cycles(component_t *comp)
{
    glue_t *glue;
    int i, j;
    int ii, jj;
    int g, gg;
    real_t d, dd;

    ST_CHECK_PARAM(comp == NULL, false);

    // check *overlapped* cycles
    for (i = 0; i < comp->num_glue_cycle; i++) {
        g = comp->glue_cycles[i][1];
        glue = comp->glues[g];
        d = glue->param.dropout;
        for (j = 1; j <= comp->glue_cycles[i][0]; j++) {
            g = comp->glue_cycles[i][j];
            for (ii = i + 1; ii < comp->num_glue_cycle; ii++) {
                gg = comp->glue_cycles[ii][1];
                glue = comp->glues[gg];
                dd = glue->param.dropout;
                for (jj = 1; jj <= comp->glue_cycles[ii][0]; jj++) {
                    gg = comp->glue_cycles[ii][jj];
                    if (g == gg) {
                        if (d != dd) {
                            ST_ERROR("glue[%s] is in two cycles whose heads "
                                       "is [%s] and [%s], but with different "
                                       "dropout.");
                            return false;
                        }
                    }
                }
            }
        }
    }

    return true;
}
