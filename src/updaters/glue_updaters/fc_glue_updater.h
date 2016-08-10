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

#ifndef  _CONNLM_FC_GLUE_UPDATER_H_
#define  _CONNLM_FC_GLUE_UPDATER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <connlm/config.h>

#include "../../glues/fc_glue.h"
#include "glue_updater.h"

/** @defgroup g_glue_updater_wt full-connected glue_updater.
 * @ingroup g_updater
 * Data structures and functions for fc glue_updater.
 */

/**
 * Initialize a fc glue_updater.
 * @ingroup g_glue_updater_wt
 * @param[in] glue_updater fc glue_updater to be initialized.
 * @return non-zero if any error
 */
int fc_glue_updater_init(glue_updater_t *glue_updater);

/**
 * Feed-forward one word for a fc_glue_updater.
 * @ingroup g_glue_updater_fc
 * @param[in] glue_updater glue_updater.
 * @param[in] comp_updater the comp_updater.
 * @param[in, unused] words input words buffer.
 * @param[in, unused] n_word length of words.
 * @param[in, unused] tgt_pos position of target word in words buffer.
 * @return non-zero value if any error.
 */
int fc_glue_updater_forward(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, int *words, int n_word, int tgt_pos);

/**
 * Back-prop one word for a fc_glue_updater.
 * @ingroup g_glue_updater_fc
 * @param[in] glue_updater glue_updater.
 * @param[in] comp_updater the comp_updater.
 * @param[in, unused] words input words buffer.
 * @param[in, unused] n_word length of words.
 * @param[in, unused] tgt_pos position of target word in words buffer.
 * @return non-zero value if any error.
 */
int fc_glue_updater_backprop(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, int *words, int n_word, int tgt_pos);

#ifdef __cplusplus
}
#endif

#endif
