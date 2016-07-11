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

#ifndef  _CONNLM_COMPONENT_TEST_H_
#define  _CONNLM_COMPONENT_TEST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stutils/st_macro.h>
#include <stutils/st_utils.h>

#include <connlm/config.h>

#include "component.h"

#include "input-test.h"
#include "layer-test.h"
#include "glue-test.h"

#define COMP_TEST_NAME "comp"
#define COMP_TEST_N 16

typedef struct _comp_ref_t_ {
    input_ref_t input_ref;
    layer_ref_t layer_refs[COMP_TEST_N];
    int num_layer;

    glue_ref_t glue_refs[COMP_TEST_N];
    int num_glue;
} comp_ref_t;

char* comp_test_get_name(char *name, size_t name_len, int id)
{
    snprintf(name, name_len, "%s%d", COMP_TEST_NAME, id);

    return name;
}

void comp_test_mk_topo_lines(char *lines, size_t len, comp_ref_t *ref, int id)
{
    char line[MAX_LINE_LEN];
    char name[MAX_NAME_LEN];
    int i;

    assert(lines != NULL && ref != NULL);

    snprintf(lines, len, "# comments\n");
    st_strncatf(lines, len, "property name=%s\n",
            comp_test_get_name(name, MAX_NAME_LEN, id));
    st_strncatf(lines, len, "\n");

    input_test_mk_topo_line(line, MAX_LINE_LEN, &(ref->input_ref));
    st_strncatf(lines, len, "%s\n", line);

    for (i = 0; i < ref->num_layer; i++) {
        layer_test_mk_topo_line(line, MAX_LINE_LEN, ref->layer_refs + i, i);
        st_strncatf(lines, len, "%s\n", line);
    }
    st_strncatf(lines, len, "\n");
    for (i = 0; i < ref->num_glue; i++) {
        glue_test_mk_topo_line(line, MAX_LINE_LEN, ref->glue_refs + i, i);
        st_strncatf(lines, len, "%s\n", line);
    }

#ifdef _COMP_TEST_PRINT_TOPO_
    fprintf(stderr, "%s", lines);
#endif
}

static int check_input(input_t *input, int input_size, input_ref_t *input_ref)
{
    if (input->input_size != input_size) {
        fprintf(stderr, "input size not match.\n");
        return -1;
    }

    if (input_test_check_input(input, input_ref) < 0) {
        fprintf(stderr, "input not match.\n");
        return -1;
    }

    return 0;
}

int comp_test_check_comp(component_t *comp, int input_size,
        comp_ref_t *ref, int id)
{
    char name[MAX_NAME_LEN];
    int i;

    assert(comp != NULL && ref != NULL);

    if (strcmp(comp_test_get_name(name, MAX_NAME_LEN, id), comp->name) != 0) {
        fprintf(stderr, "component name not match.\n");
        return -1;
    }


    if (check_input(comp->input, input_size, &(ref->input_ref)) != 0) {
        fprintf(stderr, "input not match\n");
        return -1;
    }

    if (comp->num_layer != ref->num_layer + 2) {
        fprintf(stderr, "num_layer not match\n");
        return -1;
    }

    for (i = 2; i < comp->num_layer; i++) {
        if (layer_test_check_layer(comp->layers[i],
                    ref->layer_refs + i, i) != 0) {
            fprintf(stderr, "layer not match\n");
            return -1;
        }
    }

    if (comp->num_glue != ref->num_glue) {
        fprintf(stderr, "num_glue not match\n");
        return -1;
    }

    for (i = 0; i < comp->num_glue; i++) {
        if (glue_test_check_glue(comp->glues[i],
                    ref->glue_refs + i, i) != 0) {
            fprintf(stderr, "glue not match\n");
            return -1;
        }
    }

    return 0;
}

#ifdef __cplusplus
}
#endif

#endif
