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
#include <string.h>

#include <stutils/st_utils.h>

#include "glues/sum_glue.h"
#include "glues/direct_glue.h"
#include "connlm.h"

#include "input-test.h"

#define MAX_REF_COMP_NUM 16
#define MAX_REF_LAYER_NUM 16
#define MAX_REF_GLUE_NUM 16
#define MAX_REF_GLUE_IN_NUM 16
#define MAX_REF_GLUE_OUT_NUM 16

#define VOCAB_SIZE 16

typedef struct _connlm_ref_t_ {
    output_norm_t norm;

    char comp_name[MAX_NAME_LEN];
    int num_comp;

    input_ref_t input_refs[MAX_REF_COMP_NUM];
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

    struct direct_glue_ref {
        long long sz;
    } direct_glue[MAX_REF_GLUE_NUM];
} connlm_ref_t;

static char* get_layer_name(char *name, int name_len,
        const char *layer_name, int id)
{
    switch (id) {
        case 0:
            snprintf(name, name_len, "output");
            break;
        case 1:
            snprintf(name, name_len, "input");
            break;
        default:
            snprintf(name, name_len, "%s%d", layer_name, id);
            break;
    }

    return name;
}

static const char *norm_str[] = {
    "Undefined",
    "Softmax",
    "NCE",
};

static FILE* connlm_mk_topo_file(connlm_ref_t *ref)
{
    char line[MAX_LINE_LEN];
    char name[MAX_NAME_LEN];
    FILE *fp;

    int c, l, g;
    int i, n;

    int sum_i = 0;
    int direct_i = 0;

    fp = tmpfile();
    assert(fp != NULL);

    fprintf(fp, "foo foo\n");
    fprintf(fp, "<output>\n");
    fprintf(fp, "property norm=%s\n", norm_str[ref->norm]);
    fprintf(fp, "</output>\n");
    for (c = 0; c < ref->num_comp; c++) {
        fprintf(fp, "<component>\n");
        fprintf(fp, "# comments\n");
        fprintf(fp, "property name=%s%d\n", ref->comp_name, c);
        fprintf(fp, "\n");


        input_test_mk_topo_line(line, ref->input_refs + c);
        fprintf(fp, "%s\n", line);

        for (l = 0; l < ref->num_layer[c]; l++) {
            fprintf(fp, "layer name=%s%d type=%s size=%d \n",
                    ref->layer_name, l+2, ref->layer_type[c][l],
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
                fprintf(fp, "%s,", get_layer_name(name, MAX_NAME_LEN,
                            ref->layer_name, ref->glue_in[c][g][i]));
            }
            fprintf(fp, "%s", get_layer_name(name, MAX_NAME_LEN,
                        ref->layer_name, ref->glue_in[c][g][i]));

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
                fprintf(fp, "%s,", get_layer_name(name, MAX_NAME_LEN,
                            ref->layer_name, ref->glue_out[c][g][i]));
            }
            fprintf(fp, "%s", get_layer_name(name, MAX_NAME_LEN,
                        ref->layer_name, ref->glue_out[c][g][i]));

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
            } else if (strcasecmp(ref->glue_type[c][g], "direct") == 0) {
                fprintf(fp, " size=%s", st_ll2str(name, MAX_NAME_LEN,
                            ref->direct_glue[direct_i].sz, false));
                direct_i++;
            }

            fprintf(fp, "\n");
        }
        fprintf(fp, "</component>\n");
        fprintf(fp, "foo\nbar\n");
    }
    rewind(fp);

#ifdef _CONNLM_TEST_PRINT_TOPO_
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

