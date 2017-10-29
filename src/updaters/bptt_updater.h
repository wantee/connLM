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

#ifndef  _CONNLM_BPTT_UPDATER_H_
#define  _CONNLM_BPTT_UPDATER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <connlm/config.h>

#include "component.h"
#include "updaters/glue_updaters/glue_updater.h"

/** @defgroup g_updater_bptt Updater for BPTT
 * @ingroup g_updater
 * Perform BPTT training on recurrent glues.
 * Every cycle in component has a bptt updater.
 */

/**
 * BPTT updater.
 * @ingroup g_updater_bptt
 */
typedef struct _bptt_updater_t_ {
    bptt_opt_t bptt_opt; /**< bptt option. */

    wt_updater_t **wt_updaters; /**< wt_updater for every glue. */
    mat_t *in_acs; /**< buffer of in_ac. */
    mat_t *out_ers; /**< buffer of out_er. */
    int num_glue; /**< number of glue in this cycle. */
    mat_t *ac_bptts; /**< buffer of activation for BPTT in cycle. one-based. */
    mat_t *er_bptts; /**< buffer of error for BPTT in cycle. one-based.*/
    ivec_t *cutoffs; /**< cut-off batch id for BPTT of every time steps. */
    mat_t *cutoff_acs; /**< backup acs for cut-off time step, since they will reset after layer_reset. */
    int num_bptts; /**< number time stpes filled in ac_bptt and er_bptt. */
} bptt_updater_t;

/**
 * Destroy a bptt_updater and set the pointer to NULL.
 * @ingroup g_updater_bptt
 * @param[in] ptr pointer to updater_t.
 */
#define safe_bptt_updater_destroy(ptr) do {\
    if((ptr) != NULL) {\
        bptt_updater_destroy(ptr);\
        safe_st_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a bptt_updater.
 * @ingroup g_updater_bptt
 * @param[in] bptt_updater bptt_updater to be destroyed.
 */
void bptt_updater_destroy(bptt_updater_t *bptt_updater);

/**
 * Create a bptt_updater.
 * @ingroup g_updater_bptt
 * @param[in] comp the component including all glue_cycles.
 * @param[in] cycle_id the id in glue_cycles.
 * @param[in] glue_updaters glue updaters.
 * @return bptt_updater on success, otherwise NULL.
 */
bptt_updater_t* bptt_updater_create(component_t *comp, int cycle_id,
        glue_updater_t **glue_updaters);

/**
 * Reset a bptt_updater.
 * @ingroup g_updater_bptt
 * @param[in] bptt_updater the bptt_updater.
 * @param[in] batch_i index in batch to be reset.
 * @param[in] in_ac ac buffer of input layer.
 * @return non-zero value if any error.
 */
int bptt_updater_reset(bptt_updater_t *bptt_updater,
        int batch_i, mat_t *in_ac);

/**
 * Clear a bptt_updater.
 * @ingroup g_updater_bptt
 * @param[in] bptt_updater the bptt_updater.
 * @return non-zero value if any error.
 */
int bptt_updater_clear(bptt_updater_t *bptt_updater);

#ifdef __cplusplus
}
#endif

#endif
