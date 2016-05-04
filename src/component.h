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

#ifndef  _CONNLM_COMPONENT_H_
#define  _CONNLM_COMPONENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#include <connlm/config.h>

#include "input.h"
#include "output.h"
#include "layers/layer.h"
#include "glues/glue.h"
#include "weights/embedding_weight.h"
#include "weights/output_weight.h"
#include "graph.h"

/** @defgroup g_component NNet component.
 * Data structures and functions for NNet component.
 */

typedef int comp_id_t;
#define COMP_ID_FMT "%d"

/**
 * Parameters for training component.
 * @ingroup g_component
 */
typedef struct _component_train_opt_t_ {
    param_t param;            /**< training parameters. */
} comp_train_opt_t;

/**
 * NNet component.
 * @ingroup g_component
 */
typedef struct _component_t_ {
    char name[MAX_NAME_LEN]; /**< component name. */

    input_t *input; /**< input layer. */
    emb_wt_t *emb; /**< embedding weight. */

    layer_t **layers; /**< hidden layers. */
    layer_id_t num_layer; /**< number of hidden layers. */
    glue_t **glues; /**< glues. */
    glue_id_t num_glue; /**< number of glues. */

    graph_t *graph; /**< nnet graph for this component. */

    output_wt_t *output_wt; /**< output weights. */

    comp_train_opt_t train_opt; /**< train options. */
} component_t;

/**
 * Destroy a component and set the pointer to NULL.
 * @ingroup g_component
 * @param[in] ptr pointer to component_t.
 */
#define safe_comp_destroy(ptr) do {\
    if((ptr) != NULL) {\
        comp_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a component.
 * @ingroup g_component
 * @param[in] comp component to be destroyed.
 */
void comp_destroy(component_t *comp);

/**
 * Duplicate a component.
 * @ingroup g_component
 * @param[in] c component to be duplicated.
 * @return the duplicated component, NULL if any error.
 */
component_t* comp_dup(component_t *c);

/**
 * Initialize input and hidden layers for a component with topology file.
 * @ingroup g_component
 * @param[in] topo_content content for component in a topology file.
 * @return component initialised, NULL if any error.
 */
component_t *comp_init_from_topo(const char* topo_content);

/**
 * Construct the nnet graph for a component.
 * @ingroup g_component
 * @param[in] comp component to construct graph.
 * @return non-zero value if any error.
 */
int comp_construct_graph(component_t *comp);

/**
 * Initialize output weight for a component with specific output layer.
 * @ingroup g_component
 * @param[out] comp component to be initialised.
 * @param[in] output output layer.
 * @param[in] scale output scale.
 * @return non-zero value if any error.
 */
int comp_init_output_wt(component_t *comp, output_t *output, real_t scale);

/**
 * Load component train option.
 * @ingroup g_conmponent
 * @param[in] conmponent conmponent to be loaded with.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @return non-zero value if any error.
 */
int comp_load_train_opt(component_t *comp, st_opt_t *opt,
        const char *sec_name);

/**
 * Load component header and initialise a new component.
 * @ingroup g_component
 * @param[out] comp component initialised.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[out] binary whether the file stream is in binary format.
 * @param[in] fo file stream used to print information, if it is not NULL.
 * @see comp_load_body
 * @see comp_save_header, comp_save_body
 * @return non-zero value if any error.
 */
int comp_load_header(component_t **comp, int version,
        FILE *fp, bool *binary, FILE *fo_info);

/**
 * Load component body.
 * @ingroup g_component
 * @param[in] comp component to be loaded.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[in] binary whether to use binary format.
 * @see comp_load_header
 * @see comp_save_header, comp_save_body
 * @return non-zero value if any error.
 */
int comp_load_body(component_t *comp, int version, FILE *fp, bool binary);

/**
 * Save component header.
 * @ingroup g_component
 * @param[in] comp component to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see comp_save_body
 * @see comp_load_header, comp_load_body
 * @return non-zero value if any error.
 */
int comp_save_header(component_t *comp, FILE *fp, bool binary);

/**
 * Save component body.
 * @ingroup g_component
 * @param[in] comp component to be saved.
 * @param[in] fp file stream saved to.
 * @param[in] binary whether to use binary format.
 * @see comp_save_header
 * @see comp_load_header, comp_load_body
 * @return non-zero value if any error.
 */
int comp_save_body(component_t *comp, FILE *fp, bool binary);


/**
 * Setup runinng for component.
 * Called before runinng.
 * @ingroup g_component
 * @param[in] comp component.
 * @param[in] num_thrs number of thread to be used.
 * @param[in] backprop whether do backpropagating.
 * @return non-zero value if any error.
 */
int comp_setup(component_t *comp, output_t *output, int num_thrs,
        bool backprop);

/**
 * Reset runinng for component.
 * Called before every input sentence to be runned.
 * @ingroup g_component
 * @param[in] comp component.
 * @param[in] tid thread id (neuron id).
 * @param[in] backprop whether do backpropagating.
 * @return non-zero value if any error.
 */
int comp_reset(component_t *comp, int tid, bool backprop);

/**
 * Start runinng for component.
 * Called before every input word to be runned.
 * @ingroup g_component
 * @param[in] comp component.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @param[in] backprop whether do backpropagating.
 * @return non-zero value if any error.
 */
int comp_start(component_t *comp, int word, int tid, bool backprop);

/**
 * End runinng for component.
 * Called after every input word runned.
 * @ingroup g_component
 * @param[in] comp component.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @param[in] backprop whether do backpropagating.
 * @return non-zero value if any error.
 */
int comp_end(component_t *comp, int word, int tid, bool backprop);

/**
 * Finish runinng for component.
 * Called after all words runned.
 * @ingroup g_component
 * @param[in] comp component.
 * @param[in] tid thread id (neuron id).
 * @param[in] backprop whether do backpropagating.
 * @return non-zero value if any error.
 */
int comp_finish(component_t *comp, int tid, bool backprop);

/**
 * Training between feed-forward and back-propagate for component.
 * Called between forward and backprop during training a word.
 * @ingroup g_component
 * @param[in] comp component.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @return non-zero value if any error.
 */
int comp_fwd_bp(component_t *comp, int word, int tid);

/**
 * Feed-forward one word for a thread of component.
 * @ingroup g_component
 * @param[in] comp component.
 * @param[in] tid thread id (neuron id).
 * @see rnn_forward
 * @return non-zero value if any error.
 */
int comp_forward(component_t *comp, int tid);

/**
 * Back-propagate one word for a thread of component.
 * @ingroup g_component
 * @param[in] comp component.
 * @param[in] word current word.
 * @param[in] tid thread id (neuron id).
 * @see rnn_forward
 * @return non-zero value if any error.
 */
int comp_backprop(component_t *comp, int word, int tid);

#ifdef __cplusplus
}
#endif

#endif
