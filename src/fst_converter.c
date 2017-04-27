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
#include <stutils/st_string.h>
#include <stutils/st_int.h>
#include <stutils/st_io.h>

#include "fst_converter.h"

#define FST_CONV_LOG_STEP 10000

#define FST_INIT_STATE 0
#define FST_FINAL_STATE 1
#define FST_SENT_START_STATE 2
#define FST_WILDCARD_STATE 3

#define WILDCARD    "<wildcard>"
#define WILDCARD_ID -100

#define get_output_size(conv) (conv)->connlm->output->output_size
#define get_vocab_size(conv) (conv)->connlm->vocab->vocab_size
#define phi_id(conv) get_vocab_size(conv)
#define bos_id(conv) vocab_get_id((conv)->connlm->vocab, SENT_START)

typedef struct _fst_converter_args_t_ {
    fst_conv_t *conv;
    int tid;
    int sid;
    bloom_filter_buf_t *blm_flt_buf;

    double *output_probs;
    int *candidate_words;
    int num_candidates;
    int *selected_words;

    bool store_children;

    int *word_hist;
    int cap_word_hist;
    int num_word_hist;

    int num_arcs;
    int cur_gram;
    int *num_grams;
    int *start_sids; /* start sid for every gram. */
} fst_conv_args_t;

static void fst_conv_args_destroy(fst_conv_args_t *args)
{
    if (args == NULL) {
        return;
    }

    args->tid = -1;

    safe_bloom_filter_buf_destroy(args->blm_flt_buf);

    safe_st_free(args->output_probs);
    safe_st_free(args->candidate_words);
    args->num_candidates = 0;
    safe_st_free(args->selected_words);

    safe_st_free(args->word_hist);
    args->num_word_hist = 0;
    args->cap_word_hist = 0;

    safe_st_free(args->num_grams);
    safe_st_free(args->start_sids);

    args->conv = NULL;
}

static int fst_conv_args_init(fst_conv_args_t *args,
        fst_conv_t *conv, int tid)
{
    int o;

    ST_CHECK_PARAM(args == NULL || conv == NULL, -1);

    args->conv = conv;
    args->tid = tid;

    args->output_probs = (double *)st_malloc(sizeof(double)
            * get_output_size(conv));
    if (args->output_probs == NULL) {
        ST_WARNING("Failed to st_malloc output_probs.");
        goto ERR;
    }

    args->candidate_words = (int *)st_malloc(sizeof(int) * get_output_size(conv));
    if (args->candidate_words == NULL) {
        ST_WARNING("Failed to st_malloc candidate_words.");
        goto ERR;
    }

    args->selected_words = (int *)st_malloc(sizeof(int) * get_output_size(conv));
    if (args->selected_words == NULL) {
        ST_WARNING("Failed to st_malloc selected_words.");
        goto ERR;
    }
    args->num_candidates = 0;

    args->cap_word_hist = 32;
    args->word_hist = (int *)st_malloc(sizeof(int) * args->cap_word_hist);
    if (args->word_hist == NULL) {
        ST_WARNING("Failed to st_malloc word_hist");
        goto ERR;
    }

    if (conv->blm_flt != NULL) {
        args->blm_flt_buf = bloom_filter_buf_create(conv->blm_flt);
        if (args->blm_flt_buf == NULL) {
            ST_WARNING("Failed to bloom_filter_buf_create.");
            goto ERR;
        }
    }

    args->num_grams = (int *)st_malloc(sizeof(int) * conv->conv_opt.max_gram);
    if (args->num_grams == NULL) {
        ST_WARNING("Failed to st_realloc num_grams.");
        return -1;
    }

    args->start_sids = (int *)st_malloc(sizeof(int) * conv->conv_opt.max_gram);
    if (args->start_sids == NULL) {
        ST_WARNING("Failed to st_realloc start_sids.");
        return -1;
    }

    for (o = 0; o < conv->conv_opt.max_gram; o++) {
        args->num_grams[o] = 0;
        args->start_sids[o] = INT_MAX;
    }
    args->cur_gram = 0;

    return 0;

ERR:
    fst_conv_args_destroy(args);
    return -1;
}

