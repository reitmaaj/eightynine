/* two_tu_b.c - second translation unit using the same public API. */

#include "syntax89.h"

syntax89_status two_tu_build(syntax89_graph *g)
{
    syntax89_id a;
    syntax89_id b;
    syntax89_span span;
    syntax89_status st;

    span.source = 1;
    span.begin = 0;
    span.end = 1;
    a = SYNTAX89_ID_NONE;
    b = SYNTAX89_ID_NONE;
    st = syntax89_init(g, NULL);
    if (st != SYNTAX89_OK)
    {
        return st;
    }
    st = syntax89_add_node(g, 1, span, &a);
    if (st != SYNTAX89_OK)
    {
        return st;
    }
    st = syntax89_add_node(g, 2, span, &b);
    if (st != SYNTAX89_OK)
    {
        return st;
    }
    st = syntax89_add_child(g, a, 1, b);
    if (st != SYNTAX89_OK)
    {
        return st;
    }
    return syntax89_set_root(g, a);
}
