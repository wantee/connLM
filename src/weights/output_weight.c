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

#include <stutils/st_log.h>
#include <stutils/st_utils.h>

#include "output_weight.h"

static const int OUTPUT_WT_MAGIC_NUM = 626140498 + 80;

void out_wt_destroy(out_wt_t *out_wt)
{
    if (out_wt == NULL) {
        return;
    }

    safe_free(out_wt->matrix);
    out_wt->output_node_num = 0;
    out_wt->in_layer_sz = 0;
}

static int out_wt_alloc(out_wt_t *out_wt, int in_layer_sz,
        output_node_id_t output_node_num)
{
    size_t sz;

    sz = (output_node_num - 1) * in_layer_sz;
    if (posix_memalign((void **)&(out_wt->matrix), ALIGN_SIZE,
                sizeof(real_t) * sz) != 0
            || out_wt->matrix == NULL) {
        ST_WARNING("Failed to malloc matrix.");
        goto ERR;
    }
    out_wt->in_layer_sz = in_layer_sz;
    out_wt->output_node_num = output_node_num;

    return 0;

ERR:
    out_wt_destroy(out_wt);
    return -1;
}

out_wt_t* out_wt_dup(out_wt_t *o)
{
    out_wt_t *out_wt = NULL;

    ST_CHECK_PARAM(o == NULL, NULL);

    out_wt = (out_wt_t *) malloc(sizeof(out_wt_t));
    if (out_wt == NULL) {
        ST_WARNING("Falied to malloc out_wt_t.");
        goto ERR;
    }
    memset(out_wt, 0, sizeof(out_wt_t));

    if (out_wt_alloc(out_wt, o->in_layer_sz, o->output_node_num) < 0) {
        ST_WARNING("Failed to out_wt_alloc.");
        goto ERR;
    }
    memcpy(out_wt->matrix, o->matrix,
            sizeof(real_t) * o->in_layer_sz * (o->output_node_num - 1));

    return out_wt;

ERR:
    safe_out_wt_destroy(out_wt);
    return NULL;
}

