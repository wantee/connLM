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

#include <stutils/st_macro.h>
#include <stutils/st_log.h>
#include <stutils/st_io.h>
#include <stutils/st_string.h>

#include "reader.h"
#include "bloom_filter.h"

static const int BLOOM_FILTER_MAGIC_NUM = 626140498 + 50;

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

    ST_OPT_SEC_GET_INT(opt, sec_name, "MAX_ORDER", blm_flt_opt->max_order, 5,
            "max order of n-grams.");
    if (blm_flt_opt->max_order <= 0) {
        ST_WARNING("MAX_ORDER must be larger than zero");
        goto ST_OPT_ERR;
    }

    ST_OPT_SEC_GET_BOOL(opt, sec_name, "FULL_CONTEXT",
            blm_flt_opt->full_context, false,
            "Compute only grams begins with <s>, if false.");

    return 0;

ST_OPT_ERR:
    return -1;
}

void bloom_filter_destroy(bloom_filter_t *blm_flt)
{
    int o;

    if (blm_flt == NULL) {
        return;
    }

    if (blm_flt->nghashes != NULL) {
        for (o = 0; o <= blm_flt->blm_flt_opt.max_order; o++) {
            safe_ngram_hash_destroy(blm_flt->nghashes[o]);
        }
        safe_free(blm_flt->nghashes);
    }

    safe_free(blm_flt->cells);

    safe_vocab_destroy(blm_flt->vocab);
}

bloom_filter_t* bloom_filter_create(bloom_filter_opt_t *blm_flt_opt,
        vocab_t *vocab)
{
    bloom_filter_t *blm_flt = NULL;

    int *context = NULL;
    int o;

    ST_CHECK_PARAM(blm_flt_opt == NULL || vocab == NULL, NULL);

    blm_flt = (bloom_filter_t *)malloc(sizeof(bloom_filter_t));
    if (blm_flt == NULL) {
        ST_WARNING("Failed to malloc bloom filter.");
        return NULL;
    }
    memset(blm_flt, 0, sizeof(bloom_filter_t));

    blm_flt->blm_flt_opt = *blm_flt_opt;

    blm_flt->vocab = vocab_dup(vocab);

    blm_flt->blm_flt_opt.capacity = blm_flt_opt->capacity;
    blm_flt->cells = (char *)malloc(BITNSLOTS(blm_flt->blm_flt_opt.capacity));
    if (blm_flt->cells != NULL) {
        ST_WARNING("Failed to malloc cells");
        goto ERR;
    }
    memset(blm_flt->cells, 0, BITNSLOTS(blm_flt->blm_flt_opt.capacity));

    if (blm_flt->blm_flt_opt.full_context) {
        blm_flt->nghashes = (ngram_hash_t **)malloc(sizeof(ngram_hash_t*)
                * blm_flt_opt->max_order);
    } else {
        blm_flt->nghashes = (ngram_hash_t **)malloc(sizeof(ngram_hash_t*));
    }
    if (blm_flt->nghashes == NULL) {
        ST_WARNING("Failed to malloc nghashes.");
        goto ERR;
    }

    context = (int *)malloc(sizeof(int) * blm_flt_opt->max_order);
    if (context == NULL) {
        ST_WARNING("Failed to malloc context.");
        goto ERR;
    }
    for (o = 0; o < blm_flt_opt->max_order; o++) {
        context[o] = -(blm_flt_opt->max_order - o - 1);
    }

    blm_flt->nghashes[0] = ngram_hash_create(context, blm_flt_opt->max_order);
    if (blm_flt->nghashes[0] == NULL) {
        ST_WARNING("Failed to ngram_hash_create for [%d]-gram.",
                blm_flt_opt->max_order);
        goto ERR;
    }

    if (blm_flt->blm_flt_opt.full_context) {
        for (o = 1; o < blm_flt_opt->max_order; o++) {
            blm_flt->nghashes[o] = ngram_hash_create(context + o,
                    blm_flt_opt->max_order - o);
            if (blm_flt->nghashes[o] == NULL) {
                ST_WARNING("Failed to ngram_hash_create for [%d]-gram.", o);
                goto ERR;
            }
        }
    }

    safe_free(context);

    return blm_flt;

ERR:
    safe_free(context);
    safe_bloom_filter_destroy(blm_flt);
    return NULL;
}

static int bloom_filter_load_header(bloom_filter_t *blm_flt,
        FILE *fp, bool *binary, FILE *fo_info)
{
    char sym[MAX_LINE_LEN];
    union {
        char str[4];
        int magic_num;
    } flag;

    int version;
    size_t capacity;
    int num_hash;
    int max_order;
    bool full_context;

    ST_CHECK_PARAM((blm_flt == NULL && fo_info == NULL) || fp == NULL
            || binary == NULL, -1);

    if (fread(&flag.magic_num, sizeof(int), 1, fp) != 1) {
        ST_WARNING("Failed to load magic num.");
        return -1;
    }

    if (strncmp(flag.str, "    ", 4) == 0) {
        *binary = false;
    } else if (BLOOM_FILTER_MAGIC_NUM != flag.magic_num) {
        ST_WARNING("magic num wrong.");
        return -2;
    } else {
        *binary = true;
    }

    if (*binary) {
        if (fread(&version, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read version.");
            return -1;
        }

        if (version > CONNLM_FILE_VERSION) {
            ST_WARNING("Too high file versoin[%d], "
                    "please upgrade connlm toolkit", version);
            return -1;
        }

        if (fread(&capacity, sizeof(size_t), 1, fp) != 1) {
            ST_WARNING("Failed to read capacity.");
            return -1;
        }

        if (fread(&num_hash, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read num_hash.");
            return -1;
        }

        if (fread(&max_order, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read max_order.");
            return -1;
        }

        if (fread(&full_context, sizeof(bool), 1, fp) != 1) {
            ST_WARNING("Failed to read full_context.");
            return -1;
        }
    } else {
        if (st_readline(fp, "    ") != 0) {
            ST_WARNING("flag error.");
            return -1;
        }
        if (st_readline(fp, "<BLOOM_FILTER>") != 0) {
            ST_WARNING("flag error.");
            return -1;
        }

        if (st_readline(fp, "Version: %d", &version) != 1) {
            ST_WARNING("Failed to read version.");
            return -1;
        }

        if (version > CONNLM_FILE_VERSION) {
            ST_WARNING("Too high file versoin[%d], "
                    "please update connlm toolkit");
            return -1;
        }

        if (st_readline(fp, "Capacity: %zu", &capacity) != 1) {
            ST_WARNING("Failed to parse capacity.[%zu]", capacity);
            return -1;
        }

        if (st_readline(fp, "Num Hash: %d", &num_hash) != 1) {
            ST_WARNING("Failed to parse num_hash.[%d]", num_hash);
            return -1;
        }

        if (st_readline(fp, "Max Order: %d", &max_order) != 1) {
            ST_WARNING("Failed to parse max_order.[%d]", max_order);
            return -1;
        }

        if (st_readline(fp, "Full Context: %"xSTR(MAX_LINE_LEN)"s",
                    sym) != 1) {
            ST_WARNING("Failed to parse full_context.[%s]", sym);
            return -1;
        }
        sym[MAX_LINE_LEN - 1] = '\0';
        full_context = str2bool(sym);
    }

    if (blm_flt != NULL) {
        blm_flt->blm_flt_opt.capacity = capacity;
        blm_flt->blm_flt_opt.num_hash = num_hash;
        blm_flt->blm_flt_opt.max_order = max_order;
        blm_flt->blm_flt_opt.full_context = full_context;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "<BLOOM_FILTER>\n");
        fprintf(fo_info, "Version: %d\n", version);
        fprintf(fo_info, "Capacity: %zu\n", capacity);
        fprintf(fo_info, "Num Hash: %d\n", num_hash);
        fprintf(fo_info, "Max Order: %d\n", max_order);
        fprintf(fo_info, "Full Context: %s\n", bool2str(full_context));
    }

    return version;
}

static int bloom_filter_load_body(bloom_filter_t *blm_flt, int version,
        FILE *fp, bool binary)
{
    int n;

    size_t i;
    int f;

    ST_CHECK_PARAM(blm_flt == NULL || fp == NULL, -1);

    blm_flt->cells = NULL;

    if (binary) {
        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read magic num.");
            goto ERR;
        }

        if (n != -BLOOM_FILTER_MAGIC_NUM) {
            ST_WARNING("Magic num error.");
            goto ERR;
        }

        blm_flt->cells = (char *)malloc(BITNSLOTS(blm_flt->blm_flt_opt.capacity));
        if (blm_flt->cells == NULL) {
            ST_WARNING("Failed to malloc cells.");
            goto ERR;
        }

        if (fread(blm_flt->cells, sizeof(char),
                    BITNSLOTS(blm_flt->blm_flt_opt.capacity),
                    fp) != BITNSLOTS(blm_flt->blm_flt_opt.capacity)) {
            ST_WARNING("Failed to read cells.");
            goto ERR;
        }
    } else {
        if (st_readline(fp, "<BLOOM_FILTER-DATA>") != 0) {
            ST_WARNING("body flag error.");
            goto ERR;
        }

        blm_flt->cells = (char *)malloc(BITNSLOTS(blm_flt->blm_flt_opt.capacity));
        if (blm_flt->cells == NULL) {
            ST_WARNING("Failed to malloc cells.");
            goto ERR;
        }
        memset(blm_flt->cells, 0, BITNSLOTS(blm_flt->blm_flt_opt.capacity));

        if (st_readline(fp, " Cells:") != 0) {
            ST_WARNING("cells flag error.");
            goto ERR;
        }

        for (i = 0; i < blm_flt->blm_flt_opt.capacity; i++) {
            if (st_readline(fp, "\t%*zu\t%d\n", &f) != 1) {
                ST_WARNING("Failed to parse cells.");
                goto ERR;
            }
            if (f != 0) {
                BITSET(blm_flt->cells, i);
            }
        }
    }

    return 0;
ERR:
    safe_free(blm_flt->cells);
    return -1;
}

static int bloom_filter_save_header(bloom_filter_t *blm_flt, FILE *fp,
        bool binary)
{
    int n;

    ST_CHECK_PARAM(blm_flt == NULL || fp == NULL, -1);

    if (binary) {
        if (fwrite(&BLOOM_FILTER_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        n = CONNLM_FILE_VERSION;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write version.");
            return -1;
        }

        if (fwrite(&blm_flt->blm_flt_opt.capacity, sizeof(size_t), 1, fp) != 1) {
            ST_WARNING("Failed to write capacity.");
            return -1;
        }

        if (fwrite(&blm_flt->blm_flt_opt.num_hash, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write num_hash.");
            return -1;
        }

        if (fwrite(&blm_flt->blm_flt_opt.max_order, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write max_order.");
            return -1;
        }

        if (fwrite(&blm_flt->blm_flt_opt.full_context, sizeof(bool), 1, fp) != 1) {
            ST_WARNING("Failed to write full_context.");
            return -1;
        }
    } else {
        if (fprintf(fp, "    \n<BLOOM_FILTER>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }
        if (fprintf(fp, "Version: %d\n", CONNLM_FILE_VERSION) < 0) {
            ST_WARNING("Failed to fprintf version.");
            return -1;
        }

        if (fprintf(fp, "Capacity: %zu\n", blm_flt->blm_flt_opt.capacity) < 0) {
            ST_WARNING("Failed to fprintf capacity.");
            return -1;
        }

        if (fprintf(fp, "Num Hash: %d\n", blm_flt->blm_flt_opt.num_hash) < 0) {
            ST_WARNING("Failed to fprintf num_hash.");
            return -1;
        }

        if (fprintf(fp, "Max Order: %d\n", blm_flt->blm_flt_opt.max_order) < 0) {
            ST_WARNING("Failed to fprintf max_order.");
            return -1;
        }

        if (fprintf(fp, "Full Context: %s\n",
                    bool2str(blm_flt->blm_flt_opt.full_context)) < 0) {
            ST_WARNING("Failed to fprintf full_context.");
            return -1;
        }
    }

    return 0;
}

static int bloom_filter_save_body(bloom_filter_t *blm_flt, FILE *fp, bool binary)
{
    int n;

    size_t i;
    int f;

    ST_CHECK_PARAM(blm_flt == NULL || fp == NULL, -1);

    if (binary) {
        n = -BLOOM_FILTER_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        if (fwrite(blm_flt->cells, sizeof(char),
                    BITNSLOTS(blm_flt->blm_flt_opt.capacity),
                    fp) != BITNSLOTS(blm_flt->blm_flt_opt.capacity)) {
            ST_WARNING("Failed to write cells.");
            return -1;
        }
    } else {
        if (fprintf(fp, "<BLOOM_FILTER-DATA>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }

        if (fprintf(fp, "Cells:\n") < 0) {
            ST_WARNING("Failed to fprintf cells.");
            return -1;
        }
        for (i = 0; i < blm_flt->blm_flt_opt.capacity; i++) {
            if (BITTEST(blm_flt->cells, i)) {
                f = 1;
            } else {
                f = 0;
            }
            if (fprintf(fp, "\t%zu\t%d\n", i, f) < 0) {
                ST_WARNING("Failed to fprintf cell[%zu]", i);
                return -1;
            }
        }
    }

    return 0;
}

int bloom_filter_save(bloom_filter_t *blm_flt, FILE *fp, bool binary)
{
    ST_CHECK_PARAM(blm_flt == NULL || fp == NULL, -1);

    if (bloom_filter_save_header(blm_flt, fp, binary) < 0) {
        ST_WARNING("Failed to bloom_filter_save_header.");
        return -1;
    }

    if (bloom_filter_save_body(blm_flt, fp, binary) < 0) {
        ST_WARNING("Failed to bloom_filter_save_body.");
        return -1;
    }

    return 0;
}

bloom_filter_t* bloom_filter_load(FILE *fp)
{
    bloom_filter_t *blm_flt = NULL;
    int version;
    bool binary;

    ST_CHECK_PARAM(fp == NULL, NULL);

    blm_flt = (bloom_filter_t *)malloc(sizeof(bloom_filter_t));
    if (blm_flt == NULL) {
        ST_WARNING("Failed to malloc bloom_filter_t");
        goto ERR;
    }
    memset(blm_flt, 0, sizeof(bloom_filter_t));

    version = bloom_filter_load_header(blm_flt, fp, &binary, NULL);
    if (version < 0) {
        ST_WARNING("Failed to bloom_filter_load_header.");
        goto ERR;
    }

    if (bloom_filter_load_body(blm_flt, version, fp, binary) < 0) {
        ST_WARNING("Failed to bloom_filter_load_body.");
        goto ERR;
    }

    return blm_flt;

ERR:
    safe_bloom_filter_destroy(blm_flt);
    return NULL;
}

int bloom_filter_print_info(FILE *fp, FILE *fo_info)
{
    bool binary;

    ST_CHECK_PARAM(fp == NULL || fo_info == NULL, -1);

    if (bloom_filter_load_header(NULL, fp, &binary, fo_info) < 0) {
        ST_WARNING("Failed to bloom_filter_load_header.");
        return -1;
    }

    return 0;
}

int bloom_filter_add(bloom_filter_t *blm_flt, int *words, int n)
{
    ST_CHECK_PARAM(blm_flt == NULL || words == NULL || n <= 0, -1);

    return 0;
}

bool bloom_filter_lookup(bloom_filter_t *blm_flt, int *words, int n)
{
    ST_CHECK_PARAM(blm_flt == NULL || words == NULL || n <= 0, false);

    return false;
}

#define EPOCH_SIZE 1000

int bloom_filter_build(bloom_filter_t *blm_flt, FILE *text_fp)
{
    connlm_egs_t egs = {
        .words = NULL,
        .size = 0,
        .capacity = 0,
    };

    int max_order;
    bool full_context;

    int sent_start;
    int pos;
    int order;

    ST_CHECK_PARAM(blm_flt == NULL || text_fp == NULL, -1);

    max_order = blm_flt->blm_flt_opt.max_order;
    full_context = blm_flt->blm_flt_opt.full_context;

    while (!feof(text_fp)) {
        if (connlm_egs_read(&egs, NULL, EPOCH_SIZE,
                    text_fp, blm_flt->vocab, NULL) < 0) {
            ST_WARNING("Failed to connlm_egs_read.");
            goto ERR;
        }

        for (pos = 0; pos < egs.size; pos++) {
            if (egs.words[pos] == vocab_get_id(blm_flt->vocab, SENT_START)) {
                sent_start = pos;
            }

            order = pos - sent_start + 1;
            order = min(order, max_order);

            if (pos - order + 1 <= sent_start) {
                if (bloom_filter_add(blm_flt, egs.words + sent_start,
                            order) < 0) {
                    ST_WARNING("Failed to bloom_filter_add.");
                    goto ERR;
                }
                --order;
            }

            if (full_context) {
                while (order > 0) {
                    if (bloom_filter_add(blm_flt,
                                egs.words + pos - order + 1, order) < 0) {
                        ST_WARNING("Failed to bloom_filter_add.");
                        goto ERR;
                    }
                    --order;
                }
            }
        }
    }

    connlm_egs_destroy(&egs);

    return 0;

ERR:
    connlm_egs_destroy(&egs);

    return -1;
}
