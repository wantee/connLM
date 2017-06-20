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
#include <stutils/st_string.h>
#include <stutils/st_rand.h>
#include <stutils/st_io.h>

#include "utils.h"
#include "reader.h"

#define NUM_WORD_PER_SENT 128

void word_pool_destroy(word_pool_t *wp)
{
    if (wp != NULL) {
        safe_st_free(wp->words);
        wp->capacity = 0;
        wp->size = 0;
        safe_st_free(wp->row_starts);
        wp->batch_size = 0;
    }
}

//#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-overflow"
static int word_pool_ensure_capacity(word_pool_t *wp, int capacity)
{
    ST_CHECK_PARAM(wp == NULL, -1);

    if (wp->capacity < capacity) {
        wp->words = st_realloc(wp->words, sizeof(int)*capacity);
        if (wp->words == NULL) {
            ST_WARNING("Failed to st_realloc wp->words. capacity[%d]",
                    capacity);
            return -1;
        }

        wp->capacity = capacity;
    }

    return 0;
}
//#pragma GCC diagnostic pop

static int word_pool_print(FILE *fp, pthread_mutex_t *fp_lock,
        word_pool_t *wp, vocab_t *vocab)
{
    int word;
    int i;

    ST_CHECK_PARAM(fp == NULL || fp_lock == NULL
            || wp == NULL, -1);

    i = 0;
    while (i < wp->size) {
        (void)pthread_mutex_lock(fp_lock);
        fprintf(fp, "<EGS>: ");
        while (wp->words[i] != SENT_END_ID && i < wp->size) {
            word = wp->words[i];
            if (vocab != NULL && word == vocab_get_id(vocab, SENT_START)) {
                i++;
                continue;
            }

            if (vocab != NULL) {
                fprintf(fp, "%s", vocab_get_word(vocab, word));
            } else {
                fprintf(fp, "<%d>", word);
            }
            if (i < wp->size - 1 && wp->words[i+1] != 0) {
                fprintf(fp, " ");
            }
            i++;
        }
        fprintf(fp, "\n");
        fflush(fp);
        i++;
        (void)pthread_mutex_unlock(fp_lock);
    }

    return 0;
}

static int word_pool_build_mini_batch(word_pool_t *wp, int batch_size)
{
    int i, n;
    int cnt, eos;
    int words_per_batch;

    ST_CHECK_PARAM(wp == NULL || batch_size <= 1, -1);

    if (batch_size > wp->batch_size) {
        wp->row_starts = (int *)st_realloc(wp->row_starts,
                sizeof(int) * (batch_size + 1));
        if (wp->row_starts == NULL) {
            ST_WARNING("Failed to st_realloc row_starts.");
            return -1;
        }
    }
    wp->batch_size = batch_size;

    words_per_batch = wp->size / batch_size + 1;

    wp->row_starts[0] = 0;
    n = 1;

    eos = 0;
    i = 0;
    cnt = 0;
    while (i < wp->size) {
        if (wp->words[i] == SENT_END_ID) {
            eos = i;
        }
        ++cnt;
        if (cnt >= words_per_batch) {
            wp->row_starts[n] = eos + 1;
            ++n;
            if (n >= wp->batch_size) {
                break;
            }
            cnt = i - eos;
        }

        ++i;
    }
    wp->row_starts[wp->batch_size] = wp->size;

    return 0;
}

