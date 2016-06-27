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
    if (updater == NULL) {
        return;
    }

    updater->connlm = NULL;
}

updater_t* updater_create(connlm_t *connlm)
{
    updater_t *updater = NULL;

    ST_CHECK_PARAM(connlm == NULL, NULL);

    updater = (updater_t *)malloc(sizeof(updater_t));
    if (updater == NULL) {
        ST_WARNING("Failed to malloc updater.");
        goto ERR;
    }
    memset(updater, 0, sizeof(updater_t));

    updater->connlm = connlm;

    return updater;

ERR:
    safe_updater_destroy(updater);
    return NULL;
}

int updater_setup(updater_t *updater, bool backprob)
{
    ST_CHECK_PARAM(updater == NULL, -1);

    return 0;
}
