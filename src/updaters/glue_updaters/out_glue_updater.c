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
    /* these variables are used to store the ac or er for tree nodes of all
       words in a batch. They are all array with size equals to num_tree_nodes

       usage is:
       for word in batch:
           for node in path(word):
               in0 = MAT_ROW(node_ins0[node], node_iters[node])
               in1 = MAT_ROW(node_ins1[node], node_iters[node])
               out = MAT_ROW(node_outs[node], node_iters[node])
               node_iters[node]++
    */
    int num_nodes;
    int *node_iters;
    mat_t *node_ins0;
    mat_t *node_ins1;
    mat_t *node_outs;
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

    safe_st_free(data->node_iters);

    if (data->node_ins0 != NULL) {
        for (i = 0; i < data->num_nodes; i++) {
            mat_destroy(data->node_ins0 + i);
        }
        safe_st_free(data->node_ins0);
    }

    if (data->node_ins1 != NULL) {
        for (i = 0; i < data->num_nodes; i++) {
            mat_destroy(data->node_ins1 + i);
        }
        safe_st_free(data->node_ins1);
    }

    if (data->node_outs != NULL) {
        for (i = 0; i < data->num_nodes; i++) {
            mat_destroy(data->node_outs + i);
        }
        safe_st_free(data->node_outs);
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

int ogu_data_setup(ogu_data_t *data, int num_nodes, bool backprop)
{
    ST_CHECK_PARAM(data == NULL, -1);

    data->num_nodes = num_nodes;

    data->node_iters = (int *)st_malloc(sizeof(int) * num_nodes);
    if (data->node_iters == NULL) {
        ST_WARNING("Failed to st_malloc node_iters.");
        goto ERR;
    }
    memset(data->node_iters, 0, sizeof(int) * num_nodes);

    data->node_ins0 = (mat_t *)st_malloc(sizeof(mat_t) * num_nodes);
    if (data->node_ins0 == NULL) {
        ST_WARNING("Failed to st_malloc node_ins0.");
        goto ERR;
    }
    memset(data->node_ins0, 0, sizeof(mat_t) * num_nodes);

    data->node_outs = (mat_t *)st_malloc(sizeof(mat_t) * num_nodes);
    if (data->node_outs == NULL) {
        ST_WARNING("Failed to st_malloc node_outs.");
        goto ERR;
    }
    memset(data->node_outs, 0, sizeof(mat_t) * num_nodes);

    if (backprop) {
        data->node_ins1 = (mat_t *)st_malloc(sizeof(mat_t) * num_nodes);
        if (data->node_ins1 == NULL) {
            ST_WARNING("Failed to st_malloc node_ins1.");
            goto ERR;
        }
        memset(data->node_ins1, 0, sizeof(mat_t) * num_nodes);
    }

    return 0;
ERR:
    ogu_data_destroy(data);
    return -1;
}

int out_glue_updater_setup(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, bool backprop)
{
    st_int_seg_t *segs = NULL;
    int n_seg;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL, -1);

    if (ogu_data_setup((ogu_data_t *)glue_updater->extra,
                comp_updater->out_updater->output->tree->num_node,
                backprop) < 0) {
        ST_WARNING("Failed to ogu_data_setup");
        goto ERR;
    }

    if (backprop) {
        if (glue_updater->wt_updater->segs == NULL) {
            segs = output_gen_segs(comp_updater->out_updater->output, &n_seg);
            if (segs == NULL || n_seg <= 0) {
                ST_WARNING("Failed to output_gen_segs.");
                goto ERR;
            }

            if (wt_updater_set_segs(glue_updater->wt_updater, segs, n_seg) < 0) {
                ST_WARNING("Failed to wt_updater_set_segs.");
                goto ERR;
            }

            safe_st_free(segs);
        }
    }

    return 0;

ERR:
    safe_st_free(segs);
    return -1;
}

static int clear_tree_nodes_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    ogu_data_t *data;

    data = (ogu_data_t *) args;

    mat_clear(data->node_ins0 + node);
    mat_clear(data->node_outs + node);
    if (data->node_ins1 != NULL) {
        mat_clear(data->node_ins1 + node);
    }
    data->node_iters[node] = 0;

    return 0;
}

