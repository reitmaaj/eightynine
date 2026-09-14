/* syntax89_graph.c - graph lifecycle, state, counts, and test hooks. */

#include <stdlib.h>

#include "syntax89_internal.h"

static void *default_alloc(void *ctx, size_t size)
{
    void *p;

    (void)ctx;
    p = malloc(size);
    return p;
}

static void *default_realloc(void *ctx, void *ptr, size_t size)
{
    void *p;

    (void)ctx;
    p = realloc(ptr, size);
    return p;
}

static void default_free(void *ctx, void *ptr)
{
    (void)ctx;
    free(ptr);
}

static void copy_allocator(syntax89_graph *g, const syntax89_allocator *alloc)
{
    g->alloc = *alloc;
}

syntax89_status syntax89_init(syntax89_graph *g,
                              const syntax89_allocator *alloc)
{
    if (g == NULL)
    {
        return SYNTAX89_EINVAL;
    }
    g->alloc.ctx = NULL;
    g->alloc.alloc = default_alloc;
    g->alloc.realloc = default_realloc;
    g->alloc.free = default_free;
    if (alloc != NULL)
    {
        copy_allocator(g, alloc);
    }
    g->nodes = NULL;
    g->node_count = 0;
    g->node_capacity = 0;
    g->edge_count = 0;
    g->root = SYNTAX89_ID_NONE;
    g->state = SYNTAX89_STATE_BUILDING;
    g->test_limit_nodes = 0;
    g->test_limit_edges = 0;
    return SYNTAX89_OK;
}

static void release_node(syntax89_graph *g, struct syntax89_node *n)
{
    syntax89__free(g, n->edges);
    n->edges = NULL;
    n->edge_count = 0;
    n->edge_capacity = 0;
}

void syntax89_destroy(syntax89_graph *g)
{
    unsigned long i;

    if (g == NULL)
    {
        return;
    }
    for (i = 0; i < g->node_count; ++i)
    {
        release_node(g, &g->nodes[i]);
    }
    syntax89__free(g, g->nodes);
    g->nodes = NULL;
    g->node_count = 0;
    g->node_capacity = 0;
    g->edge_count = 0;
    g->root = SYNTAX89_ID_NONE;
    g->state = SYNTAX89_STATE_UNINIT;
    g->test_limit_nodes = 0;
    g->test_limit_edges = 0;
}

int syntax89_is_frozen(const syntax89_graph *g)
{
    if (g == NULL)
    {
        return 0;
    }
    if (g->state != SYNTAX89_STATE_FROZEN)
    {
        return 0;
    }
    return 1;
}

syntax89_id syntax89_root(const syntax89_graph *g)
{
    if (g == NULL)
    {
        return SYNTAX89_ID_NONE;
    }
    return g->root;
}

unsigned long syntax89_node_count(const syntax89_graph *g)
{
    if (g == NULL)
    {
        return 0;
    }
    return g->node_count;
}

unsigned long syntax89_edge_count(const syntax89_graph *g)
{
    if (g == NULL)
    {
        return 0;
    }
    return g->edge_count;
}

void syntax89__set_limit_nodes(syntax89_graph *g, unsigned long limit)
{
    if (g == NULL)
    {
        return;
    }
    g->test_limit_nodes = limit;
}

void syntax89__set_limit_edges(syntax89_graph *g, unsigned long limit)
{
    if (g == NULL)
    {
        return;
    }
    g->test_limit_edges = limit;
}
