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

    ST_CHECK_PARAM(connlm == NULL || n_thr <= 0, NULL);

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
    memset(driver->updaters, 0, sizeof(updater_t*) * n_thr);

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

int driver_setup(driver_t *driver, driver_mode_t mode)
{
    int i;
    bool backprop;

    ST_CHECK_PARAM(driver == NULL, -1);

    if (mode == DRIVER_TRAIN) {
        backprop = true;
    } else {
        backprop = false;
        if (mode == DRIVER_GEN) {
            st_srand(driver->gen_opt.rand_seed);
        }
    }

    driver->mode = mode;

    if (connlm_setup(driver->connlm) < 0) {
        ST_WARNING("Failed to connlm_setup.");
        return -1;
    }

    for (i = 0; i < driver->n_thr; i++) {
        if (updater_setup(driver->updaters[i], backprop) < 0) {
            ST_WARNING("Failed to updater_setup.");
            return -1;
        }
    }

    if (mode == DRIVER_GEN) {
        if (driver->updaters[0]->input_updater->ctx_rightmost > 0) {
            ST_WARNING("Can not generating: future words in input context.");
            return -1;
        }
    }

    return 0;
}

int driver_set_eval(driver_t *driver, driver_eval_opt_t *eval_opt, FILE *fp_log)
{
    ST_CHECK_PARAM(driver == NULL || eval_opt == NULL, -1);

    driver->eval_opt = *eval_opt;
    driver->fp_log = fp_log;

    return 0;
}

int driver_set_gen(driver_t *driver, driver_gen_opt_t *gen_opt, int num_sents)
{
    ST_CHECK_PARAM(driver == NULL || gen_opt == NULL || num_sents < 0, -1);

    driver->gen_opt = *gen_opt;
    driver->gen_num_sents = num_sents;

    return 0;
}

static int driver_steps(driver_t *driver, int tid, double *logp,
        double *logp_sent, count_t *sents, count_t *words)
{
    updater_t *updater;

    int word;

    ST_CHECK_PARAM(driver == NULL || tid < 0 || logp == NULL
            || logp_sent == NULL || sents == NULL || words == NULL, -1);

    updater = driver->updaters[tid];

    while (updater_steppable(updater)) {
        word = updater_step(updater);
        if (word < 0) {
            ST_WARNING("Failed to updater_step.");
            return -1;
        }

        *logp += updater->logp;
        if (driver->fp_log != NULL
                    && driver->eval_opt.print_sent_prob) {
            *logp_sent += logn(updater->logp, driver->eval_opt.out_log_base);
        }

        if ((*logp != *logp) || (isinf(*logp))) {
            ST_WARNING("Numerical error. tid[%d], log(p_word(%d)) = %g",
                    tid, word, updater->logp);
            return -1;
        }

        if (driver->fp_log != NULL
                && (! driver->eval_opt.print_sent_prob)) {
            fprintf(driver->fp_log, "%d\t%.6f\t%s", word,
                    logn(updater->logp, driver->eval_opt.out_log_base),
                    vocab_get_word(updater->connlm->vocab, word));

            fprintf(driver->fp_log, "\n");
        }

        if (word == SENT_END_ID) {
            if (driver->fp_log != NULL
                    && driver->eval_opt.print_sent_prob) {
                fprintf(driver->fp_log, "%.6f\n", *logp_sent);
                *logp_sent = 0.0;
            }
            ++(*sents);
        }
        ++(*words);
    }

    return 0;
}

typedef struct _driver_thread_t_ {
    driver_t *driver;
    int tid;
    thr_stat_t *stat;
} driver_thr_t;

static void* driver_thread(void *args)
{
    driver_thr_t *thr;
    driver_t *driver;
    updater_t *updater;
    reader_t *reader;
    int tid;

    connlm_egs_t *egs = NULL;

    count_t words;
    count_t sents;
    double logp;
    double logp_sent;

    struct timeval tts, tte;
    long ms;

    struct timeval tts_wait, tte_wait;
    long ms_wait = 0;

    ST_CHECK_PARAM(args == NULL, NULL);

    thr = (driver_thr_t *)args;
    tid = thr->tid;
    driver = thr->driver;
    updater = driver->updaters[tid];
    reader = driver->reader;

    gettimeofday(&tts, NULL);

    words = 0;
    sents = 0;
    logp = 0.0;
    logp_sent = 0.0;
    while (true) {
        if (driver->err != 0) {
            break;
        }

        gettimeofday(&tts_wait, NULL);
        egs = reader_hold_egs(reader);
        gettimeofday(&tte_wait, NULL);

        if (egs == NULL) { // finish
            if (updater_finalize(updater) < 0) {
                ST_WARNING("Failed to updater_finalize.");
                goto ERR;
            }

            if (driver_steps(driver, tid, &logp, &logp_sent,
                        &sents, &words) < 0) {
                ST_WARNING("Failed to driver_steps.");
                goto ERR;
            }
            break;
        }

        if (updater_feed(updater, egs->words, egs->size) < 0) {
            ST_WARNING("Failed to updater_feed.");
            goto ERR;
        }

        if (driver_steps(driver, tid, &logp, &logp_sent, &sents, &words) < 0) {
            ST_WARNING("Failed to driver_steps.");
            goto ERR;
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
                logp, -logp / log(2) / words,
                exp(-logp / (double) words),
                (ms - ms_wait) / 1000.0, (ms - ms_wait) / (ms / 100.0),
                ms_wait / 1000.0, ms_wait / (ms / 100.0),
                TIMEDIFF(tts_wait, tte_wait) / 1000.0);

        if (reader_release_egs(reader, egs) < 0) {
            ST_WARNING("Failed to reader_release_egs.");
            goto ERR;
        }
    }

    return NULL;
ERR:
    driver->err = -1;

    return NULL;
}

