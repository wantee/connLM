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

#include <stutils/st_log.h>
#include <stutils/st_utils.h>

#include "input.h"

void input_destroy(input_t *input)
{
    if (input == NULL) {
        return;
    }

    safe_free(input->context);
    input->num_ctx = 0;
}

input_t* input_parse_topo(const char *line)
{
    input_t *input = NULL;

    char keyvalue[2*MAX_LINE_LEN];
    char token[MAX_LINE_LEN];

    const char *p;

    ST_CHECK_PARAM(line == NULL, NULL);

    input = (input_t *)malloc(sizeof(input_t));
    if (input == NULL) {
        ST_WARNING("Failed to malloc input_t.");
        goto ERR;
    }
    memset(input, 0, sizeof(input_t));

    p = line;

    p = get_next_token(p, token);
    if (strcasecmp("input", token) != 0) {
        ST_WARNING("Not input line.");
        goto ERR;
    }

    safe_free(input->context);
    input->num_ctx = 0;
    while (p != NULL) {
        p = get_next_token(p, token);
        if (split_line(token, keyvalue, 2, MAX_LINE_LEN, "=") != 2) {
            ST_WARNING("Failed to split key/value. [%s][%s]", line, token);
            goto ERR;
        }

        if (strcasecmp("context", keyvalue) == 0) {
            if (input->num_ctx != 0) {
                ST_WARNING("Duplicated context.");
            }
            if (st_parse_int_array(keyvalue + MAX_LINE_LEN, &input->context,
                        &input->num_ctx) < 0) {
                ST_WARNING("Failed to parse context string.[%s]",
                        keyvalue + MAX_LINE_LEN);
                goto ERR;
            }
        } else {
            ST_WARNING("Unknown key/value[%s]", token);
        }
    }

    if (input->num_ctx == 0) {
        ST_WARNING("No context found.");
        goto ERR;
    }

    return input;

ERR:
    safe_input_destroy(input);
    return NULL;
}

input_t* input_dup(input_t *in)
{
    input_t *input = NULL;

    int i;

    ST_CHECK_PARAM(in == NULL, NULL);

    input = (input_t *) malloc(sizeof(input_t));
    if (input == NULL) {
        ST_WARNING("Falied to malloc input_t.");
        goto ERR;
    }
    memset(input, 0, sizeof(input_t));

    input->context = (int *)malloc(sizeof(int)*in->num_ctx);
    if (input->context == NULL) {
        ST_WARNING("Failed to malloc context.");
        goto ERR;
    }
    for (i = 0; i < in->num_ctx; i++) {
        input->context[i] = in->context[i];
    }
    input->num_ctx = in->num_ctx;

    return input;

ERR:
    safe_input_destroy(input);
    return NULL;
}

int input_load_header(input_t **input, int version,
        FILE *fp, bool *binary, FILE *fo_info)
{
    return 0;
}

int input_load_body(input_t *input, int version, FILE *fp, bool binary)
{
    return 0;
}

int input_save_header(input_t *input, FILE *fp, bool binary)
{
    return 0;
}

int input_save_body(input_t *input, FILE *fp, bool binary)
{
    return 0;
}
