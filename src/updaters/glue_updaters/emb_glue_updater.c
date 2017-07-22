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

int emb_glue_updater_forward(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, egs_batch_t *batch,
        mat_t* in_ac, mat_t *out_ac)
{
    glue_t *glue;
    input_t *input;
    real_t *wt;
    emb_glue_data_t *data;

    int b, w, i, j, col, pos;
    real_t scale, ac;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || batch == NULL || out_ac == NULL, -1);

    glue = glue_updater->glue;
    data = (emb_glue_data_t *)glue->extra;
    input = comp_updater->comp->input;
    col = glue->wt->col;
    wt = glue_updater->wt_updater->wt;

    switch (data->combine) {
        case EC_SUM:
            for (b = 0; b < batch->num_egs; b++) {
                for (w = 0; w < batch->inputs->num_words; w++) {
                    scale = batch->inputs->weights[w];

                    j = batch->inputs->words[w] * col;

                    if (glue_updater->keep_mask.vals != NULL) {
                        for (i = 0; i < col; i++, j++) {
                            if (glue_updater->keep_mask.vals[i] == 1.0) {
                                MAT_VAL(out_ac, b, i) += scale * wt[j]
                                    / glue_updater->keep_prob;
                            }
                        }
                    } else {
                        for (i = 0; i < col; i++, j++) {
                            MAT_VAL(out_ac, b, i) += scale * wt[j];
                        }
                    }
                }
            }
            break;
        case EC_AVG:
            for (b = 0; b < batch->num_egs; b++) {
                for (i = 0; i < col; i++) {
                    if (glue_updater->keep_mask.vals != NULL
                            && glue_updater->keep_mask.vals[i] == 0.0) {
                        continue;
                    }

                    ac = 0;
                    for (w = 0; w < batch->inputs->num_words; w++) {
                        scale = batch->inputs->weights[w];
                        j = batch->inputs->words[w] * col + i;

                        ac += scale * wt[j];
                    }
                    if (glue_updater->keep_mask.vals != NULL) {
                        MAT_VAL(out_ac, b, i) += ac / input->n_ctx
                            / glue_updater->keep_prob;
                    } else {
                        MAT_VAL(out_ac, b, i) += ac / input->n_ctx;
                    }
                }
            }
            break;
        case EC_CONCAT:
            for (b = 0; b < batch->num_egs; b++) {
                pos = 0;
                for (w = 0; w < batch->inputs->num_words; w++) {
                    scale = batch->inputs->weights[w];

                    j = batch->inputs->words[w] * col;

                    while (pos < input->n_ctx) {
                        if (input->context[pos].i == batch->inputs->positions[w]) {
                            break;
                        }
                        pos++;
                    }

                    if (glue_updater->keep_mask.vals != NULL) {
                        for (i = pos * col; i < (pos + 1) * col; i++, j++) {
                            if (glue_updater->keep_mask.vals[i] == 1.0) {
                                MAT_VAL(out_ac, b, i) += scale * wt[j]
                                    / glue_updater->keep_prob;
                            }
                        }
                    } else {
                        for (i = pos * col; i < (pos + 1) * col; i++, j++) {
                            MAT_VAL(out_ac, b, i) += scale * wt[j];
                        }
                    }
                }
            }
            break;
        default:
            ST_WARNING("Unknown combine[%d]", (int)data->combine);
            return -1;
    }

    return 0;
}

int emb_glue_updater_backprop(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, egs_batch_t *batch,
        mat_t *in_ac, mat_t *out_er, mat_t *in_er)
{
    glue_t *glue;
    emb_glue_data_t *data;
    input_t *input;

    real_t *er;

    st_wt_int_t in_idx;
    int b, i, j;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || batch == NULL, -1);

    glue = glue_updater->glue;
    data = (emb_glue_data_t *)glue->extra;
    input = comp_updater->comp->input;

    if (glue_updater->keep_mask.vals != NULL) {
        er = glue_updater->dropout_val;
        for (i = 0; i < glue_updater->keep_mask_len; i++) {
            if (glue_updater->keep_mask.vals[i] == 1.0) {
                er[i] = out_er[i];
            } else {
                er[i] = 0.0;
            }
        }
    } else {
        er = out_er;
    }

    if (data->combine == EC_CONCAT) {
        for (b = 0; b < batch->num_egs; b++) {
            j = 0;
            for (i = 0; i < batch->inputs->num_words; i++) {
                while (j < input->n_ctx) {
                    if (input->context[j] == batch->inputs->positions[i]) {
                        break;
                    }
                    j++;
                }

                in_idx.w = batch->inputs->weights[i];
                in_idx.i = batch->inputs->words[i];
                if (wt_update(glue_updater->wt_updater, NULL, -1,
                            er + j * glue->wt->col,
                            1.0, NULL, 1.0, &in_idx) < 0) {
                    ST_WARNING("Failed to wt_update.");
                    return -1;
                }
            }
        }
    } else {
        for (b = 0; b < batch->num_egs; b++) {
            for (i = 0; i < batch->inputs->num_words; i++) {
                in_idx.w = batch->inputs->weights[i];
                in_idx.i = batch->inputs->words[i];
                if (wt_update(glue_updater->wt_updater, NULL, -1,
                            er, 1.0, NULL, 1.0, &in_idx) < 0) {
                    ST_WARNING("Failed to wt_update.");
                    return -1;
                }
            }
        }
    }

    return 0;
}
