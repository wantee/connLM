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

#include <stdio.h>
#include <assert.h>

#include <st_utils.h>

#include "glues/sum_glue.h"
#include "connlm.h"

#define MAX_REF_COMP_NUM 16
#define MAX_REF_LAYER_NUM 16
#define MAX_REF_GLUE_NUM 16
#define MAX_REF_GLUE_IN_NUM 16
#define MAX_REF_GLUE_OUT_NUM 16

typedef struct _ref_t_ {
    char comp_name[MAX_NAME_LEN];
    int num_comp;

    char layer_name[MAX_NAME_LEN];
    int num_layer[MAX_REF_COMP_NUM];
    char layer_type[MAX_REF_COMP_NUM][MAX_REF_LAYER_NUM][MAX_NAME_LEN];
    int layer_size[MAX_REF_COMP_NUM][MAX_REF_LAYER_NUM];

    char glue_name[MAX_NAME_LEN];
    int num_glue[MAX_REF_COMP_NUM];
    char glue_type[MAX_REF_COMP_NUM][MAX_REF_GLUE_NUM][MAX_NAME_LEN];
    int glue_in[MAX_REF_COMP_NUM][MAX_REF_GLUE_NUM][MAX_REF_GLUE_IN_NUM];
    int num_glue_in[MAX_REF_COMP_NUM][MAX_REF_GLUE_NUM];
    int glue_in_offset[MAX_REF_COMP_NUM][MAX_REF_GLUE_NUM][MAX_REF_GLUE_IN_NUM];
    real_t glue_in_scale[MAX_REF_COMP_NUM][MAX_REF_GLUE_NUM][MAX_REF_GLUE_IN_NUM];
    int glue_out[MAX_REF_COMP_NUM][MAX_REF_GLUE_NUM][MAX_REF_GLUE_OUT_NUM];
    int num_glue_out[MAX_REF_COMP_NUM][MAX_REF_GLUE_NUM];
    int glue_out_offset[MAX_REF_COMP_NUM][MAX_REF_GLUE_NUM][MAX_REF_GLUE_OUT_NUM];
    real_t glue_out_scale[MAX_REF_COMP_NUM][MAX_REF_GLUE_NUM][MAX_REF_GLUE_OUT_NUM];

    struct sum_glue_ref {
        bool avg;
        bool set;
    } sum_glue[MAX_REF_GLUE_NUM];
} ref_t;