typedef struct _tree_nodes_walker_args_t_ {
    mat_t *val0;
    mat_t *val1;
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

    // tnw_args->vals0->num_cols is the hidden size of prev layer
    if (mat_resize(data->node_ins0 + node, data->node_iters[node] + 1,
                tnw_args->val0->num_cols, NAN) > 0) {
        ST_WARNING("Failed to mat_resize for node_ins0.");
        return -1;
    }

    memcpy(MAT_ROW(data->node_ins0 + node, data->node_iters[node]),
           MAT_ROW(tnw_args->val0, tnw_args->batch_i),
           sizeof(real_t) * tnw_args->val0->num_cols);

    if (tnw_args->val1 != NULL && child_e - child_s - 1 > 0) {
        // tnw_args->vals1->num_cols is num_cols of node weight
        if (mat_resize(data->node_ins1 + node, data->node_iters[node] + 1,
                    child_e - child_s - 1, NAN) > 0) {
            ST_WARNING("Failed to mat_resize for node_ins1.");
            return -1;
        }

        memcpy(MAT_ROW(data->node_ins1 + node, data->node_iters[node]),
               MAT_ROW(tnw_args->val1, tnw_args->batch_i),
               sizeof(real_t) * tnw_args->val1->num_cols);
    }

    data->node_iters[node]++;

    return 0;
}

// this function fill in the activation into ogu_data_t's buffer
static int fill_tree_nodes(output_t *output, egs_batch_t *batch,
        mat_t *in_ac, mat_t *out_er, ogu_data_t *data)
{
    tree_nodes_walker_args_t tnw_args;
    int i;

    ST_CHECK_PARAM(output == NULL || data == NULL || batch == NULL
            || in_ac == NULL, -1);

    // clear buffers
    for (i = 0; i < batch->num_egs; i++) {
        if (output_walk_through_path(output, batch->targets[i],
                    clear_tree_nodes_walker, (void *)data) < 0) {
            ST_WARNING("Failed to output_walk_through_path.");
            return -1;
        }
    }

    // fill in buffer with in_ac and out_er if not NULL
    tnw_args.val0 = in_ac;
    tnw_args.val1 = out_er;
    tnw_args.data = data;
    for (i = 0; i < batch->num_egs; i++) {
        tnw_args.batch_i = i;
        if (output_walk_through_path(output, batch->targets[i],
                    fill_tree_nodes_walker, (void *)&tnw_args) < 0) {
            ST_WARNING("Failed to output_walk_through_path.");
            return -1;
        }
    }

    return 0;
}

typedef struct _tree_nodes_run_alker_args_t_ {
    mat_t *wt;
    vec_t *bias;
    real_t scale;
    ogu_data_t *data;
} tree_nodes_run_walker_args_t;

static int forward_tree_nodes_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    mat_t sub_wt;
    vec_t sub_bias;
    tree_nodes_run_walker_args_t *tnrw_args;
    ogu_data_t *data;

    tnrw_args = (tree_nodes_run_walker_args_t *)args;
    data = tnrw_args->data;

    if (output->norm == ON_SOFTMAX) {
        if (child_e - child_s - 1 > 0) {
            if (mat_submat(tnrw_args->wt, output_param_idx(output, child_s),
                        child_e - child_s - 1, 0, -1, &sub_wt) < 0) {
                ST_WARNING("Failed to mat_submat for sub_wt");
                return -1;
            }
            if (mat_resize(data->node_outs + node,
                        data->node_ins0[node].num_rows, sub_wt.num_rows,
                        NAN) < 0) {
                ST_WARNING("Failed to mat_resize.");
                return -1;
            }

            if (add_mat_mat(tnrw_args->scale, data->node_ins0 + node, MT_NoTrans,
                        &sub_wt, MT_Trans, 0.0, data->node_outs + node) < 0) {
                ST_WARNING("Failed to add_mat_mat.");
                return -1;
            }

            if (tnrw_args->bias->size > 0) {
                if (vec_subvec(tnrw_args->bias,
                            output_param_idx(output, child_s),
                            child_e - child_s - 1, &sub_bias) < 0) {
                    ST_WARNING("Failed to vec_subvec.");
                    return -1;
                }
                if (mat_add_vec(data->node_outs + node, &sub_bias,
                            tnrw_args->scale) < 0) {
                    ST_WARNING("Failed to mat_add_vec.");
                    return -1;
                }
            }
        }
    }

    return 0;
}

