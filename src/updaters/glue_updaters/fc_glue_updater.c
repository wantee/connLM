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
#include <stutils/st_log.h>

#include "../../glues/fc_glue.h"
#include "../component_updater.h"

#include "fc_glue_updater.h"

int fc_glue_updater_forward(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, egs_batch_t* batch,
        mat_t *in_ac, mat_t *out_ac)
{
    wt_updater_t *wt_updater;
    size_t i, j;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || in_ac == NULL || out_ac == NULL, -1);

    wt_updater = glue_updater->wt_updaters[0];

    if (add_mat_mat(1.0, in_ac, MT_NoTrans,
                &wt_updater->wt, MT_Trans, 1.0, out_ac) < 0) {
        ST_WARNING("Failed to add_mat_mat.");
        return -1;
    }

    if (wt_updater->bias.size > 0) {
        for (i = 0; i < out_ac->num_rows; i++) {
            for (j = 0; j < out_ac->num_cols; j++) {
                MAT_VAL(out_ac, i, j) += VEC_VAL(&wt_updater->bias, j);
            }
        }
    }

    return 0;
}

int fc_glue_updater_backprop(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, egs_batch_t *batch,
        mat_t *in_ac, mat_t *out_er, mat_t *in_er)
{
    wt_updater_t *wt_updater;

    ST_CHECK_PARAM(glue_updater == NULL || out_er == NULL, -1);

    wt_updater = glue_updater->wt_updaters[0];

    if (in_er != NULL && in_er->num_rows > 0) {
        if (propagate_error(&wt_updater->wt, out_er, 1.0,
                    wt_updater->param.er_cutoff, in_er) < 0) {
            ST_WARNING("Failed to propagate_error.");
            return -1;
        }
    }

    if (in_ac != NULL && in_ac->num_rows > 0) {
        if (wt_update(wt_updater, out_er, 1.0,
                    in_ac, 1.0, NULL, NULL) < 0) {
            ST_WARNING("Failed to wt_update.");
            return -1;
        }
    }

    return 0;
}
