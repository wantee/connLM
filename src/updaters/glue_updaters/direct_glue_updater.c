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

#include <stutils/st_macro.h>
#include <stutils/st_log.h>

#include "../../glues/direct_glue.h"
#include "direct_glue_updater.h"

const unsigned int PRIMES[] = {
    108641969, 116049371, 125925907, 133333309, 145678979, 175308587,
    197530793, 234567803, 251851741, 264197411, 330864029, 399999781,
    407407183, 459258997, 479012069, 545678687, 560493491, 607407037,
    629629243, 656789717, 716048933, 718518067, 725925469, 733332871,
    753085943, 755555077, 782715551, 790122953, 812345159, 814814293,
    893826581, 923456189, 940740127, 953085797, 985184539, 990122807
};

const unsigned int PRIMES_SIZE = sizeof(PRIMES) / sizeof(PRIMES[0]);

typedef struct _direct_glue_updater_data_t_ {
    hash_t *hash;
} direct_glue_updater_data_t;

#define safe_direct_glue_updater_data_destroy(ptr) do {\
    if((ptr) != NULL) {\
        direct_glue_updater_data_destroy((direct_glue_updater_data_t *)ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)

void direct_glue_updater_data_destroy(direct_glue_updater_data_t *data)
{
    if (data == NULL) {
        return;
    }
    safe_free(data->hash);
}

direct_glue_updater_data_t* direct_glue_updater_data_init()
{
    direct_glue_updater_data_t *data = NULL;

    data = (direct_glue_updater_data_t *)malloc(sizeof(direct_glue_updater_data_t));
    if (data == NULL) {
        ST_WARNING("Failed to malloc direct_glue_updater_data.");
        goto ERR;
    }
    memset(data, 0, sizeof(direct_glue_updater_data_t));

    return data;
ERR:
    safe_direct_glue_updater_data_destroy(data);
    return NULL;
}

void direct_glue_updater_destroy(glue_updater_t *glue_updater)
{
    if (glue_updater == NULL) {
        return;
    }

    safe_direct_glue_updater_data_destroy(glue_updater->extra);
}

int direct_glue_updater_init(glue_updater_t *glue_updater)
{
    ST_CHECK_PARAM(glue_updater == NULL, -1);

    if (strcasecmp(glue_updater->glue->type, DIRECT_GLUE_NAME) != 0) {
        ST_WARNING("Not a direct glue_updater. [%s]",
                glue_updater->glue->type);
        return -1;
    }

    glue_updater->extra = (void *)direct_glue_updater_data_init();
    if (glue_updater->extra == NULL) {
        ST_WARNING("Failed to direct_glue_updater_data_init.");
        goto ERR;
    }

    return 0;

ERR:
    safe_direct_glue_updater_data_destroy(glue_updater->extra);
    return -1;
}

static int direct_wt_get_hash(hash_t *hash, hash_t init_val,
        int *frag, int order, bool reverse)
{
    int a;
    int b;

    hash[0] = PRIMES[0] * PRIMES[1] * init_val;
    if (reverse) {
        for (a = 1; a < order; a++) {
            if (frag[order - a] < 0 || frag[order - a] == UNK_ID) {
                // if OOV was in future, do not use
                // this N-gram feature and higher orders
                return a;
            }

            hash[a] = hash[0];
            for (b = 1; b <= a; b++) {
                hash[a] += PRIMES[(a*PRIMES[b] + b) % PRIMES_SIZE]
                    * (hash_t)(frag[order - b] + 1);
            }
        }
    } else {
        for (a = 1; a < order; a++) {
            if (frag[a - 1] < 0 || frag[a - 1] == UNK_ID) {
                // if OOV was in history, do not use
                // this N-gram feature and higher orders
                return a;
            }

            hash[a] = hash[0];
            for (b = 1; b <= a; b++) {
                hash[a] += PRIMES[(a*PRIMES[b] + b) % PRIMES_SIZE]
                    * (hash_t)(frag[b-1] + 1);
            }
        }
    }

    return order;
}

int direct_glue_updater_forward(glue_updater_t *glue_updater,
        comp_updater_t *comp_updater, out_updater_t *out_updater)
{
    direct_glue_data_t *data = NULL;

    ST_CHECK_PARAM(glue_updater == NULL || comp_updater == NULL
            || out_updater == NULL, -1);

    data = (direct_glue_data_t *)glue_updater->glue->extra;

#if 0
    if (direct_wt_forward(data->direct_wt,
                (direct_wt_data_t *)glue_updater->extra,
                glue_updater->out_scales[0],
                comp_updater, out_updater) < 0) {
        ST_WARNING("Failed to direct_wt_forward.");
        return -1;
    }

    input_neuron_t *in_neu;
    output_neuron_t *out_neu;

    hash_t h;
    int future_order;
    int order;

    output_path_t *path;
    output_node_id_t node;
    output_node_id_t n;
    output_node_id_t ch;

    int a;

    ST_CHECK_PARAM(direct_wt == NULL || input == NULL
            || output == NULL, -1);

    in_neu = input->neurons + tid;
    out_neu = output->neurons + tid;

    if (in_neu->n_frag - 1 > MAX_HASH_ORDER) {
        ST_WARNING("Too large order for hash. please enlarge "
                "MAX_HASH_ORDER and recompile.");
        return -1;
    }

    /* history ngrams. */
    order = direct_wt_get_hash(hash, (hash_t)1, in_neu->frag,
            in_neu->target, true);
    if (order < 0) {
        ST_WARNING("Failed to direct_wt_get_hash history.");
        return -1;
    }

    /* future ngrams. */
    if (in_neu->n_frag - in_neu->target - 1 > 0) {
        future_order = direct_wt_get_hash(hash + order, (hash_t)1,
                in_neu->frag + in_neu->target + 1,
                in_neu->n_frag - in_neu->target - 1,
                false);
        if (future_order < 0) {
            ST_WARNING("Failed to direct_wt_get_hash future.");
            return -1;
        }

        order += future_order;
    }

    for (a = 0; a < order; a++) {
        hash[a] = hash[a] % direct_wt->wt_sz;
    }

    path = output->paths + in_neu->frag[in_neu->target];
    for (a = 0; a < order; a++) {
        node = output->tree->root;
        ch = s_children(output->tree, node);
        h = hash[a] + ch;
        for (; ch < e_children(output->tree, node); ch++) {
            if (h >= direct_wt->wt_sz) {
                h = 0;
            }
            out_neu->ac[ch] += scale * direct_wt->hash_wt[h];
            h++;
        }
        for (n = 0; n < path->num_node; n++) {
            node = path->nodes[n];
            ch = s_children(output->tree, node);
            h = hash[a] + ch;
            for (; ch < e_children(output->tree, node); ch++) {
                if (h >= direct_wt->wt_sz) {
                    h = 0;
                }
                out_neu->ac[ch] += scale * direct_wt->hash_wt[h];
                h++;
            }
        }
    }

#endif
    return 0;
}

int direct_glue_updater_backprop(glue_updater_t *glue_updater, int tid)
{
    ST_CHECK_PARAM(glue_updater == NULL, -1);

    return 0;
}
