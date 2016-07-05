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
#include <stutils/st_stack.h>

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

static int graph_dfs(graph_t *graph, int start,
        st_stack_t *node_stack, bool *on_stack,
        bool *visited, bool *passed,
        int *post_order, int *post_i)
{
    node_t *node;

    int l, lk;
    int to;
    st_stack_id_t s;

    void *tmp;

    ST_CHECK_PARAM(graph == NULL, -1);

    if (st_stack_push(node_stack, (void *)(long)start) != ST_STACK_OK) {
        ST_WARNING("Failed to st_stack_push node[%d].", start);
        return -1;
    }
#ifdef _GRAPH_DEBUG_
    ST_DEBUG("Node push: %d", start);
#endif
    visited[start] = true;
    on_stack[start] = true;

#ifdef _GRAPH_DEBUG_
    ST_DEBUG("DFS: %d", start);
#endif

    node = graph->nodes + start;
    for (l = 0; l < node->num_link; l++) {
        lk = node->links[l];
        passed[lk] = true;

        to = graph->links[lk].to;
        if(!visited[to]) {
            if (graph_dfs(graph, to, node_stack, on_stack,
                        visited, passed, post_order, post_i) < 0) {
                ST_WARNING("Failed to graph_dfs[%d].", to);
                return -1;
            }
        } else if(on_stack[to]) {
            for(s = 1; s <= node_stack->top; s++) {
                if (st_stack_topn(node_stack, s, &tmp) != ST_STACK_OK) {
                    ST_WARNING("Failed to st_stack_topn node.[%d]", s);
                    return -1;
                }

                graph->glues[(int)(long)tmp]->recur = true;

                if ((int)(long)tmp == to) {
                    break;
                }
            }
        }
    }

    if (st_stack_pop(node_stack, &tmp) != ST_STACK_OK) {
        ST_WARNING("Failed to st_stack_pop node.");
        return -1;
    }
#ifdef _GRAPH_DEBUG_
    ST_DEBUG("Node pop: %d", (int)(long)tmp);
#endif
    on_stack[start] = false;
    post_order[*post_i] = start;
    *post_i += 1;

    return 0;
}

/*
 * Get the forward order of nodes in a graph.
 */
int* graph_sort(graph_t *graph)
{
    st_stack_t *node_stack = NULL; /* node stack. */
    bool *on_stack = NULL; /* node whether on current stack. */
    bool *visited = NULL; /* node whether visited. */
    bool *passed = NULL; /* link whether passed. */
    int *post_order = NULL; /* sequence for post-order traverse. */
    int post_i;

    int n, t;
    int l;

    ST_CHECK_PARAM(graph == NULL, NULL);

    post_order = (int *)malloc(sizeof(int) * graph->num_node);
    if (post_order == NULL) {
        ST_WARNING("Failed to malloc post_order");
        goto ERR;
    }
    for (n = 0; n < graph->num_node; n++) {
        post_order[n] = -1;
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

    node_stack = st_stack_create((st_stack_id_t)graph->num_node);
    if (node_stack == NULL) {
        ST_WARNING("Failed to st_stack_create node_stack.");
        goto ERR;
    }
    (void)st_stack_clear(node_stack);

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

    post_i = 0;
    if (graph_dfs(graph, 0, node_stack, on_stack,
                visited, passed, post_order, &post_i) < 0) {
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

    safe_st_stack_destroy(node_stack);
    safe_free(on_stack);
    safe_free(visited);
    safe_free(passed);

#ifdef _GRAPH_DEBUG_
    ST_DEBUG("Post order:");
    for (n = 0; n < graph->num_node; n++) {
        ST_DEBUG("%d", post_order[n]);
    }
#endif
    /* reverse to get forward order. */
    for (n = 0; n < graph->num_node / 2; n++) {
        t = post_order[n];
        post_order[n] = post_order[graph->num_node - 1 - n];
        post_order[graph->num_node - 1 - n] = t;
    }
#ifdef _GRAPH_DEBUG_
    ST_DEBUG("Forward order:");
    for (n = 0; n < graph->num_node; n++) {
        ST_DEBUG("%d", post_order[n]);
    }
#endif

    return post_order;

ERR:
    safe_st_stack_destroy(node_stack);
    safe_free(on_stack);
    safe_free(visited);
    safe_free(passed);
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
    int m;
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
        for (l = 0; l < glues[g]->num_in_layer; l++) {
            if (strcasecmp(layers[glues[g]->in_layers[l]]->type,
                        INPUT_LAYER_NAME) == 0) {
                if (graph->root != -1) {
                    ST_WARNING("Too many weights out from input layer");
                }
                graph->root = g;
            }
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
        for (l = 0; l < glues[g]->num_in_layer; l++) {
            if (glue_set_add(out_glues + glues[g]->in_layers[l], g) < 0) {
                ST_WARNING("Failed to out_glue_add.");
                goto ERR;
            }
        }
    }
#ifdef _GRAPH_DEBUG_
    for (l = 0; l < n_layer; l++) {
        ST_DEBUG("Layer[%d/%s]:%d", l,
                layers[l]->name, out_glues[l].num_glue);
        for (g = 0; g < out_glues[l].num_glue; g++) {
            ST_DEBUG("    -> "GLUE_ID_FMT"/%s",
                    out_glues[l].glues[g],
                    glues[out_glues[l].glues[g]]->name);
        }
    }
#endif

    for (g = 0; g < n_glue; g++) {
        for (m = 0; m < glues[g]->num_out_layer; m++) {
            l = glues[g]->out_layers[m];
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
    }

    for (l = 0; l < n_layer; l++) {
        safe_free(out_glues[l].glues);
    }
    safe_free(out_glues);

#ifdef _GRAPH_DEBUG_
    ST_DEBUG("Graph: node: %d, link: %d",
            graph->num_node, graph->num_link);
    for (g = 0; g < graph->num_node; g++) {
        ST_DEBUG("Node[%d/%s]: %d", g,
                glues[graph->nodes[g].glue]->name,
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
