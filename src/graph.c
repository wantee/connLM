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
#include <st_stack.h>

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
    safe_free(graph->fwd_order);

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

    return node->num_link++;
}

static int graph_dfs(graph_t *graph, node_id_t start,
        st_stack_t *node_stack, st_stack_t *link_stack,
        bool *on_stack, bool *visited, bool *passed,
        node_id_t *post_order, node_id_t *post_i)
{
    node_t *node;

    link_id_t l, lk;
    node_id_t n, to;
    st_stack_id_t s;

    ST_CHECK_PARAM(graph == NULL, -1);

    visited[start] = true;
    ST_DEBUG("DFS: %d", start);

    if (st_stack_push(node_stack, (void *)(long)start) != ST_STACK_OK) {
        ST_WARNING("Failed to st_stack_push node[%d].", start);
        return -1;
    }
    ST_DEBUG("Node push: %d", start);
    on_stack[start] = true;

    node = graph->nodes + start;
    for (l = 0; l < node->num_link; l++) {
        lk = node->links[l];
        if (st_stack_push(link_stack, (void *)(long)lk) != ST_STACK_OK) {
            ST_WARNING("Failed to st_stack_push link[%d].", lk);
            return -1;
        }
        ST_DEBUG("Link push: %d", lk);

        to = graph->links[node->links[l]].to;
        if(!visited[to]) {
            if (graph_dfs(graph, to, node_stack, link_stack,
                    on_stack, visited, passed, post_order, post_i) < 0) {
                ST_WARNING("Failed to graph_dfs[%d].", to);
                return -1;
            }
        } else if(on_stack[to]) {
            for(s = 1; s <= node_stack->top; s++) {
                if (st_stack_topn(node_stack, s, (void *)&n) != ST_STACK_OK) {
                    ST_WARNING("Failed to st_stack_topn node.[%d]", s);
                    return -1;
                }

                if (st_stack_topn(link_stack, s, (void *)&lk) != ST_STACK_OK) {
                    ST_WARNING("Failed to st_stack_topn link.[%d]", s);
                    return -1;
                }

                graph->links[lk].cycle = true;

                if (n == to) {
                    break;
                }
            }
        }

        if (st_stack_pop(link_stack, (void **)&lk) != ST_STACK_OK) {
            ST_WARNING("Failed to st_stack_pop link.");
            return -1;
        }
        ST_DEBUG("Link pop: %d", lk);
    }

    if (st_stack_pop(node_stack, (void **)&n) != ST_STACK_OK) {
        ST_WARNING("Failed to st_stack_pop node.");
        return -1;
    }
    ST_DEBUG("Node pop: %d", n);
    on_stack[start] = false;
    post_order[*post_i] = start;
    *post_i += 1;

    return 0;
}

static int graph_sort(graph_t *graph)
{
    st_stack_t *node_stack = NULL; /* node stack. */
    st_stack_t *link_stack = NULL; /* link stack. */
    bool *on_stack = NULL; /* node whether on current stack. */
    bool *visited = NULL; /* node whether visited. */
    bool *passed = NULL; /* link whether passed. */
    node_id_t *post_order = NULL; /* sequence for post-order traverse. */
    node_id_t post_i;

    node_id_t n;
    link_id_t l;

    ST_CHECK_PARAM(graph == NULL || graph->fwd_order == NULL, -1);

    if (graph->num_link <= 0) {
        if (graph->num_node == 1) {
            graph->fwd_order[0] = 0;
            return 0;
        }

        ST_WARNING("Graph has no links.");
        return -1;
    }

    node_stack = st_stack_create((st_stack_id_t)graph->num_node);
    if (node_stack == NULL) {
        ST_WARNING("Failed to st_stack_create node_stack.");
        goto ERR;
    }
    (void)st_stack_clear(node_stack);

    link_stack = st_stack_create((st_stack_id_t)graph->num_link);
    if (link_stack == NULL) {
        ST_WARNING("Failed to st_stack_create link_stack.");
        goto ERR;
    }
    (void)st_stack_clear(link_stack);

    on_stack = (bool *)malloc(sizeof(bool) * graph->num_node);
    if (on_stack == NULL) {
        ST_WARNING("Failed to malloc on_stack");
        goto ERR;
    }
    memset(on_stack, 0, sizeof(bool) * graph->num_node);

    visited = (bool *)malloc(sizeof(bool) * graph->num_node);
    if (visited == NULL) {
        ST_WARNING("Failed to malloc visited");
        goto ERR;
    }
    memset(visited, 0, sizeof(bool) * graph->num_node);

    passed = (bool *)malloc(sizeof(bool) * graph->num_link);
    if (passed == NULL) {
        ST_WARNING("Failed to malloc passed");
        goto ERR;
    }
    memset(passed, 0, sizeof(bool) * graph->num_link);

    post_order = (node_id_t *)malloc(sizeof(node_id_t) * graph->num_node);
    if (post_order == NULL) {
        ST_WARNING("Failed to malloc post_order");
        goto ERR;
    }
    memset(post_order, 0, sizeof(node_id_t) * graph->num_node);

    post_i = 0;
    /* initial node MUST be the first layer. */
    if (graph_dfs(graph, 0, node_stack, link_stack,
                on_stack, visited, passed, post_order, &post_i) < 0) {
        ST_WARNING("Failed to dfs.");
        goto ERR;
    }

    for (n = 0; n < graph->num_node; n++) {
        if (!visited[n]) {
            ST_WARNING("Node[%d] not visited.", n);
            goto ERR;
        }
    }

    for (l = 0; l < graph->num_link; l++) {
        if (!passed[l]) {
            ST_WARNING("link[%d] not visited.", l);
            goto ERR;
        }
    }

    if (post_i != graph->num_node) {
        ST_WARNING("post order not finished.");
        goto ERR;
    }

    for (n = 0; n < graph->num_node; n++) {
        graph->fwd_order[n] = post_order[graph->num_node - n - 1];
    }

    safe_st_stack_destroy(node_stack);
    safe_st_stack_destroy(link_stack);
    safe_free(on_stack);
    safe_free(visited);
    safe_free(passed);
    safe_free(post_order);
    return 0;

ERR:
    safe_st_stack_destroy(node_stack);
    safe_st_stack_destroy(link_stack);
    safe_free(on_stack);
    safe_free(visited);
    safe_free(passed);
    safe_free(post_order);

    return -1;
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

    graph->fwd_order = (node_id_t *)malloc(sizeof(node_id_t) * graph->num_node);
    if (graph->fwd_order == NULL) {
        ST_WARNING("Falied to malloc fwd_order.");
        goto ERR;
    }
    memset(graph->fwd_order, 0, sizeof(node_id_t) * graph->num_node);

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

    ST_DEBUG("Graph: node: %d, link: %d", graph->num_node, graph->num_link);

    if (graph_sort(graph) < 0) {
        ST_WARNING("Failed to graph_sort.");
        goto ERR;
    }

    return graph;

ERR:
    safe_graph_destroy(graph);
    return NULL;
}