// this function do forward within ogu_data_t's buffer
static int forward_tree_nodes(output_t *output, egs_batch_t *batch,
        mat_t *wt, vec_t *bias, real_t scale, ogu_data_t *data)
{
    tree_nodes_run_walker_args_t tnrw_args;
    int i;

    ST_CHECK_PARAM(output == NULL || batch == NULL
            || data == NULL || wt == NULL || bias == NULL, -1);

    // do matrix multiplication
    tnrw_args.wt = wt;
    tnrw_args.bias = bias;
    tnrw_args.scale = scale;
    tnrw_args.data = data;
    for (i = 0; i < batch->num_egs; i++) {
        if (output_walk_through_path(output, batch->targets[i],
                    forward_tree_nodes_walker, (void *)&tnrw_args) < 0) {
            ST_WARNING("Failed to output_walk_through_path.");
            return -1;
        }
    }

    return 0;
}

static int reset_tree_nodes_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    ogu_data_t *data;

    data = (ogu_data_t *) args;

    data->node_iters[node] = 0;

    return 0;
}

static int acc_tree_nodes_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    mat_t sub_vals;
    mat_t out;
    tree_nodes_walker_args_t *tnw_args;
    ogu_data_t *data;

    tnw_args = (tree_nodes_walker_args_t *)args;
    data = tnw_args->data;

    // tnw_args->vals if a full matrix for all nodes
    if (mat_resize(tnw_args->val0, tnw_args->batch_i + 1,
                data->num_nodes, 0.0) > 0) {
        ST_WARNING("Failed to mat_resize for val0.");
        return -1;
    }

    if (mat_submat(tnw_args->val0, tnw_args->batch_i, 1,
                child_s, child_e - child_s - 1, &sub_vals) < 0) {
        ST_WARNING("Failed to mat_submat for sub_vals.");
        return -1;
    }

    // write to the child position in out_ac
    if (mat_submat(data->node_outs + node, data->node_iters[node], 1,
                0, -1, &out) < 0) {
        ST_WARNING("Failed to mat_submat for out.");
        return -1;
    }

    if (mat_add_elems(&sub_vals, &out, &sub_vals) < 0) {
        ST_WARNING("Failed to mat_add_elems to sub_vals.");
        return -1;
    }

    data->node_iters[node]++;

    return 0;
}

// this function accumulate the output in buffer to out
static int acc_tree_nodes(output_t *output, egs_batch_t *batch,
        ogu_data_t *data, mat_t *out)
{
    tree_nodes_walker_args_t tnw_args;
    int i;

    ST_CHECK_PARAM(output == NULL || batch == NULL ||
            data == NULL || out == NULL, -1);

    // reset node_iters
    for (i = 0; i < batch->num_egs; i++) {
        if (output_walk_through_path(output, batch->targets[i],
                    reset_tree_nodes_walker, (void *)data) < 0) {
            ST_WARNING("Failed to output_walk_through_path.");
            return -1;
        }
    }

    // accumulate the output in buffer to out
    tnw_args.val0 = out;
    tnw_args.val1 = NULL;
    tnw_args.data = data;
    for (i = 0; i < batch->num_egs; i++) {
        tnw_args.batch_i = i;
        if (output_walk_through_path(output, batch->targets[i],
                    acc_tree_nodes_walker, (void *)&tnw_args) < 0) {
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
    output_t *output;
    ogu_data_t *data;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || batch == NULL || out_ac == NULL, -1);

    data = (ogu_data_t *)glue_updater->extra;
    output = comp_updater->out_updater->output;

    if (fill_tree_nodes(output, batch, in_ac, NULL, data) < 0) {
        ST_WARNING("Failed to fill_tree_nodes.");
        return -1;
    }

    if (forward_tree_nodes(output, batch, &glue_updater->wt_updater->wt,
                &glue_updater->wt_updater->bias,
                comp_updater->comp->comp_scale, data) < 0) {
        ST_WARNING("Failed to forward_tree_nodes.");
        return -1;
    }

    if (acc_tree_nodes(output, batch, data, out_ac) < 0) {
        ST_WARNING("Failed to acc_tree_nodes.");
        return -1;
    }

    return 0;
}

static int backprop_tree_nodes_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    tree_nodes_run_walker_args_t *tnrw_args;
    ogu_data_t *data;

    tnrw_args = (tree_nodes_run_walker_args_t *)args;
    data = tnrw_args->data;

    // diemension same as in_ac
    if (mat_resize(data->node_outs + node,
                data->node_ins0[node].num_rows,
                data->node_ins0[node].num_cols,
                NAN) < 0) {
        ST_WARNING("Failed to mat_resize for node_outs.");
        return -1;
    }

    // propagate from out_er to in_er
    if (propagate_error(tnrw_args->wt, data->node_ins1 + node,
                tnrw_args->scale, wt_updater->param.er_cutoff,
                data->node_outs + node) < 0) {
        ST_WARNING("Failed to propagate_error.");
        return -1;
    }

    if (output->norm == ON_SOFTMAX) {
        if (child_e - child_s - 1 > 0) {
            if (wt_update(sub_wt_updater, data->node_ins1 + node, tnrw_args->scale,
                        data->node_ins0 + node, 1.0, NULL, NULL) < 0) {
                ST_WARNING("Failed to wt_update.");
                return -1;
            }
        }
    }

    return 0;
}

