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

void output_wt_destroy(output_wt_t *output_wt)
{
    if (output_wt == NULL) {
        return;
    }

    output_wt->output = NULL;
}

output_wt_t* output_wt_dup(output_wt_t *o)
{
    output_wt_t *output_wt = NULL;

    ST_CHECK_PARAM(o == NULL, NULL);

    output_wt = (output_wt_t *) malloc(sizeof(output_wt_t));
    if (output_wt == NULL) {
        ST_WARNING("Falied to malloc output_wt_t.");
        goto ERR;
    }
    memset(output_wt, 0, sizeof(output_wt_t));

    output_wt->output = o->output;

    output_wt->forward = o->forward;
    output_wt->backprop = o->backprop;

    return output_wt;

ERR:
    safe_output_wt_destroy(output_wt);
    return NULL;
}

int output_wt_load_header(output_wt_t **output_wt, int version,
        FILE *fp, bool *binary, FILE *fo_info)
{
    return 0;
}

int output_wt_load_body(output_wt_t *output_wt, int version, FILE *fp, bool binary)
{
    return 0;
}

int output_wt_save_header(output_wt_t *output_wt, FILE *fp, bool binary)
{
    return 0;
}

int output_wt_save_body(output_wt_t *output_wt, FILE *fp, bool binary)
{
    return 0;
}

#if 0
static int output_hs_forward(real_t *p, real_t *in, real_t *wt,
        int output_wt_size, int *pts, char *codes, int max_code_len,
        real_t *ac)
{
    double d;
    int c;

    ST_CHECK_PARAM(p == NULL || in == NULL || wt == NULL
            || output_wt_size <= 0 || pts == NULL || codes == NULL
            || max_code_len <= 0 || ac == NULL, -1);

    d = 1.0;
    c = 0;
    while (pts[c] > 0 && c < max_code_len) {
        ac[c] = dot_product(ac, wt + pts[c] * output_wt_size, output_wt_size);

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
                    output->hs_output_wt_size,
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
                output->wt_hs_w + output->hs_output_wt_size
                               * output_hs_wt_w_pos(cls, s),
                output->hs_output_wt_size,
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
