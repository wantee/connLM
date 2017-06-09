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
#include "../../glues/glue.h"
#include "glue_updater.h"

/** @defgroup g_glue_updater_direct Updater for Direct Glue
 * @ingroup g_updater_glue
 * Perform forward and backprop on a direct glue.
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
 * @param[in] input_sent input sentence buffer.
 * @param[in] in_ac [unused] activation of in layer.
 * @param[out] out_ac activation of out layer.
 * @return non-zero value if any error.
 */
int direct_glue_updater_forward(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, sent_t *input_sent,
        real_t *in_ac, real_t *out_ac);

/**
 * Back-prop one word for a direct_glue_updater.
 * @ingroup g_glue_updater_direct
 * @param[in] glue_updater glue_updater.
 * @param[in] comp_updater the comp_updater.
 * @param[in] input_sent input sentence buffer.
 * @param[in] in_ac [unused] activation of in layer.
 * @param[in] out_er error of out layer.
 * @param[out] in_er [unused] error of in layer.
 * @return non-zero value if any error.
 */
int direct_glue_updater_backprop(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, sent_t *input_sent,
        real_t *in_ac, real_t *out_er, real_t *in_er);

/**
 * Feed-forward util output layer.
 * @ingroup g_glue_updater_direct
 * @param[in] glue_updater the glue_updater.
 * @param[in] comp_updater the comp_updater.
 * @param[in] input_sent input sentence buffer.
 * @param[in] in_ac [unused] activation of in layer.
 * @param[in] out_ac [unused] activation of out layer.
 * @return non-zero value if any error.
 */
int direct_glue_updater_forward_util_out(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, sent_t *input_sent,
        real_t *in_ac, real_t *out_ac);

/**
 * Feed-forward one node of output layer.
 * @ingroup g_glue_updater_direct
 * @param[in] glue_updater the glue_updater.
 * @param[in] comp_updater the comp_updater.
 * @param[in] node node of output tree.
 * @param[in] in_ac [unused] activation of in layer.
 * @param[out] out_ac activation of out layer.
 * @return non-zero value if any error.
 */
int direct_glue_updater_forward_out(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, output_node_id_t node,
        real_t *in_ac, real_t *out_ac);

/**
 * Feed-forward one word of output layer.
 * @ingroup g_glue_updater_direct
 * @param[in] glue_updater the glue_updater.
 * @param[in] comp_updater the comp_updater.
 * @param[in] word the word.
 * @param[in] in_ac [unused] activation of in layer.
 * @param[out] out_ac activation of out layer.
 * @return non-zero value if any error.
 */
int direct_glue_updater_forward_out_word(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, int word,
        real_t *in_ac, real_t *out_ac);

/**
 * Generate keep mask for direct_glue_updater.
 * @ingroup g_glue_updater_direct
 * @param[in] glue_updater the glue_updater.
 * @return non-zero value if any error.
 */
int direct_glue_updater_gen_keep_mask(glue_updater_t *glue_updater);

#ifdef __cplusplus
}
#endif

#endif
