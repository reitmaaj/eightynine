/* ref_syntax89.c - independent reference model and validation oracle. */

#include "ref_syntax89.h"

void ref_init(struct ref_syntax89 *r)
{
    r->node_count = 0;
    r->edge_count = 0;
    r->root = 0;
}

unsigned long ref_add_node(struct ref_syntax89 *r, syntax89_kind kind,
                           syntax89_span span)
{
    if (r->node_count >= REF_MAX_NODES)
    {
        return 0;
    }
    r->node_count += 1;
    r->kinds[r->node_count - 1] = kind;
    r->spans[r->node_count - 1] = span;
    return r->node_count;
}

int ref_add_child(struct ref_syntax89 *r, unsigned long parent,
                  syntax89_role role, unsigned long child)
{
    if (parent == 0)
    {
        return -1;
    }
    if (parent > r->node_count)
    {
        return -1;
    }
    if (child == 0)
    {
        return -1;
    }
    if (child > r->node_count)
    {
        return -1;
    }
    if (r->edge_count >= REF_MAX_EDGES)
    {
        return -1;
    }
    r->roles[r->edge_count] = role;
    r->from[r->edge_count] = parent;
    r->to[r->edge_count] = child;
    r->edge_count += 1;
    return 0;
}

void ref_set_root(struct ref_syntax89 *r, unsigned long root)
{
    r->root = root;
}

static int ref_dfs(const struct ref_syntax89 *r, unsigned long node,
                   unsigned char *color)
{
    unsigned long i;

    if (color[node - 1] == 1)
    {
        return 0;
    }
    if (color[node - 1] == 2)
    {
        return 1;
    }
    color[node - 1] = 1;
    for (i = 0; i < r->edge_count; ++i)
    {
        if (r->from[i] == node)
        {
            if (ref_dfs(r, r->to[i], color) == 0)
            {
                return 0;
            }
        }
    }
    color[node - 1] = 2;
    return 1;
}

syntax89_status ref_validate(const struct ref_syntax89 *r)
{
    unsigned char color[REF_MAX_NODES];
    unsigned long i;
    unsigned long visited;

    if (r->root == 0)
    {
        return SYNTAX89_EGRAPH;
    }
    if (r->root > r->node_count)
    {
        return SYNTAX89_EGRAPH;
    }
    for (i = 0; i < r->node_count; ++i)
    {
        color[i] = 0;
    }
    if (ref_dfs(r, r->root, color) == 0)
    {
        return SYNTAX89_ECYCLE;
    }
    visited = 0;
    for (i = 0; i < r->node_count; ++i)
    {
        if (color[i] == 2)
        {
            visited += 1;
        }
    }
    if (visited != r->node_count)
    {
        return SYNTAX89_EGRAPH;
    }
    return SYNTAX89_OK;
}

int ref_equals_graph(const struct ref_syntax89 *r, const syntax89_graph *g)
{
    syntax89_node_info info;
    syntax89_role role;
    syntax89_id child;
    unsigned long i;
    unsigned long j;
    unsigned long k;

    if (syntax89_node_count(g) != r->node_count)
    {
        return 0;
    }
    if (syntax89_edge_count(g) != r->edge_count)
    {
        return 0;
    }
    if (syntax89_root(g) != r->root)
    {
        return 0;
    }
    for (i = 1; i <= r->node_count; ++i)
    {
        if (syntax89_node(g, i, &info) != SYNTAX89_OK)
        {
            return 0;
        }
        if (info.kind != r->kinds[i - 1])
        {
            return 0;
        }
        if (info.span.source != r->spans[i - 1].source)
        {
            return 0;
        }
        if (info.span.begin != r->spans[i - 1].begin)
        {
            return 0;
        }
        if (info.span.end != r->spans[i - 1].end)
        {
            return 0;
        }
    }
    for (i = 1; i <= r->node_count; ++i)
    {
        j = 0;
        for (k = 0; k < r->edge_count; ++k)
        {
            if (r->from[k] != i)
            {
                continue;
            }
            if (syntax89_child_at(g, i, j, &role, &child) != SYNTAX89_OK)
            {
                return 0;
            }
            if (role != r->roles[k])
            {
                return 0;
            }
            if (child != r->to[k])
            {
                return 0;
            }
            j += 1;
        }
    }
    return 1;
}
