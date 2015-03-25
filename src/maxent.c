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
#include <st_log.h>

#include "maxent.h"

int maxent_load_opt(maxent_opt_t *maxent_opt,
        st_opt_t *opt, const char *sec_name)
{
    float f;
    int d;

    ST_CHECK_PARAM(maxent_opt == NULL || opt == NULL, -1);

    memset(maxent_opt, 0, sizeof(maxent_opt_t));

    ST_OPT_SEC_GET_FLOAT(opt, sec_name, "SCALE", f, 1.0, 
            "Scale of MaxEnt model output");
    maxent_opt->scale = (real_t)f;

    if (maxent_opt->scale <= 0) {
        return 0;
    }

    ST_OPT_SEC_GET_INT(opt, sec_name, "HASH_SIZE", d, 2,
            "Size of MaxEnt hash(in millions)");
    maxent_opt->direct_size = d * 1000000;
    ST_OPT_SEC_GET_INT(opt, sec_name, "ORDER",
            maxent_opt->direct_order, 3,
            "Order of MaxEnt");

    return 0;

ST_OPT_ERR:
    return -1;
}

maxent_t *maxent_create(maxent_opt_t *maxent_opt)
{
    maxent_t *maxent = NULL;

    ST_CHECK_PARAM(maxent_opt == NULL, NULL);

    maxent = (maxent_t *) malloc(sizeof(maxent_t));
    if (maxent == NULL) {
        ST_WARNING("Falied to malloc maxent_t.");
        goto ERR;
    }
    memset(maxent, 0, sizeof(maxent_t));

    return maxent;
  ERR:
    safe_maxent_destroy(maxent);
    return NULL;
}

void maxent_destroy(maxent_t *maxent)
{
    if (maxent == NULL) {
        return;
    }
}

int maxent_train(maxent_t *maxent)
{
    ST_CHECK_PARAM(maxent == NULL, -1);

    return 0;
}

