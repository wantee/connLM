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

#ifndef  _CONNLM_BLOOM_FILTER_H_
#define  _CONNLM_BLOOM_FILTER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#include <connlm/config.h>

#include "vocab.h"
#include "ngram_hash.h"

/** @defgroup g_bloom_filter N-gram Bloom Filter
 * A Bloom Filter to determine whether a n-gram is present.
 */

/**
 * Storage Format for Bloom Filter
 * @ingroup g_bloom_filter
 */
typedef enum _bloom_filter_format_t_ {
    BF_FMT_UNKNOWN     = 0x0000, /**< Unknown format. */
    BF_FMT_TXT         = 0x0001, /**< Text format. */
    BF_FMT_BIN         = 0x0002, /**< (flat) Binary format. */
    BF_FMT_COMPRESSED  = 0x0004, /**< Compressed (binary) format. */
} bloom_filter_format_t;

#define blm_flt_fmt_is_bin(fmt) ((fmt) > 1)

/**
 * Parse the string representation to a bloom_filter_format_t
 * @ingroup g_bloom_filter
 * @param[in] str string to be parsed.
 * @return the format, BF_FMT_UNKNOWN if string not valid or any other error.
 */
bloom_filter_format_t bloom_filter_format_parse(const char *str);

/**
 * Options for bloom_filter.
 * @ingroup g_bloom_filter
 */
typedef struct _bloom_filter_opt_t_ {
    size_t capacity; /**< number of bits in this filter. */
    int num_hashes; /**< number of hashes. */

    int max_order; /**< max order of grams. */
    int *min_counts; /**< min-count per every ngram order. */
} bloom_filter_opt_t;

/**
 * Load bloom filter option.
 * @ingroup g_bloom_filter
 * @param[out] blm_flt_opt options to be loaded.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @return non-zero value if any error.
 */
int bloom_filter_load_opt(bloom_filter_opt_t *blm_flt_opt,
        st_opt_t *opt, const char *sec_name);

/**
 * Bloom Filter
 * @ingroup g_bloom_filter
 */
typedef struct _bloom_filter_t_ {
    unsigned char *cells; /**< cell store truth value for elements. */

    vocab_t *vocab; /**< vocabulary. */

    ngram_hash_t **nghashes; /**< ngram hash for every order. */

    bloom_filter_opt_t blm_flt_opt; /**< options. */
} bloom_filter_t;

/**
 * Destroy a bloom filter and set the pointer to NULL.
 * @ingroup g_bloom_filter
 * @param[in] ptr pointer to bloom_filter_t.
 */
#define safe_bloom_filter_destroy(ptr) do {\
    if((ptr) != NULL) {\
        bloom_filter_destroy(ptr);\
        safe_st_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a bloom filter.
 * @ingroup g_bloom_filter
 * @param[in] blm_flt bloom filter to be destroyed.
 */
void bloom_filter_destroy(bloom_filter_t *blm_flt);

/**
 * Create a bloom filter.
 * @ingroup g_bloom_filter
 * @param[in] blm_flt_opt options for bloom filter.
 * @param[in] vocab vocabulary.
 * @return bloom_filter on success, otherwise NULL.
 */
bloom_filter_t* bloom_filter_create(bloom_filter_opt_t *blm_flt_opt,
        vocab_t *vocab);

/**
 * Load a bloom filter from file stream.
 * @ingroup g_bloom_filter
 * @param[in] fp file stream loaded from.
 * @return a bloom filter, NULL if error.
 */
bloom_filter_t* bloom_filter_load(FILE *fp);

/**
 * Save a bloom filter.
 * @ingroup g_bloom_filter
 * @param[in] blm_flt bloom filter to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] fmt storage format.
 * @return non-zero value if any error.
 */
int bloom_filter_save(bloom_filter_t *blm_flt, FILE *fp,
        bloom_filter_format_t fmt);

/**
 * Print info of a bloom filter file stream.
 * @ingroup g_bloom_filter
 * @param[in] model_fp file stream read from.
 * @param[in] fo file stream print info to.
 * @return non-zero value if any error.
 */
int bloom_filter_print_info(FILE *model_fp, FILE *fo);

/**
 * Add an ngram a bloom filter.
 * @ingroup g_bloom_filter
 * @param[in] blm_flt the bloom filter.
 * @param[in] text_fp file stream of text file.
 * @return non-zero value if any error.
 */
int bloom_filter_build(bloom_filter_t *blm_flt, FILE *text_fp);

/**
 * Bloom Filter Data Buffer
 * @ingroup g_bloom_filter
 */
typedef struct _bloom_filter_buffer_t_ {
    hash_t *hash_vals; /**< buffer stored (num_hashes) hash values. */
    int unit_bits; /**< number of bits in a slot of hash. */
} bloom_filter_buf_t;

/**
 * Destroy a bloom filter buffer and set the pointer to NULL.
 * @ingroup g_bloom_filter
 * @param[in] ptr pointer to bloom_filter_buf_t.
 */
#define safe_bloom_filter_buf_destroy(ptr) do {\
    if((ptr) != NULL) {\
        bloom_filter_buf_destroy(ptr);\
        safe_st_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a bloom filter buffer.
 * @ingroup g_bloom_filter
 * @param[in] buf bloom filter buffer to be destroyed.
 */
void bloom_filter_buf_destroy(bloom_filter_buf_t *buf);

/**
 * Create a bloom filter buffer.
 * @ingroup g_bloom_filter
 * @param[in] blm_flt bloom filter.
 * @return bloom_filter_buf on success, otherwise NULL.
 */
bloom_filter_buf_t* bloom_filter_buf_create(bloom_filter_t *blm_flt);

/**
 * Add an ngram a bloom filter.
 * @ingroup g_bloom_filter
 * @param[in] blm_flt the bloom filter.
 * @param[in] buf a bloom filter buffer.
 * @param[in] words words in ngram.
 * @param[in] n number of words.
 * @return non-zero value if any error.
 */
int bloom_filter_add(bloom_filter_t *blm_flt, bloom_filter_buf_t *buf,
        int *words, int n);

/**
 * Add an ngram a bloom filter.
 * @ingroup g_bloom_filter
 * @param[in] blm_flt the bloom filter.
 * @param[in] buf a bloom filter buffer.
 * @param[in] words words in ngram.
 * @param[in] n number of words.
 * @return true if exists, false otherwise.
 */
bool bloom_filter_lookup(bloom_filter_t *blm_flt, bloom_filter_buf_t *buf,
        int *words, int n);

/**
 * Compute the load factor for bloom filter.
 * @ingroup g_bloom_filter
 * @param[in] blm_flt the bloom filter.
 * @return the load factor
 */
float bloom_filter_load_factor(bloom_filter_t *blm_flt);

#ifdef __cplusplus
}
#endif

#endif
