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
#include <st_utils.h>

#include "utils.h"
#include "output.h"

static const int OUTPUT_MAGIC_NUM = 626140498 + 6;

int output_load_opt(output_opt_t *output_opt, st_opt_t *opt,
        const char *sec_name)
{
    ST_CHECK_PARAM(output_opt == NULL || opt == NULL, -1);

    ST_OPT_SEC_GET_INT(opt, sec_name, "CLASS_SIZE",
            output_opt->class_size, 100, "Size of class layer");

    ST_OPT_SEC_GET_BOOL(opt, sec_name, "HS",
            output_opt->hs, true, "Hierarchical softmax");

    return 0;

ST_OPT_ERR:
    return -1;
}

output_t *output_create(output_opt_t *output_opt, int output_size)
{
    output_t *output = NULL;

    ST_CHECK_PARAM(output_opt == NULL, NULL);

    output = (output_t *) malloc(sizeof(output_t));
    if (output == NULL) {
        ST_WARNING("Falied to malloc output_t.");
        goto ERR;
    }
    memset(output, 0, sizeof(output_t));

    output->output_opt = *output_opt;
    output->output_size = output_size;

    return output;
  ERR:
    safe_output_destroy(output);
    return NULL;
}

void output_destroy(output_t *output)
{
    int i;

    if (output == NULL) {
        return;
    }

    safe_free(output->w2c);
    safe_free(output->c2w_s);
    safe_free(output->c2w_e);

    for (i = 0; i < output->num_thrs; i++) {
        safe_free(output->neurons[i].ac_o_c);
        safe_free(output->neurons[i].er_o_c);
        safe_free(output->neurons[i].ac_o_w);
        safe_free(output->neurons[i].er_o_w);
    }
    safe_free(output->neurons);
}

output_t* output_dup(output_t *o)
{
    output_t *output = NULL;

    ST_CHECK_PARAM(o == NULL, NULL);

    output = (output_t *) malloc(sizeof(output_t));
    if (output == NULL) {
        ST_WARNING("Falied to malloc output_t.");
        goto ERR;
    }
    memset(output, 0, sizeof(output_t));

    output->output_opt = o->output_opt;
    *output = *o;

    output->w2c = (int *) malloc(sizeof(int) * o->output_size);
    if (output->w2c == NULL) {
        ST_WARNING("Failed to malloc w2c.");
        goto ERR;
    }
    memcpy(output->w2c, o->w2c, sizeof(int)*o->output_size);

    output->c2w_s = (int *) malloc(sizeof(int) * o->output_opt.class_size);
    if (output->c2w_s == NULL) {
        ST_WARNING("Failed to malloc c2w_s.");
        goto ERR;
    }
    memcpy(output->c2w_s, o->c2w_s, sizeof(int)*o->output_opt.class_size);

    output->c2w_e = (int *) malloc(sizeof(int) * o->output_opt.class_size);
    if (output->c2w_e == NULL) {
        ST_WARNING("Failed to malloc c2w_e.");
        goto ERR;
    }
    memcpy(output->c2w_e, o->c2w_e, sizeof(int)*o->output_opt.class_size);

    return output;

ERR:
    safe_output_destroy(output);
    return NULL;
}

