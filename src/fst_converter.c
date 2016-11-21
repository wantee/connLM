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

#include "fst_converter.h"

#define FST_INIT_STATE 0
#define FST_FINAL_STATE 1

int fst_converter_load_opt(fst_converter_opt_t *converter_opt,
        st_opt_t *opt, const char *sec_name)
{
    char method_str[MAX_ST_CONF_LEN];

    ST_CHECK_PARAM(converter_opt == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_BOOL(opt, sec_name, "PRINT_SYMS",
            converter_opt->print_syms, false,
            "Print symbols instead of numbers, if true. ");

    ST_OPT_SEC_GET_STR(opt, sec_name, "BACKOFF_METHOD",
            method_str, MAX_ST_CONF_LEN, "Beam",
            "Backoff method(Beam/Sampling)");
    if (strcasecmp(method_str, "beam") == 0) {
        converter_opt->bom = BOM_BEAM;

        ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "BEAM",
                converter_opt->beam, 0.0, "Threshold for beam.");
        if (converter_opt->beam < 0.0) {
            ST_WARNING("beam must be larger than or equal to zero.");
            goto ST_OPT_ERR;
        }
    } else if (strcasecmp(method_str, "sampling") == 0) {
        converter_opt->bom = BOM_SAMPLING;

        ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "BOOST",
                converter_opt->boost, 0.0,
                "Boost probability for sampling.");
        if (converter_opt->boost < 0.0 || converter_opt->boost > 1.0) {
            ST_WARNING("boost must be in [0, 1].");
            goto ST_OPT_ERR;
        }

        ST_OPT_SEC_GET_UINT(opt, sec_name, "INIT_RANDOM_SEED",
                converter_opt->init_rand_seed, (unsigned int)time(NULL),
                "Initial random seed, seed for each thread would be "
                "incremented from this value. "
                "Default is value of time(NULl).");
    } else {
        ST_WARNING("Unknown backoff method[%s].", method_str);
        goto ST_OPT_ERR;
    }

    return 0;

ST_OPT_ERR:
    return -1;
}

void fst_converter_destroy(fst_converter_t *converter)
{
    int i;

    if (converter == NULL) {
        return;
    }

    converter->connlm = NULL;

    for(i = 0; i < converter->n_thr; i++) {
        safe_updater_destroy(converter->updaters[i]);
    }
    safe_free(converter->updaters);
    converter->n_thr = 0;

    (void)pthread_mutex_destroy(&converter->n_fst_state_lock);

    safe_st_block_cache_destroy(converter->model_state_cache);
    (void)pthread_mutex_destroy(&converter->model_state_cache_lock);

    safe_free(converter->fst_state_infos);
    (void)pthread_mutex_destroy(&converter->fst_state_info_lock);
    converter->cap_infos = 0;

    (void)pthread_mutex_destroy(&converter->fst_fp_lock);
    converter->fst_fp = NULL;
}

