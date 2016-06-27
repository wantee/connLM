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

#include "driver.h"

int driver_load_eval_opt(driver_eval_opt_t *eval_opt,
        st_opt_t *opt, const char *sec_name)
{
    char str[MAX_ST_CONF_LEN];

    ST_CHECK_PARAM(eval_opt == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_BOOL(opt, sec_name, "PRINT_SENT_PROB",
            eval_opt->print_sent_prob, false,
            "Print sentence prob only, if true. ");

    ST_OPT_SEC_GET_STR(opt, sec_name, "OUT_LOG_BASE",
            str, MAX_ST_CONF_LEN, "e",
            "Log base for printing prob. Could be 'e' or other number.");
    if (str[0] == 'e' && str[1] == '\0') {
        eval_opt->out_log_base = 0;
    } else {
        eval_opt->out_log_base = (real_t)atof(str);
    }

    return 0;

ST_OPT_ERR:
    return -1;
}

int driver_load_gen_opt(driver_gen_opt_t *gen_opt,
        st_opt_t *opt, const char *sec_name)
{
    ST_CHECK_PARAM(gen_opt == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_UINT(opt, sec_name, "RANDOM_SEED",
            gen_opt->rand_seed, (unsigned int)time(NULL),
            "Random seed. Default is value of time(NULl).");

    ST_OPT_SEC_GET_STR(opt, sec_name, "PREFIX_FILE",
            gen_opt->prefix_file, MAX_DIR_LEN, "",
            "File contains text prefix to be generated after");

    return 0;

ST_OPT_ERR:
    return -1;
}

void driver_destroy(driver_t *driver)
{
    int i;

    if (driver == NULL) {
        return;
    }

    driver->connlm = NULL;
    driver->reader = NULL;

    for(i = 0; i < driver->n_thr; i++) {
        safe_updater_destroy(driver->updaters[i]);
    }
    safe_free(driver->updaters);
    driver->n_thr = 0;
}

driver_t* driver_create(connlm_t *connlm, reader_t *reader, int n_thr)
{
    driver_t *driver = NULL;

    int i;

    ST_CHECK_PARAM(connlm == NULL || reader == NULL || n_thr <= 0, NULL);

    driver = (driver_t *)malloc(sizeof(driver_t));
    if (driver == NULL) {
        ST_WARNING("Failed to malloc driver.");
        return NULL;
    }
    memset(driver, 0, sizeof(driver_t));

    driver->connlm = connlm;
    driver->reader = reader;
    driver->n_thr = n_thr;

    driver->updaters = (updater_t **)malloc(sizeof(updater_t*)*n_thr);
    if (driver->updaters == NULL) {
        ST_WARNING("Failed to malloc updaters.");
        goto ERR;
    }
    memset(driver, 0, sizeof(updater_t*) * n_thr);

    for (i = 0; i < driver->n_thr; i++) {
        driver->updaters[i] = updater_create(connlm);
        if (driver->updaters[i] == NULL) {
            ST_WARNING("Failed to malloc updater[%d].", i);
            goto ERR;
        }
    }

    return driver;

ERR:
    safe_driver_destroy(driver);
    return NULL;
}

#if 0
int connlm_setup(connlm_t *connlm, bool backprop)
{
    int c;

    ST_CHECK_PARAM(connlm == NULL, -1);

    if (output_setup(connlm->output, connlm->opt.num_thread,
                backprop) < 0) {
        ST_WARNING("Failed to output_setup.");
        return -1;
    }

    for (c = 0; c < connlm->num_comp; c++) {
        if (comp_setup(connlm->comps[c], connlm->opt.num_thread,
                    backprop) < 0) {
            ST_WARNING("Failed to comp_setup[%s].",
                    connlm->comps[c]->name);
            return -1;
        }
    }

    return 0;
}

typedef struct _connlm_thread_t_ {
    connlm_t *connlm; /**< connlm model. */
    int tid; /**< thread id. */

    reader_t *reader;
    thr_stat_t *stat;
} connlm_thr_t;

static void* connlm_train_thread(void *args)
{
    connlm_t *connlm;
    connlm_thr_t *thr;
    int tid;

    connlm_egs_t *egs = NULL;

    count_t words;
    count_t sents;
    double logp;
    bool finish;

    int word;
    int i;

    struct timeval tts, tte;
    long ms;

    struct timeval tts_wait, tte_wait;
    long ms_wait = 0;

    ST_CHECK_PARAM(args == NULL, NULL);

    thr = (connlm_thr_t *)args;
    connlm = thr->connlm;
    tid = thr->tid;

    gettimeofday(&tts, NULL);

    finish = false;
    words = 0;
    sents = 0;
    logp = 0.0;
    while (true) {
        if (connlm->err != 0) {
            break;
        }

        gettimeofday(&tts_wait, NULL);
        egs = reader_hold_egs(thr->reader);
        gettimeofday(&tte_wait, NULL);

        if (egs == NULL) { // finish
            break;
        }

        if (connlm_reset(connlm, tid, true) < 0) {
            ST_WARNING("Failed to connlm_reset.");
            goto ERR;
        }

        for (i = 0; i < egs->size; i++) {
            word = egs->words[i];

            if (connlm_start(connlm, word, tid, true) < 0) {
                ST_WARNING("connlm_start.");
                goto ERR;
            }

            if (connlm_forward(connlm, word, tid) < 0) {
                ST_WARNING("Failed to connlm_forward.");
                goto ERR;
            }

            logp += output_get_logprob(connlm->output, word, tid);

            if ((logp != logp) || (isinf(logp))) {
                ST_WARNING("Numerical error. tid[%d], log(p_word(%d)) = %g",
                        tid, word,
                        output_get_logprob(connlm->output, word, tid));
                goto ERR;
            }

            if (connlm_fwd_bp(connlm, word, tid) < 0) {
                ST_WARNING("Failed to connlm_fwd_bp.");
                goto ERR;
            }

            if (connlm_backprop(connlm, word, tid) < 0) {
                ST_WARNING("Failed to connlm_backprop.");
                goto ERR;
            }

            if (connlm_end(connlm, word, tid, true) < 0) {
                ST_WARNING("connlm_end.");
                goto ERR;
            }

            if (word == SENT_END_ID) {
                if (connlm_reset(connlm, tid, true) < 0) {
                    ST_WARNING("Failed to connlm_reset.");
                    goto ERR;
                }
                sents++;
            }
            words++;
        }

        thr->stat->words = words;
        thr->stat->sents = sents;
        thr->stat->logp = logp;

        gettimeofday(&tte, NULL);
        ms = TIMEDIFF(tts, tte);
        ms_wait += TIMEDIFF(tts_wait, tte_wait);

        ST_TRACE("Thread: %d, Words: " COUNT_FMT
                ", Sentences: " COUNT_FMT ", words/sec: %.1f, "
                "LogP: %f, Entropy: %f, PPL: %f, "
                "Time(cpu/wait): %.3fs(%.2f%%)/%.3fs(%.2f%%):%.3f",
                tid, words, sents, words / ((double) ms / 1000.0),
                logp, -logp / log10(2) / words,
                exp10(-logp / (double) words),
                (ms - ms_wait) / 1000.0, (ms - ms_wait) / (ms / 100.0),
                ms_wait / 1000.0, ms_wait / (ms / 100.0),
                TIMEDIFF(tts_wait, tte_wait) / 1000.0);

        if (reader_release_egs(thr->reader, egs) < 0) {
            ST_WARNING("Failed to reader_release_egs.");
            goto ERR;
        }
    }

    return NULL;
ERR:
    connlm->err = -1;

    /* TODO: unlock mutex and post semaphore */
    return NULL;
}

int connlm_setup_train(connlm_t *connlm, connlm_opt_t *connlm_opt,
        const char *train_file)
{
    if (connlm_setup(connlm, true) < 0) {
        ST_WARNING("Failed to connlm_setup.");
        return -1;
    }

    return 0;
}

int connlm_train(connlm_t *connlm, reader_t *reader)
{
    connlm_thr_t *thrs = NULL;
    pthread_t *pts = NULL;
    thr_stat_t *stats = NULL;

    int n_thr;

    count_t words;
    count_t sents;
    double logp;
    struct timeval tts_train, tte_train;
    long ms;

    int i;

    ST_CHECK_PARAM(connlm == NULL, -1);

    gettimeofday(&tts_train, NULL);

    n_thr = connlm->opt.num_thread;
    pts = (pthread_t *)malloc(n_thr * sizeof(pthread_t));
    if (pts == NULL) {
        ST_WARNING("Failed to malloc pts");
        goto ERR;
    }

    thrs = (connlm_thr_t *)malloc(sizeof(connlm_thr_t) * n_thr);
    if (thrs == NULL) {
        ST_WARNING("Failed to malloc thrs");
        goto ERR;
    }
    memset(thrs, 0, sizeof(connlm_thr_t) * n_thr);

    stats = (thr_stat_t *)malloc(sizeof(thr_stat_t) * n_thr);
    if (stats == NULL) {
        ST_WARNING("Failed to malloc stats");
        goto ERR;
    }
    memset(stats, 0, sizeof(thr_stat_t) * n_thr);

    connlm->err = 0;

    if (reader_read(reader, stats, &connlm->err) < 0) {
        ST_WARNING("Failed to reader_read.");
        goto ERR;
    }

    for (i = 0; i < n_thr; i++) {
        thrs[i].connlm = connlm;
        thrs[i].tid = i;
        thrs[i].reader = reader;
        thrs[i].stat = stats + i;
        if (pthread_create(pts + i, NULL, connlm_train_thread,
                (void *)(thrs + i)) != 0) {
            ST_WARNING("Falied to pthread_create.");
            goto ERR;
        }
    }

    for (i = 0; i < n_thr; i++) {
        if (pthread_join(pts[i], NULL) != 0) {
            ST_WARNING("Falied to pthread_join.");
            goto ERR;
        }
    }
    if (reader_wait(reader) != 0) {
        ST_WARNING("Falied to reader_wait.");
        goto ERR;
    }

    if (connlm->err != 0) {
        goto ERR;
    }

    for (i = 0; i < n_thr; i++) {
        if (connlm_finish(connlm, i, true) < 0) {
            ST_WARNING("Failed to connlm_finish.");
            goto ERR;
        }
    }

    gettimeofday(&tte_train, NULL);
    ms = TIMEDIFF(tts_train, tte_train);

    words = 0;
    sents = 0;
    logp = 0.0;
    for (i = 0; i < n_thr; i++) {
        words += thrs[i].words;
        sents += thrs[i].sents;
        logp += thrs[i].logp;
    }

    if (words != read_thr.words || sents != read_thr.sents) {
        ST_WARNING("words[" COUNT_FMT "] != read_words[" COUNT_FMT "] "
                "or sents[" COUNT_FMT "] != read_sents[" COUNT_FMT "] ",
                words, read_thr.words, sents, read_thr.sents);
    }

    ST_NOTICE("Finish train in %ldms.", ms);
    ST_NOTICE("Words: " COUNT_FMT ", Sentences: " COUNT_FMT
            ", OOVs: " COUNT_FMT ", words/sec: %.1f", words, sents,
            read_thr.oovs, words / ((double) ms / 1000.0));
    ST_NOTICE("LogP: %f", logp);
    ST_NOTICE("Entropy: %f", -logp / log10(2) / words);
    ST_NOTICE("PPL: %f", exp10(-logp / (double) words));

    safe_free(pts);
    safe_free(thrs);

    return 0;

ERR:
    safe_free(pts);
    safe_free(thrs);

    return -1;
}

static void* connlm_eval_thread(void *args)
{
    connlm_t *connlm;
    connlm_thr_t *thr;
    int tid;

    connlm_egs_t *egs = NULL;

    count_t words;
    count_t sents;
    double logp;
    double logp_sent;
    bool finish;

    int word;
    double logp_word;

    int i;

    ST_CHECK_PARAM(args == NULL, NULL);

    thr = (connlm_thr_t *)args;
    connlm = thr->connlm;
    tid = thr->tid;

    finish = false;
    words = 0;
    sents = 0;
    logp = 0.0;
    while (true) {
        if (connlm->err != 0) {
            break;
        }

        if (st_sem_wait(&connlm->sem_full) != 0) {
            ST_WARNING("Failed to st_sem_wait sem_full.");
            goto ERR;
        }
        if (pthread_mutex_lock(&connlm->full_egs_lock) != 0) {
            ST_WARNING("Failed to pthread_mutex_lock full_egs_lock.");
            goto ERR;
        }

        if (connlm->full_egs == NULL) {
            finish = true;
        } else {
            if (connlm->opt.num_thread == 1) {
                /* ensure FIFO */
                connlm_egs_t *prev_egs = NULL;
                egs = connlm->full_egs;
                while (egs->next != NULL) {
                    /* full_egs list will not be too long */
                    prev_egs = egs;
                    egs = egs->next;
                }

                if (prev_egs == NULL) {
                    connlm->full_egs = NULL;
                } else {
                    prev_egs->next = NULL;
                }
            } else {
                egs = connlm->full_egs;
                connlm->full_egs = connlm->full_egs->next;
            }
        }

        if (pthread_mutex_unlock(&connlm->full_egs_lock) != 0) {
            ST_WARNING("Failed to pthread_mutex_unlock full_egs_lock.");
            goto ERR;
        }
        //ST_DEBUG("OUT: %s, %p, %d", bool2str(finish), egs, egs->size);
        if (finish) {
            break;
        }

        if (connlm->fp_debug != NULL) {
            if (connlm_egs_print(connlm->fp_debug, &connlm->fp_debug_lock,
                        egs, connlm->vocab) < 0) {
                ST_WARNING("Failed to connlm_egs_print.");
                goto ERR;
            }
        }

        if (connlm_reset(connlm, tid, false) < 0) {
            ST_WARNING("Failed to connlm_reset.");
            goto ERR;
        }
        logp_sent = 0.0;
        for (i = 0; i < egs->size; i++) {
            word = egs->words[i];

            if (connlm_start(connlm, word, tid, false) < 0) {
                ST_WARNING("connlm_start.");
                goto ERR;
            }

            if (connlm_forward(connlm, word, tid) < 0) {
                ST_WARNING("Failed to connlm_forward.");
                goto ERR;
            }

            logp_word = output_get_logprob(connlm->output, word, tid);
            logp += logp_word;
            logp_sent += logn10(logp_word, connlm->eval_opt.out_log_base);

            if ((logp != logp) || (isinf(logp))) {
                ST_WARNING("Numerical error. tid[%d], log(p_word(%d)) = %g",
                        tid, word,
                        output_get_logprob(connlm->output, word, tid));
                goto ERR;
            }

            if (connlm_end(connlm, word, tid, false) < 0) {
                ST_WARNING("connlm_end.");
                goto ERR;
            }

            if (connlm->fp_log != NULL
                    && (! connlm->eval_opt.print_sent_prob)) {
                (void)pthread_mutex_lock(&connlm->fp_lock);
                fprintf(connlm->fp_log, "%d\t%.6f\t%s", word,
                        logn10(logp_word, connlm->eval_opt.out_log_base),
                        vocab_get_word(connlm->vocab, word));

                fprintf(connlm->fp_log, "\n");
                (void)pthread_mutex_unlock(&connlm->fp_lock);
            }

            if (word == SENT_END_ID) {
                if (connlm_reset(connlm, tid, false) < 0) {
                    ST_WARNING("Failed to connlm_reset.");
                    goto ERR;
                }
                sents++;

                if (connlm->fp_log != NULL
                        && connlm->eval_opt.print_sent_prob) {
                    (void)pthread_mutex_lock(&connlm->fp_lock);
                    fprintf(connlm->fp_log, "%.6f\n", logp_sent);
                    (void)pthread_mutex_unlock(&connlm->fp_lock);
                }
                logp_sent = 0;
            }

            words++;
        }

        if (pthread_mutex_lock(&connlm->empty_egs_lock) != 0) {
            ST_WARNING("Failed to pthread_mutex_lock empty_egs_lock.");
            goto ERR;
        }
        egs->next = connlm->empty_egs;
        connlm->empty_egs = egs;
        if (pthread_mutex_unlock(&connlm->empty_egs_lock) != 0) {
            ST_WARNING("Failed to pthread_mutex_unlock empty_egs_lock.");
            goto ERR;
        }
        if (st_sem_post(&connlm->sem_empty) != 0) {
            ST_WARNING("Failed to st_sem_post sem_empty.");
            goto ERR;
        }
    }

    thr->words = words;
    thr->sents = sents;
    thr->logp = logp;

    return NULL;
ERR:
    connlm->err = -1;

    /* TODO: unlock mutex and post semaphore */
    return NULL;
}

int connlm_setup_eval(connlm_t *connlm, connlm_opt_t *connlm_opt,
        const char *eval_file)
{
    if (connlm_setup_read(connlm, connlm_opt, eval_file) < 0) {
        ST_WARNING("Failed to connlm_setup_read.");
        return -1;
    }

    if (connlm_setup(connlm, false) < 0) {
        ST_WARNING("Failed to connlm_setup.");
        return -1;
    }

    return 0;
}

int connlm_eval(connlm_t *connlm, FILE *fp_log)
{
    connlm_thr_t *thrs = NULL;
    pthread_t *pts = NULL;

    connlm_read_thr_t read_thr;
    int n_thr;

    count_t words;
    count_t sents;
    double logp;

    int i;

    struct timeval tts_eval, tte_eval;
    long ms;

    ST_CHECK_PARAM(connlm == NULL, -1);

    if (fp_log != NULL) {
        connlm->fp_log = fp_log;
        pthread_mutex_init(&connlm->fp_lock, NULL);
        if (connlm->fp_log != NULL
                && (! connlm->eval_opt.print_sent_prob)) {
            fprintf(fp_log, "Index   logP(NET)          Word\n");
            fprintf(fp_log, "----------------------------------\n");
        }
        ST_NOTICE("Printing Probs, setting num_thread to 1.")
            connlm->opt.num_thread = 1;
    }

    gettimeofday(&tts_eval, NULL);

    n_thr = connlm->opt.num_thread;
    pts = (pthread_t *)malloc((n_thr + 1) * sizeof(pthread_t));
    if (pts == NULL) {
        ST_WARNING("Failed to malloc pts");
        goto ERR;
    }

    thrs = (connlm_thr_t *)malloc(sizeof(connlm_thr_t) * n_thr);
    if (thrs == NULL) {
        ST_WARNING("Failed to malloc thrs");
        goto ERR;
    }
    memset(thrs, 0, sizeof(connlm_thr_t) * n_thr);

    connlm->err = 0;

    read_thr.connlm = connlm;
    read_thr.thrs = thrs;
    read_thr.n_thr = connlm->opt.num_thread;
    read_thr.epoch_size = connlm->opt.epoch_size;
    read_thr.shuffle = false;
    read_thr.random = 0;

    pthread_create(pts + n_thr, NULL,
            connlm_read_thread, (void *)&read_thr);

    for (i = 0; i < n_thr; i++) {
        thrs[i].connlm = connlm;
        thrs[i].tid = i;
        pthread_create(pts + i, NULL, connlm_eval_thread,
                (void *)(thrs + i));
    }

    for (i = 0; i < n_thr; i++) {
        pthread_join(pts[i], NULL);
    }

    if (connlm->err != 0) {
        ST_WARNING("Testing error.");
        goto ERR;
    }

    words = 0;
    sents = 0;
    logp = 0;

    for (i = 0; i < n_thr; i++) {
        words += thrs[i].words;
        sents += thrs[i].sents;
        logp += thrs[i].logp;
    }

    if (words != read_thr.words || sents != read_thr.sents) {
        ST_WARNING("words[" COUNT_FMT "] != read_words[" COUNT_FMT "] "
                "or sents[" COUNT_FMT "] != read_sents[" COUNT_FMT "] ",
                words, read_thr.words, sents, read_thr.sents);
    }

    gettimeofday(&tte_eval, NULL);
    ms = TIMEDIFF(tts_eval, tte_eval);

    ST_NOTICE("Finish eval in %ldms.", ms);

    ST_NOTICE("Words: " COUNT_FMT "    OOVs: " COUNT_FMT
            "    Sentences: " COUNT_FMT
            ", words/sec: %.1f", words, read_thr.oovs, sents,
             words / ((double) ms / 1000));
    ST_NOTICE("LogP: %f", logp);
    ST_NOTICE("Entropy: %f", -logp / log10(2) / words);
    ST_NOTICE("PPL: %f", exp10(-logp / (double) words));

    if (fp_log != NULL
            && (! connlm->eval_opt.print_sent_prob)) {
        fprintf(fp_log, "\nSummary:\n");
        fprintf(fp_log, "Words: " COUNT_FMT "    OOVs: " COUNT_FMT "\n",
                words, read_thr.oovs);
        fprintf(fp_log, "LogP: %f\n", logp);
        fprintf(fp_log, "Entropy: %f\n", -logp / log10(2) / words);
        fprintf(fp_log, "PPL: %f\n", exp10(-logp / (double) words));

        pthread_mutex_destroy(&connlm->fp_lock);
    }

    safe_free(pts);
    safe_free(thrs);

    return 0;

ERR:

    if (fp_log != NULL) {
        pthread_mutex_destroy(&connlm->fp_lock);
    }
    safe_free(pts);
    safe_free(thrs);

    return -1;
}

int connlm_setup_gen(connlm_t *connlm, connlm_gen_opt_t *gen_opt)
{
    ST_CHECK_PARAM(connlm == NULL || gen_opt == NULL, -1);

    connlm->gen_opt = *gen_opt;

    st_srand(gen_opt->rand_seed);

    if (connlm_setup(connlm, 1) < 0) {
        ST_WARNING("Failed to connlm_setup.");
        return -1;
    }

    return 0;
}

static int connlm_gen_word(connlm_t *connlm)
{
    int word;

    ST_CHECK_PARAM(connlm == NULL, -1);

    if (connlm_forward_hidden(connlm, 0) < 0) {
        ST_WARNING("Failed to connlm_forward_hidden.");
        return -1;
    }

    word = output_gen_word(connlm->output, 0);
    if (word < 0) {
        ST_WARNING("Failed to output_gen_word.");
        return -1;
    }

    return word;
}

int connlm_gen(connlm_t *connlm, int num_sents)
{
    count_t words;
    count_t sents;

    FILE *text_fp = NULL;

    connlm_egs_t egs = {
        .words = NULL,
        .size = 0,
        .capacity = 0,
    };

    int word;
    int i;

    struct timeval tts_gen, tte_gen;
    long ms;

    ST_CHECK_PARAM(connlm == NULL || num_sents < 0, -1);

    if (connlm->gen_opt.prefix_file[0] != '\0') {
        text_fp = st_fopen(connlm->gen_opt.prefix_file, "rb");
        if (text_fp == NULL) {
            ST_WARNING("Failed to open prefix file[%s]",
                    connlm->gen_opt.prefix_file);
            goto ERR;
        }
    }

    gettimeofday(&tts_gen, NULL);
    sents = 0;
    words = 0;
    while(true) {
        if (connlm_reset(connlm, 0, false) < 0) {
            ST_WARNING("Failed to connlm_reset.");
            goto ERR;
        }
        if (text_fp != NULL && !feof(text_fp)) {
            if (connlm_get_egs(&egs, NULL, 1, text_fp,
                        connlm->vocab, NULL) < 0) {
                ST_WARNING("Failed to connlm_get_egs.");
                goto ERR;
            }

            if (egs.size > 1) {
                printf("[");
                for (i = 0; i < egs.size - 1; i++) {
                    word = egs.words[i];
                    if (connlm_forward(connlm, word, 0) < 0) {
                        ST_WARNING("Failed to connlm_forward.");
                        goto ERR;
                    }

                    if (connlm_end(connlm, word, 0, false) < 0) {
                        ST_WARNING("connlm_end_gen.");
                        goto ERR;
                    }

                    printf("%s", vocab_get_word(connlm->vocab, word));
                    if (i < egs.size - 2) {
                        printf(" ");
                    }
                }
                printf("] ");
            } else if (feof(text_fp)) {
                break;
            }
        }

        word = -1;
        while (word != SENT_END_ID) {
            if (word != -1) { // not first word
                printf(" ");
            }

            word = connlm_gen_word(connlm);
            if (word < 0) {
                ST_WARNING("Failed to connlm_gen_word.");
                goto ERR;
            }

            if (word == SENT_END_ID) {
                printf("\n");
            } else {
                printf("%s", vocab_get_word(connlm->vocab, word));
            }
            fflush(stdout);

            if (connlm_end(connlm, word, 0, false) < 0) {
                ST_WARNING("connlm_end_gen.");
                goto ERR;
            }

            words++;
        }

        sents++;
        if (sents >= num_sents) {
            if(text_fp != NULL) {
                if (feof(text_fp)) {
                    break;
                }
            } else {
                break;
            }
        }
    }

    gettimeofday(&tte_gen, NULL);
    ms = TIMEDIFF(tts_gen, tte_gen);

    ST_NOTICE("Finish generating in %ldms.", ms);

    ST_NOTICE("Words: " COUNT_FMT "    Sentences: " COUNT_FMT
            ", words/sec: %.1f", words, sents,
             words / ((double) ms / 1000));

    return 0;

ERR:

    return -1;
}
#endif

int driver_setup(driver_t *driver, driver_mode_t mode)
{
    int i;
    bool backprop;

    ST_CHECK_PARAM(driver == NULL, -1);

    if (mode == DRIVER_TRAIN) {
        backprop = true;
    } else {
        backprop = false;
    }

    driver->mode = mode;

    for (i = 0; i < driver->n_thr; i++) {
        if (updater_setup(driver->updaters[i], backprop) < 0) {
            ST_WARNING("Failed to updater_setup.");
            return -1;
        }
    }

    return 0;
}

int driver_run(driver_t *driver)
{
    thr_stat_t *stats = NULL;

    ST_CHECK_PARAM(driver == NULL, -1);

    if (driver->mode == DRIVER_EVAL) {
        if (driver->eval_opt == NULL) {
            ST_WARNING("Eval opt not set.");
            return -1;
        }
    } else if (driver->mode == DRIVER_GEN) {
        if (driver->gen_opt == NULL) {
            ST_WARNING("Gen opt not set.");
            return -1;
        }
    }

    driver->err = 0;

    if (driver->reader != NULL) {
        stats = (thr_stat_t *)malloc(sizeof(thr_stat_t) * driver->n_thr);
        if (stats == NULL) {
            ST_WARNING("Failed to malloc stats");
            goto ERR;
        }
        memset(stats, 0, sizeof(thr_stat_t) * driver->n_thr);


        if (reader_read(driver->reader, stats, &driver->err) < 0) {
            ST_WARNING("Failed to reader_read.");
            goto ERR;
        }
    }

    if (driver->reader != NULL) {
        if(reader_wait(driver->reader) != 0) {
            ST_WARNING("Failed to reader_wait.");
            goto ERR;
        }
    }

    safe_free(stats);

    return 0;

ERR:

    safe_free(stats);
    return -1;
}

int driver_set_eval(driver_t *driver, driver_eval_opt_t *eval_opt,
        FILE *fp_log)
{
    ST_CHECK_PARAM(driver == NULL || eval_opt == NULL, -1);

    driver->eval_opt = eval_opt;
    driver->fp_log = fp_log;

    return 0;
}

int driver_set_gen(driver_t *driver, driver_gen_opt_t *gen_opt,
        int num_sents)
{
    ST_CHECK_PARAM(driver == NULL || gen_opt == NULL, -1);

    driver->gen_opt = gen_opt;
    driver->gen_num_sents = num_sents;

    return 0;
}
