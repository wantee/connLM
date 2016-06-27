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
#include <stutils/st_utils.h>
#include <stutils/st_io.h>

#include "utils.h"
#include "reader.h"

#define NUM_WORD_PER_SENT 128

static void connlm_egs_destroy(connlm_egs_t *egs)
{
    if (egs != NULL) {
        safe_free(egs->words);
        egs->capacity = 0;
        egs->size = 0;
    }
}

//#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-overflow"
static int connlm_egs_ensure(connlm_egs_t *egs, int capacity)
{
    ST_CHECK_PARAM(egs == NULL, -1);

    if (egs->capacity < capacity) {
        egs->words = realloc(egs->words, sizeof(int)*capacity);
        if (egs->words == NULL) {
            ST_WARNING("Failed to realloc egs->words. capacity[%d]",
                    capacity);
            return -1;
        }

        egs->capacity = capacity;
    }

    return 0;
}
//#pragma GCC diagnostic pop

static int connlm_egs_print(FILE *fp, pthread_mutex_t *fp_lock,
        connlm_egs_t *egs, vocab_t *vocab)
{
    int word;
    int i;

    ST_CHECK_PARAM(fp == NULL || fp_lock == NULL
            || egs == NULL, -1);

    i = 0;
    while (i < egs->size) {
        (void)pthread_mutex_lock(fp_lock);
        fprintf(fp, "<EGS>: ");
        while (egs->words[i] != SENT_END_ID && i < egs->size) {
            word = egs->words[i];

            if (vocab != NULL) {
                fprintf(fp, "%s", vocab_get_word(vocab, word));
            } else {
                fprintf(fp, "<%d>", word);
            }
            if (i < egs->size - 1 && egs->words[i+1] != 0) {
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

static int reader_read_egs(connlm_egs_t *egs, int *sent_ends,
        int epoch_size, FILE *text_fp, vocab_t *vocab, int *oovs)
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
    ST_CHECK_PARAM(egs == NULL || text_fp == NULL, -1);

    err = false;
    num_sents = 0;
    oov = 0;
    egs->size = 0;
    while (st_fgets(&line, &line_sz, text_fp, &err)) {
        remove_newline(line);

        if (line[0] == '\0') {
            continue;
        }

        p = line;
        i = 0;
        while (*p != '\0') {
            if (*p == ' ' || *p == '\t') {
                if (i > 0) {
                    word[i] = '\0';
                    if (egs->size >= egs->capacity - 1) {
                        if (connlm_egs_ensure(egs,
                                egs->capacity + NUM_WORD_PER_SENT) < 0) {
                            ST_WARNING("Failed to connlm_egs_ensure. ");
                            goto ERR;
                        }
                    }
                    egs->words[egs->size] = vocab_get_id(vocab, word);
                    if (egs->words[egs->size] < 0
                            || egs->words[egs->size] == UNK_ID) {
                        egs->words[egs->size] = UNK_ID;
                        oov++;
                    }
                    egs->size++;

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
            if (egs->size >= egs->capacity - 1) {
                if (connlm_egs_ensure(egs,
                            egs->capacity + NUM_WORD_PER_SENT) < 0) {
                    ST_WARNING("Failed to connlm_egs_ensure. ");
                    goto ERR;
                }
            }
            egs->words[egs->size] = vocab_get_id(vocab, word);
            if (egs->words[egs->size] < 0
                    || egs->words[egs->size] == UNK_ID) {
                egs->words[egs->size] = UNK_ID;
                oov++;
            }
            egs->size++;

            i = 0;
        }

        egs->words[egs->size] = SENT_END_ID;
        egs->size++; // do not need to check, prev check num_egs - 1
        if (sent_ends != NULL) {
            sent_ends[num_sents] = egs->size;
        }

        num_sents++;
        if (num_sents >= epoch_size) {
            break;
        }
    }

    safe_free(line);

    if (err) {
        return -1;
    }

    if (oovs != NULL) {
        *oovs = oov;
    }

    return num_sents;

ERR:

    safe_free(line);
    return -1;
}

void reader_destroy(reader_t *reader)
{
    connlm_egs_t *p;
    connlm_egs_t *q;

    if (reader == NULL) {
        return;
    }

    p = reader->full_egs;
    while (p != NULL) {
        q = p;
        p = p->next;

        connlm_egs_destroy(q);
        safe_free(q);
    }
    reader->full_egs = NULL;

    p = reader->empty_egs;
    while (p != NULL) {
        q = p;
        p = p->next;

        connlm_egs_destroy(q);
        safe_free(q);
    }
    reader->empty_egs = NULL;

    (void)pthread_mutex_destroy(&reader->full_egs_lock);
    (void)pthread_mutex_destroy(&reader->empty_egs_lock);
    (void)st_sem_destroy(&reader->sem_full);
    (void)st_sem_destroy(&reader->sem_empty);

    safe_fclose(reader->fp_debug);
    (void)pthread_mutex_destroy(&reader->fp_debug_lock);
}

int reader_load_opt(reader_opt_t *reader_opt,
        st_opt_t *opt, const char *sec_name)
{
    char name[MAX_ST_CONF_LEN];

    ST_CHECK_PARAM(reader_opt == NULL || opt == NULL, -1);

    if (sec_name == NULL || sec_name[0] == '\0') {
        snprintf(name, MAX_ST_CONF_LEN, "%s", "READER");
    } else {
        snprintf(name, MAX_ST_CONF_LEN, "%s/%s", sec_name,
                "READER");
    }

    ST_OPT_SEC_GET_UINT(opt, sec_name, "RANDOM_SEED",
            reader_opt->rand_seed, 1, "Random seed");

    ST_OPT_SEC_GET_INT(opt, sec_name, "EPOCH_SIZE",
            reader_opt->epoch_size, 10,
            "Number of sentences read in one epoch per thread (in kilos)");
    if (reader_opt->epoch_size > 0) {
        reader_opt->epoch_size *= 1000;
    } else {
        reader_opt->epoch_size = 1;
    }

    ST_OPT_SEC_GET_BOOL(opt, sec_name, "SHUFFLE",
            reader_opt->shuffle, true, "Shuffle after reading");

    ST_OPT_SEC_GET_STR(opt, sec_name, "DEBUG_FILE",
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
    connlm_egs_t *egs;
    int pool_size;

    int i;

    ST_CHECK_PARAM(opt == NULL || num_thrs < 0 || vocab == NULL
            || text_file == NULL, NULL);

    reader = (reader_t *)malloc(sizeof(reader_t));
    if (reader == NULL) {
        ST_WARNING("Failed to malloc reader.");
        return NULL;
    }
    memset(reader, 0, sizeof(reader_t));

    reader->opt = *opt;
    reader->num_thrs = num_thrs;
    reader->vocab = vocab;
    strncpy(reader->text_file, text_file, MAX_DIR_LEN);
    reader->text_file[MAX_DIR_LEN - 1] = '\0';

    pool_size = 2 * num_thrs;
    reader->full_egs = NULL;
    reader->empty_egs = NULL;;
    for (i = 0; i < pool_size; i++) {
        egs = (connlm_egs_t *)malloc(sizeof(connlm_egs_t));
        if (egs == NULL) {
            ST_WARNING("Failed to malloc egs.");
            goto ERR;
        }
        memset(egs, 0, sizeof(connlm_egs_t));
        egs->next = reader->empty_egs;
        reader->empty_egs = egs;
    }

    if (st_sem_init(&reader->sem_empty, pool_size) != 0) {
        ST_WARNING("Failed to st_sem_init sem_empty.");
        goto ERR;
    }

    if (st_sem_init(&reader->sem_full, 0) != 0) {
        ST_WARNING("Failed to st_sem_init sem_full.");
        goto ERR;
    }

    if (pthread_mutex_init(&reader->full_egs_lock, NULL) != 0) {
        ST_WARNING("Failed to pthread_mutex_init full_egs_lock.");
        goto ERR;
    }

    if (pthread_mutex_init(&reader->empty_egs_lock, NULL) != 0) {
        ST_WARNING("Failed to pthread_mutex_init empty_egs_lock.");
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

typedef struct _reader_thread_t_ {
    reader_t *reader;
    thr_stat_t *stats;
    unsigned random;
    int *err;
} reader_thr_t;

static void* reader_read_thread(void *args)
{
    reader_thr_t *thr;
    thr_stat_t *stats;

    FILE *text_fp = NULL;
    int *shuffle_buf = NULL;
    off_t fsize;
    int *sent_ends = NULL;

    connlm_egs_t egs = {
        .words = NULL,
        .size = 0,
        .capacity = 0,
    };
    int num_thrs;
    int epoch_size;

    connlm_egs_t *egs_in_pool;
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

    thr = (reader_thr_t *)args;
    stats = thr->stats;
    num_thrs = thr->reader->num_thrs;
    epoch_size = thr->reader->opt.epoch_size;

    if (connlm_egs_ensure(&egs, NUM_WORD_PER_SENT) < 0) {
        ST_WARNING("Failed to connlm_egs_ensure.");
        goto ERR;
    }

    if (thr->reader->opt.shuffle) {
        shuffle_buf = (int *)malloc(sizeof(int) * epoch_size);
        if (shuffle_buf == NULL) {
            ST_WARNING("Failed to malloc shuffle_buf");
            goto ERR;
        }

        sent_ends = (int *)malloc(sizeof(int) * epoch_size);
        if (sent_ends == NULL) {
            ST_WARNING("Failed to malloc sent_ends");
            goto ERR;
        }
    }

    fsize = st_fsize(thr->reader->text_file);

    text_fp = st_fopen(thr->reader->text_file, "rb");
    if (text_fp == NULL) {
        ST_WARNING("Failed to open text file[%s]", thr->reader->text_file);
        goto ERR;
    }

    num_reads = 0;
    thr->reader->oovs = 0;
    thr->reader->sents = 0;
    thr->reader->words = 0;
    gettimeofday(&tts, NULL);
    while (!feof(text_fp)) {
        if (*(thr->err) != 0) {
            break;
        }

#ifdef _TIME_PROF_
        gettimeofday(&tts_io, NULL);
#endif
        num_sents = reader_read_egs(&egs, sent_ends,
                epoch_size, text_fp, thr->reader->vocab, &oovs);
        if (num_sents < 0) {
            ST_WARNING("Failed to reader_read_egs.");
            goto ERR;
        }
#ifdef _TIME_PROF_
        gettimeofday(&tte_io, NULL);
#endif
        if (num_sents == 0) {
            continue;
        }

        thr->reader->oovs += oovs;
        thr->reader->sents += num_sents;
        thr->reader->words += egs.size;

#ifdef _TIME_PROF_
        gettimeofday(&tts_shuf, NULL);
#endif
        if (shuffle_buf != NULL) {
            for (i = 0; i < num_sents; i++) {
                shuffle_buf[i] = i;
            }

            st_shuffle_r(shuffle_buf, num_sents, &thr->random);
        }
#ifdef _TIME_PROF_
        gettimeofday(&tte_shuf, NULL);
#endif

#ifdef _TIME_PROF_
        gettimeofday(&tts_lock, NULL);
#endif
        if (st_sem_wait(&thr->reader->sem_empty) != 0) {
            ST_WARNING("Failed to st_sem_wait sem_empty.");
            goto ERR;
        }

        if (pthread_mutex_lock(&thr->reader->empty_egs_lock) != 0) {
            ST_WARNING("Failed to pthread_mutex_lock empty_egs_lock.");
            goto ERR;
        }
        egs_in_pool = thr->reader->empty_egs;
        thr->reader->empty_egs = thr->reader->empty_egs->next;
        if (pthread_mutex_unlock(&thr->reader->empty_egs_lock) != 0) {
            ST_WARNING("Failed to pthread_mutex_unlock empty_egs_lock.");
            goto ERR;
        }
#ifdef _TIME_PROF_
        gettimeofday(&tte_lock, NULL);
#endif

#ifdef _TIME_PROF_
        gettimeofday(&tts_fill, NULL);
#endif
        if (connlm_egs_ensure(egs_in_pool, egs.size) < 0) {
            ST_WARNING("Failed to connlm_egs_ensure. size[%d].",
                    egs.size);
            goto ERR;
        }

        if (shuffle_buf != NULL) {
            egs_in_pool->size = 0;
            for (i = 0; i < num_sents; i++) {
                if (shuffle_buf[i] == 0) {
                    start = 0;
                } else {
                    start = sent_ends[shuffle_buf[i] - 1];
                }
                end = sent_ends[shuffle_buf[i]];

                memcpy(egs_in_pool->words + egs_in_pool->size,
                        egs.words + start, sizeof(int)*(end - start));
                egs_in_pool->size += end - start;
            }
        } else {
            memcpy(egs_in_pool->words, egs.words, sizeof(int)*egs.size);
            egs_in_pool->size = egs.size;
        }
#ifdef _TIME_PROF_
        gettimeofday(&tte_fill, NULL);
#endif

        if (pthread_mutex_lock(&thr->reader->full_egs_lock) != 0) {
            ST_WARNING("Failed to pthread_mutex_lock full_egs_lock.");
            goto ERR;
        }
        egs_in_pool->next = thr->reader->full_egs;
        thr->reader->full_egs = egs_in_pool;
        //ST_DEBUG("IN: %p, %d", egs_in_pool, egs_in_pool->size);
        if (pthread_mutex_unlock(&thr->reader->full_egs_lock) != 0) {
            ST_WARNING("Failed to pthread_mutex_unlock full_egs_lock.");
            goto ERR;
        }
        if (st_sem_post(&thr->reader->sem_full) != 0) {
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
                            total_words, total_sents, thr->reader->oovs,
                            total_words / ((double) ms / 1000.0),
                            logp, -logp / log10(2) / total_words,
                            exp10(-logp / (double) total_words),
                            ms / 1000.0);
                } else {
                    ST_TRACE("Words: " COUNT_FMT ", Sentences: " COUNT_FMT
                            ", OOVs: " COUNT_FMT ", words/sec: %.1f, "
                            "LogP: %f, Entropy: %f, PPL: %f, Time: %.3fs",
                            total_words, total_sents, thr->reader->oovs,
                            total_words / ((double) ms / 1000.0),
                            logp, -logp / log10(2) / total_words,
                            exp10(-logp / (double) total_words),
                            ms / 1000.0);
                }
            }
        }
    }

    for (i = 0; i < num_thrs; i++) {
        /* posting semaphore without adding data indicates finish. */
        if (st_sem_post(&thr->reader->sem_full) != 0) {
            ST_WARNING("Failed to st_sem_post sem_full.");
            goto ERR;
        }
    }

    connlm_egs_destroy(&egs);
    safe_fclose(text_fp);
    safe_free(shuffle_buf);
    safe_free(sent_ends);

    return NULL;

ERR:
    *(thr->err) = -1;

    connlm_egs_destroy(&egs);
    safe_fclose(text_fp);
    safe_free(shuffle_buf);
    safe_free(sent_ends);

    for (i = 0; i < num_thrs; i++) {
        /* posting semaphore without adding data indicates finish. */
        if (st_sem_post(&thr->reader->sem_full) != 0) {
            ST_WARNING("Failed to st_sem_post sem_full.");
            goto ERR;
        }
    }

    return NULL;
}

int reader_read(reader_t *reader, thr_stat_t *stats, int *err)
{
    reader_thr_t thr;

    ST_CHECK_PARAM(reader == NULL || stats == NULL
            || err == NULL, -1);

    thr.reader = reader;
    thr.random = reader->opt.rand_seed;
    thr.stats = stats;
    thr.err = err;

    if (pthread_create(&reader->tid, NULL, reader_read_thread,
                (void *)&thr) != 0) {
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

connlm_egs_t* reader_hold_egs(reader_t *reader)
{
    connlm_egs_t *egs;

    ST_CHECK_PARAM(reader == NULL, NULL);

    if (st_sem_wait(&reader->sem_full) != 0) {
        ST_WARNING("Failed to st_sem_wait sem_full.");
        return NULL;
    }
    if (pthread_mutex_lock(&reader->full_egs_lock) != 0) {
        ST_WARNING("Failed to pthread_mutex_lock full_egs_lock.");
        return NULL;
    }

    if (reader->full_egs == NULL) {
        egs = NULL;
    } else {
        egs = reader->full_egs;
        reader->full_egs = reader->full_egs->next;
    }

    if (pthread_mutex_unlock(&reader->full_egs_lock) != 0) {
        ST_WARNING("Failed to pthread_mutex_unlock full_egs_lock.");
        return NULL;
    }

    if (reader->fp_debug != NULL) {
        if (connlm_egs_print(reader->fp_debug, &reader->fp_debug_lock,
                    egs, reader->vocab) < 0) {
            ST_WARNING("Failed to connlm_egs_print.");
            return NULL;
        }
    }

    return egs;
}

int reader_release_egs(reader_t *reader, connlm_egs_t *egs)
{
    ST_CHECK_PARAM(reader == NULL || egs == NULL, -1);

    if (pthread_mutex_lock(&reader->empty_egs_lock) != 0) {
        ST_WARNING("Failed to pthread_mutex_lock empty_egs_lock.");
        return -1;
    }
    egs->next = reader->empty_egs;
    reader->empty_egs = egs;
    if (pthread_mutex_unlock(&reader->empty_egs_lock) != 0) {
        ST_WARNING("Failed to pthread_mutex_unlock empty_egs_lock.");
        return -1;
    }
    if (st_sem_post(&reader->sem_empty) != 0) {
        ST_WARNING("Failed to st_sem_post sem_empty.");
        return -1;
    }

    return 0;
}
