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
#include "out_wt_glue.h"

int out_wt_glue_forward(glue_t *glue, int tid)
{
    ST_CHECK_PARAM(glue == NULL, -1);

    return 0;
}

int out_wt_glue_backprop(glue_t *glue, int tid)
{
    ST_CHECK_PARAM(glue == NULL, -1);

    return 0;
}

#define safe_out_wt_glue_data_destroy(ptr) do {\
    if((ptr) != NULL) {\
        out_wt_glue_data_destroy((out_wt_glue_data_t *)ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)

void out_wt_glue_data_destroy(out_wt_glue_data_t *data)
{
    if (data == NULL) {
        return;
    }
    safe_out_wt_destroy(data->out_wt);
}

out_wt_glue_data_t* out_wt_glue_data_init()
{
    out_wt_glue_data_t *data = NULL;

    data = (out_wt_glue_data_t *)malloc(sizeof(out_wt_glue_data_t));
    if (data == NULL) {
        ST_WARNING("Failed to malloc out_wt_glue_data.");
        goto ERR;
    }
    memset(data, 0, sizeof(out_wt_glue_data_t));

    return data;
ERR:
    safe_out_wt_glue_data_destroy(data);
    return NULL;
}

out_wt_glue_data_t* out_wt_glue_data_dup(out_wt_glue_data_t *src)
{
    out_wt_glue_data_t *dst = NULL;

    ST_CHECK_PARAM(src == NULL, NULL);

    dst = out_wt_glue_data_init();
    if (dst == NULL) {
        ST_WARNING("Failed to out_wt_glue_data_init.");
        goto ERR;
    }

    dst->out_wt = out_wt_dup(src->out_wt);
    if (dst->out_wt == NULL) {
        ST_WARNING("Failed to weight_dup.");
        goto ERR;
    }

    return dst;
ERR:
    safe_out_wt_glue_data_destroy(dst);
    return NULL;
}

void out_wt_glue_destroy(glue_t *glue)
{
    if (glue == NULL) {
        return;
    }

    safe_out_wt_glue_data_destroy(glue->extra);
}

int out_wt_glue_init(glue_t *glue)
{
    ST_CHECK_PARAM(glue == NULL, -1);

    if (strcasecmp(glue->type, OUT_WT_GLUE_NAME) != 0) {
        ST_WARNING("Not a out_wt glue. [%s]", glue->type);
        return -1;
    }

    glue->extra = (void *)out_wt_glue_data_init();
    if (glue->extra == NULL) {
        ST_WARNING("Failed to out_wt_glue_data_init.");
        goto ERR;
    }

    return 0;

ERR:
    safe_out_wt_glue_data_destroy(glue->extra);
    return -1;
}

int out_wt_glue_dup(glue_t *dst, glue_t *src)
{
    ST_CHECK_PARAM(dst == NULL || src == NULL, -1);

    if (strcasecmp(dst->type, OUT_WT_GLUE_NAME) != 0) {
        ST_WARNING("dst is Not a out_wt glue. [%s]", dst->type);
        return -1;
    }

    if (strcasecmp(src->type, OUT_WT_GLUE_NAME) != 0) {
        ST_WARNING("src is Not a out_wt glue. [%s]", src->type);
        return -1;
    }

    dst->extra = (void *)out_wt_glue_data_dup((out_wt_glue_data_t *)src->extra);
    if (dst->extra == NULL) {
        ST_WARNING("Failed to out_wt_glue_data_dup.");
        goto ERR;
    }

    return 0;
ERR:
    safe_out_wt_glue_data_destroy(dst->extra);
    return -1;
}

int out_wt_glue_parse_topo(glue_t *glue, const char *line)
{
    char *p;

    ST_CHECK_PARAM(glue == NULL || line == NULL, -1);

    p = (char *)line;
    while(*p != '\0') {
        if (*p != ' ' || *p != '\t') {
            ST_WARNING("out_wt_glue should be empty. [%s]", line);
            return -1;
        }
    }

    return 0;
}

bool out_wt_glue_check(glue_t *glue, layer_t **layers, int n_layer)
{
    ST_CHECK_PARAM(glue == NULL, false);

    if (strcasecmp(glue->type, OUT_WT_GLUE_NAME) != 0) {
        ST_WARNING("Not a out_wt glue. [%s]", glue->type);
        return false;
    }

    if (glue->num_out_layer != 1) {
        ST_WARNING("out_wt glue: num_out_layer shoule be equal to 1.");
        return false;
    }

    if (glue->num_in_layer != 1) {
        ST_WARNING("out_wt glue: num_in_layer shoule be equal to 1.");
        return false;
    }

    if (layers == NULL) {
        ST_WARNING("No layers.");
        return false;
    }

    if (strcasecmp(layers[glue->in_layers[0]]->type,
                INPUT_LAYER_NAME) == 0) {
        ST_WARNING("out_wt glue: in layer should not be input layer.");
        return false;
    }

    if (strcasecmp(layers[glue->out_layers[0]]->type,
                OUTPUT_LAYER_NAME) != 0) {
        ST_WARNING("out_wt glue: out layer should be output layer.");
        return false;
    }

    return true;
}

char* out_wt_glue_draw_label(glue_t *glue, char *label, size_t label_len)
{
    ST_CHECK_PARAM(glue == NULL || label == NULL || label_len <= 0, NULL);

    label[0] = '\0';

    return label;
}

int out_wt_glue_load_header(void **extra, int version,
        FILE *fp, bool *binary, FILE *fo_info)
{
    out_wt_glue_data_t *data = NULL;

    if (extra != NULL) {
        data = out_wt_glue_data_init();
        if (data == NULL) {
            ST_WARNING("Failed to out_wt_glue_data_init.");
            goto ERR;
        }

        *extra = (void *)data;
    }

    if (out_wt_load_header(data != NULL ? &(data->out_wt) : NULL,
                version, fp, binary, fo_info) < 0) {
        ST_WARNING("Failed to out_wt_load_header.");
        goto ERR;
    }

    return 0;

ERR:
    safe_out_wt_glue_data_destroy(data);
    if (extra != NULL) {
        *extra = NULL;
    }
    return -1;
}

int out_wt_glue_load_body(void *extra, int version, FILE *fp, bool binary)
{
    out_wt_glue_data_t *data = NULL;

    data = (out_wt_glue_data_t *)extra;

    if (out_wt_load_body(data->out_wt, version, fp, binary) < 0) {
        ST_WARNING("Failed to out_wt_load_body.");
        return -1;
    }

    return 0;
}

int out_wt_glue_save_header(void *extra, FILE *fp, bool binary)
{
    out_wt_glue_data_t *data = NULL;

    data = (out_wt_glue_data_t *)extra;

    if (out_wt_save_header(data->out_wt, fp, binary) < 0) {
        ST_WARNING("Failed to out_wt_save_header.");
        return -1;
    }

    return 0;
}

int out_wt_glue_save_body(void *extra, FILE *fp, bool binary)
{
    out_wt_glue_data_t *data = NULL;

    data = (out_wt_glue_data_t *)extra;

    if (out_wt_save_body(data->out_wt, fp, binary) < 0) {
        ST_WARNING("Failed to out_wt_save_body.");
        return -1;
    }

    return 0;
}

int out_wt_glue_init_data(glue_t *glue, input_t *input,
        layer_t **layers, output_t *output)
{
    out_wt_glue_data_t *data;

    ST_CHECK_PARAM(glue == NULL || layers == NULL || output == NULL, -1);

    if (strcasecmp(glue->type, OUT_WT_GLUE_NAME) != 0) {
        ST_WARNING("Not a out_wt glue. [%s]", glue->type);
        return -1;
    }

    data = (out_wt_glue_data_t *)glue->extra;
    data->out_wt = out_wt_init(layers[glue->in_layers[0]]->size,
            output->tree->num_node);
    if (data->out_wt == NULL) {
        ST_WARNING("Failed to out_wt_init.");
        return -1;
    }

    return 0;
}

int out_wt_glue_load_train_opt(glue_t *glue, st_opt_t *opt,
        const char *sec_name, param_t *parent)
{
    out_wt_glue_data_t *data;

    ST_CHECK_PARAM(glue == NULL || opt == NULL, -1);

    if (strcasecmp(glue->type, OUT_WT_GLUE_NAME) != 0) {
        ST_WARNING("Not a out_wt glue. [%s]", glue->type);
        return -1;
    }

    data = (out_wt_glue_data_t *)glue->extra;

    if (param_load(&data->param, opt, sec_name, parent) < 0) {
        ST_WARNING("Failed to param_load.");
        goto ST_OPT_ERR;
    }

    return 0;

ST_OPT_ERR:
    return -1;
}
