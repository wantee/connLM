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

#ifndef  _CONNLM_DIRECT_GLUE_UPDATER_H_
#define  _CONNLM_DIRECT_GLUE_UPDATER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <connlm/config.h>

#include "../../glues/glue.h"
#include "glue_updater.h"

/** @defgroup g_glue_updater_direct direct glue_updater.
 * @ingroup g_updater
 * Data structures and functions for direct glue_updater.
 */

/**
 * Destroy a direct glue_updater.
 * @ingroup g_glue_updater_direct
 * @param[in] glue_updater direct glue_updater to be destroyed.
 */
void direct_glue_updater_destroy(glue_updater_t* glue_updater);

/**
 * Initialize a direct glue_updater.
 * @ingroup g_glue_updater_direct
 * @param[in] glue_updater direct glue_updater to be initialized.
 * @return non-zero if any error
 */
int direct_glue_updater_init(glue_updater_t *glue_updater);

/**
 * Setup direct_glue_updater for running.
 * @ingroup g_glue_updater_direct
 * @param[in] glue_updater glue_updater.
 * @param[in] comp_updater the comp_updater.
 * @param[in] backprop whether do backprop.
 * @return non-zero value if any error.
 */
int direct_glue_updater_setup(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, bool backprop);

/**
 * Feed-forward one word for a direct_glue_updater.
 * @ingroup g_glue_updater_direct
 * @param[in] glue_updater glue_updater.
 * @param[in] comp_updater the comp_updater.
 * @param[in] words input words buffer.
 * @param[in] n_word length of words.
 * @param[in] tgt_pos position of target word in words buffer.
 * @return non-zero value if any error.
 */
int direct_glue_updater_forward(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, int *words, int n_word, int tgt_pos);

/**
 * Back-prop one word for a direct_glue_updater.
 * @ingroup g_glue_updater_direct
 * @param[in] glue_updater glue_updater.
 * @param[in] n_step updating step.
 * @param[in] comp_updater the comp_updater.
 * @param[in] words input words buffer.
 * @param[in] n_word length of words.
 * @param[in] tgt_pos position of target word in words buffer.
 * @return non-zero value if any error.
 */
int direct_glue_updater_backprop(glue_updater_t *glue_updater, count_t n_step,
        comp_updater_t *comp_updater, int *words, int n_word, int tgt_pos);

#ifdef __cplusplus
}
#endif

#endif
