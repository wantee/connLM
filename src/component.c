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

#include <st_log.h>
#include <st_utils.h>

#include "component.h"

void comp_destroy(component_t *comp)
{
    int i;

    if (comp == NULL) {
        return;
    }

    comp->name[0] = 0;
    comp->id = -1;

    safe_input_destroy(comp->input);
    safe_emb_wt_destroy(comp->emb);

    for (i = 0; i < comp->num_layer; i++) {
        safe_layer_destroy(comp->layers[i]);
    }
    safe_free(comp->layers);
    comp->num_layer = 0;

    for (i = 0; i < comp->num_wt; i++) {
        safe_wt_destroy(comp->wts[i]);
    }
    safe_free(comp->wts);
    comp->num_wt = 0;

    safe_output_wt_destroy(comp->output_wt);
}

component_t* comp_dup(component_t *c)
{
    component_t *comp = NULL;

    int i;

    ST_CHECK_PARAM(c == NULL, NULL);

    comp = (component_t *)malloc(sizeof(component_t));
    if (comp == NULL) {
        ST_WARNING("Falied to malloc component_t.");
        goto ERR;
    }
    memset(comp, 0, sizeof(component_t));

    strncpy(comp->name, c->name, MAX_NAME_LEN);
    comp->name[MAX_NAME_LEN - 1] = '\0';
    comp->id = c->id;

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

        for (i = 0; i < c->num_layer; i++) {
            comp->layers[i] = layer_dup(c->layers[i]);
            if (comp->layers[i] == NULL) {
                ST_WARNING("Failed to layer_dup.");
                goto ERR;
            }
        }
        comp->num_layer = c->num_layer;
    }

    if (c->num_wt > 0) {
        comp->wts = (weight_t **)malloc(sizeof(weight_t *)
                * c->num_wt);
        if (comp->wts == NULL) {
            ST_WARNING("Failed to malloc wts.");
            goto ERR;
        }

        for (i = 0; i < c->num_wt; i++) {
            comp->wts[i] = wt_dup(c->wts[i]);
            if (comp->wts[i] == NULL) {
                ST_WARNING("Failed to wt_dup.");
                goto ERR;
            }
        }
        comp->num_wt = c->num_wt;
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
    ST_CHECK_PARAM(comp == NULL || line == NULL, -1);

    return 0;
}

component_t *comp_init_from_topo(FILE *topo_fp)
{
    component_t *comp = NULL;

    char token[MAX_LINE_LEN];
    char *line = NULL;
    size_t line_sz = 0;

    bool err;
    bool has_comp;

    ST_CHECK_PARAM(topo_fp == NULL, NULL);

    has_comp = false;
    while (st_fgets(&line, &line_sz, topo_fp, &err)) {
        remove_newline(line);
        remove_leading_space(line);

        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        if (has_comp) {
            if (strncasecmp("</component>", line, 12) == 0) {
                break;
            } 
        } else {
            if (strncasecmp("<component>", line, 11) == 0) {
                has_comp = true;
            }
            continue;
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
        } else if (strcasecmp("layer", token) == 0) {
            if (comp->layers == NULL) {
                comp->layers = (layer_t **)malloc(sizeof(layer_t *));
            } else {
                comp->layers = (layer_t **)realloc(comp->layers,
                        sizeof(layer_t *) * (comp->num_layer + 1));
            }
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
        } else if (strcasecmp("weight", token) == 0) {
            if (comp->wts == NULL) {
                comp->wts = (weight_t **)malloc(sizeof(weight_t *));
            } else {
                comp->wts = (weight_t **)realloc(comp->wts,
                        sizeof(weight_t *) * (comp->num_wt + 1));
            }
            if (comp->wts == NULL) {
                ST_WARNING("Failed to alloc wts.");
                goto ERR;
            }

            comp->wts[comp->num_wt] = wt_parse_topo(line);
            if (comp->wts[comp->num_wt] == NULL) {
                ST_WARNING("Failed to wt_parse_topo.");
                goto ERR;
            }
            comp->num_wt++;
        }
    }

    if (err) {
        ST_WARNING("st_fgets error.");
        goto ERR;
    }

    if (!has_comp) {
        ST_WARNING("No component found.");
        goto ERR;
    }

    safe_free(line);

    return comp;

ERR:
    safe_free(line);

    comp_destroy(comp);
    return NULL;
}

int comp_init_output_wt(component_t *comp, output_t *output)
{
    ST_CHECK_PARAM(comp == NULL || output == NULL, -1);

    return 0;
}

