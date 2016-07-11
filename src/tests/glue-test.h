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
#include <stutils/st_utils.h>

#include <connlm/config.h>

#include "layer-test.h"

#include "glues/glue.h"

#define GLUE_TEST_NAME "glue"

#define GLUE_TEST_N_LAYERS 16
typedef struct _glue_ref_t_ {
    char type[MAX_NAME_LEN];
    int in_layers[GLUE_TEST_N_LAYERS];
    int in_offsets[GLUE_TEST_N_LAYERS];
    real_t in_scales[GLUE_TEST_N_LAYERS];
    int num_in_layer;
    int out_layers[GLUE_TEST_N_LAYERS];
    int out_offsets[GLUE_TEST_N_LAYERS];
    real_t out_scales[GLUE_TEST_N_LAYERS];
    int num_out_layer;
    bool recur;

    struct sum_glue_ref {
        bool avg;
        bool set;
    } sum_glue;

    struct direct_glue_ref {
        long long sz;
    } direct_glue;
} glue_ref_t;

char* glue_test_get_name(char *name, size_t len, glue_ref_t *ref, int id)
{
    int i;

    snprintf(name, len, "%s%d-", GLUE_TEST_NAME, id);
    for (i = 0; i < ref->num_in_layer; i++) {
        st_strncatf(name, len, "%d", ref->in_layers[i]);
    }
    strncat(name, "-", len - strlen(name) - 1);
    for (i = 0; i < ref->num_out_layer; i++) {
        st_strncatf(name, len, "%d", ref->out_layers[i]);
    }

    return name;
}

void glue_test_mk_topo_line(char *line, size_t len, glue_ref_t *ref, int id)
{
    char name[MAX_NAME_LEN];
    int i, n;

    assert(line != NULL && ref != NULL);

    snprintf(line, len, "glue name=%s",
            glue_test_get_name(name, len, ref, id));

    st_strncatf(line, len, " type=%s", ref->type);

    st_strncatf(line, len, " in=");
    for (i = 0; i < ref->num_in_layer - 1; i++) {
        st_strncatf(line, len, "%s,", layer_test_get_name(name, MAX_NAME_LEN,
                    ref->in_layers[i]));
    }
    st_strncatf(line, len, "%s", layer_test_get_name(name, MAX_NAME_LEN,
                ref->in_layers[i]));

    for (n = ref->num_in_layer - 1; n >= 0; n--) {
        if (ref->in_offsets[n] != 0) {
            break;
        }
    }

    if (n >= 0) {
        st_strncatf(line, len, " in-offset=");
        for (i = 0; i < n; i++) {
            st_strncatf(line, len, "%d,", ref->in_offsets[i]);
        }
        st_strncatf(line, len, "%d", ref->in_offsets[i]);
    }

    for (n = ref->num_in_layer - 1; n >= 0; n--) {
        if (ref->in_scales[n] != 1.0) {
            break;
        }
    }

    if (n >= 0) {
        st_strncatf(line, len, " in-scale=");
        for (i = 0; i < n; i++) {
            st_strncatf(line, len, "%g,", ref->in_scales[i]);
        }
        st_strncatf(line, len, "%g", ref->in_scales[i]);
    }


    st_strncatf(line, len, " out=");
    for (i = 0; i < ref->num_out_layer - 1; i++) {
        st_strncatf(line, len, "%s,", layer_test_get_name(name, MAX_NAME_LEN,
                    ref->out_layers[i]));
    }
    st_strncatf(line, len, "%s", layer_test_get_name(name, MAX_NAME_LEN,
                ref->out_layers[i]));

    for (n = ref->num_out_layer - 1; n >= 0; n--) {
        if (ref->out_offsets[n] != 0) {
            break;
        }
    }

    if (n >= 0) {
        st_strncatf(line, len, " out-offset=");
        for (i = 0; i < n; i++) {
            st_strncatf(line, len, "%d,", ref->out_offsets[i]);
        }
        st_strncatf(line, len, "%d", ref->out_offsets[i]);
    }

    for (n = ref->num_out_layer - 1; n >= 0; n--) {
        if (ref->out_scales[n] != 1.0) {
            break;
        }
    }

    if (n >= 0) {
        st_strncatf(line, len, " out-scale=");
        for (i = 0; i < n; i++) {
            st_strncatf(line, len, "%g,", ref->out_scales[i]);
        }
        st_strncatf(line, len, "%g", ref->out_scales[i]);
    }

    if (strcasecmp(ref->type, "sum") == 0) {
        if (ref->sum_glue.avg) {
            st_strncatf(line, len, " avg=true");
        } else {
            if (ref->sum_glue.set) {
                st_strncatf(line, len, " avg=false");
            }
        }
    } else if (strcasecmp(ref->type, "direct_wt") == 0) {
        st_strncatf(line, len, " size=%s", st_ll2str(name, MAX_NAME_LEN,
                    ref->direct_glue.sz, false));
    }

#ifdef _GLUE_TEST_PRINT_TOPO_
    fprintf(stderr, "%s\n", line);
#endif
}


int glue_test_check_glue(glue_t *glue, glue_ref_t *ref, int id)
{
    int i;

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

    if (glue->num_in_layer != ref->num_in_layer || glue->in_layers == NULL) {
        fprintf(stderr, "glue in num not match.[%d/%d]\n",
                ref->num_in_layer, glue->num_in_layer);
        return -1;
    }
    for (i = 0; i < ref->num_in_layer; i++) {
        if (ref->in_layers[i] != glue->in_layers[i]) {
            fprintf(stderr, "glue in layer not match.[%d/%d]\n",
                    ref->in_layers[i], glue->in_layers[i]);
            return -1;
        }

        if (ref->in_offsets[i] != glue->in_offsets[i]) {
            fprintf(stderr, "glue in offset not match.[%d/%d]\n",
                    ref->in_offsets[i], glue->in_offsets[i]);
            return -1;
        }

        if (ref->in_scales[i] != glue->in_scales[i]) {
            fprintf(stderr, "glue in scale not match.[%g/%g]\n",
                    ref->in_scales[i], glue->in_scales[i]);
            return -1;
        }
    }

    for (i = 0; i < ref->num_out_layer; i++) {
        if (ref->out_layers[i] != glue->out_layers[i]) {
            fprintf(stderr, "glue out layer not match.[%d/%d]\n",
                    ref->out_layers[i], glue->out_layers[i]);
            return -1;
        }

        if (ref->out_offsets[i] != glue->out_offsets[i]) {
            fprintf(stderr, "glue out offset not match.[%d/%d]\n",
                    ref->out_offsets[i], glue->out_offsets[i]);
            return -1;
        }

        if (ref->out_scales[i] != glue->out_scales[i]) {
            fprintf(stderr, "glue out scale not match.[%g/%g]\n",
                    ref->out_scales[i], glue->out_scales[i]);
            return -1;
        }
    }

    if (strcasecmp(ref->type, "sum") == 0) {
        if (((sum_glue_data_t *)(glue->extra))->avg
                != ref->sum_glue.avg) {
            fprintf(stderr, "sum glue avg not match.[%s/%s]\n",
                    bool2str(ref->sum_glue.avg),
                    bool2str(((sum_glue_data_t *)glue->extra)->avg));
            return -1;
        }
    } else if (strcasecmp(ref->type, "direct") == 0) {
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
