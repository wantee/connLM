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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <stutils/st_macro.h>
#include <stutils/st_log.h>

#include "updater.h"

void updater_destroy(updater_t *updater)
{
    int c;

    if (updater == NULL) {
        return;
    }

    safe_out_updater_destroy(updater->out_updater);
    if (updater->comp_updaters != NULL) {
        for (c = 0; c < updater->connlm->num_comp; c++) {
            safe_comp_updater_destroy(updater->comp_updaters[c]);
        }
        safe_free(updater->comp_updaters);
    }
    updater->connlm = NULL;
}

updater_t* updater_create(connlm_t *connlm)
{
    updater_t *updater = NULL;

    size_t sz;
    int c;

    ST_CHECK_PARAM(connlm == NULL, NULL);

    updater = (updater_t *)malloc(sizeof(updater_t));
    if (updater == NULL) {
        ST_WARNING("Failed to malloc updater.");
        goto ERR;
    }
    memset(updater, 0, sizeof(updater_t));

    updater->connlm = connlm;

    updater->out_updater = out_updater_create(connlm->output);
    if (updater->out_updater == NULL) {
        ST_WARNING("Failed to out_updater_create.");
        goto ERR;
    }

    if (connlm->num_comp > 0) {
        sz = sizeof(comp_updater_t*)*connlm->num_comp;
        updater->comp_updaters = (comp_updater_t **)malloc(sz);
        if (updater->comp_updaters == NULL) {
            ST_WARNING("Failed to malloc comp_updaters.");
            goto ERR;
        }

        for (c = 0; c < connlm->num_comp; c++) {
            updater->comp_updaters[c] = comp_updater_create(connlm->comps[c]);
            if (updater->comp_updaters[c] == NULL) {
                ST_WARNING("Failed to comp_updater_create[%s].",
                        connlm->comps[c]->name);
                goto ERR;
            }
        }
    }

    return updater;

ERR:
    safe_updater_destroy(updater);
    return NULL;
}

int updater_setup(updater_t *updater, bool backprob)
{
    int c;

    ST_CHECK_PARAM(updater == NULL, -1);

    if (out_updater_setup(updater->out_updater, backprob) < 0) {
        ST_WARNING("Failed to out_updater_setup.");
        return -1;
    }

    for (c = 0; c < updater->connlm->num_comp; c++) {
        if (comp_updater_setup(updater->comp_updaters[c], backprob) < 0) {
            ST_WARNING("Failed to comp_updater_setup[%s].",
                    updater->connlm->comps[c]->name);
            return -1;
        }
    }

    return 0;
}
