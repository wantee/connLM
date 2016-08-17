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

#include "input.h"
#include "output.h"
#include "fc_glue.h"

int fc_glue_parse_topo(glue_t *glue, const char *line)
{
    char *p;

    ST_CHECK_PARAM(glue == NULL || line == NULL, -1);

    p = (char *)line;
    while(*p != '\0') {
        if (*p != ' ' || *p != '\t') {
            ST_WARNING("wt glue should be empty. [%s]", line);
            return -1;
        }
    }

    return 0;
}

bool fc_glue_check(glue_t *glue, layer_t **layers, int n_layer,
        input_t *input, output_t *output)
{
    ST_CHECK_PARAM(glue == NULL, false);

    if (strcasecmp(glue->type, FC_GLUE_NAME) != 0) {
        ST_WARNING("Not a fc glue. [%s]", glue->type);
        return false;
    }

    if (layers == NULL) {
        ST_WARNING("No layers.");
        return false;
    }

    if (strcasecmp(layers[glue->in_layer]->type,
                INPUT_LAYER_NAME) == 0) {
        ST_WARNING("fc_wt glue: in layer should not be input layer.");
        return false;
    }

    if (strcasecmp(layers[glue->out_layer]->type,
                OUTPUT_LAYER_NAME) == 0) {
        ST_WARNING("fc_wt glue: out layer should not be output layer.");
        return false;
    }

    return true;
}

int fc_glue_init_data(glue_t *glue, input_t *input,
        layer_t **layers, output_t *output)
{
    ST_CHECK_PARAM(glue == NULL || glue->wt == NULL || input == NULL, -1);

    if (strcasecmp(glue->type, FC_GLUE_NAME) != 0) {
        ST_WARNING("Not a fc glue. [%s]", glue->type);
        return -1;
    }

    if (wt_init(glue->wt, layers[glue->out_layer]->size,
                layers[glue->in_layer]->size) < 0) {
        ST_WARNING("Failed to wt_init.");
        return -1;
    }

    return 0;
}
