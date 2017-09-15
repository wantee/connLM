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
#include <stutils/st_utils.h>
#include <stutils/st_log.h>

#include "utils.h"
#include "output.h"
#include "../../glues/out_glue.h"
#include "../component_updater.h"

#include "out_glue_updater.h"

typedef struct _ogu_data_t_ {
    ivec_t targets;
    int num_nodes;
    mat_t *node_in_acs;
    mat_t *node_out_ers;

    // const variables
    int *node_iters;
} ogu_data_t;

#define safe_ogu_data_destroy(ptr) do {\
    if((ptr) != NULL) {\
        ogu_data_destroy((ogu_data_t *)ptr);\
        safe_st_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)

void ogu_data_destroy(ogu_data_t *data)
{
    int i;

    if (data == NULL) {
        return;
    }

    ivec_destroy(&data->targets);

    if (data->node_in_acs != NULL) {
        for (i = 0; i < data->num_nodes; i++) {
            mat_destroy(data->node_in_acs + i);
        }
        safe_st_free(data->node_in_acs);
    }

    if (data->node_out_ers != NULL) {
        for (i = 0; i < data->num_nodes; i++) {
            mat_destroy(data->node_out_ers + i);
        }
        safe_st_free(data->node_out_ers);
    }

    data->num_nodes = 0;
}

ogu_data_t* ogu_data_init(glue_updater_t *glue_updater)
{
    ogu_data_t *data = NULL;

    data = (ogu_data_t *)st_malloc(sizeof(ogu_data_t));
    if (data == NULL) {
        ST_WARNING("Failed to st_malloc ogu_data.");
        goto ERR;
    }
    memset(data, 0, sizeof(ogu_data_t));

    return data;
ERR:
    safe_ogu_data_destroy(data);
    return NULL;
}

void out_glue_updater_destroy(glue_updater_t *glue_updater)
{
    if (glue_updater == NULL) {
        return;
    }

    safe_ogu_data_destroy(glue_updater->extra);
}

int out_glue_updater_init(glue_updater_t *glue_updater)
{
    glue_t *glue;

    ST_CHECK_PARAM(glue_updater == NULL, -1);

    glue= glue_updater->glue;

    if (strcasecmp(glue->type, OUT_GLUE_NAME) != 0) {
        ST_WARNING("Not a out glue_updater. [%s]", glue->type);
        return -1;
    }

    glue_updater->extra = (void *)ogu_data_init(glue_updater);
    if (glue_updater->extra == NULL) {
        ST_WARNING("Failed to ogu_data_init.");
        goto ERR;
    }

    return 0;

ERR:
    safe_ogu_data_destroy(glue_updater->extra);
    return -1;
}

int ogu_data_setup(ogu_data_t *data, out_updater_t *out_updater, bool backprop)
{
    ST_CHECK_PARAM(data == NULL, -1);

    data->num_nodes = out_updater->output->tree->num_node;

    data->node_iters = out_updater->node_iters;

    data->node_in_acs = (mat_t *)st_malloc(sizeof(mat_t) * data->num_nodes);
    if (data->node_in_acs == NULL) {
        ST_WARNING("Failed to st_malloc node_in_acs.");
        goto ERR;
    }
    memset(data->node_in_acs, 0, sizeof(mat_t) * data->num_nodes);

    if (backprop) {
        data->node_out_ers = (mat_t *)st_malloc(sizeof(mat_t) * data->num_nodes);
        if (data->node_out_ers == NULL) {
            ST_WARNING("Failed to st_malloc node_out_ers.");
            goto ERR;
        }
        memset(data->node_out_ers, 0, sizeof(mat_t) * data->num_nodes);
    }

    return 0;
ERR:
    ogu_data_destroy(data);
    return -1;
}

int out_glue_updater_setup(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, bool backprop)
{
    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL, -1);

    if (ogu_data_setup((ogu_data_t *)glue_updater->extra,
                comp_updater->out_updater, backprop) < 0) {
        ST_WARNING("Failed to ogu_data_setup");
        return -1;
    }

    return 0;
}

static int clear_tree_nodes_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    ogu_data_t *data;

    data = (ogu_data_t *) args;

    mat_clear(data->node_in_acs + node);
    if (data->node_out_ers != NULL) {
        mat_clear(data->node_out_ers + node);
    }
    data->node_iters[node] = 0;

    return 0;
}