int output_load_header(output_t **output, FILE *fp, bool *binary,
        FILE *fo_info)
{
    char sym[MAX_LINE_LEN];
    union {
        char str[4];
        int magic_num;
    } flag;

    int output_size;
    int class_size;
    bool hs;

    ST_CHECK_PARAM((output == NULL && fo_info == NULL) || fp == NULL
            || binary == NULL, -1);

    if (fread(&flag.magic_num, sizeof(int), 1, fp) != 1) {
        ST_WARNING("Failed to load magic num.");
        return -1;
    }

    if (strncmp(flag.str, "    ", 4) == 0) {
        *binary = false;
    } else if (OUTPUT_MAGIC_NUM != flag.magic_num) {
        ST_WARNING("magic num wrong.");
        return -2;
    } else {
        *binary = true;
    }

    if (output != NULL) {
        *output = NULL;
    }
    
    if (*binary) {
        if (fread(&output_size, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read output size.");
            return -1;
        }

        if (output_size <= 0) {
            if (fo_info != NULL) {
                fprintf(fo_info, "\n<OUTPUT>: None\n");
            }
            return 0;
        }

        if (fread(&class_size, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read class size.");
            return -1;
        }

        if (fread(&hs, sizeof(bool), 1, fp) != 1) {
            ST_WARNING("Failed to read hs.");
            return -1;
        }
    } else {
        if (st_readline(fp, "<OUTPUT>") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }

        if (st_readline(fp, "Output size: %d", &output_size) != 1) {
            ST_WARNING("Failed to parse output_size.");
            goto ERR;
        }

        if (output_size <= 0) {
            if (fo_info != NULL) {
                fprintf(fo_info, "\n<OUTPUT>: None\n");
            }
            return 0;
        }

        if (st_readline(fp, "Class size: %d", &class_size) != 1) {
            ST_WARNING("Failed to parse class size.");
            return -1;
        }

        if (st_readline(fp, "HS: %s", sym) != 1) {
            ST_WARNING("Failed to parse hs.");
            return -1;
        }
        hs = str2bool(sym);
    }

    if (output != NULL) {
        *output = (output_t *)malloc(sizeof(output_t));
        if (*output == NULL) {
            ST_WARNING("Failed to malloc output_t");
            goto ERR;
        }
        memset(*output, 0, sizeof(output_t));

        (*output)->output_size = output_size;
        (*output)->output_opt.class_size = class_size;
        (*output)->output_opt.hs = hs;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<OUTPUT>\n");
        fprintf(fo_info, "Output size: %d\n", output_size);
        fprintf(fo_info, "Class size: %d\n", class_size);
        fprintf(fo_info, "HS: %s\n", bool2str(hs));
    }

    return 0;

ERR:
    safe_output_destroy(*output);
    return -1;
}

int output_load_body(output_t *output, FILE *fp, bool binary)
{
    int n;

    int i;
    int class_size;

    ST_CHECK_PARAM(output == NULL || fp == NULL, -1);

    output->w2c = NULL;
    output->c2w_s = NULL;
    output->c2w_e = NULL;

    if (output->output_size <= 0) {
        return 0;
    }

    class_size = output->output_opt.class_size;
    if (class_size > 0) {
        output->w2c = (int *) malloc(sizeof(int) * output->output_size);
        if (output->w2c == NULL) {
            ST_WARNING("Failed to malloc w2c.");
            goto ERR;
        }
        memset(output->w2c, 0, sizeof(int) * output->output_size);

        output->c2w_s = (int *) malloc(sizeof(int) * class_size);
        if (output->c2w_s == NULL) {
            ST_WARNING("Failed to malloc c2w_s.");
            goto ERR;
        }
        memset(output->c2w_s, 0, sizeof(int) * class_size);

        output->c2w_e = (int *) malloc(sizeof(int) * class_size);
        if (output->c2w_e == NULL) {
            ST_WARNING("Failed to malloc c2w_e.");
            goto ERR;
        }
        memset(output->c2w_e, 0, sizeof(int) * class_size);
    }

    if (binary) {
        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read magic num.");
            goto ERR;
        }

        if (n != -OUTPUT_MAGIC_NUM) {
            ST_WARNING("Magic num error.");
            goto ERR;
        }

        if (class_size > 0) {
            if (fread(output->w2c, sizeof(int), output->output_size, fp)
                    != output->output_size) {
                ST_WARNING("Failed to read w2c.");
                goto ERR;
            }

            if (fread(output->c2w_s, sizeof(int), class_size, fp)
                    != class_size) {
                ST_WARNING("Failed to read c2w_s.");
                goto ERR;
            }

            if (fread(output->c2w_e, sizeof(int), class_size, fp)
                    != class_size) {
                ST_WARNING("Failed to read c2w_e.");
                goto ERR;
            }
        }
    } else {
        if (st_readline(fp, "<OUTPUT-DATA>") != 0) {
            ST_WARNING("body flag error.");
            goto ERR;
        }

        if (class_size > 0) {
            if (st_readline(fp, "Words to Classes:") != 0) {
                ST_WARNING("w2c flag error.");
                goto ERR;
            }
            for (i = 0; i < output->output_size; i++) {
                if (st_readline(fp, "\t%*d\t%d", output->w2c + i) != 1) {
                    ST_WARNING("Failed to parse w2c.");
                    goto ERR;
                }
            }

            if (st_readline(fp, "Classes to Words:") != 0) {
                ST_WARNING("c2w flag error.");
                goto ERR;
            }
            for (i = 0; i < class_size; i++) {
                if (st_readline(fp, "\t%*d\t%d\t%d", output->c2w_s + i,
                            output->c2w_e + i) != 2) {
                    ST_WARNING("Failed to parse c2w.");
                    goto ERR;
                }
            }
        }
    }

    return 0;
ERR:
    safe_free(output->w2c);
    safe_free(output->c2w_s);
    safe_free(output->c2w_e);
    return -1;
}

int output_save_header(output_t *output, FILE *fp, bool binary)
{
    int n;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (binary) {
        if (fwrite(&OUTPUT_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }
        if (output == NULL) {
            n = 0;
            if (fwrite(&n, sizeof(int), 1, fp) != 1) {
                ST_WARNING("Failed to write output size.");
                return -1;
            }
            return 0;
        }

        if (fwrite(&output->output_size, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write output size.");
            return -1;
        }

        if (fwrite(&output->output_opt.class_size,
                    sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write class size.");
            return -1;
        }

        if (fwrite(&output->output_opt.hs, sizeof(bool), 1, fp) != 1) {
            ST_WARNING("Failed to write hs.");
            return -1;
        }
    } else {
        fprintf(fp, "    \n<OUTPUT>\n");

        if (output == NULL) {
            fprintf(fp, "Output size: 0\n");
            return 0;
        }

        fprintf(fp, "Output size: %d\n", output->output_size);
        fprintf(fp, "Class size: %d\n", output->output_opt.class_size);
        fprintf(fp, "HS: %s\n", bool2str(output->output_opt.hs));
    }

    return 0;
}

int output_save_body(output_t *output, FILE *fp, bool binary)
{
    int n;

    int i;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (output == NULL) {
        return 0;
    }

    if (binary) {
        n = -OUTPUT_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        if (fwrite(output->w2c, sizeof(int), output->output_size, fp)
                != output->output_size) {
            ST_WARNING("Failed to write w2c.");
            return -1;
        }

        if (fwrite(output->c2w_s, sizeof(int),
                    output->output_opt.class_size, fp)
                != output->output_opt.class_size) {
            ST_WARNING("Failed to write c2w_s.");
            return -1;
        }

        if (fwrite(output->c2w_e, sizeof(int),
                    output->output_opt.class_size, fp)
                != output->output_opt.class_size) {
            ST_WARNING("Failed to write c2w_e.");
            return -1;
        }
    } else {
        fprintf(fp, "<OUTPUT-DATA>\n");

        fprintf(fp, "Words to Classes:\n");
        for (i = 0; i < output->output_size; i++) {
            fprintf(fp, "\t%d\t%d\n", i, output->w2c[i]);
        }

        fprintf(fp, "Classes to Words:\n");
        for (i = 0; i < output->output_opt.class_size; i++) {
            fprintf(fp, "\t%d\t%d\t%d\n", i, output->c2w_s[i],
                    output->c2w_e[i]);
        }
    }

    return 0;
}

int output_generate(output_t *output, count_t *word_cnts)
{
    int class_size;

    int i;
    int a;
    long b;
    double dd;
    double df;

    ST_CHECK_PARAM(output == NULL || word_cnts == NULL, -1);

    safe_free(output->w2c);
    safe_free(output->c2w_s);
    safe_free(output->c2w_e);

    class_size = output->output_opt.class_size;
    if (class_size <= 0) {
        return 0;
    }

    output->w2c = (int *) malloc(sizeof(int) * output->output_size);
    if (output->w2c == NULL) {
        ST_WARNING("Failed to malloc w2c.");
        goto ERR;
    }
    memset(output->w2c, 0, sizeof(int) * output->output_size);

    output->c2w_s = (int *) malloc(sizeof(int) * class_size);
    if (output->c2w_s == NULL) {
        ST_WARNING("Failed to malloc c2w_s.");
        goto ERR;
    }
    memset(output->c2w_s, 0, sizeof(int) * class_size);

    output->c2w_e = (int *) malloc(sizeof(int) * class_size);
    if (output->c2w_e == NULL) {
        ST_WARNING("Failed to malloc c2w_e.");
        goto ERR;
    }
    memset(output->c2w_e, 0, sizeof(int) * class_size);

    a = 0;
    b = 0;
    dd = 0.0;
    df = 0.0;
    for (i = 0; i < output->output_size; i++) {
        b += word_cnts[i];
    }
    for (i = 0; i < output->output_size; i++) {
        dd += sqrt(word_cnts[i] / (double) b);
    }

    for (i = 0; i < output->output_size; i++) {
        df += sqrt(word_cnts[i] / (double) b) / dd;
        if (df > 1) {
            df = 1;
        }
        if (df > (a + 1) / (double) class_size) {
            output->w2c[i] = a;
            if (a < class_size - 1) {
                output->c2w_e[a] = i + 1;
                a++;
                output->c2w_s[a] = i + 1;
            }
        } else {
            output->w2c[i] = a;
        }
    }
    output->c2w_e[class_size - 1] = output->output_size;

    return 0;
ERR:
    safe_free(output->w2c);
    safe_free(output->c2w_s);
    safe_free(output->c2w_e);
    return -1;
}

int output_activate_pre_layer(output_t *output, int tid)
{
    output_neuron_t *neu;

    ST_CHECK_PARAM(output == NULL || tid < 0, -1);

    neu = output->neurons + tid;
    if (output->output_opt.class_size > 0) {
        softmax(neu->ac_o_c, output->output_opt.class_size);
    }

    return 0;
}

int output_activate_last_layer(output_t *output, int cls, int tid)
{
    output_neuron_t *neu;

    int s;
    int e;

    ST_CHECK_PARAM(output == NULL || tid < 0, -1);

    neu = output->neurons + tid;
    if (output->output_opt.class_size > 0 && cls >= 0) {
        s = output->c2w_s[cls];
        e = output->c2w_e[cls];
    } else {
        s = 0;
        e = output->output_size;
    }

    softmax(neu->ac_o_w + s, e - s);

    return 0;
}

int output_loss(output_t *output, int word, int tid)
{
    output_neuron_t *neu;
    
    int c;
    int s;
    int e;

    int o;

    ST_CHECK_PARAM(output == NULL || tid < 0, -1);

    if (word < 0) {
        return 0;
    }

    neu = output->neurons + tid;
    if (output->output_opt.class_size > 0) {
        c = output->w2c[word];
        s = output->c2w_s[c];
        e = output->c2w_e[c];
        
        for (o = 0; o < output->output_opt.class_size; o++) {
            neu->er_o_c[o] = (0 - neu->ac_o_c[o]);
        }
        neu->er_o_c[c] = (1 - neu->ac_o_c[c]);
    } else {
        s = 0;
        e = output->output_size;
    }

    for (o = s; o < e; o++) {
        neu->er_o_w[o] = (0 - neu->ac_o_w[o]);
    }
    neu->er_o_w[word] = (1 - neu->ac_o_w[word]);

    return 0;
}

double output_get_prob(output_t *output, int word, int tid)
{
    output_neuron_t *neu;
    double p = 1;

    neu = output->neurons + tid;
    if (output->output_opt.class_size > 0) {
        p *= neu->ac_o_c[output->w2c[word]];
    }

    p *= neu->ac_o_w[word];

    return p;
}

double output_get_class_prob_for_class(output_t *output, int cls, int tid)
{
    output_neuron_t *neu;

    neu = output->neurons + tid;
    if (output->output_opt.class_size > 0) {
        return neu->ac_o_c[cls];
    }

    return 0;
}

double output_get_class_prob(output_t *output, int word, int tid)
{
    output_neuron_t *neu;

    neu = output->neurons + tid;
    if (output->output_opt.class_size > 0) {
        return neu->ac_o_c[output->w2c[word]];
    }

    return 0;
}

double output_get_word_prob(output_t *output, int word, int tid)
{
    output_neuron_t *neu;

    neu = output->neurons + tid;

    return neu->ac_o_w[word];
}

int output_setup_train(output_t *output, int num_thrs)
{
    output_neuron_t *neu;
    size_t sz;

    int class_size;

    int i;
    int t;

    ST_CHECK_PARAM(output == NULL || num_thrs < 0, -1);

    output->num_thrs = num_thrs;
    sz = sizeof(output_neuron_t) * num_thrs;
    output->neurons = (output_neuron_t *)malloc(sz);
    if (output->neurons == NULL) {
        ST_WARNING("Failed to malloc neurons.");
        goto ERR;
    }
    memset(output->neurons, 0, sz);

    class_size = output->output_opt.class_size;

    for (t = 0; t < num_thrs; t++) {
        neu = output->neurons + t;
        if (class_size > 0) {
            posix_memalign((void **)&neu->ac_o_c, ALIGN_SIZE, sizeof(real_t) * class_size);
            if (neu->ac_o_c == NULL) {
                ST_WARNING("Failed to malloc ac_o_c.");
                goto ERR;
            }

            posix_memalign((void **)&neu->er_o_c, ALIGN_SIZE, sizeof(real_t) * class_size);
            if (neu->er_o_c == NULL) {
                ST_WARNING("Failed to malloc er_o_c.");
                goto ERR;
            }

            for (i = 0; i < class_size; i++) {
                neu->ac_o_c[i] = 0;
                neu->er_o_c[i] = 0;
            }
        }

        posix_memalign((void **)&neu->ac_o_w, ALIGN_SIZE, sizeof(real_t) * output->output_size);
        if (neu->ac_o_w == NULL) {
            ST_WARNING("Failed to malloc ac_o_w.");
            goto ERR;
        }

        posix_memalign((void **)&neu->er_o_w, ALIGN_SIZE, sizeof(real_t) * output->output_size);
        if (neu->er_o_w == NULL) {
            ST_WARNING("Failed to malloc er_o_w.");
            goto ERR;
        }

        for (i = 0; i < output->output_size; i++) {
            neu->ac_o_w[i] = 0;
            neu->er_o_w[i] = 0;
        }
    }

    return 0;

ERR:
    for (i = 0; i < output->num_thrs; i++) {
        safe_free(output->neurons[i].ac_o_c);
        safe_free(output->neurons[i].er_o_c);
        safe_free(output->neurons[i].ac_o_w);
        safe_free(output->neurons[i].er_o_w);
    }
    safe_free(output->neurons);

    return -1;
}

int output_reset_train(output_t *output, int tid)
{
#if 0
    int class_size;

    int i;

    ST_CHECK_PARAM(output == NULL, -1);

    class_size = output->output_opt.class_size;
    if (class_size > 0) {
        for (i = 0; i < class_size; i++) {
            output->ac_o_c[i] = 0;
            output->er_o_c[i] = 0;
        }
    }

    for (i = 0; i < output->output_size; i++) {
        output->ac_o_w[i] = 0;
        output->er_o_w[i] = 0;
    }
#endif

    return 0;
}

int output_start_train(output_t *output, int word, int tid)
{
    output_neuron_t *neu;
    int class_size;

    int c;
    int s;
    int e;

    int i;

    ST_CHECK_PARAM(output == NULL || tid < 0, -1);

    if (word < 0) {
        return 0;
    }

    neu = output->neurons + tid;
    class_size = output->output_opt.class_size;
    if (class_size > 0) {
        for (i = 0; i < class_size; i++) {
            neu->ac_o_c[i] = 0;
            neu->er_o_c[i] = 0;
        }

        c = output->w2c[word];
        s = output->c2w_s[c];
        e = output->c2w_e[c];
    } else {
        s = 0;
        e = output->output_size;
    }

    for (i = s; i < e; i++) {
        neu->ac_o_w[i] = 0;
        neu->er_o_w[i] = 0;
    }

    return 0;
}

int output_end_train(output_t *output, int word, int tid)
{
    ST_CHECK_PARAM(output == NULL, -1);

    return 0;
}

int output_finish_train(output_t *output, int tid)
{
    ST_CHECK_PARAM(output == NULL, -1);

    return 0;
}

int output_setup_test(output_t *output, int num_thrs)
{
    output_neuron_t *neu;
    size_t sz;

    int class_size;

    int i;
    int t;

    ST_CHECK_PARAM(output == NULL || num_thrs < 0, -1);

    output->num_thrs = num_thrs;
    sz = sizeof(output_neuron_t) * num_thrs;
    output->neurons = (output_neuron_t *)malloc(sz);
    if (output->neurons == NULL) {
        ST_WARNING("Failed to malloc output neurons.");
        goto ERR;
    }
    memset(output->neurons, 0, sz);

    class_size = output->output_opt.class_size;

    for (t = 0; t < num_thrs; t++) {
        neu = output->neurons + t;

        if (class_size > 0) {
            posix_memalign((void **)&neu->ac_o_c, ALIGN_SIZE, sizeof(real_t) * class_size);
            if (neu->ac_o_c == NULL) {
                ST_WARNING("Failed to malloc ac_o_c.");
                goto ERR;
            }

            for (i = 0; i < class_size; i++) {
                neu->ac_o_c[i] = 0;
            }
        }

        posix_memalign((void **)&neu->ac_o_w, ALIGN_SIZE, sizeof(real_t) * output->output_size);
        if (neu->ac_o_w == NULL) {
            ST_WARNING("Failed to malloc ac_o_w.");
            goto ERR;
        }

        for (i = 0; i < output->output_size; i++) {
            neu->ac_o_w[i] = 0;
        }
    }

    return 0;

ERR:
    for (i = 0; i < output->num_thrs; i++) {
        safe_free(output->neurons[i].ac_o_c);
        safe_free(output->neurons[i].er_o_c);
        safe_free(output->neurons[i].ac_o_w);
        safe_free(output->neurons[i].er_o_w);
    }
    safe_free(output->neurons);

    return -1;
}

int output_reset_test(output_t *output, int tid)
{
#if 0
    int class_size;

    int i;

    ST_CHECK_PARAM(output == NULL, -1);

    class_size = output->output_opt.class_size;
    if (class_size > 0) {
        for (i = 0; i < class_size; i++) {
            output->ac_o_c[i] = 0;
        }
    }

    for (i = 0; i < output->output_size; i++) {
        output->ac_o_w[i] = 0;
    }
#endif

    return 0;
}

int output_start_test(output_t *output, int word, int tid)
{
    output_neuron_t *neu;
    int class_size;

    int c;
    int s;
    int e;

    int i;

    ST_CHECK_PARAM(output == NULL || tid < 0, -1);

    if (word < 0) {
        return 0;
    }

    neu = output->neurons + tid;
    class_size = output->output_opt.class_size;
    if (class_size > 0) {
        for (i = 0; i < class_size; i++) {
            neu->ac_o_c[i] = 0;
        }

        c = output->w2c[word];
        s = output->c2w_s[c];
        e = output->c2w_e[c];
    } else {
        s = 0;
        e = output->output_size;
    }

    for (i = s; i < e; i++) {
        neu->ac_o_w[i] = 0;
    }

    return 0;
}

int output_end_test(output_t *output, int word, int tid)
{
    ST_CHECK_PARAM(output == NULL, -1);

    return 0;
}

int output_setup_gen(output_t *output)
{
    return output_setup_test(output, 1);
}

int output_reset_gen(output_t *output)
{
    return output_reset_test(output, 0);
}

int output_end_gen(output_t *output, int word)
{
    return output_end_test(output, word, 0);
}

