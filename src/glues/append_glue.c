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

#include "input.h"
#include "output.h"
#include "append_glue.h"

int append_glue_forward(glue_t *glue, int tid)
{
    ST_CHECK_PARAM(glue == NULL, -1);

    return 0;
}

int append_glue_backprop(glue_t *glue, int tid)
{
    ST_CHECK_PARAM(glue == NULL, -1);

    return 0;
}

void append_glue_destroy(glue_t *glue)
{
    if (glue == NULL) {
        return;
    }

    glue->extra = NULL;
}

int append_glue_init(glue_t *glue)
{
    ST_CHECK_PARAM(glue == NULL, -1);

    if (strcasecmp(glue->type, APPEND_GLUE_NAME) != 0) {
        ST_WARNING("Not a append glue. [%s]", glue->type);
        return -1;
    }

    glue->extra = NULL;

    return 0;
}

int append_glue_dup(glue_t *dst, glue_t *src)
{
    ST_CHECK_PARAM(dst == NULL || src == NULL, -1);

    if (strcasecmp(dst->type, APPEND_GLUE_NAME) != 0) {
        ST_WARNING("dst is Not a append glue. [%s]", dst->type);
        return -1;
    }

    if (strcasecmp(src->type, APPEND_GLUE_NAME) != 0) {
        ST_WARNING("src is Not a append glue. [%s]", src->type);
        return -1;
    }

    return 0;
}

int append_glue_parse_topo(glue_t *glue, const char *line)
{
    char *p;

    ST_CHECK_PARAM(glue == NULL || line == NULL, -1);

    p = (char *)line;
    while(*p != '\0') {
        if (*p != ' ' || *p != '\t') {
            ST_WARNING("append glue should be empty. [%s]", line);
            return -1;
        }
    }

    return 0;
}

bool append_glue_check(glue_t *glue, layer_t **layers, int n_layer)
{
    int i;
    int n;

    ST_CHECK_PARAM(glue == NULL, false);

    if (strcasecmp(glue->type, APPEND_GLUE_NAME) != 0) {
        ST_WARNING("Not a append glue. [%s]", glue->type);
        return false;
    }

    if (glue->num_out_layer != 1) {
        ST_WARNING("append glue: num_out_layer shoule be equal to 1.");
        return false;
    }

    if (glue->num_in_layer < 1) {
        ST_WARNING("append glue: num_in_layer shoule be bigger then 1.");
        return false;
    }

    if (layers == NULL) {
        ST_WARNING("No layers.");
        return false;
    }

    n = 0;
    for (i = 0; i < glue->num_in_layer; i++) {
        n += layers[glue->in_layers[i]]->size - glue->in_offsets[i];
        if (strcasecmp(layers[glue->in_layers[i]]->type,
                    INPUT_LAYER_NAME) == 0) {
            ST_WARNING("wt glue: in layer should not be input layer.");
            return false;
        }
    }
    if (strcasecmp(layers[glue->out_layers[0]]->type,
                OUTPUT_LAYER_NAME) == 0) {
        ST_WARNING("wt glue: out layer should not be output layer.");
        return false;
    }
    if (n != layers[glue->out_layers[0]]->size - glue->out_offsets[0]) {
        ST_WARNING("total in_layer size([%d]) not match with "
                "out_layer[%s]([%d/%d]).",
                n,
                layers[glue->out_layers[0]]->name,
                layers[glue->out_layers[0]]->size,
                glue->out_offsets[0]);
        return false;
    }

    return true;
}

char* append_glue_draw_label(glue_t *glue, char *label, size_t label_len)
{
    ST_CHECK_PARAM(glue == NULL || label == NULL || label_len <= 0, NULL);

    label[0] = '\0';

    return label;
}