typedef struct _tree_nodes_walker_args_t_ {
    mat_t *in_ac;
    mat_t *out_er;
    int batch_i;
    ogu_data_t *data;
} tree_nodes_walker_args_t;

static int fill_tree_nodes_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    tree_nodes_walker_args_t *tnw_args;
    ogu_data_t *data;

    tnw_args = (tree_nodes_walker_args_t *)args;
    data = tnw_args->data;

    if (tnw_args->in_ac != NULL) {
        // tnw_args->in_ac->num_cols is the hidden size of prev layer
        if (mat_resize(data->node_in_acs + node, data->node_iters[node] + 1,
                    tnw_args->in_ac->num_cols, NAN) > 0) {
            ST_WARNING("Failed to mat_resize for node_in_acs.");
            return -1;
        }

        memcpy(MAT_ROW(data->node_in_acs + node, data->node_iters[node]),
                MAT_ROW(tnw_args->in_ac, tnw_args->batch_i),
                sizeof(real_t) * tnw_args->in_ac->num_cols);
    }

    if (tnw_args->out_er != NULL && child_e - child_s - 1 > 0) {
        // tnw_args->vals1->num_cols is num_cols of node weight
        if (mat_resize(data->node_out_ers + node, data->node_iters[node] + 1,
                    child_e - child_s - 1, NAN) > 0) {
            ST_WARNING("Failed to mat_resize for node_out_ers.");
            return -1;
        }

        memcpy(MAT_ROW(data->node_out_ers + node, data->node_iters[node]),
               MAT_ROW(tnw_args->out_er, tnw_args->batch_i),
               sizeof(real_t) * tnw_args->out_er->num_cols);
    }

    data->node_iters[node]++;

    return 0;
}

// this function fill in the activation into ogu_data_t's buffer
static int fill_tree_nodes(output_t *output, ivec_t *targets,
        mat_t *in_ac, mat_t *out_er, ogu_data_t *data)
{
    tree_nodes_walker_args_t tnw_args;
    int i;

    ST_CHECK_PARAM(output == NULL || data == NULL || targets == NULL
            || in_ac == NULL, -1);

    // clear buffers
    for (i = 0; i < targets->size; i++) {
        if (output_walk_through_path(output, VEC_VAL(targets, i),
                    clear_tree_nodes_walker, (void *)data) < 0) {
            ST_WARNING("Failed to output_walk_through_path.");
            return -1;
        }
    }

    // fill in buffer with in_ac and out_er if not NULL
    tnw_args.in_ac = in_ac;
    tnw_args.out_er = out_er;
    tnw_args.data = data;
    for (i = 0; i < targets->size; i++) {
        tnw_args.batch_i = i;
        if (output_walk_through_path(output, VEC_VAL(targets, i),
                    fill_tree_nodes_walker, (void *)&tnw_args) < 0) {
            ST_WARNING("Failed to output_walk_through_path.");
            return -1;
        }
    }

    return 0;
}

typedef struct _tree_nodes_forward_args_t_ {
    wt_updater_t **wt_updaters;
    real_t scale;
    mat_t *node_in_acs;
    mat_t *node_out_acs;
} tree_nodes_fwd_walker_args_t;

static int forward_tree_nodes_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    tree_nodes_fwd_walker_args_t *tnfw_args;
    mat_t *in_ac;
    mat_t *out_ac;
    mat_t *wt;
    vec_t *bias;

    tnfw_args = (tree_nodes_fwd_walker_args_t *)args;
    in_ac = tnfw_args->node_in_acs + node;
    out_ac = tnfw_args->node_out_acs + node;
    wt = &tnfw_args->wt_updaters[node]->wt;
    bias = &tnfw_args->wt_updaters[node]->bias;

    if (output->norm == ON_SOFTMAX) {
        if (child_e - child_s - 1 > 0) {
            if (mat_resize(out_ac, in_ac->num_rows, wt->num_rows, NAN) < 0) {
                ST_WARNING("Failed to mat_resize.");
                return -1;
            }

            if (add_mat_mat(tnfw_args->scale, in_ac, MT_NoTrans,
                        wt, MT_Trans, 0.0, out_ac) < 0) {
                ST_WARNING("Failed to add_mat_mat.");
                return -1;
            }

            if (bias->size > 0) {
                if (mat_add_vec(out_ac, bias, tnfw_args->scale) < 0) {
                    ST_WARNING("Failed to mat_add_vec.");
                    return -1;
                }
            }
        }
    }

    return 0;
}

