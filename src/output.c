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
    if (output == NULL) {
        return;
    }

    safe_free(output->w2c);
    safe_free(output->c2w_s);
    safe_free(output->c2w_e);

    safe_free(output->ac_o_c);
    safe_free(output->er_o_c);
    safe_free(output->ac_o_w);
    safe_free(output->er_o_w);

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

long output_load_header(output_t **output, FILE *fp, bool *binary,
        FILE *fo_info)
{
    char line[MAX_LINE_LEN];
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
        if (fgets(line, MAX_LINE_LEN, fp) == NULL) {
            ST_WARNING("Failed to read flag.");
            return -1;
        }

        if (fgets(line, MAX_LINE_LEN, fp) == NULL) {
            ST_WARNING("Failed to read flag.");
            return -1;
        }
        if (strncmp(line, "<OUTPUT>", 7) != 0) {
            ST_WARNING("flag error.[%s]", line);
            return -1;
        }

        if (fgets(line, MAX_LINE_LEN, fp) == NULL) {
            ST_WARNING("Failed to read output_size.");
            return -1;
        }
        if (sscanf(line, "Output size: %d", &output_size) != 1) {
            ST_WARNING("Failed to parse output_size.[%s]", line);
            return -1;
        }

        if (output_size <= 0) {
            if (fo_info != NULL) {
                fprintf(fo_info, "\n<OUTPUT>: None\n");
            }
            return 0;
        }

        if (fgets(line, MAX_LINE_LEN, fp) == NULL) {
            ST_WARNING("Failed to read class size.");
            return -1;
        }
        if (sscanf(line, "Class size: %d", &class_size) != 1) {
            ST_WARNING("Failed to parse class size.[%s]", line);
            return -1;
        }

        if (fgets(line, MAX_LINE_LEN, fp) == NULL) {
            ST_WARNING("Failed to read hs.");
            return -1;
        }
        if (sscanf(line, "HS: %s", sym) != 1) {
            ST_WARNING("Failed to parse hs.[%s]", line);
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
    char line[MAX_LINE_LEN];
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
        if (fgets(line, MAX_LINE_LEN, fp) == NULL) {
            ST_WARNING("Failed to read body flag.");
            goto ERR;
        }
        if (strncmp(line, "<OUTPUT-DATA>", 12) != 0) {
            ST_WARNING("body flag error.[%s]", line);
            goto ERR;
        }

        if (class_size > 0) {
            if (fgets(line, MAX_LINE_LEN, fp) == NULL) {
                ST_WARNING("Failed to read w2c flag.");
                goto ERR;
            }
            if (strncmp(line, "Words to Classes:", 17) != 0) {
                ST_WARNING("w2c flag error.[%s]", line);
                goto ERR;
            }
            for (i = 0; i < output->output_size; i++) {
                if (fgets(line, MAX_LINE_LEN, fp) == NULL) {
                    ST_WARNING("Failed to read w2c[%d].", i);
                    goto ERR;
                }
                if (sscanf(line, "\t%*d\t%d", output->w2c + i) != 1) {
                    ST_WARNING("Failed to parse w2c.[%s]", line);
                    goto ERR;
                }
            }

            if (fgets(line, MAX_LINE_LEN, fp) == NULL) {
                ST_WARNING("Failed to read c2w flag.");
                goto ERR;
            }
            if (strncmp(line, "Classes to Words:", 17) != 0) {
                ST_WARNING("c2w flag error.[%s]", line);
                goto ERR;
            }
            for (i = 0; i < class_size; i++) {
                if (fgets(line, MAX_LINE_LEN, fp) == NULL) {
                    ST_WARNING("Failed to read c2w[%d].", i);
                    goto ERR;
                }
                if (sscanf(line, "\t%*d\t%d\t%d", output->c2w_s + i,
                            output->c2w_e + i) != 2) {
                    ST_WARNING("Failed to parse c2w.[%s]", line);
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

    output->w2c = NULL;
    output->c2w_s = NULL;
    output->c2w_e = NULL;

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

int output_activate(output_t *output, int word)
{
    int c;
    int s;
    int e;

    ST_CHECK_PARAM(output == NULL, -1);

    if (word < 0) {
        return 0;
    }

    if (output->output_opt.class_size > 0) {
        c = output->w2c[word];
        s = output->c2w_s[c];
        e = output->c2w_e[c];
        
        softmax(output->ac_o_c, output->output_opt.class_size);
    } else {
        s = 0;
        e = output->output_size;
    }

    softmax(output->ac_o_w + s, e - s);

    return 0;
}

int output_loss(output_t *output, int word)
{
    int c;
    int s;
    int e;

    int o;

    ST_CHECK_PARAM(output == NULL, -1);

    if (word < 0) {
        return 0;
    }

    if (output->output_opt.class_size > 0) {
        c = output->w2c[word];
        s = output->c2w_s[c];
        e = output->c2w_e[c];
        
        for (o = 0; o < output->output_opt.class_size; o++) {
            output->er_o_c[o] = (0 - output->ac_o_c[o]);
        }
        output->er_o_c[c] = (1 - output->ac_o_c[c]);
    } else {
        s = 0;
        e = output->output_size;
    }

    for (o = s; o < e; o++) {
        output->er_o_w[o] = (0 - output->ac_o_w[o]);
    }
    output->er_o_w[word] = (1 - output->ac_o_w[word]);

    return 0;
}

double output_get_prob(output_t *output, int word)
{
    double p = 1;

    if (output->output_opt.class_size > 0) {
        p *= output->ac_o_c[output->w2c[word]];
    }

    p *= output->ac_o_w[word];

    return p;
}

int output_setup_train(output_t *output)
{
    int class_size;

    int i;

    ST_CHECK_PARAM(output == NULL, -1);

    class_size = output->output_opt.class_size;
    if (class_size > 0) {
        output->ac_o_c = (real_t *) malloc(sizeof(real_t) * class_size);
        if (output->ac_o_c == NULL) {
            ST_WARNING("Failed to malloc ac_o_c.");
            goto ERR;
        }

        output->er_o_c = (real_t *) malloc(sizeof(real_t) * class_size);
        if (output->er_o_c == NULL) {
            ST_WARNING("Failed to malloc er_o_c.");
            goto ERR;
        }

        for (i = 0; i < class_size; i++) {
            output->ac_o_c[i] = 0;
            output->er_o_c[i] = 0;
        }
    }

    output->ac_o_w = (real_t *) malloc(sizeof(real_t)*output->output_size);
    if (output->ac_o_w == NULL) {
        ST_WARNING("Failed to malloc ac_o_w.");
        goto ERR;
    }

    output->er_o_w = (real_t *) malloc(sizeof(real_t)*output->output_size);
    if (output->er_o_w == NULL) {
        ST_WARNING("Failed to malloc er_o_w.");
        goto ERR;
    }

    for (i = 0; i < output->output_size; i++) {
        output->ac_o_w[i] = 0;
        output->er_o_w[i] = 0;
    }

    return 0;

ERR:
    safe_free(output->ac_o_c);
    safe_free(output->er_o_c);
    safe_free(output->ac_o_w);
    safe_free(output->er_o_w);

    return -1;
}

int output_reset_train(output_t *output)
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

int output_start_train(output_t *output, int word)
{
    int class_size;

    int c;
    int s;
    int e;

    int i;

    ST_CHECK_PARAM(output == NULL, -1);

    if (word < 0) {
        return 0;
    }

    class_size = output->output_opt.class_size;
    if (class_size > 0) {
        for (i = 0; i < class_size; i++) {
            output->ac_o_c[i] = 0;
            output->er_o_c[i] = 0;
        }

        c = output->w2c[word];
        s = output->c2w_s[c];
        e = output->c2w_e[c];
    } else {
        s = 0;
        e = output->output_size;
    }

    for (i = s; i < e; i++) {
        output->ac_o_w[i] = 0;
        output->er_o_w[i] = 0;
    }

    return 0;
}

int output_end_train(output_t *output, int word)
{
    ST_CHECK_PARAM(output == NULL, -1);

    return 0;
}

int output_setup_test(output_t *output)
{
    int class_size;

    int i;

    ST_CHECK_PARAM(output == NULL, -1);

    class_size = output->output_opt.class_size;
    if (class_size > 0) {
        output->ac_o_c = (real_t *) malloc(sizeof(real_t) * class_size);
        if (output->ac_o_c == NULL) {
            ST_WARNING("Failed to malloc ac_o_c.");
            goto ERR;
        }

        for (i = 0; i < class_size; i++) {
            output->ac_o_c[i] = 0;
        }
    }

    output->ac_o_w = (real_t *) malloc(sizeof(real_t)*output->output_size);
    if (output->ac_o_w == NULL) {
        ST_WARNING("Failed to malloc ac_o_w.");
        goto ERR;
    }

    for (i = 0; i < output->output_size; i++) {
        output->ac_o_w[i] = 0;
    }

    return 0;

ERR:
    safe_free(output->ac_o_c);
    safe_free(output->ac_o_w);

    return -1;
}

int output_reset_test(output_t *output)
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

int output_start_test(output_t *output, int word)
{
    int class_size;

    int c;
    int s;
    int e;

    int i;

    ST_CHECK_PARAM(output == NULL, -1);

    if (word < 0) {
        return 0;
    }

    class_size = output->output_opt.class_size;
    if (class_size > 0) {
        for (i = 0; i < class_size; i++) {
            output->ac_o_c[i] = 0;
        }

        c = output->w2c[word];
        s = output->c2w_s[c];
        e = output->c2w_e[c];
    } else {
        s = 0;
        e = output->output_size;
    }

    for (i = s; i < e; i++) {
        output->ac_o_w[i] = 0;
    }

    return 0;
}

int output_end_test(output_t *output, int word)
{
    ST_CHECK_PARAM(output == NULL, -1);

    return 0;
}

