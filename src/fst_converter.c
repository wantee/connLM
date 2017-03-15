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
#include <assert.h>
#include <limits.h>

#include <stutils/st_macro.h>
#include <stutils/st_log.h>
#include <stutils/st_rand.h>
#include <stutils/st_int.h>
#include <stutils/st_io.h>

#include "fst_converter.h"

#define FST_CONV_LOG_STEP 10000

#define FST_INIT_STATE 0
#define FST_FINAL_STATE 1
#define FST_SENT_START_STATE 2
#define FST_BACKOFF_STATE 3

#define get_output_size(conv) (conv)->connlm->output->output_size
#define get_vocab_size(conv) (conv)->connlm->vocab->vocab_size
#define phi_id(conv) get_vocab_size(conv)
#define bos_id(conv) vocab_get_id((conv)->connlm->vocab, SENT_START)

typedef struct _fst_converter_args_t_ {
    fst_conv_t *conv;
    int tid;
    int sid;

    unsigned int rand_seed;
    double *output_probs;
    int *selected_words;

    bool store_children;
    double ws_arg;
    double ws_arg_power;

    int *word_hist;
    int cap_word_hist;
    int num_word_hist;

    int max_gram;
    int num_arcs;
    int *num_grams;
    int *start_sids; /* start sid for every gram. */
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

    safe_free(args->word_hist);
    args->num_word_hist = 0;
    args->cap_word_hist = 0;

    safe_free(args->num_grams);
    safe_free(args->start_sids);

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
            * get_output_size(conv));
    if (args->output_probs == NULL) {
        ST_WARNING("Failed to malloc output_probs.");
        goto ERR;
    }

    args->selected_words = (int *)malloc(sizeof(int) * get_output_size(conv));
    if (args->selected_words == NULL) {
        ST_WARNING("Failed to malloc selected_words.");
        goto ERR;
    }

    args->cap_word_hist = 32;
    args->word_hist = (int *)malloc(sizeof(int) * args->cap_word_hist);
    if (args->word_hist == NULL) {
        ST_WARNING("Failed to malloc word_hist");
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

static int get_total_arcs(fst_conv_args_t *args, int n)
{
    int i;
    int num_arcs = 0;

    for (i = 0; i < n; i++) {
        num_arcs += args[i].num_arcs;
    }

    return num_arcs;
}

static int get_max_gram(fst_conv_args_t *args, int n)
{
    int i;

    int max_gram = 0;

    for (i = 0; i < n; i++) {
        if (args[i].max_gram > max_gram) {
            max_gram = args[i].max_gram;
        }
    }

    return max_gram;
}

static int get_start_sid(fst_conv_args_t *args, int n, int order)
{
    int start_sid = INT_MAX;

    int i;

    for (i = 0; i < n; i++) {
        if (order > args[i].max_gram) {
            continue;
        }
        if (args[i].start_sids[order - 1] < start_sid) {
            start_sid = args[i].start_sids[order - 1];
        }
    }

    return start_sid;
}

int fst_conv_load_opt(fst_conv_opt_t *conv_opt,
        st_opt_t *opt, const char *sec_name)
{
    char method_str[MAX_ST_CONF_LEN];
    double def_ws_arg = 0.0;
    double def_ws_arg_power = 0.0;

    ST_CHECK_PARAM(conv_opt == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_BOOL(opt, sec_name, "PRINT_SYMS",
            conv_opt->print_syms, false,
            "Print symbols instead of ids in FST, if true.");

    ST_OPT_SEC_GET_BOOL(opt, sec_name, "OUTPUT_UNK",
            conv_opt->output_unk, false,
            "output <unk> in FST, if true.");

    ST_OPT_SEC_GET_STR(opt, sec_name, "WORD_SYMS_FILE",
            conv_opt->word_syms_file, MAX_DIR_LEN, "",
            "File to be written out word symbols.");

    ST_OPT_SEC_GET_STR(opt, sec_name, "STATE_SYMS_FILE",
            conv_opt->state_syms_file, MAX_DIR_LEN, "",
            "File to be written out state symbols(for debug).");

    ST_OPT_SEC_GET_STR(opt, sec_name, "WORD_SELECTION_METHOD",
            method_str, MAX_ST_CONF_LEN, "Majority",
            "Word selection method(Baseline/Sampling/Majority)");
    if (strcasecmp(method_str, "baseline") == 0) {
        conv_opt->wsm = WSM_BASELINE;
    } else if (strcasecmp(method_str, "sampling") == 0) {
        conv_opt->wsm = WSM_SAMPLING;
    } else if (strcasecmp(method_str, "Majority") == 0) {
        conv_opt->wsm = WSM_MAJORITY;
    } else {
        ST_WARNING("Unknown word selection method[%s].", method_str);
        goto ST_OPT_ERR;
    }

    switch (conv_opt->wsm) {
        case WSM_SAMPLING:
            /* FALL THROUGH */
        case WSM_BASELINE:
            def_ws_arg = 0.0;
            def_ws_arg_power = 0.0;
            break;

        case WSM_MAJORITY:
            def_ws_arg = 0.5;
            def_ws_arg_power = 0.0;
            break;

        default:
            break;
    }

    ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "WS_ARG",
            conv_opt->ws_arg, def_ws_arg,
            "Arg for word selection, would be interpreted as"
            "boost for </s> if WORD_SELECTION_METHOD is Baseline or Sampling, "
            "otherwise, as threshold for accumulated probabilty.");

    ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "WS_ARG_POWER",
            conv_opt->ws_arg_power, def_ws_arg_power,
            "power to the history length for ws_arg. "
            "e.g. cur_ws_arg = ws_arg * pow(hist_len, ws_arg_power).");

    ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "WILDCARD_WS_ARG",
            conv_opt->wildcard_ws_arg, conv_opt->ws_arg,
            "WS_ARG for wildcard subFST. would be same ws_arg, "
            "if not specified");

    ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "WILDCARD_WS_ARG_POWER",
            conv_opt->wildcard_ws_arg_power, conv_opt->ws_arg_power,
            "WS_ARG_POWER for wildcard subFST. would be the same ws_arg_power, "
            "if not specified");

    if (conv_opt->wsm) {
        ST_OPT_SEC_GET_UINT(opt, sec_name, "INIT_RANDOM_SEED",
                conv_opt->init_rand_seed, (unsigned int)time(NULL),
                "Initial random seed, seed for each thread would be "
                "incremented from this value. "
                "Default is value of time(NULl).");
    }

    ST_OPT_SEC_GET_INT(opt, sec_name, "MAX_GRAM", conv_opt->max_gram, 0,
            "Maximum gram to be expaned(no limits if less than or equal to 0).");
    if (conv_opt->max_gram > 0 && conv_opt->max_gram <= 2) {
        ST_WARNING("MAX_GRAM must be larger than 2.");
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

    safe_st_block_cache_destroy(conv->model_state_cache);

    safe_free(conv->fst_states);
    (void)pthread_mutex_destroy(&conv->fst_state_lock);
    conv->cap_fst_states = 0;
    conv->n_fst_state = 0;

    safe_free(conv->fst_children);
    safe_free(conv->in_probs);
    safe_free(conv->final_probs);
    safe_free(conv->backoff_states);
    safe_free(conv->bows);
    conv->n_fst_children = 0;

    (void)pthread_mutex_destroy(&conv->fst_fp_lock);
    conv->fst_fp = NULL;

    if (conv->ssyms_fp != NULL) {
        safe_fclose(conv->ssyms_fp);
        (void)pthread_mutex_destroy(&conv->ssyms_fp_lock);
    }
}