int word_pool_read(word_pool_t *wp, int *sent_ends,
        int epoch_size, FILE *text_fp, vocab_t *vocab, int *oovs,
        bool drop_empty_line)
{
    char *line = NULL;
    size_t line_sz = 0;
    char word[MAX_LINE_LEN];

    char *p;

    int num_sents;
    int oov;
    int i;

    bool err;

    // assert len(sent_ends) >= epoch_size
    ST_CHECK_PARAM(wp == NULL || text_fp == NULL || vocab == NULL, -1);

    err = false;
    num_sents = 0;
    oov = 0;
    wp->size = 0;
    while (st_fgets(&line, &line_sz, text_fp, &err)) {
        remove_newline(line);

        if (line[0] == '\0' && drop_empty_line) {
            continue;
        }

        if (wp->size >= wp->capacity - 1) {
            if (word_pool_ensure_capacity(wp,
                        wp->capacity + NUM_WORD_PER_SENT) < 0) {
                ST_WARNING("Failed to word_pool_ensure_capacity. ");
                goto ERR;
            }
        }
        wp->words[wp->size] = vocab_get_id(vocab, SENT_START);
        wp->size++;

        p = line;
        i = 0;
        while (*p != '\0') {
            if (*p == ' ' || *p == '\t') {
                if (i > 0) {
                    word[i] = '\0';
                    if (wp->size >= wp->capacity - 1) {
                        if (word_pool_ensure_capacity(wp,
                                wp->capacity + NUM_WORD_PER_SENT) < 0) {
                            ST_WARNING("Failed to word_pool_ensure_capacity. ");
                            goto ERR;
                        }
                    }
                    wp->words[wp->size] = vocab_get_id(vocab, word);
                    if (wp->words[wp->size] < 0
                            || wp->words[wp->size] == UNK_ID) {
                        wp->words[wp->size] = UNK_ID;
                        oov++;
                    }
                    wp->size++;

                    i = 0;
                }

                while (*p == ' ' || *p == '\t') {
                    p++;
                }
            } else {
                word[i] = *p;
                i++;
                p++;
            }
        }
        if (i > 0) {
            word[i] = '\0';
            if (wp->size >= wp->capacity - 1) {
                if (word_pool_ensure_capacity(wp,
                            wp->capacity + NUM_WORD_PER_SENT) < 0) {
                    ST_WARNING("Failed to word_pool_ensure_capacity. ");
                    goto ERR;
                }
            }
            wp->words[wp->size] = vocab_get_id(vocab, word);
            if (wp->words[wp->size] < 0
                    || wp->words[wp->size] == UNK_ID) {
                wp->words[wp->size] = UNK_ID;
                oov++;
            }
            wp->size++;

            i = 0;
        }

        wp->words[wp->size] = SENT_END_ID;
        wp->size++; // do not need to check, prev check wp->size - 1
        if (sent_ends != NULL) {
            sent_ends[num_sents] = wp->size;
        }

        num_sents++;
        if (num_sents >= epoch_size) {
            break;
        }
    }

    safe_st_free(line);

    if (err) {
        return -1;
    }

    if (oovs != NULL) {
        *oovs = oov;
    }

    return num_sents;

ERR:

    safe_st_free(line);
    return -1;
}

void reader_destroy(reader_t *reader)
{
    word_pool_t *p;
    word_pool_t *q;

    if (reader == NULL) {
        return;
    }

    p = reader->full_wp_head;
    while (p != NULL) {
        q = p;
        p = p->next;

        word_pool_destroy(q);
        safe_st_free(q);
    }
    reader->full_wp_head = NULL;
    reader->full_wp_tail = NULL;

    p = reader->empty_wps;
    while (p != NULL) {
        q = p;
        p = p->next;

        word_pool_destroy(q);
        safe_st_free(q);
    }
    reader->empty_wps = NULL;

    (void)pthread_mutex_destroy(&reader->full_wp_lock);
    (void)pthread_mutex_destroy(&reader->empty_wp_lock);
    (void)st_sem_destroy(&reader->sem_full);
    (void)st_sem_destroy(&reader->sem_empty);

    safe_fclose(reader->fp_debug);
    (void)pthread_mutex_destroy(&reader->fp_debug_lock);
}

