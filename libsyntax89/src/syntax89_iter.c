/* syntax89_iter.c - allocation-free child iteration. */

#include "syntax89_internal.h"

syntax89_status syntax89_children_begin(const syntax89_graph *g,
                                        syntax89_id parent,
                                        syntax89_child_iter *it)
{
    if (g == NULL)
    {
        return SYNTAX89_EINVAL;
    }
    if (it == NULL)
    {
        return SYNTAX89_EINVAL;
    }
    if (g->state == SYNTAX89_STATE_UNINIT)
    {
        return SYNTAX89_ESTATE;
    }
    if (syntax89__node_at(g, parent) == NULL)
    {
        return SYNTAX89_ENODE;
    }
    it->graph = g;
    it->parent = parent;
    it->index = 0;
    return SYNTAX89_OK;
}

syntax89_status syntax89_children_next(syntax89_child_iter *it,
                                       syntax89_role *role, syntax89_id *child)
{
    const struct syntax89_node *n;

    if (it == NULL)
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
    if (it->graph == NULL)
    {
        return SYNTAX89_EINVAL;
    }
    n = syntax89__node_at(it->graph, it->parent);
    if (it->index >= n->edge_count)
    {
        return SYNTAX89_END;
    }
    *role = n->edges[it->index].role;
    *child = n->edges[it->index].child;
    it->index += 1;
    return SYNTAX89_OK;
}
