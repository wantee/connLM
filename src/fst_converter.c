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
#include <stutils/st_int.h>
#include <stutils/st_io.h>

#include "fst_converter.h"

#define FST_INIT_STATE 0
#define FST_FINAL_STATE 1

#define get_vocab_size(conv) (conv)->connlm->vocab->vocab_size
#define phi_id(conv) get_vocab_size(conv)

typedef struct _fst_converter_args_t_ {
    fst_conv_t *conv;
    int tid;
    int sid;

    unsigned int rand_seed;
    double *output_probs;
    int *selected_words;
} fst_conv_args_t;

static void fst_conv_args_destroy(fst_conv_args_t *args)
{
    if (args == NULL) {
        return;
    }

    args->tid = -1;
    args->rand_seed = 0;

    safe_free(args->output_probs);
    safe_free(args->selected_words);

    args->conv = NULL;
}

static int fst_conv_args_init(fst_conv_args_t *args,
        fst_conv_t *conv, int tid)
{
    ST_CHECK_PARAM(args == NULL || conv == NULL, -1);

    args->conv = conv;
    args->tid = tid;
    args->rand_seed = conv->conv_opt.init_rand_seed + tid;

    args->output_probs = (double *)malloc(sizeof(double)
            * get_vocab_size(conv));
    if (args->output_probs == NULL) {
        ST_WARNING("Failed to malloc output_probs.");
        goto ERR;
    }

    args->selected_words = (int *)malloc(sizeof(int)
            * get_vocab_size(conv));
    if (args->selected_words == NULL) {
        ST_WARNING("Failed to malloc selected_words.");
        goto ERR;
    }

    return 0;

ERR:
    fst_conv_args_destroy(args);
    return -1;
}

