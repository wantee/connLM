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
    node->glue = GLUE_ID_NONE;
}

static void link_init(link_t* link)
{
    link->to = NODE_ID_NONE;
    link->layer = LINK_ID_NONE;
    link->cycle = 0;
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

#define NUM_REALLOC_LINK 10
static link_id_t graph_add_link(graph_t *graph)
{
    link_id_t lk;

    ST_CHECK_PARAM(graph == NULL, LINK_ID_NONE);

    if (graph->num_link >= graph->cap_link) {
        graph->links = (link_t *)realloc(graph->links, sizeof(link_t)
                * (graph->cap_link + NUM_REALLOC_LINK));
        if (graph->links == NULL) {
            ST_WARNING("Failed to realloc graph links.");
            return LINK_ID_NONE;
        }
        graph->cap_link += NUM_REALLOC_LINK;

        for (lk = graph->num_link; lk < graph->cap_link; lk++) {
            link_init(graph->links + lk);
        }
    }

    return graph->num_link++;
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
    node_id_t to;
    st_stack_id_t s;

    void *tmp;

    ST_CHECK_PARAM(graph == NULL, -1);

    if (st_stack_push(node_stack, (void *)(long)start) != ST_STACK_OK) {
        ST_WARNING("Failed to st_stack_push node["NODE_ID_FMT"].", start);
        return -1;
    }
#ifdef _GRAPH_DEBUG_
    ST_DEBUG("Node push: "NODE_ID_FMT, start);
#endif
    visited[start] = true;
    on_stack[start] = true;

#ifdef _GRAPH_DEBUG_
    ST_DEBUG("DFS: "NODE_ID_FMT, start);
#endif

    node = graph->nodes + start;
    for (l = 0; l < node->num_link; l++) {
        lk = node->links[l];
        if (st_stack_push(link_stack, (void *)(long)lk) != ST_STACK_OK) {
            ST_WARNING("Failed to st_stack_push link["LINK_ID_FMT"].", lk);
            return -1;
        }
#ifdef _GRAPH_DEBUG_
        ST_DEBUG("Link push: "LINK_ID_FMT, lk);
#endif
        passed[lk] = true;

        to = graph->links[lk].to;
        if(!visited[to]) {
            if (graph_dfs(graph, to, node_stack, link_stack,
                    on_stack, visited, passed, post_order, post_i) < 0) {
                ST_WARNING("Failed to graph_dfs["NODE_ID_FMT"].", to);
                return -1;
            }
        } else if(on_stack[to]) {
            for(s = 1; s <= node_stack->top; s++) {
                if (st_stack_topn(link_stack, s, &tmp) != ST_STACK_OK) {
                    ST_WARNING("Failed to st_stack_topn link.["
                            LINK_ID_FMT"]", s);
                    return -1;
                }

                graph->links[(link_id_t)tmp].cycle = true;

                if (st_stack_topn(node_stack, s, &tmp) != ST_STACK_OK) {
                    ST_WARNING("Failed to st_stack_topn node.[%d]", s);
                    return -1;
                }
                if ((node_id_t)tmp == to) {
                    break;
                }
            }
        }

        if (st_stack_pop(link_stack, &tmp) != ST_STACK_OK) {
            ST_WARNING("Failed to st_stack_pop link.");
            return -1;
        }
#ifdef _GRAPH_DEBUG_
        ST_DEBUG("Link pop: "LINK_ID_FMT, (link_id_t)tmp);
#endif
    }

    if (st_stack_pop(node_stack, &tmp) != ST_STACK_OK) {
        ST_WARNING("Failed to st_stack_pop node.");
        return -1;
    }
#ifdef _GRAPH_DEBUG_
    ST_DEBUG("Node pop: "NODE_ID_FMT, (node_id_t)tmp);
#endif
    on_stack[start] = false;
    post_order[*post_i] = start;
    *post_i += 1;

    return 0;
}

/*
 * Get the forward order of nodes in a graph.
 */
node_id_t* graph_sort(graph_t *graph)
{
    st_stack_t *node_stack = NULL; /* node stack. */
    st_stack_t *link_stack = NULL; /* link stack. */
    bool *on_stack = NULL; /* node whether on current stack. */
    bool *visited = NULL; /* node whether visited. */
    bool *passed = NULL; /* link whether passed. */
    node_id_t *post_order = NULL; /* sequence for post-order traverse. */
    node_id_t post_i;

    node_id_t n, t;
    link_id_t l;

    ST_CHECK_PARAM(graph == NULL, NULL);

    post_order = (node_id_t *)malloc(sizeof(node_id_t) * graph->num_node);
    if (post_order == NULL) {
        ST_WARNING("Failed to malloc post_order");
        goto ERR;
    }
    for (n = 0; n < graph->num_node; n++) {
        post_order[n] = NODE_ID_NONE;
    }

    if (graph->num_link <= 0) {
        if (graph->num_node == 1) {
            post_order[0] = graph->root;
#ifdef _GRAPH_DEBUG_
            ST_DEBUG("Forward order:");
            ST_DEBUG(NODE_ID_FMT, post_order[0]);
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

    post_i = 0;
    if (graph_dfs(graph, 0, node_stack, link_stack,
                on_stack, visited, passed, post_order, &post_i) < 0) {
        ST_WARNING("Failed to dfs.");
        goto ERR;
    }

    for (n = 0; n < graph->num_node; n++) {
        if (!visited[n]) {
            ST_WARNING("Node["NODE_ID_FMT"] not visited.", n);
            goto ERR;
        }
    }

    for (l = 0; l < graph->num_link; l++) {
        if (!passed[l]) {
            ST_WARNING("link["LINK_ID_FMT"] not visited.", l);
            goto ERR;
        }
    }

    if (post_i != graph->num_node) {
        ST_WARNING("post order not finished.");
        goto ERR;
    }

    safe_st_stack_destroy(node_stack);
    safe_st_stack_destroy(link_stack);
    safe_free(on_stack);
    safe_free(visited);
    safe_free(passed);

#ifdef _GRAPH_DEBUG_
    ST_DEBUG("Post order:");
    for (n = 0; n < graph->num_node; n++) {
        ST_DEBUG(NODE_ID_FMT, post_order[n]);
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
        ST_DEBUG(NODE_ID_FMT, post_order[n]);
    }
#endif

    return post_order;

ERR:
    safe_st_stack_destroy(node_stack);
    safe_st_stack_destroy(link_stack);
    safe_free(on_stack);
    safe_free(visited);
    safe_free(passed);
    safe_free(post_order);

    return NULL;
}

typedef struct _out_glues_t_ {
    glue_id_t *glues;
    glue_id_t num_glue;
} glue_set_t;

static int glue_set_add(glue_set_t *glue_set, glue_id_t g)
{
    ST_CHECK_PARAM(glue_set == NULL || g == GLUE_ID_NONE, -1);

    glue_set->glues = (glue_id_t *)realloc(glue_set->glues,
            sizeof(glue_id_t) * (glue_set->num_glue + 1));
    if (glue_set->glues == NULL) {
        ST_WARNING("Failed to realloc glue_set glues.");
        return LINK_ID_NONE;
    }
    glue_set->glues[glue_set->num_glue] = g;

    return glue_set->num_glue++;
}

graph_t* graph_construct(layer_t **layers, layer_id_t n_layer,
        glue_t **glues, glue_id_t n_glue)
{
    graph_t *graph = NULL;
    glue_set_t *out_glues = NULL;

    size_t sz;
    layer_id_t l;
    layer_id_t m;
    glue_id_t g, gg;
    link_id_t lk;

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
    graph->root = NODE_ID_NONE;
    for (g = 0; g < n_glue; g++) {
        graph->nodes[g].links = NULL;
        graph->nodes[g].num_link = 0;
        graph->nodes[g].glue = g;
        for (l = 0; l < glues[g]->num_in_layer; l++) {
            if (strcasecmp(layers[glues[g]->in_layers[l]]->type,
                        INPUT_LAYER_NAME) == 0) {
                if (graph->root != NODE_ID_NONE) {
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
        ST_DEBUG("Layer["LAYER_ID_FMT"/%s]:"LAYER_ID_FMT, l,
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
                    if (lk == LINK_ID_NONE) {
                        ST_WARNING("Failed to graph_add_link.");
                        goto ERR;
                    }
                    graph->links[lk].layer = l;
                    graph->links[lk].to = out_glues[l].glues[gg];

                    if (node_add_link(graph->nodes + g,
                                lk) == LINK_ID_NONE) {
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
    ST_DEBUG("Graph: node: "NODE_ID_FMT", link: "LAYER_ID_FMT,
            graph->num_node, graph->num_link);
    for (g = 0; g < graph->num_node; g++) {
        ST_DEBUG("Node["NODE_ID_FMT"/%s]:"NODE_ID_FMT, g,
                glues[graph->nodes[g].glue]->name,
                graph->nodes[g].num_link);
        for (lk = 0; lk < graph->nodes[g].num_link; lk++) {
            ST_DEBUG(" "LINK_ID_FMT" -> "NODE_ID_FMT"/%s",
                    graph->nodes[g].links[lk],
                    graph->links[graph->nodes[g].links[lk]].to,
                    glues[graph->links[graph->nodes[g].links[lk]].to]->name);
        }
    }
#endif

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
