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

typedef struct _egu_data_t_ {
    sp_mat_t word_buf;
} egu_data_t;

#define safe_egu_data_destroy(ptr) do {\
    if((ptr) != NULL) {\
        egu_data_destroy((egu_data_t *)ptr);\
        safe_st_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)

void egu_data_destroy(egu_data_t *data)
{
    if (data == NULL) {
        return;
    }

    sp_mat_destroy(&data->word_buf);
}

egu_data_t* egu_data_init(glue_updater_t *glue_updater)
{
    egu_data_t *data = NULL;

    data = (egu_data_t *)st_malloc(sizeof(egu_data_t));
    if (data == NULL) {
        ST_WARNING("Failed to st_malloc egu_data.");
        goto ERR;
    }
    memset(data, 0, sizeof(egu_data_t));

    return data;
ERR:
    safe_egu_data_destroy(data);
    return NULL;
}

void emb_glue_updater_destroy(glue_updater_t *glue_updater)
{
    if (glue_updater == NULL) {
        return;
    }

    safe_egu_data_destroy(glue_updater->extra);
}

int emb_glue_updater_init(glue_updater_t *glue_updater)
{
    glue_t *glue;

    ST_CHECK_PARAM(glue_updater == NULL, -1);

    glue = glue_updater->glue;

    if (strcasecmp(glue->type, EMB_GLUE_NAME) != 0) {
        ST_WARNING("Not a emb glue_updater. [%s]", glue->type);
        return -1;
    }

    glue_updater->extra = (void *)egu_data_init(glue_updater);
    if (glue_updater->extra == NULL) {
        ST_WARNING("Failed to egu_data_init.");
        goto ERR;
    }

    return 0;

ERR:
    safe_egu_data_destroy(glue_updater->extra);
    return -1;
}

int emb_glue_updater_setup(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, bool backprop)
{
    egu_data_t *data;

    ST_CHECK_PARAM(glue_updater == NULL, -1);

    data = (egu_data_t *)glue_updater->extra;

    if (backprop) {
        data->word_buf.fmt = SP_MAT_COO;
    }

    return 0;
}

int emb_glue_updater_prepare(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater /* unused */, egs_batch_t *batch)
{
    egu_data_t *data;
    size_t total_words;
    int b;

    ST_CHECK_PARAM(glue_updater == NULL, -1);

    data = (egu_data_t *)glue_updater->extra;

    if (data->word_buf.fmt != SP_MAT_UNSET) {
        total_words = 0;
        for (b = 0; b < batch->num_egs; b++) {
            total_words += batch->inputs[b].num_words;
        }

        if (total_words > 0 && batch->num_egs > 0) {
            if (sp_mat_resize(&data->word_buf, total_words,
                        batch->num_egs,
                        glue_updater->wt_updaters[0]->wt.num_rows) < 0) {
                ST_WARNING("Failed to sp_mat_resize.");
                return -1;
            }
        }

        if (sp_mat_clear(&data->word_buf) < 0) {
            ST_WARNING("Failed to sp_mat_clear.");
            return -1;
        }
    }

    return 0;
}

