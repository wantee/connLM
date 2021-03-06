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

#include <stutils/st_macro.h>
#include <stutils/st_log.h>
#include <stutils/st_io.h>
#include <stutils/st_utils.h>
#include <stutils/st_string.h>

#include "vocab.h"

static const int VOCAB_MAGIC_NUM = 626140498 + 10;

int vocab_load_opt(vocab_opt_t *vocab_opt, st_opt_t *opt,
        const char *sec_name)
{
    ST_CHECK_PARAM(vocab_opt == NULL || opt == NULL, -1);

    memset(vocab_opt, 0, sizeof(vocab_opt_t));

    ST_OPT_SEC_GET_STR(opt, sec_name, "WORDLIST",
        vocab_opt->wordlist, MAX_DIR_LEN, "", "File name of wordlist.");

    if (vocab_opt->wordlist[0] == '\0') {
        ST_OPT_SEC_GET_INT(opt, sec_name, "MAX_ALPHABET_SIZE",
                vocab_opt->max_alphabet_size, 1000000,
                "Maximum size of alphabet for vocab.");
    }

    return 0;

ST_OPT_ERR:
    return -1;
}

static int vocab_load_wordlist(vocab_t *vocab)
{
    FILE *fp = NULL;
    char *line = NULL;
    size_t line_sz = 0;

    bool err;

    ST_CHECK_PARAM(vocab == NULL, -1);

    fp = st_fopen(vocab->vocab_opt.wordlist, "r");
    if (fp == NULL) {
        ST_ERROR("Failed to open wordlist[%s].", vocab->vocab_opt.wordlist);
        goto ERR;
    }

    err = false;
    while (st_fgets(&line, &line_sz, fp, &err)) {
        remove_newline(line);

        if (line[0] == '\0') {
            continue;
        }

        if (strchr(line, ' ') != NULL || strchr(line, '\t') != NULL) {
            ST_ERROR("words should not contain whitespaces, line[%s]", line);
            goto ERR;
        }

        if (strcasecmp(line, SENT_END) == 0 || strcasecmp(line, UNK) == 0
                || strcasecmp(line, SENT_START) == 0) {
            continue;
        }

        if (vocab_add_word(vocab, line) < 0) {
            ST_ERROR("Failed to vocab_add_word.");
            goto ERR;
        }
    }

    safe_fclose(fp);
    safe_st_free(line);

    if (err) {
        return -1;
    }

    return 0;

ERR:
    safe_fclose(fp);
    safe_st_free(line);
    return -1;
}

vocab_t *vocab_create(vocab_opt_t *vocab_opt)
{
    vocab_t *vocab = NULL;

    ST_CHECK_PARAM(vocab_opt == NULL, NULL);

    vocab = (vocab_t *)st_malloc(sizeof(vocab_t));
    if (vocab == NULL) {
        ST_ERROR("Failed to st_malloc vocab_t.");
        goto ERR;
    }
    memset(vocab, 0, sizeof(vocab_t));

    vocab->vocab_opt = *vocab_opt;

    if (vocab->vocab_opt.wordlist[0] != '\0') {
        vocab->vocab_opt.max_alphabet_size = (int)st_count_lines(
                vocab->vocab_opt.wordlist);
        if (vocab->vocab_opt.max_alphabet_size < 0) {
            ST_ERROR("Failed to st_count_lines for wordlist[%s].",
                    vocab->vocab_opt.wordlist);
            goto ERR;
        }
        vocab->vocab_opt.max_alphabet_size += 2; /* for </s> and <unk> */
    }

    vocab->alphabet = st_alphabet_create(vocab->vocab_opt.max_alphabet_size);
    if (vocab->alphabet == NULL) {
        ST_ERROR("Failed to st_alphabet_create.");
        goto ERR;
    }

    if (st_alphabet_add_label(vocab->alphabet, SENT_END) != SENT_END_ID) {
        ST_ERROR("Failed to add sent_end[%s].", SENT_END);
        goto ERR;
    }

    // <unk> must be the last special word, since we will use 'UNK_ID + 1'
    // in sorting vocab
    if (st_alphabet_add_label(vocab->alphabet, UNK) != UNK_ID) {
        ST_ERROR("Failed to add unk[%s].", UNK);
        goto ERR;
    }

    if (vocab->vocab_opt.wordlist[0] != '\0') {
        if (vocab_load_wordlist(vocab) < 0) {
            ST_ERROR("Failed to st_vocab_load_wordlist.");
            goto ERR;
        }
    }
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
    safe_st_free(vocab->cnts);
}

