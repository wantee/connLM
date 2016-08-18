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

#include "input.h"
#include "graph.h"

static void node_destroy(node_t* node)
{
    if (node == NULL) {
        return;
    }

    safe_free(node->links);
    node->num_link = 0;
}

static void link_init(link_t* link)
{
    link->to = -1;
}

static void link_destroy(link_t* link)
{
    if (link == NULL) {
        return;
    }

    link_init(link);
}

void graph_destroy(graph_t* graph)
{
    int n;
    int l;

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

    graph->glues = NULL;
}

static int node_add_link(node_t *node, int lk)
{
    ST_CHECK_PARAM(node == NULL || lk == -1, -1);

    node->links = (int *)realloc(node->links, sizeof(int)
            * (node->num_link + 1));
    if (node->links == NULL) {
        ST_WARNING("Failed to realloc node links.");
        return -1;
    }
    node->links[node->num_link] = lk;

    return node->num_link++;
}

typedef struct _elem_set_t_ {
    int *elems;
    int num;
    int cap;
} elem_set_t;

static void elem_set_destroy(elem_set_t *set)
{
    if (set == NULL) {
        return;
    }

    safe_free(set->elems);
    set->num = 0;
    set->cap = 0;
}

static int elem_set_init(elem_set_t* set, int cap)
{
    ST_CHECK_PARAM(set == NULL || cap <= 0, -1);

    set->elems = (int *)malloc(sizeof(int) * cap);
    if (set->elems == NULL) {
        ST_WARNING("Failed to malloc ->elems.");
        goto ERR;
    }
    set->num = 0;
    set->cap = cap;

    return 0;
ERR:
    safe_free(set->elems);
    return -1;
}

static int elem_set_add(elem_set_t* set, int elem)
{
    ST_CHECK_PARAM(set == NULL, -1);

    if (set->num >= set->cap) {
        ST_WARNING("elem set overflow.");
        return -1;
    }

    set->elems[set->num] = elem;
    set->num++;

    return 0;
}

typedef struct _dfs_args_t_ {
    bool *on_stack;
    bool *visited;
    bool *passed;
    int *node_order;
    int node_i; /* index for node_order. */
    elem_set_t *recur_to; /* store the dest nodes for a recur link. */
    int num_link;
} dfs_args_t;

static void dfs_args_destroy(dfs_args_t *args)
{
    int i;

    if (args == NULL) {
        return;
    }

    safe_free(args->on_stack);
    safe_free(args->visited);
    safe_free(args->passed);
    for (i = 0; i < args->num_link; i++) {
        elem_set_destroy(args->recur_to + i);
    }
    args->num_link = 0;
    args->node_i = 0;
}

static int dfs_args_init(dfs_args_t *args, graph_t *graph)
{
    int i;

    ST_CHECK_PARAM(args == NULL || graph == NULL, -1);

    args->on_stack = (bool *)malloc(sizeof(bool) * graph->num_node);
    if (args->on_stack == NULL) {
        ST_WARNING("Failed to malloc on_stack");
        goto ERR;
    }
    memset(args->on_stack, 0, sizeof(bool) * graph->num_node);

    args->visited = (bool *)malloc(sizeof(bool) * graph->num_node);
    if (args->visited == NULL) {
        ST_WARNING("Failed to malloc visited");
        goto ERR;
    }
    memset(args->visited, 0, sizeof(bool) * graph->num_node);

    args->passed = (bool *)malloc(sizeof(bool) * graph->num_link);
    if (args->passed == NULL) {
        ST_WARNING("Failed to malloc passed");
        goto ERR;
    }
    memset(args->passed, 0, sizeof(bool) * graph->num_link);

    // the num_node and num_link should not be a large number
    args->recur_to = (elem_set_t *)malloc(sizeof(elem_set_t) * graph->num_link);
    if (args->recur_to == NULL) {
        ST_WARNING("Failed to malloc recur_to");
        goto ERR;
    }
    memset(args->recur_to, 0, sizeof(elem_set_t) * graph->num_link);
    args->num_link = graph->num_link;
    for (i = 0; i < graph->num_link; i++) {
        if (elem_set_init(args->recur_to + i, graph->num_node) < 0) {
            ST_WARNING("Failed to elem_set_init recur_to.");
            goto ERR;
        }
    }

    args->node_order = (int *)malloc(sizeof(int) * graph->num_node);
    if (args->node_order == NULL) {
        ST_WARNING("Failed to malloc node_order");
        goto ERR;
    }
    for (i = 0; i < graph->num_node; i++) {
        args->node_order[i] = -graph->num_node;
    }

    args->node_i = 0;

    return 0;
ERR:
    dfs_args_destroy(args);
    return -1;
}