// this function do forward with ogu_data_t's buffer
static int forward_tree_nodes(output_t *output, ivec_t *targets,
        wt_updater_t **wt_updaters, real_t scale, mat_t *node_in_acs,
        mat_t *node_out_acs)
{
    tree_nodes_fwd_walker_args_t tnfw_args;
    int i;

    ST_CHECK_PARAM(output == NULL || targets == NULL || wt_updaters == NULL
            || node_in_acs == NULL || node_out_acs == NULL, -1);

    // do matrix multiplication
    tnfw_args.wt_updaters = wt_updaters;
    tnfw_args.scale = scale;
    tnfw_args.node_in_acs = node_in_acs;
    tnfw_args.node_out_acs = node_in_acs;
    for (i = 0; i < targets->size; i++) {
        if (output_walk_through_path(output, VEC_VAL(targets, i),
                    forward_tree_nodes_walker, (void *)&tnfw_args) < 0) {
            ST_WARNING("Failed to output_walk_through_path.");
            return -1;
        }
    }

    return 0;
}

int out_glue_updater_forward(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, egs_batch_t *batch,
        mat_t *in_ac, mat_t *out_ac)
{
    ogu_data_t *data;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || batch == NULL || out_ac == NULL, -1);

    data = (ogu_data_t *)glue_updater->extra;

    if (ivec_set(&data->targets, batch->targets, batch->num_egs) < 0) {
        ST_WARNING("Failed to ivec_set targets.");
        return -1;
    }

    if (out_glue_updater_forward_out_words(glue_updater, comp_updater,
                &data->targets, in_ac, out_ac) < 0) {
        ST_WARNING("Failed to out_glue_updater_forward_out_words.");
        return -1;
    }

    return 0;
}

typedef struct _tree_nodes_backprop_args_t_ {
    wt_updater_t **wt_updaters;
    real_t scale;
    mat_t *node_in_acs;
    mat_t *node_out_ers;
    mat_t *in_er;
} tree_nodes_bp_walker_args_t;

static int backprop_tree_nodes_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    tree_nodes_bp_walker_args_t *tnbw_args;
    mat_t *in_ac;
    mat_t *out_er;
    mat_t *in_er;
    wt_updater_t *wt_updater;

    tnbw_args = (tree_nodes_bp_walker_args_t *)args;
    in_ac = tnbw_args->node_in_acs + node;
    out_er = tnbw_args->node_out_ers + node;
    in_er = tnbw_args->in_er;
    wt_updater = tnbw_args->wt_updaters[node];

    // diemension same as in_ac
    if (mat_resize(in_er, in_ac->num_rows, in_ac->num_cols, NAN) < 0) {
        ST_WARNING("Failed to mat_resize for in_er.");
        return -1;
    }

    // propagate from out_er to in_er
    if (propagate_error(&wt_updater->wt, out_er, tnbw_args->scale,
                wt_updater->param.er_cutoff, in_er) < 0) {
        ST_WARNING("Failed to propagate_error.");
        return -1;
    }

    if (output->norm == ON_SOFTMAX) {
        if (child_e - child_s - 1 > 0) {
            if (wt_update(wt_updater, out_er, tnbw_args->scale,
                        in_ac, 1.0, NULL, NULL) < 0) {
                ST_WARNING("Failed to wt_update.");
                return -1;
            }
        }
    }

    return 0;
}

