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

int fst_converter_load_opt(fst_converter_opt_t *converter_opt,
        st_opt_t *opt, const char *sec_name)
{
    char method_str[MAX_ST_CONF_LEN];
    double d;

    ST_CHECK_PARAM(converter_opt == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_BOOL(opt, sec_name, "PRINT_SYMS",
            converter_opt->print_syms, false,
            "Print symbols instead of numbers, if true. ");

    ST_OPT_SEC_GET_STR(opt, sec_name, "BACKOFF_METHOD",
            method_str, MAX_ST_CONF_LEN, "Beam",
            "Backoff method(Beam/Sampling)");
    if (strcasecmp(method_str, "beam") == 0) {
        converter_opt->bom = BOM_BEAM;

        ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "BEAM", d, 0.0,
                "Threshold for beam.");
        converter_opt->beam = (real_t)d;
    } else if (strcasecmp(method_str, "sampling") == 0) {
        converter_opt->bom = BOM_SAMPLING;

        ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "BOOST", d, 0.0,
                "Boost probability for sampling.");
        converter_opt->boost = (real_t)d;
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

static int fst_converter_setup(fst_converter_t *converter)
{
    int i;

    ST_CHECK_PARAM(converter == NULL, -1);

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

static int fst_converter_print_arc(FILE *fp, int from, int to, int wid,
        vocab_t *vocab)
{
    if (vocab == NULL) {
        return fprintf(fp, "%d\t%d\t%d\t%d\n", from, to, wid, wid);
    } else {
        return fprintf(fp, "%d\t%d\t%s\t%s\n", from, to,
                vocab_get_word(vocab, wid), vocab_get_word(vocab, wid));
    }
}

static int fst_converter_expand(fst_converter_t *converter, int tid, int sid)
{
    updater_t *updater;

    ST_CHECK_PARAM(converter == NULL || sid < 0, -1);

    updater = converter->updaters[tid];

#if 0
    cache_id = -1; // get a new cache block
    info = (real_t *)st_block_cache_fetch(converter->model_state_cache,
            &cache_id);
    if (info == NULL) {
        ST_WARNING("Failed to st_block_cache_fetch.");
        goto ERR;
    }
    //forward(updater, state,
    if (updater_dump_state(updater, state) < 0) {
        ST_WARNING("Failed to updater_dump_state for <s> state.");
        goto ERR;
    }
    sid = fst_converter_add_state(converter);
    if (sid < 0) {
        ST_WARNING("Failed to fst_converter_add_state.");
        goto ERR;
    }

    if (fst_converter_print_arc(fst_fp, 0, sid, SENT_START_ID,
            converter->converter_opt.print_syms
                ? converter->connlm->vocab : NULL) < 0) {
        ST_WARNING("Failed to fst_converter_print_arc for <s>.");
        goto ERR;
    }
#endif
    return 0;
}

typedef struct fst_converter_thread_args_t_ {
    fst_converter_t *converter;
    int tid;
    int sid;
} converter_targs_t;

static void* converter_worker(void *args)
{
    converter_targs_t *targs;
    fst_converter_t *converter;
    int tid, sid;

    targs = (converter_targs_t *)args;
    converter = targs->converter;
    tid = targs->tid;
    sid = targs->sid;

    if (fst_converter_expand(converter, tid, sid) < 0) {
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
    real_t *state;
    int sid;
    int cache_id;
    int i, n;

    ST_CHECK_PARAM(converter == NULL || fst_fp == NULL, -1);

    if (fst_converter_setup(converter) < 0) {
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
    for (i = 0; i < converter->n_thr; i++) {
        targs[i].converter = converter;
        targs[i].tid = i;
    }

    if (fst_converter_add_state(converter) != 0) {
        ST_WARNING("Failed to fst_converter_add_state init state.");
        goto ERR;
    }
    if (fst_converter_add_state(converter) != 1) {
        ST_WARNING("Failed to fst_converter_add_state final state.");
        goto ERR;
    }

    converter->fst_state_infos[0].word_id = SENT_START_ID;
    converter->fst_state_infos[0].model_state_id = -1;

    if (fst_converter_expand(converter, 0, 0) < 0) {
        ST_WARNING("Failed to fst_converter_expand init state.");
        goto ERR;
    }

    while (sid < converter->n_fst_state) {
        for (n = 0; sid < converter->n_fst_state && n < converter->n_thr;
                sid++, n++) {
            targs[n].sid = sid;
            if (pthread_create(pts + n, NULL, converter_worker,
                        (void *)(targs + n)) != 0) {
                ST_WARNING("Failed to pthread_create.");
                return -1;
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
    safe_free(targs);
    return 0;

ERR:

    safe_free(pts);
    safe_free(targs);
    return -1;
}
