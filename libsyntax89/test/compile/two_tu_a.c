/* two_tu_a.c - two translation units share the public header and archive. */

#include "syntax89.h"

syntax89_status two_tu_build(syntax89_graph *g);

int main(void)
{
    syntax89_graph g;

    if (two_tu_build(&g) != SYNTAX89_OK)
    {
        return 1;
    }
    if (syntax89_node_count(&g) != 2)
    {
        return 1;
    }
    if (syntax89_edge_count(&g) != 1)
    {
        return 1;
    }
    syntax89_destroy(&g);
    return 0;
}