static int driver_do_run(driver_t *driver)
{
    driver_thr_t *thrs = NULL;
    pthread_t *pts = NULL;
    thr_stat_t *stats = NULL;

    count_t words;
    count_t sents;
    double logp;
    struct timeval tts, tte;
    long ms;

    int n_thr;
    int i;

    ST_CHECK_PARAM(driver == NULL, -1);

    if (driver->mode == DRIVER_EVAL) {
        if (driver->fp_log != NULL) {
            if (! driver->eval_opt.print_sent_prob) {
                fprintf(driver->fp_log, "Index   logP(NET)          Word\n");
                fprintf(driver->fp_log, "----------------------------------\n");
            }
            ST_NOTICE("Printing Probs, setting num_thread to 1.");
            driver->n_thr = 1;
        }
    }

    gettimeofday(&tts, NULL);

    n_thr = driver->n_thr;
    pts = (pthread_t *)malloc(n_thr * sizeof(pthread_t));
    if (pts == NULL) {
        ST_WARNING("Failed to malloc pts");
        goto ERR;
    }

    thrs = (driver_thr_t *)malloc(sizeof(driver_thr_t) * n_thr);
    if (thrs == NULL) {
        ST_WARNING("Failed to malloc thrs");
        goto ERR;
    }
    memset(thrs, 0, sizeof(driver_thr_t) * n_thr);

    driver->err = 0;

    stats = (thr_stat_t *)malloc(sizeof(thr_stat_t) * n_thr);
    if (stats == NULL) {
        ST_WARNING("Failed to malloc stats");
        goto ERR;
    }
    memset(stats, 0, sizeof(thr_stat_t) * n_thr);

    if (reader_read(driver->reader, stats, &driver->err) < 0) {
        ST_WARNING("Failed to reader_read.");
        goto ERR;
    }

    for (i = 0; i < n_thr; i++) {
        thrs[i].driver = driver;
        thrs[i].tid = i;
        thrs[i].stat = stats + i;
        if (pthread_create(pts + i, NULL, driver_thread,
                    (void *)(thrs + i)) != 0) {
            ST_WARNING("Falied to pthread_create driver_thread.");
            goto ERR;
        }
    }

    for (i = 0; i < n_thr; i++) {
        if (pthread_join(pts[i], NULL) != 0) {
            ST_WARNING("Falied to pthread_join.");
            goto ERR;
        }
    }

    if(reader_wait(driver->reader) != 0) {
        ST_WARNING("Failed to reader_wait.");
        goto ERR;
    }

    if (driver->err != 0) {
        goto ERR;
    }

    gettimeofday(&tte, NULL);
    ms = TIMEDIFF(tts, tte);

    words = 0;
    sents = 0;
    logp = 0.0;
    for (i = 0; i < n_thr; i++) {
        words += stats[i].words;
        sents += stats[i].sents;
        logp += stats[i].logp;
    }

    if (words != driver->reader->words || sents != driver->reader->sents) {
        ST_WARNING("words[" COUNT_FMT "] != read_words[" COUNT_FMT "] "
                "or sents[" COUNT_FMT "] != read_sents[" COUNT_FMT "] ",
                words, driver->reader->words,
                sents, driver->reader->sents);
    }

    if (driver->mode == DRIVER_TRAIN || driver->mode == DRIVER_EVAL) {
        ST_NOTICE("Finish %s in %ldms.", driver->mode == DRIVER_TRAIN
                ? "training" : "evaluating", ms);
        ST_NOTICE("Words: " COUNT_FMT ", Sentences: " COUNT_FMT
                ", OOVs: " COUNT_FMT ", words/sec: %.1f", words, sents,
                driver->reader->oovs, words / ((double) ms / 1000.0));
        ST_NOTICE("LogP: %f", logp);
        ST_NOTICE("Entropy: %f", -logp / log(2) / words);
        ST_NOTICE("PPL: %f", exp(-logp / (double) words));
    }

    if (driver->mode == DRIVER_EVAL) {
        if (driver->fp_log != NULL && (!driver->eval_opt.print_sent_prob)) {
            fprintf(driver->fp_log, "\nSummary:\n");
            fprintf(driver->fp_log, "Words: " COUNT_FMT
                    "    OOVs: " COUNT_FMT "\n",
                    words, driver->reader->oovs);
            fprintf(driver->fp_log, "LogP: %f\n", logp);
            fprintf(driver->fp_log, "Entropy: %f\n",
                    -logp / log(2) / words);
            fprintf(driver->fp_log, "PPL: %f\n",
                    exp(-logp / (double) words));
        }
    }

    safe_free(pts);
    safe_free(thrs);
    safe_free(stats);

    return 0;

ERR:

    safe_free(pts);
    safe_free(thrs);
    safe_free(stats);
    return -1;
}

