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
#include <stutils/st_int.h>
#include <stutils/st_bit.h>
#include <stutils/st_string.h>
#include <stutils/st_varint.h>

#include "connlm.h"
#include "reader.h"
#include "bloom_filter.h"

static const int BLOOM_FILTER_MAGIC_NUM = 626140498 + 50;

#define bos_id(blm_flt) vocab_get_id((blm_flt)->vocab, SENT_START)

static int str2intlist(const char *str, int *list, int n)
{
    int *arr = NULL;
    int n_arr = 0;
    int i;

    if (st_parse_int_array(str, &arr, &n_arr) < 0) {
        ST_WARNING("Failed to st_parse_int_array [%s].", str);
        return -1;
    }

    for (i = 0; i < n && i < n_arr; i++) {
        list[i] = arr[i];
    }
    for(; i < n; i++) { // fill the remaining with last value
        list[i] = arr[n_arr - 1];
    }

    safe_st_free(arr);

    if (n > n_arr) {
        return 1;
    } else if (n < n_arr) {
        return 2;
    }

    return 0;
}

static char* intlist2str(int *list, int n, char *str, size_t str_len)
{
    size_t len;
    int i;

    len = 0;
    for (i = 0; i < n - 1; i++) {
        snprintf(str + len, str_len - len, "%d,", list[i]);
        len = strlen(str);
    }
    snprintf(str + len, str_len - len, "%d", list[i]);

    return str;
}

static int get_max_min_count(bloom_filter_t *blm_flt)
{
    int i;
    int max;

    max = blm_flt->blm_flt_opt.min_counts[0];;
    for (i = 1; i < blm_flt->blm_flt_opt.max_order; i++) {
        if (blm_flt->blm_flt_opt.min_counts[i] > max) {
            max = blm_flt->blm_flt_opt.min_counts[i];
        }
    }

    return max;
}

static int get_num_unit_bits(bloom_filter_t *blm_flt)
{
    int m;
    int num_bits;

    m = get_max_min_count(blm_flt);
    num_bits = 1;
    while ((m = m >> 1) > 0) {
        ++num_bits;
    }

    return num_bits;
}

static size_t get_num_cells(bloom_filter_t *blm_flt)
{
    return NBITNSLOTS(blm_flt->blm_flt_opt.capacity, get_num_unit_bits(blm_flt));
}

bloom_filter_format_t bloom_filter_format_parse(const char *str)
{
    ST_CHECK_PARAM(str == NULL, BF_FMT_UNKNOWN);

    if (strcasecmp(str, "Text") == 0
            || strcasecmp(str, "Txt") == 0
            || strcasecmp(str, "T") == 0) {
        return BF_FMT_TXT;
    } else if (strcasecmp(str, "Binary") == 0
            || strcasecmp(str, "Bin") == 0
            || strcasecmp(str, "B") == 0) {
        return BF_FMT_BIN;
    } else if (strcasecmp(str, "Compressed") == 0
            || strcasecmp(str, "Compress") == 0
            || strcasecmp(str, "C") == 0) {
        return BF_FMT_COMPRESSED;
    }

    return BF_FMT_UNKNOWN;
}

static char* fmt2str(bloom_filter_format_t fmt)
{
    switch (fmt) {
        case BF_FMT_TXT:
            return "Text";
        case BF_FMT_BIN:
            return "Binary";
        case BF_FMT_COMPRESSED:
            return "Compressed";
        default:
            return "Unknown";
    }
    return "Unknown";
}

