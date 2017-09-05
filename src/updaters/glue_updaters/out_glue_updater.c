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
               ac = MAT_ROW(node_acs[node], node_iters[node])
               er = MAT_ROW(node_ers[node], node_iters[node])
               node_iters[node]++
    */
    int num_nodes;
    int *node_iters;
    mat_t *node_acs;
    mat_t *node_ers;
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

    if (data->node_acs != NULL) {
        for (i = 0; i < data->num_nodes; i++) {
            mat_destroy(data->node_acs + i);
        }
        safe_st_free(data->node_acs);
    }

    if (data->node_ers != NULL) {
        for (i = 0; i < data->num_nodes; i++) {
            mat_destroy(data->node_ers + i);
        }
        safe_st_free(data->node_ers);
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

    data->node_acs = (mat_t *)st_malloc(sizeof(mat_t) * num_nodes);
    if (data->node_acs == NULL) {
        ST_WARNING("Failed to st_malloc node_acs.");
        goto ERR;
    }
    memset(data->node_acs, 0, sizeof(mat_t) * num_nodes);

    if (backprop) {
        data->node_ers = (mat_t *)st_malloc(sizeof(mat_t) * num_nodes);
        if (data->node_ers == NULL) {
            ST_WARNING("Failed to st_malloc node_ers.");
            goto ERR;
        }
        memset(data->node_ers, 0, sizeof(mat_t) * num_nodes);
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

static int out_glue_updater_forward_node(glue_updater_t *glue_updater,
        output_t *output, output_node_id_t node,
        output_node_id_t child_s, output_node_id_t child_e,
        mat_t *in_ac, mat_t *out_ac, mat_t scale, bool *forwarded)
{
    real_t *wt;
    real_t *bias;
    int layer_size;
    output_node_id_t ch;

    ST_CHECK_PARAM(glue_updater == NULL, -1);

    wt = glue_updater->wt_updater->wt;
    bias = glue_updater->wt_updater->bias;
    layer_size = glue_updater->wt_updater->col;

    if (forwarded != NULL) {
        if (forwarded[node]) {
            return 0;
        }
        forwarded[node] = true;
    }

    if (output->norm == ON_SOFTMAX) {
        if (child_e - child_s - 1 > 0) {
            add_mat_mat
            matXvec(out_ac + child_s,
                    wt + output_param_idx(output, child_s) * layer_size,
                    in_ac, child_e - child_s - 1, layer_size, scale);
        }
        if (bias != NULL) {
            for (ch = child_s; ch < child_e - 1; ch++) {
                out_ac[ch] += scale * bias[output_param_idx(output, ch)];
            }
        }
    }

    return 0;
}

typedef struct _out_walker_args_t_ {
    glue_updater_t *glue_updater;
    output_t *output;
    real_t scale;
    real_t *in_ac;
    real_t *out_ac;
    real_t *out_er;
    real_t *in_er;
    bool *forwarded;
} out_walker_args_t;

static int out_forward_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    out_walker_args_t *ow_args;

    ow_args = (out_walker_args_t *) args;

    if (out_glue_updater_forward_node(ow_args->glue_updater,
                ow_args->output, node, child_s, child_e,
                ow_args->in_ac, ow_args->out_ac, ow_args->scale,
                ow_args->forwarded) < 0) {
        ST_WARNING("Failed to out_glue_updater_forward_node.");
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

    mat_clear(data->node_acs + node);
    if (data->node_ers != NULL) {
        mat_clear(data->node_ers + node);
    }
    data->node_iters[node] = 0;

    return 0;
}

typedef struct _tree_nodes_walker_args_t_ {
    mat_t *vals;
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
    data = args->data;

    memcpy(MAT_ROW(data->node_acs + node, data->node_iters[node]),
           MAT_ROW(tnw_args->vals, batch_i),
           sizeof(real_t) * tnw_args->vals->num_cols);
    data->node_iters[node]++;

    return 0;
}

// this function fill in the activation and error in ogu_data_t's buffer
static int fill_tree_nodes(output_t *output, egs_batch_t *batch,
        mat_t *in_ac, ogu_data_t *data)
{
    tree_nodes_walker_args_t tnw_args;
    int i;

    ST_CHECK_PARAM(output == NULL || data == NULL || batch == NULL, -1);

    for (i = 0; i < batch->num_egs; i++) {
        if (output_walk_through_path(output, batch->targets[i],
                    clear_tree_nodes_walker, (void *)data) < 0) {
            ST_WARNING("Failed to output_walk_through_path.");
            return -1;
        }
    }

    tnw_args.vals = in_ac;
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

int out_glue_updater_forward(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, egs_batch_t *batch,
        mat_t *in_ac, mat_t *out_ac)
{
    out_walker_args_t ow_args;
    ogu_data_t *data;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || batch == NULL || out_ac == NULL, -1);

    data = (ogu_data_t *)glue_updater->extra;

    if (fill_tree_nodes(comp_updater->out_updater->output,
                batch, in_ac, data) < 0) {
        ST_WARNING("Failed to fill_tree_nodes.");
        return -1;
    }

    if (forward_tree_nodes(glue_updater) < 0) {
        ST_WARNING("Failed to forward_tree_nodes.");
        return -1;
    }

    ow_args.glue_updater = glue_updater;
    ow_args.output = comp_updater->out_updater->output;
    ow_args.in_ac = in_ac;
    ow_args.out_ac = out_ac;
    ow_args.scale = comp_updater->comp->comp_scale;
    ow_args.forwarded = NULL;
    if (output_walk_through_path(comp_updater->out_updater->output,
                input_sent->words[input_sent->tgt_pos],
                out_forward_walker, (void *)&ow_args) < 0) {
        ST_WARNING("Failed to output_walk_through_path.");
        return -1;
    }

    return 0;
}

static int out_backprop_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    out_walker_args_t *ow_args;
    wt_updater_t *wt_updater;
    real_t *wt;
    int layer_size;

    ow_args = (out_walker_args_t *) args;

    wt_updater = ow_args->glue_updater->wt_updater;

    wt = wt_updater->wt;
    layer_size = wt_updater->col;

    propagate_error(ow_args->in_er, ow_args->out_er + child_s,
            wt + output_param_idx(ow_args->output, child_s) * layer_size,
            layer_size, child_e - child_s - 1,
            wt_updater->param.er_cutoff, ow_args->scale);

    if (output->norm == ON_SOFTMAX) {
        if (child_e <= child_s + 1) {
            return 0;
        }

        if (wt_update(wt_updater, NULL, node, ow_args->out_er + child_s,
                    ow_args->scale, ow_args->in_ac, 1.0, NULL) < 0) {
            ST_WARNING("Failed to wt_update.");
            return -1;
        }
    }

    return 0;
}

int out_glue_updater_backprop(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, egs_batch_t *batch,
        mat_t *in_ac, mat_t *out_er, mat_t *in_er)
{
    out_walker_args_t ow_args;

    output_t *output;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || batch == NULL, -1);

    output = comp_updater->out_updater->output;

    ow_args.glue_updater = glue_updater;
    ow_args.output = comp_updater->out_updater->output;
    ow_args.in_ac = in_ac;
    ow_args.out_er = out_er;
    ow_args.in_er = in_er;
    ow_args.scale = comp_updater->comp->comp_scale;
    if (output_walk_through_path(output, input_sent->words[input_sent->tgt_pos],
                out_backprop_walker, (void *)&ow_args) < 0) {
        ST_WARNING("Failed to output_walk_through_path.");
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
