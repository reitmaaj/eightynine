/* test_sharing.c - DAG sharing D01..D06. */

#include "syntax89_fixtures.h"
#include "syntax89_invariants.h"
#include "syntax89_test.h"

static void test_shared_leaf(void)
{
    syntax89_graph g;
    syntax89_role role;
    syntax89_id left_child;
    syntax89_id right_child;

    syntax89_fixture_build(&g, FX_F3, NULL);
    T_EQ_UL(syntax89_node_count(&g), 2);
    T_EQ_UL(syntax89_edge_count(&g), 2);
    T_OK(syntax89_child_at(&g, 1, 0, &role, &left_child));
    T_EQ_UL(role, R_LEFT);
    T_OK(syntax89_child_at(&g, 1, 1, &role, &right_child));
    T_EQ_UL(role, R_RIGHT);
    T_EQ_UL(left_child, right_child);
    T_OK(syntax89_freeze(&g));
    T_ASSERT(syntax89_is_frozen(&g) != 0);
    syntax89_destroy(&g);
}

static void test_diamond(void)
{
    syntax89_graph g;
    syntax89_id a;
    syntax89_id b;
    syntax89_id c;
    syntax89_id d;
    syntax89_id child;
    syntax89_role role;
    unsigned long incoming;

    T_OK(syntax89_init(&g, NULL));
    a = syntax89_fixture_node(&g, K_ADD, 0, 1);
    b = syntax89_fixture_node(&g, K_NAME, 1, 2);
    c = syntax89_fixture_node(&g, K_NAME, 2, 3);
    d = syntax89_fixture_node(&g, K_NAME, 3, 4);
    T_OK(syntax89_add_child(&g, a, R_LEFT, b));
    T_OK(syntax89_add_child(&g, a, R_RIGHT, c));
    T_OK(syntax89_add_child(&g, b, R_LEFT, d));
    T_OK(syntax89_add_child(&g, c, R_LEFT, d));
    T_OK(syntax89_set_root(&g, a));
    T_EQ_UL(syntax89_node_count(&g), 4);
    T_EQ_UL(syntax89_edge_count(&g), 4);
    incoming = 0;
    T_OK(syntax89_child_at(&g, b, 0, &role, &child));
    T_EQ_UL(child, d);
    incoming += 1;
    T_OK(syntax89_child_at(&g, c, 0, &role, &child));
    T_EQ_UL(child, d);
    incoming += 1;
    T_EQ_UL(incoming, 2);
    syntax89_test_check(&g);
    T_OK(syntax89_freeze(&g));
    syntax89_test_check(&g);
    syntax89_destroy(&g);
}

static void test_duplicate_edge_is_not_a_cycle(void)
{
    syntax89_graph g;
    syntax89_id a;
    syntax89_id b;

    T_OK(syntax89_init(&g, NULL));
    a = syntax89_fixture_node(&g, K_ADD, 0, 1);
    b = syntax89_fixture_node(&g, K_NAME, 1, 2);
    T_OK(syntax89_add_child(&g, a, R_LEFT, b));
    T_OK(syntax89_add_child(&g, a, R_LEFT, b));
    T_OK(syntax89_set_root(&g, a));
    T_OK(syntax89_freeze(&g));
    T_EQ_UL(syntax89_edge_count(&g), 2);
    syntax89_destroy(&g);
}

int main(void)
{
    test_shared_leaf();
    test_diamond();
    test_duplicate_edge_is_not_a_cycle();
    return syntax89_test_report("test_sharing");
}