fst_converter_t* fst_converter_create(connlm_t *connlm, int n_thr,
        fst_converter_opt_t *converter_opt)
{
    fst_converter_t *converter = NULL;

    int i;
    int state_size;
    int count;

    ST_CHECK_PARAM(connlm == NULL || n_thr <= 0 || converter_opt == NULL, NULL);

    converter = (fst_converter_t *)malloc(sizeof(fst_converter_t));
    if (converter == NULL) {
        ST_WARNING("Failed to malloc converter.");
        return NULL;
    }
    memset(converter, 0, sizeof(fst_converter_t));

    converter->connlm = connlm;
    converter->n_thr = n_thr;
    converter->converter_opt = *converter_opt;

    converter->updaters = (updater_t **)malloc(sizeof(updater_t*)*n_thr);
    if (converter->updaters == NULL) {
        ST_WARNING("Failed to malloc updaters.");
        goto ERR;
    }
    memset(converter->updaters, 0, sizeof(updater_t*) * n_thr);

    for (i = 0; i < converter->n_thr; i++) {
        converter->updaters[i] = updater_create(connlm);
        if (converter->updaters[i] == NULL) {
            ST_WARNING("Failed to malloc updater[%d].", i);
            goto ERR;
        }
    }

    if (pthread_mutex_init(&converter->n_fst_state_lock, NULL) != 0) {
        ST_WARNING("Failed to pthread_mutex_init n_fst_state_lock.");
        goto ERR;
    }

    count = (int)sqrt(connlm->vocab->vocab_size) * connlm->vocab->vocab_size;

    state_size = updater_state_size(converter->updaters[0]);
    converter->model_state_cache = st_block_cache_create(
            sizeof(real_t) * state_size, 5 * count, count);
    if (converter->model_state_cache == NULL) {
        ST_WARNING("Failed to st_block_cache_create model_state_cache.");
        goto ERR;
    }
    if (pthread_mutex_init(&converter->model_state_cache_lock, NULL) != 0) {
        ST_WARNING("Failed to pthread_mutex_init model_state_cache_lock.");
        goto ERR;
    }

    converter->cap_infos = count;
    converter->fst_state_infos = (fst_state_info_t *)malloc(
            sizeof(fst_state_info_t) * count);
    if (converter->fst_state_infos == NULL) {
        ST_WARNING("Failed to st_block_cache_create fst_state_infos.");
        goto ERR;
    }
    if (pthread_mutex_init(&converter->fst_state_info_lock, NULL) != 0) {
        ST_WARNING("Failed to pthread_mutex_init fst_state_info_lock.");
        goto ERR;
    }

    return converter;

ERR:
    safe_fst_converter_destroy(converter);
    return NULL;
}

static int fst_converter_setup(fst_converter_t *converter, FILE *fst_fp)
{
    int i;

    ST_CHECK_PARAM(converter == NULL, -1);

    converter->fst_fp = fst_fp;

    if (pthread_mutex_init(&converter->fst_fp_lock, NULL) != 0) {
        ST_WARNING("Failed to pthread_mutex_init fst_fp_lock.");
        return -1;
    }

    if (connlm_setup(converter->connlm) < 0) {
        ST_WARNING("Failed to connlm_setup.");
        return -1;
    }

    for (i = 0; i < converter->n_thr; i++) {
        if (updater_setup(converter->updaters[i], false) < 0) {
            ST_WARNING("Failed to updater_setup.");
            return -1;
        }
    }

    return 0;
}

static int fst_converter_maybe_realloc_infos(fst_converter_t *converter)
{
    int needed;
    int ret;

    ST_CHECK_PARAM(converter == NULL, -1);

    if (pthread_mutex_lock(&converter->fst_state_info_lock) != 0) {
        ST_WARNING("Failed to pthread_mutex_lock fst_state_info_lock.");
        return -1;
    }
    if (converter->n_fst_state < converter->cap_infos) {
        ret = 0;
        goto RET;
    }

    needed = max(converter->cap_infos + converter->connlm->vocab->vocab_size,
            converter->n_fst_state);

    converter->fst_state_infos = realloc(converter->fst_state_infos,
            sizeof(fst_state_info_t) * needed);
    if (converter->fst_state_infos == NULL) {
        ST_WARNING("Failed to realloc fst_state_infos.");
        ret = -1;
        goto RET;
    }

    ret = 0;
RET:
    if (pthread_mutex_unlock(&converter->fst_state_info_lock) != 0) {
        ST_WARNING("Failed to pthread_mutex_lock fst_state_info_lock.");
        return -1;
    }

    return ret;
}

static int fst_converter_add_state(fst_converter_t *converter)
{
    int sid;

    ST_CHECK_PARAM(converter == NULL, -1);

    if (pthread_mutex_lock(&converter->n_fst_state_lock) != 0) {
        ST_WARNING("Failed to pthread_mutex_lock n_fst_state_lock.");
        return -1;
    }

    sid = converter->n_fst_state++;

    if (fst_converter_maybe_realloc_infos(converter) < 0) {
        ST_WARNING("Failed to fst_converter_maybe_realloc_infos.");
        return -1;
    }
    if (pthread_mutex_unlock(&converter->n_fst_state_lock) != 0) {
        ST_WARNING("Failed to pthread_mutex_unlock n_fst_state_lock.");
        return -1;
    }

    return sid;
}

