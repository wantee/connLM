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
#include <pthread.h>

#include <connlm/config.h>

/** @defgroup g_bloom_filter N-gram Bloom Filter
 * A Bloom Filter to determine whether a n-gram is present.
 */

/**
 * Options for bloom_filter.
 * @ingroup g_bloom_filter
 */
typedef struct _bloom_filtererter_opt_t_ {
    size_t capacity; /**< number of bits in this filter. */
    int num_hash; /**< number of hash functions. */
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
typedef struct _bloom_filtererter_t_ {
    char *cells; /**< cell store truth value for elements. */

    int err; /**< error indicator. */

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
        safe_free(ptr);\
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
 * @return bloom_filter on success, otherwise NULL.
 */
bloom_filter_t* bloom_filter_create(bloom_filter_opt_t *blm_flt_opt);

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
 * @param[in] binary whether to use binary format.
 * @return non-zero value if any error.
 */
int bloom_filter_save(bloom_filter_t *blm_flt, FILE *fp, bool binary);

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
 * @param[in] n_thr number of worker threads.
 * @param[in] text_fp file stream of text file.
 * @return non-zero value if any error.
 */
int bloom_filter_build(bloom_filter_t *blm_flt, int n_thr, FILE *text_fp);

/**
 * Add an ngram a bloom filter.
 * @ingroup g_bloom_filter
 * @param[in] blm_flt the bloom filter.
 * @param[in] words words in ngram.
 * @param[in] n number of words.
 * @return non-zero value if any error.
 */
int bloom_filter_add(bloom_filter_t *blm_flt, int *words, int n);

/**
 * Add an ngram a bloom filter.
 * @ingroup g_bloom_filter
 * @param[in] blm_flt the bloom filter.
 * @param[in] words words in ngram.
 * @param[in] n number of words.
 * @return 1 if exists, 0 otherwise. non-zero value if any error.
 */
int bloom_filter_lookup(bloom_filter_t *blm_flt, int *words, int n);

#ifdef __cplusplus
}
#endif

#endif
