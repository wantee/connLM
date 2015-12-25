/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Wang Jian
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

#ifndef  _CONNLM_WEIGHT_H_
#define  _CONNLM_WEIGHT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <connlm/config.h>

/** @defgroup g_weight NNet weight. 
 * Data structures and functions for NNet weight.
 */

/**
 * NNet weight.
 * @ingroup g_weight
 */
typedef struct _weight_t_ {
    char name[MAX_NAME_LEN]; /**< weight name. */
    int id; /**< weight ID. */

    /** forward function. */
    int (*forward)(struct _weight_t_ *wt);
    /** backprop function. */
    int (*backprop)(struct _weight_t_ *wt);
} weight_t;

/**
 * Destroy a weight and set the pointer to NULL.
 * @ingroup g_weight
 * @param[in] ptr pointer to weight_t.
 */
#define safe_wt_destroy(ptr) do {\
    if((ptr) != NULL) {\
        wt_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a weight.
 * @ingroup g_weight
 * @param[in] wt weight to be destroyed.
 */
void wt_destroy(weight_t* wt);

/**
 * Duplicate a weight.
 * @ingroup g_weight
 * @param[in] w weight to be duplicated.
 * @return the duplicated weight. 
 */
weight_t* wt_dup(weight_t *w);

/**
 * Parse a topo config line, and return a new weight. 
 * @ingroup g_weight
 * @param[in] line topo config line.
 * @return a new weight or NULL if error.
 */
weight_t* wt_parse_topo(const char *line);

#ifdef __cplusplus
}
#endif

#endif