static int fst_converter_print_arc(fst_converter_t *converter,
        int from, int to, int wid)
{
    char *word;
    int ret;

    if (pthread_mutex_lock(&converter->fst_fp_lock) != 0) {
        ST_WARNING("Failed to pthread_mutex_lock fst_fp_lock.");
        return -1;
    }
    if (converter->converter_opt.print_syms) {
        word = vocab_get_word(converter->connlm->vocab, wid);
        ret = fprintf(converter->fst_fp, "%d\t%d\t%s\t%s\n", from, to,
                word, word);
    } else {
        ret = fprintf(converter->fst_fp, "%d\t%d\t%d\t%d\n", from, to,
                wid, wid);
    }
    if (pthread_mutex_unlock(&converter->fst_fp_lock) != 0) {
        ST_WARNING("Failed to pthread_mutex_lock fst_fp_lock.");
        return -1;
    }

    return ret;
}

static int boost_sampling(double *probs, int n_probs,
        double boost_value, unsigned int *rand_seed)
{
    int word;

    double u, p;

    while (true) {
        u = st_random_r(0, 1, rand_seed);

        p = 0;
        for (word = 0; word < n_probs; word++) {
            if (word == SENT_END_ID) {
                p += probs[word] + boost_value;
            } else {
                p += probs[word] - boost_value / (n_probs - 1);
            }

            if (p >= u) {
                break;
            }
        }

        if (word == SENT_START_ID) { // FIXME: <any>
            continue;
        }

        break;
    }

    return word;
}

static int select_word(fst_converter_t *converter, int last_word,
        double *output_probs, unsigned int *rand_seed)
{
    int vocab_size;
    int word;

    ST_CHECK_PARAM(converter == NULL || last_word < -1, -1);

    vocab_size = converter->connlm->vocab->vocab_size;

    switch (converter->converter_opt.bom) {
        case BOM_BEAM:
            word = last_word + 1;
            while (word < vocab_size) {
                if (word == SENT_END_ID || word == SENT_START_ID) {
                    // FIXME: <any>
                    continue;
                }
                if (output_probs[word] >= output_probs[SENT_END_ID]
                        - converter->converter_opt.beam) {
                    return word;
                }
                ++word;
            }

            return SENT_END_ID;
            break;
        case BOM_SAMPLING:
            return boost_sampling(output_probs, vocab_size,
                    converter->converter_opt.boost, rand_seed);
            break;
        default:
            ST_WARNING("Unkown backoff method");
            return -1;
    }

    return -1;
}

static int fst_converter_expand(fst_converter_t *converter,
        int tid, int sid, double *output_probs, unsigned int *rand_seed)
{
    updater_t *updater;
    real_t *state;
    real_t *new_state;

    int new_sid;
    int model_state_id;
    int word;

    ST_CHECK_PARAM(converter == NULL || sid < 0, -1);

    updater = converter->updaters[tid];

    if (converter->fst_state_infos[sid].model_state_id < 0) {
        // <s> state
        state = NULL;
    } else {
        state = (real_t *)st_block_cache_fetch(converter->model_state_cache,
                &(converter->fst_state_infos[sid].model_state_id));
        if (state == NULL) {
            ST_WARNING("Failed to st_block_cache_fetch state.");
            return -1;
        }
    }

    //forward(updater, state, converter->fst_state_infos[sid].word_id, -1)

    model_state_id = -1;
    new_state = (real_t *)st_block_cache_fetch(
            converter->model_state_cache, &model_state_id);
    if (new_state == NULL) {
        ST_WARNING("Failed to st_block_cache_fetch new state.");
        return -1;
    }

    if (updater_dump_state(updater, new_state) < 0) {
        ST_WARNING("Failed to updater_dump_state.");
        return -1;
    }

    // converter->output_probs =

    word = -1;
    while(true) {
        word = select_word(converter, word, output_probs, rand_seed);
        if (word < 0) {
            ST_WARNING("Failed to select_word.");
            return -1;
        }

        new_sid = fst_converter_add_state(converter);
        if (new_sid < 0) {
            ST_WARNING("Failed to fst_converter_add_state.");
            return -1;
        }
        converter->fst_state_infos[new_sid].model_state_id = model_state_id;
        converter->fst_state_infos[new_sid].word_id = word;

        if (fst_converter_print_arc(converter, sid, new_sid, word) < 0) {
            ST_WARNING("Failed to fst_converter_print_arc.");
            return -1;
        }

        if (word == SENT_END_ID) {
            break;
        }
    }

    return 0;
}

