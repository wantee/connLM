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
#include <stutils/st_rand.h>
#include <stutils/st_io.h>

#include "fst_converter.h"

int fst_converter_load_opt(fst_converter_opt_t *converter_opt,
        st_opt_t *opt, const char *sec_name)
{
    ST_CHECK_PARAM(converter_opt == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_BOOL(opt, sec_name, "PRINT_SYMS",
            converter_opt->print_syms, false,
            "Print symbols instead of numbers, if true. ");

    return 0;

ST_OPT_ERR:
    return -1;
}

void fst_converter_destroy(fst_converter_t *converter)
{
    int i;

    if (converter == NULL) {
        return;
    }

    converter->connlm = NULL;

    for(i = 0; i < converter->n_thr; i++) {
        safe_updater_destroy(converter->updaters[i]);
    }
    safe_free(converter->updaters);
    converter->n_thr = 0;
}

fst_converter_t* fst_converter_create(connlm_t *connlm, int n_thr,
        fst_converter_opt_t *converter_opt)
{
    fst_converter_t *converter = NULL;

    int i;

    ST_CHECK_PARAM(connlm == NULL || n_thr <= 0 || converter_opt == NULL, NULL);

    converter = (fst_converter_t *)malloc(sizeof(fst_converter_t));
    if (converter == NULL) {
        ST_WARNING("Failed to malloc converter.");
        return NULL;
    }
    memset(converter, 0, sizeof(fst_converter_t));

    converter->connlm = connlm;
    converter->n_thr = n_thr;
    converter->converter_opt = *converter_opt;

    converter->updaters = (updater_t **)malloc(sizeof(updater_t*)*n_thr);
    if (converter->updaters == NULL) {
        ST_WARNING("Failed to malloc updaters.");
        goto ERR;
    }
    memset(converter->updaters, 0, sizeof(updater_t*) * n_thr);

    for (i = 0; i < converter->n_thr; i++) {
        converter->updaters[i] = updater_create(connlm);
        if (converter->updaters[i] == NULL) {
            ST_WARNING("Failed to malloc updater[%d].", i);
            goto ERR;
        }
    }

    return converter;

ERR:
    safe_fst_converter_destroy(converter);
    return NULL;
}

static int fst_converter_setup(fst_converter_t *converter)
{
    int i;

    ST_CHECK_PARAM(converter == NULL, -1);

    if (connlm_setup(converter->connlm) < 0) {
        ST_WARNING("Failed to connlm_setup.");
        return -1;
    }

    for (i = 0; i < converter->n_thr; i++) {
        if (updater_setup(converter->updaters[i], false) < 0) {
            ST_WARNING("Failed to updater_setup.");
            return -1;
        }
    }

    return 0;
}

int fst_converter_convert(fst_converter_t *converter, FILE *fst_fp)
{
    ST_CHECK_PARAM(converter == NULL || fst_fp == NULL, -1);

    if (fst_converter_setup(converter) < 0) {
        ST_WARNING("Failed to fst_converter_setup.");
        goto ERR;
    }

    return 0;

ERR:

    return -1;
}
