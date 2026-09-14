/* test_wide.c - very wide child vectors preserve insertion order. */

#include "syntax89_fixtures.h"
#include "syntax89_invariants.h"
#include "syntax89_test.h"

static void run_width(unsigned long width)
{
    syntax89_graph g;
    syntax89_id parent;
    syntax89_id child;
    syntax89_role role;
    syntax89_id got;
    unsigned long i;

    T_OK(syntax89_init(&g, NULL));
    parent = syntax89_fixture_node(&g, K_CALL, 0, 1);
    T_OK(syntax89_set_root(&g, parent));
    for (i = 0; i < width; ++i)
    {
        child = syntax89_fixture_node(&g, K_INT, i, i + 1);
        T_OK(syntax89_add_child(&g, parent, R_ARG, child));
    }
    T_EQ_UL(syntax89_child_count(&g, parent), width);
    T_EQ_UL(syntax89_edge_count(&g), width);
    if (width > 0)
    {
        T_OK(syntax89_child_at(&g, parent, 0, &role, &got));
        T_EQ_UL(role, R_ARG);
        T_EQ_UL(got, 2);
        T_OK(syntax89_child_at(&g, parent, width - 1, &role, &got));
        T_EQ_UL(got, width + 1);
        T_OK(syntax89_child_at(&g, parent, width / 2, &role, &got));
        T_EQ_UL(got, width / 2 + 2);
    }
    T_OK(syntax89_freeze(&g));
    T_EQ_UL(syntax89_child_count(&g, parent), width);
    syntax89_destroy(&g);
}

int main(void)
{
    run_width(0);
    run_width(1);
    run_width(2);
    run_width(16);
    run_width(1000);
    run_width(100000);
    return syntax89_test_report("test_wide");
}
