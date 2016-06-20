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

#ifndef  _CONNLM_GRAPH_H_
#define  _CONNLM_GRAPH_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <connlm/config.h>

#include "layers/layer.h"
#include "glues/glue.h"

/** @defgroup g_graph NNet graph.
 * Data structures and functions for NNet graph.
 */

/**
 * NNet graph link.
 * @ingroup g_graph
 */
typedef struct _link_t_ {
    int to; /**< node this link point to. */
} link_t;

/**
 * NNet graph node.
 * @ingroup g_graph
 */
typedef struct _node_t_ {
    int *links; /**< links from this node. */
    int num_link; /**< number of links. */
} node_t;

/**
 * NNet graph.
 * @ingroup g_graph
 */
typedef struct _graph_t_ {
    node_t *nodes; /**< nodes in graph. */
    int num_node; /**< number of nodes. */
    int cap_link; /**< capacity of links. */

    link_t *links; /**< links in graph. */
    int num_link; /**< number of links. */

    int root; /**< root of graph. */

    glue_t** glues; /**< glues(nodes) of graph. */
} graph_t;

/**
 * Destroy a graph and set the pointer to NULL.
 * @ingroup g_graph
 * @param[in] ptr pointer to graph_t.
 */
#define safe_graph_destroy(ptr) do {\
    if((ptr) != NULL) {\
        graph_destroy(ptr);\
        safe_free(ptr);\
        (ptr) = NULL;\
    }\
    } while(0)
/**
 * Destroy a graph.
 * @ingroup g_graph
 * @param[in] graph graph to be destroyed.
 */
void graph_destroy(graph_t* graph);

/**
 * Duplicate a graph.
 * @ingroup g_graph
 * @param[in] g graph to be duplicated.
 * @return the duplicated graph.
 */
graph_t* graph_dup(graph_t *g);

/**
 * construct a graph.
 * @ingroup g_graph
 * @param[in] layers layers used to construct graph.
 * @param[in] n_layer number of layers.
 * @param[in] glues glues used to construct graph.
 * @param[in] n_glue number of glues.
 * @return the constructed graph.
 */
graph_t* graph_construct(layer_t **layers, int n_layer,
        glue_t **glues, int n_glue);

/**
 * Topological sort nodes of a graph.
 * @ingroup g_graph
 * @param[in] graph the graph.
 * @return forward order of nodes, NULL if any error.
 */
int* graph_sort(graph_t *graph);

#ifdef __cplusplus
}
#endif

#endif
