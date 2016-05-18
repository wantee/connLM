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

#include "param.h"
#include "component.h"
#include "glues/direct_glue.h"

static const int COMP_MAGIC_NUM = 626140498 + 30;

void comp_destroy(component_t *comp)
{
    layer_id_t l;
    glue_id_t g;

    if (comp == NULL) {
        return;
    }

    comp->name[0] = 0;

    safe_input_destroy(comp->input);
    safe_emb_wt_destroy(comp->emb);

    for (l = 0; l < comp->num_layer; l++) {
        safe_layer_destroy(comp->layers[l]);
    }
    safe_free(comp->layers);
    comp->num_layer = 0;

    for (g = 0; g < comp->num_glue; g++) {
        safe_glue_destroy(comp->glues[g]);
    }
    safe_free(comp->glues);
    comp->num_glue = 0;

    safe_graph_destroy(comp->graph);

    safe_output_wt_destroy(comp->output_wt);
}

component_t* comp_dup(component_t *c)
{
    component_t *comp = NULL;

    layer_id_t l;
    glue_id_t g;

    ST_CHECK_PARAM(c == NULL, NULL);

    comp = (component_t *)malloc(sizeof(component_t));
    if (comp == NULL) {
        ST_WARNING("Falied to malloc component_t.");
        goto ERR;
    }
    memset(comp, 0, sizeof(component_t));

    strncpy(comp->name, c->name, MAX_NAME_LEN);
    comp->name[MAX_NAME_LEN - 1] = '\0';

    comp->input = input_dup(c->input);
    if (comp->input == NULL) {
        ST_WARNING("Failed to input_dup.");
        goto ERR;
    }

    comp->emb = emb_wt_dup(c->emb);
    if (comp->emb == NULL) {
        ST_WARNING("Failed to emb_wt_dup.");
        goto ERR;
    }

    if (c->num_layer > 0) {
        comp->layers = (layer_t **)malloc(sizeof(layer_t *)
                * c->num_layer);
        if (comp->layers == NULL) {
            ST_WARNING("Failed to malloc layers.");
            goto ERR;
        }

        for (l = 0; l < c->num_layer; l++) {
            comp->layers[l] = layer_dup(c->layers[l]);
            if (comp->layers[l] == NULL) {
                ST_WARNING("Failed to layer_dup.");
                goto ERR;
            }
        }
        comp->num_layer = c->num_layer;
    } else {
        comp->layers = NULL;
        comp->num_layer = 0;
    }

    if (c->num_glue > 0) {
        comp->glues = (glue_t **)malloc(sizeof(glue_t *)
                * c->num_glue);
        if (comp->glues == NULL) {
            ST_WARNING("Failed to malloc glues.");
            goto ERR;
        }

        for (g = 0; g < c->num_glue; g++) {
            comp->glues[g] = glue_dup(c->glues[g]);
            if (comp->glues[g] == NULL) {
                ST_WARNING("Failed to glue_dup.");
                goto ERR;
            }
        }
        comp->num_glue = c->num_glue;
    } else {
        comp->glues = NULL;
        comp->num_glue = 0;
    }

    if (comp->graph != NULL) {
        comp->graph = graph_dup(c->graph);
        if (comp->graph == NULL) {
            ST_WARNING("Failed to graph_dup.");
            goto ERR;
        }
    } else {
        comp->graph = NULL;
    }

    comp->output_wt = output_wt_dup(c->output_wt);
    if (comp->output_wt == NULL) {
        ST_WARNING("Failed to output_wt_dup.");
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
        ST_WARNING("Not property line.");
        return -1;
    }

    while (p != NULL) {
        p = get_next_token(p, token);
        if (split_line(token, keyvalue, 2, MAX_LINE_LEN, "=") < 0) {
            ST_WARNING("Failed to split key/value. [%s]", token);
            return -1;
        }

        if (strcasecmp("name", keyvalue) == 0) {
            strncpy(comp->name, keyvalue + MAX_LINE_LEN, MAX_NAME_LEN);
            comp->name[MAX_NAME_LEN - 1] = '\0';
        } else {
            ST_WARNING("Unkown key[%s].", keyvalue);
        }
    }

    return 0;
}

component_t *comp_init_from_topo(const char* topo_content, output_t *output)
{
    component_t *comp = NULL;

    char token[MAX_LINE_LEN];
    char *line = NULL;
    size_t line_sz = 0;
    size_t line_cap = 0;

    const char *p;

    ST_CHECK_PARAM(topo_content == NULL || output == NULL, NULL);

    comp = (component_t *)malloc(sizeof(component_t));
    if (comp == NULL) {
        ST_WARNING("Failed to malloc comp.");
        goto ERR;
    }
    memset(comp, 0, sizeof(component_t));

    comp->layers = (layer_t **)realloc(comp->layers,
            sizeof(layer_t *) * (comp->num_layer + 1));
    if (comp->layers == NULL) {
        ST_WARNING("Failed to alloc layers.");
        goto ERR;
    }

    comp->layers[comp->num_layer] = output_get_layer(output);
    if (comp->layers[comp->num_layer] == NULL) {
        ST_WARNING("Failed to output_get_layer.");
        goto ERR;
    }
    comp->num_layer++;

    p = topo_content;
    while (*p != '\0') {
        line_sz = 0;
        while (*p != '\n' && *p != '\0') {
            if (line_sz >= line_cap) {
                line_cap += 1024;
                line = (char *)realloc(line, line_cap);
                if (line == NULL) {
                    ST_WARNING("Failed to realloc line.");
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
                ST_WARNING("Failed to comp_parse_prop.");
                goto ERR;
            }
        } else if (strcasecmp("input", token) == 0) {
            if (comp->input != NULL) {
                ST_WARNING("Too many input layers.");
                goto ERR;
            }
            comp->input = input_parse_topo(line);
            if (comp->input == NULL) {
                ST_WARNING("Failed to input_parse_topo.");
                goto ERR;
            }
            comp->layers = (layer_t **)realloc(comp->layers,
                    sizeof(layer_t *) * (comp->num_layer + 1));
            if (comp->layers == NULL) {
                ST_WARNING("Failed to alloc layers.");
                goto ERR;
            }

            comp->layers[comp->num_layer] = input_get_layer(comp->input);
            if (comp->layers[comp->num_layer] == NULL) {
                ST_WARNING("Failed to input_get_layer.");
                goto ERR;
            }
            comp->num_layer++;
        } else if (strcasecmp("layer", token) == 0) {
            comp->layers = (layer_t **)realloc(comp->layers,
                    sizeof(layer_t *) * (comp->num_layer + 1));
            if (comp->layers == NULL) {
                ST_WARNING("Failed to alloc layers.");
                goto ERR;
            }

            comp->layers[comp->num_layer] = layer_parse_topo(line);
            if (comp->layers[comp->num_layer] == NULL) {
                ST_WARNING("Failed to layer_parse_topo.");
                goto ERR;
            }
            comp->num_layer++;
        } else if (strcasecmp("glue", token) == 0) {
            comp->glues = (glue_t **)realloc(comp->glues,
                    sizeof(glue_t *) * (comp->num_glue + 1));
            if (comp->glues == NULL) {
                ST_WARNING("Failed to alloc glues.");
                goto ERR;
            }

            comp->glues[comp->num_glue] = glue_parse_topo(line,
                    comp->layers, comp->num_layer);
            if (comp->glues[comp->num_glue] == NULL) {
                ST_WARNING("Failed to glue_parse_topo.");
                goto ERR;
            }
            comp->num_glue++;
        }
    }

    if (comp->input == NULL) {
        ST_WARNING("NO input layer.");
        goto ERR;
    }

    safe_free(line);

    return comp;

ERR:
    safe_free(line);

    safe_comp_destroy(comp);
    return NULL;
}

int comp_init_output_wt(component_t *comp, output_t *output, real_t scale)
{
    ST_CHECK_PARAM(comp == NULL || output == NULL || scale <= 0, -1);

    return 0;
}

int comp_construct_graph(component_t *comp)
{
    layer_id_t l, m;
    glue_id_t g, h;

    ST_CHECK_PARAM(comp == NULL, -1);

    comp->graph = NULL;

    for (l = 0; l < comp->num_layer - 1; l++) {
        for (m = l+1; m < comp->num_layer; m++) {
            if (strcmp(comp->layers[l]->name, comp->layers[m]->name) == 0) {
                ST_WARNING("Duplicated layer name[%s]",
                        comp->layers[l]->name);
                return -1;
            }
        }
    }

    for (g = 0; g < comp->num_glue - 1; g++) {
        for (h = g+1; h < comp->num_glue; h++) {
            if (strcmp(comp->glues[g]->name, comp->glues[h]->name) == 0) {
                ST_WARNING("Duplicated glue name[%s]",
                        comp->glues[g]->name);
                return -1;
            }
        }
    }

    if (comp->num_layer > 0) {
        comp->graph = graph_construct(comp->layers, comp->num_layer,
                comp->glues, comp->num_glue);
        if (comp->graph == NULL) {
            ST_WARNING("Failed to graph_construct.");
            goto ERR;
        }
    } else {
        for (g = 0; g < comp->num_glue; g++) {
            if (strcasecmp(comp->glues[g]->type, DIRECT_GLUE_NAME) != 0) {
                ST_WARNING("Contain non-direct glue without layer[%s]",
                        comp->glues[g]->name);
                goto ERR;
            }
        }
    }

    return 0;

ERR:
    safe_graph_destroy(comp->graph);
    return -1;
}

int comp_load_train_opt(component_t *comp, st_opt_t *opt,
        const char *sec_name)
{
    ST_CHECK_PARAM(comp == NULL || opt == NULL, -1);

    if (param_load(&comp->train_opt.param, opt, sec_name, NULL) < 0) {
        ST_WARNING("Failed to param_load.");
        goto ST_OPT_ERR;
    }

    return 0;

ST_OPT_ERR:
    return -1;
}

int comp_setup(component_t *comp, output_t *output, int num_thrs,
        bool backprop)
{
    return 0;
}

int comp_reset(component_t *comp, int tid, bool backprop)
{
    return 0;
}

int comp_start(component_t *comp, int word, int tid, bool backprop)
{
    return 0;
}

int comp_end(component_t *comp, int word, int tid, bool backprop)
{
    return 0;
}

int comp_finish(component_t *comp, int tid, bool backprop)
{
    return 0;
}

int comp_load_header(component_t **comp, int version,
        FILE *fp, bool *binary, FILE *fo_info)
{
    union {
        char str[4];
        int magic_num;
    } flag;
    size_t sz;

    char name[MAX_NAME_LEN];
    layer_id_t num_layer;
    glue_id_t num_glue;
    layer_id_t l;
    glue_id_t g;

    bool b;

    ST_CHECK_PARAM((comp == NULL && fo_info == NULL) || fp == NULL
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
    } else if (COMP_MAGIC_NUM != flag.magic_num) {
        ST_WARNING("magic num wrong.");
        return -2;
    } else {
        *binary = true;
    }

    if (comp != NULL) {
        *comp = NULL;
    }

    if (*binary) {
        if (fread(name, sizeof(char), MAX_NAME_LEN, fp) != MAX_NAME_LEN) {
            ST_WARNING("Failed to read name.");
            return -1;
        }
        name[MAX_NAME_LEN - 1] = '\0';

        if (fread(&num_layer, sizeof(layer_id_t), 1, fp) != 1) {
            ST_WARNING("Failed to read num_layer.");
            return -1;
        }

        if (fread(&num_glue, sizeof(glue_id_t), 1, fp) != 1) {
            ST_WARNING("Failed to read num_glue.");
            return -1;
        }
    } else {
        if (st_readline(fp, "") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }
        if (st_readline(fp, "<COMPONENT>") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }

        if (st_readline(fp, "Name: %"xSTR(MAX_NAME_LEN)"s", name) != 1) {
            ST_WARNING("Failed to parse name.");
            goto ERR;
        }
        name[MAX_NAME_LEN - 1] = '\0';

        if (st_readline(fp, "Num layer: " LAYER_ID_FMT, &num_layer) != 1) {
            ST_WARNING("Failed to parse num_layer.");
            return -1;
        }

        if (st_readline(fp, "Num glue: " GLUE_ID_FMT, &num_glue) != 1) {
            ST_WARNING("Failed to parse num_glue.");
            return -1;
        }
    }

    if (comp != NULL) {
        *comp = (component_t *)malloc(sizeof(component_t));
        if (*comp == NULL) {
            ST_WARNING("Failed to malloc component_t");
            goto ERR;
        }
        memset(*comp, 0, sizeof(component_t));

        strncpy((*comp)->name, name, MAX_NAME_LEN);
        (*comp)->name[MAX_NAME_LEN - 1] = '\0';

        sz = sizeof(layer_t *)*num_layer;
        (*comp)->layers = (layer_t **)malloc(sz);
        if ((*comp)->layers == NULL) {
            ST_WARNING("Failed to alloc layers.");
            goto ERR;
        }
        memset((*comp)->layers, 0, sz);
        (*comp)->num_layer = num_layer;

        sz = sizeof(glue_t *)*num_glue;
        (*comp)->glues = (glue_t **)malloc(sz);
        if ((*comp)->glues == NULL) {
            ST_WARNING("Failed to alloc glues.");
            goto ERR;
        }
        memset((*comp)->glues, 0, sz);
        (*comp)->num_glue = num_glue;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<COMPONENT>\n");
        fprintf(fo_info, "Name: %s\n", name);
        fprintf(fo_info, "Num layer: " LAYER_ID_FMT "\n", num_layer);
        fprintf(fo_info, "Num glue: " GLUE_ID_FMT "\n", num_glue);
    }

    if (input_load_header(comp != NULL ? &((*comp)->input) : NULL,
                version, fp, &b, fo_info) < 0) {
        ST_WARNING("Failed to input_load_header.");
        goto ERR;
    }
    if (*binary != b) {
        ST_WARNING("Both binary and text format in one file.");
        goto ERR;
    }

    if (emb_wt_load_header(comp != NULL ? &((*comp)->emb) : NULL,
                version, fp, &b, fo_info) < 0) {
        ST_WARNING("Failed to emb_wt_load_header.");
        goto ERR;
    }
    if (*binary != b) {
        ST_WARNING("Both binary and text format in one file.");
        goto ERR;
    }

    for (l = 0; l < num_layer; l++) {
        if (layer_load_header(comp != NULL ? &((*comp)->layers[l]) : NULL,
                    version, fp, &b, fo_info) < 0) {
            ST_WARNING("Failed to layer_load_header[" LAYER_ID_FMT ".", l);
            goto ERR;
        }
        if (*binary != b) {
            ST_WARNING("Both binary and text format in one file.");
            goto ERR;
        }
    }

    for (g = 0; g < num_glue; g++) {
        if (glue_load_header(comp != NULL ? &((*comp)->glues[g]) : NULL,
                    version, fp, &b, fo_info) < 0) {
            ST_WARNING("Failed to glue_load_header["GLUE_ID_FMT"].", g);
            goto ERR;
        }
        if (*binary != b) {
            ST_WARNING("Both binary and text format in one file.");
            goto ERR;
        }
    }

    if (output_wt_load_header(comp != NULL ? &((*comp)->output_wt) : NULL,
                version, fp, &b, fo_info) < 0) {
        ST_WARNING("Failed to output_wt_load_header.");
        goto ERR;
    }
    if (*binary != b) {
        ST_WARNING("Both binary and text format in one file.");
        goto ERR;
    }

    return 0;

ERR:
    if (comp != NULL) {
        safe_comp_destroy(*comp);
    }
    return -1;
}

int comp_load_body(component_t *comp, int version, FILE *fp, bool binary)
{
    int n;
    layer_id_t l;
    glue_id_t g;

    ST_CHECK_PARAM(comp == NULL || fp == NULL, -1);

    if (version < 3) {
        ST_WARNING("Too old version of connlm file");
        return -1;
    }

    if (binary) {
        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read magic num.");
            goto ERR;
        }

        if (n != -COMP_MAGIC_NUM) {
            ST_WARNING("Magic num error.");
            goto ERR;
        }

    } else {
        if (st_readline(fp, "<COMPONENT-DATA>") != 0) {
            ST_WARNING("body flag error.");
            goto ERR;
        }
    }

    if (input_load_body(comp->input, version, fp, binary) < 0) {
        ST_WARNING("Failed to input_load_body.");
        goto ERR;
    }

    if (emb_wt_load_body(comp->emb, version, fp, binary) < 0) {
        ST_WARNING("Failed to emb_wt_load_body.");
        goto ERR;
    }

    for (l = 0; l < comp->num_layer; l++) {
        if (layer_load_body(comp->layers[l], version, fp, binary) < 0) {
            ST_WARNING("Failed to layer_load_body[" LAYER_ID_FMT ".", l);
            goto ERR;
        }
    }

    for (g = 0; g < comp->num_glue; g++) {
        if (glue_load_body(comp->glues[g], version, fp, binary) < 0) {
            ST_WARNING("Failed to glue_load_body[" GLUE_ID_FMT ".", g);
            goto ERR;
        }
    }

    if (output_wt_load_body(comp->output_wt, version, fp, binary) < 0) {
        ST_WARNING("Failed to output_wt_load_body.");
        goto ERR;
    }

    if (comp_construct_graph(comp) < 0) {
        ST_WARNING("Failed to comp_construct_graph.");
        goto ERR;
    }

    return 0;

ERR:
    safe_input_destroy(comp->input);
    safe_emb_wt_destroy(comp->emb);

    for (l = 0; l < comp->num_layer; l++) {
        safe_layer_destroy(comp->layers[l]);
    }

    for (g = 0; g < comp->num_glue; g++) {
        safe_glue_destroy(comp->glues[g]);
    }

    safe_output_wt_destroy(comp->output_wt);

    safe_graph_destroy(comp->graph);

    return -1;
}

