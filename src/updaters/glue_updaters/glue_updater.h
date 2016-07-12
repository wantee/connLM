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

/** @defgroup g_updater_glue Updater for glue.
 * @ingroup g_updater
 * Data structures and functions for glue updater.
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

    int (*forward)(glue_updater_t *glue_updater, comp_updater_t *comp_updater,
            int *words, int n_word, int tgt_pos); /**< forward glue updater.*/

    int (*backprob)(glue_updater_t *glue_updater, comp_updater_t *comp_updater,
            int *words, int n_word, int tgt_pos); /**< backprob glue updater.*/
} glue_updater_impl_t;

/**
 * Glue updater.
 * @ingroup g_updater_glue
 */
typedef struct _glue_updater_t_ {
    glue_t *glue; /**< the glue. */

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
        safe_free(ptr);\
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
 * @param[in] connlm the connlm model.
 * @return glue_updater on success, otherwise NULL.
 */
glue_updater_t* glue_updater_create(glue_t *glue);

/**
 * Setup glue_updater for running.
 * @ingroup g_updater_glue
 * @param[in] glue_updater glue_updater.
 * @param[in] comp_updater the comp_updater.
 * @param[in] backprob whether do backprob.
 * @return non-zero value if any error.
 */
int glue_updater_setup(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, bool backprob);

/**
 * Feed-forward one word for a glue_updater.
 * @ingroup g_updater_glue
 * @param[in] glue_updater the glue_updater.
 * @param[in] comp_updater the comp_updater.
 * @param[in] words input words buffer.
 * @param[in] n_word length of words.
 * @param[in] tgt_pos position of target word in words buffer.
 * @see glue_updater_backprop
 * @return non-zero value if any error.
 */
int glue_updater_forward(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, int *words, int n_word, int tgt_pos);

/**
 * Back-propagate one word for a glue_updater.
 * @ingroup g_updater_glue
 * @param[in] glue_updater the glue_updater.
 * @param[in] comp_updater the comp_updater.
 * @param[in] words input words buffer.
 * @param[in] n_word length of words.
 * @param[in] tgt_pos position of target word in words buffer.
 * @see glue_updater_forward
 * @return non-zero value if any error.
 */
int glue_updater_backprop(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, int *words, int n_word, int tgt_pos);

#ifdef __cplusplus
}
#endif

#endif