static FILE* mk_topo_file(ref_t *ref)
{
    FILE *fp;

    int c, l, g;
    int i, n;

    int sum_i = 0;

    fp = tmpfile();
    assert(fp != NULL);

    fprintf(fp, "foo foo\n");
    for (c = 0; c < ref->num_comp; c++) {
        fprintf(fp, "<component>\n");
        fprintf(fp, "# comments\n");
        fprintf(fp, "property name=%s%d\n", ref->comp_name, c);
        fprintf(fp, "\n");

        for (l = 0; l < ref->num_layer[c]; l++) {
            fprintf(fp, "layer name=%s%d type=%s size=%d \n",
                    ref->layer_name, l, ref->layer_type[c][l],
                    ref->layer_size[c][l]);
        }
        fprintf(fp, "\n");
        for (g = 0; g < ref->num_glue[c]; g++) {
            fprintf(fp, "glue name=%s", ref->glue_name);
            for (i = 0; i < ref->num_glue_in[c][g]; i++) {
                fprintf(fp, "%d", ref->glue_in[c][g][i]);
            }
            fprintf(fp, "-");
            for (i = 0; i < ref->num_glue_out[c][g]; i++) {
                fprintf(fp, "%d", ref->glue_out[c][g][i]);
            }
            fprintf(fp, " type=%s", ref->glue_type[c][g]);

            fprintf(fp, " in=");
            for (i = 0; i < ref->num_glue_in[c][g] - 1; i++) {
                fprintf(fp, "%s%d,", ref->layer_name, ref->glue_in[c][g][i]);
            }
            fprintf(fp, "%s%d", ref->layer_name, ref->glue_in[c][g][i]);

            for (n = ref->num_glue_in[c][g] - 1; n >= 0; n--) {
                if (ref->glue_in_offset[c][g][n] != 0) {
                    break;
                }
            }

            if (n >= 0) {
                fprintf(fp, " in-offset=");
                for (i = 0; i < n; i++) {
                    fprintf(fp, "%d,", ref->glue_in_offset[c][g][i]);
                }
                fprintf(fp, "%d", ref->glue_in_offset[c][g][i]);
            }

            for (n = ref->num_glue_in[c][g] - 1; n >= 0; n--) {
                if (ref->glue_in_scale[c][g][n] != 1.0) {
                    break;
                }
            }

            if (n >= 0) {
                fprintf(fp, " in-scale=");
                for (i = 0; i < n; i++) {
                    fprintf(fp, "%g,", ref->glue_in_scale[c][g][i]);
                }
                fprintf(fp, "%g", ref->glue_in_scale[c][g][i]);
            }


            fprintf(fp, " out=");
            for (i = 0; i < ref->num_glue_out[c][g] - 1; i++) {
                fprintf(fp, "%s%d,", ref->layer_name, ref->glue_out[c][g][i]);
            }
            fprintf(fp, "%s%d", ref->layer_name, ref->glue_out[c][g][i]);

            for (n = ref->num_glue_out[c][g] - 1; n >= 0; n--) {
                if (ref->glue_out_offset[c][g][n] != 0) {
                    break;
                }
            }

            if (n >= 0) {
                fprintf(fp, " out-offset=");
                for (i = 0; i < n; i++) {
                    fprintf(fp, "%d,", ref->glue_out_offset[c][g][i]);
                }
                fprintf(fp, "%d", ref->glue_out_offset[c][g][i]);
            }

            for (n = ref->num_glue_out[c][g] - 1; n >= 0; n--) {
                if (ref->glue_out_scale[c][g][n] != 1.0) {
                    break;
                }
            }

            if (n >= 0) {
                fprintf(fp, " out-scale=");
                for (i = 0; i < n; i++) {
                    fprintf(fp, "%g,", ref->glue_out_scale[c][g][i]);
                }
                fprintf(fp, "%g", ref->glue_out_scale[c][g][i]);
            }

            if (strcasecmp(ref->glue_type[c][g], "sum") == 0) {
                if (ref->sum_glue[sum_i].avg) {
                    fprintf(fp, " avg=true");
                } else {
                    if (ref->sum_glue[sum_i].set) {
                        fprintf(fp, " avg=false");
                    }
                }
                sum_i++;
            }

            fprintf(fp, "\n");
        }
        fprintf(fp, "</component>\n");
        fprintf(fp, "foo\nbar\n");
    }
    rewind(fp);

#if 0
    {
        char line[MAX_LINE_LEN];
        while(fgets(line, MAX_LINE_LEN, fp)) {
            fprintf(stderr, "%s", line);
        }
        rewind(fp);
    }
#endif

    return fp;
}

static int check_comp(component_t *comp, char *name, int id)
{
    if (strncmp(comp->name, name, strlen(name)) != 0
            || comp->name[strlen(name)] != '0' + id
            || comp->name[strlen(name) + 1] != '\0') {
        fprintf(stderr, "component name not match.\n");
        return -1;
    }

    return 0;
}

static int check_layer(layer_t *layer, char *name, int id,
        char *type, int size)
{ 
    if (strncmp(layer->name, name, strlen(name)) != 0
            || layer->name[strlen(name)] != '0' + id
            || layer->name[strlen(name) + 1] != '\0') {
        fprintf(stderr, "layer name not match.\n");
        return -1;
    }

    if (strcmp(layer->type, type) != 0) {
        fprintf(stderr, "layer type not match.\n");
        return -1;
    }

    if (layer->size != size) {
        fprintf(stderr, "layer size not match.\n");
        return -1;
    }

    return 0;
}

