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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

#include <st_macro.h>
#include <stlog.h>

#include "ffnn.h"

int ffnn_load_opt(ffnn_opt_t *ffnn_opt, stconf_t *pconf, const char *sec_name)
{
    float f;

    ST_CHECK_PARAM(ffnn_opt == NULL || pconf == NULL, -1);

    memset(ffnn_opt, 0, sizeof(ffnn_opt_t));

    GET_FLOAT_SEC_CONF(pconf, sec_name, "SCALE", f);
    ffnn_opt->scale = (real_t)f;

    if (ffnn_opt->scale <= 0) {
        return 0;
    }

    return 0;

STCONF_ERR:
    return -1;
}

ffnn_t *ffnn_create(ffnn_opt_t *ffnn_opt)
{
    ffnn_t *ffnn = NULL;

    ST_CHECK_PARAM(ffnn_opt == NULL, NULL);

    ffnn = (ffnn_t *) malloc(sizeof(ffnn_t));
    if (ffnn == NULL) {
        ST_WARNING("Falied to malloc ffnn_t.");
        goto ERR;
    }
    memset(ffnn, 0, sizeof(ffnn_t));

    return ffnn;
  ERR:
    safe_ffnn_destroy(ffnn);
    return NULL;
}

void ffnn_destroy(ffnn_t *ffnn)
{
    if (ffnn == NULL) {
        return;
    }
}

int ffnn_train(ffnn_t *ffnn)
{
    ST_CHECK_PARAM(ffnn == NULL, -1);

    return 0;
}

