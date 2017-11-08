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
#include <assert.h>

#include <stutils/st_macro.h>
#include <stutils/st_log.h>
#include <stutils/st_int.h>
#include <stutils/st_io.h>
#include <stutils/st_rand.h>

#include "wildcard_state.h"

int wildcard_state_load_opt(wildcard_state_opt_t *ws_opt,
        st_opt_t *opt, const char *sec_name)
{
    unsigned int seed;

    ST_CHECK_PARAM(ws_opt == NULL || opt == NULL, -1);

    ST_OPT_GET_BOOL(opt, "PRE_ACTIVATION", ws_opt->pre_activation, false,
            "Whether average values before actiation.");

    ST_OPT_GET_BOOL(opt, "RANDOM", ws_opt->random, true,
            "Whether generate random states.");

    ST_OPT_GET_INT(opt, "NUM_SAMPLINGS", ws_opt->num_samplings, 0,
            "Number of sampling sentences to generate wildcard state.");

    if (ws_opt->random || ws_opt->num_samplings > 0) {
        ST_OPT_SEC_GET_UINT(opt, sec_name, "RANDOM_SEED",
                seed, (unsigned int)time(NULL),
                "Random seed. Default is value of time(NULl).");
        st_srand(seed);
    }

    ST_OPT_GET_STR(opt, "EVAL_TEXT_FILE", ws_opt->eval_text_file,
            MAX_DIR_LEN, "", "Text file contains the sentences to "
            "be evaluated to generate wildcard state.");

    if ( ! ws_opt->random &&  ws_opt->num_samplings <= 0
            && ws_opt->eval_text_file[0] == '\0') {
        ST_ERROR("You must set at least one of RANDOM or NUM_SAMPLINGS "
                "or EVAL_TEXT_FILE");
        goto ST_OPT_ERR;
    }

    return 0;

ST_OPT_ERR:
    return -1;
}

void wildcard_state_destroy(wildcard_state_t *ws)
{
    if (ws == NULL) {
        return;
    }

    safe_st_free(ws->tmp_state);
    safe_st_free(ws->tmp_pre_ac_state);

    safe_st_free(ws->random_state);
    safe_st_free(ws->sampling_state);
    safe_st_free(ws->eval_state);

    safe_st_free(ws->state);

    ws->state_size = 0;
    safe_updater_destroy(ws->updater);
    ws->connlm = NULL;
}

wildcard_state_t* wildcard_state_create(connlm_t *connlm,
        wildcard_state_opt_t *ws_opt)
{
    wildcard_state_t *ws = NULL;

    ST_CHECK_PARAM(connlm == NULL || ws_opt == NULL, NULL);

    ws = (wildcard_state_t *)st_malloc(sizeof(wildcard_state_t));
    if (ws == NULL) {
        ST_ERROR("Failed to st_malloc wildcard_state_t.");
        return NULL;
    }
    memset(ws, 0, sizeof(wildcard_state_t));

    ws->connlm = connlm;
    ws->ws_opt = *ws_opt;

    if (connlm_setup(connlm) < 0) {
        ST_ERROR("Failed to connlm_setup.");
        goto ERR;
    }

    ws->updater = updater_create(connlm);
    if (ws->updater == NULL) {
        ST_ERROR("Failed to updater_create.");
        goto ERR;
    }

    if (updater_setup(ws->updater, false) < 0) {
        ST_ERROR("Failed to updater_setup.");
        goto ERR;
    }

    if (updater_setup_pre_ac_state(ws->updater) < 0) {
        ST_ERROR("Failed to updater_setup_pre_ac_state.");
        goto ERR;
    }

    ws->state_size = updater_state_size(ws->updater);

    ws->state = (real_t *)st_malloc(sizeof(real_t) * ws->state_size);
    if (ws->state == NULL) {
        ST_ERROR("Failed to st_malloc state");
        goto ERR;
    }
    memset(ws->state, 0, sizeof(real_t) * ws->state_size);

    if (ws->ws_opt.pre_activation) {
        ws->tmp_pre_ac_state = (real_t *)st_malloc(
                sizeof(real_t) * ws->state_size);
        if (ws->tmp_pre_ac_state == NULL) {
            ST_ERROR("Failed to st_malloc tmp_pre_ac_state");
            goto ERR;
        }
        memset(ws->tmp_pre_ac_state, 0, sizeof(real_t) * ws->state_size);
    } else {
        ws->tmp_state = (real_t *)st_malloc(sizeof(real_t) * ws->state_size);
        if (ws->tmp_state == NULL) {
            ST_ERROR("Failed to st_malloc tmp_state");
            goto ERR;
        }
        memset(ws->tmp_state, 0, sizeof(real_t) * ws->state_size);
    }

    return ws;

ERR:
    safe_wildcard_state_destroy(ws);
    return NULL;
}