#define safe_fst_conv_args_list_destroy(ptr, n) do {\
    if((ptr) != NULL) {\
        fst_conv_args_list_destroy(ptr, n);\
        safe_st_free(ptr);\
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

    args = (fst_conv_args_t *)st_malloc(sizeof(fst_conv_args_t)
            * conv->n_thr);
    if (args == NULL) {
        ST_WARNING("Failed to st_malloc fst_conv_args_t");
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

static int get_start_sid(fst_conv_args_t *args, int n, int order)
{
    int start_sid = INT_MAX;

    int i;

    for (i = 0; i < n; i++) {
        if (args[i].start_sids[order - 1] < start_sid) {
            start_sid = args[i].start_sids[order - 1];
        }
    }

    return start_sid;
}

int fst_conv_load_opt(fst_conv_opt_t *conv_opt,
        st_opt_t *opt, const char *sec_name)
{
    char str[MAX_ST_CONF_LEN];

    ST_CHECK_PARAM(conv_opt == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_BOOL(opt, sec_name, "PRINT_SYMS",
            conv_opt->print_syms, true,
            "Print symbols instead of ids in FST, if true.");

    ST_OPT_SEC_GET_STR(opt, sec_name, "WORD_SYMS_FILE",
            conv_opt->word_syms_file, MAX_DIR_LEN, "",
            "File to be written out word symbols.");

    ST_OPT_SEC_GET_STR(opt, sec_name, "STATE_SYMS_FILE",
            conv_opt->state_syms_file, MAX_DIR_LEN, "",
            "File to be written out state symbols(for debug).");

    ST_OPT_SEC_GET_STR(opt, sec_name, "WORD_SELECTION_METHOD",
            str, MAX_ST_CONF_LEN, "Majority",
            "Word selection method(Majority/Beam)");
    if (strcasecmp(str, "Beam") == 0) {
        conv_opt->wsm = WSM_BEAM;
    } else if (strcasecmp(str, "Majority") == 0) {
        conv_opt->wsm = WSM_MAJORITY;
    } else {
        ST_WARNING("Unknown word selection method[%s].", str);
        goto ST_OPT_ERR;
    }

    ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "THRESHOLD",
            conv_opt->threshold, 1.0, "threshold for word selection.");
    if (conv_opt->threshold < 0 || conv_opt->threshold > 1) {
        ST_WARNING("threshold should be in [0.0, 1.0]");
        goto ST_OPT_ERR;
    }

    ST_OPT_SEC_GET_STR(opt, sec_name, "BLOOM_FILTER_FILE",
            conv_opt->bloom_filter_file, MAX_DIR_LEN, "",
            "File stored the bloom filter.");

    ST_OPT_SEC_GET_INT(opt, sec_name, "MAX_GRAM", conv_opt->max_gram, 0,
            "Maximum gram to be expaned(decided by the bloom-filter if "
            "less than or equal to 0).");
    if (conv_opt->max_gram > 0 && conv_opt->max_gram <= 2) {
        ST_WARNING("MAX_GRAM must be larger than 2.");
        goto ST_OPT_ERR;
    }

    ST_OPT_SEC_GET_STR(opt, sec_name, "WILDCARD_STATE_FILE",
            conv_opt->wildcard_state_file, MAX_DIR_LEN, "",
            "File stored the value for wildcard state. Typically output by "
            "'connlm-wildcard-state'");

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
    safe_st_free(conv->updaters);
    conv->n_thr = 0;

    safe_st_block_cache_destroy(conv->model_state_cache);

    safe_st_free(conv->fst_states);
    (void)pthread_mutex_destroy(&conv->fst_state_lock);
    conv->cap_fst_states = 0;
    conv->n_fst_state = 0;

    safe_st_free(conv->fst_children);
    safe_st_free(conv->in_probs);
    safe_st_free(conv->final_probs);
    safe_st_free(conv->backoff_states);
    safe_st_free(conv->bows);
    conv->cap_fst_children = 0;

    (void)pthread_mutex_destroy(&conv->fst_fp_lock);
    conv->fst_fp = NULL;

    if (conv->ssyms_fp != NULL) {
        safe_fclose(conv->ssyms_fp);
        (void)pthread_mutex_destroy(&conv->ssyms_fp_lock);
    }

    safe_bloom_filter_destroy(conv->blm_flt);
    safe_st_free(conv->wildcard_state);
}

fst_conv_t* fst_conv_create(connlm_t *connlm, int n_thr,
        fst_conv_opt_t *conv_opt)
{
    fst_conv_t *conv = NULL;

    FILE *fp = NULL;
    char *line = NULL;
    size_t line_sz = 0;

    int i;
    bool err;

    ST_CHECK_PARAM(connlm == NULL || n_thr <= 0 || conv_opt == NULL, NULL);

    if (conv_opt->max_gram <= 0) {
        if (conv_opt->bloom_filter_file[0] == '\0') {
            ST_WARNING("Can not determine MAX_GRAM, since BLOOM_FILTER_FILE "
                    "not set");
            goto ERR;
        }
    }

    conv = (fst_conv_t *)st_malloc(sizeof(fst_conv_t));
    if (conv == NULL) {
        ST_WARNING("Failed to st_malloc converter.");
        return NULL;
    }
    memset(conv, 0, sizeof(fst_conv_t));

    conv->connlm = connlm;
    conv->n_thr = n_thr;
    conv->conv_opt = *conv_opt;

    conv->updaters = (updater_t **)st_malloc(sizeof(updater_t*)*n_thr);
    if (conv->updaters == NULL) {
        ST_WARNING("Failed to st_malloc updaters.");
        goto ERR;
    }
    memset(conv->updaters, 0, sizeof(updater_t*) * n_thr);

    for (i = 0; i < conv->n_thr; i++) {
        conv->updaters[i] = updater_create(connlm);
        if (conv->updaters[i] == NULL) {
            ST_WARNING("Failed to updater_create[%d].", i);
            goto ERR;
        }
    }

    if (conv->updaters[0]->input_updater->ctx_rightmost > 0) {
        ST_WARNING("Can not converting: future words in input context.");
        goto ERR;
    }

    if (conv->conv_opt.bloom_filter_file[0] != '\0') {
        fp = st_fopen(conv->conv_opt.bloom_filter_file, "r");
        if (fp == NULL) {
            ST_WARNING("Failed to st_fopen bloom_filter_file[%s].",
                    conv->conv_opt.bloom_filter_file);
            goto ERR;
        }
        conv->blm_flt = bloom_filter_load(fp);
        if (conv->blm_flt == NULL) {
            ST_WARNING("Failed to bloom_filter_load from[%s].",
                    conv->conv_opt.bloom_filter_file);
            goto ERR;
        }
        safe_fclose(fp);

        if (! vocab_equal(conv->connlm->vocab, conv->blm_flt->vocab)) {
            ST_WARNING("Vocab of bloom filter and connlm model not match.");
            goto ERR;
        }

        if (conv->conv_opt.max_gram <= 0) {
            conv->conv_opt.max_gram = conv->blm_flt->blm_flt_opt.max_order;
        }
    }

    if (conv->conv_opt.wildcard_state_file[0] != '\0') {
        fp = st_fopen(conv->conv_opt.wildcard_state_file, "r");
        if (fp == NULL) {
            ST_WARNING("Failed to st_fopen wildcard_state_file[%s].",
                    conv->conv_opt.wildcard_state_file);
            goto ERR;
        }

        while (st_fgets(&line, &line_sz, fp, &err)) {
            remove_newline(line);

            if (line[0] == '\0' || line[0] == '#') {
                continue;
            }

            conv->ws_size = parse_vec(line, &conv->wildcard_state, NULL, 0);
            if (conv->ws_size < 0) {
                ST_WARNING("Failed to parse_vec. [%s]", line);
                goto ERR;
            }

            break;
        }

        safe_st_free(line);
        safe_fclose(fp);

        if (err) {
            ST_WARNING("st_fgets error");
            goto ERR;
        }

    }

    return conv;

ERR:
    safe_st_free(line);
    safe_fclose(fp);
    safe_fst_conv_destroy(conv);
    return NULL;
}

static int fst_conv_st_realloc_states(fst_conv_t *conv, int num_extra,
        bool store_children)
{
    int i;
    int num_new_states;

    ST_CHECK_PARAM(conv == NULL, -1);

    num_new_states = conv->cap_fst_states + num_extra;

    conv->fst_states = st_realloc(conv->fst_states,
            sizeof(fst_state_t) * num_new_states);
    if (conv->fst_states == NULL) {
        ST_WARNING("Failed to st_realloc fst_states.");
        return -1;
    }

    if (store_children) {
        conv->fst_children = st_realloc(conv->fst_children,
                sizeof(fst_state_children_t) * num_new_states);
        if (conv->fst_children == NULL) {
            ST_WARNING("Failed to st_realloc fst_children.");
            return -1;
        }
        for (i = conv->cap_fst_states; i < num_new_states; i++) {
            conv->fst_children[i].first_child = -1;
            conv->fst_children[i].num_children = -1;
        }
        conv->in_probs = st_realloc(conv->in_probs,
                sizeof(real_t) * num_new_states);
        if (conv->in_probs == NULL) {
            ST_WARNING("Failed to st_realloc in_probs.");
            return -1;
        }
        conv->final_probs = st_realloc(conv->final_probs,
                sizeof(real_t) * num_new_states);
        if (conv->final_probs == NULL) {
            ST_WARNING("Failed to st_realloc final_probs.");
            return -1;
        }
        conv->backoff_states = st_realloc(conv->backoff_states,
                sizeof(int) * num_new_states);
        if (conv->backoff_states == NULL) {
            ST_WARNING("Failed to st_realloc backoff_states.");
            return -1;
        }
        conv->bows = st_realloc(conv->bows, sizeof(real_t) * num_new_states);
        if (conv->bows == NULL) {
            ST_WARNING("Failed to st_realloc bows.");
            return -1;
        }
        for (i = conv->cap_fst_states; i < num_new_states; i++) {
            conv->in_probs[i] = 0.0;
            conv->final_probs[i] = -1.0;
            conv->bows[i] = 0.0;
            conv->backoff_states[i] = -1;
        }
        conv->cap_fst_children = num_new_states;
    }

    conv->cap_fst_states = num_new_states;

    return 0;
}

static int fst_conv_setup(fst_conv_t *conv, FILE *fst_fp,
        fst_conv_args_t **args)
{
    int i;
    int state_size;

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
        if (updater_setup_multicall(conv->updaters[i]) < 0) {
            ST_WARNING("Failed to updater_setup_multicall.");
            return -1;
        }
    }

    *args = fst_conv_args_list_create(conv);
    if (*args == NULL) {
        ST_WARNING("Failed to fst_conv_args_list_create.");
        return -1;
    }

    state_size = updater_state_size(conv->updaters[0]);
    if (conv->ws_size > 0) {
        if (state_size != conv->ws_size) {
            ST_WARNING("wildcard state size not match");
            return -1;
        }
    }

    if (state_size > 0) {
        conv->model_state_cache = st_block_cache_create(
                sizeof(real_t) * state_size, get_output_size(conv));
        if (conv->model_state_cache == NULL) {
            ST_WARNING("Failed to st_block_cache_create model_state_cache.");
            return -1;
        }
    } else {
        ST_WARNING("Converting with stateless model could "
                "probably not stop.");
    }

    if (pthread_mutex_init(&conv->fst_state_lock, NULL) != 0) {
        ST_WARNING("Failed to pthread_mutex_init fst_state_lock.");
        return -1;
    }

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
        if (parent >= conv->cap_fst_children) {
            ST_WARNING("Invalid parent[%d]", parent);
            return -1;
        }

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
    if (wid == WILDCARD_ID) {
        return WILDCARD;
    }

    if (wid < 0) {
        return "";
    }

    return vocab_get_word(conv->connlm->vocab, wid);
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
        if (from < conv->cap_fst_children) {
            if (conv->final_probs[from] != -1.0) {
                ST_WARNING("state[%d] has multiple final probs.", from);
                return -1;
            }
            conv->final_probs[from] = (real_t)weight;
        }
    } else if (wid == phi_id(conv)) {
        if (from < conv->cap_fst_children) {
            if (conv->bows[from] != 0.0) {
                ST_WARNING("state[%d] has multiple bows.", from);
                return -1;
            }
            conv->bows[from] = (real_t)weight;
        }
    } else {
        if (to < conv->cap_fst_children) {
            if (conv->in_probs[to] != 0.0) {
                ST_WARNING("state[%d] has multiple in_probs.", to);
                return -1;
            }
            conv->in_probs[to] = (real_t)weight;
        }
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

static int select_words_beam(fst_conv_t *conv, fst_conv_args_t *args,
        double beam)
{
    double max_prob;
    int word;
    int i, n;

    ST_CHECK_PARAM(conv == NULL || args == NULL, -1);

    max_prob = -INFINITY;
    for (i = 0; i < args->num_candidates; i++) {
        word = args->candidate_words[i];

        if (args->output_probs[word] > max_prob) {
            max_prob = args->output_probs[word];
        }
    }

    n = 0;
    for (i = 0; i < args->num_candidates; i++) {
        word = args->candidate_words[i];

        if (args->output_probs[word] >= max_prob - beam) {
            args->selected_words[n++] = word;
        }
    }

    return n;
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

static int select_words_majority(fst_conv_t *conv, fst_conv_args_t *args,
        double threshold)
{
    double total_prob;
    int n;

    double acc;
    int i;

    ST_CHECK_PARAM(conv == NULL || args == NULL, -1);

    total_prob = 0.0;
    for (i = 0; i < args->num_candidates; i++) {
        args->selected_words[i] = args->candidate_words[i];
        total_prob += args->output_probs[args->candidate_words[i]];
    }

    st_qsort(args->selected_words, args->num_candidates, sizeof(int),
            prob_cmp, (void *)args->output_probs);

    acc = 0.0;
    n = 0;
    for (i = 0; i < args->num_candidates; i++) {
        acc += args->output_probs[args->selected_words[i]] / total_prob;
        n++;
        if (acc >= threshold) {
            break;
        }
    }

    st_int_sort(args->selected_words, n);

    return n;
}

static int get_condidate_word(fst_conv_t *conv, fst_conv_args_t *args)
{
    int n;
    int word;
    bool choose;

    ST_CHECK_PARAM(conv == NULL || args == NULL, -1);

    n = 0;
    if (conv->blm_flt == NULL) {
        for (word = 0; word < get_output_size(conv); word++) {
            if (word == UNK_ID) {
                continue;
            }
            args->candidate_words[n] = word;
            ++n;
        }
    } else {
        for (word = 0; word < get_output_size(conv); word++) {
            if (word == UNK_ID) {
                continue;
            }
            args->word_hist[args->num_word_hist] = word;

            choose = false;
            if (args->word_hist[0] == WILDCARD_ID) {
                choose = bloom_filter_lookup(conv->blm_flt, args->blm_flt_buf,
                        args->word_hist + 1, args->num_word_hist);
            } else {
                choose = bloom_filter_lookup(conv->blm_flt, args->blm_flt_buf,
                        args->word_hist, args->num_word_hist + 1);
            }

            if (choose) {
                args->candidate_words[n] = word;
                ++n;
            }
        }
    }

    return n;
}

// fill the expaned words in args->selected_words from args->candidate_words,
// and return the length
static int select_words(fst_conv_t *conv, fst_conv_args_t *args, double thresh)
{
    int n;

    ST_CHECK_PARAM(conv == NULL || args == NULL, -1);

    if (args->num_candidates <= 0) {
        return 0;
    }

    switch (conv->conv_opt.wsm) {
        case WSM_BEAM:
            n = select_words_beam(conv, args, thresh);
            if (n < 0) {
                ST_WARNING("Failed to select_words_beam.");
                return -1;
            }
            break;
        case WSM_MAJORITY:
            n = select_words_majority(conv, args, thresh);
            if (n < 0) {
                ST_WARNING("Failed to select_words_majority.");
                return -1;
            }
            break;
        default:
            ST_WARNING("Invalid word selection method");
            return -1;
    }

    return n;
}

static int fst_conv_search_children(fst_conv_t *conv, int sid, int word_id)
{
    int ch;
    int l, h;

    ST_CHECK_PARAM(conv == NULL || sid < 0 || word_id < 0, -2);

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
        if (args->num_word_hist + 1 >= args->cap_word_hist) { /* +1 for bloom filter lookup */
            args->word_hist = (int *)st_realloc(args->word_hist,
                    sizeof(int) * (args->num_word_hist + 32));
            if (args->word_hist == NULL) {
                ST_WARNING("Failed to st_realloc word_hist");
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

    assert(word_hist[0] == WILDCARD_ID || word_hist[0] == bos_id(conv));

    i = 2; // <wildcard>ABCD -> <wildcard>BCD -> <wildcard>CD -> <wildcard>D
    for (; i < num_word_hist; i++) {
        backoff_sid = FST_WILDCARD_STATE;
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

    // high order backoff state not found, just return the <wildcard> state.
    return FST_WILDCARD_STATE;
}

// This function finds the prob for <word> from <sid>,
// backing-off if needed
static int fst_conv_get_prob(fst_conv_t *conv, int sid, int word, double *prob)
{
    int ch;

    ST_CHECK_PARAM(conv == NULL || sid < 0 || word < 0 || prob == NULL, -1);

    if (sid >= conv->cap_fst_children) {
        ST_WARNING("sid not in fst_children cannot get prob");
        return -1;
    }

    *prob = 0.0;

    if (word == SENT_END_ID) {
        while (true) {
            if(conv->final_probs[sid] != -1.0) {
                if (conv->final_probs[sid] <= 0.0) {
                    ST_WARNING("state[%d]'s sent_end_prob is not valid[%f]",
                            sid, conv->final_probs[sid]);
                }
                *prob += log(conv->final_probs[sid]);
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
    double logp;

    double nominator;
    double denominator;
    double backoff_prob;
    int backoff_sid = -1;
    int sid;
    int new_sid, ret_sid;
    int to_sid;
    bcache_id_t model_state_id = -1;
    int word;
    int hist_order;

    int i, n;
    bool no_backoff;

    ST_CHECK_PARAM(conv == NULL || args == NULL, -1);

    updater = conv->updaters[args->tid];
    sid = args->sid;
    output_probs = args->output_probs;

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

    if (args->word_hist[0] == WILDCARD_ID) {
        hist_order = args->num_word_hist - 1;
    } else {
        hist_order = args->num_word_hist;
    }

    no_backoff = false;
    if (conv->fst_states[sid].word_id == WILDCARD_ID) {
        no_backoff = true;
    } else {
        backoff_sid = fst_conv_find_backoff(conv, args->word_hist,
                args->num_word_hist, sid);
        if (backoff_sid < 0) {
            ST_WARNING("Failed to fst_conv_find_backoff.");
            return -1;
        }

        if (sid < conv->cap_fst_children) {
            if (conv->backoff_states[sid] >= 0) {
                ST_WARNING("state[%d] has multiple backoff states.", sid);
                return -1;
            }
            conv->backoff_states[sid] = backoff_sid;
        }

        nominator = 1.0;
        denominator = 1.0;
    }

    if (conv->conv_opt.max_gram > 0 && hist_order >= conv->conv_opt.max_gram) {
        ret_sid = sid;
    } else {
        if (no_backoff) {
            for (word = 0; word < get_output_size(conv); word++) {
                args->candidate_words[args->num_candidates] = word;
                args->num_candidates++;
            }
        } else {
            args->num_candidates = get_condidate_word(conv, args);
            if (args->num_candidates < 0) {
                ST_WARNING("Failed to get_condidate_word.");
                return -1;
            }
        }

        if (args->word_hist[0] == WILDCARD_ID) {
            if (updater_step_with_state(updater, state, args->word_hist + 1,
                        args->num_word_hist - 1) < 0) {
                ST_WARNING("Failed to updater_step_with_state.");
                return -1;
            }
        } else {
            if (updater_step_with_state(updater, state, args->word_hist,
                        args->num_word_hist) < 0) {
                ST_WARNING("Failed to updater_step_with_state.");
                return -1;
            }
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

        for (i = 0; i < args->num_candidates; i++) {
            word = args->candidate_words[i];
            if (updater_forward_out_word(updater, word, &logp) < 0) {
                ST_WARNING("Falied to updater_forward_out_word.");
                return -1;
            }
            output_probs[word] = exp(logp);
        }

        if (no_backoff) {
            for (i = 0; i < args->num_candidates; i++) {
                args->selected_words[i] = args->candidate_words[i];
            }
            n = args->num_candidates;
        } else {
            n = select_words(conv, args, conv->conv_opt.threshold);
            if (n < 0) {
                ST_WARNING("Failed to select_words.");
                return -1;
            }
        }
        assert(n <= get_output_size(conv));

        new_sid = fst_conv_add_states(conv,
                (n > 0 && args->selected_words[0] == SENT_END_ID) ? n - 1 : n,
                sid, args->store_children);
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
            if (hist_order + 1 > args->cur_gram) {
                args->cur_gram = hist_order + 1;
                args->start_sids[hist_order] = ret_sid;
            }
            args->num_grams[hist_order] += 1;
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
        ST_TRACE("Expanded states: %d, gram: %d, states to be expaned: %d",
                sid, args->cur_gram,
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

static int fst_conv_prepare_build(fst_conv_t *conv)
{
    ST_CHECK_PARAM(conv == NULL, -1);

    if (fst_conv_st_realloc_states(conv,
                10 * conv->n_thr * get_output_size(conv), true) < 0) {
        ST_WARNING("Failed to fst_conv_st_realloc_states.");
        return -1;
    }

    if (fst_conv_add_states(conv, 1, -1, false) != FST_INIT_STATE) {
        ST_WARNING("Failed to fst_conv_add_states init state.");
        return -1;
    }
    if (fst_conv_add_states(conv, 1, -1, false) != FST_FINAL_STATE) {
        ST_WARNING("Failed to fst_conv_add_states final state.");
        return -1;
    }
    conv->fst_states[FST_FINAL_STATE].word_id = SENT_END_ID;
    if (fst_conv_add_states(conv, 1, -1, false) != FST_SENT_START_STATE) {
        ST_WARNING("Failed to fst_conv_add_states for <s>.");
        return -1;
    }
    conv->fst_states[FST_SENT_START_STATE].word_id = bos_id(conv);
    if (fst_conv_add_states(conv, 1, -1, false) != FST_WILDCARD_STATE) {
        ST_WARNING("Failed to fst_conv_add_states for <wildcard>.");
        return -1;
    }
    conv->fst_states[FST_WILDCARD_STATE].word_id = WILDCARD_ID;

    if (fst_conv_print_ssyms(conv, FST_INIT_STATE, NULL, 0,  -1) < 0) {
        ST_WARNING("Failed to fst_conv_print_ssyms.");
        return -1;
    }
    if (fst_conv_print_ssyms(conv, FST_FINAL_STATE, NULL, 0,
                SENT_END_ID) < 0) {
        ST_WARNING("Failed to fst_conv_print_ssyms.");
        return -1;
    }
    if (fst_conv_print_ssyms(conv, FST_SENT_START_STATE, NULL, 0,
                bos_id(conv)) < 0) {
        ST_WARNING("Failed to fst_conv_print_ssyms.");
        return -1;
    }
    if (fst_conv_print_ssyms(conv, FST_WILDCARD_STATE, NULL, 0,
                WILDCARD_ID) < 0) {
        ST_WARNING("Failed to fst_conv_print_ssyms.");
        return -1;
    }

    if (fst_conv_print_arc(conv, FST_INIT_STATE, FST_SENT_START_STATE,
                bos_id(conv), 1.0) < 0) {
        ST_WARNING("Failed to fst_conv_print_arc for <s>.");
        return -1;
    }

    if (fprintf(conv->fst_fp, "%d\n", FST_FINAL_STATE) < 0) {
        ST_WARNING("Failed to print final_state.(disk full?)");
        return -1;
    }

    return 0;
}

static int fst_conv_build(fst_conv_t *conv, fst_conv_args_t *args,
        int init_sid, int init_order, bool store_children)
{
    pthread_t *pts = NULL;
    int sid;
    int i, n;
    int num_states_needed;
    int n_states;
    int cur_gram;
    int start_sid;

    ST_CHECK_PARAM(conv == NULL || args == NULL, -1);

    pts = (pthread_t *)st_malloc(sizeof(pthread_t) * conv->n_thr);
    if (pts == NULL) {
        ST_WARNING("Failed to st_malloc pts");
        goto ERR;
    }

    // maximum possible number of states could be expaned
    num_states_needed = conv->n_thr * get_output_size(conv);
    if (fst_conv_st_realloc_states(conv, num_states_needed,
                store_children) < 0) {
        ST_WARNING("Failed to fst_conv_st_realloc_states.");
        goto ERR;
    }

    for (n = 0; n < conv->n_thr; n++) {
        args[n].store_children = store_children;
        for (i = 0; i < conv->conv_opt.max_gram; i++) {
            args[n].cur_gram = init_order;
            args[n].start_sids[i] = INT_MAX;
        }
    }

    cur_gram = init_order;
    sid = init_sid;
    while (sid < conv->n_fst_state) {
        if (conv->n_fst_state + num_states_needed > conv->cap_fst_states) {
            if (fst_conv_st_realloc_states(conv, num_states_needed,
                        store_children) < 0) {
                ST_WARNING("Failed to fst_conv_st_realloc_states.");
                goto ERR;
            }
        }

        n_states = conv->n_fst_state;

        // following lines ensure we expend order by order
        if (cur_gram < conv->conv_opt.max_gram) {
            start_sid = get_start_sid(args, conv->n_thr, cur_gram + 1);
            if (sid + conv->n_thr > start_sid) {
                n_states = start_sid;
                cur_gram++;
            }
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
                ST_WARNING("Failed to pthread_join.");
                goto ERR;
            }
        }

        if (conv->err != 0) {
            ST_WARNING("Error in worker threads");
            goto ERR;
        }
    }

    safe_st_free(pts);
    return 0;

ERR:

    safe_st_free(pts);
    return -1;
}

static int fst_conv_build_wildcard(fst_conv_t *conv, fst_conv_args_t *args)
{
    real_t *state;
    int sid;

    ST_CHECK_PARAM(conv == NULL || args == NULL, -1);

    args[0].sid = FST_WILDCARD_STATE;
    args[0].store_children = true;

    if (conv->wildcard_state != NULL && conv->model_state_cache != NULL) {
        conv->fst_states[FST_WILDCARD_STATE].model_state_id = -1;
        state = (real_t *)st_block_cache_fetch(conv->model_state_cache,
                &conv->fst_states[FST_WILDCARD_STATE].model_state_id);
        if (state == NULL) {
            ST_WARNING("Failed to st_block_cache_fetch.");
            return -1;
        }

        memcpy(state, conv->wildcard_state, sizeof(real_t) * conv->ws_size);
    }

    sid = fst_conv_expand(conv, args + 0);
    if (sid < 0) {
        ST_WARNING("Failed to fst_conv_expand <wildcard>.");
        return -1;
    }

    if (fst_conv_build(conv, args, sid, 1, true) < 0) {
        ST_WARNING("Failed to fst_conv_build");
        return -1;
    }

    return 0;
}

static int fst_conv_build_bos(fst_conv_t *conv, fst_conv_args_t *args)
{
    int sid;

    ST_CHECK_PARAM(conv == NULL || args == NULL, -1);

    args[0].sid = FST_SENT_START_STATE;
    args[0].store_children = false;

    sid = fst_conv_expand(conv, args + 0);
    if (sid < 0) {
        ST_WARNING("Failed to fst_conv_expand <s>.");
        return -1;
    }

    if (fst_conv_build(conv, args, sid, 2, false) < 0) {
        ST_WARNING("Failed to fst_conv_build");
        return -1;
    }

    return 0;
}

int fst_conv_convert(fst_conv_t *conv, FILE *fst_fp)
{
    FILE *fp = NULL;
    fst_conv_args_t *args = NULL;

    struct timeval tt1, tt2;

    int num_grams;

    int i, o;

    ST_CHECK_PARAM(conv == NULL || fst_fp == NULL, -1);

    if (fst_conv_setup(conv, fst_fp, &args) < 0) {
        ST_WARNING("Failed to fst_conv_setup.");
        goto ERR;
    }

    ST_TRACE("Building FST...")
    gettimeofday(&tt1, NULL);
    if (fst_conv_prepare_build(conv) < 0) {
        ST_WARNING("Failed to fst_conv_prepare_build.");
        goto ERR;
    }

    ST_TRACE("  Building wildcard subFST...")
    if (fst_conv_build_wildcard(conv, args) < 0) {
        ST_WARNING("Failed to fst_conv_build_wildcard.");
        goto ERR;
    }

    ST_TRACE("  Building bos subFST...")
    if (fst_conv_build_bos(conv, args) < 0) {
        ST_WARNING("Failed to fst_conv_build_bos.");
        goto ERR;
    }
    gettimeofday(&tt2, NULL);

    for (o = 0; o < conv->conv_opt.max_gram; o++) {
        num_grams = 0;
        for (i = 0; i < conv->n_thr; i++) {
            num_grams += args[i].num_grams[o];
        }
        if (o == 0) {
            num_grams += 1; /* <s> gram */
        }
        ST_TRACE("%d-grams: %d", (o + 1), num_grams);
    }

    ST_NOTICE("Total states: %d (max_gram = %d). "
            "Total arcs: %d, Elapsed time: %.3fs.",
            conv->n_fst_state, conv->conv_opt.max_gram,
            get_total_arcs(args, conv->n_thr) + 1/* <s> arc*/,
            TIMEDIFF(tt1, tt2) / 1000.0);

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
