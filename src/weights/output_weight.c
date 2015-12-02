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

#include "output_weight.h"

void output_wt_destroy(output_wt_t *output_wt)
{
    if (output_wt == NULL) {
        return;
    }

    output_wt->output = NULL;
}

output_wt_t* output_wt_dup(output_wt_t *o)
{
    output_wt_t *output_wt = NULL;

    ST_CHECK_PARAM(o == NULL, NULL);

    output_wt = (output_wt_t *) malloc(sizeof(output_wt_t));
    if (output_wt == NULL) {
        ST_WARNING("Falied to malloc output_wt_t.");
        goto ERR;
    }
    memset(output_wt, 0, sizeof(output_wt_t));

    output_wt->output = o->output;

    output_wt->forward = o->forward;
    output_wt->backprop = o->backprop;

    return output_wt;

ERR:
    safe_output_wt_destroy(output_wt);
    return NULL;
}

