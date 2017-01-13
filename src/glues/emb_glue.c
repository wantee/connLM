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
#include "emb_glue.h"

int emb_glue_parse_topo(glue_t *glue, const char *line)
{
    char *p;

    ST_CHECK_PARAM(glue == NULL || line == NULL, -1);

    p = (char *)line;
    while(*p != '\0') {
        if (*p != ' ' || *p != '\t') {
            ST_WARNING("emb_glue should be empty. [%s]", line);
            return -1;
        }
    }

    return 0;
}

bool emb_glue_check(glue_t *glue, layer_t **layers, int n_layer,
        input_t *input, output_t *output)
{
    ST_CHECK_PARAM(glue == NULL || input == NULL, false);

    if (strcasecmp(glue->type, EMB_GLUE_NAME) != 0) {
        ST_WARNING("Not a emb glue. [%s]", glue->type);
        return false;
    }

    if (layers == NULL) {
        ST_WARNING("No layers.");
        return false;
    }

    if (strcasecmp(layers[glue->in_layer]->type,
                INPUT_LAYER_NAME) != 0) {
        ST_WARNING("emb glue: in layer should be input layer.");
        return false;
    }

    if (strcasecmp(layers[glue->out_layer]->type,
                OUTPUT_LAYER_NAME) == 0) {
        ST_WARNING("emb glue: out layer should not be output layer.");
        return false;
    }

    if (input->n_ctx > 1) {
        if (input->combine == IC_UNDEFINED) {
            ST_WARNING("emb glue: No combine specified in input.");
            return false;
        }
    } else {
        if (input->combine == IC_UNDEFINED) {
            input->combine = IC_CONCAT;
        }
    }

    if (input->combine == IC_CONCAT) {
        if (glue->out_length % input->n_ctx != 0) {
            ST_WARNING("emb glue: can not apply CONCAT combination. "
                    "out layer length can not divided by context number.");
            return false;
        }
    }

    return true;
}

int emb_glue_init_data(glue_t *glue, input_t *input,
        layer_t **layers, output_t *output)
{
    int col;

    ST_CHECK_PARAM(glue == NULL || glue->wt == NULL
            || input == NULL, -1);

    if (strcasecmp(glue->type, EMB_GLUE_NAME) != 0) {
        ST_WARNING("Not a emb glue. [%s]", glue->type);
        return -1;
    }

    if (input->combine == IC_CONCAT) {
        col = glue->out_length / input->n_ctx;
    } else {
        col = glue->out_length;
    }

    if (wt_init(glue->wt, input->input_size, col) < 0) {
        ST_WARNING("Failed to wt_init.");
        return -1;
    }

    return 0;
}

wt_updater_t* emb_glue_init_wt_updater(glue_t *glue, param_t *param)
{
    wt_updater_t *wt_updater = NULL;

    ST_CHECK_PARAM(glue == NULL, NULL);

    if (strcasecmp(glue->type, EMB_GLUE_NAME) != 0) {
        ST_WARNING("Not a emb glue. [%s]", glue->type);
        return NULL;
    }

    wt_updater = wt_updater_create(param == NULL ? &glue->param : param,
            glue->wt->mat, glue->wt->row, glue->wt->col, WT_UT_ONE_SHOT);
    if (wt_updater == NULL) {
        ST_WARNING("Failed to wt_updater_create.");
        goto ERR;
    }

    return wt_updater;

ERR:
    safe_wt_updater_destroy(wt_updater);
    return NULL;
}

int emb_glue_generate_wildcard_repr(glue_t *glue)
{
    int i, j;

    ST_CHECK_PARAM(glue == NULL, -1);

    if (strcasecmp(glue->type, EMB_GLUE_NAME) != 0) {
        ST_WARNING("Not a emb glue. [%s]", glue->type);
        return -1;
    }

    glue->wildcard_repr = (real_t *)malloc(sizeof(real_t) * glue->wt->col);
    if (glue->wildcard_repr == NULL) {
        ST_WARNING("Failed to mallco wildcard_repr.");
        return -1;
    }

    // use the mean of embedding of all words (except </s>) as the repr
    for (j = 0; j < glue->wt->col; j++) {
        glue->wildcard_repr[j] = 0;
        for (i = 0; i < glue->wt->row; i++) {
            if (i == SENT_END_ID) {
                continue;
            }
            glue->wildcard_repr[j] += glue->wt->mat[i * glue->wt->col + j];
        }
        glue->wildcard_repr[j] /= (glue->wt->row - 1);
    }

    return 0;
}
