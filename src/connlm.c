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

#include <st_macro.h>
#include <st_log.h>
#include <st_opt.h>
#include <st_io.h>

#include "utils.h"
#include "connlm.h"

#define NUM_WORD_PER_SENT 128

static const int CONNLM_MAGIC_NUM = 626140498;

static void connlm_egs_destroy(connlm_egs_t *egs)
{
    if (egs != NULL) {
        safe_free(egs->words);
        egs->capacity = 0;
        egs->size = 0;
    }
}

static int connlm_egs_ensure(connlm_egs_t *egs, int capacity)
{
    ST_CHECK_PARAM(egs == NULL, -1);

//#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-overflow"
    if (egs->capacity < capacity) {
//#pragma GCC diagnostic pop
        egs->words = realloc(egs->words, sizeof(int)*capacity);
        if (egs->words == NULL) {
            ST_WARNING("Failed to realloc egs->words. capacity[%d]", 
                    capacity);
            return -1;
        }

        egs->capacity = capacity;
    }

    return 0;
}

static int connlm_egs_print(FILE *fp, pthread_mutex_t *fp_lock,
        connlm_egs_t *egs, vocab_t *vocab)
{
    int word;
    int i;

    ST_CHECK_PARAM(fp == NULL || fp_lock == NULL
            || egs == NULL, -1);

    i = 0;
    while (i < egs->size) {
        (void)pthread_mutex_lock(fp_lock);
        fprintf(fp, "<EGS>: ");
        while (egs->words[i] != 0 && i < egs->size) {
            word = egs->words[i];

            if (word > 0) {
                if (vocab != NULL) {
                    fprintf(fp, "%s", vocab_get_word(vocab, word));
                } else {
                    fprintf(fp, "<%d>", word);
                }
            } else {
                fprintf(fp, "<OOV>");
            }
            if (i < egs->size - 1 && egs->words[i+1] != 0) {
                fprintf(fp, " ");
            }
            i++;
        }
        fprintf(fp, "\n");
        fflush(fp);
        i++;
        (void)pthread_mutex_unlock(fp_lock);
    }

    return 0;
}

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

    ST_OPT_SEC_GET_INT(opt, sec_name, "EPOCH_SIZE",
            train_opt->epoch_size, 10,
            "Number of sentences read in one epoch per thread (in kilos)");
    train_opt->epoch_size *= 1000;
    ST_OPT_SEC_GET_BOOL(opt, sec_name, "SHUFFLE",
            train_opt->shuffle, true,
            "Shuffle after reading");

    ST_OPT_SEC_GET_STR(opt, sec_name, "DEBUG_FILE",
            train_opt->debug_file, MAX_DIR_LEN, "",
            "file to print out debug infos.");

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
    double d;

    ST_CHECK_PARAM(test_opt == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_INT(opt, sec_name, "NUM_THREAD",
            test_opt->num_thread, 1, "Number of threads");

    ST_OPT_SEC_GET_INT(opt, sec_name, "EPOCH_SIZE",
            test_opt->epoch_size, 10,
            "Number of sentences read in one epoch per thread (in kilos)");
    test_opt->epoch_size *= 1000;

    ST_OPT_SEC_GET_DOUBLE(opt, sec_name, "OOV_PENALTY",
            d, -8, "Penalty of OOV");
    test_opt->oov_penalty = (real_t)d;

    ST_OPT_SEC_GET_STR(opt, sec_name, "DEBUG_FILE",
            test_opt->debug_file, MAX_DIR_LEN, "",
            "file to print out debug infos.");

    return 0;

ST_OPT_ERR:
    return -1;
}

