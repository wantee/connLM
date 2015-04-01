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

#include <st_log.h>
#include <st_opt.h>
#include <st_io.h>

#include "connlm.h"

static const int CONNLM_MAGIC_NUM = 626140498;

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

int connlm_load_opt(connlm_opt_t *connlm_opt, 
        st_opt_t *opt, const char *sec_name)
{
    char name[MAX_ST_CONF_LEN];

    ST_CHECK_PARAM(connlm_opt == NULL || opt == NULL, -1);

    if (sec_name == NULL || sec_name[0] == '\0') {
        snprintf(name, MAX_ST_CONF_LEN, "RNN");
    } else {
        snprintf(name, MAX_ST_CONF_LEN, "%s/RNN", sec_name);
    }
    if (rnn_load_opt(&connlm_opt->rnn_opt, opt, name) < 0) {
        ST_WARNING("Failed to rnn_load_opt.");
        goto ST_OPT_ERR;
    }

    if (sec_name == NULL || sec_name[0] == '\0') {
        snprintf(name, MAX_ST_CONF_LEN, "MAXENT");
    } else {
        snprintf(name, MAX_ST_CONF_LEN, "%s/MAXENT", sec_name);
    }
    if (maxent_load_opt(&connlm_opt->maxent_opt, opt, name) < 0) {
        ST_WARNING("Failed to maxent_load_opt.");
        goto ST_OPT_ERR;
    }

    if (sec_name == NULL || sec_name[0] == '\0') {
        snprintf(name, MAX_ST_CONF_LEN, "LBL");
    } else {
        snprintf(name, MAX_ST_CONF_LEN, "%s/LBL", sec_name);
    }
    if (lbl_load_opt(&connlm_opt->lbl_opt, opt, name) < 0) {
        ST_WARNING("Failed to lbl_load_opt.");
        goto ST_OPT_ERR;
    }

    if (sec_name == NULL || sec_name[0] == '\0') {
        snprintf(name, MAX_ST_CONF_LEN, "FFNN");
    } else {
        snprintf(name, MAX_ST_CONF_LEN, "%s/FFNN", sec_name);
    }
    if (ffnn_load_opt(&connlm_opt->ffnn_opt, opt, name) < 0) {
        ST_WARNING("Failed to ffnn_load_opt.");
        goto ST_OPT_ERR;
    }

    if (connlm_param_load(&connlm_opt->param, opt, sec_name) < 0) {
        ST_WARNING("Failed to connlm_param_load.");
        goto ST_OPT_ERR;
    }

    if (sec_name == NULL || sec_name[0] == '\0') {
        snprintf(name, MAX_ST_CONF_LEN, "OUTPUT");
    } else {
        snprintf(name, MAX_ST_CONF_LEN, "%s/OUTPUT", sec_name);
    }
    if (connlm_output_opt_load(&connlm_opt->output_opt, opt, name) < 0) {
        ST_WARNING("Failed to connlm_output_opt_load.");
        goto ST_OPT_ERR;
    }

    ST_OPT_SEC_GET_INT(opt, sec_name, "NUM_THREAD",
            connlm_opt->num_thread, 1, "Number of threads");
    ST_OPT_SEC_GET_INT(opt, sec_name, "RANDOM_SEED",
            connlm_opt->rand_seed, 1, "Random seed");
    ST_OPT_SEC_GET_INT(opt, sec_name, "MAX_WORD_PER_SENT",
            connlm_opt->max_word_per_sent, 256,
            "Maximum words in one sentence");

    ST_OPT_SEC_GET_BOOL(opt, sec_name, "REPORT_PROGRESS",
            connlm_opt->report_progress, false,
            "Report training progress");

    ST_OPT_SEC_GET_INT(opt, sec_name, "NUM_LINE_READ",
            connlm_opt->num_line_read, 10,
            "Number of lines read a time (in kilos)");
    connlm_opt->num_line_read *= 1000;
    ST_OPT_SEC_GET_BOOL(opt, sec_name, "SHUFFLE",
            connlm_opt->shuffle, true,
            "Shuffle after reading");

    return 0;

ST_OPT_ERR:
    return -1;
}

