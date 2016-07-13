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

#ifndef  _CONNLM_COMP_UPDATER_H_
#define  _CONNLM_COMP_UPDATER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <connlm/config.h>

#include "component.h"
#include "updaters/output_updater.h"
#include "updaters/layer_updater.h"
#include "updaters/glue_updaters/glue_updater.h"

/** @defgroup g_updater_comp Updater for component.
 * @ingroup g_updater
 * Data structures and functions for component updater.
 */

/**
 * Component updater.
 * @ingroup g_updater_comp
 */
typedef struct _component_updater_t_ {
    component_t *comp; /**< the component. */
    out_updater_t *out_updater; /**< the output updater. */

    layer_updater_t **layer_updaters; /**< layer updaters. */
    glue_updater_t **glue_updaters; /**< glue updaters. */
} comp_updater_t;

/**
 * Destroy a comp_updater and set the pointer to NULL.
 * @ingroup g_updater_comp
 * @param[in] ptr pointer to updater_t.
 */
#define safe_comp_updater_destroy(ptr) do {\
    if((ptr) != NULL) {\
        comp_updater_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a comp_updater.
 * @ingroup g_updater_comp
 * @param[in] comp_updater comp_updater to be destroyed.
 */
void comp_updater_destroy(comp_updater_t *comp_updater);

/**
 * Create a comp_updater.
 * @ingroup g_updater_comp
 * @param[in] comp the component.
 * @return comp_updater on success, otherwise NULL.
 */
comp_updater_t* comp_updater_create(component_t *comp,
        out_updater_t *out_updater);

/**
 * Setup comp_updater for running.
 * @ingroup g_updater_comp
 * @param[in] comp_updater comp_updater.
 * @param[in] backprop whether do backprop.
 * @return non-zero value if any error.
 */
int comp_updater_setup(comp_updater_t *comp_updater, bool backprop);

/**
 * Reset running for comp_updater.
 * Called before every input sentence to be performed.
 * @ingroup g_updater_comp
 * @param[in] comp_updater comp_updater.
 * @return non-zero value if any error.
 */
int comp_updater_reset(comp_updater_t *comp_updater);

/**
 * Start running for comp_updater.
 * Called before every input word to be performed.
 * @ingroup g_updater_comp
 * @param[in] comp_updater comp_updater.
 * @return non-zero value if any error.
 */
int comp_updater_start(comp_updater_t *comp_updater);

/**
 * Feed-forward one word for a comp_updater.
 * @ingroup g_updater_comp
 * @param[in] comp_updater the comp_updater.
 * @param[in] words input words buffer.
 * @param[in] n_word length of words.
 * @param[in] tgt_pos position of target word in words buffer.
 * @see comp_updater_backprop
 * @return non-zero value if any error.
 */
int comp_updater_forward(comp_updater_t *comp_updater, int *words,
        int n_word, int tgt_pos);

/**
 * Back-propagate one word for a comp_updater.
 * @ingroup g_updater_comp
 * @param[in] comp_updater the comp_updater.
 * @param[in] words input words buffer.
 * @param[in] n_word length of words.
 * @param[in] tgt_pos position of target word in words buffer.
 * @see comp_updater_forward
 * @return non-zero value if any error.
 */
int comp_updater_backprop(comp_updater_t *comp_updater, int *words,
        int n_word, int tgt_pos);

/**
 * End running for comp_updater.
 * Called after every input word performed.
 * @ingroup g_updater_comp
 * @param[in] comp_updater comp_updater.
 * @return non-zero value if any error.
 */
int comp_updater_end(comp_updater_t *comp_updater);

/**
 * Finish running for comp_updater.
 * Called after all words performed.
 * @ingroup g_updater_comp
 * @param[in] comp_updater comp_updater.
 * @return non-zero value if any error.
 */
int comp_updater_finish(comp_updater_t *comp_updater);

#ifdef __cplusplus
}
#endif

#endif