void connlm_destroy(connlm_t *connlm)
{
    int i;

    if (connlm == NULL) {
        return;
    }

    if (connlm->egs_pool != NULL) {
        for (i = 0; i < connlm->pool_size; i++) {
            connlm_egs_destroy(connlm->egs_pool + i);
        }
        safe_free(connlm->egs_pool);
    }

    (void)pthread_mutex_destroy(&connlm->pool_lock);
    (void)st_sem_destroy(&connlm->sem_full);
    (void)st_sem_destroy(&connlm->sem_empty);
    safe_fclose(connlm->fp_debug);
    (void)pthread_mutex_destroy(&connlm->fp_debug_lock);

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
    st_srand(model_opt->rand_seed);

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

static int connlm_get_egs(connlm_egs_t *egs, int *sent_ends,
        int epoch_size, FILE *text_fp, vocab_t *vocab)
{
    char *line = NULL;
    size_t line_sz = 0;
    char word[MAX_LINE_LEN];

    char *p;

    int num_sents;
    int i;

    bool err;

    // assert len(sent_ends) >= epoch_size
    ST_CHECK_PARAM(egs == NULL || text_fp == NULL, -1);

    err = false;
    num_sents = 0;
    egs->size = 0;
    while (st_fgets(&line, &line_sz, text_fp, &err)) {
        remove_newline(line);

        if (line[0] == '\0') {
            continue;
        }

        p = line;
        i = 0;
        while (*p != '\0') {
            if (*p == ' ' || *p == '\t') {
                if (i > 0) {
                    word[i] = '\0';
                    if (egs->size >= egs->capacity - 1) {
                        if (connlm_egs_ensure(egs,
                                egs->capacity + NUM_WORD_PER_SENT) < 0) {
                            ST_WARNING("Failed to connlm_egs_ensure. ");
                            goto ERR;
                        }
                    }
                    egs->words[egs->size] = vocab_get_id(vocab, word);
                    egs->size++;

                    i = 0;
                }

                while (*p == ' ' || *p == '\t') {
                    p++;
                }
            } else {
                word[i] = *p;
                i++;
                p++;
            }
        }
        if (i > 0) {
            word[i] = '\0';
            if (egs->size >= egs->capacity - 1) {
                if (connlm_egs_ensure(egs,
                            egs->capacity + NUM_WORD_PER_SENT) < 0) {
                    ST_WARNING("Failed to connlm_egs_ensure. ");
                    goto ERR;
                }
            }
            egs->words[egs->size] = vocab_get_id(vocab, word);
            egs->size++;

            i = 0;
        }

        egs->words[egs->size] = 0; // </s>
        egs->size++; // do not need to check, prev check num_egs - 1
        if (sent_ends != NULL) {
            sent_ends[num_sents] = egs->size;
        }

        num_sents++;
        if (num_sents >= epoch_size) {
            break;
        }
    }

    safe_free(line);

    if (err) {
        return -1;
    }

    return num_sents;

ERR:

    safe_free(line);
    return -1;
}

int connlm_setup_train(connlm_t *connlm, connlm_train_opt_t *train_opt,
        const char *train_file)
{
    int num_thrs;

    ST_CHECK_PARAM(connlm == NULL || train_opt == NULL
            || train_file == NULL, -1);

    connlm->train_opt = *train_opt;

    if (connlm->train_opt.num_thread < 1) {
        connlm->train_opt.num_thread = 1;
    }
    num_thrs = connlm->train_opt.num_thread;

    strncpy(connlm->text_file, train_file, MAX_DIR_LEN);
    connlm->text_file[MAX_DIR_LEN - 1] = '\0';

    connlm->pool_size = train_opt->num_thread;
    connlm->egs_pool = (connlm_egs_t *)malloc(sizeof(connlm_egs_t)
            * connlm->pool_size);
    if (connlm->egs_pool == NULL) {
        ST_WARNING("Failed to malloc egs_pool.");
        goto ERR;
    }
    memset(connlm->egs_pool, 0, sizeof(connlm_egs_t)*connlm->pool_size);

    if (st_sem_init(&connlm->sem_empty, connlm->pool_size) != 0) {
        ST_WARNING("Failed to st_sem_init sem_empty.");
        goto ERR;
    }

    if (st_sem_init(&connlm->sem_full, 0) != 0) {
        ST_WARNING("Failed to st_sem_init sem_full.");
        goto ERR;
    }

    if (pthread_mutex_init(&connlm->pool_lock, NULL) != 0) {
        ST_WARNING("Failed to pthread_mutex_init pool_lock.");
        goto ERR;
    }

    if (train_opt->debug_file[0] != '\0') {
        connlm->fp_debug = st_fopen(train_opt->debug_file, "w");
        if (connlm->fp_debug == NULL) {
            ST_WARNING("Failed to st_fopen debug_file[%s].",
                    train_opt->debug_file);
            goto ERR;
        }
        if (pthread_mutex_init(&connlm->fp_debug_lock, NULL) != 0) {
            ST_WARNING("Failed to pthread_mutex_init fp_debug_lock.");
            goto ERR;
        }
    }

    if (output_setup_train(connlm->output, num_thrs) < 0) {
        ST_WARNING("Failed to output_setup_train.");
        goto ERR;
    }

    if (connlm->rnn != NULL) {
        if (rnn_setup_train(connlm->rnn, &train_opt->rnn_opt,
                    connlm->output, num_thrs) < 0) {
            ST_WARNING("Failed to rnn_setup_train.");
            goto ERR;
        }
    }

    if (connlm->maxent != NULL) {
        if (maxent_setup_train(connlm->maxent, &train_opt->maxent_opt,
                    connlm->output, num_thrs) < 0) {
            ST_WARNING("Failed to maxent_setup_train.");
            goto ERR;
        }
    }

    if (connlm->lbl != NULL) {
        if (lbl_setup_train(connlm->lbl, &train_opt->lbl_opt,
                    connlm->output, num_thrs) < 0) {
            ST_WARNING("Failed to lbl_setup_train.");
            goto ERR;
        }
    }

    if (connlm->ffnn != NULL) {
        if (ffnn_setup_train(connlm->ffnn, &train_opt->ffnn_opt,
                    connlm->output, num_thrs) < 0) {
            ST_WARNING("Failed to ffnn_setup_train.");
            goto ERR;
        }
    }

    return 0;

ERR:
    safe_free(connlm->egs_pool);
    (void)pthread_mutex_destroy(&connlm->pool_lock);
    (void)st_sem_destroy(&connlm->sem_full);
    (void)st_sem_destroy(&connlm->sem_empty);
    safe_fclose(connlm->fp_debug);
    (void)pthread_mutex_destroy(&connlm->fp_debug_lock);

    return -1;
}

int connlm_reset_train(connlm_t *connlm, int tid)
{
    ST_CHECK_PARAM(connlm == NULL, -1);

    if (output_reset_train(connlm->output, tid) < 0) {
        ST_WARNING("Failed to output_reset_train.");
        return -1;
    }

    if (connlm->rnn != NULL) {
        if (rnn_reset_train(connlm->rnn, tid) < 0) {
            ST_WARNING("Failed to rnn_reset_train.");
            return -1;
        }
    }

    if (connlm->maxent != NULL) {
        if (maxent_reset_train(connlm->maxent, tid) < 0) {
            ST_WARNING("Failed to maxent_reset_train.");
            return -1;
        }
    }

    if (connlm->lbl != NULL) {
        if (lbl_reset_train(connlm->lbl, tid) < 0) {
            ST_WARNING("Failed to lbl_reset_train.");
            return -1;
        }
    }

    if (connlm->ffnn != NULL) {
        if (ffnn_reset_train(connlm->ffnn, tid) < 0) {
            ST_WARNING("Failed to ffnn_reset_train.");
            return -1;
        }
    }

    return 0;
}

int connlm_start_train(connlm_t *connlm, int word, int tid)
{
    ST_CHECK_PARAM(connlm == NULL, -1);

    if (output_start_train(connlm->output, word, tid) < 0) {
        ST_WARNING("Failed to output_start_train.");
        return -1;
    }

    if (connlm->rnn != NULL) {
        if (rnn_start_train(connlm->rnn, word, tid) < 0) {
            ST_WARNING("Failed to rnn_start_train.");
            return -1;
        }
    }

    if (connlm->maxent != NULL) {
        if (maxent_start_train(connlm->maxent, word, tid) < 0) {
            ST_WARNING("Failed to maxent_start_train.");
            return -1;
        }
    }

    if (connlm->lbl != NULL) {
        if (lbl_start_train(connlm->lbl, word, tid) < 0) {
            ST_WARNING("Failed to lbl_start_train.");
            return -1;
        }
    }

    if (connlm->ffnn != NULL) {
        if (ffnn_start_train(connlm->ffnn, word, tid) < 0) {
            ST_WARNING("Failed to ffnn_start_train.");
            return -1;
        }
    }

    return 0;
}

int connlm_end_train(connlm_t *connlm, int word, int tid)
{
    ST_CHECK_PARAM(connlm == NULL, -1);

    if (output_end_train(connlm->output, word, tid) < 0) {
        ST_WARNING("Failed to output_end_train.");
        return -1;
    }

    if (connlm->rnn != NULL) {
        if (rnn_end_train(connlm->rnn, word, tid) < 0) {
            ST_WARNING("Failed to rnn_end_train.");
            return -1;
        }
    }

    if (connlm->maxent != NULL) {
        if (maxent_end_train(connlm->maxent, word, tid) < 0) {
            ST_WARNING("Failed to maxent_end_train.");
            return -1;
        }
    }

    if (connlm->lbl != NULL) {
        if (lbl_end_train(connlm->lbl, word, tid) < 0) {
            ST_WARNING("Failed to lbl_end_train.");
            return -1;
        }
    }

    if (connlm->ffnn != NULL) {
        if (ffnn_end_train(connlm->ffnn, word, tid) < 0) {
            ST_WARNING("Failed to ffnn_end_train.");
            return -1;
        }
    }

    return 0;
}

int connlm_finish_train(connlm_t *connlm, int tid)
{
    ST_CHECK_PARAM(connlm == NULL, -1);

    if (output_finish_train(connlm->output, tid) < 0) {
        ST_WARNING("Failed to output_finish_train.");
        return -1;
    }

    if (connlm->rnn != NULL) {
        if (rnn_finish_train(connlm->rnn, tid) < 0) {
            ST_WARNING("Failed to rnn_finish_train.");
            return -1;
        }
    }

    if (connlm->maxent != NULL) {
        if (maxent_finish_train(connlm->maxent, tid) < 0) {
            ST_WARNING("Failed to maxent_finish_train.");
            return -1;
        }
    }

    if (connlm->lbl != NULL) {
        if (lbl_finish_train(connlm->lbl, tid) < 0) {
            ST_WARNING("Failed to lbl_finish_train.");
            return -1;
        }
    }

    if (connlm->ffnn != NULL) {
        if (ffnn_finish_train(connlm->ffnn, tid) < 0) {
            ST_WARNING("Failed to ffnn_finish_train.");
            return -1;
        }
    }

    return 0;
}

int connlm_forward(connlm_t *connlm, int word, int tid)
{
    ST_CHECK_PARAM(connlm == NULL, -1);

    if (connlm->rnn != NULL) {
        if (rnn_forward(connlm->rnn, word, tid) < 0) {
            ST_WARNING("Failed to rnn_forward.");
            return -1;
        }
    }

    if (connlm->maxent != NULL) {
        if (maxent_forward(connlm->maxent, word, tid) < 0) {
            ST_WARNING("Failed to maxent_forward.");
            return -1;
        }
    }

    if (connlm->lbl != NULL) {
        if (lbl_forward(connlm->lbl, word, tid) < 0) {
            ST_WARNING("Failed to lbl_forward.");
            return -1;
        }
    }

    if (connlm->ffnn != NULL) {
        if (ffnn_forward(connlm->ffnn, word, tid) < 0) {
            ST_WARNING("Failed to ffnn_forward.");
            return -1;
        }
    }

    if (output_activate(connlm->output, word, tid) < 0) {
        ST_WARNING("Failed to output_activate.");
        return -1;
    }

    return 0;
}

int connlm_fwd_bp(connlm_t *connlm, int word, int tid)
{
    ST_CHECK_PARAM(connlm == NULL, -1);

    if (connlm->rnn != NULL) {
        if (rnn_fwd_bp(connlm->rnn, word, tid) < 0) {
            ST_WARNING("Failed to rnn_fwd_bp.");
            return -1;
        }
    }

    return 0;
}

int connlm_backprop(connlm_t *connlm, int word, int tid)
{
    ST_CHECK_PARAM(connlm == NULL, -1);

    if (output_loss(connlm->output, word, tid) < 0) {
        ST_WARNING("Failed to output_loss.");
        return -1;
    }

    if (connlm->rnn != NULL) {
        if (rnn_backprop(connlm->rnn, word, tid) < 0) {
            ST_WARNING("Failed to rnn_backprop.");
            return -1;
        }
    }

    if (connlm->maxent != NULL) {
        if (maxent_backprop(connlm->maxent, word, tid) < 0) {
            ST_WARNING("Failed to maxent_backprop.");
            return -1;
        }
    }

    if (connlm->lbl != NULL) {
        if (lbl_backprop(connlm->lbl, word, tid) < 0) {
            ST_WARNING("Failed to lbl_backprop.");
            return -1;
        }
    }

    if (connlm->ffnn != NULL) {
        if (ffnn_backprop(connlm->ffnn, word, tid) < 0) {
            ST_WARNING("Failed to ffnn_backprop.");
            return -1;
        }
    }

    return 0;
}

typedef struct _connlm_thread_t_ {
    connlm_t *connlm; /**< connlm model. */
    int tid; /**< thread id. */

    count_t words; /**< total number of words trained in this thread. */
    count_t sents; /**< total number of sentences trained in this thread. */
    double logp; /**< total log probability in this thread. */

    count_t oovs; /**< total number of OOV words found in this thread. */
} connlm_thr_t;

typedef struct _connlm_read_thread_t_ {
    connlm_t *connlm;
    connlm_thr_t *thrs;

    int num_thrs;
    int epoch_size;

    bool shuffle;
    unsigned random;

    count_t sents;
    count_t words;
} connlm_read_thr_t;

static void* connlm_read_thread(void *args)
{
    connlm_read_thr_t *read_thr;
    connlm_t *connlm;
    connlm_thr_t *thrs;
    
    int *shuffle_buf = NULL;
    FILE *text_fp = NULL;
    off_t fsize;
    int *sent_ends = NULL;

    connlm_egs_t egs = {
        .words = NULL,
        .size = 0,
        .capacity = 0,
    };
    int num_thrs;
    int epoch_size;

    connlm_egs_t *egs_in_pool;
    int num_sents;
    int i;
    int start;
    int end;

    count_t total_words;
    count_t total_sents;
    double logp;
    struct timeval tts, tte;
    long ms;

    ST_CHECK_PARAM(args == NULL, NULL);

    read_thr = (connlm_read_thr_t *)args;
    connlm = read_thr->connlm;
    thrs = read_thr->thrs;
    num_thrs = read_thr->num_thrs;
    epoch_size = read_thr->epoch_size;

    if (connlm_egs_ensure(&egs, NUM_WORD_PER_SENT) < 0) {
        ST_WARNING("Failed to connlm_ensure_egs.");
        goto ERR;
    }

    if (read_thr->shuffle) {
        shuffle_buf = (int *)malloc(sizeof(int) * epoch_size);
        if (shuffle_buf == NULL) {
            ST_WARNING("Failed to malloc shuffle_buf");
            goto ERR;
        }

        sent_ends = (int *)malloc(sizeof(int) * epoch_size);
        if (sent_ends == NULL) {
            ST_WARNING("Failed to malloc sent_ends");
            goto ERR;
        }
    }

    fsize = st_fsize(connlm->text_file);

    text_fp = st_fopen(connlm->text_file, "rb");
    if (text_fp == NULL) {
        ST_WARNING("Failed to open train file[%s]", connlm->text_file);
        goto ERR;
    }

    read_thr->sents = 0;
    read_thr->words = 0;
    gettimeofday(&tts, NULL);
    while (!feof(text_fp)) {
        if (connlm->err != 0) {
            break;
        }

        num_sents = connlm_get_egs(&egs, sent_ends,
                epoch_size, text_fp, connlm->vocab);
        if (num_sents < 0) {
            ST_WARNING("Failed to connlm_get_egs.");
            goto ERR;
        }
        if (num_sents == 0) {
            continue;
        }

        read_thr->sents += num_sents;
        read_thr->words += egs.size;

        if (shuffle_buf != NULL) {
            for (i = 0; i < num_sents; i++) {
                shuffle_buf[i] = i;
            }

            st_shuffle_r(shuffle_buf, num_sents, &read_thr->random);
        }

        if (st_sem_wait(&connlm->sem_empty) != 0) {
            ST_WARNING("Failed to st_sem_wait sem_empty.");
            goto ERR;
        }

        // only one read thread, do not need to lock the pool.
        // copy egs to pool
        egs_in_pool = connlm->egs_pool + connlm->pool_in;
        egs_in_pool->size = 0;
        if (connlm_egs_ensure(egs_in_pool, egs.size) < 0) {
            ST_WARNING("Failed to connlm_ensure_egs. pool_in[%d], "
                    "num_words[%d].", connlm->pool_in, egs.size);
            goto ERR;
        }

        if (shuffle_buf != NULL) {
            for (i = 0; i < num_sents; i++) {
                if (shuffle_buf[i] == 0) {
                    start = 0;
                } else {
                    start = sent_ends[shuffle_buf[i] - 1];
                }
                end = sent_ends[shuffle_buf[i]];

                memcpy(egs_in_pool->words + egs_in_pool->size,
                        egs.words + start, sizeof(int)*(end - start));
                egs_in_pool->size += end - start;
            }
        } else {
            memcpy(egs_in_pool->words, egs.words, sizeof(int)*egs.size);
            egs_in_pool->size = egs.size;
        }

        connlm->pool_in = (connlm->pool_in + 1) % connlm->pool_size;
        if (st_sem_post(&connlm->sem_full) != 0) {
            ST_WARNING("Failed to st_sem_post sem_full.");
            goto ERR;
        }

        if (connlm->pool_in == 0) {
            total_words = 0;
            total_sents = 0;
            logp = 0.0;
            for (i = 0; i < num_thrs; i++) {
                total_words += thrs[i].words;
                total_sents += thrs[i].sents;
                logp += thrs[i].logp;
            }

            if (total_words > 0) {
                gettimeofday(&tte, NULL);
                ms = TIMEDIFF(tts, tte);

                if (fsize > 0) {
                    ST_TRACE("Total progress: %.2f%%. Words: " COUNT_FMT
                            ", Sentences: " COUNT_FMT ", words/sec: %.1f, "
                            "LogP: %f, Entropy: %f, PPL: %f, Time: %.3fs", 
                            ftell(text_fp) / fsize * 100.0,
                            total_words, total_sents,
                            total_words / ((double) ms / 1000.0),
                            logp, -logp / log10(2) / total_words,
                            exp10(-logp / (double) total_words),
                            ms / 1000.0);
                } else {
                    ST_TRACE("Words: " COUNT_FMT
                            ", Sentences: " COUNT_FMT ", words/sec: %.1f, "
                            "LogP: %f, Entropy: %f, PPL: %f, Time: %.3fs", 
                            total_words, total_sents,
                            total_words / ((double) ms / 1000.0),
                            logp, -logp / log10(2) / total_words,
                            exp10(-logp / (double) total_words),
                            ms / 1000.0);
                }
            }
        }
    }

    for (i = 0; i < num_thrs; i++) {
        if (st_sem_wait(&connlm->sem_empty) != 0) {
            ST_WARNING("Failed to st_sem_wait sem_empty.");
            goto ERR;
        }

        egs_in_pool = connlm->egs_pool + connlm->pool_in;
        egs_in_pool->size = -2; // finish
        connlm->pool_in = (connlm->pool_in + 1) % connlm->pool_size;

        if (st_sem_post(&connlm->sem_full) != 0) {
            ST_WARNING("Failed to st_sem_post sem_full.");
            goto ERR;
        }
    }

    connlm_egs_destroy(&egs);
    safe_fclose(text_fp);
    safe_free(shuffle_buf);
    safe_free(sent_ends);

    return NULL;

ERR:
    connlm->err = -1;

    connlm_egs_destroy(&egs);
    safe_fclose(text_fp);
    safe_free(shuffle_buf);
    safe_free(sent_ends);

    for (i = 0; i < num_thrs; i++) {
        if (st_sem_wait(&connlm->sem_empty) != 0) {
            ST_WARNING("Failed to st_sem_wait sem_empty.");
            goto ERR;
        }

        egs_in_pool = connlm->egs_pool + connlm->pool_in;
        egs_in_pool->size = -2; // finish
        connlm->pool_in = (connlm->pool_in + 1) % connlm->pool_size;

        if (st_sem_post(&connlm->sem_full) != 0) {
            ST_WARNING("Failed to st_sem_post sem_full.");
            goto ERR;
        }
    }

    return NULL;
}

static void* connlm_train_thread(void *args)
{
    connlm_t *connlm;
    connlm_thr_t *thr;
    int tid;

    connlm_egs_t *egs_in_pool;
    connlm_egs_t egs = {
        .words = NULL,
        .size = 0,
        .capacity = 0,
    };

    count_t words;
    count_t sents;
    double logp;

    int word;
    int i;

    struct timeval tts, tte;
    long ms;

    struct timeval tts_wait, tte_wait;
    long ms_wait;

    ST_CHECK_PARAM(args == NULL, NULL);

    thr = (connlm_thr_t *)args;
    connlm = thr->connlm;
    tid = thr->tid;

    gettimeofday(&tts, NULL);

    words = 0;
    sents = 0;
    logp = 0.0;
    while (true) {
        if (connlm->err != 0) {
            break;
        }

        gettimeofday(&tts_wait, NULL);
        if (st_sem_wait(&connlm->sem_full) != 0) {
            ST_WARNING("Failed to st_sem_wait sem_full.");
            goto ERR;
        }
        if (pthread_mutex_lock(&connlm->pool_lock) != 0) {
            ST_WARNING("Failed to pthread_mutex_lock pool_lock.");
            goto ERR;
        }

        egs_in_pool = connlm->egs_pool + connlm->pool_out;
        connlm->pool_out = (connlm->pool_out + 1) % connlm->pool_size;

        if (egs_in_pool->size > 0) {
            if (connlm_egs_ensure(&egs, egs_in_pool->size) < 0) {
                ST_WARNING("Failed to connlm_ensure_egs. pool_out[%d], "
                        "num_words[%d].", connlm->pool_out, egs_in_pool->size);
                goto ERR;
            }
            memcpy(egs.words, egs_in_pool->words, sizeof(int)*egs_in_pool->size);
        }
        egs.size = egs_in_pool->size;

        if (pthread_mutex_unlock(&connlm->pool_lock) != 0) {
            ST_WARNING("Failed to pthread_mutex_unlock pool_lock.");
            goto ERR;
        }
        if (st_sem_post(&connlm->sem_empty) != 0) {
            ST_WARNING("Failed to st_sem_post sem_empty.");
            goto ERR;
        }
        gettimeofday(&tte_wait, NULL);

        if (egs.size == -2) { // finish
            break;
        }

        if (connlm->fp_debug != NULL) {
            if (connlm_egs_print(connlm->fp_debug, &connlm->fp_debug_lock,
                        &egs, connlm->vocab) < 0) {
                ST_WARNING("Failed to connlm_egs_print.");
                goto ERR;
            }
        }

        if (connlm_reset_train(connlm, tid) < 0) {
            ST_WARNING("Failed to connlm_reset_train.");
            goto ERR;
        }

        for (i = 0; i < egs.size; i++) {
            word = egs.words[i];

            if (word < 0) {
                /* TODO: OOV during training. */
                words++;
                continue;
            }
            if (connlm_start_train(connlm, word, tid) < 0) {
                ST_WARNING("connlm_start_train.");
                goto ERR;
            }

            if (connlm_forward(connlm, word, tid) < 0) {
                ST_WARNING("Failed to connlm_forward.");
                goto ERR;
            }

            logp += log10(output_get_prob(connlm->output, word, tid));

            if ((logp != logp) || (isinf(logp))) {
                if (connlm->output->output_opt.class_size > 0) {
                    ST_WARNING("Numerical error. tid[%d], "
                            "p_class(%d) = %g, p_word(%d) = %g", tid,
                            connlm->output->w2c[word],
                            output_get_class_prob(connlm->output,
                                word, tid),
                            word, 
                            output_get_word_prob(connlm->output,
                                word, tid));
                } else {
                    ST_WARNING("Numerical error. tid[%d], p_word(%d) = %g",
                            tid, word, 
                            output_get_word_prob(connlm->output,
                                word, tid));
                }
                goto ERR;
            }

            if (connlm_fwd_bp(connlm, word, tid) < 0) {
                ST_WARNING("Failed to connlm_fwd_bp.");
                goto ERR;
            }

            if (connlm_backprop(connlm, word, tid) < 0) {
                ST_WARNING("Failed to connlm_backprop.");
                goto ERR;
            }

            if (connlm_end_train(connlm, word, tid) < 0) {
                ST_WARNING("connlm_end_train.");
                goto ERR;
            }

            if (word == 0) { // </s>
                if (connlm_reset_train(connlm, tid) < 0) {
                    ST_WARNING("Failed to connlm_reset_train.");
                    goto ERR;
                }
                sents++;
            }
            words++;
        }

        thr->words = words;
        thr->sents = sents;
        thr->logp = logp;

        gettimeofday(&tte, NULL);
        ms = TIMEDIFF(tts, tte);
        ms_wait = TIMEDIFF(tts_wait, tte_wait);

        ST_TRACE("Thread: %d, Words: " COUNT_FMT
                ", Sentences: " COUNT_FMT ", words/sec: %.1f, "
                "LogP: %f, Entropy: %f, PPL: %f, "
                "Time(cpu/wait): %.3fs(%.2f%%)/%.3fs(%.2f%%)", 
                tid, words, sents, words / ((double) ms / 1000.0),
                logp, -logp / log10(2) / words,
                exp10(-logp / (double) words),
                (ms - ms_wait) / 1000.0, (ms - ms_wait) / (ms / 100.0),
                ms_wait / 1000.0, ms_wait / (ms / 100.0));
    }

    connlm_egs_destroy(&egs);

    return NULL;
ERR:
    connlm->err = -1;

    connlm_egs_destroy(&egs);

    /* TODO: unlock mutex and post semaphore */
    return NULL;
}

int connlm_train(connlm_t *connlm)
{
    connlm_thr_t *thrs = NULL;
    pthread_t *pts = NULL;

    connlm_read_thr_t read_thr;
    int num_thrs;

    count_t words;
    count_t sents;
    double logp;
    struct timeval tts_train, tte_train;
    long ms;

    int i;

    ST_CHECK_PARAM(connlm == NULL, -1);

    gettimeofday(&tts_train, NULL);

    num_thrs = connlm->train_opt.num_thread;
    pts = (pthread_t *)malloc((num_thrs + 1) * sizeof(pthread_t));
    if (pts == NULL) {
        ST_WARNING("Failed to malloc pts");
        goto ERR;
    }

    thrs = (connlm_thr_t *)malloc(sizeof(connlm_thr_t) * num_thrs);
    if (thrs == NULL) {
        ST_WARNING("Failed to malloc thrs");
        goto ERR;
    }
    memset(thrs, 0, sizeof(connlm_thr_t) * num_thrs);

    connlm->err = 0;
    connlm->pool_in = 0;
    connlm->pool_out = 0;

    read_thr.connlm = connlm;
    read_thr.thrs = thrs;
    read_thr.num_thrs = connlm->train_opt.num_thread;
    read_thr.epoch_size = connlm->train_opt.epoch_size;
    read_thr.shuffle = connlm->train_opt.shuffle;
    read_thr.random = connlm->train_opt.rand_seed;

    pthread_create(pts + num_thrs, NULL,
            connlm_read_thread, (void *)&read_thr);

    for (i = 0; i < num_thrs; i++) {
        thrs[i].connlm = connlm;
        thrs[i].tid = i;
        pthread_create(pts + i, NULL, connlm_train_thread,
                (void *)(thrs + i));
    }

    for (i = 0; i < num_thrs + 1; i++) {
        pthread_join(pts[i], NULL);
    }

    if (connlm->err != 0) {
        goto ERR;
    }

    for (i = 0; i < num_thrs; i++) {
        if (connlm_finish_train(connlm, i) < 0) {
            ST_WARNING("Failed to connlm_finish_train.");
            goto ERR;
        }
    }

    gettimeofday(&tte_train, NULL);
    ms = TIMEDIFF(tts_train, tte_train);

    words = 0;
    sents = 0;
    logp = 0.0;
    for (i = 0; i < num_thrs; i++) {
        words += thrs[i].words;
        sents += thrs[i].sents;
        logp += thrs[i].logp;
    }

    if (words != read_thr.words || sents != read_thr.sents) {
        ST_WARNING("words[" COUNT_FMT "] != read_words[" COUNT_FMT "] "
                "or sents[" COUNT_FMT "] != read_sents[" COUNT_FMT "] ",
                words, read_thr.words, sents, read_thr.sents);
    }

    ST_NOTICE("Finish train in %ldms.", ms);
    ST_NOTICE("Words: " COUNT_FMT ", Sentences: " COUNT_FMT
            ", words/sec: %.1f", words, sents,
            words / ((double) ms / 1000.0));
    ST_NOTICE("LogP: %f", logp);
    ST_NOTICE("Entropy: %f", -logp / log10(2) / words);
    ST_NOTICE("PPL: %f", exp10(-logp / (double) words));

    safe_free(pts);
    safe_free(thrs);

    return 0;

ERR:
    safe_free(pts);
    safe_free(thrs);

    return -1;
}

int connlm_setup_test(connlm_t *connlm, connlm_test_opt_t *test_opt,
        const char *test_file)
{
    int num_thrs;

    ST_CHECK_PARAM(connlm == NULL || test_opt == NULL
            || test_file == NULL, -1);

    connlm->test_opt = *test_opt;

    if (connlm->test_opt.num_thread < 1) {
        connlm->test_opt.num_thread = 1;
    }
    num_thrs = connlm->test_opt.num_thread;

    strncpy(connlm->text_file, test_file, MAX_DIR_LEN);
    connlm->text_file[MAX_DIR_LEN - 1] = '\0';

    connlm->pool_size = test_opt->num_thread;
    connlm->egs_pool = (connlm_egs_t *)malloc(sizeof(connlm_egs_t)
            * connlm->pool_size);
    if (connlm->egs_pool == NULL) {
        ST_WARNING("Failed to malloc egs_pool.");
        goto ERR;
    }
    memset(connlm->egs_pool, 0, sizeof(connlm_egs_t)*connlm->pool_size);

    if (st_sem_init(&connlm->sem_empty, connlm->pool_size) != 0) {
        ST_WARNING("Failed to st_sem_init sem_empty.");
        goto ERR;
    }

    if (st_sem_init(&connlm->sem_full, 0) != 0) {
        ST_WARNING("Failed to st_sem_init sem_full.");
        goto ERR;
    }

    if (pthread_mutex_init(&connlm->pool_lock, NULL) != 0) {
        ST_WARNING("Failed to pthread_mutex_init pool_lock.");
        goto ERR;
    }

    if (test_opt->debug_file[0] != '\0') {
        connlm->fp_debug = st_fopen(test_opt->debug_file, "w");
        if (connlm->fp_debug == NULL) {
            ST_WARNING("Failed to st_fopen debug_file[%s].",
                    test_opt->debug_file);
            goto ERR;
        }
        if (pthread_mutex_init(&connlm->fp_debug_lock, NULL) != 0) {
            ST_WARNING("Failed to pthread_mutex_init fp_debug_lock.");
            goto ERR;
        }
    }

    if (output_setup_test(connlm->output, num_thrs) < 0) {
        ST_WARNING("Failed to output_setup_test.");
        goto ERR;
    }

    if (connlm->rnn != NULL) {
        if (rnn_setup_test(connlm->rnn, connlm->output, num_thrs) < 0) {
            ST_WARNING("Failed to rnn_setup_test.");
            goto ERR;
        }
    }

    if (connlm->maxent != NULL) {
        if (maxent_setup_test(connlm->maxent, connlm->output,
                    num_thrs) < 0) {
            ST_WARNING("Failed to maxent_setup_test.");
            goto ERR;
        }
    }

    if (connlm->lbl != NULL) {
        if (lbl_setup_test(connlm->lbl, connlm->output, num_thrs) < 0) {
            ST_WARNING("Failed to lbl_setup_test.");
            goto ERR;
        }
    }

    if (connlm->ffnn != NULL) {
        if (ffnn_setup_test(connlm->ffnn, connlm->output, num_thrs) < 0) {
            ST_WARNING("Failed to ffnn_setup_test.");
            goto ERR;
        }
    }

    return 0;

ERR:
    safe_free(connlm->egs_pool);
    (void)pthread_mutex_destroy(&connlm->pool_lock);
    (void)st_sem_destroy(&connlm->sem_full);
    (void)st_sem_destroy(&connlm->sem_empty);
    safe_fclose(connlm->fp_debug);
    (void)pthread_mutex_destroy(&connlm->fp_debug_lock);

    return -1;
}

int connlm_reset_test(connlm_t *connlm, int tid)
{
    ST_CHECK_PARAM(connlm == NULL, -1);

    if (output_reset_test(connlm->output, tid) < 0) {
        ST_WARNING("Failed to output_reset_test.");
        return -1;
    }

    if (connlm->rnn != NULL) {
        if (rnn_reset_test(connlm->rnn, tid) < 0) {
            ST_WARNING("Failed to rnn_reset_test.");
            return -1;
        }
    }

    if (connlm->maxent != NULL) {
        if (maxent_reset_test(connlm->maxent, tid) < 0) {
            ST_WARNING("Failed to maxent_reset_test.");
            return -1;
        }
    }

    if (connlm->lbl != NULL) {
        if (lbl_reset_test(connlm->lbl, tid) < 0) {
            ST_WARNING("Failed to lbl_reset_test.");
            return -1;
        }
    }

    if (connlm->ffnn != NULL) {
        if (ffnn_reset_test(connlm->ffnn, tid) < 0) {
            ST_WARNING("Failed to ffnn_reset_test.");
            return -1;
        }
    }

    return 0;
}

int connlm_start_test(connlm_t *connlm, int word, int tid)
{
    ST_CHECK_PARAM(connlm == NULL, -1);

    if (output_start_test(connlm->output, word, tid) < 0) {
        ST_WARNING("Failed to output_start_test.");
        return -1;
    }

    if (connlm->rnn != NULL) {
        if (rnn_start_test(connlm->rnn, word, tid) < 0) {
            ST_WARNING("Failed to rnn_start_test.");
            return -1;
        }
    }

    if (connlm->maxent != NULL) {
        if (maxent_start_test(connlm->maxent, word, tid) < 0) {
            ST_WARNING("Failed to maxent_start_test.");
            return -1;
        }
    }

    if (connlm->lbl != NULL) {
        if (lbl_start_test(connlm->lbl, word, tid) < 0) {
            ST_WARNING("Failed to lbl_start_test.");
            return -1;
        }
    }

    if (connlm->ffnn != NULL) {
        if (ffnn_start_test(connlm->ffnn, word, tid) < 0) {
            ST_WARNING("Failed to ffnn_start_test.");
            return -1;
        }
    }

    return 0;
}

int connlm_end_test(connlm_t *connlm, int word, int tid)
{
    ST_CHECK_PARAM(connlm == NULL, -1);

    if (output_end_test(connlm->output, word, tid) < 0) {
        ST_WARNING("Failed to output_end_test.");
        return -1;
    }

    if (connlm->rnn != NULL) {
        if (rnn_end_test(connlm->rnn, word, tid) < 0) {
            ST_WARNING("Failed to rnn_end_test.");
            return -1;
        }
    }

    if (connlm->maxent != NULL) {
        if (maxent_end_test(connlm->maxent, word, tid) < 0) {
            ST_WARNING("Failed to maxent_end_test.");
            return -1;
        }
    }

    if (connlm->lbl != NULL) {
        if (lbl_end_test(connlm->lbl, word, tid) < 0) {
            ST_WARNING("Failed to lbl_end_test.");
            return -1;
        }
    }

    if (connlm->ffnn != NULL) {
        if (ffnn_end_test(connlm->ffnn, word, tid) < 0) {
            ST_WARNING("Failed to ffnn_end_test.");
            return -1;
        }
    }

    return 0;
}

static void* connlm_test_thread(void *args)
{
    connlm_t *connlm;
    connlm_thr_t *thr;
    int tid;

    connlm_egs_t *egs_in_pool;
    connlm_egs_t egs = {
        .words = NULL,
        .size = 0,
        .capacity = 0,
    };

    count_t words;
    count_t sents;
    count_t oovs;
    double logp;

    int word;
    double oov_penalty;
    double p;

    int i;

    ST_CHECK_PARAM(args == NULL, NULL);

    thr = (connlm_thr_t *)args;
    connlm = thr->connlm;
    tid = thr->tid;

    oov_penalty = connlm->test_opt.oov_penalty;

    words = 0;
    sents = 0;
    oovs = 0;
    logp = 0.0;
    while (true) {
        if (connlm->err != 0) {
            break;
        }

        if (st_sem_wait(&connlm->sem_full) != 0) {
            ST_WARNING("Failed to st_sem_wait sem_full.");
            goto ERR;
        }
        if (pthread_mutex_lock(&connlm->pool_lock) != 0) {
            ST_WARNING("Failed to pthread_mutex_lock pool_lock.");
            goto ERR;
        }

        egs_in_pool = connlm->egs_pool + connlm->pool_out;
        connlm->pool_out = (connlm->pool_out + 1) % connlm->pool_size;

        if (egs_in_pool->size > 0) {
            if (connlm_egs_ensure(&egs, egs_in_pool->size) < 0) {
                ST_WARNING("Failed to connlm_ensure_egs. pool_out[%d], "
                        "num_words[%d].", connlm->pool_out, egs_in_pool->size);
                goto ERR;
            }
            memcpy(egs.words, egs_in_pool->words, sizeof(int)*egs_in_pool->size);
        }
        egs.size = egs_in_pool->size;

        if (pthread_mutex_unlock(&connlm->pool_lock) != 0) {
            ST_WARNING("Failed to pthread_mutex_unlock pool_lock.");
            goto ERR;
        }
        if (st_sem_post(&connlm->sem_empty) != 0) {
            ST_WARNING("Failed to st_sem_post sem_empty.");
            goto ERR;
        }

        if (egs.size == -2) { // finish
            break;
        }

        if (connlm->fp_debug != NULL) {
            if (connlm_egs_print(connlm->fp_debug, &connlm->fp_debug_lock,
                        &egs, connlm->vocab) < 0) {
                ST_WARNING("Failed to connlm_egs_print.");
                goto ERR;
            }
        }

        if (connlm_reset_test(connlm, tid) < 0) {
            ST_WARNING("Failed to connlm_reset_test.");
            goto ERR;
        }
        for (i = 0; i < egs.size; i++) {
            word = egs.words[i];

            if (connlm_start_test(connlm, word, tid) < 0) {
                ST_WARNING("connlm_start_test.");
                goto ERR;
            }

            if (connlm_forward(connlm, word, tid) < 0) {
                ST_WARNING("Failed to connlm_forward.");
                goto ERR;
            }

            if (word == -1) {
                logp += oov_penalty;
                oovs++;
                p = 0;
            } else {
                p = output_get_prob(connlm->output, word, tid);
                logp += log10(p);

                if ((logp != logp) || (isinf(logp))) {
                    if (connlm->output->output_opt.class_size > 0) {
                        ST_WARNING("Numerical error. tid[%d], "
                                "p_class(%d) = %g, p_word(%d) = %g", tid,
                                connlm->output->w2c[word],
                                output_get_class_prob(connlm->output,
                                    word, tid),
                                word, 
                                output_get_word_prob(connlm->output,
                                    word, tid));
                    } else {
                        ST_WARNING("Numerical error. tid[%d], p_word(%d) = %g",
                                tid, word, 
                                output_get_word_prob(connlm->output,
                                    word, tid));
                    }
                    goto ERR;
                }
            }

            if (connlm_end_test(connlm, word, tid) < 0) {
                ST_WARNING("connlm_end_test.");
                goto ERR;
            }

            if (connlm->fp_log != NULL) {
                (void)pthread_mutex_lock(&connlm->fp_lock);
                if (word != -1) {
                    fprintf(connlm->fp_log, "%d\t%.10f\t%s", word, p,
                            vocab_get_word(connlm->vocab, word));
                } else {
                    fprintf(connlm->fp_log, "-1\t0\t\t<OOV>");
                }

                fprintf(connlm->fp_log, "\n");
                (void)pthread_mutex_unlock(&connlm->fp_lock);
            }

            if (word == 0) {
                if (connlm_reset_test(connlm, tid) < 0) {
                    ST_WARNING("Failed to connlm_reset_test.");
                    goto ERR;
                }
                sents++;
            }

            words++;
        }
    }

    thr->words = words;
    thr->sents = sents;
    thr->oovs = oovs;
    thr->logp = logp;

    connlm_egs_destroy(&egs);
    return NULL;
ERR:
    connlm->err = -1;

    connlm_egs_destroy(&egs);
    /* TODO: unlock mutex and post semaphore */
    return NULL;
}

int connlm_test(connlm_t *connlm, FILE *fp_log)
{
    connlm_thr_t *thrs = NULL;
    pthread_t *pts = NULL;

    connlm_read_thr_t read_thr;
    int num_thrs;

    count_t words;
    count_t sents;
    count_t oovs;
    double logp;

    int i;

    struct timeval tts_test, tte_test;
    long ms;

    ST_CHECK_PARAM(connlm == NULL, -1);

    if (fp_log != NULL) {
        connlm->fp_log = fp_log;
        pthread_mutex_init(&connlm->fp_lock, NULL);
        fprintf(fp_log, "Index   P(NET)          Word\n");
        fprintf(fp_log, "----------------------------------\n");
    }

    gettimeofday(&tts_test, NULL);

    num_thrs = connlm->test_opt.num_thread;
    pts = (pthread_t *)malloc((num_thrs + 1) * sizeof(pthread_t));
    if (pts == NULL) {
        ST_WARNING("Failed to malloc pts");
        goto ERR;
    }

    thrs = (connlm_thr_t *)malloc(sizeof(connlm_thr_t) * num_thrs);
    if (thrs == NULL) {
        ST_WARNING("Failed to malloc thrs");
        goto ERR;
    }
    memset(thrs, 0, sizeof(connlm_thr_t) * num_thrs);

    connlm->err = 0;
    connlm->pool_in = 0;
    connlm->pool_out = 0;

    read_thr.connlm = connlm;
    read_thr.thrs = thrs;
    read_thr.num_thrs = connlm->test_opt.num_thread;
    read_thr.epoch_size = connlm->test_opt.epoch_size;
    read_thr.shuffle = false;
    read_thr.random = 0;

    pthread_create(pts + num_thrs, NULL,
            connlm_read_thread, (void *)&read_thr);

    for (i = 0; i < num_thrs; i++) {
        thrs[i].connlm = connlm;
        thrs[i].tid = i;
        pthread_create(pts + i, NULL, connlm_test_thread,
                (void *)(thrs + i));
    }

    for (i = 0; i < num_thrs; i++) {
        pthread_join(pts[i], NULL);
    }

    if (connlm->err != 0) {
        ST_WARNING("Testing error.");
        goto ERR;
    }

    words = 0;
    sents = 0;
    oovs = 0;
    logp = 0;

    for (i = 0; i < num_thrs; i++) {
        words += thrs[i].words;
        sents += thrs[i].sents;
        oovs += thrs[i].oovs;
        logp += thrs[i].logp;
    }

    if (words != read_thr.words || sents != read_thr.sents) {
        ST_WARNING("words[" COUNT_FMT "] != read_words[" COUNT_FMT "] "
                "or sents[" COUNT_FMT "] != read_sents[" COUNT_FMT "] ",
                words, read_thr.words, sents, read_thr.sents);
    }

    gettimeofday(&tte_test, NULL);
    ms = TIMEDIFF(tts_test, tte_test);

    ST_NOTICE("Finish test in %ldms.", ms);

    ST_NOTICE("Words: " COUNT_FMT "    OOVs: " COUNT_FMT
            "    Sentences: " COUNT_FMT
            ", words/sec: %.1f", words, oovs, sents,
             words / ((double) ms / 1000));
    ST_NOTICE("LogP: %f", logp);
    ST_NOTICE("Entropy: %f", -logp / log10(2) / words);
    ST_NOTICE("PPL: %f", exp10(-logp / (double) words));

    if (fp_log != NULL) {
        fprintf(fp_log, "\nSummary:\n");
        fprintf(fp_log, "Words: " COUNT_FMT "    OOVs: " COUNT_FMT "\n",
                words, oovs);
        fprintf(fp_log, "LogP: %f\n", logp);
        fprintf(fp_log, "Entropy: %f\n", -logp / log10(2) / words);
        fprintf(fp_log, "PPL: %f\n", exp10(-logp / (double) words));

        pthread_mutex_destroy(&connlm->fp_lock);
    }

    safe_free(pts);
    safe_free(thrs);

    return 0;

ERR:

    if (fp_log != NULL) {
        pthread_mutex_destroy(&connlm->fp_lock);
    }
    safe_free(pts);
    safe_free(thrs);

    return -1;
}
/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */

