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

#ifndef  _CONNLM_UPDATER_H_
#define  _CONNLM_UPDATER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>

#include <stutils/st_semaphore.h>

#include <connlm/config.h>

#include "connlm.h"

/** @defgroup g_updater Updater to update connlm parameter.
 * Data structure and functions for updater.
 */

/**
 * Updater.
 * @ingroup g_updater
 */
typedef struct _updater_t_ {
    connlm_t *connlm; /**< the model. */
} updater_t;

/**
 * Destroy a updater and set the pointer to NULL.
 * @ingroup g_updater
 * @param[in] ptr pointer to updater_t.
 */
#define safe_updater_destroy(ptr) do {\
    if((ptr) != NULL) {\
        updater_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a updater.
 * @ingroup g_updater
 * @param[in] updater updater to be destroyed.
 */
void updater_destroy(updater_t *updater);

/**
 * Create a updater.
 * @ingroup g_updater
 * @param[in] connlm the connlm model.
 * @return updater on success, otherwise NULL.
 */
updater_t* updater_create(connlm_t *connlm);

/**
 * Setup updater for running.
 * @ingroup g_updater
 * @param[in] updater updater.
 * @param[in] backprob whether do backprob.
 * @return non-zero value if any error.
 */
int updater_setup(updater_t *updater, bool backprob);

#ifdef __cplusplus
}
#endif

#endif
