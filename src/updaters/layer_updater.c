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
#include <stutils/st_mem.h>

#include "../layers/linear_layer.h"
#include "../layers/sigmoid_layer.h"
#include "../layers/tanh_layer.h"
#include "../layers/relu_layer.h"

#include "layer_updater.h"

void layer_updater_destroy(layer_updater_t *layer_updater)
{
    if (layer_updater == NULL) {
        return;
    }

    mat_destroy(&layer_updater->ac);
    mat_destroy(&layer_updater->er);

    mat_destroy(&layer_updater->ac_state);
    mat_destroy(&layer_updater->er_raw);

    mat_destroy(&layer_updater->pre_ac_state);

    layer_updater->layer = NULL;
}

typedef struct _layer_activate_func_t_ {
    char type[MAX_NAME_LEN];
    activate_func_t activate;
    deriv_func_t deriv;
    random_state_func_t random_state;
} layer_act_t;

static layer_act_t LAYER_ACT[] = {
    {LINEAR_NAME, linear_activate, linear_deriv, linear_random_state},
    {SIGMOID_NAME, sigmoid_activate, sigmoid_deriv, sigmoid_random_state},
    {TANH_NAME, tanh_activate, tanh_deriv, tanh_random_state},
    {RELU_NAME, relu_activate, relu_deriv, relu_random_state},
};

static layer_act_t* layer_get_act(const char *type)
{
    int t;

    for (t = 0; t < sizeof(LAYER_ACT) / sizeof(LAYER_ACT[0]); t++) {
        if (strcasecmp(type, LAYER_ACT[t].type) == 0) {
            return LAYER_ACT + t;
        }
    }

    return NULL;
}

layer_updater_t* layer_updater_create(layer_t *layer)
{
    layer_updater_t *layer_updater = NULL;

    ST_CHECK_PARAM(layer == NULL, NULL);

    layer_updater = (layer_updater_t *)st_malloc(sizeof(layer_updater_t));
    if (layer_updater == NULL) {
        ST_ERROR("Failed to st_malloc layer_updater.");
        goto ERR;
    }
    memset(layer_updater, 0, sizeof(layer_updater_t));

    layer_updater->layer = layer;
    layer_updater->activate = layer_get_act(layer->type)->activate;
    layer_updater->deriv = layer_get_act(layer->type)->deriv;
    layer_updater->random_state = layer_get_act(layer->type)->random_state;

    return layer_updater;

ERR:
    safe_layer_updater_destroy(layer_updater);
    return NULL;
}

int layer_updater_setup(layer_updater_t *layer_updater, bool backprop)
{
    ST_CHECK_PARAM(layer_updater == NULL, -1);

    if (mat_resize(&layer_updater->ac, 1,
                layer_updater->layer->size, 0.0) < 0) {
        ST_ERROR("Failed to mat_resize ac");
        return -1;
    }
    layer_updater->activated = false;

    if (backprop) {
        if (mat_resize(&layer_updater->er, 1,
                layer_updater->layer->size, 0.0) < 0) {
            ST_ERROR("Failed to mat_resize er");
            return -1;
        }
        layer_updater->derived = false;
    }

    return 0;
}

int layer_updater_setup_state(layer_updater_t *layer_updater, bool backprop)
{
    ST_CHECK_PARAM(layer_updater == NULL, -1);

    if (mat_resize(&layer_updater->ac_state, 1,
                layer_updater->layer->size, 0.0) < 0) {
        ST_ERROR("Failed to mat_resize ac_state.");
        return -1;
    }

    return 0;
}

int layer_updater_setup_er_raw(layer_updater_t *layer_updater)
{
    ST_CHECK_PARAM(layer_updater == NULL, -1);

    if (mat_resize(&layer_updater->er_raw, 1,
                layer_updater->layer->size, 0.0) < 0) {
        ST_ERROR("Failed to mat_resize ac_state.");
        return -1;
    }

    return 0;
}

