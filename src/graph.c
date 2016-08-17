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

#define NUM_REALLOC_LINK 10
static int graph_add_link(graph_t *graph)
{
    int lk;

    ST_CHECK_PARAM(graph == NULL, -1);

    if (graph->num_link >= graph->cap_link) {
        graph->links = (link_t *)realloc(graph->links, sizeof(link_t)
                * (graph->cap_link + NUM_REALLOC_LINK));
        if (graph->links == NULL) {
            ST_WARNING("Failed to realloc graph links.");
            return -1;
        }
        graph->cap_link += NUM_REALLOC_LINK;

        for (lk = graph->num_link; lk < graph->cap_link; lk++) {
            link_init(graph->links + lk);
        }
    }

    return graph->num_link++;
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

typedef struct _dfs_args_t_ {
    bool *on_stack;
    bool *visited;
    bool *passed;
    int post_i;
    int *recur_to; /* store the dest nodes for a recur node. */
} dfs_args_t;

static void dfs_args_destroy(dfs_args_t *args)
{
    if (args == NULL) {
        return;
    }

    safe_free(args->on_stack);
    safe_free(args->visited);
    safe_free(args->passed);
    safe_free(args->recur_to);
    args->post_i = 0;
}

static int dfs_args_init(dfs_args_t *args, graph_t *graph)
{
    size_t i, sz;

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

    // the num_node should not be a large number
    sz = graph->num_node * graph->num_node;
    args->recur_to = (int *)malloc(sizeof(int) * sz);
    if (args->recur_to == NULL) {
        ST_WARNING("Failed to malloc recur_to");
        goto ERR;
    }
    for (i = 0; i < sz; i++) {
        args->recur_to[i] = -1;
    }

    args->post_i = 0;

    return 0;
ERR:
    dfs_args_destroy(args);
    return -1;
}

static int graph_dfs(graph_t *graph, int start, int *post_order,
        dfs_args_t *args)
{
    node_t *node;

    int l, lk;
    int to, recur_i;

    ST_CHECK_PARAM(graph == NULL || post_order == NULL || args == NULL, -1);

    args->visited[start] = true;
    args->on_stack[start] = true;

#ifdef _GRAPH_DEBUG_
    ST_DEBUG("DFS: %d", start);
#endif

    recur_i = 0;
    node = graph->nodes + start;
    for (l = 0; l < node->num_link; l++) {
        lk = node->links[l];
        args->passed[lk] = true;

        to = graph->links[lk].to;
        if(!args->visited[to]) {
            if (graph_dfs(graph, to, post_order, args) < 0) {
                ST_WARNING("Failed to graph_dfs[%d].", to);
                return -1;
            }
        } else if(args->on_stack[to]) {
            graph->glues[start]->recur = true;
            if (to != start) {
                args->recur_to[start*graph->num_node + recur_i] = to;
                recur_i++;
#ifdef _GRAPH_DEBUG_
                ST_DEBUG("recur_to[%d]", to);
#endif
            }
        }
    }

    args->on_stack[start] = false;
    if (recur_i > 0) {
        post_order[args->post_i] = -(start + 1); // avoid start == 0
    } else {
        post_order[args->post_i] = start;
    }
    args->post_i += 1;

    return 0;
}

/*
 * in-place change the post order to forward order.
 * len(recur_to) == num_node * num_node, store the dest nodes for all
 * recur nodes. The recur node should be in front of its dest nodes.
 */
static int post2forward_order(int *post_order, int num_node, int *recur_to)
{
    int i, j, n, pos;
    int recur_node, t;
    int *to_nodes;

    int prev_node_left, recur_node_left;

    ST_CHECK_PARAM(post_order == NULL || recur_to == NULL, -1);

    /* reverse to get forward order. */
    for (n = 0; n < num_node / 2; n++) {
        t = post_order[n];
        post_order[n] = post_order[num_node - 1 - n];
        post_order[num_node - 1 - n] = t;
    }

    prev_node_left = -1;
    while (true) {
        recur_node_left = 0;
        /* find recur nodes and insert them to the right position. */
        for (n = 0; n < num_node; n++) {
            if (post_order[n] < 0) {
                recur_node = -post_order[n] - 1;
                to_nodes = recur_to + recur_node*num_node;
                if (to_nodes[0] < 0) {
                    ST_WARNING("No recur_to for recur_node[%d]", recur_node);
                    return -1;
                }

                /* find the most front dest node. */
                i = 0;
                pos = num_node;
                while (to_nodes[i] >= 0) {
                    for (j = 0; j < num_node; j++) {
                        if (-post_order[j] - 1 == to_nodes[i]) {
                            // our dest node is a recur node and
                            // it hasn't found its own position,
                            // so we wait for next round
                            recur_node_left++;
                            goto SKIP_ONE;
                        }

                        if (post_order[j] == to_nodes[i]) {
                            if (j < pos) {
                                pos = j;
                            }
                            break;
                        }
                    }
                    i++;
                }
                if (pos >= num_node) {
                    ST_WARNING("dest node[%d] not found in forward order",
                            to_nodes[i]);
                    return -1;
                }

                // insert this recur node to front of pos
                if (pos == n) {
                    ST_WARNING("dest node should not equal to recur_node.");
                    return -1;
                } else if (pos > n) {
                    for (i = n; i < pos - 1; i++) {
                        post_order[i] = post_order[i + 1];
                    }
                    post_order[pos - 1] = recur_node;
                } else {
                    for (i = n; i >= pos + 1; i--) {
                        post_order[i] = post_order[i - 1];
                    }
                    post_order[pos] = recur_node;
                }
            }
SKIP_ONE:
            continue;
        }

        if (recur_node_left <= 0) {
            break;
        }

        if (prev_node_left == recur_node_left) {
            ST_WARNING("Something went wrong, we can't find a proper "
                       "forward order for the graph.");
            return -1;
        }

        prev_node_left = recur_node_left;
    }

    return 0;
}

/*
 * Get the forward order of nodes in a graph.
 */
int* graph_sort(graph_t *graph)
{
    dfs_args_t args;
    int *post_order = NULL;

    int n;
    int l;

    ST_CHECK_PARAM(graph == NULL, NULL);

    memset(&args, 0, sizeof(dfs_args_t));

    post_order = (int *)malloc(sizeof(int) * graph->num_node);
    if (post_order == NULL) {
        ST_WARNING("Failed to malloc post_order");
        goto ERR;
    }
    for (n = 0; n < graph->num_node; n++) {
        post_order[n] = -graph->num_node;
    }

    if (graph->num_link <= 0) {
        if (graph->num_node == 1) {
            post_order[0] = graph->root;
#ifdef _GRAPH_DEBUG_
            ST_DEBUG("Forward order:");
            ST_DEBUG("%d", post_order[0]);
#endif
            return post_order;
        }

        ST_WARNING("Graph has no links.");
        goto ERR;
    }

    if (dfs_args_init(&args, graph) < 0) {
        ST_WARNING("Failed to dfs_args_init.");
        goto ERR;
    }

    if (graph_dfs(graph, 0, post_order, &args) < 0) {
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

    if (args.post_i != graph->num_node) {
        ST_WARNING("post order not finished.");
        goto ERR;
    }

#ifdef _GRAPH_DEBUG_
    ST_DEBUG("Post order:");
    for (n = 0; n < graph->num_node; n++) {
        ST_DEBUG("%d", post_order[n]);
    }
#endif
    if (post2forward_order(post_order, graph->num_node,
                args.recur_to) < 0) {
        ST_WARNING("Failed to post2forward_order.");
        goto ERR;
    }
#ifdef _GRAPH_DEBUG_
    ST_DEBUG("Forward order:");
    for (n = 0; n < graph->num_node; n++) {
        ST_DEBUG("%d", post_order[n]);
    }
#endif

    dfs_args_destroy(&args);

    return post_order;

ERR:
    dfs_args_destroy(&args);
    safe_free(post_order);

    return NULL;
}

typedef struct _out_glues_t_ {
    int *glues;
    int num_glue;
} glue_set_t;

static int glue_set_add(glue_set_t *glue_set, int g)
{
    ST_CHECK_PARAM(glue_set == NULL || g < 0, -1);

    glue_set->glues = (int *)realloc(glue_set->glues,
            sizeof(int) * (glue_set->num_glue + 1));
    if (glue_set->glues == NULL) {
        ST_WARNING("Failed to realloc glue_set glues.");
        return -1;
    }
    glue_set->glues[glue_set->num_glue] = g;

    return glue_set->num_glue++;
}

graph_t* graph_construct(layer_t **layers, int n_layer,
        glue_t **glues, int n_glue)
{
    graph_t *graph = NULL;
    glue_set_t *out_glues = NULL;

    size_t sz;
    int l;
    int g, gg;
    int lk;

    ST_CHECK_PARAM(layers == NULL || n_layer <= 0, NULL);

    graph = (graph_t *)malloc(sizeof(graph_t));
    if (graph == NULL) {
        ST_WARNING("Falied to malloc graph_t.");
        goto ERR;
    }
    memset(graph, 0, sizeof(graph_t));

    graph->nodes = (node_t *)malloc(sizeof(node_t) * n_glue);
    if (graph->nodes == NULL) {
        ST_WARNING("Falied to malloc nodes.");
        goto ERR;
    }
    graph->root = -1;
    for (g = 0; g < n_glue; g++) {
        graph->nodes[g].links = NULL;
        graph->nodes[g].num_link = 0;
        if (strcasecmp(layers[glues[g]->in_layer]->type,
                    INPUT_LAYER_NAME) == 0) {
            if (graph->root != -1) {
                ST_WARNING("Too many weights out from input layer");
            }
            graph->root = g;
        }
    }
    graph->num_node = n_glue;

    graph->links = (link_t *)malloc(sizeof(link_t) * n_layer);
    if (graph->links == NULL) {
        ST_WARNING("Falied to malloc links.");
        goto ERR;
    }
    graph->cap_link = n_layer;
    graph->num_link = 0;
    for (lk = graph->num_link; lk < graph->cap_link; lk++) {
        link_init(graph->links + lk);
    }

    sz = sizeof(glue_set_t) * n_layer;
    out_glues = (glue_set_t *)malloc(sz);
    if (out_glues == NULL) {
        ST_WARNING("Failed to malloc out_glues.");
        goto ERR;
    }
    memset(out_glues, 0, sz);

    for (g = 0; g < n_glue; g++) {
        if (glue_set_add(out_glues + glues[g]->in_layer, g) < 0) {
            ST_WARNING("Failed to out_glue_add.");
            goto ERR;
        }
    }
#ifdef _GRAPH_DEBUG_
    for (l = 0; l < n_layer; l++) {
        ST_DEBUG("Layer[%d/%s]:%d", l,
                layers[l]->name, out_glues[l].num_glue);
        for (g = 0; g < out_glues[l].num_glue; g++) {
            ST_DEBUG("    -> %d/%s",
                    out_glues[l].glues[g],
                    glues[out_glues[l].glues[g]]->name);
        }
    }
#endif

    for (g = 0; g < n_glue; g++) {
        l = glues[g]->out_layer;
        if (out_glues[l].glues != NULL) {
            for (gg = 0; gg < out_glues[l].num_glue; gg++) {
                lk = graph_add_link(graph);
                if (lk == -1) {
                    ST_WARNING("Failed to graph_add_link.");
                    goto ERR;
                }
                graph->links[lk].to = out_glues[l].glues[gg];

                if (node_add_link(graph->nodes + g, lk) == -1) {
                    ST_WARNING("Failed to node_add_link.");
                    goto ERR;
                }
            }
        }
    }

    for (l = 0; l < n_layer; l++) {
        safe_free(out_glues[l].glues);
    }
    safe_free(out_glues);

#ifdef _GRAPH_DEBUG_
    ST_DEBUG("Graph: node: %d, link: %d",
            graph->num_node, graph->num_link);
    for (g = 0; g < graph->num_node; g++) {
        ST_DEBUG("Node[%d/%s]: %d", g, glues[g]->name,
                graph->nodes[g].num_link);
        for (lk = 0; lk < graph->nodes[g].num_link; lk++) {
            ST_DEBUG(" %d -> %d/%s",
                    graph->nodes[g].links[lk],
                    graph->links[graph->nodes[g].links[lk]].to,
                    glues[graph->links[graph->nodes[g].links[lk]].to]->name);
        }
    }
#endif

    graph->glues = glues;

    return graph;

ERR:
    if (out_glues != NULL) {
        for (l = 0; l < n_layer; l++) {
            safe_free(out_glues[l].glues);
        }
        safe_free(out_glues);
    }
    safe_graph_destroy(graph);
    return NULL;
}
