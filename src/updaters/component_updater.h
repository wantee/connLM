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
#include "updaters/layer_updater.h"

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

    layer_updater_t **layer_updaters; /**< layer updaters. */
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
 * @param[in] connlm the connlm model.
 * @return comp_updater on success, otherwise NULL.
 */
comp_updater_t* comp_updater_create(component_t *comp);

/**
 * Setup comp_updater for running.
 * @ingroup g_updater_comp
 * @param[in] comp_updater comp_updater.
 * @param[in] backprob whether do backprob.
 * @return non-zero value if any error.
 */
int comp_updater_setup(comp_updater_t *comp_updater, bool backprob);

/**
 * Step one word for a comp_updater.
 * @ingroup g_updater_comp
 * @param[in] comp_updater comp_updater.
 * @return non-zero value if any error.
 */
int comp_updater_step(comp_updater_t *comp_updater);

#ifdef __cplusplus
}
#endif

#endif