int layer_updater_setup_pre_ac_state(layer_updater_t *layer_updater)
{
    ST_CHECK_PARAM(layer_updater == NULL, -1);

    if (mat_resize(&layer_updater->pre_ac_state, 1,
                layer_updater->layer->size, 0.0) < 0) {
        ST_ERROR("Failed to mat_resize ac_state.");
        return -1;
    }

    return 0;
}

int layer_updater_activate(layer_updater_t *layer_updater)
{
    ST_CHECK_PARAM(layer_updater == NULL, -1);

    if (layer_updater->activated) {
        return 0;
    }

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Activate: layer[%s]", layer_updater->layer->name);
#endif

    if (layer_updater->pre_ac_state.num_rows > 0) {
        if (mat_cpy(&layer_updater->pre_ac_state, &layer_updater->ac) < 0) {
            ST_ERROR("Failed to mat_cpy ac to pre_ac_state.");
            return -1;
        }
    }

    if (layer_updater->activate != NULL) {
        if (layer_updater->activate(layer_updater->layer,
                    &layer_updater->ac) < 0) {
            ST_ERROR("Failed to layer_updater->activate.[%s]",
                    layer_updater->layer->name);
            return -1;
        }
    }

    layer_updater->activated = true;

    return 0;
}

int layer_updater_deriv(layer_updater_t *layer_updater)
{
    ST_CHECK_PARAM(layer_updater == NULL, -1);

    if (layer_updater->derived) {
        return 0;
    }

#ifdef _CONNLM_TRACE_PROCEDURE_
    ST_TRACE("Deriv: layer[%s]", layer_updater->layer->name);
#endif

    if (layer_updater->er_raw.num_rows > 0) {
        if (mat_cpy(&layer_updater->er_raw, &layer_updater->er) < 0) {
            ST_ERROR("Failed to mat_cpy er to er_raw");
            return -1;
        }
    }

    if (layer_updater->deriv != NULL) {
        if (layer_updater->deriv(layer_updater->layer, &layer_updater->er,
                    &layer_updater->ac) < 0) {
            ST_ERROR("Failed to layer_updater->deriv.[%s]",
                    layer_updater->layer->name);
            return -1;
        }
    }

    layer_updater->derived = true;

    return 0;
}

int layer_updater_save_state(layer_updater_t *layer_updater)
{
    ST_CHECK_PARAM(layer_updater == NULL, -1);

    if (layer_updater->ac_state.num_rows > 0) {
        if (mat_cpy(&layer_updater->ac_state, &layer_updater->ac) < 0) {
            ST_ERROR("Failed to mat_cpy ac to ac_state");
            return -1;
        }
    }

    return 0;
}

int layer_updater_prepare(layer_updater_t *layer_updater, int batch_size)
{
    ST_CHECK_PARAM(layer_updater == NULL, -1);

    if (mat_resize(&layer_updater->ac, batch_size,
                layer_updater->layer->size, 0.0) < 0) {
        ST_ERROR("Failed to mat_resize ac");
        return -1;
    }
    layer_updater->activated = false;

    if (layer_updater->er.num_rows > 0) {
        if (mat_resize(&layer_updater->er, batch_size,
                layer_updater->layer->size, 0.0) < 0) {
            ST_ERROR("Failed to mat_resize er");
            return -1;
        }
        layer_updater->derived = false;
    }

    if (layer_updater->ac_state.num_rows > 0) {
        if (layer_updater->ac_state.num_rows != batch_size) {
            // init ac_state
            if (mat_resize(&layer_updater->ac_state, batch_size,
                        layer_updater->layer->size, 0.0) < 0) {
                ST_ERROR("Failed to mat_resize ac_state");
                return -1;
            }
        }
    }

    if (layer_updater->pre_ac_state.num_rows > 0) {
        if (layer_updater->pre_ac_state.num_rows != batch_size) {
            // init pre_ac_state
            if (mat_resize(&layer_updater->pre_ac_state, batch_size,
                        layer_updater->layer->size, 0.0) < 0) {
                ST_ERROR("Failed to mat_resize pre_ac_state");
                return -1;
            }
        }
    }

    return 0;
}

