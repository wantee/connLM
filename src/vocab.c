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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

#include <st_macro.h>
#include <st_log.h>

#include "vocab.h"

static const int VOCAB_MAGIC_NUM = 626140498 + 1;

int vocab_load_opt(vocab_opt_t *vocab_opt, st_opt_t *opt,
        const char *sec_name)
{
    ST_CHECK_PARAM(vocab_opt == NULL || opt == NULL, -1);

    memset(vocab_opt, 0, sizeof(vocab_opt_t));

    ST_OPT_SEC_GET_INT(opt, sec_name, "MAX_WORD_NUM",
            vocab_opt->max_word_num, 1000000, 
            "Maximum number of words in Vocabulary");

    return 0;

ST_OPT_ERR:
    return -1;
}

vocab_t *vocab_create(vocab_opt_t *vocab_opt)
{
    vocab_t *vocab = NULL;

    ST_CHECK_PARAM(vocab_opt == NULL, NULL);

    vocab = (vocab_t *) malloc(sizeof(vocab_t));
    if (vocab == NULL) {
        ST_WARNING("Falied to malloc vocab_t.");
        goto ERR;
    }
    memset(vocab, 0, sizeof(vocab_t));

    vocab->vocab_opt = *vocab_opt;

    vocab->alphabet = st_alphabet_create(vocab_opt->max_word_num);
    if (vocab->alphabet == NULL) {
        ST_WARNING("Failed to st_alphabet_create.");
        goto ERR;
    }

    vocab->word_infos = (word_info_t *) malloc(sizeof(word_info_t) 
            * vocab_opt->max_word_num);
    if (vocab->word_infos == NULL) {
        ST_WARNING("Failed to malloc word_infos.");
        goto ERR;
    }
    memset(vocab->word_infos, 0, sizeof(word_info_t) 
            * vocab_opt->max_word_num);

    if (st_alphabet_add_label(vocab->alphabet, "</s>") != 0) {
        ST_WARNING("Failed to st_alphabet_add_label.");
        goto ERR;
    }
    vocab->word_infos[0].id = 0;
    vocab->word_infos[0].cnt = 0;

    return vocab;
  ERR:
    safe_vocab_destroy(vocab);
    return NULL;
}

void vocab_destroy(vocab_t *vocab)
{
    if (vocab == NULL) {
        return;
    }

    safe_st_alphabet_destroy(vocab->alphabet);
    safe_free(vocab->word_infos);
}

vocab_t* vocab_dup(vocab_t *v)
{
    vocab_t *vocab = NULL;

    ST_CHECK_PARAM(v == NULL, NULL);

    vocab = (vocab_t *) malloc(sizeof(vocab_t));
    if (vocab == NULL) {
        ST_WARNING("Falied to malloc vocab_t.");
        goto ERR;
    }
    memset(vocab, 0, sizeof(vocab_t));

    vocab->vocab_opt = v->vocab_opt;
    *vocab = *v;

    vocab->alphabet = st_alphabet_dup(v->alphabet);
    if (vocab->alphabet == NULL) {
        ST_WARNING("Failed to st_alphabet_dup.");
        goto ERR;
    }

    vocab->word_infos = (word_info_t *) malloc(sizeof(word_info_t) 
            * v->vocab_opt.max_word_num);
    if (vocab->word_infos == NULL) {
        ST_WARNING("Failed to malloc word_infos.");
        goto ERR;
    }
    memcpy(vocab->word_infos, v->word_infos, sizeof(word_info_t) 
            * v->vocab_opt.max_word_num);

    return vocab;

ERR:
    safe_vocab_destroy(vocab);
    return NULL;
}

