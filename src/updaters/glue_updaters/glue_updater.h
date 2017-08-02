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
            comp_updater_t *comp_updater, egs_batch_t *batch,
            mat_t *in_ac, mat_t *out_ac); /**< forward glue updater.*/

    int (*backprop)(glue_updater_t *glue_updater,
            comp_updater_t *comp_updater,
            egs_batch_t *batch, mat_t *in_ac, mat_t *out_er,
            mat_t *in_er); /**< backprop glue updater.*/

    int (*forward_util_out)(glue_updater_t *glue_updater,
            comp_updater_t *comp_updater, egs_batch_t *batch,
            mat_t *in_ac, mat_t *out_ac); /**< forward_util_out glue updater.*/

    int (*forward_out)(glue_updater_t *glue_updater,
            comp_updater_t *comp_updater, output_node_id_t node,
            mat_t *in_ac, mat_t *out_ac); /**< forward_out glue updater.*/

    int (*forward_out_word)(glue_updater_t *glue_updater,
            comp_updater_t *comp_updater, ivec_t *words,
            mat_t *in_ac, mat_t *out_ac); /**< forward_out_word glue updater.*/

    int (*gen_keep_mask)(glue_updater_t *glue_updater,
            int batch_size); /**< gen_keep_mask glue updater.*/

    bool multicall; /**< whether need multi-call of forword_out_word.*/
} glue_updater_impl_t;

/**
 * Glue updater.
 * @ingroup g_updater_glue
 */
typedef struct _glue_updater_t_ {
    glue_t *glue; /**< the glue. */

    unsigned int *rand_seed; /**< random seed. */
    real_t keep_prob; /**< keep probability, i.e., 1 - dropout probability. */
    mat_t keep_mask; /**< keep mask. */
    mat_t dropout_val; /**< transformed activation/error after dropout. */

    wt_updater_t *wt_updater; /**< the wt_updater. */
    glue_updater_impl_t *impl; /**< implementation for glue. */
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
 * Setup dropout for glue_updater.
 * @ingroup g_updater_glue
 * @param[in] glue_updater glue_updater.
 * @param[in] dropout dropout probability
 * @return non-zero value if any error.
 */
int glue_updater_setup_dropout(glue_updater_t *glue_updater, real_t dropout);

/**
 * Generate keep mask of dropout for glue_updater.
 * @ingroup g_updater_glue
 * @param[in] glue_updater glue_updater.
 * @param[in] batch_size batch size.
 * @return non-zero value if any error.
 */
int glue_updater_gen_keep_mask(glue_updater_t *glue_updater, int batch_size);

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
 * Set rand seed for glue_updater.
 * @ingroup g_updater_glue
 * @param[in] glue_updater glue_updater.
 * @param[in] seed the rand seed pointer.
 * @return non-zero value if any error.
 */
int glue_updater_set_rand_seed(glue_updater_t *glue_updater, unsigned int *seed);

/**
 * Feed-forward a batch for a glue_updater.
 * @ingroup g_updater_glue
 * @param[in] glue_updater the glue_updater.
 * @param[in] comp_updater the comp_updater.
 * @param[in] batch egs batch.
 * @see glue_updater_backprop
 * @return non-zero value if any error.
 */
int glue_updater_forward(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, egs_batch_t *batch);

/**
 * Back-propagate a batch for a glue_updater.
 * @ingroup g_updater_glue
 * @param[in] glue_updater the glue_updater.
 * @param[in] comp_updater the comp_updater.
 * @param[in] batch egs batch.
 * @see glue_updater_forward
 * @return non-zero value if any error.
 */
int glue_updater_backprop(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, egs_batch_t *batch);

/**
 * Feed-forward util output layer.
 * @ingroup g_updater_glue
 * @param[in] glue_updater the glue_updater.
 * @param[in] comp_updater the comp_updater.
 * @param[in] batch egs batch.
 * @return non-zero value if any error.
 */
int glue_updater_forward_util_out(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, egs_batch_t *batch);

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
 * Feed-forward batch of words of output layer.
 * @ingroup g_updater_glue
 * @param[in] glue_updater the glue_updater.
 * @param[in] comp_updater the comp_updater.
 * @param[in] words the words.
 * @return non-zero value if any error.
 */
int glue_updater_forward_out_word(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, ivec_t *words);

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

/**
 * Propagate error to previous layer.
 * @ingroup g_updater
 * @param[in] in_er error of input layer.
 * @param[in] wt weight matrix.
 * @param[in] scale the scale.
 * @param[in] er_cutoff cutoff of error.
 * @param[out] out_er error of output layer.
 * @return word for this step, -1 if any error.
 */
int propagate_error(mat_t *wt, mat_t *in_er,
        real_t scale, real_t er_cutoff, mat_t *out_er);

#ifdef __cplusplus
}
#endif

#endif
