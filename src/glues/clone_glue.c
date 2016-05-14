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

#include <stutils/st_log.h>
#include <stutils/st_utils.h>

#include "clone_glue.h"

static int clone_glue_forward(glue_t *glue)
{
    ST_CHECK_PARAM(glue == NULL, -1);

    return 0;
}

static int clone_glue_backprop(glue_t *glue)
{
    ST_CHECK_PARAM(glue == NULL, -1);

    return 0;
}

void clone_glue_destroy(glue_t *glue)
{
    if (glue == NULL) {
        return;
    }

    glue->forward = NULL;
    glue->backprop = NULL;
    glue->extra = NULL;
}

int clone_glue_init(glue_t *glue)
{
    ST_CHECK_PARAM(glue == NULL, -1);

    if (strcasecmp(glue->type, CLONE_GLUE_NAME) != 0) {
        ST_WARNING("Not a clone glue. [%s]", glue->type);
        return -1;
    }

    glue->forward = clone_glue_forward;
    glue->backprop = clone_glue_backprop;
    glue->extra = NULL;

    return 0;
}

int clone_glue_dup(glue_t *dst, glue_t *src)
{
    ST_CHECK_PARAM(dst == NULL || src == NULL, -1);

    if (strcasecmp(dst->type, CLONE_GLUE_NAME) != 0) {
        ST_WARNING("dst is Not a clone glue. [%s]", dst->type);
        return -1;
    }

    if (strcasecmp(src->type, CLONE_GLUE_NAME) != 0) {
        ST_WARNING("src is Not a clone glue. [%s]", src->type);
        return -1;
    }

    dst->forward = src->forward;
    dst->backprop = src->forward;
    dst->extra = src->extra;

    return 0;
}

int clone_glue_parse_topo(glue_t *glue, const char *line)
{
    char *p;

    ST_CHECK_PARAM(glue == NULL || line == NULL, -1);

    p = (char *)line;
    while(*p != '\0') {
        if (*p != ' ' || *p != '\t') {
            ST_WARNING("clone glue should be empty. [%s]", line);
            return -1;
        }
    }

    return 0;
}

bool clone_glue_check(glue_t *glue, layer_t **layers, layer_id_t n_layer)
{
    int i;

    ST_CHECK_PARAM(glue == NULL, false);

    if (strcasecmp(glue->type, CLONE_GLUE_NAME) != 0) {
        ST_WARNING("Not a clone glue. [%s]", glue->type);
        return false;
    }

    if (!glue_check(glue)) {
        ST_WARNING("Failed to glue_check.");
        return false;
    }

    if (glue->num_in_layer != 1) {
        ST_WARNING("clone glue: num_in_layer shoule be equal to 1.");
        return false;
    }

    if (glue->num_out_layer < 1) {
        ST_WARNING("clone glue: num_out_layer shoule be bigger then 1.");
        return false;
    }

    if (layers == NULL) {
        ST_WARNING("No layers.");
        return false;
    }

    for (i = 0; i < glue->num_out_layer; i++) {
        if (layers[glue->out_layers[i]]->size - glue->out_offsets[i]
                != layers[glue->in_layers[0]]->size - glue->in_offsets[0]) {
            ST_WARNING("in_layer[%s]([%d/%d]) size not match with "
                    "out_layer[%s]([%d/%d]).",
                    layers[glue->in_layers[0]]->name,
                    layers[glue->in_layers[0]]->size,
                    glue->in_offsets[0],
                    layers[glue->out_layers[i]]->name,
                    layers[glue->out_layers[i]]->size,
                    glue->out_offsets[i]);
            return false;
        }
    }

    return true;
}