static int graph_dfs(graph_t *graph, int start, dfs_args_t *args)
{
    node_t *node;

    int l, lk;
    int to;

    ST_CHECK_PARAM(graph == NULL || args == NULL, -1);

    args->visited[start] = true;
    args->on_stack[start] = true;

#ifdef _GRAPH_DEBUG_
    ST_DEBUG("DFS: %d", start);
#endif

    node = graph->nodes + start;
    for (l = 0; l < node->num_link; l++) {
        lk = node->links[l];
        args->passed[lk] = true;

        to = graph->links[lk].to;
        if(!args->visited[to]) {
            if (graph_dfs(graph, to, args) < 0) {
                ST_WARNING("Failed to graph_dfs[%d].", to);
                return -1;
            }
        } else if(args->on_stack[to]) {
            graph->glues[lk]->recur = true;
            if (elem_set_add(args->recur_to + lk, to) < 0) {
                ST_WARNING("Failed to elem_set_add for recur_to.");
                return -1;
            }
#ifdef _GRAPH_DEBUG_
            ST_DEBUG("recur_to[%d] for link[%d]", to, lk);
#endif
        }
    }

    args->on_stack[start] = false;
    args->node_order[args->node_i] = start;
    args->node_i += 1;

    return 0;
}

/*
 * convert node order to link order, considering the recur link.
 */
static int* node2link_order(int *node_order, graph_t *graph,
        elem_set_t *recur_to)
{
    int *link_order = NULL;
    elem_set_t *recur_links_for_node = NULL;

    size_t sz;
    int i, j, n, l, pos;
    int num_lk, lk;
    node_t *node;

    ST_CHECK_PARAM(node_order == NULL || graph == NULL
            || recur_to == NULL, NULL);

    link_order = (int *)malloc(sizeof(int) * graph->num_link);
    if (link_order == NULL) {
        ST_WARNING("Failed to malloc link_order");
        goto ERR;
    }

    // find positions for the recur links
    //
    // recur_links_for_node records which the recur link should be placed just
    // before this node. for node n the num of recur links is in
    // recur_links_for_node[n*(1+graph->num_link)], and the following is
    // the links
    sz = sizeof(elem_set_t) * graph->num_node;
    recur_links_for_node = (elem_set_t *)malloc(sz);
    if (recur_links_for_node == NULL) {
        ST_WARNING("Failed to malloc recur_pos");
        goto ERR;
    }
    memset(recur_links_for_node, 0, sz);
    for (i = 0; i < graph->num_node; i++) {
        if (elem_set_init(recur_links_for_node + i, graph->num_link) < 0) {
            ST_WARNING("Failed to elem_set_init recur_links_for_node.");
            goto ERR;
        }
    }
    for (n = 0; n < graph->num_node; n++) {
        node = graph->nodes + node_order[n];
        for (l = 0; l < node->num_link; l++) {
            lk = node->links[l];
            if (!graph->glues[lk]->recur) {
                continue;
            }

            if (recur_to[lk].num <= 0) {
                ST_WARNING("No recur_to for recur_link[%d]", lk);
                goto ERR;
            }

            /* find the most front dest node. */
            pos = graph->num_node;
            for (i = 0; i < recur_to[lk].num; i++) {
                for (j = 0; j < graph->num_node; j++) {
                    if (node_order[j] == recur_to[lk].elems[i]) {
                        if (j < pos) {
                            pos = j;
                        }
                        break;
                    }
                }
            }
            if (pos >= graph->num_node) {
                ST_WARNING("dest node[%d] not found in node order",
                        recur_to[lk].elems[i]);
                goto ERR;
            }

            if (elem_set_add(recur_links_for_node + pos, lk) < 0) {
                ST_WARNING("Failed to elem_set_add for recur_links_for_node.");
                goto ERR;
            }
        }
    }

    // sort the links
    num_lk = 0;
    for (n = 0; n < graph->num_node; n++) {
        node = graph->nodes + node_order[n];
        // place the recur links first
        for (i = 0; i < recur_links_for_node[n].num; i++) {
            if (num_lk >= graph->num_link) {
                ST_WARNING("num_lk overflow");
                goto ERR;
            }
            link_order[num_lk] = recur_links_for_node[n].elems[i];
            num_lk++;
        }
        for (l = 0; l < node->num_link; l++) {
            lk = node->links[l];
            if (graph->glues[lk]->recur) {
                continue;
            }

            if (num_lk >= graph->num_link) {
                ST_WARNING("num_lk overflow");
                goto ERR;
            }
            link_order[num_lk] = node->links[l];
            num_lk++;
        }
    }

    if (num_lk != graph->num_link) {
        ST_WARNING("Some links are missing.");
        goto ERR;
    }

    for (i = 0; i < graph->num_node; i++) {
        elem_set_destroy(recur_links_for_node + i);
    }
    safe_free(recur_links_for_node);

    return link_order;

ERR:
    if (recur_links_for_node != NULL) {
        for (i = 0; i < graph->num_node; i++) {
            elem_set_destroy(recur_links_for_node + i);
        }
        safe_free(recur_links_for_node);
    }
    safe_free(link_order);
    return NULL;
}