// this function do back-prop within ogu_data_t's buffer
static int backprop_tree_nodes(output_t *output, ivec_t *targets,
        wt_updater_t **wt_updaters, real_t scale, mat_t *node_in_acs,
        mat_t *node_out_ers, mat_t *in_er)
{
    tree_nodes_bp_walker_args_t tnbw_args;
    int i;

    ST_CHECK_PARAM(output == NULL || targets == NULL || wt_updaters == NULL
            || node_in_acs == NULL || node_out_ers == NULL
            || in_er == NULL, -1);

    // do matrix multiplication
    tnbw_args.wt_updaters = wt_updaters;
    tnbw_args.scale = scale;
    tnbw_args.node_in_acs = node_in_acs;
    tnbw_args.node_out_ers = node_out_ers;
    tnbw_args.in_er = in_er;
    for (i = 0; i < targets->size; i++) {
        if (output_walk_through_path(output, VEC_VAL(targets, i),
                    backprop_tree_nodes_walker, (void *)&tnbw_args) < 0) {
            ST_WARNING("Failed to output_walk_through_path.");
            return -1;
        }
    }

    return 0;
}

int out_glue_updater_backprop(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, egs_batch_t *batch,
        mat_t *in_ac, mat_t *out_er, mat_t *in_er)
{
    output_t *output;
    ogu_data_t *data;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || batch == NULL || out_er == NULL, -1);

    data = (ogu_data_t *)glue_updater->extra;
    output = comp_updater->out_updater->output;

    if (ivec_set(&data->targets, batch->targets, batch->num_egs) < 0) {
        ST_WARNING("Failed to ivec_set targets.");
        return -1;
    }

    if (fill_tree_nodes(output, &data->targets,
                NULL /* in_ac should be filled druing forward pass. */,
                out_er, data) < 0) {
        ST_WARNING("Failed to fill_tree_nodes.");
        return -1;
    }

    if (backprop_tree_nodes(output, &data->targets, glue_updater->wt_updaters,
                comp_updater->comp->comp_scale, data->node_in_acs,
                comp_updater->out_updater->node_ers, in_er) < 0) {
        ST_WARNING("Failed to backprop_tree_nodes.");
        return -1;
    }

    return 0;
}

int out_glue_updater_forward_out(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, output_node_id_t node,
        mat_t *in_ac, mat_t *out_ac)
{
    output_t *output;
    mat_t *wt;
    vec_t *bias;

    real_t scale;
    output_node_id_t child_s, child_e;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || node == OUTPUT_NODE_NONE || out_ac == NULL, -1);

    output = comp_updater->out_updater->output;

    child_s = s_children(output->tree, node);
    child_e = e_children(output->tree, node);

    if (child_s >= child_e) {
        return 0;
    }

    wt = &glue_updater->wt_updaters[node]->wt;
    bias = &glue_updater->wt_updaters[node]->bias;
    scale = comp_updater->comp->comp_scale;

    if (output->norm == ON_SOFTMAX) {
        if (child_e - child_s - 1 > 0) {
            if (mat_resize(out_ac, in_ac->num_rows, wt->num_rows, NAN) < 0) {
                ST_WARNING("Failed to mat_resize.");
                return -1;
            }

            if (add_mat_mat(scale, in_ac, MT_NoTrans,
                        wt, MT_Trans, 0.0, out_ac) < 0) {
                ST_WARNING("Failed to add_mat_mat.");
                return -1;
            }

            if (bias->size > 0) {
                if (mat_add_vec(out_ac, bias, scale) < 0) {
                    ST_WARNING("Failed to mat_add_vec.");
                    return -1;
                }
            }
        }
    }

    return 0;
}

int out_glue_updater_forward_out_words(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, ivec_t *words,
        mat_t *in_ac, mat_t *out_ac)
{
    ogu_data_t *data;
    output_t *output;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || words == NULL || out_ac == NULL, -1);

    if (words->size != in_ac->num_rows) {
        ST_WARNING("words->size and in_ac->num_rows not match");
        return -1;
    }

    data = (ogu_data_t *)glue_updater->extra;
    output = comp_updater->out_updater->output;

    if (fill_tree_nodes(output, words, in_ac, NULL, data) < 0) {
        ST_WARNING("Failed to fill_tree_nodes.");
        return -1;
    }

    if (forward_tree_nodes(output, words, glue_updater->wt_updaters,
                comp_updater->comp->comp_scale, data->node_in_acs,
                comp_updater->out_updater->node_acs) < 0) {
        ST_WARNING("Failed to forward_tree_nodes.");
        return -1;
    }

    return 0;
}