int reader_load_opt(reader_opt_t *reader_opt,
        st_opt_t *opt, const char *sec_name)
{
    char name[MAX_ST_CONF_LEN];
    char str[MAX_ST_CONF_LEN];

    ST_CHECK_PARAM(reader_opt == NULL || opt == NULL, -1);

    if (sec_name == NULL || sec_name[0] == '\0') {
        snprintf(name, MAX_ST_CONF_LEN, "%s", "READER");
    } else {
        snprintf(name, MAX_ST_CONF_LEN, "%s/%s", sec_name,
                "READER");
    }

    ST_OPT_SEC_GET_UINT(opt, name, "RANDOM_SEED",
            reader_opt->rand_seed, 1, "Random seed");

    ST_OPT_SEC_GET_STR(opt, name, "EPOCH_SIZE", str, MAX_ST_CONF_LEN, "10k",
            "Number of sentences read in one epoch per thread (can be set to "
            "10k, 25M, etc.)");
    reader_opt->epoch_size = (int)st_str2ll(str);
    if (reader_opt->epoch_size <= 0) {
        ST_WARNING("Invalid epoch_size[%s]", str);
        goto ST_OPT_ERR;
    }

    ST_OPT_SEC_GET_INT(opt, name, "MINI_BATCH_SIZE", reader_opt->mini_batch,
            1, "Size of mini-batch.");
    if (reader_opt->mini_batch <= 0) {
        ST_WARNING("mini batch size must be at least 1, get [%d]",
                reader_opt->mini_batch);
        goto ST_OPT_ERR;
    }

    ST_OPT_SEC_GET_BOOL(opt, name, "SHUFFLE",
            reader_opt->shuffle, true, "Shuffle after reading");

    ST_OPT_SEC_GET_BOOL(opt, name, "DROP_EMPTY_LINE",
            reader_opt->drop_empty_line, true,
            "whether drop empty lines in text");

    ST_OPT_SEC_GET_STR(opt, name, "DEBUG_FILE",
            reader_opt->debug_file, MAX_DIR_LEN, "",
            "file to print out debug infos.");

    return 0;

ST_OPT_ERR:
    return -1;
}

reader_t* reader_create(reader_opt_t *opt, int num_thrs,
        vocab_t *vocab, const char *text_file)
{
    reader_t *reader = NULL;
    word_pool_t *wp;
    int pool_size;

    int i;

    ST_CHECK_PARAM(opt == NULL || num_thrs < 0 || vocab == NULL
            || text_file == NULL, NULL);

    reader = (reader_t *)st_malloc(sizeof(reader_t));
    if (reader == NULL) {
        ST_WARNING("Failed to st_malloc reader.");
        return NULL;
    }
    memset(reader, 0, sizeof(reader_t));

    reader->opt = *opt;
    reader->num_thrs = num_thrs;
    reader->vocab = vocab;
    strncpy(reader->text_file, text_file, MAX_DIR_LEN);
    reader->text_file[MAX_DIR_LEN - 1] = '\0';

    pool_size = 2 * num_thrs;
    reader->full_wp_head = NULL;
    reader->full_wp_tail = NULL;
    reader->empty_wps = NULL;;
    for (i = 0; i < pool_size; i++) {
        wp = (word_pool_t *)st_malloc(sizeof(word_pool_t));
        if (wp == NULL) {
            ST_WARNING("Failed to st_malloc wp.");
            goto ERR;
        }
        memset(wp, 0, sizeof(word_pool_t));
        wp->next = reader->empty_wps;
        reader->empty_wps = wp;
    }

    if (st_sem_init(&reader->sem_empty, pool_size) != 0) {
        ST_WARNING("Failed to st_sem_init sem_empty.");
        goto ERR;
    }

    if (st_sem_init(&reader->sem_full, 0) != 0) {
        ST_WARNING("Failed to st_sem_init sem_full.");
        goto ERR;
    }

    if (pthread_mutex_init(&reader->full_wp_lock, NULL) != 0) {
        ST_WARNING("Failed to pthread_mutex_init full_wp_lock.");
        goto ERR;
    }

    if (pthread_mutex_init(&reader->empty_wp_lock, NULL) != 0) {
        ST_WARNING("Failed to pthread_mutex_init empty_wp_lock.");
        goto ERR;
    }

    if (opt->debug_file[0] != '\0') {
        reader->fp_debug = st_fopen(opt->debug_file, "w");
        if (reader->fp_debug == NULL) {
            ST_WARNING("Failed to st_fopen debug_file[%s].",
                    opt->debug_file);
            goto ERR;
        }
        if (pthread_mutex_init(&reader->fp_debug_lock, NULL) != 0) {
            ST_WARNING("Failed to pthread_mutex_init fp_debug_lock.");
            goto ERR;
        }
    }

    return reader;

ERR:
    safe_reader_destroy(reader);
    return NULL;
}

