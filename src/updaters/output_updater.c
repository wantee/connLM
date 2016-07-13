/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Wang Jian
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

#include <stutils/st_macro.h>
#include <stutils/st_log.h>

#include "output_updater.h"

void out_updater_destroy(out_updater_t *out_updater)
{
    if (out_updater == NULL) {
        return;
    }

    safe_free(out_updater->ac);
    safe_free(out_updater->er);

    out_updater->output = NULL;
}

out_updater_t* out_updater_create(output_t *output)
{
    out_updater_t *out_updater = NULL;

    ST_CHECK_PARAM(output == NULL, NULL);

    out_updater = (out_updater_t *)malloc(sizeof(out_updater_t));
    if (out_updater == NULL) {
        ST_WARNING("Failed to malloc out_updater.");
        goto ERR;
    }
    memset(out_updater, 0, sizeof(out_updater_t));

    out_updater->output = output;

    return out_updater;

ERR:
    safe_out_updater_destroy(out_updater);
    return NULL;
}

int out_updater_setup(out_updater_t *out_updater, bool backprob)
{
    ST_CHECK_PARAM(out_updater == NULL, -1);

    return 0;
}
#if 0
{
    output_neuron_t *neu;
    size_t sz;

    output_node_id_t num_node;
    int t;

    ST_CHECK_PARAM(output == NULL || num_thrs < 0, -1);

    output->n_neu = num_thrs;
    sz = sizeof(output_neuron_t) * num_thrs;
    output->neurons = (output_neuron_t *)malloc(sz);
    if (output->neurons == NULL) {
        ST_WARNING("Failed to malloc neurons.");
        goto ERR;
    }
    memset(output->neurons, 0, sz);

    num_node = output->tree->num_node;
    if (num_node > 0) {
        for (t = 0; t < num_thrs; t++) {
            neu = output->neurons + t;
            sz = sizeof(real_t) * num_node;
            if (posix_memalign((void **)&neu->ac, ALIGN_SIZE, sz) != 0
                    || neu->ac == NULL) {
                ST_WARNING("Failed to malloc ac.");
                goto ERR;
            }
            memset(neu->ac, 0, sz);

            if (backprop) {
                sz = sizeof(real_t) * num_node;
                if (posix_memalign((void **)&neu->er, ALIGN_SIZE, sz) != 0
                        || neu->er == NULL) {
                    ST_WARNING("Failed to malloc er.");
                    goto ERR;
                }
                memset(neu->er, 0, sz);
            }
        }
    }

    return 0;

ERR:
    if (output->neurons != NULL) {
        for (t = 0; t < output->n_neu; t++) {
            safe_free(output->neurons[t].ac);
            safe_free(output->neurons[t].er);
        }
        safe_free(output->neurons);
    }
    output->n_neu = 0;

    return -1;
}
#endif

int out_updater_reset(out_updater_t *out_updater)
{
    ST_CHECK_PARAM(out_updater == NULL, -1);

    return 0;
}

int out_updater_start(out_updater_t *out_updater, int word)
{
    ST_CHECK_PARAM(out_updater == NULL, -1);

    return 0;
}

int out_updater_forward(out_updater_t *out_updater, int word)
{
    ST_CHECK_PARAM(out_updater == NULL || word < 0, -1);

#if _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Forward:output: word[%d]", word);
#endif

    return 0;
}

int out_updater_backprop(out_updater_t *out_updater, int word)
{
    ST_CHECK_PARAM(out_updater == NULL || word < 0, -1);

#if _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Backprop:output: word[%d]", word);
#endif

    return 0;
}

int out_updater_end(out_updater_t *out_updater)
{
    ST_CHECK_PARAM(out_updater == NULL, -1);

    return 0;
}

int out_updater_finish(out_updater_t *out_updater)
{
    ST_CHECK_PARAM(out_updater == NULL, -1);

    return 0;
}
