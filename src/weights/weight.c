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

#include <st_log.h>
#include <st_utils.h>

#include "weight.h"

void wt_destroy(weight_t *wt)
{
    if (wt == NULL) {
        return;
    }

    wt->name[0] = '\0';
    wt->id = -1;
}

weight_t* wt_parse_topo(const char *line)
{
    weight_t *wt = NULL;

    ST_CHECK_PARAM(line == NULL, NULL);

    return wt;
}

weight_t* wt_dup(weight_t *w)
{
    weight_t *wt = NULL;

    ST_CHECK_PARAM(w == NULL, NULL);

    wt = (weight_t *) malloc(sizeof(weight_t));
    if (wt == NULL) {
        ST_WARNING("Falied to malloc weight_t.");
        goto ERR;
    }
    memset(wt, 0, sizeof(weight_t));

    strncpy(wt->name, w->name, MAX_NAME_LEN);
    wt->name[MAX_NAME_LEN - 1] = '\0';
    wt->id = w->id;

    wt->forward = w->forward;
    wt->backprop = w->backprop;

    return wt;

ERR:
    safe_wt_destroy(wt);
    return NULL;
}