int out_wt_load_header(out_wt_t **out_wt, int version,
        FILE *fp, bool *binary, FILE *fo_info)
{
    union {
        char str[4];
        int magic_num;
    } flag;

    int in_layer_sz;
    output_node_id_t output_node_num;

    ST_CHECK_PARAM((out_wt == NULL && fo_info == NULL) || fp == NULL
            || binary == NULL, -1);

    if (version < 3) {
        ST_WARNING("Too old version of connlm file");
        return -1;
    }

    if (fread(&flag.magic_num, sizeof(int), 1, fp) != 1) {
        ST_WARNING("Failed to load magic num.");
        return -1;
    }

    if (strncmp(flag.str, "    ", 4) == 0) {
        *binary = false;
    } else if (OUTPUT_WT_MAGIC_NUM != flag.magic_num) {
        ST_WARNING("magic num wrong.");
        return -2;
    } else {
        *binary = true;
    }

    if (out_wt != NULL) {
        *out_wt = NULL;
    }

    if (*binary) {
        if (fread(&in_layer_sz, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read in_layer_sz.");
            return -1;
        }
        if (fread(&output_node_num, sizeof(output_node_id_t),
                    1, fp) != 1) {
            ST_WARNING("Failed to read output_node_num.");
            return -1;
        }
    } else {
        if (st_readline(fp, "") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }
        if (st_readline(fp, "<OUTPUT_WT>") != 0) {
            ST_WARNING("tag error.");
            goto ERR;
        }

        if (st_readline(fp, "In layer size: %d", &in_layer_sz) != 1) {
            ST_WARNING("Failed to parse in_layer_sz.");
            goto ERR;
        }
        if (st_readline(fp, "Output node num: "OUTPUT_NODE_FMT,
                    &output_node_num) != 1) {
            ST_WARNING("Failed to parse output_node_num.");
            goto ERR;
        }
    }

    if (out_wt != NULL) {
        *out_wt = (out_wt_t *)malloc(sizeof(out_wt_t));
        if (*out_wt == NULL) {
            ST_WARNING("Failed to malloc out_wt_t");
            goto ERR;
        }
        memset(*out_wt, 0, sizeof(out_wt_t));

        (*out_wt)->in_layer_sz = in_layer_sz;
        (*out_wt)->output_node_num = output_node_num;
    }

    if (fo_info != NULL) {
        fprintf(fo_info, "\n<OUTPUT_WT>\n");
        fprintf(fo_info, "In layer size: %d\n", in_layer_sz);
        fprintf(fo_info, "Output node num: "OUTPUT_NODE_FMT"\n",
                output_node_num);
    }

    return 0;

ERR:
    if (out_wt != NULL) {
        safe_out_wt_destroy(*out_wt);
    }
    return -1;
}

int out_wt_load_body(out_wt_t *out_wt, int version, FILE *fp, bool binary)
{
    int n;
    size_t i;

    ST_CHECK_PARAM(out_wt == NULL || fp == NULL, -1);

    if (version < 3) {
        ST_WARNING("Too old version of connlm file");
        return -1;
    }

    if (out_wt_alloc(out_wt, out_wt->in_layer_sz,
                out_wt->output_node_num) < 0) {
        ST_WARNING("Failed to out_wt_alloc.");
        return -1;
    }

    if (binary) {
        if (fread(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read magic num.");
            goto ERR;
        }

        if (n != -OUTPUT_WT_MAGIC_NUM) {
            ST_WARNING("Magic num error.");
            goto ERR;
        }

        if (fread(out_wt->matrix, sizeof(real_t),
                out_wt->in_layer_sz * (out_wt->output_node_num - 1), fp)
                != out_wt->in_layer_sz * (out_wt->output_node_num - 1)) {
            ST_WARNING("Failed to read matrix.");
            goto ERR;
        }
    } else {
        if (st_readline(fp, "<OUTPUT_WT-DATA>") != 0) {
            ST_WARNING("body flag error.");
            goto ERR;
        }

        i = 0;
        while (i < out_wt->in_layer_sz * (out_wt->output_node_num - 1)) {
            if (st_readline(fp, REAL_FMT, out_wt->matrix + i) != 1) {
                ST_WARNING("Failed to parse matrix[%zu].", i);
                goto ERR;
            }
            i++;
        }
    }

    return 0;
ERR:

    return -1;
}

int out_wt_save_header(out_wt_t *out_wt, FILE *fp, bool binary)
{
    ST_CHECK_PARAM(fp == NULL, -1);

    if (binary) {
        if (fwrite(&OUTPUT_WT_MAGIC_NUM, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        if (fwrite(&out_wt->in_layer_sz, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to read in_layer_sz.");
            return -1;
        }
        if (fwrite(&out_wt->output_node_num, sizeof(output_node_id_t),
                    1, fp) != 1) {
            ST_WARNING("Failed to read output_node_num.");
            return -1;
        }
    } else {
        if (fprintf(fp, "    \n<OUTPUT_WT>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }

        if (fprintf(fp, "In layer size: %d\n", out_wt->in_layer_sz) < 0) {
            ST_WARNING("Failed to fprintf in_layer_sz.");
            return -1;
        }
        if (fprintf(fp, "Output node num: "OUTPUT_NODE_FMT"\n",
                    out_wt->output_node_num) < 0) {
            ST_WARNING("Failed to fprintf output_node_num.");
            return -1;
        }
    }

    return 0;
}

int out_wt_save_body(out_wt_t *out_wt, FILE *fp, bool binary)
{
    int n;
    size_t i;

    ST_CHECK_PARAM(fp == NULL, -1);

    if (binary) {
        n = -OUTPUT_WT_MAGIC_NUM;
        if (fwrite(&n, sizeof(int), 1, fp) != 1) {
            ST_WARNING("Failed to write magic num.");
            return -1;
        }

        if (fwrite(out_wt->matrix, sizeof(real_t),
                out_wt->in_layer_sz * (out_wt->output_node_num - 1), fp)
                != out_wt->in_layer_sz * (out_wt->output_node_num - 1)) {
            ST_WARNING("Failed to write matrix.");
            return -1;
        }
    } else {
        if (fprintf(fp, "<OUTPUT_WT-DATA>\n") < 0) {
            ST_WARNING("Failed to fprintf header.");
            return -1;
        }

        i = 0;
        while (i < out_wt->in_layer_sz * (out_wt->output_node_num - 1)) {
            if (fprintf(fp, REAL_FMT"\n", out_wt->matrix[i]) < 0) {
                ST_WARNING("Failed to fprintf matrix.");
                return -1;
            }
            i++;
        }
    }

    return 0;
}

out_wt_t* out_wt_init(int in_layer_sz, output_node_id_t output_node_num)
{
    out_wt_t *out_wt = NULL;
    size_t i;

    ST_CHECK_PARAM(in_layer_sz <= 0 || output_node_num <= 0, NULL);

    out_wt = (out_wt_t *) malloc(sizeof(out_wt_t));
    if (out_wt == NULL) {
        ST_WARNING("Falied to malloc out_wt_t.");
        goto ERR;
    }
    memset(out_wt, 0, sizeof(out_wt_t));

    if (out_wt_alloc(out_wt, in_layer_sz, output_node_num) < 0) {
        ST_WARNING("Failed to out_wt_alloc.");
        goto ERR;
    }

    for (i = 0; i < (output_node_num - 1) * in_layer_sz; i++) {
        out_wt->matrix[i] = st_random(-0.1, 0.1)
            + st_random(-0.1, 0.1) + st_random(-0.1, 0.1);
    }

    return out_wt;

ERR:
    safe_out_wt_destroy(out_wt);
    return NULL;
}

#if 0
static int output_hs_forward(real_t *p, real_t *in, real_t *wt,
        int out_wt_size, int *pts, char *codes, int max_code_len,
        real_t *ac)
{
    double d;
    int c;

    ST_CHECK_PARAM(p == NULL || in == NULL || wt == NULL
            || out_wt_size <= 0 || pts == NULL || codes == NULL
            || max_code_len <= 0 || ac == NULL, -1);

    d = 1.0;
    c = 0;
    while (pts[c] > 0 && c < max_code_len) {
        ac[c] = dot_product(ac, wt + pts[c] * out_wt_size, out_wt_size);

        sigmoid(ac + c, 1);

        if (codes[c] == 0) {
            d *= ac[c];
        } else {
            d *= (1 - ac[c]);
        }

        c++;
    }

    *p = d;

    return 0;
}

int output_activate_pre_layer(output_t *output, int cls, int tid)
{
    output_neuron_t *neu;

    ST_CHECK_PARAM(output == NULL || tid < 0, -1);

    neu = output->neurons + tid;
    if (output->output_opt.class_size > 0) {
        if (output->output_opt.hs && output->output_opt.class_hs) {
            if (cls < 0) {
                ST_WARNING("class not specified.");
                return -1;
            }
            if (output_hs_forward(&neu->p_hs_c, neu->ac_o_c,
                    output->wt_hs_c,
                    output->hs_out_wt_size,
                    output->pt_c + cls * output->output_opt.max_code_len,
                    output->code_c + cls * output->output_opt.max_code_len,
                    output->output_opt.max_code_len,
                    neu->ac_hs_c) < 0) {
                ST_WARNING("Failed to output_hs_forward class");
                return -1;
            }
        } else {
            softmax(neu->ac_o_c, output->output_opt.class_size);
        }
    }

    return 0;
}

int output_activate_last_layer(output_t *output, int word, int tid)
{
    output_neuron_t *neu;

    int cls;
    int s;
    int e;

    ST_CHECK_PARAM(output == NULL || word < 0 || tid < 0, -1);

    neu = output->neurons + tid;

    if (output->output_opt.class_size > 0) {
        cls = output->w2c[word];
        s = output->c2w_s[cls];
        e = output->c2w_e[cls];
    } else {
        cls = -1;
        s = 0;
        e = output->output_size;
    }

    if (output->output_opt.hs) {
        if (output_hs_forward(&neu->p_hs_w, neu->ac_o_w + s,
                output->wt_hs_w + output->hs_out_wt_size
                               * output_hs_wt_w_pos(cls, s),
                output->hs_out_wt_size,
                output->pt_w + word * output->output_opt.max_code_len,
                output->code_w + word * output->output_opt.max_code_len,
                output->output_opt.max_code_len,
                neu->ac_hs_w) < 0) {
            ST_WARNING("Failed to output_hs_forward word");
            return -1;
        }
    } else {
        softmax(neu->ac_o_w + s, e - s);
    }

    return 0;
}
#endif