static int wildcard_state_summary(wildcard_state_t *ws)
{
    int n;
    int i;

    n = 0;
    if (ws->random_state != NULL) {
        for (i = 0; i < ws->state_size; i++) {
            ws->state[i] += ws->random_state[i];
        }
        ++n;
    }

    if (ws->sampling_state != NULL) {
        for (i = 0; i < ws->state_size; i++) {
            ws->state[i] += ws->sampling_state[i];
        }
        ++n;
    }

    if (ws->eval_state != NULL) {
        for (i = 0; i < ws->state_size; i++) {
            ws->state[i] += ws->eval_state[i];
        }
        ++n;
    }

    if (n > 1) {
        for (i = 0; i < ws->state_size; i++) {
            ws->state[i] /= n;
        }
    }

    return 0;
}

static int generate_random_state(wildcard_state_t *ws)
{
    ws->random_state = (real_t *)st_malloc(sizeof(real_t)*ws->state_size);
    if (ws->random_state == NULL) {
        ST_ERROR("Failed to st_malloc random_state");
        return -1;
    }
    memset(ws->random_state, 0, sizeof(real_t) * ws->state_size);

    if (updater_random_state(ws->updater, ws->random_state) < 0) {
        ST_ERROR("Failed to updater_random_state.");
        return -1;
    }

    return 0;
}

static int generate_sampling_state(wildcard_state_t *ws)
{
    int i, n;
    int word;
    count_t n_word;
    bool startover;

    ws->sampling_state = (real_t *)st_malloc(sizeof(real_t)*ws->state_size);
    if (ws->sampling_state == NULL) {
        ST_ERROR("Failed to st_malloc sampling_state");
        return -1;
    }
    memset(ws->sampling_state, 0, sizeof(real_t) * ws->state_size);

    n_word = 0;
    for (n = 0; n < ws->ws_opt.num_samplings; n++) {
        startover = true;
        word = -1;
        while (word != SENT_END_ID) {
            word = updater_sampling_state(ws->updater, ws->tmp_state,
                    ws->tmp_pre_ac_state, startover);
            if (word < 0) {
                ST_ERROR("Failed to updater_sampling_state.");
                return -1;
            }
            startover = false;

            if (ws->tmp_state != NULL) {
                for (i = 0; i < ws->state_size; i++) {
                    ws->sampling_state[i] += ws->tmp_state[i];
                }
            } else {
                for (i = 0; i < ws->state_size; i++) {
                    ws->sampling_state[i] += ws->tmp_pre_ac_state[i];
                }
            }
            ++n_word;
        }
    }

    for (i = 0; i < ws->state_size; i++) {
        ws->sampling_state[i] /= n_word;
    }

    if (ws->ws_opt.pre_activation) {
        if (updater_activate_state(ws->updater, ws->sampling_state)) {
            ST_ERROR("Failed to updater_activate_state.");
            return -1;
        }
    }

    return 0;
}

