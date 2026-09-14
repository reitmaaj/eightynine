/* test_validate_cycles.c - cycle detection C01..C10. */

#include "syntax89_fixtures.h"
#include "syntax89_invariants.h"
#include "syntax89_test.h"

static syntax89_id node(syntax89_graph *g, unsigned long tag)
{
    return syntax89_fixture_node(g, K_NAME, tag, tag + 1);
}

static void test_self_cycle(void)
{
    syntax89_graph g;
    syntax89_validation v;

    syntax89_fixture_build(&g, FX_F6, NULL);
    T_EQ_LONG(syntax89_validate(&g, &v), SYNTAX89_ECYCLE);
    T_EQ_UL(v.node, 1);
    T_EQ_UL(v.related, 1);
    T_EQ_LONG(syntax89_freeze(&g), SYNTAX89_ECYCLE);
    syntax89_destroy(&g);
}

static void test_two_node_cycle(void)
{
    syntax89_graph g;
    syntax89_id a;
    syntax89_id b;
    syntax89_validation v;

    T_OK(syntax89_init(&g, NULL));
    a = node(&g, 0);
    b = node(&g, 1);
    T_OK(syntax89_add_child(&g, a, R_LEFT, b));
    T_OK(syntax89_add_child(&g, b, R_LEFT, a));
    T_OK(syntax89_set_root(&g, a));
    T_EQ_LONG(syntax89_validate(&g, &v), SYNTAX89_ECYCLE);
    T_EQ_UL(v.node, a);
    T_EQ_UL(v.related, b);
    syntax89_destroy(&g);
}

static void test_three_node_cycle(void)
{
    syntax89_graph g;

    syntax89_fixture_build(&g, FX_F7, NULL);
    T_EQ_LONG(syntax89_validate(&g, NULL), SYNTAX89_ECYCLE);
    T_EQ_LONG(syntax89_freeze(&g), SYNTAX89_ECYCLE);
    T_ASSERT(syntax89_is_frozen(&g) == 0);
    syntax89_destroy(&g);
}

static void test_cycle_below_root(void)
{
    syntax89_graph g;
    syntax89_id root;
    syntax89_id a;
    syntax89_id b;

    T_OK(syntax89_init(&g, NULL));
    root = node(&g, 0);
    a = node(&g, 1);
    b = node(&g, 2);
    T_OK(syntax89_add_child(&g, root, R_LEFT, a));
    T_OK(syntax89_add_child(&g, a, R_LEFT, b));
    T_OK(syntax89_add_child(&g, b, R_LEFT, a));
    T_OK(syntax89_set_root(&g, root));
    T_EQ_LONG(syntax89_validate(&g, NULL), SYNTAX89_ECYCLE);
    syntax89_destroy(&g);
}

static void test_diamond_is_acyclic(void)
{
    syntax89_graph g;

    syntax89_fixture_build(&g, FX_F3, NULL);
    T_OK(syntax89_validate(&g, NULL));
    syntax89_destroy(&g);
}

static void test_long_chain(void)
{
    syntax89_graph g;
    syntax89_id prev;
    syntax89_id id;
    unsigned long i;

    T_OK(syntax89_init(&g, NULL));
    prev = node(&g, 0);
    T_OK(syntax89_set_root(&g, prev));
    for (i = 0; i < 1000; ++i)
    {
        id = node(&g, i + 1);
        T_OK(syntax89_add_child(&g, prev, R_LEFT, id));
        prev = id;
    }
    T_OK(syntax89_validate(&g, NULL));
    syntax89_destroy(&g);
}

static void test_long_chain_back_edge_to_root(void)
{
    syntax89_graph g;
    syntax89_id root;
    syntax89_id prev;
    syntax89_id id;
    unsigned long i;

    T_OK(syntax89_init(&g, NULL));
    root = node(&g, 0);
    prev = root;
    for (i = 0; i < 1000; ++i)
    {
        id = node(&g, i + 1);
        T_OK(syntax89_add_child(&g, prev, R_LEFT, id));
        prev = id;
    }
    T_OK(syntax89_add_child(&g, prev, R_LEFT, root));
    T_OK(syntax89_set_root(&g, root));
    T_EQ_LONG(syntax89_validate(&g, NULL), SYNTAX89_ECYCLE);
    syntax89_destroy(&g);
}

static void test_long_chain_back_edge_to_middle(void)
{
    syntax89_graph g;
    syntax89_id root;
    syntax89_id middle;
    syntax89_id prev;
    syntax89_id id;
    unsigned long i;

    T_OK(syntax89_init(&g, NULL));
    root = node(&g, 0);
    middle = node(&g, 1);
    T_OK(syntax89_add_child(&g, root, R_LEFT, middle));
    prev = middle;
    for (i = 0; i < 1000; ++i)
    {
        id = node(&g, i + 2);
        T_OK(syntax89_add_child(&g, prev, R_LEFT, id));
        prev = id;
    }
    T_OK(syntax89_add_child(&g, prev, R_LEFT, middle));
    T_OK(syntax89_set_root(&g, root));
    T_EQ_LONG(syntax89_validate(&g, NULL), SYNTAX89_ECYCLE);
    syntax89_destroy(&g);
}

int main(void)
{
    test_self_cycle();
    test_two_node_cycle();
    test_three_node_cycle();
    test_cycle_below_root();
    test_diamond_is_acyclic();
    test_long_chain();
    test_long_chain_back_edge_to_root();
    test_long_chain_back_edge_to_middle();
    return syntax89_test_report("test_validate_cycles");
}