static int check_glue(glue_t *glue, ref_t *ref, int c, int g, int sum_i,
        layer_t **layers, layer_id_t n_layer)
{ 
    char name[MAX_NAME_LEN];
    char tmp[MAX_NAME_LEN];

    int i;

    snprintf(name, MAX_NAME_LEN, "%s", ref->glue_name);
    for (i = 0; i < ref->num_glue_in[c][g]; i++) {
        snprintf(tmp, MAX_NAME_LEN, "%d", ref->glue_in[c][g][i]);
        strncat(name, tmp, MAX_NAME_LEN - strlen(name) - 1);
    }
    strncat(name, "-", MAX_NAME_LEN - strlen(name) - 1);
    for (i = 0; i < ref->num_glue_out[c][g]; i++) {
        snprintf(tmp, MAX_NAME_LEN, "%d", ref->glue_out[c][g][i]);
        strncat(name, tmp, MAX_NAME_LEN - strlen(name) - 1);
    }

    if (strcmp(name, glue->name) != 0) {
        fprintf(stderr, "glue name not match.[%s/%s]\n", name, glue->name);
        return -1;
    }

    if (strcmp(ref->glue_type[c][g], glue->type) != 0) {
        fprintf(stderr, "glue type not match.[%s/%s]\n",
                ref->glue_type[c][g], glue->type);
        return -1;
    }

    if (glue->num_in_layer != ref->num_glue_in[c][g]
            || glue->in_layers == NULL) {
        fprintf(stderr, "glue in num not match.[%d/%d]\n",
                ref->num_glue_in[c][g], glue->num_in_layer);
        return -1;
    }
    for (i = 0; i < ref->num_glue_in[c][g]; i++) {
        snprintf(name, MAX_NAME_LEN, "%s%d", ref->layer_name,
                ref->glue_in[c][g][i]);
        if (strcmp(name, layers[glue->in_layers[i]]->name) != 0) {
            fprintf(stderr, "glue in layer not match.[%s/%s]\n",
                    name, layers[glue->in_layers[i]]->name);
            return -1;
        }

        if (ref->glue_in_offset[c][g][i] != glue->in_offsets[i]) {
            fprintf(stderr, "glue in offset not match.[%d/%d]\n",
                    ref->glue_in_offset[c][g][i], glue->in_offsets[i]);
            return -1;
        }

        if (ref->glue_in_scale[c][g][i] != glue->in_scales[i]) {
            fprintf(stderr, "glue in scale not match.[%g/%g]\n",
                    ref->glue_in_scale[c][g][i], glue->in_scales[i]);
            return -1;
        }
    }

    for (i = 0; i < ref->num_glue_out[c][g]; i++) {
        snprintf(name, MAX_NAME_LEN, "%s%d", ref->layer_name,
                ref->glue_out[c][g][i]);
        if (strcmp(name, layers[glue->out_layers[i]]->name) != 0) {
            fprintf(stderr, "glue out layer not match.[%s/%s]\n",
                    name, layers[glue->out_layers[i]]->name);
            return -1;
        }

        if (ref->glue_out_offset[c][g][i] != glue->out_offsets[i]) {
            fprintf(stderr, "glue out offset not match.[%d/%d]\n",
                    ref->glue_out_offset[c][g][i], glue->out_offsets[i]);
            return -1;
        }

        if (ref->glue_out_scale[c][g][i] != glue->out_scales[i]) {
            fprintf(stderr, "glue out scale not match.[%g/%g]\n",
                    ref->glue_out_scale[c][g][i], glue->out_scales[i]);
            return -1;
        }
    }

    if (strcasecmp(ref->glue_type[c][g], "sum") == 0) {
        if (((sum_glue_data_t *)(glue->extra))->avg
                != ref->sum_glue[sum_i].avg) {
            fprintf(stderr, "sum glue avg not match.[%s/%s]\n",
                    bool2str(ref->sum_glue[sum_i].avg),
                    bool2str(((sum_glue_data_t *)glue->extra)->avg));
            return -1;
        }
    }

    return 0;
}

static int check_connlm(connlm_t *connlm, ref_t *ref)
{
    int c, l, sum_i;

    if (connlm->num_comp != ref->num_comp) {
        fprintf(stderr, "num_comp not match\n");
        return -1;
    }
    for (c = 0; c < connlm->num_comp; c++) {
        if (check_comp(connlm->comps[c], ref->comp_name, c) != 0) {
            fprintf(stderr, "component not match\n");
            return -1;
        }

        if (connlm->comps[c]->num_layer != ref->num_layer[c]) {
            fprintf(stderr, "num_layer not match\n");
            return -1;
        }

        for (l = 0; l < connlm->comps[c]->num_layer; l++) {
            if (check_layer(connlm->comps[c]->layers[l], ref->layer_name, l,
                    ref->layer_type[c][l], ref->layer_size[c][l]) != 0) {
                fprintf(stderr, "layer not match\n");
                return -1;
            }
        }

        if (connlm->comps[c]->num_glue != ref->num_glue[c]) {
            fprintf(stderr, "num_glue not match\n");
            return -1;
        }

        sum_i = 0;
        for (l = 0; l < connlm->comps[c]->num_glue; l++) {
            if (check_glue(connlm->comps[c]->glues[l], ref,
                        c, l, sum_i, connlm->comps[c]->layers,
                        connlm->comps[c]->num_layer) != 0) {
                fprintf(stderr, "glue not match\n");
                return -1;
            }
            if (strcasecmp(ref->glue_type[c][l], "sum") == 0) {
                sum_i++;
            }
        }
    }

    return 0;
}

