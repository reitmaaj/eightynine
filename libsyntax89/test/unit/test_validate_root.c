/* test_validate_root.c - root/indegree invariants I01..I03. */

#include "syntax89_fixtures.h"
#include "syntax89_invariants.h"
#include "syntax89_test.h"

static void test_root_without_incoming(void)
{
    syntax89_graph g;

    syntax89_fixture_build(&g, FX_F4, NULL);
    T_OK(syntax89_validate(&g, NULL));
    T_OK(syntax89_freeze(&g));
    syntax89_destroy(&g);
}

static void test_incoming_root_edge_is_cycle(void)
{
    syntax89_graph g;
    syntax89_id root;
    syntax89_id a;
    syntax89_validation v;

    T_OK(syntax89_init(&g, NULL));
    root = syntax89_fixture_node(&g, K_NAME, 0, 1);
    a = syntax89_fixture_node(&g, K_NAME, 1, 2);
    T_OK(syntax89_add_child(&g, root, R_LEFT, a));
    T_OK(syntax89_add_child(&g, a, R_LEFT, root));
    T_OK(syntax89_set_root(&g, root));
    T_EQ_LONG(syntax89_validate(&g, &v), SYNTAX89_ECYCLE);
    T_EQ_UL(v.node, root);
    T_EQ_UL(v.related, a);
    syntax89_destroy(&g);
}

static void test_root_self_loop(void)
{
    syntax89_graph g;

    syntax89_fixture_build(&g, FX_F6, NULL);
    T_EQ_LONG(syntax89_validate(&g, NULL), SYNTAX89_ECYCLE);
    syntax89_destroy(&g);
}

int main(void)
{
    test_root_without_incoming();
    test_incoming_root_edge_is_cycle();
    test_root_self_loop();
    return syntax89_test_report("test_validate_root");
}
