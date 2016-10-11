/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Wang Jian
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
#include <stutils/st_mem.h>

#include "bptt_updater.h"

void bptt_updater_destroy(bptt_updater_t *bptt_updater)
{
    int i;

    if (bptt_updater == NULL) {
        return;
    }

    if (bptt_updater->ac_bptt != NULL) {
        for (i = 1; i <= bptt_updater->num_glue; i++) {
            safe_st_aligned_free(bptt_updater->ac_bptt[i]);
        }
        safe_free(bptt_updater->ac_bptt);
    }

    if (bptt_updater->er_bptt != NULL) {
        for (i = 1; i <= bptt_updater->num_glue; i++) {
            safe_st_aligned_free(bptt_updater->er_bptt[i]);
        }
        safe_free(bptt_updater->er_bptt);
    }

    if (bptt_updater->wt_updaters != NULL) {
        for (i = 1; i <= bptt_updater->num_glue; i++) {
            safe_wt_updater_destroy(bptt_updater->wt_updaters[i]);
        }
        safe_free(bptt_updater->wt_updaters);
    }
}

bptt_updater_t* bptt_updater_create(component_t *comp, int cycle_id)
{
    param_t param;
    bptt_updater_t *bptt_updater = NULL;
    glue_t *glue;
    int g, i, bptt, bptt_delay, sz;

    ST_CHECK_PARAM(comp == NULL || cycle_id < 0
            || cycle_id >= comp->num_glue_cycle, NULL);

    bptt_updater = (bptt_updater_t *)malloc(sizeof(bptt_updater_t));
    if (bptt_updater == NULL) {
        ST_WARNING("Failed to malloc bptt_updater.");
        goto ERR;
    }
    memset(bptt_updater, 0, sizeof(bptt_updater_t));

    g = comp->glue_cycles[cycle_id][1];
    bptt_updater->bptt_opt = comp->glues[g]->bptt_opt;
    bptt = bptt_updater->bptt_opt.bptt;
    bptt_delay = bptt_updater->bptt_opt.bptt_delay;
    param = comp->glues[g]->param;

    bptt_updater->num_glue = comp->glue_cycles[cycle_id][1];

    sz = sizeof(real_t *) * (bptt_updater->num_glue + 1);
    bptt_updater->ac_bptt = (real_t **)malloc(sz);
    if (bptt_updater->ac_bptt == NULL) {
        ST_WARNING("Failed to malloc ac_bptt.");
        goto ERR;
    }
    memset(bptt_updater->ac_bptt, 0, sz);

    bptt_updater->er_bptt = (real_t **)malloc(sz);
    if (bptt_updater->er_bptt == NULL) {
        ST_WARNING("Failed to malloc er_bptt.");
        goto ERR;
    }
    memset(bptt_updater->er_bptt, 0, sz);

    sz = sizeof(wt_updater_t *) * (bptt_updater->num_glue + 1);
    bptt_updater->wt_updaters = (wt_updater_t **)malloc(sz);
    if (bptt_updater->wt_updaters == NULL) {
        ST_WARNING("Failed to malloc wt_updaters.");
        goto ERR;
    }
    memset(bptt_updater->wt_updaters, 0, sz);

    // NOTE: To correctly BPTT, _BATCH_UPDATE_ must be defined
    // we must make sure mini_batch is at least 1,
    // so that the bptt could be batch updated.
    if (param.mini_batch <= 0) {
        param.mini_batch = 1;
    }

    for (i = 1; i <= bptt_updater->num_glue; i++) {
        g = comp->glue_cycles[cycle_id][i];
        glue = comp->glues[g];
        sz = sizeof(real_t) * comp->layers[glue->in_layer]->size
            * (bptt + bptt_delay);
        bptt_updater->ac_bptt[i] = st_aligned_malloc(sz, ALIGN_SIZE);
        if (bptt_updater->ac_bptt[i] == NULL) {
            ST_WARNING("Failed to st_aligned_malloc ac_bptt[%d].", i);
            goto ERR;
        }
        memset(bptt_updater->ac_bptt[i], 0, sz);

        sz = sizeof(real_t) * comp->layers[glue->out_layer]->size
            * bptt_delay;
        bptt_updater->er_bptt[i] = st_aligned_malloc(sz, ALIGN_SIZE);
        if (bptt_updater->er_bptt[i] == NULL) {
            ST_WARNING("Failed to st_aligned_malloc er_bptt[%d].", i);
            goto ERR;
        }
        memset(bptt_updater->er_bptt[i], 0, sz);

        // FIXME: when sync_size > 0, the wt_updaters for the same glue
        // in the cycle could be *Non-Synchronized*, which may lead to
        // worse performance.
        bptt_updater->wt_updaters[i] = glue_init_wt_updater(glue, &param);
        if (bptt_updater->wt_updaters[i] == NULL) {
            ST_WARNING("Failed to init_wt_updater[%s].", glue->name);
            goto ERR;
        }
    }

    return bptt_updater;

ERR:
    safe_bptt_updater_destroy(bptt_updater);
    return NULL;
}

int bptt_updater_reset(bptt_updater_t *bptt_updater)
{
    ST_CHECK_PARAM(bptt_updater == NULL, -1);

    bptt_updater->num_ac_bptt = 0;
    bptt_updater->num_er_bptt = 0;

    return 0;
}
