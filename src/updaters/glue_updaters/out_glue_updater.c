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

void out_glue_updater_destroy(glue_updater_t *glue_updater)
{
    if (glue_updater == NULL) {
        return;
    }
}

int out_glue_updater_init(glue_updater_t *glue_updater)
{
    glue_t *glue;

    ST_CHECK_PARAM(glue_updater == NULL, -1);

    glue = glue_updater->glue;

    if (strcasecmp(glue_updater->glue->type, OUT_GLUE_NAME) != 0) {
        ST_WARNING("Not a out glue_updater. [%s]",
                glue_updater->glue->type);
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

typedef struct _out_walker_args_t_ {
    layer_updater_t *in_layer_updater;
    out_updater_t *out_updater;
    real_t in_scale;
    real_t out_scale;

    wt_updater_t *wt_updater;
    count_t n_step;
} out_walker_args_t;

static int out_forward_walker(output_t *output, output_node_id_t node,
        output_node_id_t next_node,
        output_node_id_t child_s, output_node_id_t child_e, void *args)
{
    out_walker_args_t *ow_args;
    real_t *wt;
    real_t *in_ac;
    real_t *out_ac;
    real_t scale;
    int layer_size;

    ow_args = (out_walker_args_t *) args;

    wt = ow_args->wt_updater->wt;
    in_ac = ow_args->in_layer_updater->ac;
    out_ac = ow_args->out_updater->ac;
    layer_size = ow_args->wt_updater->col;
    scale = ow_args->in_scale * ow_args->out_scale;

    if (output->norm == ON_SOFTMAX) {
        if (child_e - child_s - 1 > 0) {
            matXvec(out_ac + child_s, wt + child_s * layer_size,
                    in_ac, child_e - child_s - 1, layer_size, scale);
        }
    }

    return 0;
}

int out_glue_updater_forward(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, int *words, int n_word, int tgt_pos)
{
    out_walker_args_t ow_args;
    glue_t *glue;
    layer_updater_t *in_layer_updater;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || words == NULL, -1);

    glue = glue_updater->glue;
    in_layer_updater = comp_updater->layer_updaters[glue->in_layers[0]];

    ow_args.in_layer_updater = in_layer_updater;
    ow_args.out_updater = comp_updater->out_updater;
    ow_args.in_scale = glue->in_scales[0];
    ow_args.out_scale = glue->out_scales[0];
    ow_args.wt_updater = glue_updater->wt_updater;
    ow_args.n_step = -1;
    if (output_walk_through_path(comp_updater->out_updater->output,
                words[tgt_pos], out_forward_walker, (void *)&ow_args) < 0) {
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
    st_int_seg_t seg;
    real_t *wt;
    real_t *in_er;
    real_t *in_ac;
    real_t *out_er;
    real_t scale;
    int layer_size;

    ow_args = (out_walker_args_t *) args;

    wt = ow_args->wt_updater->wt;
    in_er = ow_args->in_layer_updater->er;
    in_ac = ow_args->in_layer_updater->ac;
    out_er = ow_args->out_updater->er;
    layer_size = ow_args->wt_updater->col;
    scale = ow_args->in_scale * ow_args->out_scale;

    if (output->norm == ON_SOFTMAX) {
        if (child_e <= child_s + 1) {
            return 0;
        }

        seg.s = child_s;
        seg.e = child_e - 1;
        if (wt_update(ow_args->wt_updater, ow_args->n_step, 0,
                    out_er, ow_args->out_scale, &seg,
                    in_ac, ow_args->in_scale, NULL) < 0) {
            ST_WARNING("Failed to wt_update.");
            return -1;
        }
    }

    propagate_error(in_er, out_er + child_s,
            wt + child_s * layer_size, layer_size, child_e - child_s,
            ow_args->wt_updater->param.er_cutoff, scale);

    return 0;
}

int out_glue_updater_backprop(glue_updater_t *glue_updater, count_t n_step,
        comp_updater_t *comp_updater, int *words, int n_word, int tgt_pos)
{
    out_walker_args_t ow_args;
    layer_updater_t *in_layer_updater;
    glue_t *glue;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || words == NULL, -1);

    glue = glue_updater->glue;
    in_layer_updater = comp_updater->layer_updaters[glue->in_layers[0]];

    ow_args.in_layer_updater = in_layer_updater;
    ow_args.out_updater = comp_updater->out_updater;
    ow_args.in_scale = glue->in_scales[0];
    ow_args.out_scale = glue->out_scales[0];
    ow_args.wt_updater = glue_updater->wt_updater;
    ow_args.n_step = n_step;
    if (output_walk_through_path(comp_updater->out_updater->output,
                words[tgt_pos], out_backprop_walker, (void *)&ow_args) < 0) {
        ST_WARNING("Failed to output_walk_through_path.");
        return -1;
    }

    if (wt_flush(glue_updater->wt_updater, n_step) < 0) {
        ST_WARNING("Failed to wt_flush.");
        return -1;
    }

    return 0;
}
