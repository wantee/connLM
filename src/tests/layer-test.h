/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Wang Jian
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

#ifndef  _CONNLM_LAYER_TEST_H_
#define  _CONNLM_LAYER_TEST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stutils/st_macro.h>
#include <stutils/st_utils.h>

#include <connlm/config.h>

#include "layers/layer.h"

#define LAYER_TEST_NAME "layer"

typedef struct _layer_ref_t_ {
    char type[MAX_NAME_LEN];
    int size;
} layer_ref_t;

char* layer_test_get_name(char *name, int name_len, int id)
{
    switch (id) {
        case 0:
            snprintf(name, name_len, "output");
            break;
        case 1:
            snprintf(name, name_len, "input");
            break;
        default:
            snprintf(name, name_len, "%s%d", LAYER_TEST_NAME, id);
            break;
    }

    return name;
}

void layer_test_mk_topo_line(char *line, size_t len, layer_ref_t *ref, int id)
{
    assert(line != NULL && ref != NULL);

    snprintf(line, len, "layer name=%s%d type=%s size=%d",
            LAYER_TEST_NAME, id+2, ref->type, ref->size);

#ifdef _LAYER_TEST_PRINT_TOPO_
    fprintf(stderr, "%s\n", line);
#endif
}

int layer_test_check_layer(layer_t *layer, layer_ref_t *ref, int id)
{
    char name[MAX_NAME_LEN];

    assert(layer != NULL && ref != NULL);

    if (strcmp(layer_test_get_name(name, MAX_NAME_LEN, id),
                layer->name) != 0) {
        fprintf(stderr, "layer name not match.[%s/%s]\n", layer->name, name);
        return -1;
    }

    if (strcmp(layer->type, ref->type) != 0) {
        fprintf(stderr, "layer type not match.\n");
        return -1;
    }

    if (layer->size != ref->size) {
        fprintf(stderr, "layer size not match.\n");
        return -1;
    }

    return 0;
}

#ifdef __cplusplus
}
#endif

#endif
