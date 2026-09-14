/* test_root.c - root semantics R01..R09. */

#include "syntax89_fixtures.h"
#include "syntax89_invariants.h"
#include "syntax89_test.h"

static void test_root_initially_none(void)
{
    syntax89_graph g;

    T_OK(syntax89_init(&g, NULL));
    T_EQ_UL(syntax89_root(&g), SYNTAX89_ID_NONE);
    syntax89_destroy(&g);
}

static void test_set_and_change_root(void)
{
    syntax89_graph g;
    syntax89_id a;
    syntax89_id b;

    T_OK(syntax89_init(&g, NULL));
    a = syntax89_fixture_node(&g, K_NAME, 0, 1);
    b = syntax89_fixture_node(&g, K_NAME, 1, 2);
    T_OK(syntax89_set_root(&g, a));
    T_EQ_UL(syntax89_root(&g), a);
    T_OK(syntax89_set_root(&g, b));
    T_EQ_UL(syntax89_root(&g), b);
    T_OK(syntax89_set_root(&g, SYNTAX89_ID_NONE));
    T_EQ_UL(syntax89_root(&g), SYNTAX89_ID_NONE);
    syntax89_destroy(&g);
}

static void test_unknown_root(void)
{
    syntax89_graph g;
    syntax89_id a;

    T_OK(syntax89_init(&g, NULL));
    a = syntax89_fixture_node(&g, K_NAME, 0, 1);
    T_EQ_LONG(syntax89_set_root(&g, 99), SYNTAX89_ENODE);
    T_EQ_LONG(syntax89_set_root(&g, SYNTAX89_ID_NONE), SYNTAX89_OK);
    T_EQ_LONG(syntax89_set_root(NULL, a), SYNTAX89_EINVAL);
    syntax89_destroy(&g);
}

static void test_freeze_without_root(void)
{
    syntax89_graph g;
    struct syntax89_test_snapshot *before;
    syntax89_validation v;

    syntax89_fixture_build(&g, FX_F1, NULL);
    T_OK(syntax89_set_root(&g, SYNTAX89_ID_NONE));
    T_ASSERT(syntax89_test_snapshot_take(&g, &before) == 0);
    T_EQ_LONG(syntax89_freeze(&g), SYNTAX89_EGRAPH);
    T_EQ_LONG(syntax89_validate(&g, &v), SYNTAX89_EGRAPH);
    T_EQ_LONG(v.error, SYNTAX89_EGRAPH);
    T_EQ_UL(v.node, SYNTAX89_ID_NONE);
    T_ASSERT(syntax89_is_frozen(&g) == 0);
    T_ASSERT(syntax89_test_snapshot_equal(before, &g) != 0);
    syntax89_test_snapshot_free(before);
    syntax89_destroy(&g);
}

static void test_root_with_incoming_edge(void)
{
    syntax89_graph g;
    syntax89_id a;
    syntax89_id b;
    syntax89_validation v;

    T_OK(syntax89_init(&g, NULL));
    a = syntax89_fixture_node(&g, K_NAME, 0, 1);
    b = syntax89_fixture_node(&g, K_NAME, 1, 2);
    T_OK(syntax89_add_child(&g, a, R_LEFT, b));
    T_OK(syntax89_add_child(&g, b, R_LEFT, a));
    T_OK(syntax89_set_root(&g, a));
    T_EQ_LONG(syntax89_validate(&g, &v), SYNTAX89_ECYCLE);
    T_EQ_UL(v.node, a);
    T_EQ_UL(v.related, b);
    T_EQ_LONG(syntax89_freeze(&g), SYNTAX89_ECYCLE);
    T_ASSERT(syntax89_is_frozen(&g) == 0);
    syntax89_destroy(&g);
}

static void test_root_stable_after_freeze(void)
{
    syntax89_graph g;
    syntax89_id root;

    root = syntax89_fixture_build(&g, FX_F4, NULL);
    T_OK(syntax89_freeze(&g));
    T_EQ_UL(syntax89_root(&g), root);
    T_EQ_LONG(syntax89_set_root(&g, 2), SYNTAX89_ESTATE);
    T_EQ_UL(syntax89_root(&g), root);
    syntax89_destroy(&g);
}

int main(void)
{
    test_root_initially_none();
    test_set_and_change_root();
    test_unknown_root();
    test_freeze_without_root();
    test_root_with_incoming_edge();
    test_root_stable_after_freeze();
    return syntax89_test_report("test_root");
}
