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

static const int INPUT_MAGIC_NUM = 626140498 + 40;

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
    union {
        char str[4];
        int magic_num;
    } flag;

    int num_ctx;

    ST_CHECK_PARAM((input == NULL && fo_info == NULL) || fp == NULL
            || binary == NULL, -1);

    if (version < 3) {
        ST_WARNING("Too old version of connlm file");
        return -1;
    }

    if (fread(&flag.magic_num, sizeof(int), 1, fp) != 1) {
        ST_WARNING("Failed to load magic num.");
        return -1;
    }

    if (strncmp(flag.str, "    ", 4) == 0) {
        *binary = false;
    } else if (INPUT_MAGIC_NUM != flag.magic_num) {
        ST_WARNING("magic num wrong.");
        return -2;
    } else {
        *binary = true;
    }

    if (input != NULL) {
        *input = NULL;
    }

    if (*binary) {
        if (fread(&num_ctx, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read num_ctx.");
            return -1;
        }
    } else {
        if (st_readline(fp, "") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }
        if (st_readline(fp, "<INPUT>") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }

        if (st_readline(fp, "Num context: %d", &num_ctx) != 1) {
            ST_WARNING("Failed to parse num_ctx.");
            goto ERR;
        }
    }

    if (input != NULL) {
        *input = (input_t *)malloc(sizeof(input_t));
        if (*input == NULL) {
            ST_WARNING("Failed to malloc input_t");
            goto ERR;
        }
        memset(*input, 0, sizeof(input_t));

        (*input)->num_ctx = num_ctx;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<INPUT>\n");
        fprintf(fo_info, "Num context: %d\n", num_ctx);
    }

    return 0;

ERR:
    if (input != NULL) {
        safe_input_destroy(*input);
    }
    return -1;
}

int input_load_body(input_t *input, int version, FILE *fp, bool binary)
{
    size_t sz;
    int n;
    int i;

    ST_CHECK_PARAM(input == NULL || fp == NULL, -1);

    if (version < 3) {
        ST_WARNING("Too old version of connlm file");
        return -1;
    }

    safe_free(input->context);

    sz = sizeof(int) * input->num_ctx;
    input->context = (int *) malloc(sz);
    if (input->context == NULL) {
        ST_WARNING("Failed to malloc context.");
        goto ERR;
    }
    memset(input->context, 0, sz);

    if (binary) {
        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read magic num.");
            goto ERR;
        }

        if (n != -INPUT_MAGIC_NUM) {
            ST_WARNING("Magic num error.");
            goto ERR;
        }

        if (fread(input->context, sizeof(int), input->num_ctx, fp)
                != input->num_ctx) {
            ST_WARNING("Failed to read context.");
            goto ERR;
        }
    } else {
        if (st_readline(fp, "<INPUT-DATA>") != 0) {
            ST_WARNING("body flag error.");
            goto ERR;
        }

        if (st_readline(fp, "Context:") != 0) {
            ST_WARNING("context flag error.");
            goto ERR;
        }
        for (i = 0; i < input->num_ctx; i++) {
            if (st_readline(fp, "\t%*d\t%d", input->context + i) != 1) {
                ST_WARNING("Failed to parse context.");
                goto ERR;
            }
        }
    }

    return 0;
ERR:
    safe_free(input->context);

    return -1;
}

int input_save_header(input_t *input, FILE *fp, bool binary)
{
    int n;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (binary) {
        if (fwrite(&INPUT_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }
        if (input == NULL) {
            n = 0;
            if (fwrite(&n, sizeof(int), 1, fp) != 1) {
                ST_WARNING("Failed to write input size.");
                return -1;
            }
            return 0;
        }

        if (fwrite(&input->num_ctx, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write num_ctx.");
            return -1;
        }
    } else {
        if (fprintf(fp, "    \n<INPUT>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }

        if (fprintf(fp, "Num context: %d\n", input->num_ctx) < 0) {
            ST_WARNING("Failed to fprintf num ctx.");
            return -1;
        }
    }

    return 0;
}

int input_save_body(input_t *input, FILE *fp, bool binary)
{
    int n;
    int i;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (binary) {
        n = -INPUT_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        if (fwrite(input->context, sizeof(int), input->num_ctx, fp)
                != input->num_ctx) {
            ST_WARNING("Failed to write context.");
            return -1;
        }
    } else {
        if (fprintf(fp, "<INPUT-DATA>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }

        if (fprintf(fp, "Context:\n") < 0) {
            ST_WARNING("Failed to fprintf context.");
            return -1;
        }
        for (i = 0; i < input->num_ctx; i++) {
            if (fprintf(fp, "\t%d\t%d\n", i, input->context[i]) < 0) {
                ST_WARNING("Failed to fprintf context[%d].", i);
                return -1;
            }
        }
    }

    return 0;
}

char* input_draw_label(input_t *input, char *label, size_t label_len)
{
    char buf[MAX_LINE_LEN];
    int i;

    ST_CHECK_PARAM(input == NULL || label == NULL, NULL);

    snprintf(buf, MAX_LINE_LEN, "ctx={");
    for (i = 0; i < input->num_ctx - 1; i++) {
        snprintf(label, label_len, "%s%d,", buf, input->context[i]);
        snprintf(buf, MAX_LINE_LEN, "%s", label);
    }
    snprintf(label, label_len, "%s%d}", buf, input->context[i]);

    return label;
}
