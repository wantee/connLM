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

    input->input_size = 0;
    safe_free(input->context);
    input->n_ctx = 0;
}

input_t* input_parse_topo(const char *line, int input_size)
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

    input->input_size = input_size;

    p = line;

    p = get_next_token(p, token);
    if (strcasecmp("input", token) != 0) {
        ST_WARNING("Not input line.");
        goto ERR;
    }

    safe_free(input->context);
    input->n_ctx = 0;
    while (p != NULL) {
        p = get_next_token(p, token);
        if (split_line(token, keyvalue, 2, MAX_LINE_LEN, "=") != 2) {
            ST_WARNING("Failed to split key/value. [%s][%s]", line, token);
            goto ERR;
        }

        if (strcasecmp("context", keyvalue) == 0) {
            if (input->n_ctx != 0) {
                ST_WARNING("Duplicated context.");
            }
            if (st_parse_int_array(keyvalue + MAX_LINE_LEN,
                        &input->context, &input->n_ctx) < 0) {
                ST_WARNING("Failed to parse context string.[%s]",
                        keyvalue + MAX_LINE_LEN);
                goto ERR;
            }
        } else {
            ST_WARNING("Unknown key/value[%s]", token);
        }
    }

    if (input->n_ctx == 0) {
        ST_WARNING("No context found.");
        goto ERR;
    }

    for (i = 0; i < input->n_ctx; i++) {
        if (input->context[i] == 0) {
            ST_WARNING("Context should not contain 0.");
            goto ERR;
        }
    }

    int_sort(input->context, input->n_ctx);

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

    input->context = (int *)malloc(sizeof(int)*in->n_ctx);
    if (input->context == NULL) {
        ST_WARNING("Failed to malloc context.");
        goto ERR;
    }
    for (i = 0; i < in->n_ctx; i++) {
        input->context[i] = in->context[i];
    }
    input->n_ctx = in->n_ctx;

    return input;

ERR:
    safe_input_destroy(input);
    return NULL;
}

layer_t* input_get_layer(input_t *input)
{
    layer_t *layer = NULL;

    ST_CHECK_PARAM(input == NULL, NULL);

    layer = (layer_t *)malloc(sizeof(layer_t));
    if (layer == NULL) {
        ST_WARNING("Failed to malloc layer.");
        goto ERR;
    }
    memset(layer, 0, sizeof(layer_t));

    strncat(layer->name, INPUT_LAYER_NAME, MAX_NAME_LEN - 1);
    strncat(layer->type, INPUT_LAYER_NAME, MAX_NAME_LEN - 1);

    return layer;
ERR:
    safe_free(layer);
    return NULL;
}

int input_load_header(input_t **input, int version,
        FILE *fp, bool *binary, FILE *fo_info)
{
    union {
        char str[4];
        int magic_num;
    } flag;

    int input_size;
    int n_ctx;

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
        if (fread(&input_size, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read input_size.");
            return -1;
        }
        if (fread(&n_ctx, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read n_ctx.");
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

        if (st_readline(fp, "Input size: %d", &input_size) != 1) {
            ST_WARNING("Failed to parse input_size.");
            goto ERR;
        }
        if (st_readline(fp, "Num context: %d", &n_ctx) != 1) {
            ST_WARNING("Failed to parse n_ctx.");
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

        (*input)->input_size = input_size;
        (*input)->n_ctx = n_ctx;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<INPUT>\n");
        fprintf(fo_info, "Input size: %d\n", input_size);
        fprintf(fo_info, "Num context: %d\n", n_ctx);
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

    sz = sizeof(int) * input->n_ctx;
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

        if (fread(input->context, sizeof(int), input->n_ctx, fp)
                != input->n_ctx) {
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
        for (i = 0; i < input->n_ctx; i++) {
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

        if (fwrite(&input->input_size, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write input_size.");
            return -1;
        }
        if (fwrite(&input->n_ctx, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write n_ctx.");
            return -1;
        }
    } else {
        if (fprintf(fp, "    \n<INPUT>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }

        if (fprintf(fp, "Input size: %d\n", input->input_size) < 0) {
            ST_WARNING("Failed to fprintf input_size.");
            return -1;
        }
        if (fprintf(fp, "Num context: %d\n", input->n_ctx) < 0) {
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

        if (fwrite(input->context, sizeof(int), input->n_ctx, fp)
                != input->n_ctx) {
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
        for (i = 0; i < input->n_ctx; i++) {
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

    snprintf(buf, MAX_LINE_LEN, "size=%d,ctx={", input->input_size);
    for (i = 0; i < input->n_ctx - 1; i++) {
        snprintf(label, label_len, "%s%d,", buf, input->context[i]);
        snprintf(buf, MAX_LINE_LEN, "%s", label);
    }
    snprintf(label, label_len, "%s%d}", buf, input->context[i]);

    return label;
}

int input_setup(input_t *input, int num_thrs)
{
    input_neuron_t *neu;
    size_t sz;

    int t;

    ST_CHECK_PARAM(input == NULL || num_thrs < 0, -1);

    input->n_neu = num_thrs;
    sz = sizeof(input_neuron_t) * num_thrs;
    input->neurons = (input_neuron_t *)malloc(sz);
    if (input->neurons == NULL) {
        ST_WARNING("Failed to malloc neurons.");
        goto ERR;
    }
    memset(input->neurons, 0, sz);

    if (input->context[0] < 0) {
        input->n_buf = -input->context[0];
        if (input->context[input->n_ctx - 1] > 0) {
            input->n_buf += input->context[input->n_ctx - 1];
        }
    } else {
        input->n_buf = input->context[input->n_ctx - 1];
    }
    ++input->n_buf;

    for (t = 0; t < input->n_neu; t++) {
        neu = input->neurons + t;
        sz = sizeof(int) * input->n_ctx;
        neu->frag = (int *)malloc(sz);
        if (neu->frag == NULL) {
            ST_WARNING("Failed to malloc frag.");
            goto ERR;
        }
        memset(neu->frag, 0, sz);

        sz = sizeof(int) * input->n_buf;
        neu->buf = (int *)malloc(sz);
        if (neu->buf == NULL) {
            ST_WARNING("Failed to malloc buf.");
            goto ERR;
        }
    }

    return 0;

ERR:
    if (input->neurons != NULL) {
        for (t = 0; t < input->n_neu; t++) {
            safe_free(input->neurons[t].frag);
        }
        safe_free(input->neurons);
    }
    input->n_neu = 0;

    return -1;
}

int input_reset(input_t *input, int tid)
{
    input_neuron_t *neu;

    ST_CHECK_PARAM(input == NULL || tid < 0, -1);

    neu = input->neurons + tid;
    for (i = 0; i < input->n_buf; i++) {
        neu->buf[i] = -1;
    }
    neu->i_buf = 0;

    return 0;
}

int input_feed(input_t *input, int word, int tid)
{
    ST_CHECK_PARAM(input == NULL || tid < 0, -1);

    if (neu->i_buf < input->n_buf) {
        neu->buf[neu->i_buf] = word;
        neu->i_buf++;
    } else {
        // assert(neu-.i_buf == input->n_buf);
        memmove(neu->buf, neu->buf + 1, neu->i_buf - 1);
        neu->buf[neu->i_buf - 1] = word;
    }

    for (i = 0; i < input->n_ctx; i++) {
        neu->frag[i] = neu->buf[];
    }
}
