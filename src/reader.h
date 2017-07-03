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

#ifndef  _CONNLM_READER_H_
#define  _CONNLM_READER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>

#include <stutils/st_semaphore.h>

#include <connlm/config.h>
#include "vocab.h"

/** @defgroup g_reader Samples Reader
 * Reader to read samples from source text files.
 */

/**
 * Startistics for single thread.
 * @ingroup g_reader
 */
typedef struct _thread_statistics_t_ {
    count_t words; /**< total number of words trained in this thread. */
    count_t sents; /**< total number of sentences trained in this thread. */
    double logp; /**< total log probability in this thread. */
} thr_stat_t;

/**
 * Options for reader.
 * @ingroup g_reader
 */
typedef struct _reader_opt_t_ {
    int epoch_size;  /**< number sentences read one time per thread. */
    int mini_batch;  /**< mini-batch size. */
    unsigned int rand_seed;   /**< seed for random function. */
    bool shuffle;             /**< whether shuffle the sentences. */
    bool drop_empty_line;     /**< whether drop empty lines in text. */
    char debug_file[MAX_DIR_LEN]; /**< file to print out debug infos. */
} reader_opt_t;

/**
 * A bunch of word
 * @ingroup g_reader
 */
typedef struct _word_pool_t_ {
    int *words; /**< word ids. */
    int size; /**< size of words. */
    int capacity; /**< capacity of words. */
    int *row_starts; /**< start index of each row in mini-batch. */
    int batch_size; /**< batche size, number of rows. */
    struct _word_pool_t_ *next; /**< pointer to the next list element. */
} word_pool_t;

/**
 * Destroy a word_pool.
 * @ingroup g_reader
 * @param[in] pool pool to be destroyed.
 */
void word_pool_destroy(word_pool_t *wp);

/**
 * Read words into pool.
 * @ingroup g_reader
 * @param[in] wp word pool.
 * @param[out] sent_ends contains postion of \</s\>s, if not NULL.
 * @param[in] epoch_size number sents read one time.
 * @param[in] text_fp text file.
 * @param[in] vocab vocab.
 * @param[out] oovs number of oovs, if not NULL.
 * @param[in] drop_empty_line whether drop the empty lines.
 * @return non-zero value if any error.
 */
int word_pool_read(word_pool_t *wp, int *sent_ends,
        int epoch_size, FILE *text_fp, vocab_t *vocab, int *oovs,
        bool drop_empty_line);

/**
 * Load reader option.
 * @ingroup g_reader
 * @param[out] reader_opt options loaded.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @return non-zero value if any error.
 */
int reader_load_opt(reader_opt_t *reader_opt, st_opt_t *opt,
        const char *sec_name);

/**
 * Sample reader.
 * @ingroup g_reader
 */
typedef struct _reader_t_ {
    reader_opt_t opt; /**< reader option. */
    char text_file[MAX_DIR_LEN]; /**< text file name. */
    vocab_t *vocab; /**< vocabulary. */
    int num_thrs; /**< number of working threads. */

    word_pool_t *full_wp_head; /**< head of list for word pool filled with data. */
    word_pool_t *full_wp_tail; /**< tail of list for word pool filled with data. */
    word_pool_t *empty_wps; /**< list for word pool with data consumed. */
    st_sem_t sem_full; /**< semaphore for full word pool list. */
    st_sem_t sem_empty; /**< semaphore for empty word pool list. */
    pthread_mutex_t full_wp_lock; /**< lock for full word pool list. */
    pthread_mutex_t empty_wp_lock; /**< lock for empty word pool list. */

    FILE *fp_debug; /**< file pointer to print out debug info. */
    pthread_mutex_t fp_debug_lock; /**< lock for fp_debug_log. */

    count_t words; /**< total words readed. */
    count_t sents; /**< total sentences readed. */
    count_t oovs; /**< total OOVs readed. */

    pthread_t tid; /**< thread id for read thread. */
    unsigned int random; /**< random seed. */
    thr_stat_t *stats; /**< random seed. */
    int *err; /**< error indicator. */
} reader_t;

/**
 * Destroy a reader and set the pointer to NULL.
 * @ingroup g_reader
 * @param[in] ptr pointer to reader_t.
 */
#define safe_reader_destroy(ptr) do {\
    if((ptr) != NULL) {\
        reader_destroy(ptr);\
        safe_st_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a reader.
 * @ingroup g_reader
 * @param[in] reader reader to be destroyed.
 */
void reader_destroy(reader_t *reader);

/**
 * Create a reader.
 * @ingroup g_reader
 * @param[in] opt reader options.
 * @param[in] num_thrs number of worker threads.
 * @param[in] vocab vocabulary.
 * @param[in] text_file text file name.
 * @return reader on success, otherwise NULL.
 */
reader_t* reader_create(reader_opt_t *opt, int num_thrs,
        vocab_t *vocab, const char *text_file);

/**
 * Start reading text.
 * Will start a new thread to read text.
 * @ingroup g_reader
 * @param[in] reader reader.
 * @param[in] stats thread statistics.
 * @param[in] err global err indicator.
 * @return non-zero value if any error.
 */
int reader_read(reader_t *reader, thr_stat_t *stats, int *err);

/**
 * Wait reading thread.
 * @ingroup g_reader
 * @param[in] reader reader.
 * @return non-zero value if any error.
 */
int reader_wait(reader_t *reader);

/**
 * Get and hold a word pool in reading queue.
 * @ingroup g_reader
 * @param[in] reader reader.
 * @return NULL if there is no more data or any error.
 */
word_pool_t* reader_hold_word_pool(reader_t *reader);

/**
 * Relase a word pool to reading queue.
 * @ingroup g_reader
 * @param[in] reader reader.
 * @param[in] wp word pool.
 * @return non-zero value if any error.
 */
int reader_release_word_pool(reader_t *reader, word_pool_t* wp);

#ifdef __cplusplus
}
#endif

#endif
