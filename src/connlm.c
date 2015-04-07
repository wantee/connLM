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
#include <math.h>

#include <st_log.h>
#include <st_opt.h>
#include <st_io.h>

#include "utils.h"
#include "connlm.h"

static const int CONNLM_MAGIC_NUM = 626140498;

int connlm_load_model_opt(connlm_model_opt_t *model_opt, 
        st_opt_t *opt, const char *sec_name)
{
    char name[MAX_ST_CONF_LEN];

    ST_CHECK_PARAM(model_opt == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_INT(opt, sec_name, "RANDOM_SEED",
            model_opt->rand_seed, 1, "Random seed");

    if (sec_name == NULL || sec_name[0] == '\0') {
        snprintf(name, MAX_ST_CONF_LEN, "RNN");
    } else {
        snprintf(name, MAX_ST_CONF_LEN, "%s/RNN", sec_name);
    }
    if (rnn_load_model_opt(&model_opt->rnn_opt, opt, name) < 0) {
        ST_WARNING("Failed to rnn_load_model_opt.");
        goto ST_OPT_ERR;
    }

    if (sec_name == NULL || sec_name[0] == '\0') {
        snprintf(name, MAX_ST_CONF_LEN, "MAXENT");
    } else {
        snprintf(name, MAX_ST_CONF_LEN, "%s/MAXENT", sec_name);
    }
    if (maxent_load_model_opt(&model_opt->maxent_opt, opt, name) < 0) {
        ST_WARNING("Failed to maxent_load_model_opt.");
        goto ST_OPT_ERR;
    }

    if (sec_name == NULL || sec_name[0] == '\0') {
        snprintf(name, MAX_ST_CONF_LEN, "LBL");
    } else {
        snprintf(name, MAX_ST_CONF_LEN, "%s/LBL", sec_name);
    }
    if (lbl_load_model_opt(&model_opt->lbl_opt, opt, name) < 0) {
        ST_WARNING("Failed to lbl_load_model_opt.");
        goto ST_OPT_ERR;
    }

    if (sec_name == NULL || sec_name[0] == '\0') {
        snprintf(name, MAX_ST_CONF_LEN, "FFNN");
    } else {
        snprintf(name, MAX_ST_CONF_LEN, "%s/FFNN", sec_name);
    }
    if (ffnn_load_model_opt(&model_opt->ffnn_opt, opt, name) < 0) {
        ST_WARNING("Failed to ffnn_load_model_opt.");
        goto ST_OPT_ERR;
    }

    return 0;

ST_OPT_ERR:
    return -1;
}

int connlm_load_train_opt(connlm_train_opt_t *train_opt, 
        st_opt_t *opt, const char *sec_name)
{
    char name[MAX_ST_CONF_LEN];

    ST_CHECK_PARAM(train_opt == NULL || opt == NULL, -1);

    if (param_load(&train_opt->param, opt, sec_name, NULL) < 0) {
        ST_WARNING("Failed to param_load.");
        goto ST_OPT_ERR;
    }

    ST_OPT_SEC_GET_INT(opt, sec_name, "NUM_THREAD",
            train_opt->num_thread, 1, "Number of threads");
    ST_OPT_SEC_GET_INT(opt, sec_name, "RANDOM_SEED",
            train_opt->rand_seed, 1, "Random seed");
    ST_OPT_SEC_GET_INT(opt, sec_name, "MAX_WORD_PER_SENT",
            train_opt->max_word_per_sent, 256,
            "Maximum words in one sentence");

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

    if (sec_name == NULL || sec_name[0] == '\0') {
        snprintf(name, MAX_ST_CONF_LEN, "RNN");
    } else {
        snprintf(name, MAX_ST_CONF_LEN, "%s/RNN", sec_name);
    }
    if (rnn_load_train_opt(&train_opt->rnn_opt, opt, name,
                &train_opt->param) < 0) {
        ST_WARNING("Failed to rnn_load_train_opt.");
        goto ST_OPT_ERR;
    }

    if (sec_name == NULL || sec_name[0] == '\0') {
        snprintf(name, MAX_ST_CONF_LEN, "MAXENT");
    } else {
        snprintf(name, MAX_ST_CONF_LEN, "%s/MAXENT", sec_name);
    }
    if (maxent_load_train_opt(&train_opt->maxent_opt, opt, name,
                &train_opt->param) < 0) {
        ST_WARNING("Failed to maxent_load_train_opt.");
        goto ST_OPT_ERR;
    }

    if (sec_name == NULL || sec_name[0] == '\0') {
        snprintf(name, MAX_ST_CONF_LEN, "LBL");
    } else {
        snprintf(name, MAX_ST_CONF_LEN, "%s/LBL", sec_name);
    }
    if (lbl_load_train_opt(&train_opt->lbl_opt, opt, name,
                &train_opt->param) < 0) {
        ST_WARNING("Failed to lbl_load_train_opt.");
        goto ST_OPT_ERR;
    }

    if (sec_name == NULL || sec_name[0] == '\0') {
        snprintf(name, MAX_ST_CONF_LEN, "FFNN");
    } else {
        snprintf(name, MAX_ST_CONF_LEN, "%s/FFNN", sec_name);
    }
    if (ffnn_load_train_opt(&train_opt->ffnn_opt, opt, name,
                &train_opt->param) < 0) {
        ST_WARNING("Failed to ffnn_load_train_opt.");
        goto ST_OPT_ERR;
    }

    return 0;

ST_OPT_ERR:
    return -1;
}

int connlm_load_test_opt(connlm_test_opt_t *test_opt, 
        st_opt_t *opt, const char *sec_name)
{
    float f;

    ST_CHECK_PARAM(test_opt == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_INT(opt, sec_name, "NUM_THREAD",
            test_opt->num_thread, 1, "Number of threads");
    ST_OPT_SEC_GET_INT(opt, sec_name, "MAX_WORD_PER_SENT",
            test_opt->max_word_per_sent, 256,
            "Maximum words in one sentence");

    ST_OPT_SEC_GET_INT(opt, sec_name, "NUM_LINE_READ",
            test_opt->num_line_read, 10,
            "Number of lines read a time (in kilos)");
    test_opt->num_line_read *= 1000;

    ST_OPT_SEC_GET_FLOAT(opt, sec_name, "OOV_PENALTY",
            f, -8, "Penalty of OOV");
    test_opt->oov_penalty = (real_t)f;

    return 0;

ST_OPT_ERR:
    return -1;
}

void connlm_destroy(connlm_t *connlm)
{
    if (connlm == NULL) {
        return;
    }


    safe_st_fclose(connlm->text_fp);
    safe_free(connlm->egs);
    safe_free(connlm->shuffle_buf);

    safe_vocab_destroy(connlm->vocab);
    safe_output_destroy(connlm->output);
    safe_rnn_destroy(connlm->rnn);
    safe_maxent_destroy(connlm->maxent);
    safe_lbl_destroy(connlm->lbl);
    safe_ffnn_destroy(connlm->ffnn);
}

int connlm_init(connlm_t *connlm, connlm_model_opt_t *model_opt)
{
    ST_CHECK_PARAM(connlm == NULL || model_opt == NULL, -1);

    connlm->model_opt = *model_opt;
    srand(model_opt->rand_seed);

    if (rnn_init(&connlm->rnn, &model_opt->rnn_opt,
            connlm->output) < 0) {
        ST_WARNING("Failed to rnn_init.");
        goto ERR;
    }

    if (maxent_init(&connlm->maxent, &model_opt->maxent_opt,
                connlm->output) < 0) {
        ST_WARNING("Failed to maxent_init.");
        goto ERR;
    }

    if (lbl_init(&connlm->lbl, &model_opt->lbl_opt, connlm->output) < 0) {
        ST_WARNING("Failed to lbl_init.");
        goto ERR;
    }

    if (ffnn_init(&connlm->ffnn, &model_opt->ffnn_opt,
                connlm->output) < 0) {
        ST_WARNING("Failed to ffnn_init.");
        goto ERR;
    }

    return 0;

ERR:
    safe_rnn_destroy(connlm->rnn);
    safe_maxent_destroy(connlm->maxent);
    safe_lbl_destroy(connlm->lbl);
    safe_ffnn_destroy(connlm->ffnn);
    return -1;
}

connlm_t *connlm_new(vocab_t *vocab, output_t *output,
        rnn_t *rnn, maxent_t *maxent, lbl_t* lbl, ffnn_t *ffnn)
{
    connlm_t *connlm = NULL;

    ST_CHECK_PARAM(vocab == NULL && output == NULL 
            && rnn == NULL && maxent == NULL
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

    if (output != NULL) {
        connlm->output = output_dup(output);
        if (connlm->output == NULL) {
            ST_WARNING("Failed to output_dup.");
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
    bool b;

    union {
        char str[4];
        int magic_num;
    } flag;

    int version;
    int real_size;

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

        if (fread(&real_size, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read real size.");
            return -1;
        }

        if (real_size != sizeof(real_t)) {
            ST_WARNING("Real type not match. Please recompile toolkit");
            return -1;
        }
    } else {
        if (st_readline(fp, "    ") != 0) {
            ST_WARNING("Failed to read tag.");
            return -1;
        }

        if (st_readline(fp, "<CONNLM>") != 0) {
            ST_WARNING("tag error");
            return -1;
        }

        if (st_readline(fp, "Version: %d", &version) != 1) {
            ST_WARNING("Failed to read version.");
            return -1;
        }

        if (version > CONNLM_FILE_VERSION) {
            ST_WARNING("Too high file versoin[%d], "
                    "please update connlm toolkit");
            return -1;
        }

        if (st_readline(fp, "Real type: %s", line) != 1) {
            ST_WARNING("Failed to read real type.");
            return -1;
        }

        if (strcasecmp(line, "double") == 0) {
            real_size = sizeof(double);
        } else {
            real_size = sizeof(float);
        }

        if (real_size != sizeof(real_t)) {
            ST_WARNING("Real type not match. Please recompile toolkit");
            return -1;
        }
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "<CONNLM>\n");
        fprintf(fo_info, "Version: %d\n", version);
        fprintf(fo_info, "Real type: %s\n",
                (real_size == sizeof(double)) ? "double" : "float");
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

    if (output_load_header((connlm == NULL) ? NULL : &connlm->output,
                fp, binary, fo_info) < 0) {
        ST_WARNING("Failed to output_load_header.");
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

    if (connlm->output != NULL) {
        if (output_load_body(connlm->output, fp, binary) < 0) {
            ST_WARNING("Failed to output_load_body.");
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
    int n;

    ST_CHECK_PARAM(connlm == NULL || fp == NULL, -1);

    if (binary) {
        if (fwrite(&CONNLM_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        n = CONNLM_FILE_VERSION;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write version.");
            return -1;
        }

        n = sizeof(real_t);
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write real size.");
            return -1;
        }
    } else {
        fprintf(fp, "    \n<CONNLM>\n");
        fprintf(fp, "Version: %d\n", CONNLM_FILE_VERSION);
        fprintf(fp, "Real type: %s\n",
                (sizeof(double) == sizeof(real_t)) ? "double" : "float");
    }

    if (vocab_save_header(connlm->vocab, fp, binary) < 0) {
        ST_WARNING("Failed to vocab_save_header.");
        return -1;
    }

    if (output_save_header(connlm->output, fp, binary) < 0) {
        ST_WARNING("Failed to output_save_header.");
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

    if (connlm->output != NULL) {
        if (output_save_body(connlm->output, fp, binary) < 0) {
            ST_WARNING("Failed to output_save_body.");
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

static int connlm_get_egs(connlm_t *connlm, int max_word_per_sent,
        int num_line_read, bool skip_oov)
{
    char line[MAX_LINE_LEN];
    char word[MAX_LINE_LEN];

    int *eg;
    char *p;

    int num_sents;
    int i;
    int w;

    ST_CHECK_PARAM(connlm == NULL, -1);

    if (connlm->shuffle_buf != NULL) {
        for (i = 0; i < num_line_read; i++) {
            connlm->shuffle_buf[i] = i;
        }

        st_shuffle_r(connlm->shuffle_buf, num_line_read, &connlm->random);
    }

    for (i = 0; i < num_line_read; i++) {
        connlm->egs[i*max_word_per_sent] = -2;
    }

    num_sents = 0;
    while (fgets(line, MAX_LINE_LEN, connlm->text_fp)) {
        remove_newline(line);

        if (line[0] == '\0') {
            continue;
        }

        if (connlm->shuffle_buf != NULL) {
            eg = connlm->egs + max_word_per_sent * connlm->shuffle_buf[num_sents];
        } else {
            eg = connlm->egs + max_word_per_sent * num_sents;
        }

        p = line;
        i = 0;
        w = 0;
        while (*p != '\0') {
            if (*p == ' ' || *p == '\t') {
                if (i > 0) {
                    word[i] = '\0';
                    eg[w] = vocab_get_id(connlm->vocab, word);
                    if (eg[w] < 0) {
                        if (skip_oov) {
                            ST_WARNING("Failed to st_alphabet_get_index "
                                    "for word[%s]", word);
                            goto SKIP;
                        } else {
                            eg[w] = -1;
                        }
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
        if (w < max_word_per_sent) {
            eg[w] = -2;
        }

        num_sents++;
        if (num_sents >= num_line_read) {
            break;
        }

        continue;
SKIP:
        eg[0] = -2;
    }

    return num_sents;
}

int connlm_setup_train(connlm_t *connlm, connlm_train_opt_t *train_opt,
        const char *train_file)
{
    ST_CHECK_PARAM(connlm == NULL || train_opt == NULL
            || train_file == NULL, -1);

    connlm->train_opt = *train_opt;

    connlm->random = train_opt->rand_seed;

    connlm->text_fp = NULL;
    connlm->egs = NULL;
    connlm->shuffle_buf = NULL;

    connlm->text_fp = st_fopen(train_file, "rb");
    if (connlm->text_fp == NULL) {
        ST_WARNING("Failed to open train file[%s]", train_file);
        goto ERR;
    }

    connlm->egs = (int *)malloc(sizeof(int)
            *train_opt->num_line_read*train_opt->max_word_per_sent);
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

    if (output_setup_train(connlm->output) < 0) {
        ST_WARNING("Failed to output_setup_train.");
        goto ERR;
    }

    if (connlm->rnn != NULL) {
        if (rnn_setup_train(connlm->rnn, &train_opt->rnn_opt,
                    connlm->output) < 0) {
            ST_WARNING("Failed to rnn_setup_train.");
            goto ERR;
        }
    }

    if (connlm->maxent != NULL) {
        if (maxent_setup_train(connlm->maxent, &train_opt->maxent_opt,
                    connlm->output) < 0) {
            ST_WARNING("Failed to maxent_setup_train.");
            goto ERR;
        }
    }

    if (connlm->lbl != NULL) {
        if (lbl_setup_train(connlm->lbl, &train_opt->lbl_opt,
                    connlm->output) < 0) {
            ST_WARNING("Failed to lbl_setup_train.");
            goto ERR;
        }
    }

    if (connlm->ffnn != NULL) {
        if (ffnn_setup_train(connlm->ffnn, &train_opt->ffnn_opt,
                    connlm->output) < 0) {
            ST_WARNING("Failed to ffnn_setup_train.");
            goto ERR;
        }
    }

    return 0;

ERR:
    safe_st_fclose(connlm->text_fp);
    safe_free(connlm->egs);
    safe_free(connlm->shuffle_buf);

    return -1;
}

int connlm_reset_train(connlm_t *connlm)
{
    ST_CHECK_PARAM(connlm == NULL, -1);

    if (output_reset_train(connlm->output) < 0) {
        ST_WARNING("Failed to output_reset_train.");
        return -1;
    }

    if (connlm->rnn != NULL) {
        if (rnn_reset_train(connlm->rnn) < 0) {
            ST_WARNING("Failed to rnn_reset_train.");
            return -1;
        }
    }

    if (connlm->maxent != NULL) {
        if (maxent_reset_train(connlm->maxent) < 0) {
            ST_WARNING("Failed to maxent_reset_train.");
            return -1;
        }
    }

    if (connlm->lbl != NULL) {
        if (lbl_reset_train(connlm->lbl) < 0) {
            ST_WARNING("Failed to lbl_reset_train.");
            return -1;
        }
    }

    if (connlm->ffnn != NULL) {
        if (ffnn_reset_train(connlm->ffnn) < 0) {
            ST_WARNING("Failed to ffnn_reset_train.");
            return -1;
        }
    }

    return 0;
}

int connlm_clear_train(connlm_t *connlm, int word)
{
    ST_CHECK_PARAM(connlm == NULL, -1);

    if (output_clear_train(connlm->output, word) < 0) {
        ST_WARNING("Failed to output_clear_train.");
        return -1;
    }

    if (connlm->rnn != NULL) {
        if (rnn_clear_train(connlm->rnn, word) < 0) {
            ST_WARNING("Failed to rnn_clear_train.");
            return -1;
        }
    }

    if (connlm->maxent != NULL) {
        if (maxent_clear_train(connlm->maxent, word) < 0) {
            ST_WARNING("Failed to maxent_clear_train.");
            return -1;
        }
    }

    if (connlm->lbl != NULL) {
        if (lbl_clear_train(connlm->lbl, word) < 0) {
            ST_WARNING("Failed to lbl_clear_train.");
            return -1;
        }
    }

    if (connlm->ffnn != NULL) {
        if (ffnn_clear_train(connlm->ffnn, word) < 0) {
            ST_WARNING("Failed to ffnn_clear_train.");
            return -1;
        }
    }

    return 0;
}

int connlm_forward(connlm_t *connlm, int word)
{
    ST_CHECK_PARAM(connlm == NULL, -1);

    if (connlm->rnn != NULL) {
        if (rnn_forward(connlm->rnn, word) < 0) {
            ST_WARNING("Failed to rnn_forward.");
            return -1;
        }
    }

    if (connlm->maxent != NULL) {
        if (maxent_forward(connlm->maxent, word) < 0) {
            ST_WARNING("Failed to maxent_forward.");
            return -1;
        }
    }

    if (connlm->lbl != NULL) {
        if (lbl_forward(connlm->lbl, word) < 0) {
            ST_WARNING("Failed to lbl_forward.");
            return -1;
        }
    }

    if (connlm->ffnn != NULL) {
        if (ffnn_forward(connlm->ffnn, word) < 0) {
            ST_WARNING("Failed to ffnn_forward.");
            return -1;
        }
    }

    if (output_activate(connlm->output, word) < 0) {
        ST_WARNING("Failed to output_activate.");
        return -1;
    }

    return 0;
}

int connlm_backprop(connlm_t *connlm, int word)
{
    ST_CHECK_PARAM(connlm == NULL, -1);

    if (output_loss(connlm->output, word) < 0) {
        ST_WARNING("Failed to output_loss.");
        return -1;
    }

    if (connlm->rnn != NULL) {
        if (rnn_backprop(connlm->rnn, word) < 0) {
            ST_WARNING("Failed to rnn_backprop.");
            return -1;
        }
    }

    if (connlm->maxent != NULL) {
        if (maxent_backprop(connlm->maxent, word) < 0) {
            ST_WARNING("Failed to maxent_backprop.");
            return -1;
        }
    }

    if (connlm->lbl != NULL) {
        if (lbl_backprop(connlm->lbl, word) < 0) {
            ST_WARNING("Failed to lbl_backprop.");
            return -1;
        }
    }

    if (connlm->ffnn != NULL) {
        if (ffnn_backprop(connlm->ffnn, word) < 0) {
            ST_WARNING("Failed to ffnn_backprop.");
            return -1;
        }
    }

    return 0;
}

int connlm_train(connlm_t *connlm)
{
    connlm_train_opt_t *train_opt;
    int *eg;

    count_t words;
    double logp;
    real_t p;

    int max_word_per_sent;
    int i;
    int j;

    ST_CHECK_PARAM(connlm == NULL, -1);

    train_opt = &connlm->train_opt;
    max_word_per_sent = connlm->train_opt.max_word_per_sent;

    words = 0;
    logp = 0;
    while (!feof(connlm->text_fp)) {
        if (connlm_get_egs(connlm, train_opt->max_word_per_sent,
                    train_opt->num_line_read, true) < 0) {
            ST_WARNING("Failed to connlm_get_egs.");
            return -1;
        }

        for (i = 0; i < train_opt->num_line_read; i++) {
            eg = connlm->egs + max_word_per_sent * i;
            if (eg[0] < 0) {
                continue;
            }

            if (connlm_reset_train(connlm) < 0) {
                ST_WARNING("Failed to connlm_reset_train.");
                return -1;
            }

            j = 0;
            while (j < max_word_per_sent && eg[j] >= 0) {
                if (connlm_clear_train(connlm, eg[j]) < 0) {
                    ST_WARNING("connlm_clear_train.");
                    return -1;
                }

                if (connlm_forward(connlm, eg[j]) < 0) {
                    ST_WARNING("Failed to connlm_forward.");
                    return -1;
                }

                p = output_get_prob(connlm->output, eg[j]);
                logp += log10(p);

                if ((logp != logp) || (isinf(logp))) {
                    ST_WARNING("Numerical error. "
                            "p_class(%d) = %g, p_word(%d) = %g",
                            connlm->output->w2c[eg[j]],
                            connlm->output->ac_o_c[connlm->output->w2c[eg[j]]],
                            eg[j], connlm->output->ac_o_w[eg[j]]);
                    return -1;
                }
                words++;

                if (connlm_backprop(connlm, eg[j]) < 0) {
                    ST_WARNING("Failed to connlm_backprop.");
                    return -1;
                }
                j++;
            }
        }
    }

    return 0;
}

int connlm_setup_test(connlm_t *connlm, connlm_test_opt_t *test_opt,
        const char *test_file)
{
    ST_CHECK_PARAM(connlm == NULL || test_opt == NULL
            || test_file == NULL, -1);

    connlm->test_opt = *test_opt;

    connlm->text_fp = NULL;
    connlm->egs = NULL;

    connlm->text_fp = st_fopen(test_file, "rb");
    if (connlm->text_fp == NULL) {
        ST_WARNING("Failed to open test file[%s]", test_file);
        goto ERR;
    }

    connlm->egs = (int *)malloc(sizeof(int)
            *test_opt->num_line_read*test_opt->max_word_per_sent);
    if (connlm->egs == NULL) {
        ST_WARNING("Failed to malloc egs");
        goto ERR;
    }

    if (output_setup_test(connlm->output) < 0) {
        ST_WARNING("Failed to output_setup_test.");
        goto ERR;
    }

    if (connlm->rnn != NULL) {
        if (rnn_setup_test(connlm->rnn, connlm->output) < 0) {
            ST_WARNING("Failed to rnn_setup_train.");
            goto ERR;
        }
    }

    if (connlm->maxent != NULL) {
        if (maxent_setup_test(connlm->maxent, connlm->output) < 0) {
            ST_WARNING("Failed to maxent_setup_test.");
            goto ERR;
        }
    }

    if (connlm->lbl != NULL) {
        if (lbl_setup_test(connlm->lbl, connlm->output) < 0) {
            ST_WARNING("Failed to lbl_setup_test.");
            goto ERR;
        }
    }

    if (connlm->ffnn != NULL) {
        if (ffnn_setup_test(connlm->ffnn, connlm->output) < 0) {
            ST_WARNING("Failed to ffnn_setup_test.");
            goto ERR;
        }
    }

    return 0;

ERR:
    safe_st_fclose(connlm->text_fp);
    safe_free(connlm->egs);

    return -1;
}

int connlm_reset_test(connlm_t *connlm)
{
    ST_CHECK_PARAM(connlm == NULL, -1);

    if (output_reset_test(connlm->output) < 0) {
        ST_WARNING("Failed to output_reset_test.");
        return -1;
    }

    if (connlm->rnn != NULL) {
        if (rnn_reset_test(connlm->rnn) < 0) {
            ST_WARNING("Failed to rnn_reset_test.");
            return -1;
        }
    }

    if (connlm->maxent != NULL) {
        if (maxent_reset_test(connlm->maxent) < 0) {
            ST_WARNING("Failed to maxent_reset_test.");
            return -1;
        }
    }

    if (connlm->lbl != NULL) {
        if (lbl_reset_test(connlm->lbl) < 0) {
            ST_WARNING("Failed to lbl_reset_test.");
            return -1;
        }
    }

    if (connlm->ffnn != NULL) {
        if (ffnn_reset_test(connlm->ffnn) < 0) {
            ST_WARNING("Failed to ffnn_reset_test.");
            return -1;
        }
    }

    return 0;
}

int connlm_clear_test(connlm_t *connlm, int word)
{
    ST_CHECK_PARAM(connlm == NULL, -1);

    if (output_clear_test(connlm->output, word) < 0) {
        ST_WARNING("Failed to output_clear_test.");
        return -1;
    }

    if (connlm->rnn != NULL) {
        if (rnn_clear_test(connlm->rnn, word) < 0) {
            ST_WARNING("Failed to rnn_clear_test.");
            return -1;
        }
    }

    if (connlm->maxent != NULL) {
        if (maxent_clear_test(connlm->maxent, word) < 0) {
            ST_WARNING("Failed to maxent_clear_test.");
            return -1;
        }
    }

    if (connlm->lbl != NULL) {
        if (lbl_clear_test(connlm->lbl, word) < 0) {
            ST_WARNING("Failed to lbl_clear_test.");
            return -1;
        }
    }

    if (connlm->ffnn != NULL) {
        if (ffnn_clear_test(connlm->ffnn, word) < 0) {
            ST_WARNING("Failed to ffnn_clear_test.");
            return -1;
        }
    }

    return 0;
}

int connlm_test(connlm_t *connlm, FILE *fp_log)
{
    connlm_test_opt_t *test_opt;
    int *eg;

    count_t words;
    count_t oovs;
    double logp;
    real_t p;

    int num_sents;
    int max_word_per_sent;
    int i;
    int j;

    ST_CHECK_PARAM(connlm == NULL, -1);

    test_opt = &connlm->test_opt;
    max_word_per_sent = connlm->test_opt.max_word_per_sent;

    if (fp_log != NULL) {
        fprintf(fp_log, "Index   P(NET)          Word\n");
        fprintf(fp_log, "----------------------------------\n");
    }

    words = 0;
    oovs = 0;
    logp = 0;
    while (!feof(connlm->text_fp)) {
        num_sents = connlm_get_egs(connlm, test_opt->max_word_per_sent,
                test_opt->num_line_read, false);
        if (num_sents < 0) {
            ST_WARNING("Failed to connlm_get_egs.");
            return -1;
        }

        for (i = 0; i < num_sents; i++) {
            eg = connlm->egs + max_word_per_sent * i;

            if (connlm_reset_test(connlm) < 0) {
                ST_WARNING("Failed to connlm_reset_test.");
                return -1;
            }

            j = 0;
            while (j < max_word_per_sent && eg[j] != -2) {
                if (connlm_clear_test(connlm, eg[j]) < 0) {
                    ST_WARNING("connlm_clear_test.");
                    return -1;
                }

                if (connlm_forward(connlm, eg[j]) < 0) {
                    ST_WARNING("Failed to connlm_forward.");
                    return -1;
                }

                if (eg[j] == -1) {
                    logp += test_opt->oov_penalty;
                    oovs++;
                } else {
                    p = output_get_prob(connlm->output, eg[j]);
                    logp += log10(p);

                    if ((logp != logp) || (isinf(logp))) {
                        ST_WARNING("Numerical error. "
                          "p_class(%d) = %g, p_word(%d) = %g",
                          connlm->output->w2c[eg[j]],
                          connlm->output->ac_o_c[connlm->output->w2c[eg[j]]],
                          eg[j], connlm->output->ac_o_w[eg[j]]);
                        return -1;
                    }
                }
                words++;

                if (fp_log != NULL) {
                    if (eg[j] != -1) {
                        fprintf(fp_log, "%d\t%.10f\t%s", eg[j], p,
                                vocab_get_word(connlm->vocab, eg[j]));
                    } else {
                        fprintf(fp_log, "-1\t0\t\t<OOV>");
                    }

                    fprintf(fp_log, "\n");
                }

                j++;
            }
        }
    }

    ST_NOTICE("Words: " COUNT_FMT "    OOVs: " COUNT_FMT, words, oovs);
    ST_NOTICE("Log prob: %f\n", logp);
    ST_NOTICE("PPL: %f\n", exp10(-logp / (real_t) words));

    if (fp_log != NULL) {
        fprintf(fp_log, "\nSummary:\n");
        fprintf(fp_log, "Words: " COUNT_FMT "    OOVs: " COUNT_FMT,
                words, oovs);
        fprintf(fp_log, "Log prob: %f\n", logp);
        fprintf(fp_log, "PPL: %f\n", exp10(-logp / (real_t) words));
    }

    return 0;
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */

