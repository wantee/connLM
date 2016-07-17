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

#include <stutils/st_macro.h>
#include <stutils/st_log.h>
#include <stutils/st_string.h>

#include "direct_glue.h"

#define safe_direct_glue_data_destroy(ptr) do {\
    if((ptr) != NULL) {\
        direct_glue_data_destroy((direct_glue_data_t *)ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)

void direct_glue_data_destroy(direct_glue_data_t *data)
{
    if (data == NULL) {
        return;
    }

    data->hash_sz = 0;
}

direct_glue_data_t* direct_glue_data_init()
{
    direct_glue_data_t *data = NULL;

    data = (direct_glue_data_t *)malloc(sizeof(direct_glue_data_t));
    if (data == NULL) {
        ST_WARNING("Failed to malloc direct_glue_data.");
        goto ERR;
    }
    memset(data, 0, sizeof(direct_glue_data_t));

    return data;
ERR:
    safe_direct_glue_data_destroy(data);
    return NULL;
}

void direct_glue_destroy(glue_t *glue)
{
    if (glue == NULL) {
        return;
    }

    safe_direct_glue_data_destroy(glue->extra);
}

int direct_glue_init(glue_t *glue)
{
    ST_CHECK_PARAM(glue == NULL, -1);

    if (strcasecmp(glue->type, DIRECT_GLUE_NAME) != 0) {
        ST_WARNING("Not a sum glue. [%s]", glue->type);
        return -1;
    }

    glue->extra = (void *)direct_glue_data_init();
    if (glue->extra == NULL) {
        ST_WARNING("Failed to direct_glue_data_init.");
        goto ERR;
    }

    return 0;

ERR:
    safe_direct_glue_data_destroy(glue->extra);
    return -1;
}

void* direct_glue_dup(void *src)
{
    direct_glue_data_t *dst = NULL;

    ST_CHECK_PARAM(src == NULL, NULL);

    dst = direct_glue_data_init();
    if (dst == NULL) {
        ST_WARNING("Failed to direct_glue_data_init.");
        goto ERR;
    }
    dst->hash_sz = ((direct_glue_data_t *)src)->hash_sz;

    return (void *)dst;
ERR:
    safe_direct_glue_data_destroy(dst);
    return NULL;
}

int direct_glue_parse_topo(glue_t *glue, const char *line)
{
    char keyvalue[2*MAX_LINE_LEN];
    char token[MAX_LINE_LEN];

    direct_glue_data_t *data;
    const char *p;

    ST_CHECK_PARAM(glue == NULL || line == NULL, -1);

    data = (direct_glue_data_t *)glue->extra;
    p = line;

    while (p != NULL) {
        p = get_next_token(p, token);
        if (token[0] == '\0') {
            continue;
        }

        if (split_line(token, keyvalue, 2, MAX_LINE_LEN, "=") != 2) {
            ST_WARNING("Failed to split key/value. [%s]", token);
            goto ERR;
        }

        if (strcasecmp("size", keyvalue) == 0) {
            data->hash_sz = (int)st_str2ll(keyvalue + MAX_LINE_LEN);
            if (data->hash_sz <= 0) {
                ST_WARNING("Illegal size[%s]", keyvalue + MAX_LINE_LEN);
                goto ERR;
            }
        } else {
            ST_WARNING("Unknown key/value[%s]", token);
        }
    }

    if (data->hash_sz <= 0) {
        ST_WARNING("Hash size not set");
        goto ERR;
    }

    return 0;

ERR:
    return -1;
}

bool direct_glue_check(glue_t *glue, layer_t **layers, int n_layer,
        input_t *input, output_t *output)
{
    direct_glue_data_t *data;

    ST_CHECK_PARAM(glue == NULL || input == NULL, false);

    data = (direct_glue_data_t *)glue->extra;

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
        ST_WARNING("direct_wt glue: in layer should be input layer.");
        return false;
    }

    if (strcasecmp(layers[glue->out_layers[0]]->type,
                OUTPUT_LAYER_NAME) != 0) {
        ST_WARNING("direct_wt glue: out layer should be output layer.");
        return false;
    }

    if (input->combine != IC_UNDEFINED) {
        ST_WARNING("combine will be ignored.");
    }

    if (data->hash_sz <= output->tree->num_node) {
        ST_WARNING("Hash size should NOT less than output tree nodes.");
        return false;
    }

    if (glue->wt->init_type != WT_INIT_CONST) {
        glue->wt->init_type = WT_INIT_CONST;
        glue->wt->init_param = 0;
    }

    return true;
}

char* direct_glue_draw_label(glue_t *glue, char *label, size_t label_len)
{
    char buf[MAX_NAME_LEN];
    direct_glue_data_t *data;

    ST_CHECK_PARAM(glue == NULL || label == NULL, NULL);

    data = (direct_glue_data_t *)glue->extra;

    snprintf(label, label_len, ",size=%s",
            st_ll2str(buf, MAX_NAME_LEN, (long long)data->hash_sz, false));

    return label;
}

int direct_glue_init_data(glue_t *glue, input_t *input,
        layer_t **layers, output_t *output)
{
    direct_glue_data_t *data;

    ST_CHECK_PARAM(glue == NULL || glue->wt == NULL
            || input == NULL || output == NULL, -1);

    if (strcasecmp(glue->type, DIRECT_GLUE_NAME) != 0) {
        ST_WARNING("Not a direct glue. [%s]", glue->type);
        return -1;
    }

    data = (direct_glue_data_t *)glue->extra;

    if (wt_init(glue->wt, data->hash_sz, 0) < 0) {
        ST_WARNING("Failed to wt_init.");
        return -1;
    }

    return 0;
}
