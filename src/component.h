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

/** @defgroup g_component NNet Component
 * Component for a NNet, including a input layer, zero or more hidden layers and glues.
 */

/**
 * Parameters for training component.
 * @ingroup g_component
 */
typedef struct _component_train_opt_t_ {
} comp_train_opt_t;

/**
 * NNet component.
 * @ingroup g_component
 */
typedef struct _component_t_ {
    char name[MAX_NAME_LEN]; /**< component name. */

    input_t *input; /**< input layer. */

    layer_t **layers; /**< layers. 0 is output layer, 1 is input layer. */
    int num_layer; /**< number of hidden layers. */
    glue_t **glues; /**< glues. */
    int num_glue; /**< number of glues. */
    real_t comp_scale; /**< scale of component for output. */

    int* fwd_order; /**< forward order of glue. */
    int **glue_cycles; /**< cycles of glues for graph.
                       First elem for each cycle is num_glue for this cycle. */
    int num_glue_cycle;/**< number of cycles. */

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
 * @param[in] output output layer for component.
 * @param[in] input_size size of input layer.
 * @return component initialised, NULL if any error.
 */
component_t *comp_init_from_topo(const char* topo_content,
        output_t *output, int input_size);

/**
 * Load component train option.
 * @ingroup g_component
 * @param[in] comp component to be loaded with.
 * @param[in] opt runtime options passed by caller.
 * @param[in] sec_name section name of runtime options to be loaded.
 * @param[in] parent_param parent param.
 * @param[in] parent_bptt_opt parent bptt_opt.
 * @return non-zero value if any error.
 */
int comp_load_train_opt(component_t *comp, st_opt_t *opt, const char *sec_name,
        param_t *parent_param, bptt_opt_t *parent_bptt_opt);

/**
 * Load component header and initialise a new component.
 * @ingroup g_component
 * @param[out] comp component initialised.
 * @param[in] version file version of loading file.
 * @param[in] fp file stream loaded from.
 * @param[out] binary whether the file stream is in binary format.
 * @param[in] fo_info file stream used to print information, if it is not NULL.
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
 * Get nodename in graphviz format of input layer.
 * @ingroup g_component
 * @param[in] comp component.
 * @param[out] nodename buffer to write string.
 * @param[in] name_len length of label.
 * @return nodename on success, NULL if any error.
 */
char* comp_input_nodename(component_t *comp, char *nodename,
        size_t name_len);

/**
 * Convert a component into graphviz format.
 * @ingroup g_component
 * @param[in] comp component.
 * @param[in] fp file stream to be printed out.
 * @param[in] verbose verbose output.
 * @return non-zero value if any error.
 */
int comp_draw(component_t *comp, FILE *fp, bool verbose);

/**
 * Generate representation for wildcard symbol.
 * @ingroup g_component
 * @param[in] comp component.
 * @return non-zero value if any error.
 */
int comp_generate_wildcard_repr(component_t *comp);

/**
 * Do sanity check on a component and print warnings.
 * @ingroup g_component
 * @param[in] comp component
 */
void comp_sanity_check(component_t *comp);

#ifdef __cplusplus
}
#endif

#endif
