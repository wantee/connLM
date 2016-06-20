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
#include "sum_glue.h"

int sum_glue_forward(glue_t *glue, int tid)
{
    ST_CHECK_PARAM(glue == NULL, -1);

    return 0;
}

int sum_glue_backprop(glue_t *glue, int tid)
{
    ST_CHECK_PARAM(glue == NULL, -1);

    return 0;
}

#define safe_sum_glue_data_destroy(ptr) do {\
    if((ptr) != NULL) {\
        sum_glue_data_destroy((sum_glue_data_t *)ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)

void sum_glue_data_destroy(sum_glue_data_t *data)
{
    if (data == NULL) {
        return;
    }

    data->avg = false;
}

sum_glue_data_t* sum_glue_data_init()
{
    sum_glue_data_t *data = NULL;

    data = (sum_glue_data_t *)malloc(sizeof(sum_glue_data_t));
    if (data == NULL) {
        ST_WARNING("Failed to malloc sum_glue_data.");
        goto ERR;
    }
    memset(data, 0, sizeof(sum_glue_data_t));

    return data;
ERR:
    safe_sum_glue_data_destroy(data);
    return NULL;
}

sum_glue_data_t* sum_glue_data_dup(sum_glue_data_t *src)
{
    sum_glue_data_t *dst = NULL;

    ST_CHECK_PARAM(src == NULL, NULL);

    dst = sum_glue_data_init();
    if (dst == NULL) {
        ST_WARNING("Failed to sum_glue_data_init.");
        goto ERR;
    }

    dst->avg = src->avg;

    return dst;
ERR:
    safe_sum_glue_data_destroy(dst);
    return NULL;
}

void sum_glue_destroy(glue_t *glue)
{
    if (glue == NULL) {
        return;
    }

    safe_sum_glue_data_destroy(glue->extra);
}

int sum_glue_init(glue_t *glue)
{
    ST_CHECK_PARAM(glue == NULL, -1);

    if (strcasecmp(glue->type, SUM_GLUE_NAME) != 0) {
        ST_WARNING("Not a sum glue. [%s]", glue->type);
        return -1;
    }

    glue->extra = (void *)sum_glue_data_init();
    if (glue->extra == NULL) {
        ST_WARNING("Failed to sum_glue_data_init.");
        goto ERR;
    }

    return 0;

ERR:
    safe_sum_glue_data_destroy(glue->extra);
    return -1;
}

int sum_glue_dup(glue_t *dst, glue_t *src)
{
    ST_CHECK_PARAM(dst == NULL || src == NULL, -1);

    if (strcasecmp(dst->type, SUM_GLUE_NAME) != 0) {
        ST_WARNING("dst is Not a sum glue. [%s]", dst->type);
        return -1;
    }

    if (strcasecmp(src->type, SUM_GLUE_NAME) != 0) {
        ST_WARNING("src is Not a sum glue. [%s]", src->type);
        return -1;
    }

    dst->extra = (void *)sum_glue_data_dup((sum_glue_data_t *)src->extra);
    if (dst->extra == NULL) {
        ST_WARNING("Failed to sum_glue_data_dup.");
        goto ERR;
    }

    return 0;
ERR:
    safe_sum_glue_data_destroy(dst->extra);
    return -1;
}

int sum_glue_parse_topo(glue_t *glue, const char *line)
{
    char keyvalue[2*MAX_LINE_LEN];
    char token[MAX_LINE_LEN];

    sum_glue_data_t *data = NULL;
    const char *p;

    ST_CHECK_PARAM(glue == NULL || line == NULL, -1);

    data = (sum_glue_data_t *)glue->extra;

    p = line;

    while (p != NULL) {
        p = get_next_token(p, token);
        if (*token == '\0') {
            break;
        }

        if (split_line(token, keyvalue, 2, MAX_LINE_LEN, "=") != 2) {
            ST_WARNING("Failed to split key/value. [%s]", token);
            goto ERR;
        }

        if (strcasecmp("avg", keyvalue) == 0) {
            data->avg = str2bool(keyvalue + MAX_LINE_LEN);
        } else {
            ST_WARNING("Unknown key/value[%s]", token);
        }
    }

    return 0;

ERR:
    return -1;
}

bool sum_glue_check(glue_t *glue, layer_t **layers, layer_id_t n_layer)
{
    int i;

    ST_CHECK_PARAM(glue == NULL, false);

    if (strcasecmp(glue->type, SUM_GLUE_NAME) != 0) {
        ST_WARNING("Not a sum glue. [%s]", glue->type);
        return false;
    }

    if (glue->num_out_layer != 1) {
        ST_WARNING("sum glue: num_out_layer shoule be equal to 1.");
        return false;
    }

    if (glue->num_in_layer < 1) {
        ST_WARNING("sum glue: num_in_layer shoule be bigger then 1.");
        return false;
    }

    if (layers == NULL) {
        ST_WARNING("No layers.");
        return false;
    }

    if (strcasecmp(layers[glue->out_layers[0]]->type,
                OUTPUT_LAYER_NAME) == 0) {
        ST_WARNING("wt glue: out layer should not be output layer.");
        return false;
    }

    for (i = 0; i < glue->num_in_layer; i++) {
        if (strcasecmp(layers[glue->in_layers[i]]->type,
                    INPUT_LAYER_NAME) == 0) {
            ST_WARNING("wt glue: in layer should not be input layer.");
            return false;
        }

        if (layers[glue->in_layers[i]]->size - glue->in_offsets[i]
                != layers[glue->out_layers[0]]->size - glue->out_offsets[0]) {
            ST_WARNING("in_layer[%s]([%d/%d]) size not match with "
                    "out_layer[%s]([%d/%d]).",
                    layers[glue->in_layers[i]]->name,
                    layers[glue->in_layers[i]]->size,
                    glue->in_offsets[i],
                    layers[glue->out_layers[0]]->name,
                    layers[glue->out_layers[0]]->size,
                    glue->out_offsets[0]);
            return false;
        }
    }

    return true;
}

char* sum_glue_draw_label(glue_t *glue, char *label, size_t label_len)
{
    sum_glue_data_t *data;

    ST_CHECK_PARAM(glue == NULL || label == NULL, NULL);

    data = (sum_glue_data_t *)glue->extra;

    snprintf(label, label_len, ",avg=%s", bool2str(data->avg));

    return label;
}
