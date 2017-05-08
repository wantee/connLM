/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Wang Jian
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

#ifndef  _CONNLM_WILDCARD_STATE_H_
#define  _CONNLM_WILDCARD_STATE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#include <connlm/config.h>

#include "connlm.h"
#include "updaters/updater.h"

/** @defgroup g_wildcard_state Wildcard state for a connLM model
 * Generate and manipulate wildcard state for a connLM model
 */

/**
 * Options for wildcard_state.
 * @ingroup g_wildcard_state
 */
typedef struct _wildcard_state_opt_t_ {
    bool pre_activation; /**< whether averaging values before activation. */
    bool random; /**< whether generateing random states. */
    int num_samplings; /**< number of sentences to be sampled. */
    char eval_text_file[MAX_DIR_LEN]; /**< file contains sentences to be evaluated. */
} wildcard_state_opt_t;

/**
 * Load wildcard state option.
 * @ingroup g_wildcard_state
 * @param[out] ws_opt options to be loaded.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @return non-zero value if any error.
 */
int wildcard_state_load_opt(wildcard_state_opt_t *ws_opt,
        st_opt_t *opt, const char *sec_name);

/**
 * Wildcard state
 * @ingroup g_wildcard_state
 */
typedef struct _wildcard_state_t_ {
    wildcard_state_opt_t ws_opt; /**< options. */

    connlm_t *connlm; /**< the connLM model. */
    updater_t *updater; /**< the updater. */

    int state_size; /**< size of state. */
    real_t *random_state; /**< state generated by random. */
    real_t *sampling_state; /**< state generated by sampling. */
    real_t *eval_state; /**< state generated by evaluating. */

    real_t *state; /**< final wildcard state. */

    real_t *tmp_state; /**< temp buffer of state. */
    real_t *tmp_pre_ac_state; /**< temp buffer of pre-activation state. */
} wildcard_state_t;

/**
 * Destroy a wildcard state and set the pointer to NULL.
 * @ingroup g_wildcard_state
 * @param[in] ptr pointer to wildcard_state_t.
 */
#define safe_wildcard_state_destroy(ptr) do {\
    if((ptr) != NULL) {\
        wildcard_state_destroy(ptr);\
        safe_st_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a wildcard state.
 * @ingroup g_wildcard_state
 * @param[in] ws wildcard state to be destroyed.
 */
void wildcard_state_destroy(wildcard_state_t *ws);

/**
 * Create a wildcard state.
 * @ingroup g_wildcard_state
 * @param[in] connlm the connlm model.
 * @param[in] ws_opt options for wildcard state.
 * @return wildcard_state on success, otherwise NULL.
 */
wildcard_state_t* wildcard_state_create(connlm_t *connlm,
        wildcard_state_opt_t *ws_opt);

/**
 * Generate wildcard state.
 * @ingroup g_wildcard_state
 * @param[in] ws the wildcard state.
 * @return non-zero value if any error.
 */
int wildcard_state_generate(wildcard_state_t *ws);

/**
 * Generate wildcard state.
 * @ingroup g_wildcard_state
 * @param[in] ws the wildcard state.
 * @param[in] fp file stream to be written into.
 * @return non-zero value if any error.
 */
int wildcard_state_save(wildcard_state_t *ws, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif
