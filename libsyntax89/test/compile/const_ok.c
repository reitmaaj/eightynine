/* const_ok.c - const graph pointers are accepted by every query. */

#include "syntax89.h"

int main(void)
{
    const syntax89_graph *g;
    syntax89_node_info info;
    syntax89_role role;
    syntax89_id child;
    syntax89_child_iter it;

    g = NULL;
    if (syntax89_node(g, 1, &info) != SYNTAX89_EINVAL)
    {
        return 1;
    }
    if (syntax89_child_count(g, 1) != 0)
    {
        return 1;
    }
    if (syntax89_child_at(g, 1, 0, &role, &child) != SYNTAX89_EINVAL)
    {
        return 1;
    }
    if (syntax89_children_begin(g, 1, &it) != SYNTAX89_EINVAL)
    {
        return 1;
    }
    if (syntax89_validate(g, NULL) != SYNTAX89_EINVAL)
    {
        return 1;
    }
    return 0;
}
