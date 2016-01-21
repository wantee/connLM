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

#ifndef  _CONNLM_INPUT_H_
#define  _CONNLM_INPUT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stutils/st_opt.h>

#include <connlm/config.h>

/** @defgroup g_input Input Layer
 * Data structures and functions for Input Layer.
 */

/**
 * Input Layer.
 * @ingroup g_input
 */
typedef struct _input_t_ {
    int *context;
    int num_ctx;
} input_t;

/**
 * Destroy a input layer and set the pointer to NULL.
 * @ingroup g_input
 * @param[in] ptr pointer to input_t.
 */
#define safe_input_destroy(ptr) do {\
    if((ptr) != NULL) {\
        input_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a input layer.
 * @ingroup g_input
 * @param[in] input input layer to be destroyed.
 */
void input_destroy(input_t* input);

/**
 * Duplicate a input layer.
 * @ingroup g_input
 * @param[in] i input layer to be duplicated.
 * @return the duplicated input layer. 
 */
input_t* input_dup(input_t *i);

/**
 * Parse a topo config line, and return a new input layer.
 * @ingroup g_input
 * @param[in] line topo config line.
 * @return a new input layer or NULL if error.
 */
input_t* input_parse_topo(const char *line);

#ifdef __cplusplus
}
#endif

#endif
