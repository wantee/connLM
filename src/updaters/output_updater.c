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

#include "output_updater.h"

void out_updater_destroy(out_updater_t *out_updater)
{
    if (out_updater == NULL) {
        return;
    }

    safe_free(out_updater->ac);
    safe_free(out_updater->er);

    out_updater->output = NULL;
}

out_updater_t* out_updater_create(output_t *output)
{
    out_updater_t *out_updater = NULL;

    ST_CHECK_PARAM(output == NULL, NULL);

    out_updater = (out_updater_t *)malloc(sizeof(out_updater_t));
    if (out_updater == NULL) {
        ST_WARNING("Failed to malloc out_updater.");
        goto ERR;
    }
    memset(out_updater, 0, sizeof(out_updater_t));

    out_updater->output = output;

    return out_updater;

ERR:
    safe_out_updater_destroy(out_updater);
    return NULL;
}

int out_updater_setup(out_updater_t *out_updater, bool backprob)
{
    ST_CHECK_PARAM(out_updater == NULL, -1);

    return 0;
}

int out_updater_step(out_updater_t *out_updater)
{
    ST_CHECK_PARAM(out_updater == NULL, -1);

    return 0;
}
