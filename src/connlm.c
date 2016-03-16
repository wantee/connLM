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

#include <stutils/st_macro.h>
#include <stutils/st_log.h>
#include <stutils/st_opt.h>
#include <stutils/st_io.h>

#include "utils.h"
#include "connlm.h"

#define NUM_WORD_PER_SENT 128

static const int CONNLM_MAGIC_NUM = 626140498;

const char* connlm_revision()
{
    return CONNLM_GIT_COMMIT;
}

static void connlm_egs_destroy(connlm_egs_t *egs)
{
    if (egs != NULL) {
        safe_free(egs->words);
        egs->capacity = 0;
        egs->size = 0;
    }
}

//#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-overflow"
static int connlm_egs_ensure(connlm_egs_t *egs, int capacity)
{
    ST_CHECK_PARAM(egs == NULL, -1);

    if (egs->capacity < capacity) {
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
//#pragma GCC diagnostic pop

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
        while (egs->words[i] != SENT_END_ID && i < egs->size) {
            word = egs->words[i];

            if (vocab != NULL) {
                fprintf(fp, "%s", vocab_get_word(vocab, word));
            } else {
                fprintf(fp, "<%d>", word);
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
    ST_OPT_SEC_GET_UINT(opt, sec_name, "RANDOM_SEED",
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

int connlm_load_eval_opt(connlm_eval_opt_t *eval_opt,
        st_opt_t *opt, const char *sec_name)
{
    char str[MAX_ST_CONF_LEN];

    ST_CHECK_PARAM(eval_opt == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_INT(opt, sec_name, "NUM_THREAD",
            eval_opt->num_thread, 1, "Number of threads");

    ST_OPT_SEC_GET_INT(opt, sec_name, "EPOCH_SIZE",
            eval_opt->epoch_size, 1,
            "Number of sentences read in one epoch per thread (in kilos)");
    eval_opt->epoch_size *= 1000;

    ST_OPT_SEC_GET_BOOL(opt, sec_name, "PRINT_SENT_PROB",
            eval_opt->print_sent_prob, false,
            "Print sentence prob only, if true. ");

    ST_OPT_SEC_GET_STR(opt, sec_name, "OUT_LOG_BASE",
            str, MAX_ST_CONF_LEN, "e",
            "Log base for printing prob. Could be 'e' or other number.");
    if (str[0] == 'e' && str[1] == '\0') {
        eval_opt->out_log_base = 0;
    } else {
        eval_opt->out_log_base = (real_t)atof(str);
    }

    ST_OPT_SEC_GET_STR(opt, sec_name, "DEBUG_FILE",
            eval_opt->debug_file, MAX_DIR_LEN, "",
            "file to print out debug infos.");

    return 0;

ST_OPT_ERR:
    return -1;
}

int connlm_load_gen_opt(connlm_gen_opt_t *gen_opt,
        st_opt_t *opt, const char *sec_name)
{
    ST_CHECK_PARAM(gen_opt == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_UINT(opt, sec_name, "RANDOM_SEED",
            gen_opt->rand_seed, (unsigned int)time(NULL),
            "Random seed. Default is value of time(NULl).");

    ST_OPT_SEC_GET_STR(opt, sec_name, "PREFIX_FILE",
            gen_opt->prefix_file, MAX_DIR_LEN, "",
            "File contains text prefix to be generated after");

    return 0;

ST_OPT_ERR:
    return -1;
}

void connlm_destroy(connlm_t *connlm)
{
    connlm_egs_t *p;
    connlm_egs_t *q;

    comp_id_t c;

    if (connlm == NULL) {
        return;
    }

    p = connlm->full_egs;
    while (p != NULL) {
        q = p;
        p = p->next;

        connlm_egs_destroy(q);
        safe_free(q);
    }
    connlm->full_egs = NULL;

    p = connlm->empty_egs;;
    while (p != NULL) {
        q = p;
        p = p->next;

        connlm_egs_destroy(q);
        safe_free(q);
    }
    connlm->empty_egs = NULL;

    (void)pthread_mutex_destroy(&connlm->full_egs_lock);
    (void)pthread_mutex_destroy(&connlm->empty_egs_lock);
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

    for (c = 0; c < connlm->num_comp; c++) {
        safe_comp_destroy(connlm->comps[c]);
    }
    safe_free(connlm->comps);
    connlm->num_comp = 0;
}

int connlm_init(connlm_t *connlm, FILE *topo_fp)
{
    char *content = NULL;
    size_t content_sz = 0;
    size_t content_cap = 0;

    char *line = NULL;
    size_t line_sz = 0;

    comp_id_t c, d;
    bool err;

    bool is_content;

    ST_CHECK_PARAM(connlm == NULL || topo_fp == NULL, -1);

    safe_free(connlm->comps);
    connlm->num_comp = 0;
    is_content = false;
    while (st_fgets(&line, &line_sz, topo_fp, &err)) {
        trim(line);

        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        if (!is_content) {
            if (strncasecmp("<component>", line, 11) == 0) {
                is_content = true;
            } 
            continue;
        }

        if (strncasecmp("</component>", line, 12) == 0) {
            connlm->comps = (component_t **)realloc(connlm->comps,
                    sizeof(component_t *) * (connlm->num_comp + 1));
            if (connlm->comps == NULL) {
                ST_WARNING("Failed to alloc comps.");
                goto ERR;
            }
            connlm->comps[connlm->num_comp] = comp_init_from_topo(content);
            if (connlm->comps[connlm->num_comp] == NULL) {
                ST_WARNING("Failed to comp_init_from_topo.");
                goto ERR;
            }
            connlm->num_comp++;

            content_sz = 0;
            is_content = false;
            continue;
        } 

        if (strlen(line) + 2 + content_sz > content_cap) {
            content_cap += (strlen(line) + 2);
            content = (char *)realloc(content, content_cap);
            if (content == NULL) {
                ST_WARNING("Failed to realloc content.");
                goto ERR;
            }
        }

        strcpy(content + content_sz, line);
        content_sz += strlen(line);
        content[content_sz] = '\n';
        content_sz++;
        content[content_sz] = '\0';
    }

    if (err) {
        ST_WARNING("st_fgets error.");
        goto ERR;
    }

    if (connlm->comps == NULL) {
        ST_WARNING("No componet found.");
        goto ERR;
    }

    safe_free(line);
    safe_free(content);

    for (c = 0; c < connlm->num_comp - 1; c++) {
        for (d = c+1; d < connlm->num_comp; d++) {
            if (strcmp(connlm->comps[c]->name, connlm->comps[d]->name) == 0) {
                ST_WARNING("Duplicated component name[%s]",
                        connlm->comps[c]->name);
                goto ERR;
            }
        }
    }

    for (c = 0; c < connlm->num_comp; c++) {
        if (comp_construct_graph(connlm->comps[c]) < 0) {
            ST_WARNING("Failed to construct graph for component [%s]",
                    connlm->comps[c]->name);
            goto ERR;
        }
    }

    return 0;

ERR:
    safe_free(line);
    safe_free(content);

    for (c = 0; c < connlm->num_comp; c++) {
        safe_comp_destroy(connlm->comps[c]);
    }
    safe_free(connlm->comps);
    connlm->num_comp = 0;

    return -1;
}

connlm_t *connlm_new(vocab_t *vocab, output_t *output,
        component_t **comps, int n_comp)
{
    connlm_t *connlm = NULL;

    comp_id_t c;

    ST_CHECK_PARAM(vocab == NULL && output == NULL
            && comps == NULL, NULL);

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

    if (comps != NULL && n_comp > 0) {
        connlm->comps = (component_t **)malloc(sizeof(component_t *)
                * n_comp);
        if (connlm->comps == NULL) {
            ST_WARNING("Failed to malloc comps.");
            goto ERR;
        }

        for (c = 0; c < n_comp; c++) {
            connlm->comps[c] = comp_dup(comps[c]);
            if (connlm->comps[c] == NULL) {
                ST_WARNING("Failed to comp_dup. c[%d]", c);
                goto ERR;
            }
        }
        connlm->num_comp = n_comp;
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
                    "please upgrade connlm toolkit", version);
            return -1;
        }

        if (version < 3) {
            ST_WARNING("File versoin[%d] less than 3 is not supported, "
                    "please downgrade connlm toolkit", version);
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
                version, fp, binary, fo_info) < 0) {
        ST_WARNING("Failed to vocab_load_header.");
        return -1;
    }
    if (*binary != b) {
        ST_WARNING("Both binary and text format in one file.");
        return -1;
    }

    if (output_load_header((connlm == NULL) ? NULL : &connlm->output,
                version, fp, binary, fo_info) < 0) {
        ST_WARNING("Failed to output_load_header.");
        return -1;
    }
    if (*binary != b) {
        ST_WARNING("Both binary and text format in one file.");
        return -1;
    }

    if (rnn_load_header((connlm == NULL) ? NULL : &connlm->rnn,
                version, fp, binary, fo_info) < 0) {
        ST_WARNING("Failed to rnn_load_header.");
        return -1;
    }
    if (*binary != b) {
        ST_WARNING("Both binary and text format in one file.");
        return -1;
    }

    if (maxent_load_header((connlm == NULL) ? NULL : &connlm->maxent,
                version, fp, binary, fo_info) < 0) {
        ST_WARNING("Failed to maxent_load_header.");
        return -1;
    }
    if (*binary != b) {
        ST_WARNING("Both binary and text format in one file.");
        return -1;
    }

    if (lbl_load_header((connlm == NULL) ? NULL : &connlm->lbl,
                version, fp, binary, fo_info) < 0) {
        ST_WARNING("Failed to lbl_load_header.");
        return -1;
    }
    if (*binary != b) {
        ST_WARNING("Both binary and text format in one file.");
        return -1;
    }

    if (ffnn_load_header((connlm == NULL) ? NULL : &connlm->ffnn,
                version, fp, binary, fo_info) < 0) {
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

    return version;
}

static int connlm_load_body(connlm_t *connlm, int version,
        FILE *fp, bool binary)
{
    ST_CHECK_PARAM(connlm == NULL || fp == NULL, -1);

    if (connlm->vocab != NULL) {
        if (vocab_load_body(connlm->vocab, version, fp, binary) < 0) {
            ST_WARNING("Failed to vocab_load_body.");
            return -1;
        }
    }

    if (connlm->output != NULL) {
        if (output_load_body(connlm->output, version, fp, binary) < 0) {
            ST_WARNING("Failed to output_load_body.");
            return -1;
        }
    }
    if (connlm->rnn != NULL) {
        if (rnn_load_body(connlm->rnn, version, fp, binary) < 0) {
            ST_WARNING("Failed to rnn_load_body.");
            return -1;
        }
    }

    if (connlm->maxent != NULL) {
        if (maxent_load_body(connlm->maxent, version, fp, binary) < 0) {
            ST_WARNING("Failed to maxent_load_body.");
            return -1;
        }
    }

    if (connlm->lbl != NULL) {
        if (lbl_load_body(connlm->lbl, version, fp, binary) < 0) {
            ST_WARNING("Failed to lbl_load_body.");
            return -1;
        }
    }

    if (connlm->ffnn != NULL) {
        if (ffnn_load_body(connlm->ffnn, version, fp, binary) < 0) {
            ST_WARNING("Failed to ffnn_load_body.");
            return -1;
        }
    }

    return 0;
}

connlm_t* connlm_load(FILE *fp)
{
    connlm_t *connlm = NULL;
    int version;
    bool binary;

    ST_CHECK_PARAM(fp == NULL, NULL);

    connlm = (connlm_t *)malloc(sizeof(connlm_t));
    if (connlm == NULL) {
        ST_WARNING("Failed to malloc connlm_t");
        goto ERR;
    }
    memset(connlm, 0, sizeof(connlm_t));

    version = connlm_load_header(connlm, fp, &binary, NULL);
    if (version < 0) {
        ST_WARNING("Failed to connlm_load_header.");
        goto ERR;
    }

    if (connlm_load_body(connlm, version, fp, binary) < 0) {
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
        int epoch_size, FILE *text_fp, vocab_t *vocab, int *oovs)
{
    char *line = NULL;
    size_t line_sz = 0;
    char word[MAX_LINE_LEN];

    char *p;

    int num_sents;
    int oov;
    int i;

    bool err;

    // assert len(sent_ends) >= epoch_size
    ST_CHECK_PARAM(egs == NULL || text_fp == NULL, -1);

    err = false;
    num_sents = 0;
    oov = 0;
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
                    if (egs->words[egs->size] < 0
                            || egs->words[egs->size] == UNK_ID) {
                        egs->words[egs->size] = UNK_ID;
                        oov++;
                    }
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
            if (egs->words[egs->size] < 0
                    || egs->words[egs->size] == UNK_ID) {
                egs->words[egs->size] = UNK_ID;
                oov++;
            }
            egs->size++;

            i = 0;
        }

        egs->words[egs->size] = SENT_END_ID;
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

    if (oovs != NULL) {
        *oovs = oov;
    }

    return num_sents;

ERR:

    safe_free(line);
    return -1;
}

int connlm_setup_train(connlm_t *connlm, connlm_train_opt_t *train_opt,
        const char *train_file)
{
    connlm_egs_t *egs;
    connlm_egs_t *p;
    connlm_egs_t *q;
    int pool_size;

    int num_thrs;

    int i;

    ST_CHECK_PARAM(connlm == NULL || train_opt == NULL
            || train_file == NULL, -1);

    connlm->train_opt = *train_opt;

    if (connlm->train_opt.num_thread < 1) {
        connlm->train_opt.num_thread = 1;
    }
    num_thrs = connlm->train_opt.num_thread;

    strncpy(connlm->text_file, train_file, MAX_DIR_LEN);
    connlm->text_file[MAX_DIR_LEN - 1] = '\0';

    pool_size = 2 * train_opt->num_thread;
    connlm->full_egs = NULL;
    connlm->empty_egs = NULL;;
    for (i = 0; i < pool_size; i++) {
        egs = (connlm_egs_t *)malloc(sizeof(connlm_egs_t));
        if (egs == NULL) {
            ST_WARNING("Failed to malloc egs.");
            goto ERR;
        }
        memset(egs, 0, sizeof(connlm_egs_t));
        egs->next = connlm->empty_egs;
        connlm->empty_egs = egs;
    }

    if (st_sem_init(&connlm->sem_empty, pool_size) != 0) {
        ST_WARNING("Failed to st_sem_init sem_empty.");
        goto ERR;
    }

    if (st_sem_init(&connlm->sem_full, 0) != 0) {
        ST_WARNING("Failed to st_sem_init sem_full.");
        goto ERR;
    }

    if (pthread_mutex_init(&connlm->full_egs_lock, NULL) != 0) {
        ST_WARNING("Failed to pthread_mutex_init full_egs_lock.");
        goto ERR;
    }

    if (pthread_mutex_init(&connlm->empty_egs_lock, NULL) != 0) {
        ST_WARNING("Failed to pthread_mutex_init empty_egs_lock.");
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
    p = connlm->full_egs;
    while (p != NULL) {
        q = p;
        p = p->next;

        connlm_egs_destroy(q);
        safe_free(q);
    }
    connlm->full_egs = NULL;

    p = connlm->empty_egs;
    while (p != NULL) {
        q = p;
        p = p->next;

        connlm_egs_destroy(q);
        safe_free(q);
    }
    connlm->empty_egs = NULL;

    (void)pthread_mutex_destroy(&connlm->full_egs_lock);
    (void)pthread_mutex_destroy(&connlm->empty_egs_lock);
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

/* forward non-output-word layer */
int connlm_forward_pre_layer(connlm_t *connlm, int cls, int tid)
{
    ST_CHECK_PARAM(connlm == NULL, -1);

    if (connlm->rnn != NULL) {
        if (rnn_forward_pre_layer(connlm->rnn, tid) < 0) {
            ST_WARNING("Failed to rnn_forward_pre_layer.");
            return -1;
        }
    }

    if (connlm->maxent != NULL) {
        if (maxent_forward_pre_layer(connlm->maxent, tid) < 0) {
            ST_WARNING("Failed to maxent_forward_pre_layer.");
            return -1;
        }
    }

    if (connlm->lbl != NULL) {
        if (lbl_forward_pre_layer(connlm->lbl, tid) < 0) {
            ST_WARNING("Failed to lbl_forward_pre_layer.");
            return -1;
        }
    }

    if (connlm->ffnn != NULL) {
        if (ffnn_forward_pre_layer(connlm->ffnn, tid) < 0) {
            ST_WARNING("Failed to ffnn_forward_pre_layer.");
            return -1;
        }
    }

    if (output_activate_pre_layer(connlm->output, cls, tid) < 0) {
        ST_WARNING("Failed to output_activate_pre_layer.");
        return -1;
    }

    return 0;
}

/* forward the last output-word layer */
int connlm_forward_last_layer(connlm_t *connlm, int word, int tid)
{
    ST_CHECK_PARAM(connlm == NULL, -1);

    if (connlm->rnn != NULL) {
        if (rnn_forward_last_layer(connlm->rnn, word, tid) < 0) {
            ST_WARNING("Failed to rnn_forward_last_layer.");
            return -1;
        }
    }

    if (connlm->maxent != NULL) {
        if (maxent_forward_last_layer(connlm->maxent, word, tid) < 0) {
            ST_WARNING("Failed to maxent_forward_last_layer.");
            return -1;
        }
    }

    if (connlm->lbl != NULL) {
        if (lbl_forward_last_layer(connlm->lbl, word, tid) < 0) {
            ST_WARNING("Failed to lbl_forward_last_layer.");
            return -1;
        }
    }

    if (connlm->ffnn != NULL) {
        if (ffnn_forward_last_layer(connlm->ffnn, word, tid) < 0) {
            ST_WARNING("Failed to ffnn_forward_last_layer.");
            return -1;
        }
    }

    if (output_activate_last_layer(connlm->output, word, tid) < 0) {
        ST_WARNING("Failed to output_activate_last_layer.");
        return -1;
    }

    return 0;
}

int connlm_forward(connlm_t *connlm, int word, int tid)
{
    int cls;

    ST_CHECK_PARAM(connlm == NULL, -1);

    if (connlm->output->output_opt.class_size > 0) {
        cls = connlm->output->w2c[word];
    } else {
        cls = -1;
    }

    if (connlm_forward_pre_layer(connlm, cls, tid) < 0) {
        ST_WARNING("Failed to connlm_forward_pre_layer.");
        return -1;
    }

    if (connlm_forward_last_layer(connlm, word, tid) < 0) {
        ST_WARNING("Failed to connlm_forward_last_layer.");
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
    count_t oovs;
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
    int oovs;
    int i;
    int start;
    int end;
    int num_reads;

    count_t total_words;
    count_t total_sents;
    double logp;
    struct timeval tts, tte;
    long ms = 0;
#ifdef _TIME_PROF_
    struct timeval tts_io, tte_io;
    long ms_io = 0;
    struct timeval tts_shuf, tte_shuf;
    long ms_shuf = 0;
    struct timeval tts_lock, tte_lock;
    long ms_lock = 0;
    struct timeval tts_fill, tte_fill;
    long ms_fill = 0;
#endif

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
        ST_WARNING("Failed to open text file[%s]", connlm->text_file);
        goto ERR;
    }

    num_reads = 0;
    read_thr->oovs = 0;
    read_thr->sents = 0;
    read_thr->words = 0;
    gettimeofday(&tts, NULL);
    while (!feof(text_fp)) {
        if (connlm->err != 0) {
            break;
        }

#ifdef _TIME_PROF_
        gettimeofday(&tts_io, NULL);
#endif
        num_sents = connlm_get_egs(&egs, sent_ends,
                epoch_size, text_fp, connlm->vocab, &oovs);
        if (num_sents < 0) {
            ST_WARNING("Failed to connlm_get_egs.");
            goto ERR;
        }
#ifdef _TIME_PROF_
        gettimeofday(&tte_io, NULL);
#endif
        if (num_sents == 0) {
            continue;
        }

        read_thr->oovs += oovs;
        read_thr->sents += num_sents;
        read_thr->words += egs.size;

#ifdef _TIME_PROF_
        gettimeofday(&tts_shuf, NULL);
#endif
        if (shuffle_buf != NULL) {
            for (i = 0; i < num_sents; i++) {
                shuffle_buf[i] = i;
            }

            st_shuffle_r(shuffle_buf, num_sents, &read_thr->random);
        }
#ifdef _TIME_PROF_
        gettimeofday(&tte_shuf, NULL);
#endif

#ifdef _TIME_PROF_
        gettimeofday(&tts_lock, NULL);
#endif
        if (st_sem_wait(&connlm->sem_empty) != 0) {
            ST_WARNING("Failed to st_sem_wait sem_empty.");
            goto ERR;
        }

        if (pthread_mutex_lock(&connlm->empty_egs_lock) != 0) {
            ST_WARNING("Failed to pthread_mutex_lock empty_egs_lock.");
            goto ERR;
        }
        egs_in_pool = connlm->empty_egs;
        connlm->empty_egs = connlm->empty_egs->next;
        if (pthread_mutex_unlock(&connlm->empty_egs_lock) != 0) {
            ST_WARNING("Failed to pthread_mutex_unlock empty_egs_lock.");
            goto ERR;
        }
#ifdef _TIME_PROF_
        gettimeofday(&tte_lock, NULL);
#endif

#ifdef _TIME_PROF_
        gettimeofday(&tts_fill, NULL);
#endif
        if (connlm_egs_ensure(egs_in_pool, egs.size) < 0) {
            ST_WARNING("Failed to connlm_ensure_egs. size[%d].", egs.size);
            goto ERR;
        }

        if (shuffle_buf != NULL) {
            egs_in_pool->size = 0;
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
#ifdef _TIME_PROF_
        gettimeofday(&tte_fill, NULL);
#endif

        if (pthread_mutex_lock(&connlm->full_egs_lock) != 0) {
            ST_WARNING("Failed to pthread_mutex_lock full_egs_lock.");
            goto ERR;
        }
        egs_in_pool->next = connlm->full_egs;
        connlm->full_egs = egs_in_pool;
        //ST_DEBUG("IN: %p, %d", egs_in_pool, egs_in_pool->size);
        if (pthread_mutex_unlock(&connlm->full_egs_lock) != 0) {
            ST_WARNING("Failed to pthread_mutex_unlock full_egs_lock.");
            goto ERR;
        }
        if (st_sem_post(&connlm->sem_full) != 0) {
            ST_WARNING("Failed to st_sem_post sem_full.");
            goto ERR;
        }

#ifdef _TIME_PROF_
        gettimeofday(&tte, NULL);
        ms = TIMEDIFF(tts, tte);
        ms_io += TIMEDIFF(tts_io, tte_io);
        ms_shuf += TIMEDIFF(tts_shuf, tte_shuf);
        ms_lock += TIMEDIFF(tts_lock, tte_lock);
        ms_fill += TIMEDIFF(tts_fill, tte_fill);
        ST_TRACE("Time: %.3fs, I/O: %.3f(%.2f%%), Shuf: %.3f(%.2f%%), Lock: %.3f(%.2f%%), Fill: %.3f(%.2f%%)",
                ms / 1000.0, ms_io / 1000.0, ms_io / (ms / 100.0),
                ms_shuf / 1000.0, ms_shuf / (ms / 100.0),
                ms_lock / 1000.0, ms_lock / (ms / 100.0),
                ms_fill / 1000.0, ms_fill / (ms / 100.0));
#endif

        num_reads++;
        if (num_reads >= num_thrs) {
            num_reads = 0;

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
                    ST_TRACE("Total progress: %.2f%%. "
                            "Words: " COUNT_FMT ", Sentences: " COUNT_FMT
                            ", OOVs: " COUNT_FMT ", words/sec: %.1f, "
                            "LogP: %f, Entropy: %f, PPL: %f, Time: %.3fs",
                            ftell(text_fp) / (fsize / 100.0),
                            total_words, total_sents, read_thr->oovs,
                            total_words / ((double) ms / 1000.0),
                            logp, -logp / log10(2) / total_words,
                            exp10(-logp / (double) total_words),
                            ms / 1000.0);
                } else {
                    ST_TRACE("Words: " COUNT_FMT ", Sentences: " COUNT_FMT
                            ", OOVs: " COUNT_FMT ", words/sec: %.1f, "
                            "LogP: %f, Entropy: %f, PPL: %f, Time: %.3fs",
                            total_words, total_sents, read_thr->oovs,
                            total_words / ((double) ms / 1000.0),
                            logp, -logp / log10(2) / total_words,
                            exp10(-logp / (double) total_words),
                            ms / 1000.0);
                }
            }
        }
    }

    for (i = 0; i < num_thrs; i++) {
        /* posting semaphore without adding data indicates finish. */
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
        /* posting semaphore without adding data indicates finish. */
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

    connlm_egs_t *egs = NULL;

    count_t words;
    count_t sents;
    double logp;
    bool finish;

    int word;
    int i;

    struct timeval tts, tte;
    long ms;

    struct timeval tts_wait, tte_wait;
    long ms_wait = 0;

    ST_CHECK_PARAM(args == NULL, NULL);

    thr = (connlm_thr_t *)args;
    connlm = thr->connlm;
    tid = thr->tid;

    gettimeofday(&tts, NULL);

    finish = false;
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
        if (pthread_mutex_lock(&connlm->full_egs_lock) != 0) {
            ST_WARNING("Failed to pthread_mutex_lock full_egs_lock.");
            goto ERR;
        }

        if (connlm->full_egs == NULL) {
            finish = true;
        } else {
            egs = connlm->full_egs;
            connlm->full_egs = connlm->full_egs->next;
        }

        if (pthread_mutex_unlock(&connlm->full_egs_lock) != 0) {
            ST_WARNING("Failed to pthread_mutex_unlock full_egs_lock.");
            goto ERR;
        }
        gettimeofday(&tte_wait, NULL);

        //ST_DEBUG("OUT: %s, %p, %d", bool2str(finish), egs, egs->size);
        if (finish) {
            break;
        }

        if (connlm->fp_debug != NULL) {
            if (connlm_egs_print(connlm->fp_debug, &connlm->fp_debug_lock,
                        egs, connlm->vocab) < 0) {
                ST_WARNING("Failed to connlm_egs_print.");
                goto ERR;
            }
        }

        if (connlm_reset_train(connlm, tid) < 0) {
            ST_WARNING("Failed to connlm_reset_train.");
            goto ERR;
        }

        for (i = 0; i < egs->size; i++) {
            word = egs->words[i];

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

            if (word == SENT_END_ID) {
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
        ms_wait += TIMEDIFF(tts_wait, tte_wait);

        ST_TRACE("Thread: %d, Words: " COUNT_FMT
                ", Sentences: " COUNT_FMT ", words/sec: %.1f, "
                "LogP: %f, Entropy: %f, PPL: %f, "
                "Time(cpu/wait): %.3fs(%.2f%%)/%.3fs(%.2f%%):%.3f",
                tid, words, sents, words / ((double) ms / 1000.0),
                logp, -logp / log10(2) / words,
                exp10(-logp / (double) words),
                (ms - ms_wait) / 1000.0, (ms - ms_wait) / (ms / 100.0),
                ms_wait / 1000.0, ms_wait / (ms / 100.0),
                TIMEDIFF(tts_wait, tte_wait) / 1000.0);

        if (pthread_mutex_lock(&connlm->empty_egs_lock) != 0) {
            ST_WARNING("Failed to pthread_mutex_lock empty_egs_lock.");
            goto ERR;
        }
        egs->next = connlm->empty_egs;
        connlm->empty_egs = egs;
        if (pthread_mutex_unlock(&connlm->empty_egs_lock) != 0) {
            ST_WARNING("Failed to pthread_mutex_unlock empty_egs_lock.");
            goto ERR;
        }
        if (st_sem_post(&connlm->sem_empty) != 0) {
            ST_WARNING("Failed to st_sem_post sem_empty.");
            goto ERR;
        }
    }

    return NULL;
ERR:
    connlm->err = -1;

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
            ", OOVs: " COUNT_FMT ", words/sec: %.1f", words, sents,
            read_thr.oovs, words / ((double) ms / 1000.0));
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

int connlm_setup_eval(connlm_t *connlm, connlm_eval_opt_t *eval_opt,
        const char *test_file)
{
    connlm_egs_t *egs;
    connlm_egs_t *p;
    connlm_egs_t *q;
    int pool_size;

    int num_thrs;

    int i;

    ST_CHECK_PARAM(connlm == NULL || eval_opt == NULL
            || test_file == NULL, -1);

    connlm->eval_opt = *eval_opt;

    if (connlm->eval_opt.num_thread < 1) {
        connlm->eval_opt.num_thread = 1;
    }
    num_thrs = connlm->eval_opt.num_thread;

    strncpy(connlm->text_file, test_file, MAX_DIR_LEN);
    connlm->text_file[MAX_DIR_LEN - 1] = '\0';

    pool_size = 2 * eval_opt->num_thread;
    connlm->full_egs = NULL;
    connlm->empty_egs = NULL;;
    for (i = 0; i < pool_size; i++) {
        egs = (connlm_egs_t *)malloc(sizeof(connlm_egs_t));
        if (egs == NULL) {
            ST_WARNING("Failed to malloc egs.");
            goto ERR;
        }
        memset(egs, 0, sizeof(connlm_egs_t));
        egs->next = connlm->empty_egs;
        connlm->empty_egs = egs;
    }

    if (st_sem_init(&connlm->sem_empty, pool_size) != 0) {
        ST_WARNING("Failed to st_sem_init sem_empty.");
        goto ERR;
    }

    if (st_sem_init(&connlm->sem_full, 0) != 0) {
        ST_WARNING("Failed to st_sem_init sem_full.");
        goto ERR;
    }

    if (pthread_mutex_init(&connlm->full_egs_lock, NULL) != 0) {
        ST_WARNING("Failed to pthread_mutex_init full_egs_lock.");
        goto ERR;
    }

    if (pthread_mutex_init(&connlm->empty_egs_lock, NULL) != 0) {
        ST_WARNING("Failed to pthread_mutex_init empty_egs_lock.");
        goto ERR;
    }

    if (eval_opt->debug_file[0] != '\0') {
        connlm->fp_debug = st_fopen(eval_opt->debug_file, "w");
        if (connlm->fp_debug == NULL) {
            ST_WARNING("Failed to st_fopen debug_file[%s].",
                    eval_opt->debug_file);
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
    p = connlm->full_egs;
    while (p != NULL) {
        q = p;
        p = p->next;

        connlm_egs_destroy(q);
        safe_free(q);
    }
    connlm->full_egs = NULL;

    p = connlm->empty_egs;;
    while (p != NULL) {
        q = p;
        p = p->next;

        connlm_egs_destroy(q);
        safe_free(q);
    }
    connlm->empty_egs = NULL;
    (void)pthread_mutex_destroy(&connlm->full_egs_lock);
    (void)pthread_mutex_destroy(&connlm->empty_egs_lock);
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

static void* connlm_eval_thread(void *args)
{
    connlm_t *connlm;
    connlm_thr_t *thr;
    int tid;

    connlm_egs_t *egs = NULL;

    count_t words;
    count_t sents;
    double logp;
    double logp_sent;
    bool finish;

    int word;
    double p;

    int i;

    ST_CHECK_PARAM(args == NULL, NULL);

    thr = (connlm_thr_t *)args;
    connlm = thr->connlm;
    tid = thr->tid;

    finish = false;
    words = 0;
    sents = 0;
    logp = 0.0;
    while (true) {
        if (connlm->err != 0) {
            break;
        }

        if (st_sem_wait(&connlm->sem_full) != 0) {
            ST_WARNING("Failed to st_sem_wait sem_full.");
            goto ERR;
        }
        if (pthread_mutex_lock(&connlm->full_egs_lock) != 0) {
            ST_WARNING("Failed to pthread_mutex_lock full_egs_lock.");
            goto ERR;
        }

        if (connlm->full_egs == NULL) {
            finish = true;
        } else {
            if (connlm->eval_opt.num_thread == 1) {
                /* ensure FIFO */
                connlm_egs_t *prev_egs = NULL;
                egs = connlm->full_egs;
                while (egs->next != NULL) {
                    /* full_egs list will not be too long */
                    prev_egs = egs;
                    egs = egs->next;
                }

                if (prev_egs == NULL) {
                    connlm->full_egs = NULL;
                } else {
                    prev_egs->next = NULL;
                }
            } else {
                egs = connlm->full_egs;
                connlm->full_egs = connlm->full_egs->next;
            }
        }

        if (pthread_mutex_unlock(&connlm->full_egs_lock) != 0) {
            ST_WARNING("Failed to pthread_mutex_unlock full_egs_lock.");
            goto ERR;
        }
        //ST_DEBUG("OUT: %s, %p, %d", bool2str(finish), egs, egs->size);
        if (finish) {
            break;
        }

        if (connlm->fp_debug != NULL) {
            if (connlm_egs_print(connlm->fp_debug, &connlm->fp_debug_lock,
                        egs, connlm->vocab) < 0) {
                ST_WARNING("Failed to connlm_egs_print.");
                goto ERR;
            }
        }

        if (connlm_reset_test(connlm, tid) < 0) {
            ST_WARNING("Failed to connlm_reset_test.");
            goto ERR;
        }
        logp_sent = 0.0;
        for (i = 0; i < egs->size; i++) {
            word = egs->words[i];

            if (connlm_start_test(connlm, word, tid) < 0) {
                ST_WARNING("connlm_start_test.");
                goto ERR;
            }

            if (connlm_forward(connlm, word, tid) < 0) {
                ST_WARNING("Failed to connlm_forward.");
                goto ERR;
            }

            p = output_get_prob(connlm->output, word, tid);
            logp += log10(p);
            logp_sent += logn(p, connlm->eval_opt.out_log_base);

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

            if (connlm_end_test(connlm, word, tid) < 0) {
                ST_WARNING("connlm_end_test.");
                goto ERR;
            }

            if (connlm->fp_log != NULL
                    && (! connlm->eval_opt.print_sent_prob)) {
                (void)pthread_mutex_lock(&connlm->fp_lock);
                fprintf(connlm->fp_log, "%d\t%.6f\t%s", word,
                        logn(p, connlm->eval_opt.out_log_base),
                        vocab_get_word(connlm->vocab, word));

                fprintf(connlm->fp_log, "\n");
                (void)pthread_mutex_unlock(&connlm->fp_lock);
            }

            if (word == SENT_END_ID) {
                if (connlm_reset_test(connlm, tid) < 0) {
                    ST_WARNING("Failed to connlm_reset_test.");
                    goto ERR;
                }
                sents++;

                if (connlm->fp_log != NULL
                        && connlm->eval_opt.print_sent_prob) {
                    (void)pthread_mutex_lock(&connlm->fp_lock);
                    fprintf(connlm->fp_log, "%.6f\n", logp_sent);
                    (void)pthread_mutex_unlock(&connlm->fp_lock);
                }
                logp_sent = 0;
            }

            words++;
        }

        if (pthread_mutex_lock(&connlm->empty_egs_lock) != 0) {
            ST_WARNING("Failed to pthread_mutex_lock empty_egs_lock.");
            goto ERR;
        }
        egs->next = connlm->empty_egs;
        connlm->empty_egs = egs;
        if (pthread_mutex_unlock(&connlm->empty_egs_lock) != 0) {
            ST_WARNING("Failed to pthread_mutex_unlock empty_egs_lock.");
            goto ERR;
        }
        if (st_sem_post(&connlm->sem_empty) != 0) {
            ST_WARNING("Failed to st_sem_post sem_empty.");
            goto ERR;
        }
    }

    thr->words = words;
    thr->sents = sents;
    thr->logp = logp;

    return NULL;
ERR:
    connlm->err = -1;

    /* TODO: unlock mutex and post semaphore */
    return NULL;
}

int connlm_eval(connlm_t *connlm, FILE *fp_log)
{
    connlm_thr_t *thrs = NULL;
    pthread_t *pts = NULL;

    connlm_read_thr_t read_thr;
    int num_thrs;

    count_t words;
    count_t sents;
    double logp;

    int i;

    struct timeval tts_eval, tte_eval;
    long ms;

    ST_CHECK_PARAM(connlm == NULL, -1);

    if (fp_log != NULL) {
        connlm->fp_log = fp_log;
        pthread_mutex_init(&connlm->fp_lock, NULL);
        if (connlm->fp_log != NULL
                && (! connlm->eval_opt.print_sent_prob)) {
            fprintf(fp_log, "Index   logP(NET)          Word\n");
            fprintf(fp_log, "----------------------------------\n");
        }
        ST_NOTICE("Printing Probs, setting num_thread to 1.")
            connlm->eval_opt.num_thread = 1;
    }

    gettimeofday(&tts_eval, NULL);

    num_thrs = connlm->eval_opt.num_thread;
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

    read_thr.connlm = connlm;
    read_thr.thrs = thrs;
    read_thr.num_thrs = connlm->eval_opt.num_thread;
    read_thr.epoch_size = connlm->eval_opt.epoch_size;
    read_thr.shuffle = false;
    read_thr.random = 0;

    pthread_create(pts + num_thrs, NULL,
            connlm_read_thread, (void *)&read_thr);

    for (i = 0; i < num_thrs; i++) {
        thrs[i].connlm = connlm;
        thrs[i].tid = i;
        pthread_create(pts + i, NULL, connlm_eval_thread,
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
    logp = 0;

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

    gettimeofday(&tte_eval, NULL);
    ms = TIMEDIFF(tts_eval, tte_eval);

    ST_NOTICE("Finish eval in %ldms.", ms);

    ST_NOTICE("Words: " COUNT_FMT "    OOVs: " COUNT_FMT
            "    Sentences: " COUNT_FMT
            ", words/sec: %.1f", words, read_thr.oovs, sents,
             words / ((double) ms / 1000));
    ST_NOTICE("LogP: %f", logp);
    ST_NOTICE("Entropy: %f", -logp / log10(2) / words);
    ST_NOTICE("PPL: %f", exp10(-logp / (double) words));

    if (fp_log != NULL
            && (! connlm->eval_opt.print_sent_prob)) {
        fprintf(fp_log, "\nSummary:\n");
        fprintf(fp_log, "Words: " COUNT_FMT "    OOVs: " COUNT_FMT "\n",
                words, read_thr.oovs);
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

int connlm_setup_gen(connlm_t *connlm, connlm_gen_opt_t *gen_opt)
{
    ST_CHECK_PARAM(connlm == NULL || gen_opt == NULL, -1);

    connlm->gen_opt = *gen_opt;

    st_srand(gen_opt->rand_seed);

    if (output_setup_gen(connlm->output) < 0) {
        ST_WARNING("Failed to output_setup_gen.");
        goto ERR;
    }

    if (connlm->rnn != NULL) {
        if (rnn_setup_gen(connlm->rnn, connlm->output) < 0) {
            ST_WARNING("Failed to rnn_setup_gen.");
            goto ERR;
        }
    }

    if (connlm->maxent != NULL) {
        if (maxent_setup_gen(connlm->maxent, connlm->output) < 0) {
            ST_WARNING("Failed to maxent_setup_gen.");
            goto ERR;
        }
    }

    if (connlm->lbl != NULL) {
        if (lbl_setup_gen(connlm->lbl, connlm->output) < 0) {
            ST_WARNING("Failed to lbl_setup_gen.");
            goto ERR;
        }
    }

    if (connlm->ffnn != NULL) {
        if (ffnn_setup_gen(connlm->ffnn, connlm->output) < 0) {
            ST_WARNING("Failed to ffnn_setup_gen.");
            goto ERR;
        }
    }

    return 0;

ERR:

    return -1;
}

int connlm_reset_gen(connlm_t *connlm)
{
    ST_CHECK_PARAM(connlm == NULL, -1);

    if (output_reset_gen(connlm->output) < 0) {
        ST_WARNING("Failed to output_reset_gen.");
        return -1;
    }

    if (connlm->rnn != NULL) {
        if (rnn_reset_gen(connlm->rnn) < 0) {
            ST_WARNING("Failed to rnn_reset_gen.");
            return -1;
        }
    }

    if (connlm->maxent != NULL) {
        if (maxent_reset_gen(connlm->maxent) < 0) {
            ST_WARNING("Failed to maxent_reset_gen.");
            return -1;
        }
    }

    if (connlm->lbl != NULL) {
        if (lbl_reset_gen(connlm->lbl) < 0) {
            ST_WARNING("Failed to lbl_reset_gen.");
            return -1;
        }
    }

    if (connlm->ffnn != NULL) {
        if (ffnn_reset_gen(connlm->ffnn) < 0) {
            ST_WARNING("Failed to ffnn_reset_gen.");
            return -1;
        }
    }

    return 0;
}

int connlm_end_gen(connlm_t *connlm, int word)
{
    ST_CHECK_PARAM(connlm == NULL, -1);

    if (output_end_gen(connlm->output, word) < 0) {
        ST_WARNING("Failed to output_end_gen.");
        return -1;
    }

    if (connlm->rnn != NULL) {
        if (rnn_end_gen(connlm->rnn, word) < 0) {
            ST_WARNING("Failed to rnn_end_gen.");
            return -1;
        }
    }

    if (connlm->maxent != NULL) {
        if (maxent_end_gen(connlm->maxent, word) < 0) {
            ST_WARNING("Failed to maxent_end_gen.");
            return -1;
        }
    }

    if (connlm->lbl != NULL) {
        if (lbl_end_gen(connlm->lbl, word) < 0) {
            ST_WARNING("Failed to lbl_end_gen.");
            return -1;
        }
    }

    if (connlm->ffnn != NULL) {
        if (ffnn_end_gen(connlm->ffnn, word) < 0) {
            ST_WARNING("Failed to ffnn_end_gen.");
            return -1;
        }
    }

    return 0;
}

static int connlm_gen_class(connlm_t *connlm)
{
    double u;
    double p;
    int cls;

    int class_size;

    ST_CHECK_PARAM(connlm == NULL, -1);

    class_size = connlm->output->output_opt.class_size;
    if (class_size <= 0) {
        ST_WARNING("No class to generate.");
        return -1;
    }

    if (connlm_forward_pre_layer(connlm, 0, 0) < 0) {
        ST_WARNING("Failed to connlm_forward_pre_layer.");
        return -1;
    }

    u = st_random(0, 1);
    cls = 0;

    p = output_get_class_prob_for_class(connlm->output, cls, 0);
    if (p >= u) {
        return cls;
    }

    for (cls = 1; cls < class_size; cls++) {
        if (connlm->output->output_opt.hs
                && connlm->output->output_opt.class_hs) {
            if (output_activate_pre_layer(connlm->output, cls, 0) < 0) {
                ST_WARNING("Failed to output_activate_pre_layer.");
                return -1;
            }
        }

        p += output_get_class_prob_for_class(connlm->output, cls, 0);

        if (p >= u) {
            return cls;
        }
    }

    return class_size - 1;
}

static int connlm_gen_word(connlm_t *connlm, int cls)
{
    int word;

    int s;
    int e;
    double u;
    double p;

    ST_CHECK_PARAM(connlm == NULL, -1);

    if (connlm->output->output_opt.class_size > 0) {
        if (cls < 0) {
            ST_WARNING("Invalid class id[%d].", cls);
            return -1;
        }

        s = connlm->output->c2w_s[cls];
        e = connlm->output->c2w_e[cls];

        if (e - s == 1 && s == UNK_ID) {
            return UNK_ID;
        }
    } else {
        cls = -1;
        s = 0;
        e = connlm->output->output_size;
    }

    if (connlm_forward_last_layer(connlm, s, 0) < 0) {
        ST_WARNING("Failed to connlm_forward_last_layer.");
        return -1;
    }

    while (true) {
        u = st_random(0, 1);
        word = s;

        p = output_get_word_prob(connlm->output, word, 0);
        if (p >= u) {
            if (word == UNK_ID) {
                continue;
            } else {
                return word;
            }
        }

        for (word = s + 1; word < e; word++) {
            if (connlm->output->output_opt.hs) {
                if (output_activate_last_layer(connlm->output,
                            word, 0) < 0) {
                    ST_WARNING("Failed to output_activate_last_layer.");
                    return -1;
                }
            }

            p += output_get_word_prob(connlm->output, word, 0);

            if (p >= u) {
                break;
            }
        }

        if (word != UNK_ID) {
            return word;
        }
    }

    return e - 1;
}

int connlm_gen(connlm_t *connlm, int num_sents)
{
    count_t words;
    count_t sents;

    FILE *text_fp = NULL;

    connlm_egs_t egs = {
        .words = NULL,
        .size = 0,
        .capacity = 0,
    };

    int class_size;

    int word;
    int cls;
    int i;

    struct timeval tts_gen, tte_gen;
    long ms;

    ST_CHECK_PARAM(connlm == NULL || num_sents < 0, -1);

    class_size = connlm->output->output_opt.class_size;

    if (connlm->gen_opt.prefix_file[0] != '\0') {
        text_fp = st_fopen(connlm->gen_opt.prefix_file, "rb");
        if (text_fp == NULL) {
            ST_WARNING("Failed to open prefix file[%s]",
                    connlm->gen_opt.prefix_file);
            goto ERR;
        }
    }

    gettimeofday(&tts_gen, NULL);
    sents = 0;
    words = 0;
    while(true) {
        if (connlm_reset_gen(connlm) < 0) {
            ST_WARNING("Failed to connlm_reset_gen.");
            goto ERR;
        }
        if (text_fp != NULL && !feof(text_fp)) {
            if (connlm_get_egs(&egs, NULL, 1, text_fp,
                        connlm->vocab, NULL) < 0) {
                ST_WARNING("Failed to connlm_get_egs.");
                goto ERR;
            }

            if (egs.size > 1) {
                printf("[");
                for (i = 0; i < egs.size - 1; i++) {
                    word = egs.words[i];
                    if (connlm_forward(connlm, word, 0) < 0) {
                        ST_WARNING("Failed to connlm_forward.");
                        goto ERR;
                    }

                    if (connlm_end_gen(connlm, word) < 0) {
                        ST_WARNING("connlm_end_gen.");
                        goto ERR;
                    }

                    printf("%s", vocab_get_word(connlm->vocab, word));
                    if (i < egs.size - 2) {
                        printf(" ");
                    }
                }
                printf("] ");
            } else if (feof(text_fp)) {
                break;
            }
        }

        word = -1;
        while (word != SENT_END_ID) {
            if (word != -1) { // not first word
                printf(" ");
            }
            if (class_size > 0) {
                // generate class
                cls = connlm_gen_class(connlm);
                if (cls < 0) {
                    ST_WARNING("Failed to connlm_gen_class.");
                    goto ERR;
                }
            } else {
                cls = -1;
            }

            word = connlm_gen_word(connlm, cls);
            if (word < 0) {
                ST_WARNING("Failed to connlm_gen_word.");
                goto ERR;
            }

            if (word == SENT_END_ID) {
                printf("\n");
            } else {
                printf("%s", vocab_get_word(connlm->vocab, word));
            }
            fflush(stdout);

            if (connlm_end_gen(connlm, word) < 0) {
                ST_WARNING("connlm_end_gen.");
                goto ERR;
            }

            words++;
        }

        sents++;
        if (sents >= num_sents) {
            if(text_fp != NULL) {
                if (feof(text_fp)) {
                    break;
                }
            } else {
                break;
            }
        }
    }

    gettimeofday(&tte_gen, NULL);
    ms = TIMEDIFF(tts_gen, tte_gen);

    ST_NOTICE("Finish generating in %ldms.", ms);

    ST_NOTICE("Words: " COUNT_FMT "    Sentences: " COUNT_FMT
            ", words/sec: %.1f", words, sents,
             words / ((double) ms / 1000));

    return 0;

ERR:

    return -1;
}