static void* reader_read_thread(void *args)
{
    reader_t *reader;
    thr_stat_t *stats;

    FILE *text_fp = NULL;
    int *shuffle_buf = NULL;
    off_t fsize;
    int *sent_ends = NULL;

    word_pool_t wp = {
        .words = NULL,
        .size = 0,
        .capacity = 0,
        .row_starts = NULL,
        .batch_size = 0,
    };
    int num_thrs;
    int epoch_size, mini_batch;

    word_pool_t *wp_in_queue;
    int num_sents;
    int oovs;
    int i;
    int start;
    int end;
    int num_reads;

    count_t total_words;
    count_t total_sents;
    double logp;
    struct timeval tts, tte;
    long ms = 0;
#ifdef _TIME_PROF_
    struct timeval tts_io, tte_io;
    long ms_io = 0;
    struct timeval tts_shuf, tte_shuf;
    long ms_shuf = 0;
    struct timeval tts_lock, tte_lock;
    long ms_lock = 0;
    struct timeval tts_fill, tte_fill;
    long ms_fill = 0;
#endif

    ST_CHECK_PARAM(args == NULL, NULL);

    reader = (reader_t *)args;
    stats = reader->stats;
    num_thrs = reader->num_thrs;
    epoch_size = reader->opt.epoch_size;
    mini_batch = reader->opt.mini_batch;

    if (word_pool_ensure_capacity(&wp, NUM_WORD_PER_SENT) < 0) {
        ST_WARNING("Failed to word_pool_ensure_capacity.");
        goto ERR;
    }

    if (reader->opt.shuffle) {
        shuffle_buf = (int *)st_malloc(sizeof(int) * epoch_size);
        if (shuffle_buf == NULL) {
            ST_WARNING("Failed to st_malloc shuffle_buf");
            goto ERR;
        }

        sent_ends = (int *)st_malloc(sizeof(int) * epoch_size);
        if (sent_ends == NULL) {
            ST_WARNING("Failed to st_malloc sent_ends");
            goto ERR;
        }
    }

    fsize = st_fsize(reader->text_file);

    text_fp = st_fopen(reader->text_file, "rb");
    if (text_fp == NULL) {
        ST_WARNING("Failed to open text file[%s]", reader->text_file);
        goto ERR;
    }

    num_reads = 0;
    reader->oovs = 0;
    reader->sents = 0;
    reader->words = 0;
    gettimeofday(&tts, NULL);
    while (!feof(text_fp)) {
        if (*(reader->err) != 0) {
            break;
        }

#ifdef _TIME_PROF_
        gettimeofday(&tts_io, NULL);
#endif
        num_sents = word_pool_read(&wp, sent_ends, epoch_size, text_fp,
                reader->vocab, &oovs, reader->opt.drop_empty_line);
        if (num_sents < 0) {
            ST_WARNING("Failed to word_pool_read.");
            goto ERR;
        }
#ifdef _TIME_PROF_
        gettimeofday(&tte_io, NULL);
#endif
        if (num_sents == 0) {
            continue;
        }

        reader->oovs += oovs;
        reader->sents += num_sents;
        reader->words += wp.size - num_sents; // Do not accumulate <s

#ifdef _TIME_PROF_
        gettimeofday(&tts_shuf, NULL);
#endif
        if (shuffle_buf != NULL) {
            for (i = 0; i < num_sents; i++) {
                shuffle_buf[i] = i;
            }

            st_shuffle_r(shuffle_buf, num_sents, &reader->random);
        }
#ifdef _TIME_PROF_
        gettimeofday(&tte_shuf, NULL);
#endif


#ifdef _TIME_PROF_
        gettimeofday(&tts_lock, NULL);
#endif
        if (st_sem_wait(&reader->sem_empty) != 0) {
            ST_WARNING("Failed to st_sem_wait sem_empty.");
            goto ERR;
        }

        if (pthread_mutex_lock(&reader->empty_wp_lock) != 0) {
            ST_WARNING("Failed to pthread_mutex_lock empty_wp_lock.");
            goto ERR;
        }
        wp_in_queue = reader->empty_wps;
        reader->empty_wps = reader->empty_wps->next;
        if (pthread_mutex_unlock(&reader->empty_wp_lock) != 0) {
            ST_WARNING("Failed to pthread_mutex_unlock empty_wp_lock.");
            goto ERR;
        }
#ifdef _TIME_PROF_
        gettimeofday(&tte_lock, NULL);
#endif

#ifdef _TIME_PROF_
        gettimeofday(&tts_fill, NULL);
#endif
        if (word_pool_ensure_capacity(wp_in_queue, wp.size) < 0) {
            ST_WARNING("Failed to word_pool_ensure_capacity. size[%d].",
                    wp.size);
            goto ERR;
        }

        if (shuffle_buf != NULL) {
            wp_in_queue->size = 0;
            for (i = 0; i < num_sents; i++) {
                if (shuffle_buf[i] == 0) {
                    start = 0;
                } else {
                    start = sent_ends[shuffle_buf[i] - 1];
                }
                end = sent_ends[shuffle_buf[i]];

                memcpy(wp_in_queue->words + wp_in_queue->size,
                        wp.words + start, sizeof(int)*(end - start));
                wp_in_queue->size += end - start;
            }
        } else {
            memcpy(wp_in_queue->words, wp.words, sizeof(int)*wp.size);
            wp_in_queue->size = wp.size;
        }

        if (mini_batch > 1) {
            if (word_pool_build_mini_batch(wp_in_queue, mini_batch) < 0) {
                ST_WARNING("Failed to word_pool_build_mini_batch.");
                goto ERR;
            }
        }
#ifdef _TIME_PROF_
        gettimeofday(&tte_fill, NULL);
#endif

        if (pthread_mutex_lock(&reader->full_wp_lock) != 0) {
            ST_WARNING("Failed to pthread_mutex_lock full_wp_lock.");
            goto ERR;
        }
        wp_in_queue->next = NULL;
        if (reader->full_wp_tail != NULL) {
            reader->full_wp_tail->next = wp_in_queue;
        }
        reader->full_wp_tail = wp_in_queue;
        if (reader->full_wp_head == NULL) {
            reader->full_wp_head = wp_in_queue;
        }
        //ST_DEBUG("IN: %p, %d", wp_in_queue, wp_in_queue->size);
        if (pthread_mutex_unlock(&reader->full_wp_lock) != 0) {
            ST_WARNING("Failed to pthread_mutex_unlock full_wp_lock.");
            goto ERR;
        }
        if (st_sem_post(&reader->sem_full) != 0) {
            ST_WARNING("Failed to st_sem_post sem_full.");
            goto ERR;
        }

#ifdef _TIME_PROF_
        gettimeofday(&tte, NULL);
        ms = TIMEDIFF(tts, tte);
        ms_io += TIMEDIFF(tts_io, tte_io);
        ms_shuf += TIMEDIFF(tts_shuf, tte_shuf);
        ms_lock += TIMEDIFF(tts_lock, tte_lock);
        ms_fill += TIMEDIFF(tts_fill, tte_fill);
        ST_TRACE("Time: %.3fs, I/O: %.3f(%.2f%%), Shuf: %.3f(%.2f%%), Lock: %.3f(%.2f%%), Fill: %.3f(%.2f%%)",
                ms / 1000.0, ms_io / 1000.0, ms_io / (ms / 100.0),
                ms_shuf / 1000.0, ms_shuf / (ms / 100.0),
                ms_lock / 1000.0, ms_lock / (ms / 100.0),
                ms_fill / 1000.0, ms_fill / (ms / 100.0));
#endif

        num_reads++;
        if (num_reads >= num_thrs) {
            num_reads = 0;

            total_words = 0;
            total_sents = 0;
            logp = 0.0;
            for (i = 0; i < num_thrs; i++) {
                total_words += stats[i].words;
                total_sents += stats[i].sents;
                logp += stats[i].logp;
            }

            if (total_words > 0) {
                gettimeofday(&tte, NULL);
                ms = TIMEDIFF(tts, tte);

                if (fsize > 0) {
                    ST_TRACE("Total progress: %.2f%%. "
                            "Words: " COUNT_FMT ", Sentences: " COUNT_FMT
                            ", OOVs: " COUNT_FMT ", words/sec: %.1f, "
                            "LogP: %f, Entropy: %f, PPL: %f, Time: %.3fs",
                            ftell(text_fp) / (fsize / 100.0),
                            total_words, total_sents, reader->oovs,
                            total_words / ((double) ms / 1000.0),
                            logp, -logp / log(2) / total_words,
                            exp(-logp / (double) total_words),
                            ms / 1000.0);
                } else {
                    ST_TRACE("Words: " COUNT_FMT ", Sentences: " COUNT_FMT
                            ", OOVs: " COUNT_FMT ", words/sec: %.1f, "
                            "LogP: %f, Entropy: %f, PPL: %f, Time: %.3fs",
                            total_words, total_sents, reader->oovs,
                            total_words / ((double) ms / 1000.0),
                            logp, -logp / log(2) / total_words,
                            exp(-logp / (double) total_words),
                            ms / 1000.0);
                }
            }
        }
    }

    for (i = 0; i < num_thrs; i++) {
        /* posting semaphore without adding data indicates finish. */
        if (st_sem_post(&reader->sem_full) != 0) {
            ST_WARNING("Failed to st_sem_post sem_full.");
            goto ERR;
        }
    }

    word_pool_destroy(&wp);
    safe_fclose(text_fp);
    safe_st_free(shuffle_buf);
    safe_st_free(sent_ends);

    return NULL;

ERR:
    *(reader->err) = -1;

    word_pool_destroy(&wp);
    safe_fclose(text_fp);
    safe_st_free(shuffle_buf);
    safe_st_free(sent_ends);

    for (i = 0; i < num_thrs; i++) {
        /* posting semaphore without adding data indicates finish. */
        if (st_sem_post(&reader->sem_full) != 0) {
            ST_WARNING("Failed to st_sem_post sem_full.");
            goto ERR;
        }
    }

    return NULL;
}

int reader_read(reader_t *reader, thr_stat_t *stats, int *err)
{
    ST_CHECK_PARAM(reader == NULL || stats == NULL
            || err == NULL, -1);

    reader->random = reader->opt.rand_seed;
    reader->stats = stats;
    reader->err = err;

    if (pthread_create(&reader->tid, NULL, reader_read_thread,
                (void *)reader) != 0) {
        ST_WARNING("Failed to pthread_create.");
        return -1;
    }

    return 0;
}

int reader_wait(reader_t *reader)
{
    ST_CHECK_PARAM(reader == NULL, -1);

    if (pthread_join(reader->tid, NULL) != 0) {
        ST_WARNING("Failed to pthread_join.");
        return -1;
    }

    return 0;
}

word_pool_t* reader_hold_word_pool(reader_t *reader)
{
    word_pool_t *wp;

    ST_CHECK_PARAM(reader == NULL, NULL);

    if (st_sem_wait(&reader->sem_full) != 0) {
        ST_WARNING("Failed to st_sem_wait sem_full.");
        return NULL;
    }
    if (pthread_mutex_lock(&reader->full_wp_lock) != 0) {
        ST_WARNING("Failed to pthread_mutex_lock full_wp_lock.");
        return NULL;
    }

    wp = reader->full_wp_head;
    if (wp != NULL) {
        reader->full_wp_head = wp->next;
    }
    if (wp == reader->full_wp_tail) {
        reader->full_wp_tail = NULL;
    }

    if (pthread_mutex_unlock(&reader->full_wp_lock) != 0) {
        ST_WARNING("Failed to pthread_mutex_unlock full_wp_lock.");
        return NULL;
    }

    if (reader->fp_debug != NULL) {
        if (word_pool_print(reader->fp_debug, &reader->fp_debug_lock,
                    wp, reader->vocab) < 0) {
            ST_WARNING("Failed to word_pool_print.");
            return NULL;
        }
    }

    return wp;
}

int reader_release_word_pool(reader_t *reader, word_pool_t *wp)
{
    ST_CHECK_PARAM(reader == NULL || wp == NULL, -1);

    if (pthread_mutex_lock(&reader->empty_wp_lock) != 0) {
        ST_WARNING("Failed to pthread_mutex_lock empty_wp_lock.");
        return -1;
    }
    wp->next = reader->empty_wps;
    reader->empty_wps = wp;
    if (pthread_mutex_unlock(&reader->empty_wp_lock) != 0) {
        ST_WARNING("Failed to pthread_mutex_unlock empty_wp_lock.");
        return -1;
    }
    if (st_sem_post(&reader->sem_empty) != 0) {
        (void) pthread_mutex_unlock(&reader->empty_wp_lock);
        ST_WARNING("Failed to st_sem_post sem_empty.");
        return -1;
    }

    return 0;
}
