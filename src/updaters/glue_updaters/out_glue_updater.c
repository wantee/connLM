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

int out_glue_updater_init(glue_updater_t *glue_updater)
{
    glue_t *glue;

    ST_CHECK_PARAM(glue_updater == NULL, -1);

    glue = glue_updater->glue;

    if (strcasecmp(glue->type, OUT_GLUE_NAME) != 0) {
        ST_WARNING("Not a out glue_updater. [%s]", glue->type);
        return -1;
    }

    glue_updater->wt_updater = wt_updater_create(&glue->param, glue->wt->mat,
            glue->wt->row, glue->wt->col, WT_UT_SEG);
    if (glue_updater->wt_updater == NULL) {
        ST_WARNING("Failed to wt_updater_create.");
        goto ERR;
    }

    return 0;

ERR:
    wt_updater_destroy(glue_updater->wt_updater);
    return -1;
}

static int out_glue_updater_forward_node(glue_updater_t *glue_updater,
        output_t *output, output_node_id_t child_s, output_node_id_t child_e,
        real_t *in_ac, real_t *out_ac, real_t scale)
{
    real_t *wt;
    int layer_size;

    ST_CHECK_PARAM(glue_updater == NULL, -1);

    wt = glue_updater->wt_updater->wt;
    layer_size = glue_updater->wt_updater->col;

    if (output->norm == ON_SOFTMAX) {
        if (child_e - child_s - 1 > 0) {
            matXvec(out_ac + child_s,
                    wt + output_param_idx(output, child_s) * layer_size,
                    in_ac, child_e - child_s - 1, layer_size, scale);
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
} out_walker_args_t;

static int out_forward_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    out_walker_args_t *ow_args;

    ow_args = (out_walker_args_t *) args;

    if (out_glue_updater_forward_node(ow_args->glue_updater,
                ow_args->output, child_s, child_e,
                ow_args->in_ac, ow_args->out_ac, ow_args->scale) < 0) {
        ST_WARNING("Failed to out_glue_updater_forward_node.");
        return -1;
    }


    return 0;
}

int out_glue_updater_forward(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, sent_t *input_sent, real_t *in_ac)
{
    out_walker_args_t ow_args;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || input_sent == NULL, -1);

    ow_args.glue_updater = glue_updater;
    ow_args.output = comp_updater->out_updater->output;
    ow_args.in_ac = in_ac;
    ow_args.out_ac = comp_updater->out_updater->ac;
    ow_args.scale = comp_updater->comp->comp_scale;
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
        comp_updater_t *comp_updater, sent_t *input_sent,
        real_t *in_ac, real_t *out_er, real_t *in_er)
{
    out_walker_args_t ow_args;

    output_t *output;
    st_int_seg_t *segs = NULL;
    int n_seg;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || input_sent == NULL, -1);

    output = comp_updater->out_updater->output;
    if (glue_updater->wt_updater->segs == NULL) {
        segs = output_gen_segs(output, &n_seg);
        if (segs == NULL || n_seg <= 0) {
            ST_WARNING("Failed to output_gen_segs.");
            goto ERR;
        }

        if (wt_updater_set_segs(glue_updater->wt_updater, segs, n_seg) < 0) {
            ST_WARNING("Failed to wt_updater_set_segs.");
            goto ERR;
        }

        safe_free(segs);
    }

    ow_args.glue_updater = glue_updater;
    ow_args.output = comp_updater->out_updater->output;
    ow_args.in_ac = in_ac;
    ow_args.out_er = out_er;
    ow_args.in_er = in_er;
    ow_args.scale = comp_updater->comp->comp_scale;
    if (output_walk_through_path(output, input_sent->words[input_sent->tgt_pos],
                out_backprop_walker, (void *)&ow_args) < 0) {
        ST_WARNING("Failed to output_walk_through_path.");
        goto ERR;
    }

    return 0;

ERR:
    safe_free(segs);
    return -1;
}

int out_glue_updater_forward_out(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, output_node_id_t node, real_t *in_ac)
{
    output_t *output;

    output_node_id_t child_s, child_e;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || node == OUTPUT_NODE_NONE, -1);

    output = comp_updater->out_updater->output;

    child_s = s_children(output->tree, node);
    child_e = e_children(output->tree, node);

    if (child_s >= child_e) {
        return 0;
    }

    if (out_glue_updater_forward_node(glue_updater, output,
                child_s, child_e, in_ac, comp_updater->out_updater->ac,
                comp_updater->comp->comp_scale) < 0) {
        ST_WARNING("Failed to out_glue_updater_forward_node.");
        return -1;
    }

    return 0;
}
