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

    switch (input->combine) {
        case IC_SUM:
            for (a = 0; a < input->n_ctx; a++) {
                pos = input_sent->tgt_pos + input->context[a].i;
                if (pos < -1 || pos >= input_sent->n_word) {
                    continue;
                }
                scale = input->context[a].w;

                switch (data->index_method) {
                    case EI_WORD:
                        if (pos == -1) { // <s>
                            wt = glue_updater->wt_updater->wt;
                            j = 0 * col;
                        } else if (input_sent->words[pos] == ANY_ID) {
                            wt = glue->wildcard_repr;
                            j = 0;
                        } else {
                            wt = glue_updater->wt_updater->wt;
                            j = input_sent->words[pos] * col;
                        }
                        break;
                    case EI_POS:
                        wt = glue_updater->wt_updater->wt;
                        if (pos + 1 >= data->num_vecs) {
                            j = (data->num_vecs - 1) * col;
                        } else {
                            j = (pos + 1) * col;
                        }
                        break;
                    default:
                        ST_WARNING("Unknown index method[%d].",
                                (int)data->index_method);
                        return -1;
                }

                for (b = 0; b < col; b++, j++) {
                    out_ac[b] += scale * wt[j];
                }
            }
            break;
        case IC_AVG:
            for (b = 0; b < col; b++) {
                ac = 0;
                for (a = 0; a < input->n_ctx; a++) {
                    pos = input_sent->tgt_pos + input->context[a].i;
                    if (pos < -1 || pos >= input_sent->n_word) {
                        continue;
                    }
                    scale = input->context[a].w;

                    switch (data->index_method) {
                        case EI_WORD:
                            if (pos == -1) { // <s>
                                wt = glue_updater->wt_updater->wt;
                                j = 0 * col + b;
                            } else if (input_sent->words[pos] == ANY_ID) {
                                wt = glue->wildcard_repr;
                                j = b;
                            } else {
                                wt = glue_updater->wt_updater->wt;
                                j = input_sent->words[pos] * col + b;
                            }
                            break;
                        case EI_POS:
                            wt = glue_updater->wt_updater->wt;
                            if (pos + 1 >= data->num_vecs) {
                                j = (data->num_vecs - 1) * col + b;
                            } else {
                                j = (pos + 1) * col + b;
                            }
                            break;
                        default:
                            ST_WARNING("Unknown index method[%d].",
                                    (int)data->index_method);
                            return -1;
                    }

                    ac += scale * wt[j];
                }
                out_ac[b] += ac / input->n_ctx;
            }
            break;
        case IC_CONCAT:
            for (a = 0; a < input->n_ctx; a++) {
                pos = input_sent->tgt_pos + input->context[a].i;
                if (pos < -1 || pos >= input_sent->n_word) {
                    continue;
                }
                scale = input->context[a].w;

                switch (data->index_method) {
                    case EI_WORD:
                        if (pos == -1) { // <s>
                            wt = glue_updater->wt_updater->wt;
                            j = 0 * col;
                        } else if (input_sent->words[pos] == ANY_ID) {
                            wt = glue->wildcard_repr;
                            j = 0;
                        } else {
                            wt = glue_updater->wt_updater->wt;
                            j = input_sent->words[pos] * col;
                        }
                        break;
                    case EI_POS:
                        wt = glue_updater->wt_updater->wt;
                        if (pos + 1 >= data->num_vecs) {
                            j = (data->num_vecs - 1) * col;
                        } else {
                            j = (pos + 1) * col;
                        }
                        break;
                    default:
                        ST_WARNING("Unknown index method[%d].",
                                (int)data->index_method);
                        return -1;
                }

                for (b = a * col; b < (a + 1) * col; b++, j++) {
                    out_ac[b] += scale * wt[j];
                }
            }
            break;
        default:
            ST_WARNING("Unknown combine[%d]", (int)input->combine);
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

    st_wt_int_t in_idx;
    int pos;
    int a;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || input_sent == NULL, -1);

    glue = glue_updater->glue;
    data = (emb_glue_data_t *)glue->extra;
    input = comp_updater->comp->input;

    // backprop words should not contain <any>

    if (input->combine == IC_CONCAT) {
        for (a = 0; a < input->n_ctx; a++) {
            pos = input_sent->tgt_pos + input->context[a].i;
            if (pos < -1 || pos >= input_sent->n_word) {
                continue;
            }
            in_idx.w = input->context[a].w;
            switch (data->index_method) {
                case EI_WORD:
                    if (pos == -1) { // <s>
                        in_idx.i = 0;
                    } else {
                        in_idx.i = input_sent->words[pos];
                    }
                    break;
                case EI_POS:
                    if (pos + 1 >= data->num_vecs) {
                        in_idx.i = data->num_vecs - 1;
                    } else {
                        in_idx.i = pos + 1;
                    }
                    break;
                default:
                    ST_WARNING("Unknown index method[%d].",
                            (int)data->index_method);
                    return -1;
            }
            if (wt_update(glue_updater->wt_updater, NULL, -1,
                        out_er + a * glue->wt->col,
                        1.0, NULL, 1.0, &in_idx) < 0) {
                ST_WARNING("Failed to wt_update.");
                return -1;
            }
        }
    } else {
        for (a = 0; a < input->n_ctx; a++) {
            pos = input_sent->tgt_pos + input->context[a].i;
            if (pos < -1 || pos >= input_sent->n_word) {
                continue;
            }
            in_idx.w = input->context[a].w;
            switch (data->index_method) {
                case EI_WORD:
                    if (pos == -1) { // <s>
                        in_idx.i = 0;
                    } else {
                        in_idx.i = input_sent->words[pos];
                    }
                    break;
                case EI_POS:
                    if (pos + 1 >= data->num_vecs) {
                        in_idx.i = data->num_vecs - 1;
                    } else {
                        in_idx.i = pos + 1;
                    }
                    break;
                default:
                    ST_WARNING("Unknown index method[%d].",
                            (int)data->index_method);
                    return -1;
            }
            if (wt_update(glue_updater->wt_updater, NULL, -1,
                        out_er, 1.0, NULL, 1.0, &in_idx) < 0) {
                ST_WARNING("Failed to wt_update.");
                return -1;
            }
        }
    }

    return 0;
}
