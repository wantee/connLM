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
#include "wt_glue.h"

int wt_glue_forward(glue_t *glue, int tid)
{
    ST_CHECK_PARAM(glue == NULL, -1);

    return 0;
}

int wt_glue_backprop(glue_t *glue, int tid)
{
    ST_CHECK_PARAM(glue == NULL, -1);

    return 0;
}

#define safe_wt_glue_data_destroy(ptr) do {\
    if((ptr) != NULL) {\
        wt_glue_data_destroy((wt_glue_data_t *)ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)

void wt_glue_data_destroy(wt_glue_data_t *data)
{
    if (data == NULL) {
        return;
    }
    safe_wt_destroy(data->wt);
}

wt_glue_data_t* wt_glue_data_init()
{
    wt_glue_data_t *data = NULL;

    data = (wt_glue_data_t *)malloc(sizeof(wt_glue_data_t));
    if (data == NULL) {
        ST_WARNING("Failed to malloc wt_glue_data.");
        goto ERR;
    }
    memset(data, 0, sizeof(wt_glue_data_t));

    return data;
ERR:
    safe_wt_glue_data_destroy(data);
    return NULL;
}

wt_glue_data_t* wt_glue_data_dup(wt_glue_data_t *src)
{
    wt_glue_data_t *dst = NULL;

    ST_CHECK_PARAM(src == NULL, NULL);

    dst = wt_glue_data_init();
    if (dst == NULL) {
        ST_WARNING("Failed to wt_glue_data_init.");
        goto ERR;
    }

    dst->wt = wt_dup(src->wt);
    if (dst->wt == NULL) {
        ST_WARNING("Failed to weight_dup.");
        goto ERR;
    }

    return dst;
ERR:
    safe_wt_glue_data_destroy(dst);
    return NULL;
}

void wt_glue_destroy(glue_t *glue)
{
    if (glue == NULL) {
        return;
    }

    safe_wt_glue_data_destroy(glue->extra);
}

int wt_glue_init(glue_t *glue)
{
    ST_CHECK_PARAM(glue == NULL, -1);

    if (strcasecmp(glue->type, WT_GLUE_NAME) != 0) {
        ST_WARNING("Not a wt glue. [%s]", glue->type);
        return -1;
    }

    glue->extra = (void *)wt_glue_data_init();
    if (glue->extra == NULL) {
        ST_WARNING("Failed to wt_glue_data_init.");
        goto ERR;
    }

    return 0;

ERR:
    safe_wt_glue_data_destroy(glue->extra);
    return -1;
}

int wt_glue_dup(glue_t *dst, glue_t *src)
{
    ST_CHECK_PARAM(dst == NULL || src == NULL, -1);

    if (strcasecmp(dst->type, WT_GLUE_NAME) != 0) {
        ST_WARNING("dst is Not a wt glue. [%s]", dst->type);
        return -1;
    }

    if (strcasecmp(src->type, WT_GLUE_NAME) != 0) {
        ST_WARNING("src is Not a wt glue. [%s]", src->type);
        return -1;
    }

    dst->extra = (void *)wt_glue_data_dup((wt_glue_data_t *)src->extra);
    if (dst->extra == NULL) {
        ST_WARNING("Failed to wt_glue_data_dup.");
        goto ERR;
    }

    return 0;
ERR:
    safe_wt_glue_data_destroy(dst->extra);
    return -1;
}

int wt_glue_parse_topo(glue_t *glue, const char *line)
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

bool wt_glue_check(glue_t *glue, layer_t **layers, int n_layer)
{
    ST_CHECK_PARAM(glue == NULL, false);

    if (strcasecmp(glue->type, WT_GLUE_NAME) != 0) {
        ST_WARNING("Not a wt glue. [%s]", glue->type);
        return false;
    }

    if (glue->num_out_layer != 1) {
        ST_WARNING("wt glue: num_out_layer shoule be equal to 1.");
        return false;
    }

    if (glue->num_in_layer != 1) {
        ST_WARNING("wt glue: num_in_layer shoule be equal to 1.");
        return false;
    }

    if (layers == NULL) {
        ST_WARNING("No layers.");
        return false;
    }

    if (strcasecmp(layers[glue->in_layers[0]]->type,
                INPUT_LAYER_NAME) == 0) {
        ST_WARNING("wt glue: in layer should not be input layer.");
        return false;
    }

    if (strcasecmp(layers[glue->out_layers[0]]->type,
                OUTPUT_LAYER_NAME) == 0) {
        ST_WARNING("wt glue: out layer should not be output layer.");
        return false;
    }

    return true;
}

char* wt_glue_draw_label(glue_t *glue, char *label, size_t label_len)
{
    ST_CHECK_PARAM(glue == NULL || label == NULL, NULL);

    snprintf(label, label_len, "");

    return label;
}

int wt_glue_load_header(void **extra, int version,
        FILE *fp, bool *binary, FILE *fo_info)
{
    wt_glue_data_t *data = NULL;

    if (extra != NULL) {
        data = wt_glue_data_init();
        if (data == NULL) {
            ST_WARNING("Failed to wt_glue_data_init.");
            goto ERR;
        }

        *extra = (void *)data;
    }

    if (wt_load_header(data != NULL ? &(data->wt) : NULL,
                version, fp, binary, fo_info) < 0) {
        ST_WARNING("Failed to wt_load_header.");
        goto ERR;
    }

    return 0;

ERR:
    safe_wt_glue_data_destroy(data);
    if (extra != NULL) {
        *extra = NULL;
    }
    return -1;
}

int wt_glue_load_body(void *extra, int version, FILE *fp, bool binary)
{
    wt_glue_data_t *data = NULL;

    data = (wt_glue_data_t *)extra;

    if (wt_load_body(data->wt, version, fp, binary) < 0) {
        ST_WARNING("Failed to wt_load_body.");
        return -1;
    }

    return 0;
}

int wt_glue_save_header(void *extra, FILE *fp, bool binary)
{
    wt_glue_data_t *data = NULL;

    data = (wt_glue_data_t *)extra;

    if (wt_save_header(data->wt, fp, binary) < 0) {
        ST_WARNING("Failed to wt_save_header.");
        return -1;
    }

    return 0;
}

int wt_glue_save_body(void *extra, FILE *fp, bool binary)
{
    wt_glue_data_t *data = NULL;

    data = (wt_glue_data_t *)extra;

    if (wt_save_body(data->wt, fp, binary) < 0) {
        ST_WARNING("Failed to wt_save_body.");
        return -1;
    }

    return 0;
}

int wt_glue_init_data(glue_t *glue, input_t *input,
        layer_t **layers, output_t *output)
{
    wt_glue_data_t *data = NULL;

    ST_CHECK_PARAM(glue == NULL || layers == NULL, -1);

    if (strcasecmp(glue->type, WT_GLUE_NAME) != 0) {
        ST_WARNING("Not a wt glue. [%s]", glue->type);
        return -1;
    }

    data = (wt_glue_data_t *)glue->extra;
    data->wt = wt_init(layers[glue->out_layers[0]]->size,
                layers[glue->in_layers[0]]->size);
    if (data->wt == NULL) {
        ST_WARNING("Failed to wt_init.");
        return -1;
    }

    return 0;
}

int wt_glue_load_train_opt(glue_t *glue, st_opt_t *opt,
        const char *sec_name, param_t *parent)
{
    wt_glue_data_t *data;

    ST_CHECK_PARAM(glue == NULL || opt == NULL, -1);

    if (strcasecmp(glue->type, WT_GLUE_NAME) != 0) {
        ST_WARNING("Not a wt glue. [%s]", glue->type);
        return -1;
    }

    data = (wt_glue_data_t *)glue->extra;

    if (param_load(&data->param, opt, sec_name, parent) < 0) {
        ST_WARNING("Failed to param_load.");
        goto ST_OPT_ERR;
    }

    return 0;

ST_OPT_ERR:
    return -1;
}
