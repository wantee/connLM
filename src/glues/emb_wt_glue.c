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
#include "emb_wt_glue.h"

static int emb_wt_glue_forward(glue_t *glue)
{
    ST_CHECK_PARAM(glue == NULL, -1);

    return 0;
}

static int emb_wt_glue_backprop(glue_t *glue)
{
    ST_CHECK_PARAM(glue == NULL, -1);

    return 0;
}

#define safe_emb_wt_glue_data_destroy(ptr) do {\
    if((ptr) != NULL) {\
        emb_wt_glue_data_destroy((emb_wt_glue_data_t *)ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)

void emb_wt_glue_data_destroy(emb_wt_glue_data_t *data)
{
    if (data == NULL) {
        return;
    }
    safe_emb_wt_destroy(data->emb_wt);
}

emb_wt_glue_data_t* emb_wt_glue_data_init()
{
    emb_wt_glue_data_t *data = NULL;

    data = (emb_wt_glue_data_t *)malloc(sizeof(emb_wt_glue_data_t));
    if (data == NULL) {
        ST_WARNING("Failed to malloc emb_wt_glue_data.");
        goto ERR;
    }
    memset(data, 0, sizeof(emb_wt_glue_data_t));

    return data;
ERR:
    safe_emb_wt_glue_data_destroy(data);
    return NULL;
}

emb_wt_glue_data_t* emb_wt_glue_data_dup(emb_wt_glue_data_t *src)
{
    emb_wt_glue_data_t *dst = NULL;

    ST_CHECK_PARAM(src == NULL, NULL);

    dst = emb_wt_glue_data_init();
    if (dst == NULL) {
        ST_WARNING("Failed to emb_wt_glue_data_init.");
        goto ERR;
    }

    dst->emb_wt = emb_wt_dup(src->emb_wt);
    if (dst->emb_wt == NULL) {
        ST_WARNING("Failed to weight_dup.");
        goto ERR;
    }

    return dst;
ERR:
    safe_emb_wt_glue_data_destroy(dst);
    return NULL;
}

void emb_wt_glue_destroy(glue_t *glue)
{
    if (glue == NULL) {
        return;
    }

    glue->forward = NULL;
    glue->backprop = NULL;
    safe_emb_wt_glue_data_destroy(glue->extra);
}

int emb_wt_glue_init(glue_t *glue)
{
    ST_CHECK_PARAM(glue == NULL, -1);

    if (strcasecmp(glue->type, EMB_WT_GLUE_NAME) != 0) {
        ST_WARNING("Not a emb_wt glue. [%s]", glue->type);
        return -1;
    }

    glue->forward = emb_wt_glue_forward;
    glue->backprop = emb_wt_glue_backprop;
    glue->extra = (void *)emb_wt_glue_data_init();
    if (glue->extra == NULL) {
        ST_WARNING("Failed to emb_wt_glue_data_init.");
        goto ERR;
    }

    return 0;

ERR:
    safe_emb_wt_glue_data_destroy(glue->extra);
    return -1;
}

int emb_wt_glue_dup(glue_t *dst, glue_t *src)
{
    ST_CHECK_PARAM(dst == NULL || src == NULL, -1);

    if (strcasecmp(dst->type, EMB_WT_GLUE_NAME) != 0) {
        ST_WARNING("dst is Not a emb_wt glue. [%s]", dst->type);
        return -1;
    }

    if (strcasecmp(src->type, EMB_WT_GLUE_NAME) != 0) {
        ST_WARNING("src is Not a emb_wt glue. [%s]", src->type);
        return -1;
    }

    dst->forward = src->forward;
    dst->backprop = src->forward;

    dst->extra = (void *)emb_wt_glue_data_dup((emb_wt_glue_data_t *)src->extra);
    if (dst->extra == NULL) {
        ST_WARNING("Failed to emb_wt_glue_data_dup.");
        goto ERR;
    }

    return 0;
ERR:
    safe_emb_wt_glue_data_destroy(dst->extra);
    return -1;
}

int emb_wt_glue_parse_topo(glue_t *glue, const char *line)
{
    char *p;

    ST_CHECK_PARAM(glue == NULL || line == NULL, -1);

    p = (char *)line;
    while(*p != '\0') {
        if (*p != ' ' || *p != '\t') {
            ST_WARNING("emb_wt_glue should be empty. [%s]", line);
            return -1;
        }
    }

    return 0;
}

bool emb_wt_glue_check(glue_t *glue, layer_t **layers, layer_id_t n_layer)
{
    ST_CHECK_PARAM(glue == NULL, false);

    if (strcasecmp(glue->type, EMB_WT_GLUE_NAME) != 0) {
        ST_WARNING("Not a emb_wt glue. [%s]", glue->type);
        return false;
    }

    if (glue->num_out_layer != 1) {
        ST_WARNING("emb_wt glue: num_out_layer shoule be equal to 1.");
        return false;
    }

    if (glue->num_in_layer != 1) {
        ST_WARNING("emb_wt glue: num_in_layer shoule be equal to 1.");
        return false;
    }

    if (layers == NULL) {
        ST_WARNING("No layers.");
        return false;
    }

    if (strcasecmp(layers[glue->in_layers[0]]->type,
                INPUT_LAYER_NAME) != 0) {
        ST_WARNING("emb_wt glue: in layer should be input layer.");
        return false;
    }

    if (strcasecmp(layers[glue->out_layers[0]]->type,
                OUTPUT_LAYER_NAME) == 0) {
        ST_WARNING("emb_wt glue: out layer should not be output layer.");
        return false;
    }

    return true;
}

char* emb_wt_glue_draw_label(glue_t *glue, char *label, size_t label_len)
{
    ST_CHECK_PARAM(glue == NULL || label == NULL, NULL);

    snprintf(label, label_len, "");

    return label;
}

int emb_wt_glue_load_header(void **extra, int version,
        FILE *fp, bool *binary, FILE *fo_info)
{
    emb_wt_glue_data_t *data = NULL;

    if (extra != NULL) {
        data = emb_wt_glue_data_init();
        if (data == NULL) {
            ST_WARNING("Failed to emb_wt_glue_data_init.");
            goto ERR;
        }

        *extra = (void *)data;
    }

    if (emb_wt_load_header(data != NULL ? &(data->emb_wt) : NULL,
                version, fp, binary, fo_info) < 0) {
        ST_WARNING("Failed to emb_wt_load_header.");
        goto ERR;
    }

    return 0;

ERR:
    safe_emb_wt_glue_data_destroy(data);
    if (extra != NULL) {
        *extra = NULL;
    }
    return -1;
}

int emb_wt_glue_load_body(void *extra, int version, FILE *fp, bool binary)
{
    emb_wt_glue_data_t *data = NULL;

    data = (emb_wt_glue_data_t *)extra;

    if (emb_wt_load_body(data->emb_wt, version, fp, binary) < 0) {
        ST_WARNING("Failed to emb_wt_load_body.");
        return -1;
    }

    return 0;
}

int emb_wt_glue_save_header(void *extra, FILE *fp, bool binary)
{
    emb_wt_glue_data_t *data = NULL;

    data = (emb_wt_glue_data_t *)extra;

    if (emb_wt_save_header(data->emb_wt, fp, binary) < 0) {
        ST_WARNING("Failed to emb_wt_save_header.");
        return -1;
    }

    return 0;
}

int emb_wt_glue_save_body(void *extra, FILE *fp, bool binary)
{
    emb_wt_glue_data_t *data = NULL;

    data = (emb_wt_glue_data_t *)extra;

    if (emb_wt_save_body(data->emb_wt, fp, binary) < 0) {
        ST_WARNING("Failed to emb_wt_save_body.");
        return -1;
    }

    return 0;
}
