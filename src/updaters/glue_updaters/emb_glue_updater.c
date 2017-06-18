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
        comp_updater_t *comp_updater, sent_t *input_sent,
        real_t* in_ac, real_t *out_ac)
{
    glue_t *glue;
    input_t *input;
    real_t *wt;
    emb_glue_data_t *data;

    int pos;
    int a, b, j, col;
    real_t scale, ac;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || input_sent == NULL || out_ac == NULL, -1);

    glue = glue_updater->glue;
    data = (emb_glue_data_t *)glue->extra;
    input = comp_updater->comp->input;
    col = glue->wt->col;

    switch (data->combine) {
        case EC_SUM:
            for (a = 0; a < input->n_ctx; a++) {
                pos = input_sent->tgt_pos + input->context[a].i;
                if (pos < 0 || pos >= input_sent->n_word) {
                    continue;
                }
                scale = input->context[a].w;

                wt = glue_updater->wt_updater->wt;
                j = input_sent->words[pos] * col;

                if (glue_updater->keep_mask != NULL) {
                    for (b = 0; b < col; b++, j++) {
                        if (glue_updater->keep_mask[b]) {
                            out_ac[b] += scale * wt[j] / glue_updater->keep_prob;
                        }
                    }
                } else {
                    for (b = 0; b < col; b++, j++) {
                        out_ac[b] += scale * wt[j];
                    }
                }
            }
            break;
        case EC_AVG:
            for (b = 0; b < col; b++) {
                if (glue_updater->keep_mask != NULL
                        && ! glue_updater->keep_mask[b]) {
                    continue;
                }

                ac = 0;
                for (a = 0; a < input->n_ctx; a++) {
                    pos = input_sent->tgt_pos + input->context[a].i;
                    if (pos < 0 || pos >= input_sent->n_word) {
                        continue;
                    }
                    scale = input->context[a].w;

                    wt = glue_updater->wt_updater->wt;
                    j = input_sent->words[pos] * col + b;

                    ac += scale * wt[j];
                }
                if (glue_updater->keep_mask != NULL) {
                    out_ac[b] += ac / input->n_ctx / glue_updater->keep_prob;
                } else {
                    out_ac[b] += ac / input->n_ctx;
                }
            }
            break;
        case EC_CONCAT:
            for (a = 0; a < input->n_ctx; a++) {
                pos = input_sent->tgt_pos + input->context[a].i;
                if (pos < 0 || pos >= input_sent->n_word) {
                    continue;
                }
                scale = input->context[a].w;

                wt = glue_updater->wt_updater->wt;
                j = input_sent->words[pos] * col;

                if (glue_updater->keep_mask != NULL) {
                    for (b = a * col; b < (a + 1) * col; b++, j++) {
                        if (glue_updater->keep_mask[b]) {
                            out_ac[b] += scale * wt[j] / glue_updater->keep_prob;
                        }
                    }
                } else {
                    for (b = a * col; b < (a + 1) * col; b++, j++) {
                        out_ac[b] += scale * wt[j];
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
        comp_updater_t *comp_updater, sent_t *input_sent,
        real_t *in_ac, real_t *out_er, real_t *in_er)
{
    glue_t *glue;
    emb_glue_data_t *data;
    input_t *input;

    real_t *er;

    st_wt_int_t in_idx;
    int pos;
    int a;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || input_sent == NULL, -1);

    glue = glue_updater->glue;
    data = (emb_glue_data_t *)glue->extra;
    input = comp_updater->comp->input;

    if (glue_updater->keep_mask != NULL) {
        er = glue_updater->dropout_val;
        for (a = 0; a < glue_updater->keep_mask_len; a++) {
            if (glue_updater->keep_mask[a]) {
                er[a] = out_er[a];
            } else {
                er[a] = 0.0;
            }
        }
    } else {
        er = out_er;
    }

    if (data->combine == EC_CONCAT) {
        for (a = 0; a < input->n_ctx; a++) {
            pos = input_sent->tgt_pos + input->context[a].i;
            if (pos < 0 || pos >= input_sent->n_word) {
                continue;
            }
            in_idx.w = input->context[a].w;
            in_idx.i = input_sent->words[pos];
            if (wt_update(glue_updater->wt_updater, NULL, -1,
                        er + a * glue->wt->col,
                        1.0, NULL, 1.0, &in_idx) < 0) {
                ST_WARNING("Failed to wt_update.");
                return -1;
            }
        }
    } else {
        for (a = 0; a < input->n_ctx; a++) {
            pos = input_sent->tgt_pos + input->context[a].i;
            if (pos < 0 || pos >= input_sent->n_word) {
                continue;
            }
            in_idx.w = input->context[a].w;
            in_idx.i = input_sent->words[pos];
            if (wt_update(glue_updater->wt_updater, NULL, -1,
                        er, 1.0, NULL, 1.0, &in_idx) < 0) {
                ST_WARNING("Failed to wt_update.");
                return -1;
            }
        }
    }

    return 0;
}