// this function do back-prop within ogu_data_t's buffer
static int backprop_tree_nodes(output_t *output, egs_batch_t *batch,
        mat_t *wt, vec_t *bias, real_t scale, ogu_data_t *data)
{
    tree_nodes_run_walker_args_t tnrw_args;
    int i;

    ST_CHECK_PARAM(output == NULL || batch == NULL
            || data == NULL || wt == NULL || bias == NULL, -1);

    // do matrix multiplication
    tnrw_args.wt = wt;
    tnrw_args.bias = bias;
    tnrw_args.scale = scale;
    tnrw_args.data = data;
    for (i = 0; i < batch->num_egs; i++) {
        if (output_walk_through_path(output, batch->targets[i],
                    backprop_tree_nodes_walker, (void *)&tnrw_args) < 0) {
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

    if (fill_tree_nodes(output, batch, in_ac, out_er, data) < 0) {
        ST_WARNING("Failed to fill_tree_nodes.");
        return -1;
    }

    if (backprop_tree_nodes(output, batch, &glue_updater->wt_updater->wt,
                &glue_updater->wt_updater->bias,
                comp_updater->comp->comp_scale, data) < 0) {
        ST_WARNING("Failed to backprop_tree_nodes.");
        return -1;
    }

    if (acc_tree_nodes(output, batch, data, in_er) < 0) {
        ST_WARNING("Failed to acc_tree_nodes.");
        return -1;
    }

    return 0;
}

int out_glue_updater_forward_out(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, output_node_id_t node,
        mat_t *in_ac, mat_t *out_ac)
{
    output_t *output;

    output_node_id_t child_s, child_e;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || node == OUTPUT_NODE_NONE || out_ac == NULL, -1);

    output = comp_updater->out_updater->output;

    child_s = s_children(output->tree, node);
    child_e = e_children(output->tree, node);

    if (child_s >= child_e) {
        return 0;
    }

    if (out_glue_updater_forward_node(glue_updater, output, node,
                child_s, child_e, in_ac, out_ac,
                comp_updater->comp->comp_scale, NULL) < 0) {
        ST_WARNING("Failed to out_glue_updater_forward_node.");
        return -1;
    }

    return 0;
}

int out_glue_updater_forward_out_word(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, ivec_t *words,
        mat_t *in_ac, mat_t *out_ac)
{
    out_walker_args_t ow_args;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || word < 0 || out_ac == NULL, -1);

    ow_args.glue_updater = glue_updater;
    ow_args.output = comp_updater->out_updater->output;
    ow_args.in_ac = in_ac;
    ow_args.out_ac = out_ac;
    ow_args.scale = comp_updater->comp->comp_scale;
    ow_args.forwarded = glue_updater->forwarded;
    if (output_walk_through_path(comp_updater->out_updater->output,
                word, out_forward_walker, (void *)&ow_args) < 0) {
        ST_WARNING("Failed to output_walk_through_path.");
        return -1;
    }

    return 0;
}
