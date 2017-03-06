/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Wang Jian
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
#include <assert.h>
#include <limits.h>

#include <stutils/st_macro.h>
#include <stutils/st_log.h>
#include <stutils/st_string.h>

#include "bloom_filter.h"

int bloom_filter_load_opt(bloom_filter_opt_t *blm_flt_opt,
        st_opt_t *opt, const char *sec_name)
{
    char str[MAX_ST_CONF_LEN];

    ST_CHECK_PARAM(blm_flt_opt == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_STR(opt, sec_name, "CAPACITY", str, MAX_ST_CONF_LEN, "1M",
            "Number of bits in bloom filter (can be set to 10M, 2G, etc.)");
    blm_flt_opt->capacity = (size_t)st_str2ll(str);
    if (blm_flt_opt->capacity <= 0) {
        ST_WARNING("Invalid capacity[%s]", str);
        goto ST_OPT_ERR;
    }

    ST_OPT_SEC_GET_INT(opt, sec_name, "NUM_HASH", blm_flt_opt->num_hash, 1,
            "Number of hash function to be used.");
    if (blm_flt_opt->num_hash <= 0) {
        ST_WARNING("NUM_HASH must be larger than 0.");
        goto ST_OPT_ERR;
    }

    return 0;

ST_OPT_ERR:
    return -1;
}

void bloom_filter_destroy(bloom_filter_t *blm_flt)
{
    if (blm_flt == NULL) {
        return;
    }

    safe_free(blm_flt->cells);
}

bloom_filter_t* bloom_filter_create(bloom_filter_opt_t *blm_flt_opt)
{
    bloom_filter_t *blm_flt = NULL;

    ST_CHECK_PARAM(blm_flt_opt == NULL, NULL);

    blm_flt = (bloom_filter_t *)malloc(sizeof(bloom_filter_t));
    if (blm_flt == NULL) {
        ST_WARNING("Failed to malloc bloom filter.");
        return NULL;
    }
    memset(blm_flt, 0, sizeof(bloom_filter_t));

    blm_flt->blm_flt_opt = *blm_flt_opt;

    blm_flt->blm_flt_opt.capacity = blm_flt_opt->capacity / sizeof(char) * sizeof(char);
    blm_flt->cells = (char *)malloc(blm_flt->blm_flt_opt.capacity / sizeof(char));
    if (blm_flt->cells != NULL) {
        ST_WARNING("Failed to malloc cells");
        goto ERR;
    }
    memset(blm_flt->cells, 0, blm_flt->blm_flt_opt.capacity / sizeof(char));

    return blm_flt;

ERR:
    safe_bloom_filter_destroy(blm_flt);
    return NULL;
}

int bloom_filter_save(bloom_filter_t *blm_flt, FILE *fp, bool binary)
{
    ST_CHECK_PARAM(blm_flt == NULL || fp == NULL, -1);

    return 0;
}

bloom_filter_t* bloom_filter_load(FILE *fp)
{
    bloom_filter_t *blm_flt = NULL;

    ST_CHECK_PARAM(fp == NULL, NULL);

    return blm_flt;

//ERR:
//    safe_bloom_filter_destroy(blm_flt);
 //   return NULL;
}

int bloom_filter_add(bloom_filter_t *blm_flt, int *words, int n)
{
    ST_CHECK_PARAM(blm_flt == NULL || words == NULL || n < 0, -1);

    return 0;
}

int bloom_filter_lookup(bloom_filter_t *blm_flt, int *words, int n)
{
    ST_CHECK_PARAM(blm_flt == NULL || words == NULL || n < 0, -1);

    return 0;
}

int bloom_filter_build(bloom_filter_t *blm_flt, int n_thr, FILE *text_fp)
{
    ST_CHECK_PARAM(blm_flt == NULL || n_thr < 0 || text_fp == NULL, -1);

    return 0;
}
