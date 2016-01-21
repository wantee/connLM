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
#include "glue.h"

typedef struct _glue_register_t_ {
    char type[MAX_NAME_LEN];
    int (*init)(glue_t *glue);
    void (*destroy)(glue_t *glue);
    int (*dup)(glue_t *dst, glue_t *src);
    int (*parse_topo)(glue_t *glue, const char *line);
    bool (*check)(glue_t *glue, layer_t **layers, layer_id_t n_layer);
} glue_reg_t;

static glue_reg_t GLUE_REG[] = {
    {SUM_GLUE_NAME, sum_glue_init, sum_glue_destroy,
        sum_glue_dup, sum_glue_parse_topo, sum_glue_check},
    {APPEND_GLUE_NAME, append_glue_init, append_glue_destroy,
        append_glue_dup, append_glue_parse_topo, append_glue_check},
    {CLONE_GLUE_NAME, clone_glue_init, clone_glue_destroy,
        clone_glue_dup, clone_glue_parse_topo, clone_glue_check},
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

    ST_CHECK_PARAM(line == NULL || layers == NULL || n_layer <= 0, NULL);

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
        ST_WARNING("No layer name found.");
        goto ERR;
    }
    if (reg == NULL) {
        ST_WARNING("No type found.");
        goto ERR;
    }
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

glue_t* glue_dup(glue_t *l)
{
    glue_t *glue = NULL;

    ST_CHECK_PARAM(l == NULL, NULL);

    glue = (glue_t *) malloc(sizeof(glue_t));
    if (glue == NULL) {
        ST_WARNING("Falied to malloc glue_t.");
        goto ERR;
    }
    memset(glue, 0, sizeof(glue_t));

    strncpy(glue->name, l->name, MAX_NAME_LEN);
    glue->name[MAX_NAME_LEN - 1] = '\0';

    glue->forward = l->forward;
    glue->backprop = l->backprop;

    return glue;

ERR:
    safe_glue_destroy(glue);
    return NULL;
}
