/* syntax89_build.c - construction: add_node, add_child, set_root. */

#include "syntax89_internal.h"

syntax89_status syntax89_add_node(syntax89_graph *g, syntax89_kind kind,
                                  syntax89_span span, syntax89_id *out)
{
    struct syntax89_node *n;
    syntax89_status st;
    unsigned long next;

    if (g == NULL)
    {
        return SYNTAX89_EINVAL;
    }
    if (out == NULL)
    {
        return SYNTAX89_EINVAL;
    }
    if (g->state != SYNTAX89_STATE_BUILDING)
    {
        return SYNTAX89_ESTATE;
    }
    if (syntax89__span_ok(span) == 0)
    {
        return SYNTAX89_EINVAL;
    }
    if (g->node_count == ((unsigned long)-1))
    {
        return SYNTAX89_EOVERFLOW;
    }
    if (g->test_limit_nodes != 0)
    {
        if (g->node_count >= g->test_limit_nodes)
        {
            return SYNTAX89_EOVERFLOW;
        }
    }
    next = g->node_count + 1;
    st = syntax89__grow_nodes(g, next);
    if (st != SYNTAX89_OK)
    {
        return st;
    }
    n = &g->nodes[g->node_count];
    n->kind = kind;
    n->span = span;
    n->edges = NULL;
    n->edge_count = 0;
    n->edge_capacity = 0;
    g->node_count = next;
    *out = next;
    return SYNTAX89_OK;
}

syntax89_status syntax89_add_child(syntax89_graph *g, syntax89_id parent,
                                   syntax89_role role, syntax89_id child)
{
    struct syntax89_node *p;
    syntax89_status st;

    if (g == NULL)
    {
        return SYNTAX89_EINVAL;
    }
    if (g->state != SYNTAX89_STATE_BUILDING)
    {
        return SYNTAX89_ESTATE;
    }
    p = syntax89__node_mut(g, parent);
    if (p == NULL)
    {
        return SYNTAX89_ENODE;
    }
    if (syntax89__node_at(g, child) == NULL)
    {
        return SYNTAX89_ENODE;
    }
    if (g->edge_count == ((unsigned long)-1))
    {
        return SYNTAX89_EOVERFLOW;
    }
    if (g->test_limit_edges != 0)
    {
        if (g->edge_count >= g->test_limit_edges)
        {
            return SYNTAX89_EOVERFLOW;
        }
    }
    st = syntax89__grow_edges(g, p, p->edge_count + 1);
    if (st != SYNTAX89_OK)
    {
        return st;
    }
    p->edges[p->edge_count].role = role;
    p->edges[p->edge_count].child = child;
    p->edge_count += 1;
    g->edge_count += 1;
    return SYNTAX89_OK;
}

syntax89_status syntax89_set_root(syntax89_graph *g, syntax89_id root)
{
    if (g == NULL)
    {
        return SYNTAX89_EINVAL;
    }
    if (g->state != SYNTAX89_STATE_BUILDING)
    {
        return SYNTAX89_ESTATE;
    }
    if (root != SYNTAX89_ID_NONE)
    {
        if (syntax89__node_at(g, root) == NULL)
        {
            return SYNTAX89_ENODE;
        }
    }
    g->root = root;
    return SYNTAX89_OK;
}