static int unit_test_connlm_read_topo()
{
    FILE *fp = NULL;
    int ncase = 0;
    connlm_t connlm;
    ref_t ref;
    ref_t std_ref = {
        .comp_name = "comp",
        .num_comp = 2,

        .layer_name = "layer",
        .num_layer = {3, 3},
        .layer_type = {{"sigmoid", "tanh", "sigmoid"},
                       {"sigmoid", "tanh", "sigmoid"},},
        .layer_size = {{32, 16, 16}, {32, 16, 16},},

        .glue_name = "glue",
        .num_glue = {2, 1},
        .glue_type = {{"sum", "append"},
                      {"clone"}},
        .num_glue_in = {{2, 2},
                        {1}},
        .glue_in = {{{0, 1}, {1, 2}},
                    {{0}}},
        .glue_in_offset = {{{16, 0}, {0, 0}},
                           {{24}}},
        .glue_in_scale = {{{1.0, 1.0}, {1.0, 0.8}},
                           {{0.5}}},
        .num_glue_out = {{1, 1},
                        {2}},
        .glue_out = {{{2}, {0}},
                    {{1, 2}}},
        .glue_out_offset = {{{0}, {0}},
                           {{8, 8}}},
        .glue_out_scale = {{{1.0}, {1.0}},
                           {{1.0, 2.0}}},
        .sum_glue = {{true, true}},
    };

    fprintf(stderr, "  Testing Reading topology file...\n");
    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    ref = std_ref;
    ref.num_comp = 0;
    fp = mk_topo_file(&ref);
    memset(&connlm, 0, sizeof(connlm_t));
    if (connlm_init(&connlm, fp) >= 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    connlm_destroy(&connlm);
    safe_fclose(fp);
    fprintf(stderr, "Success\n");

    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    ref = std_ref;
    ref.num_comp = 1;
    ref.num_layer[0] = 0;
    ref.num_glue[0] = 0;
    fp = mk_topo_file(&ref);

    memset(&connlm, 0, sizeof(connlm_t));
    if (connlm_init(&connlm, fp) >= 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    connlm_destroy(&connlm);
    safe_fclose(fp);
    fprintf(stderr, "Success\n");

    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    ref = std_ref;
    ref.num_comp = 2;
    ref.num_layer[0] = 0;
    ref.num_layer[1] = 0;
    ref.num_glue[0] = 0;
    ref.num_glue[1] = 0;
    fp = mk_topo_file(&ref);

    memset(&connlm, 0, sizeof(connlm_t));
    if (connlm_init(&connlm, fp) >= 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    connlm_destroy(&connlm);
    safe_fclose(fp);
    fprintf(stderr, "Success\n");

    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    ref = std_ref;
    ref.num_comp = 2;
    ref.num_layer[0] = 1;
    ref.num_layer[1] = 2;
    ref.num_glue[0] = 0;
    ref.num_glue[1] = 0;
    fp = mk_topo_file(&ref);

    memset(&connlm, 0, sizeof(connlm_t));
    if (connlm_init(&connlm, fp) < 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    if (check_connlm(&connlm, &ref) != 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    connlm_destroy(&connlm);
    safe_fclose(fp);
    fprintf(stderr, "Success\n");

    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    ref.num_comp = 2;
    ref.num_layer[0] = 3;
    ref.num_layer[1] = 3;
    ref.num_glue[0] = 2;
    ref.num_glue[1] = 1;
    fp = mk_topo_file(&ref);

    memset(&connlm, 0, sizeof(connlm_t));
    if (connlm_init(&connlm, fp) < 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    if (check_connlm(&connlm, &ref) != 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    connlm_destroy(&connlm);
    safe_fclose(fp);
    fprintf(stderr, "Success\n");
    return 0;

ERR:
    connlm_destroy(&connlm);
    safe_fclose(fp);
    return -1;
}

static int run_all_tests()
{
    int ret = 0;

    if (unit_test_connlm_read_topo() != 0) {
        ret = -1;
    }

    return ret;
}

int main(int argc, const char *argv[])
{
    int ret;

    fprintf(stderr, "Start testing...\n");
    ret = run_all_tests();
    if (ret != 0) {
        fprintf(stderr, "Tests failed.\n");
    } else {
        fprintf(stderr, "Tests succeeded.\n");
    }

    return ret;
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
