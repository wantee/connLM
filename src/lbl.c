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

#include "lbl.h"

int lbl_load_opt(lbl_opt_t *lbl_opt, st_opt_t *opt, const char *sec_name)
{
    float f;

    ST_CHECK_PARAM(lbl_opt == NULL || opt == NULL, -1);

    memset(lbl_opt, 0, sizeof(lbl_opt_t));

    ST_OPT_SEC_GET_FLOAT(opt, sec_name, "SCALE", f, 1.0,
            "Scale of LBL model output");
    lbl_opt->scale = (real_t)f;

    if (lbl_opt->scale <= 0) {
        return 0;
    }

    return 0;

ST_OPT_ERR:
    return -1;
}

lbl_t *lbl_create(lbl_opt_t *lbl_opt)
{
    lbl_t *lbl = NULL;

    ST_CHECK_PARAM(lbl_opt == NULL, NULL);

    lbl = (lbl_t *) malloc(sizeof(lbl_t));
    if (lbl == NULL) {
        ST_WARNING("Falied to malloc lbl_t.");
        goto ERR;
    }
    memset(lbl, 0, sizeof(lbl_t));

    return lbl;
  ERR:
    safe_lbl_destroy(lbl);
    return NULL;
}

void lbl_destroy(lbl_t *lbl)
{
    if (lbl == NULL) {
        return;
    }
}

int lbl_train(lbl_t *lbl)
{
    ST_CHECK_PARAM(lbl == NULL, -1);

    return 0;
}