int layer_updater_reset(layer_updater_t *layer_updater, int batch_i)
{
    ST_CHECK_PARAM(layer_updater == NULL, -1);

    if (layer_updater->ac_state.num_rows > 0) {
        mat_set_row(&layer_updater->ac_state, batch_i, 0.0);
    }

    return 0;
}

int layer_updater_state_size(layer_updater_t *layer_updater)
{
    ST_CHECK_PARAM(layer_updater == NULL, -1);

    if (layer_updater->ac_state.num_rows > 0) {
        return layer_updater->layer->size;
    }

    return 0;
}

int layer_updater_dump_state(layer_updater_t *layer_updater, mat_t *state)
{
    ST_CHECK_PARAM(layer_updater == NULL || state == NULL, -1);

    if (layer_updater->ac_state.num_rows > 0) {
        if (state->num_rows != layer_updater->ac_state.num_rows
                || state->num_cols != layer_updater->ac_state.num_cols) {
            ST_ERROR("output buffer size not match.");
            return -1;
        }

        if (mat_cpy(state, &layer_updater->ac_state) < 0) {
            ST_ERROR("Failed to mat_cpy ac_state to state.");
            return -1;
        }
    }

    return 0;
}

int layer_updater_dump_pre_ac_state(layer_updater_t *layer_updater,
        mat_t *state)
{
    ST_CHECK_PARAM(layer_updater == NULL || state == NULL, -1);

    if (layer_updater->pre_ac_state.num_rows > 0) {
        if (state->num_rows != layer_updater->pre_ac_state.num_rows
                || state->num_cols != layer_updater->pre_ac_state.num_cols) {
            ST_ERROR("output buffer size not match.");
            return -1;
        }

        if (mat_cpy(state, &layer_updater->pre_ac_state) < 0) {
            ST_ERROR("Failed to mat_cpy pre_ac_state to state");
            return -1;
        }
    }

    return 0;
}

int layer_updater_feed_state(layer_updater_t *layer_updater, mat_t *state)
{
    ST_CHECK_PARAM(layer_updater == NULL || state == NULL, -1);

    if (layer_updater->ac_state.num_rows > 0) {
        if (state->num_rows != layer_updater->ac_state.num_rows
                || state->num_cols != layer_updater->ac_state.num_cols) {
            ST_ERROR("input buffer size not match.");
            return -1;
        }

        if (mat_cpy(&layer_updater->ac_state, state) < 0) {
            ST_ERROR("Failed to mat_cpy state to ac_state");
            return -1;
        }
    }

    return 0;
}

int layer_updater_random_state(layer_updater_t *layer_updater, mat_t *state)
{

    ST_CHECK_PARAM(layer_updater == NULL || state == NULL, -1);

    if (layer_updater->ac_state.num_rows > 0 && layer_updater->random_state != NULL) {
        if (state->num_cols != layer_updater->layer->size) {
            ST_ERROR("output buffer col not match.");
            return -1;
        }

        if (layer_updater->random_state(layer_updater->layer, state) < 0) {
            ST_ERROR("Failed to layer_updater->random_state.[%s]",
                    layer_updater->layer->name);
            return -1;
        }
    }

    return 0;
}

int layer_updater_activate_state(layer_updater_t *layer_updater, mat_t *state)
{

    ST_CHECK_PARAM(layer_updater == NULL || state == NULL, -1);

    if (layer_updater->activate != NULL) {
        if (state->num_cols != layer_updater->layer->size) {
            ST_ERROR("output buffer col not match.");
            return -1;
        }
        if (layer_updater->activate(layer_updater->layer, state) < 0) {
            ST_ERROR("Failed to layer_updater->activate.[%s]",
                    layer_updater->layer->name);
            return -1;
        }
    }

    return 0;
}
