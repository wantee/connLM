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

#include <stutils/st_macro.h>
#include <stutils/st_log.h>
#include <stutils/st_io.h>
#include <stutils/st_string.h>

#include "input.h"
#include "output.h"
#include "emb_glue.h"

static const int EMB_GLUE_MAGIC_NUM = 626140498 + 72;

static const char *index_method_str[] = {
    "UnDefined",
    "Word",
    "Position",
};

static const char* index2str(emb_index_method_t i)
{
    return index_method_str[i];
}

static emb_index_method_t str2index(const char *str)
{
    int i;

    i = 0;
    for (; i < sizeof(index_method_str) / sizeof(index_method_str[0]); i++) {
        if (strcasecmp(index_method_str[i], str) == 0) {
            return (emb_index_method_t)i;
        }
    }

    return EI_UNKNOWN;
}

#define safe_emb_glue_data_destroy(ptr) do {\
    if((ptr) != NULL) {\
        emb_glue_data_destroy((emb_glue_data_t *)ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)

static void emb_glue_data_destroy(emb_glue_data_t *data)
{
    if (data == NULL) {
        return;
    }

    data->index_method = EI_UNKNOWN;
    data->num_vecs = 0;
}

static emb_glue_data_t* emb_glue_data_init()
{
    emb_glue_data_t *data = NULL;

    data = (emb_glue_data_t *)malloc(sizeof(emb_glue_data_t));
    if (data == NULL) {
        ST_WARNING("Failed to malloc emb_glue_data.");
        goto ERR;
    }
    memset(data, 0, sizeof(emb_glue_data_t));

    return data;
ERR:
    safe_emb_glue_data_destroy(data);
    return NULL;
}

static emb_glue_data_t* emb_glue_data_dup(emb_glue_data_t *src)
{
    emb_glue_data_t *dst = NULL;

    ST_CHECK_PARAM(src == NULL, NULL);

    dst = emb_glue_data_init();
    if (dst == NULL) {
        ST_WARNING("Failed to emb_glue_data_init.");
        goto ERR;
    }
    dst->index_method = ((emb_glue_data_t *)src)->index_method;
    dst->num_vecs = ((emb_glue_data_t *)src)->num_vecs;

    return (void *)dst;
ERR:
    safe_emb_glue_data_destroy(dst);
    return NULL;
}

void emb_glue_destroy(glue_t *glue)
{
    if (glue == NULL) {
        return;
    }

    safe_emb_glue_data_destroy(glue->extra);
}

int emb_glue_init(glue_t *glue)
{
    ST_CHECK_PARAM(glue == NULL, -1);

    if (strcasecmp(glue->type, EMB_GLUE_NAME) != 0) {
        ST_WARNING("Not a sum glue. [%s]", glue->type);
        return -1;
    }

    glue->extra = (void *)emb_glue_data_init();
    if (glue->extra == NULL) {
        ST_WARNING("Failed to emb_glue_data_init.");
        goto ERR;
    }

    return 0;

ERR:
    safe_emb_glue_data_destroy(glue->extra);
    return -1;
}

int emb_glue_dup(glue_t *dst, glue_t *src)
{
    ST_CHECK_PARAM(dst == NULL || src == NULL, -1);

    if (strcasecmp(dst->type, EMB_GLUE_NAME) != 0) {
        ST_WARNING("dst is Not a emb glue. [%s]", dst->type);
        return -1;
    }

    if (strcasecmp(src->type, EMB_GLUE_NAME) != 0) {
        ST_WARNING("src is Not a emb glue. [%s]", src->type);
        return -1;
    }

    dst->extra = (void *)emb_glue_data_dup((emb_glue_data_t *)src->extra);
    if (dst->extra == NULL) {
        ST_WARNING("Failed to emb_glue_data_dup.");
        goto ERR;
    }

    return 0;

ERR:
    safe_emb_glue_data_destroy(dst->extra);
    return -1;
}

int emb_glue_parse_topo(glue_t *glue, const char *line)
{
    char keyvalue[2*MAX_LINE_LEN];
    char token[MAX_LINE_LEN];

    emb_glue_data_t *data;
    const char *p;

    ST_CHECK_PARAM(glue == NULL || line == NULL, -1);

    data = (emb_glue_data_t *)glue->extra;
    p = line;

    while (p != NULL) {
        p = get_next_token(p, token);
        if (token[0] == '\0') {
            continue;
        }

        if (split_line(token, keyvalue, 2, MAX_LINE_LEN, "=") != 2) {
            ST_WARNING("Failed to split key/value. [%s]", token);
            goto ERR;
        }

        if (strcasecmp("num_vecs", keyvalue) == 0) {
            data->num_vecs = atoi(keyvalue + MAX_LINE_LEN);
            if (data->num_vecs <= 0) {
                ST_WARNING("Illegal num_vecs[%s]", keyvalue + MAX_LINE_LEN);
                goto ERR;
            }
        } else if (strcasecmp("index", keyvalue) == 0) {
            if (data->index_method != EI_UNDEFINED) {
                ST_WARNING("Duplicated index method.");
            }
            data->index_method = str2index(keyvalue + MAX_LINE_LEN);
            if (data->index_method == EI_UNKNOWN) {
                ST_WARNING("Failed to parse index method string.[%s]",
                        keyvalue + MAX_LINE_LEN);
                goto ERR;
            }
        } else {
            ST_WARNING("Unknown key/value[%s]", token);
        }
    }

    if (data->index_method == EI_UNDEFINED) {
        data->index_method = EI_WORD; // default to word embedding
    }

    return 0;

ERR:
    return -1;
}

bool emb_glue_check(glue_t *glue, layer_t **layers, int n_layer,
        input_t *input, output_t *output)
{
    emb_glue_data_t *data;

    ST_CHECK_PARAM(glue == NULL || input == NULL, false);

    if (strcasecmp(glue->type, EMB_GLUE_NAME) != 0) {
        ST_WARNING("Not a emb glue. [%s]", glue->type);
        return false;
    }

    if (layers == NULL) {
        ST_WARNING("No layers.");
        return false;
    }

    if (strcasecmp(layers[glue->in_layer]->type,
                INPUT_LAYER_NAME) != 0) {
        ST_WARNING("emb glue: in layer should be input layer.");
        return false;
    }

    if (strcasecmp(layers[glue->out_layer]->type,
                OUTPUT_LAYER_NAME) == 0) {
        ST_WARNING("emb glue: out layer should not be output layer.");
        return false;
    }

    if (input->n_ctx > 1) {
        if (input->combine == IC_UNDEFINED) {
            ST_WARNING("emb glue: No combine specified in input.");
            return false;
        }
    } else {
        if (input->combine == IC_UNDEFINED) {
            input->combine = IC_CONCAT;
        }
    }

    if (input->combine == IC_CONCAT) {
        if (glue->out_length % input->n_ctx != 0) {
            ST_WARNING("emb glue: can not apply CONCAT combination. "
                    "out layer length can not divided by context number.");
            return false;
        }
    }

    data = (emb_glue_data_t *)glue->extra;
    if (data->index_method == EI_WORD) {
        if (data->num_vecs <= 0) {
            data->num_vecs = input->input_size;
        } else if (data->num_vecs != input->input_size){
            ST_WARNING("number of word embedding is not equal to vocab size.");
            return false;
        }
    } else {
        if (data->num_vecs <= 0) {
            ST_WARNING("number of position embedding is not set.");
            return false;
        }
    }

    return true;
}

char* emb_glue_draw_label(glue_t *glue, char *label, size_t label_len)
{
    emb_glue_data_t *data;

    ST_CHECK_PARAM(glue == NULL || label == NULL, NULL);

    data = (emb_glue_data_t *)glue->extra;

    snprintf(label, label_len, ",index=%s,num_vecs=%d",
            index2str(data->index_method), data->num_vecs);

    return label;
}

int emb_glue_load_header(void **extra, int version,
        FILE *fp, bool *binary, FILE *fo_info)
{
    emb_glue_data_t *data = NULL;

    union {
        char str[4];
        int magic_num;
    } flag;

    char sym[MAX_LINE_LEN];
    int i;
    int num_vecs;

    ST_CHECK_PARAM((extra == NULL && fo_info == NULL) || fp == NULL
            || binary == NULL, -1);

    if (version < 7) {
        ST_WARNING("Too old version of connlm file");
        return -1;
    }

    if (fread(&flag.magic_num, sizeof(int), 1, fp) != 1) {
        ST_WARNING("Failed to load magic num.");
        return -1;
    }

    if (strncmp(flag.str, "    ", 4) == 0) {
        *binary = false;
    } else if (EMB_GLUE_MAGIC_NUM != flag.magic_num) {
        ST_WARNING("magic num wrong.");
        return -2;
    } else {
        *binary = true;
    }

    if (extra != NULL) {
        *extra = NULL;
    }

    if (*binary) {
        if (fread(&i, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read index method.");
            return -1;
        }
        if (fread(&num_vecs, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read num_vecs.");
            return -1;
        }
    } else {
        if (st_readline(fp, "") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }
        if (st_readline(fp, "<EMB-GLUE>") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }

        if (st_readline(fp, "Index method: %"xSTR(MAX_LINE_LEN)"s", sym) != 1) {
            ST_WARNING("Failed to parse index method.");
            goto ERR;
        }
        sym[MAX_LINE_LEN - 1] = '\0';
        i = (int)str2index(sym);
        if (i == (int)EI_UNKNOWN) {
            ST_WARNING("Unknown index method[%s]", sym);
            goto ERR;
        }
        if (st_readline(fp, "Num vectors: %d", &num_vecs) != 1) {
            ST_WARNING("Failed to parse num_vecs.");
            goto ERR;
        }
    }

    if (extra != NULL) {
        data = emb_glue_data_init();
        if (data == NULL) {
            ST_WARNING("Failed to emb_glue_data_init.");
            goto ERR;
        }

        *extra = (void *)data;
        data->index_method = (emb_index_method_t)i;
        data->num_vecs = num_vecs;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<EMB-GLUE>\n");
        fprintf(fo_info, "Index method: %s\n",
                index2str((emb_index_method_t)i));
        fprintf(fo_info, "Num vectors: %d\n", num_vecs);
    }

    return 0;

ERR:
    if (extra != NULL) {
        safe_emb_glue_data_destroy(*extra);
    }
    return -1;
}

int emb_glue_save_header(void *extra, FILE *fp, bool binary)
{
    emb_glue_data_t *data;
    int i;

    ST_CHECK_PARAM(extra == NULL || fp == NULL, -1);

    data = (emb_glue_data_t *)extra;

    if (binary) {
        if (fwrite(&EMB_GLUE_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        i = (int)data->index_method;
        if (fwrite(&i, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write index method.");
            return -1;
        }
        if (fwrite(&data->num_vecs, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write num_vecs.");
            return -1;
        }
    } else {
        if (fprintf(fp, "    \n<EMB-GLUE>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }

        if (fprintf(fp, "Index method: %s\n",
                    index2str(data->index_method)) < 0) {
            ST_WARNING("Failed to fprintf index method.");
            return -1;
        }
        if (fprintf(fp, "Num vectors: %d\n", data->num_vecs) < 0) {
            ST_WARNING("Failed to fprintf num_vecs.");
            return -1;
        }
    }

    return 0;
}

int emb_glue_init_data(glue_t *glue, input_t *input,
        layer_t **layers, output_t *output)
{
    emb_glue_data_t *data;
    int col;

    ST_CHECK_PARAM(glue == NULL || glue->wt == NULL
            || input == NULL, -1);

    if (strcasecmp(glue->type, EMB_GLUE_NAME) != 0) {
        ST_WARNING("Not a emb glue. [%s]", glue->type);
        return -1;
    }

    data = (emb_glue_data_t *)glue->extra;
    if (input->combine == IC_CONCAT) {
        col = glue->out_length / input->n_ctx;
    } else {
        col = glue->out_length;
    }

    if (wt_init(glue->wt, data->num_vecs, col) < 0) {
        ST_WARNING("Failed to wt_init.");
        return -1;
    }

    return 0;
}

wt_updater_t* emb_glue_init_wt_updater(glue_t *glue, param_t *param)
{
    wt_updater_t *wt_updater = NULL;

    ST_CHECK_PARAM(glue == NULL, NULL);

    if (strcasecmp(glue->type, EMB_GLUE_NAME) != 0) {
        ST_WARNING("Not a emb glue. [%s]", glue->type);
        return NULL;
    }

    wt_updater = wt_updater_create(param == NULL ? &glue->param : param,
            glue->wt->mat, glue->wt->row, glue->wt->col, WT_UT_ONE_SHOT);
    if (wt_updater == NULL) {
        ST_WARNING("Failed to wt_updater_create.");
        goto ERR;
    }

    return wt_updater;

ERR:
    safe_wt_updater_destroy(wt_updater);
    return NULL;
}

int emb_glue_generate_wildcard_repr(glue_t *glue, count_t *word_cnts)
{
    emb_glue_data_t *data;
    real_t *word_weights = NULL;
    real_t sum;

    int num_words;
    int emb_size;
    int i, j;

    ST_CHECK_PARAM(glue == NULL || word_cnts == NULL, -1);

    if (strcasecmp(glue->type, EMB_GLUE_NAME) != 0) {
        ST_WARNING("Not a emb glue. [%s]", glue->type);
        goto ERR;
    }

    data = (emb_glue_data_t *)glue->extra;
    if (data->index_method == EI_POS) {
        // we do not need wildcard_repr, just create a empty array to pass
        // the check done by component.
        glue->wildcard_repr = (real_t *)malloc(sizeof(real_t));
        if (glue->wildcard_repr == NULL) {
            ST_WARNING("Failed to mallco wildcard_repr.");
            goto ERR;
        }

        return 0;
    }

    num_words = glue->wt->row;
    emb_size = glue->wt->col;

    glue->wildcard_repr = (real_t *)malloc(sizeof(real_t) * emb_size);
    if (glue->wildcard_repr == NULL) {
        ST_WARNING("Failed to mallco wildcard_repr.");
        goto ERR;
    }

    word_weights = (real_t *)malloc(sizeof(real_t) * num_words);
    if (word_weights == NULL) {
        ST_WARNING("Failed to mallco word_weights.");
        goto ERR;
    }

    // use the weighted mean of embedding of all words (except <s>) as the repr
    sum = 0;
    for (i = 1; i < num_words; i++) {
        sum += word_cnts[i];
    }

    for (i = 1; i < num_words; i++) {
        word_weights[i] = word_cnts[i] / sum;
    }

    for (j = 0; j < emb_size; j++) {
        glue->wildcard_repr[j] = 0;
        for (i = 1; i < num_words; i++) {
            glue->wildcard_repr[j] += word_weights[i]
                * glue->wt->mat[i * emb_size + j];
        }
    }

    safe_free(word_weights);

    return 0;

ERR:
    safe_free(glue->wildcard_repr);
    safe_free(word_weights);

    return -1;
}