int emb_glue_updater_forward(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, egs_batch_t *batch,
        mat_t* in_ac /* unused */, mat_t *out_ac)
{
    glue_t *glue;
    input_t *input;
    emb_glue_data_t *data;
    mat_t *wt;

    size_t col;
    int b, w, i, j, pos;
    real_t scale, ac;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || batch == NULL || out_ac == NULL, -1);

    glue = glue_updater->glue;
    data = (emb_glue_data_t *)glue->extra;
    input = comp_updater->comp->input;
    wt = &glue_updater->wt_updaters[0]->wt;
    col = wt->num_cols;

    switch (data->combine) {
        case EC_SUM:
            for (b = 0; b < batch->num_egs; b++) {
                for (w = 0; w < batch->inputs[b].num_words; w++) {
                    scale = batch->inputs[b].weights[w];

                    j = batch->inputs[b].words[w];
                    if (glue_updater->keep_mask.num_rows > 0) {
                        scale /= glue_updater->keep_prob;
                        for (i = 0; i < col; i++) {
                            if (MAT_VAL(&glue_updater->keep_mask, b, i) == 1.0) {
                                MAT_VAL(out_ac, b, i) += scale * MAT_VAL(wt, j, i);
                            }
                        }
                    } else {
                        for (i = 0; i < col; i++) {
                            MAT_VAL(out_ac, b, i) += scale * MAT_VAL(wt, j, i);
                        }
                    }
                }
            }
            break;
        case EC_AVG:
            for (b = 0; b < batch->num_egs; b++) {
                for (i = 0; i < col; i++) {
                    if (glue_updater->keep_mask.num_rows > 0
                            && MAT_VAL(&glue_updater->keep_mask, b, i) == 0.0) {
                        continue;
                    }

                    ac = 0;
                    for (w = 0; w < batch->inputs[b].num_words; w++) {
                        scale = batch->inputs[b].weights[w];
                        j = batch->inputs[b].words[w];

                        ac += scale * MAT_VAL(wt, j, i);
                    }
                    if (glue_updater->keep_mask.num_rows > 0) {
                        ac /= glue_updater->keep_prob;
                    }
                    MAT_VAL(out_ac, b, i) += ac / input->n_ctx;
                }
            }
            break;
        case EC_CONCAT:
            for (b = 0; b < batch->num_egs; b++) {
                pos = 0;
                for (w = 0; w < batch->inputs[b].num_words; w++) {
                    scale = batch->inputs[b].weights[w];

                    j = batch->inputs[b].words[w];

                    while (pos < input->n_ctx) {
                        if (input->context[pos].i == batch->inputs[b].positions[w]) {
                            break;
                        }
                        pos++;
                    }

                    if (glue_updater->keep_mask.num_rows > 0) {
                        scale /= glue_updater->keep_prob;
                        for (i = pos * col; i < (pos + 1) * col; i++) {
                            if (MAT_VAL(&glue_updater->keep_mask, b, i) == 1.0) {
                                MAT_VAL(out_ac, b, i) += scale * MAT_VAL(wt, j, i);
                            }
                        }
                    } else {
                        for (i = pos * col; i < (pos + 1) * col; i++) {
                            MAT_VAL(out_ac, b, i) += scale * MAT_VAL(wt, j, i);
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
        mat_t *in_ac /* unused */, mat_t *out_er, mat_t *in_er /* unused */)
{
    glue_t *glue;
    emb_glue_data_t *data;
    egu_data_t *egu_data;
    input_t *input;

    mat_t er = {0};
    mat_t part_er = {0};

    int b, i, j;
    size_t col;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || batch == NULL, -1);

    glue = glue_updater->glue;
    data = (emb_glue_data_t *)glue->extra;
    egu_data = (egu_data_t *)glue_updater->extra;
    input = comp_updater->comp->input;
    col = glue_updater->wt_updaters[0]->wt.num_cols;

    if (glue_updater->keep_mask.num_rows > 0) {
        if (mat_mul_elems(out_er, &glue_updater->keep_mask,
                    &glue_updater->dropout_val) < 0) {
            ST_WARNING("Failed to mat_mul_elems.");
            return -1;
        }
        mat_assign(&er, &glue_updater->dropout_val);
    } else {
        mat_assign(&er, out_er);
    }

    sp_mat_clear(&egu_data->word_buf);

    if (data->combine == EC_CONCAT) {
        for (j = 0; j < input->n_ctx; j++) {
            for (b = 0; b < batch->num_egs; b++) {
                i = min(j, batch->inputs[b].num_words);
                while (i >= 0) {
                    if (input->context[j].i == batch->inputs[b].positions[i]) {
                        break;
                    }
                    --i;
                }

                if (i >= 0) {
                    if (sp_mat_coo_add(&egu_data->word_buf, b,
                                batch->inputs[b].words[i],
                                batch->inputs[b].weights[i]) < 0) {
                        ST_WARNING("Failed to sp_mat_coo_add");
                        return -1;
                    }
                }
            }
            if (mat_submat(&er, 0, 0, j * col, col, &part_er) < 0) {
                ST_WARNING("Failed to mat_submat.");
                return -1;
            }
            if (wt_update(glue_updater->wt_updaters[0],
                        &part_er, 1.0, NULL, 1.0, NULL,
                        &egu_data->word_buf) < 0) {
                ST_WARNING("Failed to wt_update.");
                return -1;
            }
        }
    } else {
        for (b = 0; b < batch->num_egs; b++) {
            for (i = 0; i < batch->inputs[b].num_words; i++) {
                if (sp_mat_coo_add(&egu_data->word_buf, b,
                            batch->inputs[b].words[i],
                            batch->inputs[b].weights[i]) < 0) {
                    ST_WARNING("Failed to sp_mat_coo_add");
                    return -1;
                }
            }
        }

        if (wt_update(glue_updater->wt_updaters[0],
                    &er, 1.0, NULL, 1.0, NULL, &egu_data->word_buf) < 0) {
            ST_WARNING("Failed to wt_update.");
            return -1;
        }
    }

    return 0;
}