typedef struct fst_converter_thread_args_t_ {
    fst_converter_t *converter;
    int tid;
    int sid;

    unsigned int rand_seed;
    double *output_probs;
} converter_targs_t;

static void* converter_worker(void *args)
{
    converter_targs_t *targs;
    fst_converter_t *converter;

    targs = (converter_targs_t *)args;
    converter = targs->converter;

    if (fst_converter_expand(converter, targs->tid, targs->sid,
                targs->output_probs, &targs->rand_seed) < 0) {
        ST_WARNING("Failed to fst_converter_expand.");
        goto ERR;
    }

    return NULL;

ERR:
    converter->err = -1;

    return NULL;
}

int fst_converter_convert(fst_converter_t *converter, FILE *fst_fp)
{
    converter_targs_t *targs = NULL;
    pthread_t *pts = NULL;
    int sid;
    int i, n;

    ST_CHECK_PARAM(converter == NULL || fst_fp == NULL, -1);

    if (fst_converter_setup(converter, fst_fp) < 0) {
        ST_WARNING("Failed to fst_converter_setup.");
        goto ERR;
    }

    pts = (pthread_t *)malloc(sizeof(pthread_t) * converter->n_thr);
    if (pts == NULL) {
        ST_WARNING("Failed to malloc pts");
        goto ERR;
    }

    targs = (converter_targs_t *)malloc(sizeof(converter_targs_t)
            * converter->n_thr);
    if (targs == NULL) {
        ST_WARNING("Failed to malloc targs");
        goto ERR;
    }
    memset(targs, 0, sizeof(converter_targs_t) * converter->n_thr);

    for (i = 0; i < converter->n_thr; i++) {
        targs[i].converter = converter;
        targs[i].tid = i;

        targs[i].output_probs = (double *)malloc(sizeof(double)
                * converter->connlm->vocab->vocab_size);
        if (targs[i].output_probs == NULL) {
            ST_WARNING("Failed to malloc output_probs.");
            goto ERR;
        }
        targs[i].rand_seed = converter->converter_opt.init_rand_seed + i;
    }

    if (fst_converter_add_state(converter) != FST_INIT_STATE) {
        ST_WARNING("Failed to fst_converter_add_state init state.");
        goto ERR;
    }
    if (fst_converter_add_state(converter) != FST_FINAL_STATE) {
        ST_WARNING("Failed to fst_converter_add_state final state.");
        goto ERR;
    }

    sid = fst_converter_add_state(converter);
    if (sid < 0) {
        ST_WARNING("Failed to fst_converter_add_state.");
        goto ERR;
    }

    if (fst_converter_print_arc(converter, FST_INIT_STATE, sid,
                SENT_START_ID) < 0) {
        ST_WARNING("Failed to fst_converter_print_arc for <s>.");
        goto ERR;
    }
    converter->fst_state_infos[sid].word_id = SENT_START_ID;
    converter->fst_state_infos[sid].model_state_id = -1;

    while (sid < converter->n_fst_state) {
        for (n = 0; sid < converter->n_fst_state && n < converter->n_thr;
                sid++, n++) {
            targs[n].sid = sid;
            if (pthread_create(pts + n, NULL, converter_worker,
                        (void *)(targs + n)) != 0) {
                ST_WARNING("Failed to pthread_create.");
                goto ERR;
            }
        }

        for (i = 0; i < n; i++) {
            if (pthread_join(pts[i], NULL) != 0) {
                ST_WARNING("Falied to pthread_join.");
                goto ERR;
            }
        }

        if (converter->err != 0) {
            ST_WARNING("Error in worker threads");
            goto ERR;
        }
    }

    safe_free(pts);
    if (targs != NULL) {
        for (i = 0; i < converter->n_thr; i++) {
            safe_free(targs[i].output_probs);
        }
        safe_free(targs);
    }
    return 0;

ERR:

    safe_free(pts);
    if (targs != NULL) {
        for (i = 0; i < converter->n_thr; i++) {
            safe_free(targs[i].output_probs);
        }
        safe_free(targs);
    }
    return -1;
}