int connlm_setup_train(connlm_t *connlm, connlm_opt_t *connlm_opt,
        const char *train_file)
{
    ST_CHECK_PARAM(connlm == NULL || connlm_opt == NULL
            || train_file == NULL, -1);

    connlm->connlm_opt = *connlm_opt;

    connlm->random = connlm_opt->rand_seed;

    connlm->text_fp = st_fopen(train_file, "rb");
    if (connlm->text_fp == NULL) {
        ST_WARNING("Failed to open train file[%s]", train_file);
        return -1;
    }

    connlm->egs = (int *)malloc(sizeof(int)
            *connlm_opt->num_line_read*connlm_opt->max_word_per_sent);
    if (connlm->egs == NULL) {
        ST_WARNING("Failed to malloc egs");
        return -1;
    }

    if (connlm_opt->shuffle) {
        connlm->shuffle_buf = (int *)malloc(sizeof(int)
                *connlm_opt->num_line_read);
        if (connlm->shuffle_buf == NULL) {
            ST_WARNING("Failed to malloc shuffle_buf");
            return -1;
        }
    }


return 0;
}

static int connlm_get_egs(connlm_t *connlm)
{
    char line[MAX_LINE_LEN];
    char word[MAX_LINE_LEN];

    connlm_opt_t *connlm_opt;
    int *eg;
    char *p;

    int num_sents;
    int i;
    int w;
    int max_word_per_sent;

    ST_CHECK_PARAM(connlm == NULL, -1);

    connlm_opt = &connlm->connlm_opt;
    max_word_per_sent = connlm->connlm_opt.max_word_per_sent;

    if (connlm_opt->shuffle) {
        for (i = 0; i < connlm_opt->num_line_read; i++) {
            connlm->shuffle_buf[i] = i;
        }

        st_shuffle_r(connlm->shuffle_buf, connlm_opt->num_line_read,
                &connlm->random);
    }

    for (i = 0; i < connlm_opt->num_line_read; i++) {
        connlm->egs[i*max_word_per_sent] = -1;
    }

    num_sents = 0;
    while (fgets(line, MAX_LINE_LEN, connlm->text_fp)) {
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
                    eg[w] = vocab_get_id(connlm->vocab, word);
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
            } else {
                word[i] = *p;
                i++;
            }
            p++;
            while (*p == ' ' && *p == '\t') {
                p++;
            }
        }

        num_sents++;
        if (num_sents >= connlm_opt->num_line_read) {
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

    safe_vocab_destroy(connlm->vocab);

    safe_st_fclose(connlm->text_fp);
    safe_free(connlm->egs);
    safe_free(connlm->shuffle_buf);

    safe_rnn_destroy(connlm->rnn);
    safe_maxent_destroy(connlm->maxent);
    safe_lbl_destroy(connlm->lbl);
    safe_ffnn_destroy(connlm->ffnn);
}

connlm_t *connlm_new(vocab_t *vocab, rnn_t *rnn, maxent_t *maxent,
        lbl_t* lbl, ffnn_t *ffnn)
{
    connlm_t *connlm = NULL;

    ST_CHECK_PARAM(vocab == NULL && rnn == NULL && maxent == NULL
            && lbl == NULL && ffnn == NULL, NULL);

    connlm = (connlm_t *)malloc(sizeof(connlm_t));
    if (connlm == NULL) {
        ST_WARNING("Failed to malloc connlm_t");
        goto ERR;
    }
    memset(connlm, 0, sizeof(connlm_t));

    if (vocab != NULL) {
        connlm->vocab = vocab_dup(vocab);
        if (connlm->vocab == NULL) {
            ST_WARNING("Failed to vocab_dup.");
            goto ERR;
        }
    }

    if (rnn != NULL) {
        connlm->rnn = rnn_dup(rnn);
        if (connlm->rnn == NULL) {
            ST_WARNING("Failed to rnn_dup.");
            goto ERR;
        }
    }

    if (maxent != NULL) {
        connlm->maxent = maxent_dup(maxent);
        if (connlm->maxent == NULL) {
            ST_WARNING("Failed to maxent_dup.");
            goto ERR;
        }
    }

    if (lbl != NULL) {
        connlm->lbl = lbl_dup(lbl);
        if (connlm->lbl == NULL) {
            ST_WARNING("Failed to lbl_dup.");
            goto ERR;
        }
    }

    if (ffnn != NULL) {
        connlm->ffnn = ffnn_dup(ffnn);
        if (connlm->ffnn == NULL) {
            ST_WARNING("Failed to ffnn_dup.");
            goto ERR;
        }
    }

    return connlm;

ERR:
    safe_connlm_destroy(connlm);
    return NULL;
}

