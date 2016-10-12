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
        comp_updater_t *comp_updater, sent_t *input_sent, real_t *in_ac)
{
    glue_t *glue;
    layer_updater_t *out_layer_updater;
    wt_updater_t *wt_updater;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL, -1);

    glue = glue_updater->glue;
    out_layer_updater = comp_updater->layer_updaters[glue->out_layer];
    wt_updater = glue_updater->wt_updater;

    matXvec(out_layer_updater->ac, wt_updater->wt, in_ac,
            wt_updater->row, wt_updater->col, 1.0);

    return 0;
}

int fc_glue_updater_backprop(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, sent_t *input_sent,
        real_t *in_ac, real_t *out_er, real_t *in_er)
{
    wt_updater_t *wt_updater;

    ST_CHECK_PARAM(glue_updater == NULL || out_er == NULL, -1);

    wt_updater = glue_updater->wt_updater;

    if (in_er != NULL) {
        propagate_error(in_er, out_er,
                wt_updater->wt, wt_updater->col, wt_updater->row,
                wt_updater->param.er_cutoff, 1.0);
    }

    if (in_ac != NULL) {
        if (wt_update(wt_updater, NULL, -1, out_er, 1.0,
                    in_ac, 1.0, NULL) < 0) {
            ST_WARNING("Failed to wt_update.");
            return -1;
        }
    }

    return 0;
}
