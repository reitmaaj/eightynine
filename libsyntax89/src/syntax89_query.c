/* syntax89_query.c - node and child inspection. */

#include "syntax89_internal.h"

static unsigned long role_count_step(unsigned long count)
{
    return count + 1;
}

static unsigned long role_count(const struct syntax89_node *n,
                                syntax89_role role)
{
    unsigned long i;
    unsigned long count;

    count = 0;
    for (i = 0; i < n->edge_count; ++i)
    {
        if (n->edges[i].role == role)
        {
            count = role_count_step(count);
        }
    }
    return count;
}

static const struct syntax89_edge *role_edge_at(const struct syntax89_node *n,
                                                syntax89_role role,
                                                unsigned long index)
{
    unsigned long i;
    unsigned long seen;

    seen = 0;
    for (i = 0; i < n->edge_count; ++i)
    {
        if (n->edges[i].role == role)
        {
            if (seen == index)
            {
                return &n->edges[i];
            }
            seen = role_count_step(seen);
        }
    }
    return NULL;
}

syntax89_status syntax89_node(const syntax89_graph *g, syntax89_id id,
                              syntax89_node_info *out)
{
    const struct syntax89_node *n;

    if (g == NULL)
    {
        return SYNTAX89_EINVAL;
    }
    if (out == NULL)
    {
        return SYNTAX89_EINVAL;
    }
    if (g->state == SYNTAX89_STATE_UNINIT)
    {
        return SYNTAX89_ESTATE;
    }
    n = syntax89__node_at(g, id);
    if (n == NULL)
    {
        return SYNTAX89_ENODE;
    }
    out->kind = n->kind;
    out->span = n->span;
    return SYNTAX89_OK;
}

unsigned long syntax89_child_count(const syntax89_graph *g, syntax89_id parent)
{
    const struct syntax89_node *n;

    if (g == NULL)
    {
        return 0;
    }
    if (g->state == SYNTAX89_STATE_UNINIT)
    {
        return 0;
    }
    n = syntax89__node_at(g, parent);
    if (n == NULL)
    {
        return 0;
    }
    return n->edge_count;
}

syntax89_status syntax89_child_at(const syntax89_graph *g, syntax89_id parent,
                                  unsigned long index, syntax89_role *role,
                                  syntax89_id *child)
{
    const struct syntax89_node *n;

    if (g == NULL)
    {
        return SYNTAX89_EINVAL;
    }
    if (role == NULL)
    {
        return SYNTAX89_EINVAL;
    }
    if (child == NULL)
    {
        return SYNTAX89_EINVAL;
    }
    if (g->state == SYNTAX89_STATE_UNINIT)
    {
        return SYNTAX89_ESTATE;
    }
    n = syntax89__node_at(g, parent);
    if (n == NULL)
    {
        return SYNTAX89_ENODE;
    }
    if (index >= n->edge_count)
    {
        return SYNTAX89_EINVAL;
    }
    *role = n->edges[index].role;
    *child = n->edges[index].child;
    return SYNTAX89_OK;
}

unsigned long syntax89_child_count_role(const syntax89_graph *g,
                                        syntax89_id parent, syntax89_role role)
{
    const struct syntax89_node *n;

    if (g == NULL)
    {
        return 0;
    }
    if (g->state == SYNTAX89_STATE_UNINIT)
    {
        return 0;
    }
    n = syntax89__node_at(g, parent);
    if (n == NULL)
    {
        return 0;
    }
    return role_count(n, role);
}

syntax89_status syntax89_child_at_role(const syntax89_graph *g,
                                       syntax89_id parent, syntax89_role role,
                                       unsigned long index, syntax89_id *child)
{
    const struct syntax89_node *n;
    const struct syntax89_edge *edge;

    if (g == NULL)
    {
        return SYNTAX89_EINVAL;
    }
    if (child == NULL)
    {
        return SYNTAX89_EINVAL;
    }
    if (g->state == SYNTAX89_STATE_UNINIT)
    {
        return SYNTAX89_ESTATE;
    }
    n = syntax89__node_at(g, parent);
    if (n == NULL)
    {
        return SYNTAX89_ENODE;
    }
    edge = role_edge_at(n, role, index);
    if (edge == NULL)
    {
        return SYNTAX89_EINVAL;
    }
    *child = edge->child;
    return SYNTAX89_OK;
}
