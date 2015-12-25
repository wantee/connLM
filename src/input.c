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

#include <string.h>

#include <st_log.h>
#include <st_utils.h>

#include "input.h"

void input_destroy(input_t *input)
{
    if (input == NULL) {
        return;
    }
}

input_t* input_parse_topo(const char *line)
{
    input_t *input = NULL;

    ST_CHECK_PARAM(line == NULL, NULL);

    return input;
}

input_t* input_dup(input_t *i)
{
    input_t *input = NULL;

    ST_CHECK_PARAM(i == NULL, NULL);

    input = (input_t *) malloc(sizeof(input_t));
    if (input == NULL) {
        ST_WARNING("Falied to malloc input_t.");
        goto ERR;
    }
    memset(input, 0, sizeof(input_t));

    return input;

ERR:
    safe_input_destroy(input);
    return NULL;
}

