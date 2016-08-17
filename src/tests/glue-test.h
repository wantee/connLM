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

#ifndef  _CONNLM_GLUE_TEST_H_
#define  _CONNLM_GLUE_TEST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stutils/st_macro.h>
#include <stutils/st_string.h>

#include <connlm/config.h>

#include "layer-test.h"

#include "glues/glue.h"
#include "glues/direct_glue.h"

#define GLUE_TEST_NAME "glue"

#define GLUE_TEST_N_LAYERS 16
typedef struct _glue_ref_t_ {
    char type[MAX_NAME_LEN];
    int in_layer;
    int out_layer;
    bool recur;

    struct direct_glue_ref {
        long long sz;
    } direct_glue;
} glue_ref_t;

char* glue_test_get_name(char *name, size_t len, glue_ref_t *ref, int id)
{
    snprintf(name, len, "%s%d-", GLUE_TEST_NAME, id);
    st_strncatf(name, len, "%d", ref->in_layer);
    strncat(name, "-", len - strlen(name) - 1);
    st_strncatf(name, len, "%d", ref->out_layer);

    return name;
}

void glue_test_mk_topo_line(char *line, size_t len, glue_ref_t *ref, int id)
{
    char name[MAX_NAME_LEN];

    assert(line != NULL && ref != NULL);

    snprintf(line, len, "glue name=%s",
            glue_test_get_name(name, MAX_NAME_LEN, ref, id));

    st_strncatf(line, len, " type=%s", ref->type);

    st_strncatf(line, len, " in=");
    st_strncatf(line, len, "%s", layer_test_get_name(name, MAX_NAME_LEN,
                ref->in_layer));

    st_strncatf(line, len, " out=");
    st_strncatf(line, len, "%s", layer_test_get_name(name, MAX_NAME_LEN,
                ref->out_layer));

    if (strcasecmp(ref->type, "direct") == 0) {
        st_strncatf(line, len, " size=%s", st_ll2str(name, MAX_NAME_LEN,
                    ref->direct_glue.sz, false));
    }

#ifdef _GLUE_TEST_PRINT_TOPO_
    fprintf(stderr, "%s\n", line);
#endif
}


int glue_test_check_glue(glue_t *glue, glue_ref_t *ref, int id)
{
    char name[MAX_NAME_LEN];

    assert(glue != NULL && ref != NULL);

    (void)glue_test_get_name(name, MAX_NAME_LEN, ref, id);

    if (strcmp(name, glue->name) != 0) {
        fprintf(stderr, "glue name not match.[%s/%s]\n", name, glue->name);
        return -1;
    }

    if (strcmp(ref->type, glue->type) != 0) {
        fprintf(stderr, "glue type not match.[%s/%s]\n",
                ref->type, glue->type);
        return -1;
    }

    if (ref->in_layer != glue->in_layer) {
        fprintf(stderr, "glue in layer not match.[%d/%d]\n",
                ref->in_layer, glue->in_layer);
        return -1;
    }

    if (ref->out_layer != glue->out_layer) {
        fprintf(stderr, "glue out layer not match.[%d/%d]\n",
                ref->out_layer, glue->out_layer);
        return -1;
    }

    if (strcasecmp(ref->type, "direct") == 0) {
        if (((direct_glue_data_t *)(glue->extra))->hash_sz
                != ref->direct_glue.sz) {
            fprintf(stderr, "direct glue size not match.[%lld/%lld]\n",
                ref->direct_glue.sz,
                (long long)(((direct_glue_data_t *)glue->extra)->hash_sz));
            return -1;
        }
    }

    return 0;
}

#ifdef __cplusplus
}
#endif

#endif