/*
 * Get the forward order of links(glues) in a graph.
 */
int* graph_sort(graph_t *graph)
{
    dfs_args_t args;
    int *link_order = NULL;

    int n, t;
    int l;

    ST_CHECK_PARAM(graph == NULL, NULL);

    memset(&args, 0, sizeof(dfs_args_t));

    if (dfs_args_init(&args, graph) < 0) {
        ST_WARNING("Failed to dfs_args_init.");
        goto ERR;
    }

    if (graph_dfs(graph, graph->root, &args) < 0) {
        ST_WARNING("Failed to dfs.");
        goto ERR;
    }

    for (n = 0; n < graph->num_node; n++) {
        if (!args.visited[n]) {
            ST_WARNING("Node[%d] not visited.", n);
            goto ERR;
        }
    }

    for (l = 0; l < graph->num_link; l++) {
        if (!args.passed[l]) {
            ST_WARNING("link[%d] not visited.", l);
            goto ERR;
        }
    }

    if (args.node_i != graph->num_node) {
        ST_WARNING("post order not finished.");
        goto ERR;
    }

    /* reverse to get forward order. */
    for (n = 0; n < graph->num_node / 2; n++) {
        t = args.node_order[n];
        args.node_order[n] = args.node_order[graph->num_node - 1 - n];
        args.node_order[graph->num_node - 1 - n] = t;
    }

#ifdef _GRAPH_DEBUG_
    ST_DEBUG("Node order:");
    for (n = 0; n < graph->num_node; n++) {
        ST_DEBUG("%d", args.node_order[n]);
    }
#endif

    link_order = node2link_order(args.node_order, graph, args.recur_to);
    if (link_order == NULL) {
        ST_WARNING("Failed to node2link_order.");
        goto ERR;
    }

#ifdef _GRAPH_DEBUG_
    ST_DEBUG("Link order:");
    for (n = 0; n < graph->num_link; n++) {
        ST_DEBUG("%d", link_order[n]);
    }
#endif

    dfs_args_destroy(&args);

    return link_order;

ERR:
    dfs_args_destroy(&args);
    safe_free(link_order);

    return NULL;
}

graph_t* graph_construct(layer_t **layers, int n_layer,
        glue_t **glues, int n_glue)
{
    graph_t *graph = NULL;

    int l;
    int g;

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
    graph->root = -1;
    for (l = 0; l < n_layer; l++) {
        graph->nodes[l].links = NULL;
        graph->nodes[l].num_link = 0;
        if (strcasecmp(layers[l]->type, INPUT_LAYER_NAME) == 0) {
            if (graph->root != -1) {
                ST_WARNING("Too many input layer.");
                goto ERR;
            }
            graph->root = l;
        }
    }
    graph->num_node = n_layer;

    graph->links = (link_t *)malloc(sizeof(link_t) * n_glue);
    if (graph->links == NULL) {
        ST_WARNING("Falied to malloc links.");
        goto ERR;
    }
    graph->num_link = n_glue;

    for (g = 0; g < n_glue; g++) {
        graph->links[g].to = glues[g]->out_layer;

        if (node_add_link(graph->nodes + glues[g]->in_layer, g) == -1) {
            ST_WARNING("Failed to node_add_link.");
            goto ERR;
        }
    }

#ifdef _GRAPH_DEBUG_
    ST_DEBUG("Graph: node: %d, link: %d", graph->num_node, graph->num_link);
    for (l = 0; l < graph->num_node; l++) {
        ST_DEBUG("Node[%d/%s]: %d", l, layers[l]->name,
                graph->nodes[l].num_link);
        for (g = 0; g < graph->nodes[l].num_link; g++) {
            ST_DEBUG(" %d -> %d/%s",
                    graph->nodes[l].links[g],
                    graph->links[graph->nodes[l].links[g]].to,
                    layers[graph->links[graph->nodes[l].links[g]].to]->name);
        }
    }
#endif

    graph->glues = glues;

    return graph;

ERR:
    safe_graph_destroy(graph);
    return NULL;
}