fst_conv_t* fst_conv_create(connlm_t *connlm, int n_thr,
        fst_conv_opt_t *conv_opt)
{
    fst_conv_t *conv = NULL;

    int i;

    ST_CHECK_PARAM(connlm == NULL || n_thr <= 0 || conv_opt == NULL, NULL);

    if (connlm_generate_wildcard_repr(connlm, connlm->vocab->cnts) < 0) {
        ST_WARNING("Could NOT convert to FST: "
                "failed to generate representation for wildcard.");
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

    if (conv->updaters[0]->input_updater->ctx_rightmost > 0) {
        ST_WARNING("Can not converting: future words in input context.");
        return NULL;
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
    int state_size;
    int count;

    ST_CHECK_PARAM(conv == NULL || fst_fp == NULL || args == NULL, -1);

    conv->fst_fp = fst_fp;

    if (pthread_mutex_init(&conv->fst_fp_lock, NULL) != 0) {
        ST_WARNING("Failed to pthread_mutex_init fst_fp_lock.");
        return -1;
    }

    if (conv->conv_opt.state_syms_file[0] != '\0') {
        conv->ssyms_fp = st_fopen(conv->conv_opt.state_syms_file, "w");
        if (conv->ssyms_fp == NULL) {
            ST_WARNING("Failed to st_fopen state syms file[%s]",
                    conv->conv_opt.state_syms_file);
            return -1;
        }

        if (pthread_mutex_init(&conv->ssyms_fp_lock, NULL) != 0) {
            ST_WARNING("Failed to pthread_mutex_init ssyms_fp_lock.");
            return -1;
        }
    }

    if (connlm_setup(conv->connlm) < 0) {
        ST_WARNING("Failed to connlm_setup.");
        return -1;
    }

    for (i = 0; i < conv->n_thr; i++) {
        if (updater_setup_all(conv->updaters[i]) < 0) {
            ST_WARNING("Failed to updater_setup_all.");
            return -1;
        }
    }

    *args = fst_conv_args_list_create(conv);
    if (*args == NULL) {
        ST_WARNING("Failed to fst_conv_args_list_create.");
        return -1;
    }

    count = (int)sqrt(get_output_size(conv)) * get_output_size(conv);

    state_size = updater_state_size(conv->updaters[0]);
    if (state_size > 0) {
        conv->model_state_cache = st_block_cache_create(
                sizeof(real_t) * state_size, get_output_size(conv),
                get_output_size(conv));
        if (conv->model_state_cache == NULL) {
            ST_WARNING("Failed to st_block_cache_create model_state_cache.");
            return -1;
        }
    } else {
        ST_WARNING("Converting with stateless model could "
                "probably not stop.");
    }

    conv->cap_fst_states = count;
    conv->fst_states = (fst_state_t *)malloc(sizeof(fst_state_t) * count);
    if (conv->fst_states == NULL) {
        ST_WARNING("Failed to st_block_cache_create fst_states.");
        return -1;
    }
    if (pthread_mutex_init(&conv->fst_state_lock, NULL) != 0) {
        ST_WARNING("Failed to pthread_mutex_init fst_state_lock.");
        return -1;
    }

    return 0;
}

static int fst_conv_realloc_states(fst_conv_t *conv, int num_extra,
        bool store_children)
{
    int i;
    int num_new_states;

    ST_CHECK_PARAM(conv == NULL, -1);

    num_new_states = conv->cap_fst_states + num_extra;

    conv->fst_states = realloc(conv->fst_states,
            sizeof(fst_state_t) * num_new_states);
    if (conv->fst_states == NULL) {
        ST_WARNING("Failed to realloc fst_states.");
        return -1;
    }

    if (store_children) {
        conv->fst_children = realloc(conv->fst_children,
                sizeof(fst_state_children_t) * num_new_states);
        if (conv->fst_children == NULL) {
            ST_WARNING("Failed to realloc fst_children.");
            return -1;
        }
        for (i = conv->cap_fst_states; i < num_new_states; i++) {
            conv->fst_children[i].first_child = -1;
            conv->fst_children[i].num_children = -1;
        }
        conv->in_probs = realloc(conv->in_probs,
                sizeof(real_t) * num_new_states);
        if (conv->in_probs == NULL) {
            ST_WARNING("Failed to realloc in_probs.");
            return -1;
        }
        conv->final_probs = realloc(conv->final_probs,
                sizeof(real_t) * num_new_states);
        if (conv->final_probs == NULL) {
            ST_WARNING("Failed to realloc final_probs.");
            return -1;
        }
        conv->backoff_states = realloc(conv->backoff_states,
                sizeof(int) * num_new_states);
        if (conv->backoff_states == NULL) {
            ST_WARNING("Failed to realloc backoff_states.");
            return -1;
        }
        conv->bows = realloc(conv->bows, sizeof(real_t) * num_new_states);
        if (conv->bows == NULL) {
            ST_WARNING("Failed to realloc bows.");
            return -1;
        }
        for (i = conv->cap_fst_states; i < num_new_states; i++) {
            conv->in_probs[i] = 0.0;
            conv->final_probs[i] = 0.0;
            conv->bows[i] = 0.0;
            conv->backoff_states[i] = -1;
        }
        conv->n_fst_children = num_new_states;
    }

    conv->cap_fst_states = num_new_states;

    return 0;
}

static int fst_conv_add_states(fst_conv_t *conv, int n,
        int parent, bool store_children)
{
    int i;
    int sid;

    int ret = 0;

    ST_CHECK_PARAM(conv == NULL || n < 0, -1);

    if (pthread_mutex_lock(&conv->fst_state_lock) != 0) {
        ST_WARNING("Failed to pthread_mutex_lock fst_state_lock.");
        return -1;
    }

    if (conv->n_fst_state + n > conv->cap_fst_states) {
        ST_WARNING("fst_states overflow");
        ret = -1;
        goto RET;
    }

    sid = conv->n_fst_state;
    conv->n_fst_state += n;

    if (pthread_mutex_unlock(&conv->fst_state_lock) != 0) {
        ST_WARNING("Failed to pthread_mutex_unlock fst_state_lock.");
        return -1;
    }

    for (i = sid; i < sid + n; i++) {
        conv->fst_states[i].word_id = -1;
        conv->fst_states[i].parent = parent;
        conv->fst_states[i].model_state_id = -1;
    }

    if (store_children && parent > 0) {
        if (conv->fst_children[parent].first_child != -1) {
            ST_WARNING("duplicated parent[%d]", parent);
            return -1;
        }
        conv->fst_children[parent].first_child = sid;
        conv->fst_children[parent].num_children = n;
    }

    return sid;

RET:
    if (pthread_mutex_unlock(&conv->fst_state_lock) != 0) {
        ST_WARNING("Failed to pthread_mutex_unlock fst_state_lock.");
        return -1;
    }

    return ret;
}

static char* fst_conv_get_word(fst_conv_t *conv, int wid)
{
    if (wid == ANY_ID) {
        return ANY;
    } else if (wid >= 0) {
        return vocab_get_word(conv->connlm->vocab, wid);
    } else {
        return "";
    }
}

static int fst_conv_print_ssyms(fst_conv_t *conv, int sid,
        int *word_hist, int num_word_hist, int wid)
{
    int i;

    ST_CHECK_PARAM(conv == NULL, -1);

    if (conv->ssyms_fp == NULL) {
        return 0;
    }

    if (pthread_mutex_lock(&conv->ssyms_fp_lock) != 0) {
        ST_WARNING("Failed to pthread_mutex_lock ssyms_fp_lock.");
        return -1;
    }
    if (fprintf(conv->ssyms_fp, "%d\t", sid) < 0) {
        ST_WARNING("Failed to write ssyms.(disk full?)");
        goto UNLOCK_AND_ERR;
    }
    if (word_hist != NULL) {
        for (i = 0; i < num_word_hist; i++) {
            if (fprintf(conv->ssyms_fp, "%s:",
                    fst_conv_get_word(conv, word_hist[i])) < 0) {
                ST_WARNING("Failed to write ssyms.(disk full?)");
                goto UNLOCK_AND_ERR;
            }
        }
    }
    if (fprintf(conv->ssyms_fp, "%s\n", fst_conv_get_word(conv, wid)) < 0) {
        ST_WARNING("Failed to write ssyms.(disk full?)");
        goto UNLOCK_AND_ERR;
    }

    if (pthread_mutex_unlock(&conv->ssyms_fp_lock) != 0) {
        ST_WARNING("Failed to pthread_mutex_unlock ssyms_fp_lock.");
        return -1;
    }

    return 0;

UNLOCK_AND_ERR:
    if (pthread_mutex_unlock(&conv->ssyms_fp_lock) != 0) {
        ST_WARNING("Failed to pthread_mutex_unlock ssyms_fp_lock.");
        return -1;
    }

    return -1;
}

static int fst_conv_print_arc(fst_conv_t *conv,
        int from, int to, int wid, double weight)
{
    char *ilab_str;
    char *olab_str;
    int ilab;
    int olab;

    ST_CHECK_PARAM(conv == NULL || from < 0 || to < 0 || wid < 0, -1);

    if (to == FST_FINAL_STATE) {
        if (from < conv->n_fst_children) {
            if (conv->final_probs[from] != 0.0) {
                ST_WARNING("state[%d] has multiple final probs.", from);
                return -1;
            }
            conv->final_probs[from] = (real_t)weight;
        }
    } else if (wid == phi_id(conv)) {
        if (from < conv->n_fst_children) {
            if (conv->bows[from] != 0.0) {
                ST_WARNING("state[%d] has multiple bows.", from);
                return -1;
            }
            conv->bows[from] = (real_t)weight;
        }
    } else if (to < conv->n_fst_children) {
        if (conv->in_probs[to] != 0.0) {
            ST_WARNING("state[%d] has multiple in_probs.", to);
            return -1;
        }
        conv->in_probs[to] = (real_t)weight;
    }

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
        if (fprintf(conv->fst_fp, "%d\t%d\t%s\t%s\t%f\n", from, to,
                ilab_str, olab_str, -(float)log(weight)) < 0) {
            ST_WARNING("Failed to write out fst.(disk full?)");
            goto UNLOCK_AND_ERR;
        }
    } else {
        ilab = wid + 1; // reserve <eps>
        if (phi_id(conv) == wid) {
            olab = 0;
        } else {
            olab = ilab;
        }

        if (fprintf(conv->fst_fp, "%d\t%d\t%d\t%d\t%f\n", from, to,
                ilab, olab, -(float)log(weight)) < 0) {
            ST_WARNING("Failed to write out fst.(disk full?)");
            goto UNLOCK_AND_ERR;
        }
    }

    if (pthread_mutex_unlock(&conv->fst_fp_lock) != 0) {
        ST_WARNING("Failed to pthread_mutex_lock fst_fp_lock.");
        return -1;
    }

    return 0;

UNLOCK_AND_ERR:
    if (pthread_mutex_unlock(&conv->fst_fp_lock) != 0) {
        ST_WARNING("Failed to pthread_mutex_lock fst_fp_lock.");
        return -1;
    }

    return -1;

}

static int select_words_baseline(fst_conv_t *conv, double *output_probs,
        int *selected_words, int *num_selected,
        double boost, unsigned int *rand_seed)
{
    int word;
    int n;

    ST_CHECK_PARAM(conv == NULL || output_probs == NULL
            || selected_words == NULL || num_selected == NULL, -1);

    n = 0;
    for (word = 0; word < get_output_size(conv); word++) {
        if (word == SENT_END_ID
                || output_probs[word] >= output_probs[SENT_END_ID] + boost) {
            selected_words[n++] = word;
        }
    }
    *num_selected = n;

    return 0;
}

// probs contains the probability for all words,
// a negtive prob represents the word has been selected in the past.
static int boost_sampling(double *probs, int n_probs,
        double boost_value, unsigned int *rand_seed)
{
    int word;

    double u, p;
    double word_prob;

    while (true) {
        u = st_random_r(0, 1, rand_seed);

        p = 0;
        for (word = 0; word < n_probs; word++) {
            if (word == SENT_END_ID) {
                p += probs[word] + boost_value;
            } else {
                word_prob = probs[word];
                if (word_prob < 0.0) {
                    word_prob = -word_prob;
                }
                if (word_prob - boost_value / (n_probs - 1) > 0.0) {
                    p += word_prob - boost_value / (n_probs - 1);
                }
            }

            if (p >= u) {
                break;
            }
        }

        if (word >= n_probs) {
            // no word was selected
            ST_WARNING("Something went wrong. total probs of all words "
                       "is too small(%f<%f)", p, u);
            return -1;
        }

        // skip the duplicated words
        if (probs[word] < 0) {
            continue;
        }

        break;
    }

    return word;
}

static int select_words_sampling(fst_conv_t *conv, double *output_probs,
        int *selected_words, int *num_selected,
        double boost, unsigned int *rand_seed)
{
    int word;
    int n;

    int i;

    ST_CHECK_PARAM(conv == NULL || output_probs == NULL
            || selected_words == NULL || num_selected == NULL, -1);

    n = 0;
    word = -1;
    while(true) {
        word = boost_sampling(output_probs, get_output_size(conv),
                boost, rand_seed);
        if (word < 0) {
            ST_WARNING("Failed to boost_sampling.");
            return -1;
        }

        assert(n < get_output_size(conv));

        selected_words[n] = word;
        n++;

        // avoid selecting the same word next time
        output_probs[word] = -output_probs[word];

        if (word == SENT_END_ID) {
            break;
        }
    }
    *num_selected = n;

    // recover probs for selected words
    for (i = 0; i < n; i++) {
        word = selected_words[i];
        output_probs[word] = -output_probs[word];
    }

    st_int_sort(selected_words, n);

    return 0;
}

static int prob_cmp(const void *elem1, const void *elem2, void *args)
{
    double prob1, prob2;
    double *output_probs;

    output_probs = (double *)args;

    prob1 = output_probs[*((int *)elem1)];
    prob2 = output_probs[*((int *)elem2)];

    if (prob1 < prob2) {
        return 1;
    } else if (prob1 > prob2) {
        return -1;
    }

    return 0;
}

static int select_words_majority(fst_conv_t *conv, double *output_probs,
        int *selected_words, int *num_selected,
        double threshold, unsigned int *rand_seed)
{
    int n;

    double acc;
    int i;
    bool has_eos;

    ST_CHECK_PARAM(conv == NULL || output_probs == NULL
            || selected_words == NULL || num_selected == NULL, -1);

    for (i = 0; i < get_output_size(conv); i++) {
        selected_words[i] = i;
    }

    st_qsort(selected_words, get_output_size(conv), sizeof(int),
            prob_cmp, (void *)output_probs);

    acc = 0.0;
    n = 0;
    has_eos = false;
    for (i = 0; i < get_output_size(conv); i++) {
        acc += output_probs[selected_words[i]];
        if (selected_words[i] == SENT_END_ID) {
            has_eos = true;
        }
        n++;
        if (acc >= threshold) {
            break;
        }
    }

    if (! has_eos) {
        selected_words[n] = SENT_END_ID;
        n++;
    }

    st_int_sort(selected_words, n);

    *num_selected = n;

    return 0;
}

static int select_words(fst_conv_t *conv, double *output_probs,
        int *selected_words, int *num_selected,
        double param, unsigned int *rand_seed)
{
    ST_CHECK_PARAM(conv == NULL, -1);

    switch (conv->conv_opt.wsm) {
        case WSM_BASELINE:
            if (select_words_baseline(conv, output_probs,
                    selected_words, num_selected, param, rand_seed) < 0) {
                ST_WARNING("Failed to select_words_baseline.");
                return -1;
            }
            break;
        case WSM_SAMPLING:
            if (select_words_sampling(conv, output_probs,
                    selected_words, num_selected, param, rand_seed) < 0) {
                ST_WARNING("Failed to select_words_sampling.");
                return -1;
            }
            break;
        case WSM_MAJORITY:
            if (select_words_majority(conv, output_probs,
                    selected_words, num_selected, param, rand_seed) < 0) {
                ST_WARNING("Failed to select_words_majority.");
                return -1;
            }
            break;
        default:
            ST_WARNING("Unkown word selection method");
            return -1;
    }

    return 0;
}

static int fst_conv_search_children(fst_conv_t *conv, int sid, int word_id)
{
    int ch;
    int l, h;

    ST_CHECK_PARAM(conv == NULL || sid < 0 || word_id < 0, -2);

    if (sid >= conv->n_fst_children) {
        ST_WARNING("invalid sid for search arcs[%d] "
                ">= n_fst_children[%d]", sid, conv->n_fst_children);
        return -2;
    }

    if (conv->fst_children[sid].num_children <= 0) {
        return -1;
    }

    l = conv->fst_children[sid].first_child;
    h = l + conv->fst_children[sid].num_children - 1;

    while (l <= h) {
        ch = (l + h) / 2;
        if (conv->fst_states[ch].word_id == word_id) {
            return ch;
        } else if (conv->fst_states[ch].word_id > word_id) {
            h = ch - 1;
        } else {
            l = ch + 1;
        }
    }

    return -1;
}

static int fst_conv_find_word_hist(fst_conv_t *conv,
        fst_conv_args_t *args, int sid)
{
    int p;
    int i;

    ST_CHECK_PARAM(conv == NULL || args == NULL, -1);

    args->word_hist[0] = conv->fst_states[sid].word_id;
    args->num_word_hist = 1;
    p = conv->fst_states[sid].parent;
    while (p != -1 && conv->fst_states[p].word_id != -1 /* init state */) {
        if (args->num_word_hist >= args->cap_word_hist) {
            args->word_hist = (int *)realloc(args->word_hist,
                    sizeof(int) * (args->num_word_hist + 32));
            if (args->word_hist == NULL) {
                ST_WARNING("Failed to realloc word_hist");
                return -1;
            }
            args->cap_word_hist = args->num_word_hist + 32;
        }
        args->word_hist[args->num_word_hist] = conv->fst_states[p].word_id;
        args->num_word_hist++;
        p = conv->fst_states[p].parent;
    }

    // reverse the array
    for (i = 0; i < args->num_word_hist / 2; i++) {
        p = args->word_hist[i];
        args->word_hist[i] = args->word_hist[args->num_word_hist - 1 - i];
        args->word_hist[args->num_word_hist - 1 - i] = p;
    }

    return 0;
}

static int fst_conv_find_backoff(fst_conv_t *conv, int *word_hist,
        int num_word_hist, int sid)
{
    int i, j;
    int backoff_sid;

    ST_CHECK_PARAM(conv == NULL || word_hist == NULL || sid < 0, -1);

    assert(word_hist[0] == bos_id(conv));

    if (word_hist[1] == ANY_ID) {
        i = 3; // <s><any>XYZ -> <s><any>YZ -> <s><any>Z
    } else {
        i = 2; // <s>ABCD -> <s><any>BCD -> <s><any>CD -> <s><any>D
    }
    for (; i < num_word_hist; i++) {
        backoff_sid = FST_BACKOFF_STATE;
        for (j = i; j < num_word_hist; j++) {
            backoff_sid = fst_conv_search_children(conv,
                    backoff_sid, word_hist[j]);
            if (backoff_sid < -1) {
                ST_WARNING("Failed to fst_conv_search_children.");
                return -1;
            } else if (backoff_sid < 0) { // not found
                break;
            }
        }

        if (backoff_sid >= 0) {
            return backoff_sid;
        }
    }

    // high order backoff state not found, just return the unigram backoff state.
    return FST_BACKOFF_STATE;
}

// This function finds the prob for <word> from <sid>,
// backing-off if needed
static int fst_conv_get_prob(fst_conv_t *conv, int sid, int word, double *prob)
{
    int ch;

    ST_CHECK_PARAM(conv == NULL || sid < 0 || word < 0 || prob == NULL, -1);

    *prob = 0.0;

    if (sid >= conv->n_fst_children) {
        ST_WARNING("Can not get prob for state[%d], "
                "since its children are not stored.", sid);
        return -1;
    }

    if (word == SENT_END_ID) {
        if (conv->final_probs[sid] <= 0.0) {
            ST_WARNING("state[%d]'s sent_end_prob is not valid[%f]",
                    sid, conv->final_probs[sid]);
        }
        *prob += log(conv->final_probs[sid]);
    } else {
        while (true) {
            ch = fst_conv_search_children(conv, sid, word);
            if (ch < -1) {
                ST_WARNING("Failed to fst_conv_search_children.");
                return -1;
            } else if (ch >= 0) { // found
                if (conv->in_probs[ch] <= 0.0) {
                    ST_WARNING("state[%d]'s in_prob is not valid[%f]",
                            ch, conv->in_probs[ch]);
                }
                *prob += log(conv->in_probs[ch]);
                break;
            }

            // backoff
            if (conv->backoff_states[sid] < 0) {
                ST_WARNING("state[%d] has no backoff arc", sid);
                return -1;
            }

            if (conv->bows[sid] == 0.0) {
                ST_WARNING("state[%d]'s backoff_prob is not valid[%f]",
                        sid, conv->bows[sid]);
            }
            *prob += log(conv->bows[sid]);

            sid = conv->backoff_states[sid];
        }
    }

    *prob = exp(*prob);

    return 0;
}

static int fst_conv_expand(fst_conv_t *conv, fst_conv_args_t *args)
{
    updater_t *updater;
    real_t *state;
    real_t *new_state;
    double *output_probs;
    double ws_arg = 0.0;
    int output_size;

    double nominator;
    double denominator;
    double backoff_prob;
    int backoff_sid = -1;
    int sid;
    int new_sid, ret_sid;
    int to_sid;
    bcache_id_t model_state_id = -1;
    int word;

    int i, n, o;
    bool no_backoff;

    ST_CHECK_PARAM(conv == NULL || args == NULL, -1);

    updater = conv->updaters[args->tid];
    sid = args->sid;
    output_probs = args->output_probs;
    output_size = updater->out_updater->output->output_size;

    if (conv->model_state_cache != NULL
            && conv->fst_states[sid].model_state_id >= 0) {
        state = (real_t *)st_block_cache_read(conv->model_state_cache,
                conv->fst_states[sid].model_state_id);
        if (state == NULL) {
            ST_WARNING("Failed to st_block_cache_read state.");
            return -1;
        }
    } else {
        state = NULL;
    }

    if (fst_conv_find_word_hist(conv, args, sid) < 0) {
        ST_WARNING("Failed to fst_conv_find_word_hist");
        return -1;
    }

    no_backoff = false;
    if (conv->fst_states[sid].word_id == bos_id(conv)
            || conv->fst_states[sid].word_id == ANY_ID) {
        no_backoff = true;
    } else {
        backoff_sid = fst_conv_find_backoff(conv, args->word_hist,
                args->num_word_hist, sid);
        if (backoff_sid < 0) {
            ST_WARNING("Failed to fst_conv_find_backoff.");
            return -1;
        }

        if (sid < conv->n_fst_children) {
            if (conv->backoff_states[sid] >= 0) {
                ST_WARNING("state[%d] has multiple backoff states.", sid);
                return -1;
            }
            conv->backoff_states[sid] = backoff_sid;
        }

        nominator = 1.0;
        denominator = 1.0;
    }

    if (conv->conv_opt.max_gram > 0
            && args->num_word_hist >= conv->conv_opt.max_gram) {
        ret_sid = sid;
    } else {
        if (updater_step_with_state(updater, state, args->word_hist,
                    args->num_word_hist, output_probs) < 0) {
            ST_WARNING("Failed to updater_step_with_state.");
            return -1;
        }

        if (conv->model_state_cache != NULL) {
            if (conv->fst_states[sid].model_state_id >= 0) {
                if (st_block_cache_return(conv->model_state_cache,
                            conv->fst_states[sid].model_state_id) < 0) {
                    ST_WARNING("Failed to st_block_cache_return.");
                    return -1;
                }
                conv->fst_states[sid].model_state_id = -1;
            }

            model_state_id = -1;
            new_state = (real_t *)st_block_cache_fetch(conv->model_state_cache,
                    &model_state_id);
            if (new_state == NULL) {
                ST_WARNING("Failed to st_block_cache_fetch.");
                return -1;
            }

            if (updater_dump_state(updater, new_state) < 0) {
                ST_WARNING("Failed to updater_dump_state.");
                return -1;
            }
        }

        // logprob2prob
        for (i = 0; i < output_size; i++) {
            output_probs[i] = exp(output_probs[i]);
        }

        if (! conv->conv_opt.output_unk) {
            output_probs[UNK_ID] = -output_probs[UNK_ID];
        }

        // select words
        n = 0;
        if (no_backoff) {
            for (word = 0; word < get_output_size(conv); word++) {
                if (output_probs[word] <= 0.0) {
                    continue;
                }
                args->selected_words[n] = word;
                n++;
            }
        } else {
            assert(args->num_word_hist > 1);

            if (args->word_hist[1] == ANY_ID) {
                ws_arg = args->ws_arg * pow((args->num_word_hist - 2), args->ws_arg_power);
            } else {
                ws_arg = args->ws_arg * pow((args->num_word_hist - 1), args->ws_arg_power);
            }

            if (select_words(conv, output_probs, args->selected_words, &n,
                        ws_arg, &args->rand_seed) < 0) {
                ST_WARNING("Failed to select_words.");
                return -1;
            }

            assert(n < get_output_size(conv));
        }


        new_sid = fst_conv_add_states(conv, n - 1/* -1 for </s> */, sid,
                args->store_children);
        if (new_sid < 0) {
            ST_WARNING("Failed to fst_conv_add_states.");
            return -1;
        }
        ret_sid = new_sid;

        for (i = 0; i < n; i++) {
            word = args->selected_words[i];

            if (!no_backoff) {
                nominator -= output_probs[word];
                if (fst_conv_get_prob(conv, backoff_sid, word, &backoff_prob) < 0) {
                    ST_WARNING("Failed to fst_conv_get_prob.");
                    return -1;
                }
                denominator -= backoff_prob;
            }

            if (word == SENT_END_ID) {
                to_sid = FST_FINAL_STATE;
            } else {
                to_sid = new_sid++;
                assert(to_sid < ret_sid + n - 1);
                conv->fst_states[to_sid].word_id = word;
                if (conv->model_state_cache != NULL) {
                    conv->fst_states[to_sid].model_state_id = model_state_id;
                    // hold state_cache
                    if (st_block_cache_fetch(conv->model_state_cache,
                                &model_state_id) == NULL) {
                        ST_WARNING("Failed to st_block_cache_fetch.");
                        return -1;
                    }
                    assert(conv->fst_states[to_sid].model_state_id
                            == model_state_id);
                }

                if (fst_conv_print_ssyms(conv, to_sid, args->word_hist,
                            args->num_word_hist, word) < 0) {
                    ST_WARNING("Failed to fst_conv_print_ssyms.");
                    return -1;
                }
            }

            if (fst_conv_print_arc(conv, sid, to_sid, word,
                        output_probs[word]) < 0) {
                ST_WARNING("Failed to fst_conv_print_arc.");
                return -1;
            }

            args->num_arcs++;
            if (args->num_word_hist + 1 > args->max_gram) {
                args->num_grams = (int *)realloc(args->num_grams,
                        sizeof(int) * (args->num_word_hist + 1));
                if (args->num_grams == NULL) {
                    ST_WARNING("Failed to realloc num_grams.");
                    return -1;
                }

                args->start_sids = (int *)realloc(args->start_sids,
                        sizeof(int) * (args->num_word_hist + 1));
                if (args->start_sids == NULL) {
                    ST_WARNING("Failed to realloc start_sids.");
                    return -1;
                }

                for (o = args->max_gram; o < args->num_word_hist + 1; o++) {
                    args->num_grams[o] = 0;
                    args->start_sids[o] = 0;
                }
                args->max_gram = args->num_word_hist + 1;
                args->start_sids[args->num_word_hist] = ret_sid;
            }
            args->num_grams[args->num_word_hist] += 1;
        }

        if (conv->model_state_cache != NULL) {
            if (st_block_cache_return(conv->model_state_cache,
                        model_state_id) < 0) {
                ST_WARNING("Failed to st_block_cache_return.");
                return -1;
            }
        }
    }

    if (!no_backoff) {
        if (nominator < 0.0 || nominator > 1.0) {
            ST_WARNING("nominator is invalid[%f]", nominator);
            return -1;
        }
        if (denominator < 0.0 || denominator > 1.0) {
            ST_WARNING("denominator is invalid[%f]", denominator);
            return -1;
        }

        if (fst_conv_print_arc(conv, sid, backoff_sid,
                    phi_id(conv), nominator / denominator) < 0) {
            ST_WARNING("Failed to fst_conv_print_arc.");
            return -1;
        }

        args->num_arcs++;
    }

    if (sid % FST_CONV_LOG_STEP == 0) {
        ST_TRACE("Expanded states: %d, max gram: %d, states to be expaned: %d",
                sid, args->max_gram,
                (sid == FST_SENT_START_STATE) ? conv->n_fst_state - ret_sid
                                              : conv->n_fst_state - sid);
    }

    return ret_sid;
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
    int num_states_needed;
    int n_states;
    int cur_gram;
    int start_sid;

    updater_t *updater;
    real_t *new_state;
    bcache_id_t model_state_id;

    ST_CHECK_PARAM(conv == NULL || args == NULL, -1);

    pts = (pthread_t *)malloc(sizeof(pthread_t) * conv->n_thr);
    if (pts == NULL) {
        ST_WARNING("Failed to malloc pts");
        goto ERR;
    }

    sid = fst_conv_add_states(conv, 1, FST_SENT_START_STATE, false);
    if (sid < 0 || sid != FST_BACKOFF_STATE) {
        ST_WARNING("Failed to fst_conv_add_states for <any>.");
        goto ERR;
    }
    conv->fst_states[sid].word_id = ANY_ID;
    if (fst_conv_print_ssyms(conv, sid,
                &conv->fst_states[FST_SENT_START_STATE].word_id, 1, ANY_ID) < 0) {
        ST_WARNING("Failed to fst_conv_print_ssyms.");
        return -1;
    }

    // dump state for backoff state
    updater = conv->updaters[0];
    if (updater_step_with_state(updater, NULL, NULL, 0, NULL) < 0) {
        ST_WARNING("Failed to updater_step_with_state.");
        return -1;
    }

    if (conv->model_state_cache != NULL) {
        model_state_id = -1;
        new_state = (real_t *)st_block_cache_fetch(conv->model_state_cache,
                &model_state_id);
        if (new_state == NULL) {
            ST_WARNING("Failed to st_block_cache_fetch.");
            return -1;
        }

        if (updater_dump_state(updater, new_state) < 0) {
            ST_WARNING("Failed to updater_dump_state.");
            return -1;
        }

        conv->fst_states[sid].model_state_id = model_state_id;
    }

    for (i = 0; i < conv->n_thr; i++) {
        args[i].store_children = true;
        args[i].ws_arg = conv->conv_opt.wildcard_ws_arg;
        args[i].ws_arg_power = conv->conv_opt.wildcard_ws_arg_power;
    }

    conv->fst_children = malloc(sizeof(fst_state_children_t)
            * conv->cap_fst_states);
    if (conv->fst_children == NULL) {
        ST_WARNING("Failed to malloc fst_children.");
        return -1;
    }
    for (i = 0; i < conv->cap_fst_states; i++) {
        conv->fst_children[i].first_child = -1;
        conv->fst_children[i].num_children = -1;
    }
    conv->in_probs = malloc(sizeof(real_t) * conv->cap_fst_states);
    if (conv->in_probs == NULL) {
        ST_WARNING("Failed to malloc in_probs.");
        return -1;
    }
    conv->final_probs = malloc(sizeof(real_t) * conv->cap_fst_states);
    if (conv->final_probs == NULL) {
        ST_WARNING("Failed to malloc final_probs.");
        return -1;
    }
    conv->backoff_states = malloc(sizeof(int) * conv->cap_fst_states);
    if (conv->backoff_states == NULL) {
        ST_WARNING("Failed to malloc backoff_states.");
        return -1;
    }
    conv->bows = malloc(sizeof(real_t) * conv->cap_fst_states);
    if (conv->bows == NULL) {
        ST_WARNING("Failed to malloc bows.");
        return -1;
    }
    for (i = 0; i < conv->cap_fst_states; i++) {
        conv->in_probs[i] = 0.0;
        conv->final_probs[i] = 0.0;
        conv->bows[i] = 0.0;
        conv->backoff_states[i] = -1;
    }
    conv->n_fst_children = conv->cap_fst_states;

    cur_gram = 2;
    // maximum possible number of states could be expaned
    num_states_needed = conv->n_thr * get_output_size(conv);
    while (sid < conv->n_fst_state) {
        if (conv->n_fst_state + num_states_needed > conv->cap_fst_states) {
            if (fst_conv_realloc_states(conv, num_states_needed, true) < 0) {
                ST_WARNING("Failed to fst_conv_realloc_states.");
                goto ERR;
            }
        }

        n_states = conv->n_fst_state;

        // following lines ensure we expend order by order
        start_sid = get_start_sid(args, conv->n_thr, cur_gram + 1);
        if (sid + conv->n_thr > start_sid) {
            n_states = start_sid;
            cur_gram++;
        }

        for (n = 0; sid < n_states && n < conv->n_thr; sid++, n++) {
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
    int num_states_needed;
    int n_states;
    int cur_gram;
    int start_sid;

    ST_CHECK_PARAM(conv == NULL || args == NULL, -1);

    pts = (pthread_t *)malloc(sizeof(pthread_t) * conv->n_thr);
    if (pts == NULL) {
        ST_WARNING("Failed to malloc pts");
        goto ERR;
    }

    for (i = 0; i < conv->n_thr; i++) {
        args[i].store_children = false;
        args[i].ws_arg = conv->conv_opt.ws_arg;
        args[i].ws_arg_power = conv->conv_opt.ws_arg_power;
    }

    args[0].sid = FST_SENT_START_STATE;
    sid = fst_conv_expand(conv, args + 0);
    if (sid < 0) {
        ST_WARNING("Failed to fst_conv_expand <s>.");
        goto ERR;
    }

    cur_gram = 1;
    // maximum possible number of states could be expaned
    num_states_needed = conv->n_thr * get_output_size(conv);
    while (sid < conv->n_fst_state) {
        if (conv->n_fst_state + num_states_needed > conv->cap_fst_states) {
            if (fst_conv_realloc_states(conv, num_states_needed, false) < 0) {
                ST_WARNING("Failed to fst_conv_realloc_states.");
                goto ERR;
            }
        }

        n_states = conv->n_fst_state;

        // following lines ensure we expend order by order
        start_sid = get_start_sid(args, conv->n_thr, cur_gram + 1);
        if (sid + conv->n_thr > start_sid) {
            n_states = start_sid;
            cur_gram++;
        }

        for (n = 0; sid < n_states && n < conv->n_thr; sid++, n++) {
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
    FILE *fp = NULL;
    fst_conv_args_t *args = NULL;

    struct timeval tt0, tt1, tt2;

    int num_state;
    int num_arcs;
    int max_gram;
    int num_grams;

    int i, o;

    ST_CHECK_PARAM(conv == NULL || fst_fp == NULL, -1);

    if (fst_conv_setup(conv, fst_fp, &args) < 0) {
        ST_WARNING("Failed to fst_conv_setup.");
        goto ERR;
    }

    if (fst_conv_add_states(conv, 1, -1, false) != FST_INIT_STATE) {
        ST_WARNING("Failed to fst_conv_add_states init state.");
        goto ERR;
    }
    if (fst_conv_add_states(conv, 1, -1, false) != FST_FINAL_STATE) {
        ST_WARNING("Failed to fst_conv_add_states final state.");
        goto ERR;
    }
    conv->fst_states[FST_FINAL_STATE].word_id = SENT_END_ID;
    if (fst_conv_add_states(conv, 1, -1, false) != FST_SENT_START_STATE) {
        ST_WARNING("Failed to fst_conv_add_states for <s>.");
        goto ERR;
    }
    conv->fst_states[FST_SENT_START_STATE].word_id = bos_id(conv);

    if (fst_conv_print_ssyms(conv, FST_INIT_STATE, NULL, 0,  -1) < 0) {
        ST_WARNING("Failed to fst_conv_print_ssyms.");
        goto ERR;
    }
    if (fst_conv_print_ssyms(conv, FST_FINAL_STATE, NULL, 0,
                SENT_END_ID) < 0) {
        ST_WARNING("Failed to fst_conv_print_ssyms.");
        goto ERR;
    }
    if (fst_conv_print_ssyms(conv, FST_SENT_START_STATE, NULL, 0,
                bos_id(conv)) < 0) {
        ST_WARNING("Failed to fst_conv_print_ssyms.");
        goto ERR;
    }

    if (fst_conv_print_arc(conv, FST_INIT_STATE, FST_SENT_START_STATE,
                bos_id(conv), 1.0) < 0) {
        ST_WARNING("Failed to fst_conv_print_arc for <s>.");
        goto ERR;
    }

    if (fprintf(conv->fst_fp, "%d\n", FST_FINAL_STATE) < 0) {
        ST_WARNING("Failed to print final_state.(disk full?)");
        goto ERR;
    }

    ST_TRACE("Building wildcard subfst...")
    gettimeofday(&tt0, NULL);
    if (fst_conv_build_wildcard(conv, args) < 0) {
        ST_WARNING("Failed to fst_conv_build_wildcard.");
        goto ERR;
    }
    gettimeofday(&tt1, NULL);
    num_state = conv->n_fst_state;
    num_arcs = get_total_arcs(args, conv->n_thr);
    max_gram = get_max_gram(args, conv->n_thr);
    ST_NOTICE("Total states in wildcard subFST: %d (max_gram = %d). "
            "Total arcs: %d. Elapsed time: %.3fs.",
            conv->n_fst_state, max_gram, num_arcs,
            TIMEDIFF(tt0, tt1) / 1000.0);

    for (o = 0; o < max_gram; o++) {
        num_grams = 0;
        for (i = 0; i < conv->n_thr; i++) {
            num_grams += args[i].num_grams[o];
        }
        ST_TRACE("%d-grams in wildcard subFST: %d", (o + 1), num_grams);
    }
    for (i = 0; i < conv->n_thr; i++) {
        safe_free(args[i].num_grams);
        args[i].max_gram = 0;
    }

    if (fst_conv_reset(conv, args) < 0) {
        ST_WARNING("Failed to fst_conv_reset.");
        goto ERR;
    }

    ST_TRACE("Building normal subfst...")
    gettimeofday(&tt1, NULL);
    if (fst_conv_build_normal(conv, args) < 0) {
        ST_WARNING("Failed to fst_conv_build_normal.");
        goto ERR;
    }
    gettimeofday(&tt2, NULL);
    ST_NOTICE("Total states in normal subFST: %d (max_gram = %d). "
            "Total arcs: %d, Elapsed time: %.3fs.",
            conv->n_fst_state - num_state, get_max_gram(args, conv->n_thr),
            get_total_arcs(args, conv->n_thr) - num_arcs + 1/* <s> arc */,
            TIMEDIFF(tt1, tt2) / 1000.0);
    for (o = 0; o < max_gram; o++) {
        num_grams = 0;
        for (i = 0; i < conv->n_thr; i++) {
            num_grams += args[i].num_grams[o];
        }
        if (o == 0) {
            num_grams += 1; /* <s> gram */
        }
        ST_TRACE("%d-grams in normal subFST: %d", (o + 1), num_grams);
    }

    ST_NOTICE("Total states in FST: %d (max_gram = %d). "
            "Total arcs: %d, Elapsed time: %.3fs.",
            conv->n_fst_state, max(max_gram, get_max_gram(args, conv->n_thr)),
            get_total_arcs(args, conv->n_thr) + 1/* <s> arc*/,
            TIMEDIFF(tt0, tt2) / 1000.0);

    safe_fst_conv_args_list_destroy(args, conv->n_thr);

    if (conv->conv_opt.word_syms_file[0] != '\0') {
        fp = st_fopen(conv->conv_opt.word_syms_file, "w");
        if (fp == NULL) {
            ST_WARNING("Failed to st_fopen word syms file[%s]",
                    conv->conv_opt.word_syms_file);
            goto ERR;
        }
        if (vocab_save_syms(conv->connlm->vocab, fp, true) < 0) {
            ST_WARNING("Failed to vocab_save_syms to file[%s]",
                    conv->conv_opt.word_syms_file);
            goto ERR;
        }
        if (fprintf(fp, "%s\t%d\n", PHI, phi_id(conv) + 1) < 0) {
            ST_WARNING("Failed to write out %s.(disk full?)", PHI);
            goto ERR;
        }
        safe_fclose(fp);
    }

    return 0;

ERR:
    safe_fst_conv_args_list_destroy(args, conv->n_thr);
    safe_fclose(fp);
    return -1;
}