vocab_t* vocab_dup(vocab_t *v)
{
    vocab_t *vocab = NULL;

    ST_CHECK_PARAM(v == NULL, NULL);

    vocab = (vocab_t *)st_malloc(sizeof(vocab_t));
    if (vocab == NULL) {
        ST_ERROR("Failed to st_malloc vocab_t.");
        goto ERR;
    }
    memset(vocab, 0, sizeof(vocab_t));

    vocab->vocab_opt = v->vocab_opt;
    *vocab = *v;

    vocab->alphabet = st_alphabet_dup(v->alphabet);
    if (vocab->alphabet == NULL) {
        ST_ERROR("Failed to st_alphabet_dup.");
        goto ERR;
    }

    vocab->cnts = (count_t *)st_malloc(sizeof(count_t) * v->vocab_size);
    if (vocab->cnts == NULL) {
        ST_ERROR("Failed to st_malloc cnts.");
        goto ERR;
    }
    memcpy(vocab->cnts, v->cnts, sizeof(count_t)*v->vocab_size);

    return vocab;

ERR:
    safe_vocab_destroy(vocab);
    return NULL;
}

int vocab_load_header(vocab_t **vocab, int version, FILE *fp,
        connlm_fmt_t *fmt, FILE *fo_info)
{
    union {
        char str[4];
        int magic_num;
    } flag;

    int vocab_size;

    ST_CHECK_PARAM((vocab == NULL && fo_info == NULL) || fp == NULL
            || fmt == NULL, -1);

    if (version < 11) {
        ST_ERROR("File versoin[%d] less than 11 is not supported, "
                "please downgrade connlm toolkit", version);
        return -1;
    }

    if (fread(&flag.magic_num, sizeof(int), 1, fp) != 1) {
        ST_ERROR("Failed to load magic num.");
        return -1;
    }

    *fmt = CONN_FMT_UNKNOWN;
    if (strncmp(flag.str, "    ", 4) == 0) {
        *fmt = CONN_FMT_TXT;
    } else if (VOCAB_MAGIC_NUM != flag.magic_num) {
        ST_ERROR("magic num wrong.");
        return -2;
    }

    if (vocab != NULL) {
        *vocab = NULL;
    }

    if (*fmt != CONN_FMT_TXT) {
        if (version >= 12) {
            if (fread(fmt, sizeof(connlm_fmt_t), 1, fp) != 1) {
                ST_ERROR("Failed to read fmt.");
                goto ERR;
            }
        } else {
            *fmt = CONN_FMT_BIN;
        }

        if (fread(&vocab_size, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to read vocab size.");
            return -1;
        }

        if (vocab_size <= 0) {
            if (fo_info != NULL) {
                fprintf(fo_info, "\n<VOCAB>: None\n");
            }
            return 0;
        }
    } else {
        if (st_readline(fp, "") != 0) {
            ST_ERROR("flag error.");
            return -1;
        }
        if (st_readline(fp, "<VOCAB>") != 0) {
            ST_ERROR("flag error.");
            return -1;
        }

        if (st_readline(fp, "Vocab size: %d", &vocab_size) != 1) {
            ST_ERROR("Failed to parse vocab_size.[%d]", vocab_size);
            return -1;
        }

        if (vocab_size <= 0) {
            if (fo_info != NULL) {
                fprintf(fo_info, "\n<VOCAB>: None\n");
            }
            return 0;
        }
    }

    if (vocab != NULL) {
        *vocab = (vocab_t *)st_malloc(sizeof(vocab_t));
        if (*vocab == NULL) {
            ST_ERROR("Failed to st_malloc vocab_t");
            goto ERR;
        }
        memset(*vocab, 0, sizeof(vocab_t));

        (*vocab)->vocab_size = vocab_size;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<VOCAB>\n");
        fprintf(fo_info, "Vocab size: %d\n", vocab_size);
    }

    return 0;

ERR:
    if (vocab != NULL) {
        safe_vocab_destroy(*vocab);
    }
    return -1;
}

int vocab_load_body(vocab_t *vocab, int version, FILE *fp, connlm_fmt_t fmt)
{
    int n;

    int i;

    ST_CHECK_PARAM(vocab == NULL || fp == NULL, -1);

    if (vocab->vocab_size <= 0) {
        return 0;
    }

    vocab->alphabet = NULL;
    vocab->cnts = NULL;

    if (connlm_fmt_is_bin(fmt)) {
        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to read magic num.");
            goto ERR;
        }

        if (n != -VOCAB_MAGIC_NUM) {
            ST_ERROR("Magic num error.");
            goto ERR;
        }

        vocab->alphabet = st_alphabet_load_from_bin(fp);
        if (vocab->alphabet == NULL) {
            ST_ERROR("Failed to st_alphabet_load_from_bin.");
            goto ERR;
        }

        vocab->cnts = (count_t *)st_malloc(sizeof(count_t)*vocab->vocab_size);
        if (vocab->cnts == NULL) {
            ST_ERROR("Failed to st_malloc cnts.");
            goto ERR;
        }

        if (fread(vocab->cnts, sizeof(count_t),
                    vocab->vocab_size, fp) != vocab->vocab_size) {
            ST_ERROR("Failed to read word cnts.");
            goto ERR;
        }
    } else {
        if (st_readline(fp, "<VOCAB-DATA>") != 0) {
            ST_ERROR("body flag error.");
            goto ERR;
        }

        vocab->alphabet = st_alphabet_load_from_txt(fp, vocab->vocab_size);
        if (vocab->alphabet == NULL) {
            ST_ERROR("Failed to st_alphabet_load_from_txt.");
            goto ERR;
        }

        vocab->cnts = (count_t *)st_malloc(sizeof(count_t)*vocab->vocab_size);
        if (vocab->cnts == NULL) {
            ST_ERROR("Failed to st_malloc cnts.");
            goto ERR;
        }

        if (st_readline(fp, "Word Counts:") != 0) {
            ST_ERROR("word cnts flag error.");
            goto ERR;
        }

        for (i = 0; i < vocab->vocab_size; i++) {
            if (st_readline(fp, "\t%*d\t%lu\n", vocab->cnts + i) != 1) {
                ST_ERROR("Failed to parse word cnts.");
                goto ERR;
            }
        }
    }

    return 0;
ERR:
    safe_st_alphabet_destroy(vocab->alphabet);
    safe_st_free(vocab->cnts);
    return -1;
}

