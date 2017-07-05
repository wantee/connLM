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

/** @defgroup g_glue_updater_fc Updater for Full-Connected Glue
 * @ingroup g_updater_glue
 * Perform forward and backprop on a full-connected glue.
 */

/**
 * Feed-forward one word for a fc_glue_updater.
 * @ingroup g_glue_updater_fc
 * @param[in] glue_updater glue_updater.
 * @param[in] comp_updater the comp_updater.
 * @param[in] batch [unused] input batch.
 * @param[in] in_ac activation of in layer.
 * @param[out] out_ac activation of out layer.
 * @return non-zero value if any error.
 */
int fc_glue_updater_forward(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, egs_batch_t *batch,
        real_t *in_ac, real_t *out_ac);

/**
 * Back-prop one word for a fc_glue_updater.
 * @ingroup g_glue_updater_fc
 * @param[in] glue_updater glue_updater.
 * @param[in] comp_updater the comp_updater.
 * @param[in] batch [unused] input batch.
 * @param[in] in_ac activation of in layer.
 * @param[in] out_er error of out layer.
 * @param[out] in_er error of in layer.
 * @return non-zero value if any error.
 */
int fc_glue_updater_backprop(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, egs_batch_t *batch,
        real_t *in_ac, real_t *out_er, real_t *in_er);

#ifdef __cplusplus
}
#endif

#endif
