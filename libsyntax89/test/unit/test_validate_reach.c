/* test_validate_reach.c - reachability U01..U06. */

#include "syntax89_fixtures.h"
#include "syntax89_invariants.h"
#include "syntax89_test.h"

static syntax89_id node(syntax89_graph *g, unsigned long tag)
{
    return syntax89_fixture_node(g, K_NAME, tag, tag + 1);
}

static void test_single_root(void)
{
    syntax89_graph g;

    syntax89_fixture_build(&g, FX_F0, NULL);
    T_OK(syntax89_validate(&g, NULL));
    T_OK(syntax89_freeze(&g));
    syntax89_destroy(&g);
}

static void test_one_unreachable_node(void)
{
    syntax89_graph g;
    syntax89_validation v;

    syntax89_fixture_build(&g, FX_F5, NULL);
    T_EQ_LONG(syntax89_validate(&g, &v), SYNTAX89_EGRAPH);
    T_EQ_LONG(v.error, SYNTAX89_EGRAPH);
    T_EQ_UL(v.node, 3);
    T_EQ_UL(v.related, SYNTAX89_ID_NONE);
    T_EQ_LONG(syntax89_freeze(&g), SYNTAX89_EGRAPH);
    syntax89_destroy(&g);
}

static void test_unreachable_subtree(void)
{
    syntax89_graph g;
    syntax89_id root;
    syntax89_id a;
    syntax89_id b;
    syntax89_id c;
    syntax89_validation v;

    T_OK(syntax89_init(&g, NULL));
    root = node(&g, 0);
    a = node(&g, 1);
    b = node(&g, 2);
    c = node(&g, 3);
    T_OK(syntax89_add_child(&g, root, R_LEFT, a));
    T_OK(syntax89_add_child(&g, b, R_LEFT, c));
    T_OK(syntax89_set_root(&g, root));
    T_EQ_LONG(syntax89_validate(&g, &v), SYNTAX89_EGRAPH);
    T_EQ_UL(v.node, b);
    syntax89_destroy(&g);
}

static void test_disconnected_dag(void)
{
    syntax89_graph g;
    syntax89_id a;
    syntax89_id b;
    syntax89_id c;
    syntax89_id d;

    T_OK(syntax89_init(&g, NULL));
    a = node(&g, 0);
    b = node(&g, 1);
    c = node(&g, 2);
    d = node(&g, 3);
    T_OK(syntax89_add_child(&g, a, R_LEFT, b));
    T_OK(syntax89_add_child(&g, c, R_LEFT, d));
    T_OK(syntax89_set_root(&g, a));
    T_EQ_LONG(syntax89_validate(&g, NULL), SYNTAX89_EGRAPH);
    syntax89_destroy(&g);
}

static void test_shared_reachable(void)
{
    syntax89_graph g;

    syntax89_fixture_build(&g, FX_F3, NULL);
    T_OK(syntax89_validate(&g, NULL));
    syntax89_destroy(&g);
}

static void test_repeated_roles_reachable(void)
{
    syntax89_graph g;

    syntax89_fixture_build(&g, FX_F4, NULL);
    T_OK(syntax89_validate(&g, NULL));
    syntax89_destroy(&g);
}

int main(void)
{
    test_single_root();
    test_one_unreachable_node();
    test_unreachable_subtree();
    test_disconnected_dag();
    test_shared_reachable();
    test_repeated_roles_reachable();
    return syntax89_test_report("test_validate_reach");
}