static int check_output(output_t *output, output_norm_t norm)
{
    if (output->norm != norm) {
        fprintf(stderr, "norm not match.\n");
        return -1;
    }

    return 0;
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

static int check_glue(glue_t *glue, connlm_ref_t *ref, int c, int g, int sum_i,
        int direct_i, layer_t **layers, int n_layer)
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
        (void)get_layer_name(name, MAX_NAME_LEN, ref->layer_name,
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
        (void)get_layer_name(name, MAX_NAME_LEN, ref->layer_name,
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
    } else if (strcasecmp(ref->glue_type[c][g], "direct") == 0) {
        if (((direct_glue_data_t *)(glue->extra))->hash_sz
                != ref->direct_glue[direct_i].sz) {
            fprintf(stderr, "direct glue size not match.[%lld/%lld]\n",
                ref->direct_glue[direct_i].sz,
                (long long)(((direct_glue_data_t *)glue->extra)->hash_sz));
            return -1;
        }
    }

    return 0;
}

static int check_connlm(connlm_t *connlm, connlm_ref_t *ref)
{
    int c, l, sum_i, direct_i;

    if (check_output(connlm->output, ref->norm) != 0) {
        fprintf(stderr, "output not match\n");
        return -1;
    }

    if (connlm->num_comp != ref->num_comp) {
        fprintf(stderr, "num_comp not match\n");
        return -1;
    }
    for (c = 0; c < connlm->num_comp; c++) {
        if (check_comp(connlm->comps[c], ref->comp_name, c) != 0) {
            fprintf(stderr, "component not match\n");
            return -1;
        }

        if (check_input(connlm->comps[c]->input, VOCAB_SIZE,
                    ref->input_refs + c) != 0) {
            fprintf(stderr, "input not match\n");
            return -1;
        }

        if (connlm->comps[c]->num_layer != ref->num_layer[c] + 2) {
            fprintf(stderr, "num_layer not match\n");
            return -1;
        }

        for (l = 2; l < connlm->comps[c]->num_layer; l++) {
            if (check_layer(connlm->comps[c]->layers[l],
                        ref->layer_name, l,
                        ref->layer_type[c][l-2],
                        ref->layer_size[c][l-2]) != 0) {
                fprintf(stderr, "layer not match\n");
                return -1;
            }
        }

        if (connlm->comps[c]->num_glue != ref->num_glue[c]) {
            fprintf(stderr, "num_glue not match\n");
            return -1;
        }

        sum_i = 0;
        direct_i = 0;
        for (l = 0; l < connlm->comps[c]->num_glue; l++) {
            if (check_glue(connlm->comps[c]->glues[l], ref,
                        c, l, sum_i, direct_i,
                        connlm->comps[c]->layers,
                        connlm->comps[c]->num_layer) != 0) {
                fprintf(stderr, "glue not match\n");
                return -1;
            }
            if (strcasecmp(ref->glue_type[c][l], "sum") == 0) {
                sum_i++;
            } else if (strcasecmp(ref->glue_type[c][l], "direct") == 0) {
                direct_i++;
            }
        }
    }

    return 0;
}

static int unit_test_connlm_read_topo_bad()
{
    FILE *fp = NULL;
    int ncase = 0;
    connlm_t *connlm = NULL;
    vocab_t *vocab = NULL;
    connlm_ref_t ref;
    connlm_ref_t std_ref = {
        .norm = ON_SOFTMAX,
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

    fprintf(stderr, "  Testing Reading topology file(bad)...\n");
    vocab = (vocab_t *)malloc(sizeof(vocab_t));
    assert(vocab != NULL);
    memset(vocab, 0, sizeof(vocab_t));
    vocab->vocab_size = 15;
    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    ref = std_ref;
    ref.num_comp = 0;
    fp = connlm_mk_topo_file(&ref);
    connlm = connlm_new(vocab, NULL, NULL, 0);
    if (connlm_init(connlm, fp) >= 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    safe_connlm_destroy(connlm);
    safe_fclose(fp);
    fprintf(stderr, "Success\n");

    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    ref = std_ref;
    ref.num_comp = 1;
    ref.num_layer[0] = 0;
    ref.num_glue[0] = 0;
    fp = connlm_mk_topo_file(&ref);

    connlm = connlm_new(vocab, NULL, NULL, 0);
    if (connlm_init(connlm, fp) >= 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    safe_connlm_destroy(connlm);
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
    fp = connlm_mk_topo_file(&ref);

    connlm = connlm_new(vocab, NULL, NULL, 0);
    if (connlm_init(connlm, fp) >= 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    safe_connlm_destroy(connlm);
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
    fp = connlm_mk_topo_file(&ref);

    connlm = connlm_new(vocab, NULL, NULL, 0);
    if (connlm_init(connlm, fp) >= 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    safe_connlm_destroy(connlm);
    safe_fclose(fp);
    fprintf(stderr, "Success\n");

    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    ref = std_ref;
    ref.num_comp = 2;
    ref.num_layer[0] = 3;
    ref.num_layer[1] = 3;
    ref.num_glue[0] = 2;
    ref.num_glue[1] = 1;
    fp = connlm_mk_topo_file(&ref);

    connlm = connlm_new(vocab, NULL, NULL, 0);
    if (connlm_init(connlm, fp) >= 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    safe_connlm_destroy(connlm);
    safe_free(vocab);
    safe_fclose(fp);
    fprintf(stderr, "Success\n");
    return 0;

ERR:
    safe_connlm_destroy(connlm);
    safe_free(vocab);
    safe_fclose(fp);
    return -1;
}

static int unit_test_connlm_read_topo_good()
{
    FILE *fp = NULL;
    connlm_t *connlm = NULL;

    vocab_opt_t vocab_opt;
    vocab_t *vocab = NULL;

    output_opt_t output_opt;
    output_t *output = NULL;
    char word[16];

    connlm_ref_t ref;
    int i;
    int ncase = 0;
    connlm_ref_t std_ref = {
        .norm = ON_SOFTMAX,

        .comp_name = "comp",
        .num_comp = 2,

        .input_refs = {
            {
                .n_ctx = 2,
                .context = {{-1,1.0}, {1, 0.5}},
                .combine = IC_SUM,
            },
            {
                .n_ctx = 1,
                .context = {{-1,1.0}},
                .combine = IC_AVG,
            },
        },
        .layer_name = "layer",
        .num_layer = {0, 2},
        .layer_type = {{}, {"sigmoid", "sigmoid"},},
        .layer_size = {{}, {15, 15},},

        .glue_name = "glue",
        .num_glue = {1, 4},
        .glue_type = {{"direct"},
                      {"emb_wt", "wt", "wt", "out_wt"}},
        .num_glue_in = {{1},
                        {1, 1, 1, 1}},
        .glue_in = {{{1}},
                    {{1}, {2}, {3}, {3}}},
        .glue_in_offset = {{{0}},
                           {{0}, {0}, {0}, {0}}},
        .glue_in_scale = {{{1.0}},
                           {{1.0}, {1.0}, {1.0}, {1.0}}},
        .num_glue_out = {{1},
                        {1, 1, 1, 1}},
        .glue_out = {{{0}},
                    {{2}, {3}, {2}, {0}}},
        .glue_out_offset = {{{0}},
                           {{0}, {0}, {0}, {0}}},
        .glue_out_scale = {{{1.0}},
                           {{1.0}, {1.0}, {1.0}, {1.0}}},
        .sum_glue = {{true, true}},
        .direct_glue = {{2000000}},
    };

    fprintf(stderr, "  Testing Reading topology file(good)...\n");
    vocab_opt.max_alphabet_size = VOCAB_SIZE + 10;
    vocab = vocab_create(&vocab_opt);
    assert(vocab != NULL);
    strcpy(word, "</s>");
    vocab_add_word(vocab, word);
    for (i = 1; i < VOCAB_SIZE; i++) {
        word[0] = 'A' + i;
        word[1] = 'A' + i;
        word[2] = 'A' + i;
        word[3] = '\0';
        vocab_add_word(vocab, word);
    }
    vocab->vocab_size = VOCAB_SIZE;
    vocab->cnts = (count_t *)malloc(sizeof(count_t) * vocab->vocab_size);
    assert(vocab->cnts != NULL);
    for (i = 0; i < VOCAB_SIZE; i++) {
        vocab->cnts[i] = VOCAB_SIZE - i;
    }

    output_opt.method = OM_TOP_DOWN;
    output_opt.max_depth = 0;
    output_opt.max_branch = 3;
    output = output_generate(&output_opt, vocab->cnts, VOCAB_SIZE);
    assert (output != NULL);

    /***************************************************/
    /***************************************************/
    fprintf(stderr, "    Case %d...", ncase++);
    ref = std_ref;
    fp = connlm_mk_topo_file(&ref);
    connlm = connlm_new(vocab, output, NULL, 0);
    assert(connlm != NULL);
    if (connlm_init(connlm, fp) < 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }
    if (check_connlm(connlm, &ref) != 0) {
        fprintf(stderr, "Failed\n");
        goto ERR;
    }

    safe_connlm_destroy(connlm);
    safe_fclose(fp);
    fprintf(stderr, "Success\n");

    safe_vocab_destroy(vocab);
    safe_output_destroy(output);
    return 0;

ERR:
    safe_connlm_destroy(connlm);
    safe_vocab_destroy(vocab);
    safe_output_destroy(output);
    safe_fclose(fp);
    return -1;
}

static int unit_test_connlm_read_topo()
{
    if (unit_test_connlm_read_topo_bad() != 0) {
        return -1;
    }

    if (unit_test_connlm_read_topo_good() != 0) {
        return -1;
    }

    return 0;
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
