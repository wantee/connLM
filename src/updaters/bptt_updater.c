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

#include "param.h"
#include "bptt_updater.h"

void bptt_updater_destroy(bptt_updater_t *bptt_updater)
{
    int i;

    if (bptt_updater == NULL) {
        return;
    }

    if (bptt_updater->ac_bptts != NULL) {
        for (i = 1; i <= bptt_updater->num_glue; i++) {
            mat_destroy(bptt_updater->ac_bptts + i);
        }
        safe_st_free(bptt_updater->ac_bptts);
    }

    if (bptt_updater->er_bptts != NULL) {
        for (i = 1; i <= bptt_updater->num_glue; i++) {
            mat_destroy(bptt_updater->er_bptts + i);
        }
        safe_st_free(bptt_updater->er_bptts);
    }

    if (bptt_updater->in_acs != NULL) {
        for (i = 1; i <= bptt_updater->num_glue; i++) {
            mat_destroy(bptt_updater->in_acs + i);
        }
        safe_st_free(bptt_updater->in_acs);
    }

    if (bptt_updater->out_ers != NULL) {
        for (i = 1; i <= bptt_updater->num_glue; i++) {
            mat_destroy(bptt_updater->out_ers + i);
        }
        safe_st_free(bptt_updater->out_ers);
    }

    if (bptt_updater->cutoffs != NULL) {
        for (i = 0; i < bptt_updater->bptt_opt.bptt; i++) {
            ivec_destroy(bptt_updater->cutoffs + i);
        }
        safe_st_free(bptt_updater->cutoffs);
    }

    if (bptt_updater->cutoff_acs != NULL) {
        for (i = 0; i < bptt_updater->bptt_opt.bptt; i++) {
            mat_destroy(bptt_updater->cutoff_acs + i);
        }
        safe_st_free(bptt_updater->cutoff_acs);
    }

    safe_st_free(bptt_updater->wt_updaters);
}

bptt_updater_t* bptt_updater_create(component_t *comp, int cycle_id,
        glue_updater_t **glue_updaters)
{
    bptt_updater_t *bptt_updater = NULL;
    int g, i, sz;

    ST_CHECK_PARAM(comp == NULL || cycle_id < 0
            || cycle_id >= comp->num_glue_cycle, NULL);

    bptt_updater = (bptt_updater_t *)st_malloc(sizeof(bptt_updater_t));
    if (bptt_updater == NULL) {
        ST_WARNING("Failed to st_malloc bptt_updater.");
        goto ERR;
    }
    memset(bptt_updater, 0, sizeof(bptt_updater_t));

    g = comp->glue_cycles[cycle_id][1];
    bptt_updater->bptt_opt = comp->glues[g]->bptt_opt;

    bptt_updater->num_glue = comp->glue_cycles[cycle_id][0];

    sz = sizeof(mat_t) * (bptt_updater->num_glue + 1);
    bptt_updater->ac_bptts = (mat_t *)st_malloc(sz);
    if (bptt_updater->ac_bptts == NULL) {
        ST_WARNING("Failed to st_malloc ac_bptts.");
        goto ERR;
    }
    memset(bptt_updater->ac_bptts, 0, sz);

    bptt_updater->er_bptts = (mat_t *)st_malloc(sz);
    if (bptt_updater->er_bptts == NULL) {
        ST_WARNING("Failed to st_malloc er_bptts.");
        goto ERR;
    }
    memset(bptt_updater->er_bptts, 0, sz);

    bptt_updater->in_acs = (mat_t *)st_malloc(sz);
    if (bptt_updater->in_acs == NULL) {
        ST_WARNING("Failed to st_malloc in_acs.");
        goto ERR;
    }
    memset(bptt_updater->in_acs, 0, sz);

    bptt_updater->out_ers = (mat_t *)st_malloc(sz);
    if (bptt_updater->out_ers == NULL) {
        ST_WARNING("Failed to st_malloc out_ers.");
        goto ERR;
    }
    memset(bptt_updater->out_ers, 0, sz);

    sz = sizeof(wt_updater_t *) * (bptt_updater->num_glue + 1);
    bptt_updater->wt_updaters = (wt_updater_t **)st_malloc(sz);
    if (bptt_updater->wt_updaters == NULL) {
        ST_WARNING("Failed to st_malloc wt_updaters.");
        goto ERR;
    }
    memset(bptt_updater->wt_updaters, 0, sz);

    for (i = 1; i <= bptt_updater->num_glue; i++) {
        g = comp->glue_cycles[cycle_id][i];
        bptt_updater->wt_updaters[i] = glue_updaters[g]->wt_updaters[0];
    }

    sz = sizeof(ivec_t) * bptt_updater->bptt_opt.bptt;
    bptt_updater->cutoffs = (ivec_t *)st_malloc(sz);
    if (bptt_updater->cutoffs == NULL) {
        ST_WARNING("Failed to st_malloc cutoffs.");
        goto ERR;
    }
    memset(bptt_updater->cutoffs, 0, sz);

    sz = sizeof(mat_t) * bptt_updater->bptt_opt.bptt;
    bptt_updater->cutoff_acs = (mat_t *)st_malloc(sz);
    if (bptt_updater->cutoff_acs == NULL) {
        ST_WARNING("Failed to st_malloc cutoff_acs.");
        goto ERR;
    }
    memset(bptt_updater->cutoff_acs, 0, sz);

    return bptt_updater;

ERR:
    safe_bptt_updater_destroy(bptt_updater);
    return NULL;
}

int bptt_updater_reset(bptt_updater_t *bptt_updater,
        int batch_i, mat_t *in_ac)
{
    mat_t row = {0};
    ivec_t *cutoffs;
    mat_t *cutoff_acs;

    ST_CHECK_PARAM(bptt_updater == NULL, -1);

    cutoffs = bptt_updater->cutoffs + bptt_updater->num_bptts - 1;
    if (ivec_append(cutoffs, batch_i) < 0) {
        ST_WARNING("Failed to ivec_append batch_i");
        return -1;
    }

    if (mat_submat(in_ac, batch_i, 1, 0, 0, &row) < 0) {
        ST_WARNING("Failed to mat_submat in_ac to row.");
        return -1;
    }

    cutoff_acs = bptt_updater->cutoff_acs + bptt_updater->num_bptts - 1;
    if (mat_append(cutoff_acs, &row) < 0) {
        ST_WARNING("Failed to mat_append cutoff_acs.");
        return -1;
    }

    return 0;
}

int bptt_updater_clear(bptt_updater_t *bptt_updater)
{
    int i;

    ST_CHECK_PARAM(bptt_updater == NULL, -1);

    for (i = 0; i < bptt_updater->num_bptts; i++) {
        if (ivec_clear(bptt_updater->cutoffs + i) < 0) {
            ST_WARNING("Failed to ivec_clear cutoffs[%d]", i);
            return -1;
        }
    }

    for (i = 0; i < bptt_updater->num_bptts; i++) {
        if (mat_clear(bptt_updater->cutoff_acs + i) < 0) {
            ST_WARNING("Failed to ivec_clear cutoff_acs[%d]", i);
            return -1;
        }
    }

    bptt_updater->num_bptts = 0;

    return 0;
}
