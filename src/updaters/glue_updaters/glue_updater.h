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

#ifndef  _CONNLM_GLUE_UPDATER_H_
#define  _CONNLM_GLUE_UPDATER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <connlm/config.h>

#include "glues/glue.h"
#include "updaters/input_updater.h"
#include "updaters/wt_updater.h"

/** @defgroup g_updater_glue Updater for Glue
 * @ingroup g_updater
 * Perform forward and backprop on a glue.
 */

typedef struct _glue_updater_t_ glue_updater_t;
/**
 * Implementation of a NNet glue updater.
 * @ingroup g_glue
 */
typedef struct _glue_updater_implementation_t_ {
    char type[MAX_NAME_LEN]; /**< type of glue updater.*/

    int (*init)(glue_updater_t *glue_updater); /**< init glue updater.*/

    void (*destroy)(glue_updater_t *glue_updater); /**< destroy glue updater.*/

    int (*setup)(glue_updater_t *glue_updater, comp_updater_t *comp_updater,
            bool backprop); /**< setup glue updater.*/

    int (*forward)(glue_updater_t *glue_updater,
            comp_updater_t *comp_updater, sent_t *input_sent,
            real_t *in_ac, real_t *out_ac); /**< forward glue updater.*/

    int (*backprop)(glue_updater_t *glue_updater,
            comp_updater_t *comp_updater,
            sent_t *input_sent, real_t *in_ac, real_t *out_er,
            real_t *in_er); /**< backprop glue updater.*/

    int (*forward_util_out)(glue_updater_t *glue_updater,
            comp_updater_t *comp_updater, sent_t *input_sent,
            real_t *in_ac, real_t *out_ac); /**< forward_util_out glue updater.*/

    int (*forward_out)(glue_updater_t *glue_updater,
            comp_updater_t *comp_updater, output_node_id_t node,
            real_t *in_ac, real_t *out_ac); /**< forward_out glue updater.*/

    int (*forward_out_word)(glue_updater_t *glue_updater,
            comp_updater_t *comp_updater, int word,
            real_t *in_ac, real_t *out_ac); /**< forward_out_word glue updater.*/

    bool multicall; /**< whether need multi-call of forword_out_word.*/
} glue_updater_impl_t;

/**
 * Glue updater.
 * @ingroup g_updater_glue
 */
typedef struct _glue_updater_t_ {
    glue_t *glue; /**< the glue. */

    real_t keep_prob; /**< keep probability, i.e., 1 - dropout probability. */
    bool *keep_mask; /**< keep mask. */
    unsigned int *rand_seed; /**< rand seed. */
    real_t *in_ac; /**< transformed activation after dropout. */

    wt_updater_t *wt_updater; /**< the wt_updater. */
    glue_updater_impl_t *impl; /**< implementation for glue. */
    bool *forwarded; /**< whether a node is forwarded already. For support of multi-call. */
    void *extra; /**< hook to store extra data. */
} glue_updater_t;

/**
 * Destroy a glue_updater and set the pointer to NULL.
 * @ingroup g_updater_glue
 * @param[in] ptr pointer to updater_t.
 */
#define safe_glue_updater_destroy(ptr) do {\
    if((ptr) != NULL) {\
        glue_updater_destroy(ptr);\
        safe_st_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a glue_updater.
 * @ingroup g_updater_glue
 * @param[in] glue_updater glue_updater to be destroyed.
 */
void glue_updater_destroy(glue_updater_t *glue_updater);

/**
 * Create a glue_updater.
 * @ingroup g_updater_glue
 * @param[in] glue the glue.
 * @return glue_updater on success, otherwise NULL.
 */
glue_updater_t* glue_updater_create(glue_t *glue);

/**
 * Set rand seed for glue_updater.
 * @ingroup g_updater_glue
 * @param[in] glue_updater glue_updater.
 * @param[in] seed the rand seed pointer.
 * @return non-zero value if any error.
 */
int glue_updater_set_rand_seed(glue_updater_t *glue_updater, unsigned int *seed);

/**
 * Setup glue_updater for running.
 * @ingroup g_updater_glue
 * @param[in] glue_updater glue_updater.
 * @param[in] comp_updater the comp_updater.
 * @param[in] backprop whether do backprop.
 * @return non-zero value if any error.
 */
int glue_updater_setup(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, bool backprop);

/**
 * Setup pre-activation state of glue_updater for running.
 * @ingroup g_updater_glue
 * @param[in] glue_updater glue_updater.
 * @param[in] comp_updater the comp_updater.
 * @return non-zero value if any error.
 */
int glue_updater_setup_pre_ac_state(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater);

/**
 * Reset a glue_updater.
 * @ingroup g_updater_glue
 * @param[in] glue_updater the glue_updater.
 * @return non-zero value if any error.
 */
int glue_updater_reset(glue_updater_t *glue_updater);

/**
 * Feed-forward one word for a glue_updater.
 * @ingroup g_updater_glue
 * @param[in] glue_updater the glue_updater.
 * @param[in] comp_updater the comp_updater.
 * @param[in] input_sent input sentence buffer.
 * @see glue_updater_backprop
 * @return non-zero value if any error.
 */
int glue_updater_forward(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, sent_t *input_sent);

/**
 * Back-propagate one word for a glue_updater.
 * @ingroup g_updater_glue
 * @param[in] glue_updater the glue_updater.
 * @param[in] comp_updater the comp_updater.
 * @param[in] input_sent input sentence buffer.
 * @see glue_updater_forward
 * @return non-zero value if any error.
 */
int glue_updater_backprop(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, sent_t *input_sent);

/**
 * Finish running for glue_updater.
 * Called after all words performed.
 * @ingroup g_updater_glue
 * @param[in] glue_updater glue_updater.
 * @return non-zero value if any error.
 */
int glue_updater_finish(glue_updater_t *glue_updater);

/**
 * Feed-forward util output layer.
 * @ingroup g_updater_glue
 * @param[in] glue_updater the glue_updater.
 * @param[in] comp_updater the comp_updater.
 * @param[in] input_sent input sentence buffer.
 * @return non-zero value if any error.
 */
int glue_updater_forward_util_out(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, sent_t *input_sent);

/**
 * Feed-forward one node of output layer.
 * @ingroup g_updater_glue
 * @param[in] glue_updater the glue_updater.
 * @param[in] comp_updater the comp_updater.
 * @param[in] node node of output tree.
 * @return non-zero value if any error.
 */
int glue_updater_forward_out(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, output_node_id_t node);

/**
 * Feed-forward one word of output layer.
 * @ingroup g_updater_glue
 * @param[in] glue_updater the glue_updater.
 * @param[in] comp_updater the comp_updater.
 * @param[in] word the word.
 * @return non-zero value if any error.
 */
int glue_updater_forward_out_word(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, int word);

/**
 * Initialize multi-call of forward_out_word.
 * @ingroup g_updater_glue
 * @param[in] glue_updater glue_updater.
 * @param[in] output the output layer.
 * @return non-zero value if any error.
 */
int glue_updater_init_multicall(glue_updater_t *glue_updater,
        output_t *output);

/**
 * Clear multi-call of forward_out_word.
 * @ingroup g_updater_glue
 * @param[in] glue_updater glue_updater.
 * @param[in] output the output layer.
 * @return non-zero value if any error.
 */
int glue_updater_clear_multicall(glue_updater_t *glue_updater,
        output_t *output);

#ifdef __cplusplus
}
#endif

#endif
