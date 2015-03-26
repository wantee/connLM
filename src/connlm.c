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
#include <unistd.h>

#include <st_opt.h>
#include <st_io.h>

#include "connlm.h"

int connlm_param_load(connlm_param_t *connlm_param, 
        st_opt_t *opt, const char *sec_name)
{
    float f;

    ST_CHECK_PARAM(connlm_param == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_FLOAT(opt, sec_name, "LEARN_RATE", f, 0.1,
            "Learning rate");
    connlm_param->learn_rate = (real_t)f;

    ST_OPT_SEC_GET_FLOAT(opt, sec_name, "L1_PENALTY", f, 0.0,
            "L1 penalty (promote sparsity)");
    connlm_param->l1_penalty = (real_t)f;

    ST_OPT_SEC_GET_FLOAT(opt, sec_name, "L2_PENALTY", f, 0.0,
            "L2 penalty (weight decay)");
    connlm_param->l2_penalty = (real_t)f;

    ST_OPT_SEC_GET_FLOAT(opt, sec_name, "MOMENTUM", f, 0.0,
            "Momentum");
    connlm_param->momentum = (real_t)f;

    ST_OPT_SEC_GET_FLOAT(opt, sec_name, "GRADIENT_CUTOFF", f, 0.0,
            "Cutoff of gradient");
    connlm_param->gradient_cutoff = (real_t)f;

    return 0;
ST_OPT_ERR:
    return -1;
}

int connlm_output_opt_load(connlm_output_opt_t *output_opt, 
        st_opt_t *opt, const char *sec_name)
{
    ST_CHECK_PARAM(output_opt == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_INT(opt, sec_name, "CLASSE_SIZE",
            output_opt->class_size, 100, "Size of class layer");

    ST_OPT_SEC_GET_BOOL(opt, sec_name, "HS",
            output_opt->hs, true, "Hierarchical softmax");

    return 0;

ST_OPT_ERR:
    return -1;
}

static int connlm_load_train_opt(connlm_train_opt_t *train_opt, 
        st_opt_t *opt, const char *sec_name)
{
    char name[MAX_ST_CONF_LEN];

    ST_CHECK_PARAM(train_opt == NULL || opt == NULL, -1);

    if (sec_name == NULL || sec_name[0] == '\0') {
        snprintf(name, MAX_ST_CONF_LEN, "RNN");
    } else {
        snprintf(name, MAX_ST_CONF_LEN, "%s/RNN", sec_name);
    }
    if (rnn_load_opt(&train_opt->rnn_opt, opt, name) < 0) {
        ST_WARNING("Failed to rnn_load_opt.");
        goto ST_OPT_ERR;
    }

    if (sec_name == NULL || sec_name[0] == '\0') {
        snprintf(name, MAX_ST_CONF_LEN, "MAXENT");
    } else {
        snprintf(name, MAX_ST_CONF_LEN, "%s/MAXENT", sec_name);
    }
    if (maxent_load_opt(&train_opt->maxent_opt, opt, name) < 0) {
        ST_WARNING("Failed to maxent_load_opt.");
        goto ST_OPT_ERR;
    }

    if (sec_name == NULL || sec_name[0] == '\0') {
        snprintf(name, MAX_ST_CONF_LEN, "LBL");
    } else {
        snprintf(name, MAX_ST_CONF_LEN, "%s/LBL", sec_name);
    }
    if (lbl_load_opt(&train_opt->lbl_opt, opt, name) < 0) {
        ST_WARNING("Failed to lbl_load_opt.");
        goto ST_OPT_ERR;
    }

    if (sec_name == NULL || sec_name[0] == '\0') {
        snprintf(name, MAX_ST_CONF_LEN, "FFNN");
    } else {
        snprintf(name, MAX_ST_CONF_LEN, "%s/FFNN", sec_name);
    }
    if (ffnn_load_opt(&train_opt->ffnn_opt, opt, name) < 0) {
        ST_WARNING("Failed to ffnn_load_opt.");
        goto ST_OPT_ERR;
    }

    if (connlm_param_load(&train_opt->param, opt, sec_name) < 0) {
        ST_WARNING("Failed to connlm_param_load.");
        goto ST_OPT_ERR;
    }

    if (sec_name == NULL || sec_name[0] == '\0') {
        snprintf(name, MAX_ST_CONF_LEN, "OUTPUT");
    } else {
        snprintf(name, MAX_ST_CONF_LEN, "%s/OUTPUT", sec_name);
    }
    if (connlm_output_opt_load(&train_opt->output_opt, opt, name) < 0) {
        ST_WARNING("Failed to connlm_output_opt_load.");
        goto ST_OPT_ERR;
    }

    ST_OPT_SEC_GET_INT(opt, sec_name, "MAX_WORD_NUM",
            train_opt->max_word_num, 1000000, 
            "Maximum number of words in Vocabulary");

    ST_OPT_SEC_GET_BOOL(opt, sec_name, "BINARY",
            train_opt->binary, true,
            "Save file as binary format");
    ST_OPT_SEC_GET_BOOL(opt, sec_name, "REPORT_PROGRESS",
            train_opt->report_progress, false,
            "Report training progress");

    ST_OPT_SEC_GET_INT(opt, sec_name, "NUM_LINE_READ",
            train_opt->num_line_read, 10,
            "Number of lines read a time (in kilos)");
    train_opt->num_line_read *= 1000;
    ST_OPT_SEC_GET_BOOL(opt, sec_name, "SHUFFLE",
            train_opt->shuffle, true,
            "Shuffle after reading");

    return 0;

ST_OPT_ERR:
    return -1;
}

static int connlm_load_test_opt(connlm_test_opt_t *test_opt, 
        st_opt_t *opt, const char *sec_name)
{
    ST_CHECK_PARAM(test_opt == NULL || opt == NULL, -1);


    return 0;
}

int connlm_load_opt(connlm_opt_t *connlm_opt, 
        st_opt_t *opt, const char *sec_name)
{
    ST_CHECK_PARAM(connlm_opt == NULL || opt == NULL, -1);

    if (connlm_opt->mode == MODE_TRAIN) {
        if (connlm_load_train_opt(&connlm_opt->train_opt,
                    opt, sec_name) < 0) {
            ST_WARNING("Failed to connlm_load_train_opt");
            return -1;
        }
    } else if (connlm_opt->mode == MODE_TEST){
        if (connlm_load_test_opt(&connlm_opt->test_opt,
                    opt, sec_name) < 0) {
            ST_WARNING("Failed to connlm_load_test_opt");
            return -1;
        }
    } else {
        ST_WARNING("Unknown mode[%d]", connlm_opt->mode);
        return -1;
    }

    ST_OPT_SEC_GET_INT(opt, sec_name, "NUM_THREAD",
            connlm_opt->num_thread, 1, "Number of threads");
    ST_OPT_SEC_GET_INT(opt, sec_name, "RANDOM_SEED",
            connlm_opt->rand_seed, 1, "Random seed");
    ST_OPT_SEC_GET_INT(opt, sec_name, "MAX_WORD_PER_SENT",
            connlm_opt->max_word_per_sent, 256,
            "Maximum words in one sentence");

    return 0;

ST_OPT_ERR:
    return -1;
}

static int connlm_sort_vocab(connlm_t * connlm)
{
    word_info_t swap;
    st_alphabet_t *v = NULL;
    char *word;
    int a, b, max;

    ST_CHECK_PARAM(connlm == NULL, -1);

    for (a = 1; a < connlm->vocab_size; a++) {
        max = a;
        for (b = a + 1; b < connlm->vocab_size; b++) {
            if (connlm->word_infos[max].cnt < connlm->word_infos[b].cnt) {
                max = b;
            }
        }

        if (max == a) {
            continue;
        }

        swap = connlm->word_infos[max];
        connlm->word_infos[max] = connlm->word_infos[a];
        connlm->word_infos[a] = swap;
    }

    v = st_alphabet_create(connlm->vocab_size);
    if (v == NULL) {
        ST_WARNING("Failed to st_alphabet_create vocab.");
        return -1;
    }

    for (a = 0; a < connlm->vocab_size; a++) {
        word = st_alphabet_get_label(connlm->vocab, connlm->word_infos[a].id);
        if (word == NULL) {
            ST_WARNING("Failed to st_alphabet_get_label[%d].",
                       connlm->word_infos[a].id);
            goto ERR;
        }

        if (st_alphabet_add_label(v, word) < 0) {
            ST_WARNING("Failed to st_alphabet_add_label[%s].", word);
            goto ERR;
        }
    }

    safe_st_alphabet_destroy(connlm->vocab);
    connlm->vocab = v;

    return 0;

  ERR:
    safe_st_alphabet_destroy(v);
    return -1;
}

static int connlm_read_word(char *word, size_t word_len, FILE * fp)
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

int connlm_learn_vocab(connlm_t *connlm)
{
    char word[MAX_SYM_LEN];
    FILE *fp = NULL;
    connlm_train_opt_t *train_opt;

    int words = 0;
    int id;

    ST_CHECK_PARAM(connlm == NULL, -1);

    train_opt = &connlm->connlm_opt.train_opt;
    fp = fopen(train_opt->train_file, "rb");
    if (fp == NULL) {
        ST_WARNING("Failed to open train file[%s].", 
                train_opt->train_file);
        return -1;
    }

    connlm->vocab = st_alphabet_create(train_opt->max_word_num);
    if (connlm->vocab == NULL) {
        ST_WARNING("Failed to st_alphabet_create.");
        goto ERR;
    }

    connlm->word_infos = (word_info_t *) malloc(sizeof(word_info_t) 
            * train_opt->max_word_num);
    if (connlm->word_infos == NULL) {
        ST_WARNING("Failed to malloc word_infos.");
        goto ERR;
    }
    memset(connlm->word_infos, 0, sizeof(word_info_t) 
            * train_opt->max_word_num);

    if (st_alphabet_add_label(connlm->vocab, "</s>") != 0) {
        ST_WARNING("Failed to st_alphabet_add_label.");
        goto ERR;
    }
    connlm->word_infos[0].id = 0;
    connlm->word_infos[0].cnt = 0;

    words = 0;
    while (1) {
        if (connlm_read_word(word, MAX_SYM_LEN, fp) < 0) {
            ST_WARNING("Failed to connlm_read_word.");
            goto ERR;
        }

        if (feof(fp)) {
            break;
        }

        words++;

        id = st_alphabet_get_index(connlm->vocab, word);
        if (id == -1) {
            id = st_alphabet_add_label(connlm->vocab, word);
            if (id < 0) {
                ST_WARNING("Failed to st_alphabet_add_label.");
                goto ERR;
            }

            connlm->word_infos[id].id = id;
            connlm->word_infos[id].cnt = 1;
        } else {
            connlm->word_infos[id].cnt++;
        }
    }

    safe_fclose(fp);

    connlm->vocab_size = st_alphabet_get_label_num(connlm->vocab);
    connlm_sort_vocab(connlm);

    return 0;

  ERR:
    safe_fclose(fp);
    safe_st_alphabet_destroy(connlm->vocab);
    safe_free(connlm->word_infos);
    return -1;
}

static int connlm_get_egs(connlm_t *connlm)
{
    char line[MAX_LINE_LEN];
    char word[MAX_LINE_LEN];

    connlm_train_opt_t *train_opt;
    int *eg;
    char *p;

    int num_sents;
    int i;
    int w;
    int max_word_per_sent;

    ST_CHECK_PARAM(connlm == NULL, -1);

    train_opt = &connlm->connlm_opt.train_opt;
    max_word_per_sent = connlm->connlm_opt.max_word_per_sent;

    if (train_opt->shuffle) {
        for (i = 0; i < train_opt->num_line_read; i++) {
            connlm->shuffle_buf[i] = i;
        }

        st_shuffle_rand(connlm->shuffle_buf, train_opt->num_line_read,
                &connlm->random);
        for (i = 0; i < train_opt->num_line_read; i++) {
            printf("%d\n", connlm->shuffle_buf[i]);
        }
    }

    for (i = 0; i < train_opt->num_line_read; i++) {
        connlm->egs[i*max_word_per_sent] = -1;
    }

    num_sents = 0;
    while (fgets(line, MAX_LINE_LEN, connlm->text_fp))
    {
        remove_newline(line);

        if (line[0] == '\0') {
            continue;
        }

        eg = connlm->egs + max_word_per_sent * connlm->shuffle_buf[num_sents];
        p = line;
        i = 0;
        w = 0;
        while (*p != '\0') {
            if (*p == ' ' || *p == '\t') {
                if (i > 0) {
                    word[i] = '\0';
                    eg[w] = st_alphabet_get_index(connlm->vocab, word);
                    if (eg[w] < 0) {
                        ST_WARNING("Failed to st_alphabet_get_index "
                                "for word[%s]", word);
                        goto SKIP;
                    }

                    w++;
                    if (w > max_word_per_sent) {
                        goto SKIP;
                    }
                    i = 0;
                }
                p++;
                continue;
            }

            word[i] = *p;
            i++;
            while (*p != ' ' && *p != '\t') {
            }
        }

        num_sents++;
        if (num_sents >= train_opt->num_line_read) {
            break;
        }

        continue;
SKIP:
        eg[0] = -1;
    }

    return num_sents;
}

void connlm_destroy(connlm_t *connlm)
{
    if (connlm == NULL) {
        return;
    }

    safe_st_alphabet_destroy(connlm->vocab);
    safe_free(connlm->word_infos);

    safe_st_fclose(connlm->text_fp);
    safe_free(connlm->egs);
    safe_free(connlm->shuffle_buf);

    safe_rnn_destroy(connlm->rnn);
    safe_maxent_destroy(connlm->maxent);
    safe_lbl_destroy(connlm->lbl);
    safe_ffnn_destroy(connlm->ffnn);
}

connlm_t *connlm_create(connlm_opt_t *connlm_opt)
{
    connlm_t *connlm = NULL;
    connlm_train_opt_t *train_opt;
    connlm_test_opt_t *test_opt;

    ST_CHECK_PARAM(connlm_opt == NULL, NULL);

    connlm = (connlm_t *)malloc(sizeof(connlm_t));
    if (connlm == NULL) {
        ST_WARNING("Failed to malloc connlm_t");
        goto ERR;
    }

    connlm->connlm_opt = *connlm_opt;
    connlm->random = connlm_opt->rand_seed;

    if (connlm_opt->mode == MODE_TRAIN) {
        train_opt = &connlm_opt->train_opt;

        connlm->text_fp = st_fopen(train_opt->train_file, "rb");
        if (connlm->text_fp == NULL) {
            ST_WARNING("Failed to open train file[%s]",
                    train_opt->train_file);
            goto ERR;
        }

        connlm->egs = (int *)malloc(sizeof(int)
                *train_opt->num_line_read*connlm_opt->max_word_per_sent);
        if (connlm->egs == NULL) {
            ST_WARNING("Failed to malloc egs");
            goto ERR;
        }

        if (train_opt->shuffle) {
            connlm->shuffle_buf = (int *)malloc(sizeof(int)
                    *train_opt->num_line_read);
            if (connlm->shuffle_buf == NULL) {
                ST_WARNING("Failed to malloc shuffle_buf");
                goto ERR;
            }
        }

    } else if (connlm_opt->mode == MODE_TEST) {
        test_opt = &connlm_opt->test_opt;
    } else {
        ST_WARNING("Unknown mode[%d]", connlm_opt->mode);
        goto ERR;
    }

    return connlm;

ERR:
    safe_connlm_destroy(connlm);
    return NULL;
}

int connlm_train(connlm_t *connlm)
{
    ST_CHECK_PARAM(connlm == NULL, -1);

    if (connlm_get_egs(connlm) < 0) {
        ST_WARNING("Failed to connlm_get_egs.");
        return -1;
    }

    return 0;
}

int connlm_test(connlm_t *connlm)
{
    ST_CHECK_PARAM(connlm == NULL, -1);

    return 0;
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */

