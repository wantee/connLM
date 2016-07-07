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

#ifndef  _CONNLM_OUTPUT_UPDATER_H_
#define  _CONNLM_OUTPUT_UPDATER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <connlm/config.h>

#include "output.h"

/** @defgroup g_updater_output Updater for output layer.
 * @ingroup g_updater
 * Data structures and functions for output layer updater.
 */

/**
 * Output layer updater.
 * @ingroup g_updater_output
 */
typedef struct _output_updater_t_ {
    output_t *output; /**< the output layer. */

    real_t *ac; /**< activation of output layer. */
    real_t *er; /**< error of output layer. */
} out_updater_t;

/**
 * Destroy a out_updater and set the pointer to NULL.
 * @ingroup g_updater_output
 * @param[in] ptr pointer to updater_t.
 */
#define safe_out_updater_destroy(ptr) do {\
    if((ptr) != NULL) {\
        out_updater_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a out_updater.
 * @ingroup g_updater_output
 * @param[in] out_updater out_updater to be destroyed.
 */
void out_updater_destroy(out_updater_t *out_updater);

/**
 * Create a out_updater.
 * @ingroup g_updater_output
 * @param[in] connlm the connlm model.
 * @return out_updater on success, otherwise NULL.
 */
out_updater_t* out_updater_create(output_t *output);

/**
 * Setup out_updater for running.
 * @ingroup g_updater_output
 * @param[in] out_updater out_updater.
 * @param[in] backprob whether do backprob.
 * @return non-zero value if any error.
 */
int out_updater_setup(out_updater_t *out_updater, bool backprob);

/**
 * Step one word for a out_updater.
 * @ingroup g_updater_out
 * @param[in] out_updater out_updater.
 * @return non-zero value if any error.
 */
int out_updater_step(out_updater_t *out_updater);

#ifdef __cplusplus
}
#endif

#endif
