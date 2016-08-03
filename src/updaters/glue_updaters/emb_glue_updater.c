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

#include "output.h"
#include "../../glues/emb_glue.h"
#include "../component_updater.h"

#include "emb_glue_updater.h"

void emb_glue_updater_destroy(glue_updater_t *glue_updater)
{
    if (glue_updater == NULL) {
        return;
    }
}

int emb_glue_updater_init(glue_updater_t *glue_updater)
{
    glue_t *glue;

    ST_CHECK_PARAM(glue_updater == NULL, -1);

    glue = glue_updater->glue;

    if (strcasecmp(glue_updater->glue->type, EMB_GLUE_NAME) != 0) {
        ST_WARNING("Not a emb glue_updater. [%s]",
                glue_updater->glue->type);
        return -1;
    }

    glue_updater->wt_updater = wt_updater_create(&glue->param, glue->wt->mat,
            glue->wt->row, glue->wt->col, WT_UT_ONE_SHOT);
    if (glue_updater->wt_updater == NULL) {
        ST_WARNING("Failed to wt_updater_create.");
        goto ERR;
    }

    return 0;

ERR:
    wt_updater_destroy(glue_updater->wt_updater);
    return -1;
}

int emb_glue_updater_forward(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, int *words, int n_word, int tgt_pos)
{
    glue_t *glue;
    input_t *input;
    layer_updater_t *out_layer_updater;
    real_t *wt;

    int a, b, i, j, col;
    real_t scale, ac;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || words == NULL, -1);

    glue = glue_updater->glue;
    input = comp_updater->comp->input;
    out_layer_updater = comp_updater->layer_updaters[glue->out_layers[0]];
    wt = glue_updater->wt_updater->wt;
    col = glue->wt->col;

    switch (input->combine) {
        case IC_SUM:
            for (a = 0; a < input->n_ctx; a++) {
                i = tgt_pos + input->context[a].i;
                if (i < 0 || i >= n_word) {
                    continue;
                }
                scale = input->context[a].w * glue->in_scales[0]
                    * glue->out_scales[0];
                j = words[i] * col;
                for (b = 0; b < col; b++, j++) {
                    out_layer_updater->ac[b] += scale * wt[j];
                }
            }
            break;
        case IC_AVG:
            for (b = 0; b < col; b++) {
                ac = 0;
                for (a = 0; a < input->n_ctx; a++) {
                    i = tgt_pos + input->context[a].i;
                    if (i < 0 || i >= n_word) {
                        continue;
                    }
                    scale = input->context[a].w * glue->in_scales[0]
                        * glue->out_scales[0];
                    j = words[i] * col + b;
                    ac += scale * wt[j];
                }
                out_layer_updater->ac[b] += ac / input->n_ctx;
            }
            break;
        case IC_CONCAT:
            for (a = 0; a < input->n_ctx; a++) {
                i = tgt_pos + input->context[a].i;
                if (i < 0 || i >= n_word) {
                    continue;
                }
                scale = input->context[a].w * glue->in_scales[0]
                    * glue->out_scales[0];
                j = words[i] * col;
                for (b = a * col; b < (a + 1) * col; b++, j++) {
                    out_layer_updater->ac[b] += scale * wt[j];
                }
            }
            break;
        default:
            ST_WARNING("Unknown combine[%d]", input->combine);
            return -1;
    }

    return 0;
}

int emb_glue_updater_backprop(glue_updater_t *glue_updater, count_t n_step,
        comp_updater_t *comp_updater, int *words, int n_word, int tgt_pos)
{
    glue_t *glue;
    input_t *input;
    layer_updater_t *out_layer_updater;

    st_wt_int_t in_idx;
    int a, i;
    real_t out_scale;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || words == NULL, -1);

    glue = glue_updater->glue;
    input = comp_updater->comp->input;
    out_layer_updater = comp_updater->layer_updaters[glue->out_layers[0]];
    out_scale = glue->out_scales[0];

    if (input->combine == IC_CONCAT) {
        for (a = 0; a < input->n_ctx; a++) {
            i = tgt_pos + input->context[a].i;
            if (i < 0 || i >= n_word) {
                continue;
            }
            in_idx.w = input->context[a].w;
            in_idx.i = words[i];
            if (wt_update(glue_updater->wt_updater, n_step, NULL, -1,
                        out_layer_updater->er + a * glue->wt->col,
                        out_scale, NULL, glue->in_scales[0], &in_idx) < 0) {
                ST_WARNING("Failed to wt_update.");
                return -1;
            }
        }
    } else {
        if (input->combine == IC_AVG) {
            out_scale = glue->out_scales[0] / input->n_ctx;
        }
        for (a = 0; a < input->n_ctx; a++) {
            i = tgt_pos + input->context[a].i;
            if (i < 0 || i >= n_word) {
                continue;
            }
            in_idx.w = input->context[a].w;
            in_idx.i = words[i];
            if (wt_update(glue_updater->wt_updater, n_step, NULL, -1,
                        out_layer_updater->er, out_scale,
                        NULL, glue->in_scales[0], &in_idx) < 0) {
                ST_WARNING("Failed to wt_update.");
                return -1;
            }
        }
    }

    if (wt_flush(glue_updater->wt_updater, n_step) < 0) {
        ST_WARNING("Failed to wt_flush.");
        return -1;
    }

    return 0;
}