#define safe_fst_conv_args_list_destroy(ptr, n) do {\
    if((ptr) != NULL) {\
        fst_conv_args_list_destroy(ptr, n);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
static void fst_conv_args_list_destroy(fst_conv_args_t *args, int n)
{
    int i;

    if (args == NULL || n <= 0) {
        return;
    }

    for (i = 0; i < n; i++) {
        fst_conv_args_destroy(args + i);
    }
}

static fst_conv_args_t* fst_conv_args_list_create(fst_conv_t *conv)
{
    fst_conv_args_t *args = NULL;

    int i;

    ST_CHECK_PARAM(conv == NULL, NULL);

    args = (fst_conv_args_t *)malloc(sizeof(fst_conv_args_t)
            * conv->n_thr);
    if (args == NULL) {
        ST_WARNING("Failed to malloc fst_conv_args_t");
        goto ERR;
    }
    memset(args, 0, sizeof(fst_conv_args_t) * conv->n_thr);

    for (i = 0; i < conv->n_thr; i++) {
        if (fst_conv_args_init(args + i, conv, i) < 0) {
            ST_WARNING("Failed to fst_conv_args_init[%d].", i);
            goto ERR;
        }
    }

    return args;

ERR:
    safe_fst_conv_args_list_destroy(args, conv->n_thr);
    return NULL;
}

int fst_conv_load_opt(fst_conv_opt_t *conv_opt,
        st_opt_t *opt, const char *sec_name)
{
    char method_str[MAX_ST_CONF_LEN];

    ST_CHECK_PARAM(conv_opt == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_BOOL(opt, sec_name, "PRINT_SYMS",
            conv_opt->print_syms, false,
            "Print symbols instead of numbers, if true. ");

    ST_OPT_SEC_GET_STR(opt, sec_name, "BACKOFF_METHOD",
            method_str, MAX_ST_CONF_LEN, "Beam",
            "Backoff method(Beam/Sampling)");
    if (strcasecmp(method_str, "beam") == 0) {
        conv_opt->bom = BOM_BEAM;

        ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "BEAM",
                conv_opt->beam, 0.0, "Threshold for beam.");
        if (conv_opt->beam < 0.0) {
            ST_WARNING("beam must be larger than or equal to zero.");
            goto ST_OPT_ERR;
        }
    } else if (strcasecmp(method_str, "sampling") == 0) {
        conv_opt->bom = BOM_SAMPLING;

        ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "BOOST",
                conv_opt->boost, 0.0,
                "Boost probability for sampling.");
        if (conv_opt->boost < 0.0 || conv_opt->boost > 1.0) {
            ST_WARNING("boost must be in [0, 1].");
            goto ST_OPT_ERR;
        }

        ST_OPT_SEC_GET_UINT(opt, sec_name, "INIT_RANDOM_SEED",
                conv_opt->init_rand_seed, (unsigned int)time(NULL),
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

void fst_conv_destroy(fst_conv_t *conv)
{
    int i;

    if (conv == NULL) {
        return;
    }

    conv->connlm = NULL;

    for(i = 0; i < conv->n_thr; i++) {
        safe_updater_destroy(conv->updaters[i]);
    }
    safe_free(conv->updaters);
    conv->n_thr = 0;

    (void)pthread_mutex_destroy(&conv->n_fst_state_lock);

    safe_st_block_cache_destroy(conv->model_state_cache);
    (void)pthread_mutex_destroy(&conv->model_state_cache_lock);

    safe_free(conv->fst_states);
    (void)pthread_mutex_destroy(&conv->fst_state_lock);
    conv->cap_fst_states = 0;

    (void)pthread_mutex_destroy(&conv->fst_fp_lock);
    conv->fst_fp = NULL;
}

fst_conv_t* fst_conv_create(connlm_t *connlm, int n_thr,
        fst_conv_opt_t *conv_opt)
{
    fst_conv_t *conv = NULL;

    int i;
    int state_size;
    int count;

    ST_CHECK_PARAM(connlm == NULL || n_thr <= 0 || conv_opt == NULL, NULL);

    if (strcasecmp(vocab_get_word(connlm->vocab, ANY_ID), ANY) != 0) {
        ST_WARNING("vocab must contain <any> in order to convert to fst");
        return NULL;
    }

    conv = (fst_conv_t *)malloc(sizeof(fst_conv_t));
    if (conv == NULL) {
        ST_WARNING("Failed to malloc converter.");
        return NULL;
    }
    memset(conv, 0, sizeof(fst_conv_t));

    conv->connlm = connlm;
    conv->n_thr = n_thr;
    conv->conv_opt = *conv_opt;

    conv->updaters = (updater_t **)malloc(sizeof(updater_t*)*n_thr);
    if (conv->updaters == NULL) {
        ST_WARNING("Failed to malloc updaters.");
        goto ERR;
    }
    memset(conv->updaters, 0, sizeof(updater_t*) * n_thr);

    for (i = 0; i < conv->n_thr; i++) {
        conv->updaters[i] = updater_create(connlm);
        if (conv->updaters[i] == NULL) {
            ST_WARNING("Failed to malloc updater[%d].", i);
            goto ERR;
        }
    }

    if (pthread_mutex_init(&conv->n_fst_state_lock, NULL) != 0) {
        ST_WARNING("Failed to pthread_mutex_init n_fst_state_lock.");
        goto ERR;
    }

    count = (int)sqrt(get_vocab_size(conv)) * get_vocab_size(conv);

    state_size = updater_state_size(conv->updaters[0]);
    conv->model_state_cache = st_block_cache_create(
            sizeof(real_t) * state_size, 5 * count, count);
    if (conv->model_state_cache == NULL) {
        ST_WARNING("Failed to st_block_cache_create model_state_cache.");
        goto ERR;
    }
    if (pthread_mutex_init(&conv->model_state_cache_lock, NULL) != 0) {
        ST_WARNING("Failed to pthread_mutex_init model_state_cache_lock.");
        goto ERR;
    }

    conv->cap_fst_states = count;
    conv->fst_states = (fst_state_t *)malloc(sizeof(fst_state_t) * count);
    if (conv->fst_states == NULL) {
        ST_WARNING("Failed to st_block_cache_create fst_states.");
        goto ERR;
    }
    if (pthread_mutex_init(&conv->fst_state_lock, NULL) != 0) {
        ST_WARNING("Failed to pthread_mutex_init fst_state_lock.");
        goto ERR;
    }

    return conv;

ERR:
    safe_fst_conv_destroy(conv);
    return NULL;
}

static int fst_conv_setup(fst_conv_t *conv, FILE *fst_fp,
        fst_conv_args_t **args)
{
    int i;

    ST_CHECK_PARAM(conv == NULL || fst_fp == NULL || args == NULL, -1);

    conv->fst_fp = fst_fp;

    if (pthread_mutex_init(&conv->fst_fp_lock, NULL) != 0) {
        ST_WARNING("Failed to pthread_mutex_init fst_fp_lock.");
        return -1;
    }

    if (connlm_setup(conv->connlm) < 0) {
        ST_WARNING("Failed to connlm_setup.");
        return -1;
    }

    for (i = 0; i < conv->n_thr; i++) {
        if (updater_setup(conv->updaters[i], false) < 0) {
            ST_WARNING("Failed to updater_setup.");
            return -1;
        }
    }

    *args = fst_conv_args_list_create(conv);
    if (*args == NULL) {
        ST_WARNING("Failed to fst_conv_args_list_create.");
        return -1;
    }

    return 0;
}

static int fst_conv_maybe_realloc_infos(fst_conv_t *conv)
{
    int needed;
    int ret;

    ST_CHECK_PARAM(conv == NULL, -1);

    if (pthread_mutex_lock(&conv->fst_state_lock) != 0) {
        ST_WARNING("Failed to pthread_mutex_lock fst_state_lock.");
        return -1;
    }
    if (conv->n_fst_state < conv->cap_fst_states) {
        ret = 0;
        goto RET;
    }

    needed = max(conv->cap_fst_states + get_vocab_size(conv),
            conv->n_fst_state);

    conv->fst_states = realloc(conv->fst_states,
            sizeof(fst_state_t) * needed);
    if (conv->fst_states == NULL) {
        ST_WARNING("Failed to realloc fst_states.");
        ret = -1;
        goto RET;
    }

    ret = 0;
RET:
    if (pthread_mutex_unlock(&conv->fst_state_lock) != 0) {
        ST_WARNING("Failed to pthread_mutex_lock fst_state_lock.");
        return -1;
    }

    return ret;
}

static int fst_conv_add_state(fst_conv_t *conv,
        int word_id, int model_state_id)
{
    int sid;

    ST_CHECK_PARAM(conv == NULL, -1);

    if (pthread_mutex_lock(&conv->n_fst_state_lock) != 0) {
        ST_WARNING("Failed to pthread_mutex_lock n_fst_state_lock.");
        return -1;
    }

    sid = conv->n_fst_state++;

    if (fst_conv_maybe_realloc_infos(conv) < 0) {
        ST_WARNING("Failed to fst_conv_maybe_realloc_infos.");
        return -1;
    }
    if (pthread_mutex_unlock(&conv->n_fst_state_lock) != 0) {
        ST_WARNING("Failed to pthread_mutex_unlock n_fst_state_lock.");
        return -1;
    }

    conv->fst_states[sid].word_id = word_id;
    conv->fst_states[sid].model_state_id = model_state_id;

    return sid;
}

static int fst_conv_print_arc(fst_conv_t *conv,
        int from, int to, int wid, double weight)
{
    char *ilab_str;
    char *olab_str;
    int ilab;
    int olab;
    int ret;

    if (pthread_mutex_lock(&conv->fst_fp_lock) != 0) {
        ST_WARNING("Failed to pthread_mutex_lock fst_fp_lock.");
        return -1;
    }
    if (conv->conv_opt.print_syms) {
        if (phi_id(conv) == wid) {
            ilab_str = PHI;
            olab_str = EPS;
        } else {
            ilab_str = vocab_get_word(conv->connlm->vocab, wid);
            olab_str = ilab_str;
        }
        ret = fprintf(conv->fst_fp, "%d\t%d\t%s\t%s\t%f\n", from, to,
                ilab_str, olab_str, (float)weight);
    } else {
        if (phi_id(conv) == wid) {
            ilab = wid + 1; // reserver 0 for <eps>
            olab = 0;
        } else {
            ilab = wid + 1; // reserver 0 for <eps>
            olab = ilab;
        }
        ret = fprintf(conv->fst_fp, "%d\t%d\t%d\t%d\t%f\n", from, to,
                ilab, olab, (float)weight);
    }
    if (pthread_mutex_unlock(&conv->fst_fp_lock) != 0) {
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

        if (word == SENT_START_ID) {
            continue;
        }

        break;
    }

    return word;
}

static int select_word(fst_conv_t *conv, int last_word,
        double *output_probs, unsigned int *rand_seed)
{
    int word;

    ST_CHECK_PARAM(conv == NULL || last_word < -1, -1);

    switch (conv->conv_opt.bom) {
        case BOM_BEAM:
            word = last_word + 1;
            while (word < get_vocab_size(conv)) {
                if (word == SENT_END_ID || word == SENT_START_ID) {
                    continue;
                }
                if (output_probs[word] >= output_probs[SENT_END_ID]
                        - conv->conv_opt.beam) {
                    return word;
                }
                ++word;
            }

            return SENT_END_ID;
            break;
        case BOM_SAMPLING:
            return boost_sampling(output_probs, get_vocab_size(conv),
                    conv->conv_opt.boost, rand_seed);
            break;
        default:
            ST_WARNING("Unkown backoff method");
            return -1;
    }

    return -1;
}

static int sort_selected_words(backoff_method_t bom,
        int *selected_words, int n)
{
    ST_CHECK_PARAM(selected_words == NULL, -1);

    if (selected_words[n - 1] == SENT_END_ID) {
        ST_WARNING("The last word in selected_words should be " SENT_END);
        return -1;
    }

    switch (bom) {
        case BOM_BEAM:
            /* only need to reorder the </s> */
            n--;
            if (st_int_insert(selected_words, n + 1, &n, SENT_END_ID) < 0) {
                ST_WARNING("Failed to st_int_insert.");
                return -1;
            }
            break;
        case BOM_SAMPLING:
            st_int_sort(selected_words, n);
            break;
        default:
            ST_WARNING("Unkown backoff method");
            return -1;
    }

    return 0;
}

static int fst_conv_find_backoff(fst_conv_t *conv, int sid)
{
    ST_CHECK_PARAM(conv == NULL || sid < 0, -1);

    return 0;
}

static int fst_conv_expand(fst_conv_t *conv, fst_conv_args_t *args)
{
    updater_t *updater;
    real_t *state;
    real_t *new_state;
    double *output_probs;

    double backoff_prob;
    int sid;
    int to_sid;
    int model_state_id;
    int word;

    int i, n;
    bool no_backoff;
    bool expand_wildchar;
    bool have_any;

    ST_CHECK_PARAM(conv == NULL || args == NULL, -1);

    updater = conv->updaters[args->tid];
    sid = args->sid;
    output_probs = args->output_probs;

    if (conv->fst_states[sid].model_state_id < 0) {
        state = NULL;
    } else {
        state = (real_t *)st_block_cache_fetch(conv->model_state_cache,
                &(conv->fst_states[sid].model_state_id));
        if (state == NULL) {
            ST_WARNING("Failed to st_block_cache_fetch state.");
            return -1;
        }
    }

    //forward(updater, state, conv->fst_states[sid].word_id, -1)

    model_state_id = -1;
    new_state = (real_t *)st_block_cache_fetch(
            conv->model_state_cache, &model_state_id);
    if (new_state == NULL) {
        ST_WARNING("Failed to st_block_cache_fetch new state.");
        return -1;
    }

    if (updater_dump_state(updater, new_state) < 0) {
        ST_WARNING("Failed to updater_dump_state.");
        return -1;
    }

    // conv->output_probs =
    // logprob2prob(output_probs);
    // distribute_prob(output_probs, SENT_START_ID);

    no_backoff = false;
    expand_wildchar = false;
    if (conv->fst_states[sid].word_id == ANY_ID) {
        if(conv->fst_states[sid].parent == -1) { // no backoff
            no_backoff = true;
        }
        expand_wildchar = true;
    }

    if (no_backoff) {
        n = 0;
        for (word = 0; word < get_vocab_size(conv); word++) {
            if (word == SENT_START_ID) {
                continue;
            }
            args->selected_words[n] = word;
            n++;
        }
        have_any = true;
    } else {
        have_any = false;
        n = 0;
        word = -1;
        while(true) {
            word = select_word(conv, word, output_probs,
                    &args->rand_seed);
            if (word < 0) {
                ST_WARNING("Failed to select_word.");
                return -1;
            }
            if (word == ANY_ID) {
                have_any = true;
            }

            args->selected_words[n] = word;
            n++;
        }

        if (sort_selected_words(conv->conv_opt.bom,
                    args->selected_words, n) < 0) {
            ST_WARNING("Failed to sort_selected_words.");
            return -1;
        }
    }

    backoff_prob = 1.0;
    if (expand_wildchar) {
        if (!have_any) {
            if (fst_conv_print_arc(conv, sid,
                        FST_FINAL_STATE, SENT_END_ID,
                        output_probs[SENT_END_ID]) < 0) {
                ST_WARNING("Failed to fst_conv_print_arc.");
                return -1;
            }
            backoff_prob -= output_probs[SENT_END_ID];
            n = 0;
        } else {
            // expand <any> first, since other state may backoff to them.
            word = ANY_ID;
            to_sid = fst_conv_add_state(conv, word, model_state_id);
            if (to_sid < 0) {
                ST_WARNING("Failed to fst_conv_add_state.");
                return -1;
            }

            if (fst_conv_print_arc(conv, sid,
                        to_sid, word, output_probs[word]) < 0) {
                ST_WARNING("Failed to fst_conv_print_arc.");
                return -1;
            }
            backoff_prob -= output_probs[ANY_ID];
        }
    } else {
        // distribute_prob(output_probs, ANY_ID);
    }

    for (i = 0; i < n; i++) {
        word = args->selected_words[i];
        backoff_prob -= output_probs[word];

        if (word == ANY_ID) {
            continue;
        }

        if (word == SENT_END_ID) {
            to_sid = FST_FINAL_STATE;
        } else {
            to_sid = fst_conv_add_state(conv, word,
                    model_state_id);
            if (to_sid < 0) {
                ST_WARNING("Failed to fst_conv_add_state.");
                return -1;
            }
        }

        if (fst_conv_print_arc(conv, sid,
                    to_sid, word, output_probs[word]) < 0) {
            ST_WARNING("Failed to fst_conv_print_arc.");
            return -1;
        }
    }

    if (!no_backoff) {
        //backoff
        to_sid = fst_conv_find_backoff(conv, sid);
        if (to_sid < 0) {
            ST_WARNING("Failed to fst_conv_find_backoff.");
            return -1;
        }

        if (fst_conv_print_arc(conv, sid, to_sid,
                    phi_id(conv), backoff_prob) < 0) {
            ST_WARNING("Failed to fst_conv_print_arc.");
            return -1;
        }
    }

    return 0;
}

static void* conv_worker(void *args)
{
    fst_conv_args_t *cargs;
    fst_conv_t *conv;

    cargs = (fst_conv_args_t *)args;
    conv = cargs->conv;

    if (fst_conv_expand(conv, cargs) < 0) {
        ST_WARNING("Failed to fst_conv_expand.");
        goto ERR;
    }

    return NULL;

ERR:
    conv->err = -1;

    return NULL;
}

static int fst_conv_reset(fst_conv_t *conv, fst_conv_args_t *args)
{
    ST_CHECK_PARAM(conv == NULL, -1);

    return 0;
}

static int fst_conv_build_wildcard(fst_conv_t *conv, fst_conv_args_t *args)
{
    pthread_t *pts = NULL;
    int sid;
    int i, n;

    ST_CHECK_PARAM(conv == NULL || args == NULL, -1);

    pts = (pthread_t *)malloc(sizeof(pthread_t) * conv->n_thr);
    if (pts == NULL) {
        ST_WARNING("Failed to malloc pts");
        goto ERR;
    }

    sid = fst_conv_add_state(conv, ANY_ID, -1);
    if (sid < 0) {
        ST_WARNING("Failed to fst_conv_add_state.");
        goto ERR;
    }

    while (sid < conv->n_fst_state) {
        for (n = 0; sid < conv->n_fst_state && n < conv->n_thr;
                sid++, n++) {
            args[n].sid = sid;
            if (pthread_create(pts + n, NULL, conv_worker,
                        (void *)(args + n)) != 0) {
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

        if (conv->err != 0) {
            ST_WARNING("Error in worker threads");
            goto ERR;
        }
    }

    safe_free(pts);
    return 0;

ERR:

    safe_free(pts);
    return -1;
}

static int fst_conv_build_normal(fst_conv_t *conv, fst_conv_args_t *args)
{
    pthread_t *pts = NULL;
    int sid;
    int i, n;

    ST_CHECK_PARAM(conv == NULL || args == NULL, -1);

    pts = (pthread_t *)malloc(sizeof(pthread_t) * conv->n_thr);
    if (pts == NULL) {
        ST_WARNING("Failed to malloc pts");
        goto ERR;
    }

    sid = fst_conv_add_state(conv, SENT_START_ID, -1);
    if (sid < 0) {
        ST_WARNING("Failed to fst_conv_add_state.");
        goto ERR;
    }

    if (fst_conv_print_arc(conv, FST_INIT_STATE, sid,
                SENT_START_ID, 0.0) < 0) {
        ST_WARNING("Failed to fst_conv_print_arc for <s>.");
        goto ERR;
    }

    while (sid < conv->n_fst_state) {
        for (n = 0; sid < conv->n_fst_state && n < conv->n_thr;
                sid++, n++) {
            args[n].sid = sid;
            if (pthread_create(pts + n, NULL, conv_worker,
                        (void *)(args + n)) != 0) {
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

        if (conv->err != 0) {
            ST_WARNING("Error in worker threads");
            goto ERR;
        }
    }

    safe_free(pts);
    return 0;

ERR:

    safe_free(pts);
    return -1;
}

int fst_conv_convert(fst_conv_t *conv, FILE *fst_fp)
{
    fst_conv_args_t *args = NULL;

    ST_CHECK_PARAM(conv == NULL || fst_fp == NULL, -1);

    if (fst_conv_setup(conv, fst_fp, &args) < 0) {
        ST_WARNING("Failed to fst_conv_setup.");
        goto ERR;
    }

    if (fst_conv_add_state(conv, -1, -1) != FST_INIT_STATE) {
        ST_WARNING("Failed to fst_conv_add_state init state.");
        goto ERR;
    }
    if (fst_conv_add_state(conv, -1, -1) != FST_FINAL_STATE) {
        ST_WARNING("Failed to fst_conv_add_state final state.");
        goto ERR;
    }

    if (fst_conv_build_wildcard(conv, args) < 0) {
        ST_WARNING("Failed to fst_conv_build_wildcard.");
        goto ERR;
    }

    if (fst_conv_reset(conv, args) < 0) {
        ST_WARNING("Failed to fst_conv_reset.");
        goto ERR;
    }

    if (fst_conv_build_normal(conv, args) < 0) {
        ST_WARNING("Failed to fst_conv_build_normal.");
        goto ERR;
    }

    safe_fst_conv_args_list_destroy(args, conv->n_thr);
    return 0;

ERR:
    safe_fst_conv_args_list_destroy(args, conv->n_thr);
    return -1;
}
