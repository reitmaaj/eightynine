/* header_check.cpp - the public header compiles under C++23 and keeps its
 * C ABI shape. */

#include <cstddef>

#include "syntax89.h"

static_assert(sizeof(syntax89_span) == 3 * sizeof(unsigned long),
              "syntax89_span layout");
static_assert(sizeof(syntax89_node_info) == 4 * sizeof(unsigned long),
              "syntax89_node_info layout");
static_assert(sizeof(syntax89_validation) >=
                  sizeof(syntax89_status) + 2 * sizeof(syntax89_id),
              "syntax89_validation layout");

int main()
{
    syntax89_graph g;
    syntax89_id id;
    syntax89_span span;

    span.source = 1;
    span.begin = 0;
    span.end = 1;
    id = SYNTAX89_ID_NONE;
    if (syntax89_init(&g, nullptr) != SYNTAX89_OK)
    {
        return 1;
    }
    if (syntax89_node_count(&g) != 0)
    {
        return 1;
    }
    if (syntax89_add_node(&g, 1, span, &id) != SYNTAX89_OK)
    {
        return 1;
    }
    syntax89_destroy(&g);
    return 0;
}
