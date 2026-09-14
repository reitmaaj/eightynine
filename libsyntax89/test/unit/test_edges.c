/* test_edges.c - edge insertion E01..E10. */

#include "syntax89_fixtures.h"
#include "syntax89_invariants.h"
#include "syntax89_test.h"

static void test_first_and_multiple_edges(void)
{
    syntax89_graph g;
    syntax89_id a;
    syntax89_id b;
    syntax89_id c;

    T_OK(syntax89_init(&g, NULL));
    a = syntax89_fixture_node(&g, K_NAME, 0, 1);
    b = syntax89_fixture_node(&g, K_NAME, 1, 2);
    c = syntax89_fixture_node(&g, K_NAME, 2, 3);
    T_OK(syntax89_add_child(&g, a, R_LEFT, b));
    T_EQ_UL(syntax89_edge_count(&g), 1);
    T_EQ_UL(syntax89_child_count(&g, a), 1);
    T_OK(syntax89_add_child(&g, a, R_RIGHT, c));
    T_EQ_UL(syntax89_edge_count(&g), 2);
    T_EQ_UL(syntax89_child_count(&g, a), 2);
    syntax89_test_check(&g);
    syntax89_destroy(&g);
}

static void test_duplicate_edge_allowed(void)
{
    syntax89_graph g;
    syntax89_id a;
    syntax89_id b;

    T_OK(syntax89_init(&g, NULL));
    a = syntax89_fixture_node(&g, K_NAME, 0, 1);
    b = syntax89_fixture_node(&g, K_NAME, 1, 2);
    T_OK(syntax89_add_child(&g, a, R_LEFT, b));
    T_OK(syntax89_add_child(&g, a, R_LEFT, b));
    T_EQ_UL(syntax89_edge_count(&g), 2);
    T_EQ_UL(syntax89_child_count(&g, a), 2);
    T_EQ_UL(syntax89_child_count_role(&g, a, R_LEFT), 2);
    syntax89_test_check(&g);
    syntax89_destroy(&g);
}

static void test_same_role_different_child(void)
{
    syntax89_graph g;
    syntax89_id a;
    syntax89_id b;
    syntax89_id c;
    syntax89_role role;
    syntax89_id child;

    T_OK(syntax89_init(&g, NULL));
    a = syntax89_fixture_node(&g, K_NAME, 0, 1);
    b = syntax89_fixture_node(&g, K_NAME, 1, 2);
    c = syntax89_fixture_node(&g, K_NAME, 2, 3);
    T_OK(syntax89_add_child(&g, a, R_ARG, b));
    T_OK(syntax89_add_child(&g, a, R_ARG, c));
    T_OK(syntax89_child_at(&g, a, 0, &role, &child));
    T_EQ_UL(role, R_ARG);
    T_EQ_UL(child, b);
    T_OK(syntax89_child_at(&g, a, 1, &role, &child));
    T_EQ_UL(role, R_ARG);
    T_EQ_UL(child, c);
    syntax89_destroy(&g);
}

static void test_same_child_different_role(void)
{
    syntax89_graph g;
    syntax89_id a;
    syntax89_id b;
    syntax89_role role;
    syntax89_id child;

    T_OK(syntax89_init(&g, NULL));
    a = syntax89_fixture_node(&g, K_NAME, 0, 1);
    b = syntax89_fixture_node(&g, K_NAME, 1, 2);
    T_OK(syntax89_add_child(&g, a, R_LEFT, b));
    T_OK(syntax89_add_child(&g, a, R_RIGHT, b));
    T_OK(syntax89_child_at(&g, a, 0, &role, &child));
    T_EQ_UL(role, R_LEFT);
    T_EQ_UL(child, b);
    T_OK(syntax89_child_at(&g, a, 1, &role, &child));
    T_EQ_UL(role, R_RIGHT);
    T_EQ_UL(child, b);
    syntax89_destroy(&g);
}

static void test_unknown_endpoints(void)
{
    syntax89_graph g;
    syntax89_id a;
    struct syntax89_test_snapshot *before;

    T_OK(syntax89_init(&g, NULL));
    a = syntax89_fixture_node(&g, K_NAME, 0, 1);
    T_ASSERT(syntax89_test_snapshot_take(&g, &before) == 0);
    T_EQ_LONG(syntax89_add_child(&g, a, R_LEFT, 99), SYNTAX89_ENODE);
    T_EQ_LONG(syntax89_add_child(&g, a, R_LEFT, SYNTAX89_ID_NONE),
              SYNTAX89_ENODE);
    T_EQ_LONG(syntax89_add_child(&g, 99, R_LEFT, a), SYNTAX89_ENODE);
    T_EQ_LONG(syntax89_add_child(&g, SYNTAX89_ID_NONE, R_LEFT, a),
              SYNTAX89_ENODE);
    T_EQ_LONG(syntax89_add_child(NULL, a, R_LEFT, a), SYNTAX89_EINVAL);
    T_ASSERT(syntax89_test_snapshot_equal(before, &g) != 0);
    syntax89_test_snapshot_free(before);
    syntax89_destroy(&g);
}

static void test_cycles_allowed_while_building(void)
{
    syntax89_graph g;
    syntax89_id a;
    syntax89_id b;
    syntax89_id c;

    T_OK(syntax89_init(&g, NULL));
    a = syntax89_fixture_node(&g, K_NAME, 0, 1);
    b = syntax89_fixture_node(&g, K_NAME, 1, 2);
    c = syntax89_fixture_node(&g, K_NAME, 2, 3);
    T_OK(syntax89_add_child(&g, a, R_LEFT, a));
    T_OK(syntax89_add_child(&g, b, R_LEFT, c));
    T_OK(syntax89_add_child(&g, c, R_LEFT, b));
    T_EQ_UL(syntax89_edge_count(&g), 3);
    syntax89_test_check(&g);
    syntax89_destroy(&g);
}

int main(void)
{
    test_first_and_multiple_edges();
    test_duplicate_edge_allowed();
    test_same_role_different_child();
    test_same_child_different_role();
    test_unknown_endpoints();
    test_cycles_allowed_while_building();
    return syntax89_test_report("test_edges");
}