int vocab_save_header(vocab_t *vocab, FILE *fp, connlm_fmt_t fmt)
{
    int sz;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (connlm_fmt_is_bin(fmt)) {
        if (fwrite(&VOCAB_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to write magic num.");
            return -1;
        }

        if (fwrite(&fmt, sizeof(connlm_fmt_t), 1, fp) != 1) {
            ST_ERROR("Failed to write fmt.");
            return -1;
        }

        if (vocab == NULL) {
            sz = 0;
            if (fwrite(&sz, sizeof(int), 1, fp) != 1) {
                ST_ERROR("Failed to write vocab size.");
                return -1;
            }

            return 0;
        }

        if (fwrite(&vocab->vocab_size, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to write vocab size.");
            return -1;
        }
    } else {
        if (fprintf(fp, "    \n<VOCAB>\n") < 0) {
            ST_ERROR("Failed to fprintf header.");
            return -1;
        }

        if (vocab == NULL) {
            if (fprintf(fp, "Vocab size: %d\n", 0) < 0) {
                ST_ERROR("Failed to fprintf vocab size.");
                return -1;
            }
            return 0;
        }

        if (fprintf(fp, "Vocab size: %d\n", vocab->vocab_size) < 0) {
            ST_ERROR("Failed to fprintf vocab size.");
            return -1;
        }
    }

    return 0;
}

int vocab_save_body(vocab_t *vocab, FILE *fp, connlm_fmt_t fmt)
{
    int n;
    int i;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (vocab == NULL) {
        return 0;
    }

    if (connlm_fmt_is_bin(fmt)) {
        n = -VOCAB_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_ERROR("Failed to write magic num.");
            return -1;
        }

        if (st_alphabet_save_bin(vocab->alphabet, fp) < 0) {
            ST_ERROR("Failed to st_alphabet_save_bin.");
            return -1;
        }

        if (fwrite(vocab->cnts, sizeof(count_t),
                    vocab->vocab_size, fp) != vocab->vocab_size) {
            ST_ERROR("Failed to write word cnts.");
            return -1;
        }
    } else {
        if (fprintf(fp, "<VOCAB-DATA>\n") < 0) {
            ST_ERROR("Failed to fprintf header.");
            return -1;
        }

        if (st_alphabet_save_txt(vocab->alphabet, fp) < 0) {
            ST_ERROR("Failed to st_alphabet_save_txt.");
            return -1;
        }

        if (fprintf(fp, "Word Counts:\n") < 0) {
            ST_ERROR("Failed to fprintf word count.");
            return -1;
        }
        for (i = 0; i < vocab->vocab_size; i++) {
            if (fprintf(fp, "\t%d\t%lu\n", i, vocab->cnts[i]) < 0) {
                ST_ERROR("Failed to fprintf cnt[%d]", i);
                return -1;
            }
        }
    }

    return 0;
}

typedef struct _word_info_t_ {
    int id;
    count_t cnt;
} word_info_t;

int word_info_comp(const void *elem1, const void *elem2, void *args)
{
    word_info_t *f = (word_info_t *)elem1;
    word_info_t *s = (word_info_t *)elem2;

    if (f->cnt < s->cnt) return  1;
    if (f->cnt > s->cnt) return -1;
    return strncmp(st_alphabet_get_label((st_alphabet_t *)args, f->id),
                st_alphabet_get_label((st_alphabet_t *)args, s->id),
                MAX_SYM_LEN);
}

static int vocab_sort(vocab_t *vocab, word_info_t *word_infos,
        vocab_learn_opt_t *lr_opt)
{
    st_alphabet_t *alphabet = NULL;
    count_t *cnts = NULL;

    char *word;
    int total_vocab_size;
    int a;

    ST_CHECK_PARAM(vocab == NULL || word_infos == NULL
            || lr_opt == NULL, -1);

    total_vocab_size = st_alphabet_get_label_num(vocab->alphabet);

    st_qsort(word_infos + UNK_ID + 1, total_vocab_size - UNK_ID - 1,
            sizeof(word_info_t), word_info_comp, vocab->alphabet);

    if (lr_opt->max_vocab_size > 0) {
        vocab->vocab_size = min(total_vocab_size, lr_opt->max_vocab_size);
    } else {
        vocab->vocab_size = total_vocab_size;
    }

    if (lr_opt->min_count > 0) {
        for (a = UNK_ID + 1; a < vocab->vocab_size; a++) {
            if (word_infos[a].cnt < lr_opt->min_count) {
                break;
            }
        }
        vocab->vocab_size = a;
    }

    // give all pruned words's count to <unk>
    for (a = vocab->vocab_size; a < total_vocab_size; a++) {
        word_infos[UNK_ID].cnt += word_infos[a].cnt;
    }

    vocab->vocab_size += 1/* <s> */;
    cnts = (count_t *)st_malloc(sizeof(count_t)*vocab->vocab_size);
    if (cnts == NULL) {
        ST_ERROR("Failed to st_malloc cnts");
        goto ERR;
    }

    alphabet = st_alphabet_create(vocab->vocab_size);
    if (alphabet == NULL) {
        ST_ERROR("Failed to st_alphabet_create alphabet.");
        goto ERR;
    }

    for (a = 0; a < vocab->vocab_size - 1; a++) {
        word = st_alphabet_get_label(vocab->alphabet, word_infos[a].id);
        if (word == NULL) {
            ST_ERROR("Failed to st_alphabet_get_label[%d].",
                       word_infos[a].id);
            goto ERR;
        }

        if (st_alphabet_add_label(alphabet, word) != a) {
            ST_ERROR("Failed to st_alphabet_add_label[%d/%s].", a, word);
            goto ERR;
        }
        cnts[a] = word_infos[a].cnt;
    }
    if (st_alphabet_add_label(alphabet, SENT_START) != a) {
        ST_ERROR("Failed to st_alphabet_add_label[%d/%s].", a, SENT_START);
        goto ERR;
    }
    cnts[a] = 0;

    safe_st_alphabet_destroy(vocab->alphabet);
    vocab->alphabet = alphabet;
    vocab->cnts = cnts;

    return 0;

  ERR:
    safe_st_alphabet_destroy(alphabet);
    safe_st_free(cnts);
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
                strncpy(word, SENT_END, word_len);
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

int vocab_load_learn_opt(vocab_learn_opt_t *lr_opt, st_opt_t *opt,
        const char *sec_name)
{
    unsigned long ul;

    ST_CHECK_PARAM(lr_opt == NULL || opt == NULL, -1);

    memset(lr_opt, 0, sizeof(vocab_learn_opt_t));

    ST_OPT_SEC_GET_INT(opt, sec_name, "MAX_VOCAB_SIZE",
        lr_opt->max_vocab_size, 0,
        "Maximum size of Vocabulary. 0 denotes no limit.");

    ST_OPT_SEC_GET_ULONG(opt, sec_name, "MAX_WORD_NUM", ul, 0,
        "Maximum number of words used to learn vocab. 0 denotes no limit.");
    lr_opt->max_word_num = (count_t)ul;

    ST_OPT_SEC_GET_INT(opt, sec_name, "MIN_COUNT",
        lr_opt->min_count, 0, "Mininum count for a word to be used "
        "in learning vocab. 0 denotes no limit.");

    return 0;

ST_OPT_ERR:
    return -1;
}

int vocab_learn(vocab_t *vocab, FILE *fp, vocab_learn_opt_t *lr_opt)
{
    char word[MAX_SYM_LEN];

    word_info_t *word_infos = NULL;
    count_t words = 0;
    int id;

    ST_CHECK_PARAM(vocab == NULL || fp == NULL || lr_opt == NULL, -1);

    word_infos = (word_info_t *)st_malloc(sizeof(word_info_t)
            * vocab->vocab_opt.max_alphabet_size);
    if (word_infos == NULL) {
        ST_ERROR("Failed to st_malloc word_infos.");
        goto ERR;
    }

    for (id = 0; id < vocab->vocab_opt.max_alphabet_size; id++) {
        word_infos[id].id = id;
        word_infos[id].cnt = 0;
    }

    words = 0;
    while (1) {
        if (vocab_read_word(word, MAX_SYM_LEN, fp) < 0) {
            ST_ERROR("Failed to vocab_read_word.");
            goto ERR;
        }

        if (feof(fp)) {
            break;
        }

        id = vocab_get_id(vocab, word);
        if (id == -1) {
            if (vocab->vocab_opt.wordlist[0] != '\0') {
                id = UNK_ID;
            } else {
                id = vocab_add_word(vocab, word);
                if (id < 0) {
                    ST_ERROR("Failed to vocab_add_word.");
                    goto ERR;
                }
            }
        }
        word_infos[id].cnt++;

        words++;
        if (lr_opt->max_word_num > 0 && words >= lr_opt->max_word_num) {
            break;
        }
    }

    if (vocab_sort(vocab, word_infos, lr_opt) < 0) {
        ST_ERROR("Failed to vocab_sort.");
        goto ERR;
    }

    ST_NOTICE("Words: " COUNT_FMT, words);
    ST_NOTICE("Vocab Size: %d", vocab->vocab_size);

    safe_st_free(word_infos);
    return 0;
ERR:
    safe_st_free(word_infos);
    return -1;
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

bool vocab_equal(vocab_t *vocab1, vocab_t *vocab2)
{
    char *ptr1, *ptr2;
    int i;

    ST_CHECK_PARAM(vocab1 == NULL || vocab2 == NULL, false);

    if (vocab1->vocab_size != vocab2->vocab_size) {
        return false;
    }

    for (i = 0; i < vocab1->vocab_size; i++) {
        ptr1 = vocab_get_word(vocab1, i);
        ptr2 = vocab_get_word(vocab2, i);

        if (ptr1 == NULL || ptr2 == NULL
                || strcmp(ptr1, ptr2) != 0) {
            return false;
        }
        // FIXME: Do we need to compare count?
        if (vocab1->cnts[i] != vocab2->cnts[i]) {
            return false;
        }
    }

    return true;
}

int vocab_save_syms(vocab_t *vocab, FILE *fp, bool add_eps)
{
    int i;
    int id;

    ST_CHECK_PARAM(vocab == NULL || fp == NULL, -1);

    id = 0;
    if (add_eps) {
        fprintf(fp, "%s\t%d\n", EPS, id);
        id++;
    }
    for (i = 0; i < vocab->vocab_size; i++) {
        fprintf(fp, "%s\t%d\n", vocab_get_word(vocab, i), id);
        id++;
    }

    return 0;
}