int comp_save_header(component_t *comp, FILE *fp, bool binary)
{
    layer_id_t l;
    glue_id_t g;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (binary) {
        if (fwrite(&COMP_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        if (fwrite(comp->name, sizeof(char), MAX_NAME_LEN, fp)
                != MAX_NAME_LEN) {
            ST_WARNING("Failed to write name.");
            return -1;
        }

        if (fwrite(&comp->num_layer, sizeof(layer_id_t), 1, fp) != 1) {
            ST_WARNING("Failed to write num_layer.");
            return -1;
        }

        if (fwrite(&comp->num_glue, sizeof(glue_id_t), 1, fp) != 1) {
            ST_WARNING("Failed to write num_glue.");
            return -1;
        }
    } else {
        if (fprintf(fp, "    \n<COMPONENT>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }

        if (fprintf(fp, "Name: %s\n", comp->name) < 0) {
            ST_WARNING("Failed to fprintf name.");
            return -1;
        }
        if (fprintf(fp, "Num layer: " LAYER_ID_FMT "\n",
                    comp->num_layer) < 0) {
            ST_WARNING("Failed to fprintf num layer.");
            return -1;
        }
        if (fprintf(fp, "Num glue: " GLUE_ID_FMT "\n",
                    comp->num_glue) < 0) {
            ST_WARNING("Failed to fprintf num glue.");
            return -1;
        }
    }

    if (input_save_header(comp->input, fp, binary) < 0) {
        ST_WARNING("Failed to input_save_header.");
        return -1;
    }

    if (emb_wt_save_header(comp->emb, fp, binary) < 0) {
        ST_WARNING("Failed to emb_wt_save_header.");
        return -1;
    }

    for (l = 0; l < comp->num_layer; l++) {
        if (layer_save_header(comp->layers[l], fp, binary) < 0) {
            ST_WARNING("Failed to layer_save_header[" LAYER_ID_FMT ".", l);
            return -1;
        }
    }

    for (g = 0; g < comp->num_glue; g++) {
        if (glue_save_header(comp->glues[g], fp, binary) < 0) {
            ST_WARNING("Failed to glue_save_header[" GLUE_ID_FMT ".", g);
            return -1;
        }
    }

    if (output_wt_save_header(comp->output_wt, fp, binary) < 0) {
        ST_WARNING("Failed to output_wt_save_header.");
        return -1;
    }

    return 0;
}

int comp_save_body(component_t *comp, FILE *fp, bool binary)
{
    int n;
    layer_id_t l;
    glue_id_t g;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (comp == NULL) {
        return 0;
    }

    if (binary) {
        n = -COMP_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }
    } else {
        if (fprintf(fp, "<COMPONENT-DATA>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }
    }

    if (input_save_body(comp->input, fp, binary) < 0) {
        ST_WARNING("Failed to input_save_body.");
        return -1;
    }

    if (emb_wt_save_body(comp->emb, fp, binary) < 0) {
        ST_WARNING("Failed to emb_wt_save_body.");
        return -1;
    }

    for (l = 0; l < comp->num_layer; l++) {
        if (layer_save_body(comp->layers[l], fp, binary) < 0) {
            ST_WARNING("Failed to layer_save_body[" LAYER_ID_FMT ".", l);
            return -1;
        }
    }

    for (g = 0; g < comp->num_glue; g++) {
        if (glue_save_body(comp->glues[g], fp, binary) < 0) {
            ST_WARNING("Failed to glue_save_body[" GLUE_ID_FMT ".", g);
            return -1;
        }
    }

    if (output_wt_save_body(comp->output_wt, fp, binary) < 0) {
        ST_WARNING("Failed to output_wt_save_body.");
        return -1;
    }

    return 0;
}

int comp_fwd_bp(component_t *comp, int word, int tid)
{
    return 0;
}

int comp_forward(component_t *comp, int tid)
{
    ST_CHECK_PARAM(comp == NULL, -1);

    return 0;
}

int comp_backprop(component_t *comp, int word, int tid)
{
    return 0;
}

static inline char* layername2nodename(component_t *comp, layer_id_t l,
        char *node, size_t node_len)
{
    snprintf(node, node_len, "layer_%s_%s", comp->name,
            comp->layers[l]->name);
    return node;
}

int comp_draw(component_t *comp, FILE *fp, const char *output_node)
{
    char label[MAX_LINE_LEN];
    char nodename[MAX_NAME_LEN];
    layer_id_t l, ll;
    glue_id_t g;

    ST_CHECK_PARAM(comp == NULL || fp == NULL, -1);

    fprintf(fp, "    input_%s [label=\"%s/%s\"];\n", comp->name, comp->name,
            input_draw_label(comp->input, label, MAX_LINE_LEN));
    for (l = 0; l < comp->num_layer; l++) {
        fprintf(fp, "    %s [label=\"%s\"];\n",
                layername2nodename(comp, l, nodename, MAX_NAME_LEN),
                layer_draw_label(comp->layers[l], label, MAX_NAME_LEN));
    }

    for (g = 0; g < comp->num_glue; g++) {
        if (strcasecmp(comp->glues[g]->type, DIRECT_GLUE_NAME) == 0) {
            fprintf(fp, "    input_%s -> output [label=\"%s\"];\n",
                    comp->name,
                    glue_draw_label(comp->glues[g], 0, 0,
                        label, MAX_NAME_LEN));
            continue;
        }

        for (l = 0; l < comp->glues[g]->num_in_layer; l++) {
            for (ll = 0; ll < comp->glues[g]->num_out_layer; ll++) {
                fprintf(fp, "    %s -> %s [label=\"%s\"];\n",
                        layername2nodename(comp,
                            comp->glues[g]->in_layers[l],
                            nodename, MAX_NAME_LEN),
                        layername2nodename(comp,
                            comp->glues[g]->out_layers[ll],
                            nodename, MAX_NAME_LEN),
                        glue_draw_label(comp->glues[g], l, ll,
                            label, MAX_NAME_LEN));
            }
        }
    }

    return 0;
}
