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

    safe_st_free(node->links);
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
    int i;

    if (graph == NULL) {
        return;
    }

    for (i = 0; i < graph->num_node; i++) {
        node_destroy(graph->nodes + i);
    }
    safe_st_free(graph->nodes);
    graph->num_node = 0;

    for (i = 0; i < graph->num_link; i++) {
        link_destroy(graph->links + i);
    }
    safe_st_free(graph->links);
    graph->num_link = 0;

    graph->glues = NULL;

    for (i = 0; i < graph->num_cycle; i++) {
        safe_st_free(graph->cycles[i]);
    }
    safe_st_free(graph->cycles);
    graph->num_cycle = 0;
}

static int node_add_link(node_t *node, int lk)
{
    ST_CHECK_PARAM(node == NULL || lk == -1, -1);

    node->links = (int *)st_realloc(node->links, sizeof(int)
            * (node->num_link + 1));
    if (node->links == NULL) {
        ST_ERROR("Failed to st_realloc node links.");
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

    safe_st_free(set->elems);
    set->num = 0;
    set->cap = 0;
}

static int elem_set_init(elem_set_t* set, int cap)
{
    ST_CHECK_PARAM(set == NULL || cap <= 0, -1);

    set->elems = (int *)st_malloc(sizeof(int) * cap);
    if (set->elems == NULL) {
        ST_ERROR("Failed to st_malloc ->elems.");
        goto ERR;
    }
    set->num = 0;
    set->cap = cap;

    return 0;
ERR:
    safe_st_free(set->elems);
    return -1;
}

static int elem_set_add(elem_set_t* set, int elem)
{
    ST_CHECK_PARAM(set == NULL, -1);

    if (set->num >= set->cap) {
        ST_ERROR("elem set overflow.");
        return -1;
    }

    set->elems[set->num] = elem;
    set->num++;

    return 0;
}

typedef struct _dfs_args_t_ {
    bool *on_stack;
    bool *visited;
    st_stack_t *link_stack;
    bool *passed;
    int *node_order;
    int node_i; /* index for node_order. */
    int *recur_to; /* store the dest node for a recur link. */
} dfs_args_t;

static void dfs_args_destroy(dfs_args_t *args)
{
    if (args == NULL) {
        return;
    }

    safe_st_free(args->on_stack);
    safe_st_free(args->visited);
    safe_st_stack_destroy(args->link_stack);
    safe_st_free(args->passed);
    safe_st_free(args->recur_to);
    safe_st_free(args->node_order);
    args->node_i = 0;
}

static int dfs_args_init(dfs_args_t *args, graph_t *graph)
{
    int i;

    ST_CHECK_PARAM(args == NULL || graph == NULL, -1);

    args->on_stack = (bool *)st_malloc(sizeof(bool) * graph->num_node);
    if (args->on_stack == NULL) {
        ST_ERROR("Failed to st_malloc on_stack");
        goto ERR;
    }
    memset(args->on_stack, 0, sizeof(bool) * graph->num_node);

    args->visited = (bool *)st_malloc(sizeof(bool) * graph->num_node);
    if (args->visited == NULL) {
        ST_ERROR("Failed to st_malloc visited");
        goto ERR;
    }
    memset(args->visited, 0, sizeof(bool) * graph->num_node);

    args->link_stack = st_stack_create((st_stack_id_t)graph->num_link);
    if (args->link_stack == NULL) {
        ST_ERROR("Failed to st_stack_create link_stack.");
        goto ERR;
    }
    (void)st_stack_clear(args->link_stack);

    args->passed = (bool *)st_malloc(sizeof(bool) * graph->num_link);
    if (args->passed == NULL) {
        ST_ERROR("Failed to st_malloc passed");
        goto ERR;
    }
    memset(args->passed, 0, sizeof(bool) * graph->num_link);

    args->recur_to = (int *)st_malloc(sizeof(int) * graph->num_link);
    if (args->recur_to == NULL) {
        ST_ERROR("Failed to st_malloc recur_to");
        goto ERR;
    }
    for (i = 0; i < graph->num_link; i++) {
        args->recur_to[i] = -1;
    }

    args->node_order = (int *)st_malloc(sizeof(int) * graph->num_node);
    if (args->node_order == NULL) {
        ST_ERROR("Failed to st_malloc node_order");
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
    int *cycle;

    void *tmp;
    st_stack_id_t s;

    int l, lk, llk;
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
            if (st_stack_push(args->link_stack, (void *)(long)lk)
                    != ST_STACK_OK) {
                ST_ERROR("Failed to st_stack_push link[%d].", lk);
                return -1;
            }
#ifdef _GRAPH_DEBUG_
            ST_DEBUG("Link push: %d", lk);
#endif
            if (graph_dfs(graph, to, args) < 0) {
                ST_ERROR("Failed to graph_dfs[%d].", to);
                return -1;
            }

            if (st_stack_pop(args->link_stack, &tmp) != ST_STACK_OK) {
                ST_ERROR("Failed to st_stack_pop link.");
                return -1;
            }
#ifdef _GRAPH_DEBUG_
            ST_DEBUG("Link pop: %d", (int)(long)tmp);
#endif
        } else if(args->on_stack[to]) {
            graph->glues[lk]->recur_type = RECUR_HEAD;
            args->recur_to[lk] = to;
#ifdef _GRAPH_DEBUG_
            ST_DEBUG("recur_to[%d] for link[%d]", to, lk);
#endif
            cycle = (int *)st_malloc(sizeof(int) * (args->link_stack->top + 2));
            if (cycle == NULL) {
                ST_ERROR("Failed to st_malloc cycle.");
                return -1;
            }
            cycle[1] = lk;

            for(s = 1; s <= args->link_stack->top; s++) {
                if (st_stack_topn(args->link_stack, s, &tmp) != ST_STACK_OK) {
                    ST_ERROR("Failed to st_stack_topn link.[%d]", s);
                    return -1;
                }

                llk = (int)(long)tmp;
                if (graph->links[llk].to == to) {
                    break;
                }

                if (graph->glues[llk]->recur_type == RECUR_NON) {
                    graph->glues[llk]->recur_type = RECUR_BODY;
                }
                cycle[s + 1] = llk;
            }
            cycle[0] = s;

            graph->cycles = (int **)st_realloc(graph->cycles,
                    sizeof(int*)*(graph->num_cycle + 1));
            if (graph->cycles == NULL) {
                ST_ERROR("Failed to st_realloc cycles.");
                return -1;
            }
            graph->cycles[graph->num_cycle] = cycle;
            graph->num_cycle++;
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
static int* node2link_order(int *node_order, graph_t *graph, int *recur_to)
{
    int *link_order = NULL;

    elem_set_t *recur_links_for_node = NULL;
    int *recur_link_pos = NULL;

    size_t sz;
    int i, n, l, pos;
    int num_lk, lk;
    node_t *node;

    ST_CHECK_PARAM(node_order == NULL || graph == NULL
            || recur_to == NULL, NULL);

    link_order = (int *)st_malloc(sizeof(int) * graph->num_link);
    if (link_order == NULL) {
        ST_ERROR("Failed to st_malloc link_order");
        goto ERR;
    }

    // find positions for the recur links
    //
    // recur_links_for_node records which the recur link should be placed just
    // before this node. for node n the num of recur links is in
    // recur_links_for_node[n*(1+graph->num_link)], and the following is
    // the links
    sz = sizeof(elem_set_t) * graph->num_node;
    recur_links_for_node = (elem_set_t *)st_malloc(sz);
    if (recur_links_for_node == NULL) {
        ST_ERROR("Failed to st_malloc recur_pos");
        goto ERR;
    }
    memset(recur_links_for_node, 0, sz);
    for (i = 0; i < graph->num_node; i++) {
        if (elem_set_init(recur_links_for_node + i, graph->num_link) < 0) {
            ST_ERROR("Failed to elem_set_init recur_links_for_node.");
            goto ERR;
        }
    }
    for (n = 0; n < graph->num_node; n++) {
        node = graph->nodes + node_order[n];
        for (l = 0; l < node->num_link; l++) {
            lk = node->links[l];
            if (graph->glues[lk]->recur_type != RECUR_HEAD) {
                continue;
            }

            if (recur_to[lk] < 0) {
                ST_ERROR("No recur_to for recur_link[%d]", lk);
                goto ERR;
            }

            /* find the most front dest node. */
            pos = graph->num_node;
            for (i = 0; i < graph->num_node; i++) {
                if (node_order[i] == recur_to[lk]) {
                    pos = i;
                    break;
                }
            }
            if (pos >= graph->num_node) {
                ST_ERROR("dest node[%d] not found in node order",
                        recur_to[lk]);
                goto ERR;
            }

            if (elem_set_add(recur_links_for_node + pos, lk) < 0) {
                ST_ERROR("Failed to elem_set_add for recur_links_for_node.");
                goto ERR;
            }
        }
    }

    // sort the links
    // the recur_link_pos stores the final position of all recur_links
    recur_link_pos = (int *)st_malloc(sizeof(int) * graph->num_link);
    if (recur_link_pos == NULL) {
        ST_ERROR("Failed to st_malloc recur_link_pos.");
        goto ERR;
    }
    for (i = 0; i < graph->num_link; i++) {
        recur_link_pos[i] = -1;
    }

    num_lk = 0;
    for (n = 0; n < graph->num_node; n++) {
        node = graph->nodes + node_order[n];
        if (recur_links_for_node[n].num > 0) {
            // we must place the recur links in recur_links_for_node[n], if any,
            // in front of all out links of this node, including the recur
            // links.
            // So, first, find the most front position of out recur links
            pos = num_lk; // if there is no recur links, this is the current pos
            for (l = 0; l < node->num_link; l++) {
                lk = node->links[l];
                if (graph->glues[lk]->recur_type != RECUR_HEAD) {
                    continue;
                }
                if (graph->links[lk].to == node_order[n]) {
                    // the self-loop can be treated the same as a non-recur link
                    continue;
                }
                if (recur_link_pos[lk] < 0) {
                    ST_ERROR("recur link[%d] did not find its own "
                               "position.", lk);
                    goto ERR;
                }
                if (recur_link_pos[lk] < pos) {
                    pos = recur_link_pos[lk];
                }
            }

            // place the recur links first
            if (num_lk + recur_links_for_node[n].num >= graph->num_link) {
                ST_ERROR("num_lk[%d + %d > %d] overflow.", num_lk,
                        recur_links_for_node[n].num, graph->num_link);
                goto ERR;
            }
            if (pos < num_lk) {
                // move the links after pos
                memmove(link_order + pos + recur_links_for_node[n].num,
                        link_order + pos,
                        sizeof(int) * (num_lk - pos));
                // update the recur_link_pos
                for (i = 0; i < graph->num_link; i++) {
                    if (recur_link_pos[i] > pos) {
                        recur_link_pos[i] += recur_links_for_node[n].num;
                    }
                }
            }
            for (i = 0; i < recur_links_for_node[n].num; i++) {
                lk = recur_links_for_node[n].elems[i];
                link_order[pos] = lk;
                recur_link_pos[lk] = pos;
                pos++;
            }
            num_lk += recur_links_for_node[n].num;
        }

        // now place the non-recur links
        for (l = 0; l < node->num_link; l++) {
            lk = node->links[l];
            if (graph->glues[lk]->recur_type == RECUR_HEAD) {
                continue;
            }

            if (num_lk >= graph->num_link) {
                ST_ERROR("num_lk overflow");
                goto ERR;
            }
            link_order[num_lk] = node->links[l];
            num_lk++;
        }
    }

    if (num_lk != graph->num_link) {
        ST_ERROR("Some links are missing.");
        goto ERR;
    }

    for (i = 0; i < graph->num_node; i++) {
        elem_set_destroy(recur_links_for_node + i);
    }
    safe_st_free(recur_links_for_node);
    safe_st_free(recur_link_pos);

    return link_order;

ERR:
    if (recur_links_for_node != NULL) {
        for (i = 0; i < graph->num_node; i++) {
            elem_set_destroy(recur_links_for_node + i);
        }
        safe_st_free(recur_links_for_node);
    }
    safe_st_free(recur_link_pos);
    safe_st_free(link_order);
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
        ST_ERROR("Failed to dfs_args_init.");
        goto ERR;
    }

    if (graph_dfs(graph, graph->root, &args) < 0) {
        ST_ERROR("Failed to dfs.");
        goto ERR;
    }

    for (n = 0; n < graph->num_node; n++) {
        if (!args.visited[n]) {
            ST_ERROR("Node[%d] not visited.", n);
            goto ERR;
        }
    }

    for (l = 0; l < graph->num_link; l++) {
        if (!args.passed[l]) {
            ST_ERROR("link[%d] not visited.", l);
            goto ERR;
        }
    }

    if (args.node_i != graph->num_node) {
        ST_ERROR("post order not finished.");
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
        ST_ERROR("Failed to node2link_order.");
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
    safe_st_free(link_order);

    return NULL;
}

graph_t* graph_construct(layer_t **layers, int n_layer,
        glue_t **glues, int n_glue)
{
    graph_t *graph = NULL;

    int l;
    int g;

    ST_CHECK_PARAM(layers == NULL || n_layer <= 0, NULL);

    graph = (graph_t *)st_malloc(sizeof(graph_t));
    if (graph == NULL) {
        ST_ERROR("Failed to st_malloc graph_t.");
        goto ERR;
    }
    memset(graph, 0, sizeof(graph_t));

    graph->nodes = (node_t *)st_malloc(sizeof(node_t) * n_layer);
    if (graph->nodes == NULL) {
        ST_ERROR("Failed to st_malloc nodes.");
        goto ERR;
    }
    graph->root = -1;
    for (l = 0; l < n_layer; l++) {
        graph->nodes[l].links = NULL;
        graph->nodes[l].num_link = 0;
        if (strcasecmp(layers[l]->type, INPUT_LAYER_NAME) == 0) {
            if (graph->root != -1) {
                ST_ERROR("Too many input layer.");
                goto ERR;
            }
            graph->root = l;
        }
    }
    graph->num_node = n_layer;

    graph->links = (link_t *)st_malloc(sizeof(link_t) * n_glue);
    if (graph->links == NULL) {
        ST_ERROR("Failed to st_malloc links.");
        goto ERR;
    }
    graph->num_link = n_glue;

    for (g = 0; g < n_glue; g++) {
        graph->links[g].to = glues[g]->out_layer;

        if (node_add_link(graph->nodes + glues[g]->in_layer, g) == -1) {
            ST_ERROR("Failed to node_add_link.");
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