static int connlm_load_header(connlm_t *connlm, FILE *fp,
        bool *binary, FILE *fo_info)
{
    char line[MAX_LINE_LEN];
    int version;
    bool b;

    union {
        char str[4];
        int magic_num;
    } flag;

    ST_CHECK_PARAM((connlm == NULL && fo_info == NULL) || fp == NULL
            || binary == NULL, -1);

    if (fread(&flag.magic_num, sizeof(int), 1, fp) != 1) {
        ST_WARNING("Failed to load magic num.");
        return -1;
    }

    if (strncmp(flag.str, "    ", 4) == 0) {
        *binary = false;
    } else if (CONNLM_MAGIC_NUM != flag.magic_num) {
        ST_WARNING("magic num wrong.");
        return -2;
    } else {
        *binary = true;
    }

    if (*binary) {
        if (fread(&version, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read version.");
            return -1;
        }

        if (version > CONNLM_FILE_VERSION) {
            ST_WARNING("Too high file versoin[%d], "
                    "please update connlm toolkit");
            return -1;
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
        
        if (strncmp(line, "<CONNLM>", 8) != 0) {
            ST_WARNING("flag error.[%s]", line);
            return -1;
        }

        if (fgets(line, MAX_LINE_LEN, fp) == NULL) {
            ST_WARNING("Failed to read version.");
            return -1;
        }
        if (sscanf(line, "Version: %d", &version) != 1) {
            ST_WARNING("Failed to parse version.");
            return -1;
        }

        if (version > CONNLM_FILE_VERSION) {
            ST_WARNING("Too high file versoin[%d], "
                    "please update connlm toolkit");
            return -1;
        }
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "<CONNLM>\n");
        fprintf(fo_info, "Version: %d\n", version);
        fprintf(fo_info, "Binary: %s\n", bool2str(*binary));
    }

    b = *binary;

    if (vocab_load_header((connlm == NULL) ? NULL : &connlm->vocab,
                fp, binary, fo_info) < 0) {
        ST_WARNING("Failed to vocab_load_header.");
        return -1;
    }
    if (*binary != b) {
        ST_WARNING("Both binary and text format in one file.");
        return -1;
    }

    if (rnn_load_header((connlm == NULL) ? NULL : &connlm->rnn,
                fp, binary, fo_info) < 0) {
        ST_WARNING("Failed to rnn_load_header.");
        return -1;
    }
    if (*binary != b) {
        ST_WARNING("Both binary and text format in one file.");
        return -1;
    }

    if (maxent_load_header((connlm == NULL) ? NULL : &connlm->maxent,
                fp, binary, fo_info) < 0) {
        ST_WARNING("Failed to maxent_load_header.");
        return -1;
    }
    if (*binary != b) {
        ST_WARNING("Both binary and text format in one file.");
        return -1;
    }

    if (lbl_load_header((connlm == NULL) ? NULL : &connlm->lbl,
                fp, binary, fo_info) < 0) {
        ST_WARNING("Failed to lbl_load_header.");
        return -1;
    }
    if (*binary != b) {
        ST_WARNING("Both binary and text format in one file.");
        return -1;
    }

    if (ffnn_load_header((connlm == NULL) ? NULL : &connlm->ffnn,
                fp, binary, fo_info) < 0) {
        ST_WARNING("Failed to ffnn_load_header.");
        return -1;
    }
    if (*binary != b) {
        ST_WARNING("Both binary and text format in one file.");
        return -1;
    }

    if (fo_info != NULL) {
        fflush(fo_info);
    }

    return 0;
}

static int connlm_load_body(connlm_t *connlm, FILE *fp, bool binary) 
{
    ST_CHECK_PARAM(connlm == NULL || fp == NULL, -1);

    if (connlm->vocab != NULL) {
        if (vocab_load_body(connlm->vocab, fp, binary) < 0) {
            ST_WARNING("Failed to vocab_load_body.");
            return -1;
        }
    }

    if (connlm->rnn != NULL) {
        if (rnn_load_body(connlm->rnn, fp, binary) < 0) {
            ST_WARNING("Failed to rnn_load_body.");
            return -1;
        }
    }

    if (connlm->maxent != NULL) {
        if (maxent_load_body(connlm->maxent, fp, binary) < 0) {
            ST_WARNING("Failed to maxent_load_body.");
            return -1;
        }
    }

    if (connlm->lbl != NULL) {
        if (lbl_load_body(connlm->lbl, fp, binary) < 0) {
            ST_WARNING("Failed to lbl_load_body.");
            return -1;
        }
    }

    if (connlm->ffnn != NULL) {
        if (ffnn_load_body(connlm->ffnn, fp, binary) < 0) {
            ST_WARNING("Failed to ffnn_load_body.");
            return -1;
        }
    }

    return 0;
}

connlm_t* connlm_load(FILE *fp)
{
    connlm_t *connlm = NULL;
    bool binary;

    ST_CHECK_PARAM(fp == NULL, NULL);

    connlm = (connlm_t *)malloc(sizeof(connlm_t));
    if (connlm == NULL) {
        ST_WARNING("Failed to malloc connlm_t");
        goto ERR;
    }
    memset(connlm, 0, sizeof(connlm_t));

    if (connlm_load_header(connlm, fp, &binary, NULL) < 0) {
        ST_WARNING("Failed to connlm_load_header.");
        goto ERR;
    }

    if (connlm_load_body(connlm, fp, binary) < 0) {
        ST_WARNING("Failed to connlm_load_body.");
        goto ERR;
    }

    return connlm;

ERR:
    safe_connlm_destroy(connlm);
    return NULL;
}

int connlm_print_info(FILE *fp, FILE *fo_info)
{
    bool binary;

    ST_CHECK_PARAM(fp == NULL || fo_info == NULL, -1);

    if (connlm_load_header(NULL, fp, &binary, fo_info) < 0) {
        ST_WARNING("Failed to connlm_load_header.");
        return -1;
    }

    return 0;
}

static int connlm_save_header(connlm_t *connlm, FILE *fp, bool binary)
{
    int v;

    ST_CHECK_PARAM(connlm == NULL || fp == NULL, -1);

    if (binary) {
        if (fwrite(&CONNLM_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        v = CONNLM_FILE_VERSION;
        if (fwrite(&v, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write version.");
            return -1;
        }
    } else {
        fprintf(fp, "    \n<CONNLM>\n");
        fprintf(fp, "Version: %d\n", CONNLM_FILE_VERSION);
    }

    if (vocab_save_header(connlm->vocab, fp, binary) < 0) {
        ST_WARNING("Failed to vocab_save_header.");
        return -1;
    }

    if (rnn_save_header(connlm->rnn, fp, binary) < 0) {
        ST_WARNING("Failed to rnn_save_header.");
        return -1;
    }

    if (maxent_save_header(connlm->maxent, fp, binary) < 0) {
        ST_WARNING("Failed to maxent_save_header.");
        return -1;
    }

    if (lbl_save_header(connlm->lbl, fp, binary) < 0) {
        ST_WARNING("Failed to lbl_save_header.");
        return -1;
    }

    if (ffnn_save_header(connlm->ffnn, fp, binary) < 0) {
        ST_WARNING("Failed to ffnn_save_header.");
        return -1;
    }

    return 0;
}

static int connlm_save_body(connlm_t *connlm, FILE *fp, bool binary) 
{
    ST_CHECK_PARAM(connlm == NULL || fp == NULL, -1);

    if (connlm->vocab != NULL) {
        if (vocab_save_body(connlm->vocab, fp, binary) < 0) {
            ST_WARNING("Failed to vocab_save_body.");
            return -1;
        }
    }

    if (connlm->rnn != NULL) {
        if (rnn_save_body(connlm->rnn, fp, binary) < 0) {
            ST_WARNING("Failed to rnn_save_body.");
            return -1;
        }
    }

    if (connlm->maxent != NULL) {
        if (maxent_save_body(connlm->maxent, fp, binary) < 0) {
            ST_WARNING("Failed to maxent_save_body.");
            return -1;
        }
    }

    if (connlm->lbl != NULL) {
        if (lbl_save_body(connlm->lbl, fp, binary) < 0) {
            ST_WARNING("Failed to lbl_save_body.");
            return -1;
        }
    }

    if (connlm->ffnn != NULL) {
        if (ffnn_save_body(connlm->ffnn, fp, binary) < 0) {
            ST_WARNING("Failed to ffnn_save_body.");
            return -1;
        }
    }

    return 0;
}

int connlm_save(connlm_t *connlm, FILE *fp, bool binary)
{
    ST_CHECK_PARAM(connlm == NULL || fp == NULL, -1);

    if (connlm_save_header(connlm, fp, binary) < 0) {
        ST_WARNING("Failed to connlm_save_header.");
        return -1;
    }

    if (connlm_save_body(connlm, fp, binary) < 0) {
        ST_WARNING("Failed to connlm_save_body.");
        return -1;
    }

    return 0;
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