int bloom_filter_load_opt(bloom_filter_opt_t *blm_flt_opt,
        st_opt_t *opt, const char *sec_name)
{
    char str[MAX_ST_CONF_LEN];
    int i;

    ST_CHECK_PARAM(blm_flt_opt == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_STR(opt, sec_name, "CAPACITY", str, MAX_ST_CONF_LEN, "1M",
            "Number of bits in bloom filter (can be set to 10M, 2G, etc.)");
    blm_flt_opt->capacity = (size_t)st_str2ll(str);
    if (blm_flt_opt->capacity <= 0) {
        ST_WARNING("Invalid capacity[%s]", str);
        goto ST_OPT_ERR;
    }

    ST_OPT_SEC_GET_INT(opt, sec_name, "NUM_HASHES", blm_flt_opt->num_hashes, 1,
            "Number of hashes to be used.");
    if (blm_flt_opt->num_hashes <= 0) {
        ST_WARNING("NUM_HASHES must be larger than 0.");
        goto ST_OPT_ERR;
    }

    ST_OPT_SEC_GET_INT(opt, sec_name, "MAX_ORDER", blm_flt_opt->max_order, 5,
            "max order of n-grams.");
    if (blm_flt_opt->max_order <= 0) {
        ST_WARNING("MAX_ORDER must be larger than zero");
        goto ST_OPT_ERR;
    }

    ST_OPT_SEC_GET_STR(opt, sec_name, "MIN_COUNTS", str, MAX_ST_CONF_LEN, "1",
            "min-count per every ngram order, in a comma-separated list.");

    blm_flt_opt->min_counts = (int *)st_malloc(sizeof(int)
            * blm_flt_opt->max_order);
    if (blm_flt_opt->min_counts == NULL) {
        ST_WARNING("Failed to st_malloc min_counts.");
        goto ST_OPT_ERR;
    }
    if (str2intlist(str, blm_flt_opt->min_counts,
                blm_flt_opt->max_order) < 0) {
        ST_WARNING("Failed to str2intlist for min_counts[%s].", str);
        goto ST_OPT_ERR;
    }
    for (i = 0; i < blm_flt_opt->max_order; i++) {
        if (blm_flt_opt->min_counts[i] < 1) {
            blm_flt_opt->min_counts[i] = 1;
        }
    }

    return 0;

ST_OPT_ERR:
    return -1;
}

void bloom_filter_buf_destroy(bloom_filter_buf_t *buf)
{
    if (buf == NULL) {
        return;
    }

    safe_st_free(buf->hash_vals);
}

bloom_filter_buf_t* bloom_filter_buf_create(bloom_filter_t *blm_flt)
{
    bloom_filter_buf_t *buf = NULL;

    ST_CHECK_PARAM(blm_flt == NULL, NULL);

    buf = (bloom_filter_buf_t *)st_malloc(sizeof(bloom_filter_buf_t));
    if (buf == NULL) {
        ST_WARNING("Failed to st_malloc bloom filter buffer.");
        return NULL;
    }
    memset(buf, 0, sizeof(bloom_filter_buf_t));

    buf->hash_vals = (hash_t *)st_malloc(sizeof(hash_t)
            * blm_flt->blm_flt_opt.num_hashes);
    if (buf->hash_vals == NULL) {
        ST_WARNING("Failed to st_malloc hash_vals");
        goto ERR;
    }

    buf->unit_bits = get_num_unit_bits(blm_flt);

    return buf;

ERR:
    safe_bloom_filter_buf_destroy(buf);
    return NULL;
}

void bloom_filter_destroy(bloom_filter_t *blm_flt)
{
    int o;

    if (blm_flt == NULL) {
        return;
    }

    if (blm_flt->nghashes != NULL) {
        for (o = 0; o < blm_flt->blm_flt_opt.max_order; o++) {
            safe_ngram_hash_destroy(blm_flt->nghashes[o]);
        }
        safe_st_free(blm_flt->nghashes);
    }

    safe_st_free(blm_flt->cells);

    safe_vocab_destroy(blm_flt->vocab);

    safe_st_free(blm_flt->blm_flt_opt.min_counts);
}

static int bloom_filter_init_hash(bloom_filter_t *blm_flt)
{
    int *context = NULL;
    int o;

    blm_flt->nghashes = (ngram_hash_t **)st_malloc(sizeof(ngram_hash_t*)
            * blm_flt->blm_flt_opt.max_order);
    if (blm_flt->nghashes == NULL) {
        ST_WARNING("Failed to st_malloc nghashes.");
        goto ERR;
    }

    context = (int *)st_malloc(sizeof(int) * blm_flt->blm_flt_opt.max_order);
    if (context == NULL) {
        ST_WARNING("Failed to st_malloc context.");
        goto ERR;
    }
    for (o = 0; o < blm_flt->blm_flt_opt.max_order; o++) {
        context[o] = -(blm_flt->blm_flt_opt.max_order - o - 1);
    }

    for (o = 0; o < blm_flt->blm_flt_opt.max_order; o++) {
        blm_flt->nghashes[o] = ngram_hash_create(
                context + blm_flt->blm_flt_opt.max_order - o - 1, o + 1);
        if (blm_flt->nghashes[o] == NULL) {
            ST_WARNING("Failed to ngram_hash_create for [%d]-gram.", o);
            goto ERR;
        }
    }

    safe_st_free(context);

    return 0;

ERR:
    safe_st_free(context);
    return -1;
}

bloom_filter_t* bloom_filter_create(bloom_filter_opt_t *blm_flt_opt,
        vocab_t *vocab)
{
    bloom_filter_t *blm_flt = NULL;

    ST_CHECK_PARAM(blm_flt_opt == NULL || vocab == NULL, NULL);

    blm_flt = (bloom_filter_t *)st_malloc(sizeof(bloom_filter_t));
    if (blm_flt == NULL) {
        ST_WARNING("Failed to st_malloc bloom filter.");
        return NULL;
    }
    memset(blm_flt, 0, sizeof(bloom_filter_t));

    blm_flt->blm_flt_opt = *blm_flt_opt;

    blm_flt->vocab = vocab_dup(vocab);

    blm_flt->cells = (unsigned char *)st_malloc(get_num_cells(blm_flt));
    if (blm_flt->cells == NULL) {
        ST_WARNING("Failed to st_malloc cells");
        goto ERR;
    }
    memset(blm_flt->cells, 0, get_num_cells(blm_flt));

    if (bloom_filter_init_hash(blm_flt) < 0) {
        ST_WARNING("Failed to bloom_filter_init_hash.");
        goto ERR;
    }

    return blm_flt;

ERR:
    safe_bloom_filter_destroy(blm_flt);
    return NULL;
}

#define NUM_CELL_LINE 16
#define CELL_LINE_VAL_FMT "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d"
#define CELL_LINE_VAL_PTR(vals) (vals), (vals) + 1, (vals) + 2, (vals) + 3, \
                            (vals) + 4, (vals) + 5, (vals) + 6, (vals) + 7, \
                            (vals) + 8, (vals) + 9, (vals) + 10, (vals) + 11, \
                            (vals) + 12, (vals) + 13, (vals) + 14, (vals) + 15
#define CELL_LINE_VAL(vals) (vals)[0], (vals)[1], (vals)[2], (vals)[3], \
                            (vals)[4], (vals)[5], (vals)[6], (vals)[7], \
                            (vals)[8], (vals)[9], (vals)[10], (vals)[11], \
                            (vals)[12], (vals)[13], (vals)[14], (vals)[15]

static int bloom_filter_load_header(bloom_filter_t *blm_flt,
        FILE *fp, bloom_filter_format_t *fmt, FILE *fo_info)
{
    union {
        char str[4];
        int magic_num;
    } flag;

    int version;
    size_t capacity;
    int num_hashes;
    int max_order;
    int *min_counts = NULL;

    char str[MAX_LINE_LEN];
    connlm_fmt_t connlm_fmt;

    ST_CHECK_PARAM((blm_flt == NULL && fo_info == NULL) || fp == NULL
            || fmt == NULL, -1);

    if (fread(&flag.magic_num, sizeof(int), 1, fp) != 1) {
        ST_WARNING("Failed to load magic num.");
        return -1;
    }

    *fmt = BF_FMT_UNKNOWN;
    if (strncmp(flag.str, "    ", 4) == 0) {
        *fmt = BF_FMT_TXT;
    } else if (BLOOM_FILTER_MAGIC_NUM != flag.magic_num) {
        ST_WARNING("magic num wrong.");
        return -2;
    }

    if (*fmt != BF_FMT_TXT) {
        if (fread(&version, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read version.");
            return -1;
        }

        if (version > CONNLM_FILE_VERSION) {
            ST_WARNING("Too high file versoin[%d], "
                    "please upgrade connlm toolkit", version);
            return -1;
        }

        if (version < 9) {
            ST_WARNING("File versoin[%d] less than 9 is not supported, "
                    "please downgrade connlm toolkit", version);
            return -1;
        } else if (version == 11) {
            ST_WARNING("File versoin 11 is not supported, "
                    "please downgrade connlm toolkit");
            return -1;
        }

        if (version >= 12) {
            if (fread(fmt, sizeof(bloom_filter_format_t), 1, fp) != 1) {
                ST_WARNING("Failed to read fmt.");
                goto ERR;
            }
        } else {
            *fmt = BF_FMT_BIN;
        }

        if (fread(&capacity, sizeof(size_t), 1, fp) != 1) {
            ST_WARNING("Failed to read capacity.");
            goto ERR;
        }

        if (fread(&num_hashes, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read num_hashes.");
            goto ERR;
        }

        if (fread(&max_order, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read max_order.");
            goto ERR;
        }

        min_counts = (int *)st_malloc(sizeof(int) * max_order);
        if (min_counts == NULL) {
            ST_WARNING("Failed to st_malloc min_counts.");
            goto ERR;
        }

        if (fread(min_counts, sizeof(int), max_order, fp) != max_order) {
            ST_WARNING("Failed to read min_counts.");
            goto ERR;
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
            goto ERR;
        }

        if (st_readline(fp, "Num Hashes: %d", &num_hashes) != 1) {
            ST_WARNING("Failed to parse num_hashes.[%d]", num_hashes);
            goto ERR;
        }

        if (st_readline(fp, "Max Order: %d", &max_order) != 1) {
            ST_WARNING("Failed to parse max_order.[%d]", max_order);
            goto ERR;
        }

        if (st_readline(fp, "Min-Counts: %"xSTR(MAX_LINE_LEN)"s", str) != 1) {
            ST_WARNING("Failed to parse min_counts.[%s]", str);
            goto ERR;
        }

        min_counts = (int *)st_malloc(sizeof(int) * max_order);
        if (min_counts == NULL) {
            ST_WARNING("Failed to st_malloc min_counts.");
            goto ERR;
        }

        if (str2intlist(str, min_counts, max_order) < 0) {
            ST_WARNING("Failed to str2intlist for min_counts[%s].", str);
            goto ERR;
        }
    }

    if (blm_flt != NULL) {
        blm_flt->blm_flt_opt.capacity = capacity;
        blm_flt->blm_flt_opt.num_hashes = num_hashes;
        blm_flt->blm_flt_opt.max_order = max_order;

        blm_flt->blm_flt_opt.min_counts = (int *)st_malloc(
                sizeof(int) * max_order);
        if (blm_flt->blm_flt_opt.min_counts == NULL) {
            ST_WARNING("Failed to st_malloc min_counts for blm_flt_opt.");
            goto ERR;
        }
        memcpy(blm_flt->blm_flt_opt.min_counts, min_counts,
                sizeof(int) * max_order);
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "<BLOOM_FILTER>\n");
        fprintf(fo_info, "Version: %d\n", version);
        fprintf(fo_info, "Capacity: %zu\n", capacity);
        fprintf(fo_info, "Num Hashes: %d\n", num_hashes);
        fprintf(fo_info, "Max Order: %d\n", max_order);
        fprintf(fo_info, "Min-Counts: %s\n",
                intlist2str(min_counts, max_order, str, MAX_LINE_LEN));
        fprintf(fo_info, "Format: %s\n", fmt2str(*fmt));
    }

    if (vocab_load_header((blm_flt == NULL) ? NULL : &(blm_flt->vocab),
                version, fp, &connlm_fmt, fo_info) < 0) {
        ST_WARNING("Failed to vocab_load_header.");
        goto ERR;
    }
    if (connlm_fmt_is_bin(connlm_fmt) != blm_flt_fmt_is_bin(*fmt)) {
        ST_WARNING("Both binary and text format in one file.");
        goto ERR;
    }

    if (fo_info != NULL) {
        fflush(fo_info);
    }

    safe_st_free(min_counts);

    return version;

ERR:
    safe_st_free(min_counts);
    return -1;
}

static int bloom_filter_load_compress_cells(bloom_filter_t *blm_flt, FILE *fp)
{
    size_t i;
    size_t num_cells;
    uint64_t num_zeros;

    uint8_t c;

    ST_CHECK_PARAM(blm_flt == NULL || fp == NULL, -1);

    i = 0;
    num_cells = get_num_cells(blm_flt);
    while (i < num_cells) {
        if (fread(&c, sizeof(uint8_t), 1, fp) != 1) {
            ST_WARNING("Failed to read cell[%zu].", i);
            return -1;
        }

        if (c != 0) {
            blm_flt->cells[i] = c;
            ++i;
            continue;
        }

        if (st_varint_decode_stream_uint64(fp, &num_zeros) < 0) {
            ST_WARNING("Failed to st_varint_decode_stream_uint64[%zu]", i);
            return -1;
        }

        assert(i + num_zeros <= num_cells);
        memset(blm_flt->cells + i, 0, sizeof(uint8_t) * num_zeros);
        i += num_zeros;
    }

    return 0;
}

static int bloom_filter_load_body(bloom_filter_t *blm_flt, int version,
        FILE *fp, bloom_filter_format_t fmt)
{
    int n;

    size_t i;
    char buf[sizeof(int) * CHAR_BIT + 1];
    char str[MAX_LINE_LEN];
    int vals[NUM_CELL_LINE];
    char *p;
    int j, b;
    int unit_bits;

    ST_CHECK_PARAM(blm_flt == NULL || fp == NULL, -1);

    blm_flt->cells = NULL;

    if (blm_flt_fmt_is_bin(fmt)) {
        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read magic num.");
            goto ERR;
        }

        if (n != -BLOOM_FILTER_MAGIC_NUM) {
            ST_WARNING("Magic num error.");
            goto ERR;
        }

        blm_flt->cells = (unsigned char *)st_malloc(get_num_cells(blm_flt));
        if (blm_flt->cells == NULL) {
            ST_WARNING("Failed to st_malloc cells.");
            goto ERR;
        }

        if (version >= 10 && (fmt & BF_FMT_COMPRESSED)) {
            if (bloom_filter_load_compress_cells(blm_flt, fp) < 0) {
                ST_WARNING("Failed to bloom_filter_load_compress_cells.");
                return -1;
            }
        } else {
            if (fread(blm_flt->cells, sizeof(unsigned char),
                        get_num_cells(blm_flt), fp) != get_num_cells(blm_flt)) {
                ST_WARNING("Failed to read cells.");
                goto ERR;
            }
        }
    } else {
        if (st_readline(fp, "<BLOOM_FILTER-DATA>") != 0) {
            ST_WARNING("body flag error.");
            goto ERR;
        }

        blm_flt->cells = (unsigned char *)st_malloc(get_num_cells(blm_flt));
        if (blm_flt->cells == NULL) {
            ST_WARNING("Failed to st_malloc cells.");
            goto ERR;
        }
        memset(blm_flt->cells, 0, get_num_cells(blm_flt));

        if (st_readline(fp, " Cells:") != 0) {
            ST_WARNING("cells flag error.");
            goto ERR;
        }

        unit_bits = get_num_unit_bits(blm_flt);
        for (i = 0; i < blm_flt->blm_flt_opt.capacity / NUM_CELL_LINE; i++) {
            if (st_readline(fp, "\t%*zu:\t"CELL_LINE_VAL_FMT"\n",
                        CELL_LINE_VAL_PTR(vals)) != NUM_CELL_LINE) {
                ST_WARNING("Failed to parse cells.");
                goto ERR;
            }
            for (j = 0; j < NUM_CELL_LINE; j++) {
                if (vals[j] != 0) {
                    st_nbit_set(blm_flt->cells, unit_bits,
                            i * NUM_CELL_LINE + j, vals[j]);
                }
            }
        }
        if (st_readline(fp, "\t%*zu:\t%s\n", str) != 1) {
            ST_WARNING("Failed to parse last line cells.");
            goto ERR;
        }
        p = str;
        for (j = 0; j < blm_flt->blm_flt_opt.capacity % NUM_CELL_LINE; j++) {
            b = 0;
            while (*p != '\0' && *p != ' ') {
                if (b > sizeof(int) * CHAR_BIT) {
                    ST_WARNING("Too large slot val");
                    goto ERR;
                }
                buf[b] = *p;
                ++p;
                ++b;
            }
            buf[b] = '\0';
            if (j < blm_flt->blm_flt_opt.capacity % NUM_CELL_LINE - 1) {
                if (*p == '\0') {
                    ST_WARNING("Not enough slots");
                    goto ERR;
                }
                ++p;
            } else {
                if (*p != '\0') {
                    ST_WARNING("Too many slots");
                    goto ERR;
                }
            }

            vals[j] = atoi(buf);
            if (vals[j] != 0) {
                st_nbit_set(blm_flt->cells, unit_bits,
                        i * NUM_CELL_LINE + j, vals[j]);
            }
        }
    }

    if (vocab_load_body(blm_flt->vocab, version, fp,
            blm_flt_fmt_is_bin(fmt) ? CONN_FMT_BIN : CONN_FMT_TXT) < 0) {
        ST_WARNING("Failed to vocab_load_body.");
        return -1;
    }

    return 0;
ERR:
    safe_st_free(blm_flt->cells);
    return -1;
}

static int bloom_filter_save_header(bloom_filter_t *blm_flt, FILE *fp,
        bloom_filter_format_t fmt)
{
    char str[MAX_LINE_LEN];
    int n;

    ST_CHECK_PARAM(blm_flt == NULL || fp == NULL, -1);

    if (blm_flt_fmt_is_bin(fmt)) {
        if (fwrite(&BLOOM_FILTER_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        n = CONNLM_FILE_VERSION;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write version.");
            return -1;
        }

        if (fwrite(&fmt, sizeof(bloom_filter_format_t), 1, fp) != 1) {
            ST_WARNING("Failed to write fmt.");
            return -1;
        }

        if (fwrite(&blm_flt->blm_flt_opt.capacity, sizeof(size_t), 1, fp) != 1) {
            ST_WARNING("Failed to write capacity.");
            return -1;
        }

        if (fwrite(&blm_flt->blm_flt_opt.num_hashes, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write num_hashes.");
            return -1;
        }

        if (fwrite(&blm_flt->blm_flt_opt.max_order, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write max_order.");
            return -1;
        }

        if (fwrite(blm_flt->blm_flt_opt.min_counts, sizeof(int),
                   blm_flt->blm_flt_opt.max_order, fp)
                != blm_flt->blm_flt_opt.max_order) {
            ST_WARNING("Failed to write min_counts.");
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

        if (fprintf(fp, "Num Hashes: %d\n", blm_flt->blm_flt_opt.num_hashes) < 0) {
            ST_WARNING("Failed to fprintf num_hashes.");
            return -1;
        }

        if (fprintf(fp, "Max Order: %d\n", blm_flt->blm_flt_opt.max_order) < 0) {
            ST_WARNING("Failed to fprintf max_order.");
            return -1;
        }

        if (fprintf(fp, "Min-Counts: %s\n",
                   intlist2str(blm_flt->blm_flt_opt.min_counts,
                        blm_flt->blm_flt_opt.max_order,
                        str, MAX_LINE_LEN)) < 0) {
            ST_WARNING("Failed to fprintf min_counts.");
            return -1;
        }
    }

    if (vocab_save_header(blm_flt->vocab, fp,
            blm_flt_fmt_is_bin(fmt) ? CONN_FMT_BIN : CONN_FMT_TXT) < 0) {
        ST_WARNING("Failed to vocab_save_header.");
        return -1;
    }

    return 0;
}

static int write_zeros_cells(uint64_t num_zeros, FILE *fp)
{
    uint8_t zero = 0;

    if (fwrite(&zero, sizeof(uint8_t), 1, fp) != 1) {
        ST_WARNING("Failed to write zero cell.");
        return -1;
    }
    if (st_varint_encode_stream_uint64(num_zeros, fp) < 0) {
        ST_WARNING("Failed to st_varint_encode_stream_uint64[%"PRIu64"]",
                num_zeros);
        return -1;
    }

    return 0;
}

static int bloom_filter_save_compress_cells(bloom_filter_t *blm_flt, FILE *fp)
{
    size_t i;
    size_t num_cells;
    uint64_t num_zeros;

    ST_CHECK_PARAM(blm_flt == NULL || fp == NULL, -1);

    num_zeros = 0;
    num_cells = get_num_cells(blm_flt);
    for (i = 0; i < num_cells; i++) {
        if (blm_flt->cells[i] == 0) {
            ++num_zeros;
            continue;
        }

        if (num_zeros > 0) {
            if (write_zeros_cells(num_zeros, fp) < 0) {
                ST_WARNING("Failed to write_zeros_cells[%zu].", i);
                return -1;
            }
            num_zeros = 0;
        }

        if (fwrite(blm_flt->cells + i, sizeof(uint8_t), 1, fp) != 1) {
            ST_WARNING("Failed to write non-zero cell[%zu].", i);
            return -1;
        }
    }
    if (num_zeros > 0) {
        if (write_zeros_cells(num_zeros, fp) < 0) {
            ST_WARNING("Failed to write_zeros_cells[%zu].", i);
            return -1;
        }
    }

    return 0;
}

static int bloom_filter_save_body(bloom_filter_t *blm_flt, FILE *fp,
        bloom_filter_format_t fmt)
{
    int n;

    size_t i;
    int vals[NUM_CELL_LINE];
    int j;
    int unit_bits;

    ST_CHECK_PARAM(blm_flt == NULL || fp == NULL, -1);

    if (blm_flt_fmt_is_bin(fmt)) {
        n = -BLOOM_FILTER_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        if (fmt & BF_FMT_COMPRESSED) {
            if (bloom_filter_save_compress_cells(blm_flt, fp) < 0) {
                ST_WARNING("Failed to bloom_filter_save_compress_cells.");
                return -1;
            }
        } else {
            if (fwrite(blm_flt->cells, sizeof(unsigned char),
                        get_num_cells(blm_flt), fp) != get_num_cells(blm_flt)) {
                ST_WARNING("Failed to write cells.");
                return -1;
            }
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
        unit_bits = get_num_unit_bits(blm_flt);
        for (i = 0; i < blm_flt->blm_flt_opt.capacity / NUM_CELL_LINE; i++) {
            for (j = 0; j < NUM_CELL_LINE; j++) {
                vals[j] = st_nbit_get(blm_flt->cells, unit_bits,
                        i * NUM_CELL_LINE + j);
            }
            if (fprintf(fp, "\t%zu:\t"CELL_LINE_VAL_FMT"\n",
                        i * NUM_CELL_LINE, CELL_LINE_VAL(vals)) < 0) {
                ST_WARNING("Failed to fprintf cells[%zu].", i * NUM_CELL_LINE);
                return -1;
            }
        }
        if (fprintf(fp, "\t%zu:\t", i * NUM_CELL_LINE) < 0) {
            ST_WARNING("Failed to fprintf last line cells.");
            return -1;
        }
        for (j = 0; j < blm_flt->blm_flt_opt.capacity % NUM_CELL_LINE - 1; j++) {
            if (fprintf(fp, "%d ", st_nbit_get(blm_flt->cells, unit_bits,
                            i * NUM_CELL_LINE + j)) < 0) {
                ST_WARNING("Failed to fprintf last line cells.");
                return -1;
            }
        }
        if (fprintf(fp, "%d", st_nbit_get(blm_flt->cells, unit_bits,
                        blm_flt->blm_flt_opt.capacity - 1)) < 0) {
            ST_WARNING("Failed to fprintf last line cells.");
            return -1;
        }
    }

    if (vocab_save_body(blm_flt->vocab, fp,
            blm_flt_fmt_is_bin(fmt) ? CONN_FMT_BIN : CONN_FMT_TXT) < 0) {
        ST_WARNING("Failed to vocab_save_body.");
        return -1;
    }

    return 0;
}

int bloom_filter_save(bloom_filter_t *blm_flt, FILE *fp,
        bloom_filter_format_t fmt)
{
    ST_CHECK_PARAM(blm_flt == NULL || fp == NULL, -1);

    if (bloom_filter_save_header(blm_flt, fp, fmt) < 0) {
        ST_WARNING("Failed to bloom_filter_save_header.");
        return -1;
    }

    if (bloom_filter_save_body(blm_flt, fp, fmt) < 0) {
        ST_WARNING("Failed to bloom_filter_save_body.");
        return -1;
    }

    return 0;
}

bloom_filter_t* bloom_filter_load(FILE *fp)
{
    bloom_filter_t *blm_flt = NULL;
    int version;
    bloom_filter_format_t fmt;

    ST_CHECK_PARAM(fp == NULL, NULL);

    blm_flt = (bloom_filter_t *)st_malloc(sizeof(bloom_filter_t));
    if (blm_flt == NULL) {
        ST_WARNING("Failed to st_malloc bloom_filter_t");
        goto ERR;
    }
    memset(blm_flt, 0, sizeof(bloom_filter_t));

    version = bloom_filter_load_header(blm_flt, fp, &fmt, NULL);
    if (version < 0) {
        ST_WARNING("Failed to bloom_filter_load_header.");
        goto ERR;
    }

    if (bloom_filter_load_body(blm_flt, version, fp, fmt) < 0) {
        ST_WARNING("Failed to bloom_filter_load_body.");
        goto ERR;
    }

    if (bloom_filter_init_hash(blm_flt) < 0) {
        ST_WARNING("Failed to bloom_filter_init_hash.");
        goto ERR;
    }

    return blm_flt;

ERR:
    safe_bloom_filter_destroy(blm_flt);
    return NULL;
}

int bloom_filter_print_info(FILE *fp, FILE *fo_info)
{
    bloom_filter_format_t fmt;

    ST_CHECK_PARAM(fp == NULL || fo_info == NULL, -1);

    if (bloom_filter_load_header(NULL, fp, &fmt, fo_info) < 0) {
        ST_WARNING("Failed to bloom_filter_load_header.");
        return -1;
    }

    return 0;
}

#define HALF_BITS (sizeof(hash_t) / 2 * CHAR_BIT)

static inline hash_t dup_low_bits(hash_t val)
{
    return val | (val << HALF_BITS);
}

static inline void split_hash(hash_t val, hash_t *val_a, hash_t *val_b)
{
    static hash_t mask = (((hash_t)1) << HALF_BITS) - 1;

    *val_a = (hash_t)(val >> HALF_BITS);
    *val_b = (hash_t)(val & mask);

    *val_a = dup_low_bits(*val_a);
    *val_b = dup_low_bits(*val_b);
}

static int bloom_filter_hash(bloom_filter_t *blm_flt, int *words, int n,
        hash_t *hash_vals)
{
    hash_t val;
    hash_t val_a, val_b;

    int k;

    ST_CHECK_PARAM(blm_flt == NULL || words == NULL || n <= 0, -1);

    assert (n <= blm_flt->blm_flt_opt.max_order);

    val = ngram_hash(blm_flt->nghashes[n - 1], words, n, n - 1, n);

    split_hash(val, &val_a, &val_b);

    for (k = 0; k < blm_flt->blm_flt_opt.num_hashes; k++) {
        hash_vals[k] = val_a + (k + 1) * val_b;
        hash_vals[k] %= blm_flt->blm_flt_opt.capacity;
    }

    return 0;
}

int bloom_filter_add(bloom_filter_t *blm_flt, bloom_filter_buf_t *buf,
        int *words, int n)
{
    int k;
    int cnt;

    ST_CHECK_PARAM(blm_flt == NULL || buf == NULL
            || words == NULL || n <= 0, -1);

    if (n > blm_flt->blm_flt_opt.max_order) {
        ST_WARNING("order[%d] of n-gram must be less than max_order[%d].",
                n, blm_flt->blm_flt_opt.max_order);
        return -1;
    }

    if (bloom_filter_hash(blm_flt, words, n, buf->hash_vals) < 0) {
        ST_WARNING("Failed to bloom_filter_hash.");
        return -1;
    }

    for (k = 0; k < blm_flt->blm_flt_opt.num_hashes; k++) {
        cnt = st_nbit_get(blm_flt->cells, buf->unit_bits, buf->hash_vals[k]);
        if (cnt < blm_flt->blm_flt_opt.min_counts[n - 1]) {
            st_nbit_set(blm_flt->cells, buf->unit_bits,
                    buf->hash_vals[k], cnt + 1);
        }
    }

    return 0;
}

bool bloom_filter_lookup(bloom_filter_t *blm_flt, bloom_filter_buf_t *buf,
        int *words, int n)
{
    int k;

    ST_CHECK_PARAM(blm_flt == NULL || buf == NULL
            || words == NULL || n <= 0, false);

    if (n > blm_flt->blm_flt_opt.max_order) {
        return false;
    }

    if (bloom_filter_hash(blm_flt, words, n, buf->hash_vals) < 0) {
        ST_WARNING("Failed to bloom_filter_hash.");
        return false;
    }

    for (k = 0; k < blm_flt->blm_flt_opt.num_hashes; k++) {
        if (st_nbit_get(blm_flt->cells, buf->unit_bits, buf->hash_vals[k])
                < blm_flt->blm_flt_opt.min_counts[n - 1]) {
            return false;
        }
    }

    return true;
}

#define EPOCH_SIZE 1000

int bloom_filter_build(bloom_filter_t *blm_flt, FILE *text_fp)
{
    connlm_egs_t egs = {
        .words = NULL,
        .size = 0,
        .capacity = 0,
    };

    bloom_filter_buf_t *buf = NULL;

    int max_order;

    int sent_start = 0;
    int pos;
    int order;

    ST_CHECK_PARAM(blm_flt == NULL || text_fp == NULL, -1);

    max_order = blm_flt->blm_flt_opt.max_order;

    buf = bloom_filter_buf_create(blm_flt);
    if (buf == NULL) {
        ST_WARNING("Failed to bloom_filter_buf_create.");
        goto ERR;
    }

    while (!feof(text_fp)) {
        if (connlm_egs_read(&egs, NULL, EPOCH_SIZE,
                    text_fp, blm_flt->vocab, NULL, true) < 0) {
            ST_WARNING("Failed to connlm_egs_read.");
            goto ERR;
        }

        for (pos = 0; pos < egs.size; pos++) {
            if (egs.words[pos] == bos_id(blm_flt)) {
                sent_start = pos;
            }

            order = pos - sent_start + 1;
            order = min(order, max_order);

            while (order > 0) {
                if (bloom_filter_add(blm_flt, buf,
                            egs.words + pos - order + 1, order) < 0) {
                    ST_WARNING("Failed to bloom_filter_add.");
                    goto ERR;
                }
                --order;
            }
        }
    }

    safe_bloom_filter_buf_destroy(buf);
    connlm_egs_destroy(&egs);

    return 0;

ERR:
    safe_bloom_filter_buf_destroy(buf);
    connlm_egs_destroy(&egs);

    return -1;
}

float bloom_filter_load_factor(bloom_filter_t *blm_flt)
{
    size_t i;
    size_t n;
    int unit_bits;

    ST_CHECK_PARAM(blm_flt == NULL, -1);

    unit_bits = get_num_unit_bits(blm_flt);

    n = 0;
    for (i = 0; i < blm_flt->blm_flt_opt.capacity; i++) {
        if (st_nbit_get(blm_flt->cells, unit_bits, i) > 0) {
            ++n;
        }
    }

    return n / (float)(blm_flt->blm_flt_opt.capacity);
}
