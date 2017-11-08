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

#include <stutils/st_log.h>
#include <stutils/st_utils.h>

#include "input.h"
#include "output.h"
#include "out_glue.h"

int out_glue_parse_topo(glue_t *glue, const char *line)
{
    char *p;

    ST_CHECK_PARAM(glue == NULL || line == NULL, -1);

    p = (char *)line;
    while(*p != '\0') {
        if (*p != ' ' || *p != '\t') {
            ST_ERROR("out_glue should be empty. [%s]", line);
            return -1;
        }
    }

    return 0;
}

bool out_glue_check(glue_t *glue, layer_t **layers, int n_layer,
        input_t *input, output_t *output)
{
    ST_CHECK_PARAM(glue == NULL, false);

    if (strcasecmp(glue->type, OUT_GLUE_NAME) != 0) {
        ST_ERROR("Not a out glue. [%s]", glue->type);
        return false;
    }

    if (layers == NULL) {
        ST_ERROR("No layers.");
        return false;
    }

    if (strcasecmp(layers[glue->in_layer]->type, INPUT_LAYER_NAME) == 0) {
        ST_ERROR("out glue: in layer should not be input layer.");
        return false;
    }

    if (strcasecmp(layers[glue->out_layer]->type, OUTPUT_LAYER_NAME) != 0) {
        ST_ERROR("out glue: out layer should be output layer.");
        return false;
    }

    if (glue->out_offset > 0) {
        ST_ERROR("out glue: out layer offset must be zero.");
        return false;
    }

    return true;
}

typedef struct _init_wt_args_t_ {
    weight_t **wts;
    int in_len;
} init_wt_args_t;

static int output_tree_dfs_trav_init_wt(output_tree_t *tree,
        output_node_id_t node, st_stack_t *stack, void *args)
{
    init_wt_args_t *iw_args;
    output_node_id_t child_s, child_e;

    iw_args = (init_wt_args_t *) args;

    child_s = s_children(tree, node);
    child_e = e_children(tree, node);

    if (child_e == OUTPUT_NODE_NONE || child_e - child_s <= 1) {
        return 0;
    }

    if (wt_init(iw_args->wts[node], child_e - child_s - 1, iw_args->in_len) < 0) {
        ST_ERROR("Failed to wt_init["OUTPUT_NODE_FMT"].", node);
        return -1;
    }

    return 0;
}

int out_glue_init_data(glue_t *glue, input_t *input,
        layer_t **layers, output_t *output)
{
    init_wt_args_t iw_args;
    output_tree_dfs_aux_t *dfs_aux = NULL;

    ST_CHECK_PARAM(glue == NULL || glue->wts == NULL
            || input == NULL, -1);

    if (strcasecmp(glue->type, OUT_GLUE_NAME) != 0) {
        ST_ERROR("Not a out glue. [%s]", glue->type);
        return -1;
    }

    dfs_aux = output_tree_dfs_aux_create(output->tree);
    if (dfs_aux == NULL) {
        ST_ERROR("Failed to output_tree_dfs_aux_create.");
        goto ERR;
    }

    iw_args.wts = glue->wts;
    iw_args.in_len = glue->in_length;
    if (output_tree_dfs(output->tree, dfs_aux,
                output_tree_dfs_trav_init_wt, (void *)&iw_args) < 0) {
        ST_ERROR("Failed to output_tree_dfs.");
        goto ERR;
    }

    safe_output_tree_dfs_aux_destroy(dfs_aux);

    return 0;

ERR:
    safe_output_tree_dfs_aux_destroy(dfs_aux);
    return -1;

}
