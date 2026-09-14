/* syntax89_mem.c - allocator access, checked arithmetic, and store growth. */

#include "syntax89_internal.h"

int syntax89__span_ok(syntax89_span span)
{
    if (span.begin > span.end)
    {
        return 0;
    }
    return 1;
}

int syntax89__size_add(unsigned long a, unsigned long b, unsigned long *out)
{
    unsigned long sum;

    sum = a + b;
    if (sum < a)
    {
        return SYNTAX89_EOVERFLOW;
    }
    *out = sum;
    return SYNTAX89_OK;
}

int syntax89__size_mul(unsigned long a, unsigned long b, unsigned long *out)
{
    if (a != 0)
    {
        if (b > ((unsigned long)-1) / a)
        {
            return SYNTAX89_EOVERFLOW;
        }
    }
    *out = a * b;
    return SYNTAX89_OK;
}

const struct syntax89_node *syntax89__node_at(const syntax89_graph *g,
                                              syntax89_id id)
{
    if (g == NULL)
    {
        return NULL;
    }
    if (id == SYNTAX89_ID_NONE)
    {
        return NULL;
    }
    if (id > g->node_count)
    {
        return NULL;
    }
    return &g->nodes[id - 1];
}

struct syntax89_node *syntax89__node_mut(syntax89_graph *g, syntax89_id id)
{
    if (g == NULL)
    {
        return NULL;
    }
    if (id == SYNTAX89_ID_NONE)
    {
        return NULL;
    }
    if (id > g->node_count)
    {
        return NULL;
    }
    return &g->nodes[id - 1];
}

void *syntax89__alloc(const syntax89_graph *g, unsigned long bytes)
{
    void *p;

    p = g->alloc.alloc(g->alloc.ctx, bytes);
    return p;
}

void *syntax89__realloc(const syntax89_graph *g, void *ptr, unsigned long bytes)
{
    void *p;

    p = g->alloc.realloc(g->alloc.ctx, ptr, bytes);
    return p;
}

void syntax89__free(const syntax89_graph *g, void *ptr)
{
    if (ptr == NULL)
    {
        return;
    }
    g->alloc.free(g->alloc.ctx, ptr);
}

syntax89_status syntax89__grow_nodes(syntax89_graph *g, unsigned long need)
{
    unsigned long cap;
    unsigned long bytes;
    struct syntax89_node *p;
    syntax89_status st;

    if (need <= g->node_capacity)
    {
        return SYNTAX89_OK;
    }
    cap = g->node_capacity;
    if (cap == 0)
    {
        cap = SYNTAX89_NODE_CAP_MIN;
    }
    while (cap < need)
    {
        st = syntax89__size_mul(cap, 2, &cap);
        if (st != SYNTAX89_OK)
        {
            return st;
        }
    }
    st = syntax89__size_mul(cap, sizeof(struct syntax89_node), &bytes);
    if (st != SYNTAX89_OK)
    {
        return st;
    }
    p = syntax89__realloc(g, g->nodes, bytes);
    if (p == NULL)
    {
        return SYNTAX89_ENOMEM;
    }
    g->nodes = p;
    g->node_capacity = cap;
    return SYNTAX89_OK;
}

syntax89_status syntax89__grow_edges(syntax89_graph *g,
                                     struct syntax89_node *parent,
                                     unsigned long need)
{
    unsigned long cap;
    unsigned long bytes;
    struct syntax89_edge *p;
    syntax89_status st;

    if (need <= parent->edge_capacity)
    {
        return SYNTAX89_OK;
    }
    cap = parent->edge_capacity;
    if (cap == 0)
    {
        cap = SYNTAX89_EDGE_CAP_MIN;
    }
    while (cap < need)
    {
        st = syntax89__size_mul(cap, 2, &cap);
        if (st != SYNTAX89_OK)
        {
            return st;
        }
    }
    st = syntax89__size_mul(cap, sizeof(struct syntax89_edge), &bytes);
    if (st != SYNTAX89_OK)
    {
        return st;
    }
    p = syntax89__realloc(g, parent->edges, bytes);
    if (p == NULL)
    {
        return SYNTAX89_ENOMEM;
    }
    parent->edges = p;
    parent->edge_capacity = cap;
    return SYNTAX89_OK;
}

static syntax89_status scratch_alloc_color(const syntax89_graph *g,
                                           struct syntax89_scratch *s,
                                           unsigned long bytes)
{
    s->color = syntax89__alloc(g, bytes);
    if (s->color == NULL)
    {
        return SYNTAX89_ENOMEM;
    }
    return SYNTAX89_OK;
}

static syntax89_status scratch_alloc_stack(const syntax89_graph *g,
                                           struct syntax89_scratch *s,
                                           unsigned long bytes)
{
    s->stack = syntax89__alloc(g, bytes);
    if (s->stack != NULL)
    {
        return SYNTAX89_OK;
    }
    syntax89__free(g, s->color);
    s->color = NULL;
    return SYNTAX89_ENOMEM;
}

syntax89_status syntax89__scratch_new(const syntax89_graph *g, unsigned long n,
                                      struct syntax89_scratch *s)
{
    unsigned long frame_bytes;
    syntax89_status st;

    s->color = NULL;
    s->stack = NULL;

    /* A node store of n nodes already allocated at least n * sizeof(node)
     * bytes, and a frame is smaller than a node, so neither product can
     * overflow here. */
    frame_bytes = n * sizeof(struct syntax89_frame);
    st = scratch_alloc_color(g, s, n);
    if (st != SYNTAX89_OK)
    {
        return st;
    }
    st = scratch_alloc_stack(g, s, frame_bytes);
    return st;
}

void syntax89__scratch_free(const syntax89_graph *g, struct syntax89_scratch *s)
{
    syntax89__free(g, s->color);
    syntax89__free(g, s->stack);
    s->color = NULL;
    s->stack = NULL;
}
