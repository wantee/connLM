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
#include "direct_glue.h"

static int direct_glue_forward(glue_t *glue)
{
    ST_CHECK_PARAM(glue == NULL, -1);

    return 0;
}

static int direct_glue_backprop(glue_t *glue)
{
    ST_CHECK_PARAM(glue == NULL, -1);

    return 0;
}

void direct_glue_destroy(glue_t *glue)
{
    if (glue == NULL) {
        return;
    }

    glue->forward = NULL;
    glue->backprop = NULL;
    glue->extra = NULL;
}

int direct_glue_init(glue_t *glue)
{
    ST_CHECK_PARAM(glue == NULL, -1);

    if (strcasecmp(glue->type, DIRECT_GLUE_NAME) != 0) {
        ST_WARNING("Not a direct glue. [%s]", glue->type);
        return -1;
    }

    glue->forward = direct_glue_forward;
    glue->backprop = direct_glue_backprop;
    glue->extra = NULL;

    return 0;
}

int direct_glue_dup(glue_t *dst, glue_t *src)
{
    ST_CHECK_PARAM(dst == NULL || src == NULL, -1);

    if (strcasecmp(dst->type, DIRECT_GLUE_NAME) != 0) {
        ST_WARNING("dst is Not a direct glue. [%s]", dst->type);
        return -1;
    }

    if (strcasecmp(src->type, DIRECT_GLUE_NAME) != 0) {
        ST_WARNING("src is Not a direct glue. [%s]", src->type);
        return -1;
    }

    dst->forward = src->forward;
    dst->backprop = src->forward;
    dst->extra = src->extra;

    return 0;
}

int direct_glue_parse_topo(glue_t *glue, const char *line)
{
    char *p;

    ST_CHECK_PARAM(glue == NULL || line == NULL, -1);

    p = (char *)line;
    while(*p != '\0') {
        if (*p != ' ' || *p != '\t') {
            ST_WARNING("direct glue should be empty. [%s]", line);
            return -1;
        }
    }

    return 0;
}

bool direct_glue_check(glue_t *glue, layer_t **layers, layer_id_t n_layer)
{
    ST_CHECK_PARAM(glue == NULL, false);

    if (strcasecmp(glue->type, DIRECT_GLUE_NAME) != 0) {
        ST_WARNING("Not a direct glue. [%s]", glue->type);
        return false;
    }

    if (glue->num_in_layer != 1) {
        ST_WARNING("direct glue: in_layer should be equal to 1.");
        return false;
    }

    if (glue->num_out_layer != 1) {
        ST_WARNING("direct glue: out_layer shoule be equal to 1.");
        return false;
    }

    if (strcasecmp(layers[glue->in_layers[0]]->type,
                INPUT_LAYER_NAME) != 0) {
        ST_WARNING("wt glue: in layer should be input layer.");
        return false;
    }

    if (strcasecmp(layers[glue->out_layers[0]]->type,
                OUTPUT_LAYER_NAME) != 0) {
        ST_WARNING("wt glue: out layer should be output layer.");
        return false;
    }

    return true;
}

char* direct_glue_draw_label(glue_t *glue, char *label, size_t label_len)
{
    ST_CHECK_PARAM(glue == NULL || label == NULL, NULL);

    snprintf(label, label_len, "");

    return label;
}