long vocab_load_header(vocab_t **vocab, FILE *fp, bool *binary,
        FILE *fo_info)
{
    char line[MAX_LINE_LEN];
    int vocab_size;
    union {
        char str[4];
        int magic_num;
    } flag;

    ST_CHECK_PARAM((vocab == NULL && fo_info == NULL) || fp == NULL
            || binary == NULL, -1);

    if (fread(&flag.magic_num, sizeof(int), 1, fp) != 1) {
        ST_WARNING("Failed to load magic num.");
        return -1;
    }

    if (strncmp(flag.str, "    ", 4) == 0) {
        *binary = false;
    } else if (VOCAB_MAGIC_NUM != flag.magic_num) {
        ST_WARNING("magic num wrong.");
        return -2;
    } else {
        *binary = true;
    }

    if (vocab != NULL) {
        *vocab = NULL;
    }
    
    if (*binary) {
        if (fread(&vocab_size, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read vocab size.");
            return -1;
        }

        if (vocab != NULL) {
            *vocab = (vocab_t *)malloc(sizeof(vocab_t));
            if (*vocab == NULL) {
                ST_WARNING("Failed to malloc vocab_t");
                goto ERR;
            }
            memset(*vocab, 0, sizeof(vocab_t));
        }
    } else {
        if (fgets(line, MAX_LINE_LEN, fp) == NULL) {
            ST_WARNING("Failed to read flag.");
            return -1;
        }

        if (fgets(line, MAX_LINE_LEN, fp) == NULL) {
            ST_WARNING("Failed to read flag.");
            return -1;
        }
        if (strncmp(line, "<VOCAB>", 7) != 0) {
            ST_WARNING("flag error.[%s]", line);
            return -1;
        }

        if (vocab != NULL) {
            *vocab = (vocab_t *)malloc(sizeof(vocab_t));
            if (*vocab == NULL) {
                ST_WARNING("Failed to malloc vocab_t");
                goto ERR;
            }
            memset(*vocab, 0, sizeof(vocab_t));
        }

        if (fgets(line, MAX_LINE_LEN, fp) == NULL) {
            ST_WARNING("Failed to read vocab_size.");
            return -1;
        }
        if (sscanf(line, "Vocab size: %d", &vocab_size) != 1) {
            ST_WARNING("Failed to parse vocab_size.[%s]", line);
            return -1;
        }
    }

    if (vocab != NULL) {
        (*vocab)->vocab_size = vocab_size;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<VOCAB>\n");
        fprintf(fo_info, "Vocab size: %d\n", vocab_size);
    }

    return 0;

ERR:
    safe_vocab_destroy(*vocab);
    return -1;
}

int vocab_load_body(vocab_t *vocab, FILE *fp, bool binary)
{
    char line[MAX_LINE_LEN];
    int i;

    ST_CHECK_PARAM(vocab == NULL || fp == NULL, -1);

    vocab->alphabet = NULL;
    vocab->word_infos = NULL;

    if (binary) {
        vocab->alphabet = st_alphabet_load_from_bin(fp); 
        if (vocab->alphabet == NULL) {
            ST_WARNING("Failed to st_alphabet_load_from_bin.");
            goto ERR;
        }

        vocab->word_infos = (word_info_t *)malloc(sizeof(word_info_t)
                * vocab->vocab_size);
        if (vocab->word_infos == NULL) {
            ST_WARNING("Failed to malloc word_infos.");
            goto ERR;
        }

        if (fread(vocab->word_infos, sizeof(word_info_t),
                    vocab->vocab_size, fp) != vocab->vocab_size) {
            ST_WARNING("Failed to write word infos.");
            goto ERR;
        }
    } else {
        if (fgets(line, MAX_LINE_LEN, fp) == NULL) {
            ST_WARNING("Failed to read body flag.");
            goto ERR;
        }
        if (strncmp(line, "<VOCAB-DATA>", 12) != 0) {
            ST_WARNING("body flag error.[%s]", line);
            goto ERR;
        }

        vocab->alphabet = st_alphabet_load_from_txt(fp);
        if (vocab->alphabet == NULL) {
            ST_WARNING("Failed to st_alphabet_load_from_txt.");
            goto ERR;
        }

        vocab->word_infos = (word_info_t *)malloc(sizeof(word_info_t)
                * vocab->vocab_size);
        if (vocab->word_infos == NULL) {
            ST_WARNING("Failed to malloc word_infos.");
            goto ERR;
        }

        if (fgets(line, MAX_LINE_LEN, fp) == NULL) {
            ST_WARNING("Failed to read word info flag.");
            goto ERR;
        }
        if (strncmp(line, "Word infos:", 11) != 0) {
            ST_WARNING("word info flag error.[%s]", line);
            goto ERR;
        }

        for (i = 0; i < vocab->vocab_size; i++) {
            if (fgets(line, MAX_LINE_LEN, fp) == NULL) {
                ST_WARNING("Failed to read word info.[%d]", i);
                goto ERR;
            }
            if (sscanf(line, "\t%d\t%ld\n", &(vocab->word_infos[i].id),
                    &(vocab->word_infos[i].cnt)) != 2) {
                ST_WARNING("Failed to parse word info.[%s]", line);
                goto ERR;
            }
        }
    }

    return 0;
ERR:
    safe_st_alphabet_destroy(vocab->alphabet);
    safe_free(vocab->word_infos);
    return -1;
}

int vocab_save_header(vocab_t *vocab, FILE *fp, bool binary)
{
    ST_CHECK_PARAM(fp == NULL, -1);

    if (binary) {
        if (fwrite(&VOCAB_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }
        if (vocab == NULL) {
            return 0;
        }

        if (fwrite(&vocab->vocab_size, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write vocab size.");
            return -1;
        }
    } else {
        fprintf(fp, "    \n<VOCAB>\n");

        if (vocab == NULL) {
            return 0;
        }

        fprintf(fp, "Vocab size: %d\n", vocab->vocab_size);
    }

    return 0;
}

int vocab_save_body(vocab_t *vocab, FILE *fp, bool binary)
{
    int i;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (vocab == NULL) {
        return 0;
    }

    if (binary) {
        if (st_alphabet_save_bin(vocab->alphabet, fp) < 0) {
            ST_WARNING("Failed to st_alphabet_save_bin.");
            return -1;
        }

        if (fwrite(vocab->word_infos, sizeof(word_info_t),
                    vocab->vocab_size, fp) != vocab->vocab_size) {
            ST_WARNING("Failed to write word infos.");
            return -1;
        }
    } else {
        fprintf(fp, "<VOCAB-DATA>\n");

        if (st_alphabet_save_txt(vocab->alphabet, fp) < 0) {
            ST_WARNING("Failed to st_alphabet_save_txt.");
            return -1;
        }

        fprintf(fp, "Word infos:\n");
        for (i = 0; i < vocab->vocab_size; i++) {
            fprintf(fp, "\t%d\t%ld\n", vocab->word_infos[i].id,
                    vocab->word_infos[i].cnt);
        }
    }

    return 0;
}

static int vocab_sort(vocab_t *vocab)
{
    word_info_t swap;
    st_alphabet_t *alphabet = NULL;
    char *word;
    int a, b, max;

    ST_CHECK_PARAM(vocab == NULL, -1);

    for (a = 1; a < vocab->vocab_size; a++) {
        max = a;
        for (b = a + 1; b < vocab->vocab_size; b++) {
            if (vocab->word_infos[max].cnt < vocab->word_infos[b].cnt) {
                max = b;
            }
        }

        if (max == a) {
            continue;
        }

        swap = vocab->word_infos[max];
        vocab->word_infos[max] = vocab->word_infos[a];
        vocab->word_infos[a] = swap;
    }

    alphabet = st_alphabet_create(vocab->vocab_size);
    if (alphabet == NULL) {
        ST_WARNING("Failed to st_alphabet_create alphabet.");
        return -1;
    }

    for (a = 0; a < vocab->vocab_size; a++) {
        word = st_alphabet_get_label(vocab->alphabet,
                vocab->word_infos[a].id);
        if (word == NULL) {
            ST_WARNING("Failed to st_alphabet_get_label[%d].",
                       vocab->word_infos[a].id);
            goto ERR;
        }

        if (st_alphabet_add_label(alphabet, word) < 0) {
            ST_WARNING("Failed to st_alphabet_add_label[%s].", word);
            goto ERR;
        }
    }

    safe_st_alphabet_destroy(vocab->alphabet);
    vocab->alphabet = alphabet;

    return 0;

  ERR:
    safe_st_alphabet_destroy(alphabet);
    return -1;
}

static int vocab_read_word(char *word, size_t word_len, FILE * fp)
{
    size_t a;
    int ch;

    ST_CHECK_PARAM(word == NULL || word_len <= 0 || fp == NULL, -1);

    a = 0;
    while (!feof(fp)) {
        ch = fgetc(fp);

        if (ch == 13) {
            continue;
        }

        if ((ch == ' ') || (ch == '\t') || (ch == '\n')) {
            if (a > 0) {
                if (ch == '\n') {
                    ungetc(ch, fp);
                }
                break;
            }

            if (ch == '\n') {
                strncpy(word, "</s>", word_len);
                word[word_len - 1] = '\0';
                return 0;
            } else {
                continue;
            }
        }

        word[a] = ch;
        a++;

        if (a >= word_len) {
            a--;
        }
    }

    word[a] = '\0';

    return 0;
}

int vocab_learn(vocab_t *vocab, FILE *fp)
{
    char word[MAX_SYM_LEN];

    int words = 0;
    int id;

    ST_CHECK_PARAM(vocab == NULL || fp == NULL, -1);

    words = 0;
    while (1) {
        if (vocab_read_word(word, MAX_SYM_LEN, fp) < 0) {
            ST_WARNING("Failed to vocab_read_word.");
            return -1;
        }

        if (feof(fp)) {
            break;
        }

        words++;

        id = vocab_get_id(vocab, word);
        if (id == -1) {
            id = vocab_add_word(vocab, word);
            if (id < 0) {
                ST_WARNING("Failed to vocab_add_word.");
                return -1;
            }

            vocab->word_infos[id].id = id;
            vocab->word_infos[id].cnt = 1;
        } else {
            vocab->word_infos[id].cnt++;
        }
    }

    vocab->vocab_size = st_alphabet_get_label_num(vocab->alphabet);

    if (vocab_sort(vocab) < 0) {
        ST_WARNING("Failed to vocab_sort.");
        return -1;
    }

    return 0;
}

int vocab_get_id(vocab_t *vocab, const char *word)
{
    return st_alphabet_get_index(vocab->alphabet, word);
}

char* vocab_get_word(vocab_t *vocab, int id)
{
    return st_alphabet_get_label(vocab->alphabet, id);
}

int vocab_add_word(vocab_t *vocab, const char* word)
{
    return st_alphabet_add_label(vocab->alphabet, word);
}

