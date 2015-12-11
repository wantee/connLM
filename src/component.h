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

#include "config.h"

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

#ifdef __cplusplus
}
#endif

#endif
