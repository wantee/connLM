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

#include <st_log.h>

#include "graph.h"

void node_destroy(node_t* node)
{
    if (node == NULL) {
        return;
    }

    safe_free(node->links);
    node->num_link = 0;
    node->layer = -1;
}

void link_destroy(link_t* link)
{
    if (link == NULL) {
        return;
    }
}

void graph_destroy(graph_t* graph)
{
    node_id_t n;
    link_id_t l;

    if (graph == NULL) {
        return;
    }

    for (n = 0; n < graph->num_node; n++) {
        node_destroy(graph->nodes + n);
    }
    safe_free(graph->nodes);
    graph->num_node = 0;

    for (l = 0; l < graph->num_link; l++) {
        link_destroy(graph->links + l);
    }
    safe_free(graph->links);
    graph->num_link = 0;
}

graph_t* graph_dup(graph_t *g)
{
    graph_t *graph = NULL;

    ST_CHECK_PARAM(g == NULL, NULL);

    graph = (graph_t *)malloc(sizeof(graph_t));
    if (graph == NULL) {
        ST_WARNING("Falied to malloc graph_t.");
        goto ERR;
    }
    memset(graph, 0, sizeof(graph_t));

    return graph;

ERR:
    safe_graph_destroy(graph);
    return NULL;
}

static link_id_t node_add_link(node_t *node, link_id_t lk)
{
    ST_CHECK_PARAM(node == NULL || lk == LINK_ID_NONE, LINK_ID_NONE);

    node->links = (link_id_t *)realloc(node->links, sizeof(link_id_t)
            * (node->num_link + 1));
    if (node->links == NULL) {
        ST_WARNING("Failed to realloc node links.");
        return LINK_ID_NONE;
    }
    node->links[node->num_link] = lk;

    return node->num_link;
}

graph_t* graph_construct(layer_t **layers, layer_id_t n_layer,
        glue_t **glues, glue_id_t n_glue)
{
    graph_t *graph = NULL;

    layer_id_t l;
    layer_id_t m;
    glue_id_t g;
    link_id_t lk;

    ST_CHECK_PARAM(layers == NULL || n_layer <= 0, NULL);

    graph = (graph_t *)malloc(sizeof(graph_t));
    if (graph == NULL) {
        ST_WARNING("Falied to malloc graph_t.");
        goto ERR;
    }
    memset(graph, 0, sizeof(graph_t));

    graph->nodes = (node_t *)malloc(sizeof(node_t) * n_layer);
    if (graph->nodes == NULL) {
        ST_WARNING("Falied to malloc nodes.");
        goto ERR;
    }
    memset(graph->nodes, 0, sizeof(node_t) * n_layer);
    graph->num_node = n_layer;

    if (n_glue > 0) {
        graph->num_link = 0;
        for (g = 0; g < n_glue; g++) {
            graph->num_link += glues[g]->num_in_layer
                * glues[g]->num_out_layer;
        }

        graph->links = (link_t *)malloc(sizeof(link_t) * graph->num_link);
        if (graph->links == NULL) {
            ST_WARNING("Falied to malloc links.");
            goto ERR;
        }
        memset(graph->links, 0, sizeof(link_t) * graph->num_link);
    }

    for (l = 0; l < n_layer; l++) {
        graph->nodes[l].layer = l;
    }

    lk = 0;
    for (g = 0; g < n_glue; g++) {
        for (l = 0; l < glues[g]->num_in_layer; l++) {
            for (m = 0; m < glues[g]->num_out_layer; m++) {
                graph->links[lk].glue = g;
                graph->links[lk].glue_in = l;
                graph->links[lk].glue_out = m;

                graph->links[lk].to = glues[g]->out_layers[m];
                if (node_add_link(graph->nodes + glues[g]->in_layers[l],
                            lk) == LINK_ID_NONE) {
                    ST_WARNING("Failed to node_add_link.");
                    goto ERR;
                }
                lk++;
            }
        }
    }

    /* initial node MUST be the first layer. */
    return graph;

ERR:
    safe_graph_destroy(graph);
    return NULL;
}