static int driver_gen(driver_t *driver)
{
    FILE *text_fp = NULL;
    updater_t *updater;
    vocab_t *vocab;

    count_t n_word, n_sent;

    connlm_egs_t egs = {
        .words = NULL,
        .size = 0,
        .capacity = 0,
    };

    int word;
    int i;
    bool first;

    struct timeval tts, tte;
    long ms;

    ST_CHECK_PARAM(driver == NULL, -1);

    updater = driver->updaters[0];
    vocab = driver->connlm->vocab;

    if (driver->gen_opt.prefix_file[0] != '\0') {
        text_fp = st_fopen(driver->gen_opt.prefix_file, "rb");
        if (text_fp == NULL) {
            ST_WARNING("Failed to open prefix file[%s]",
                    driver->gen_opt.prefix_file);
            goto ERR;
        }
    }

    gettimeofday(&tts, NULL);
    n_sent = 0;
    n_word = 0;
    while(true) {
        first = true;
        if (text_fp != NULL && !feof(text_fp)) {
            if (connlm_egs_read(&egs, NULL, 1, text_fp, vocab, NULL) < 0) {
                ST_WARNING("Failed to connlm_egs_read.");
                goto ERR;
            }

            if (egs.size > 1) {
                printf("[");
                for (i = 0; i < egs.size - 1; i++) {
                    printf("%s", vocab_get_word(vocab, egs.words[i]));
                    if (i < egs.size - 2) {
                        printf(" ");
                    }
                }
                printf("]");

                if (updater_feed(updater, egs.words, egs.size - 1) < 0) {
                    ST_WARNING("Failed to updater_feed.");
                    goto ERR;
                }

                /* steps the prefix words. */
                while (updater_steppable(updater)) {
                    word = updater_step(updater);
                    if (word < 0) {
                        ST_WARNING("Failed to updater_step.");
                        return -1;
                    }
                    first = false;
                }
            } else if (feof(text_fp)) {
                break;
            }
        }

        word = -1;
        while (word != SENT_END_ID) {
            word = updater_sampling(updater);
            if (word < 0) {
                ST_WARNING("Failed to updater_sampling.");
                goto ERR;
            }

            if (word == SENT_END_ID) {
                printf("\n");
            } else {
                if (!first) {
                    printf(" ");
                }
                printf("%s", vocab_get_word(vocab, word));
            }
            fflush(stdout);

            first = false;
            n_word++;
        }

        n_sent++;
        if (n_sent >= driver->gen_num_sents) {
            if(text_fp != NULL) {
                if (feof(text_fp)) {
                    break;
                }
            } else {
                break;
            }
        }
    }

    gettimeofday(&tte, NULL);
    ms = TIMEDIFF(tts, tte);

    ST_NOTICE("Finish generating in %ldms.", ms);
    ST_NOTICE("Words: " COUNT_FMT "    Sentences: " COUNT_FMT
            ", words/sec: %.1f", n_word, n_sent,
            n_word / ((double) ms / 1000));

    safe_fclose(text_fp);
    return 0;

ERR:
    safe_fclose(text_fp);
    return -1;
}

int driver_run(driver_t *driver)
{
    ST_CHECK_PARAM(driver == NULL, -1);

    if (driver->mode == DRIVER_EVAL || driver->mode == DRIVER_TRAIN) {
        if (driver_do_run(driver) < 0) {
            ST_WARNING("Failed to driver_do_run.");
            return -1;
        }
    } else if (driver->mode == DRIVER_GEN) {
        if (driver_gen(driver) < 0) {
            ST_WARNING("Failed to driver_gen.");
            return -1;
        }
    } else {
        ST_WARNING("Unknown mode[%d]", driver->mode);
        return -1;
    }

    return 0;
}