static int generate_eval_state(wildcard_state_t *ws)
{
    FILE *text_fp = NULL;

    count_t n_word;

    word_pool_t wp = {0};

    int word;
    int i;

    ws->eval_state = (real_t *)st_malloc(sizeof(real_t) * ws->state_size);
    if (ws->eval_state == NULL) {
        ST_ERROR("Failed to st_malloc eval_state");
        goto ERR;
    }
    memset(ws->eval_state, 0, sizeof(real_t) * ws->state_size);

    text_fp = st_fopen(ws->ws_opt.eval_text_file, "rb");
    if (text_fp == NULL) {
        ST_ERROR("Failed to open text file[%s]", ws->ws_opt.eval_text_file);
        goto ERR;
    }

    n_word = 0;
    while (! feof(text_fp)) {
        if (word_pool_read(&wp, NULL, 1, text_fp,
                    ws->connlm->vocab, NULL, true) < 0) {
            ST_ERROR("Failed to word_pool_read.");
            goto ERR;
        }

        if (updater_feed(ws->updater, wp.words, wp.size) < 0) {
            ST_ERROR("Failed to updater_feed.");
            goto ERR;
        }

        while (updater_steppable(ws->updater)) {
            word = updater_step_state(ws->updater, ws->tmp_state,
                    ws->tmp_pre_ac_state);
            if (word < 0) {
                ST_ERROR("Failed to updater_step_state.");
                goto ERR;
            }
            if (ws->tmp_state != NULL) {
                for (i = 0; i < ws->state_size; i++) {
                    ws->eval_state[i] += ws->tmp_state[i];
                }
            } else {
                for (i = 0; i < ws->state_size; i++) {
                    ws->eval_state[i] += ws->tmp_pre_ac_state[i];
                }
            }
            ++n_word;
        }
    }

    for (i = 0; i < ws->state_size; i++) {
        ws->eval_state[i] /= n_word;
    }

    word_pool_destroy(&wp);
    safe_fclose(text_fp);

    if (ws->ws_opt.pre_activation) {
        if (updater_activate_state(ws->updater, ws->eval_state)) {
            ST_ERROR("Failed to updater_activate_state.");
            goto ERR;
        }
    }

    return 0;

ERR:
    word_pool_destroy(&wp);
    safe_fclose(text_fp);
    return -1;
}

int wildcard_state_generate(wildcard_state_t *ws)
{
    ST_CHECK_PARAM(ws == NULL, -1);

    if (ws->state_size <= 0) {
        return 0;
    }

    if (ws->ws_opt.random) {
        if (generate_random_state(ws) < 0) {
            ST_ERROR("Failed to generate_random_state.");
            return -1;
        }
    }

    if (ws->ws_opt.num_samplings > 0) {
        if (generate_sampling_state(ws) < 0) {
            ST_ERROR("Failed to generate_sampling_state.");
            return -1;
        }
    }

    if (ws->ws_opt.eval_text_file[0] != '\0') {
        if (generate_eval_state(ws) < 0) {
            ST_ERROR("Failed to generate_eval_state.");
            return -1;
        }
    }

    if (wildcard_state_summary(ws) < 0) {
        ST_ERROR("Failed to wildcard_state_summary.");
        return -1;
    }

    return 0;
}

int wildcard_state_save(wildcard_state_t *ws, FILE *fp)
{
    ST_CHECK_PARAM(ws == NULL || fp == NULL, -1);

    fprintf(fp, "# State size: %d\n", ws->state_size);

    if (ws->state_size <= 0) {
        return 0;
    }

    if (ws->random_state != NULL) {
        if (print_vec(fp, ws->random_state, ws->state_size,
                    "# Random state") < 0) {
            ST_ERROR("Failed to print_vec for random_state.");
            return -1;
        }
    }

    if (ws->sampling_state != NULL) {
        if (print_vec(fp, ws->sampling_state, ws->state_size,
                    "# Sampling state") < 0) {
            ST_ERROR("Failed to print_vec for sampling_state.");
            return -1;
        }
    }

    if (ws->eval_state != NULL) {
        if (print_vec(fp, ws->eval_state, ws->state_size,
                    "# Eval state") < 0) {
            ST_ERROR("Failed to print_vec for eval_state.");
            return -1;
        }
    }

    if (print_vec(fp, ws->state, ws->state_size, NULL) < 0) {
        ST_ERROR("Failed to print_vec for state.");
        return -1;
    }

    return 0;
}
