/* test_iter.c - child iterator contract. */

#include "syntax89_fixtures.h"
#include "syntax89_invariants.h"
#include "syntax89_test.h"

static void test_iteration_matches_child_at(void)
{
    syntax89_graph g;
    syntax89_child_iter it;
    syntax89_role role;
    syntax89_role expect_role;
    syntax89_id child;
    syntax89_id expect_child;
    unsigned long i;
    syntax89_status st;

    syntax89_fixture_build(&g, FX_F2, NULL);
    T_OK(syntax89_children_begin(&g, 1, &it));
    for (i = 0; i < 4; ++i)
    {
        T_OK(syntax89_child_at(&g, 1, i, &expect_role, &expect_child));
        st = syntax89_children_next(&it, &role, &child);
        T_EQ_LONG(st, SYNTAX89_OK);
        T_EQ_UL(role, expect_role);
        T_EQ_UL(child, expect_child);
    }
    st = syntax89_children_next(&it, &role, &child);
    T_EQ_LONG(st, SYNTAX89_END);
    st = syntax89_children_next(&it, &role, &child);
    T_EQ_LONG(st, SYNTAX89_END);
    syntax89_destroy(&g);
}

static void test_empty_child_list(void)
{
    syntax89_graph g;
    syntax89_child_iter it;
    syntax89_role role;
    syntax89_id child;

    syntax89_fixture_build(&g, FX_F2, NULL);
    T_OK(syntax89_children_begin(&g, 2, &it));
    T_EQ_LONG(syntax89_children_next(&it, &role, &child), SYNTAX89_END);
    syntax89_destroy(&g);
}

static void test_iterator_preconditions(void)
{
    syntax89_graph g;
    syntax89_child_iter it;
    syntax89_role role;
    syntax89_id child;

    syntax89_fixture_build(&g, FX_F2, NULL);
    T_EQ_LONG(syntax89_children_begin(NULL, 1, &it), SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_children_begin(&g, 1, NULL), SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_children_begin(&g, 9, &it), SYNTAX89_ENODE);
    T_EQ_LONG(syntax89_children_begin(&g, SYNTAX89_ID_NONE, &it),
              SYNTAX89_ENODE);
    T_EQ_LONG(syntax89_children_next(NULL, &role, &child), SYNTAX89_EINVAL);
    T_OK(syntax89_children_begin(&g, 1, &it));
    T_EQ_LONG(syntax89_children_next(&it, NULL, &child), SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_children_next(&it, &role, NULL), SYNTAX89_EINVAL);
    it.graph = NULL;
    T_EQ_LONG(syntax89_children_next(&it, &role, &child), SYNTAX89_EINVAL);
    syntax89_destroy(&g);
}

static void test_iterator_observes_append(void)
{
    syntax89_graph g;
    syntax89_child_iter it;
    syntax89_role role;
    syntax89_id child;
    syntax89_id a;
    syntax89_id b;
    syntax89_id c;

    T_OK(syntax89_init(&g, NULL));
    a = syntax89_fixture_node(&g, K_NAME, 0, 1);
    b = syntax89_fixture_node(&g, K_NAME, 1, 2);
    c = syntax89_fixture_node(&g, K_NAME, 2, 3);
    T_OK(syntax89_add_child(&g, a, R_ITEM, b));
    T_OK(syntax89_children_begin(&g, a, &it));
    T_OK(syntax89_children_next(&it, &role, &child));
    T_EQ_UL(child, b);
    T_OK(syntax89_add_child(&g, a, R_ITEM, c));
    T_OK(syntax89_children_next(&it, &role, &child));
    T_EQ_UL(child, c);
    T_EQ_LONG(syntax89_children_next(&it, &role, &child), SYNTAX89_END);
    syntax89_destroy(&g);
}

int main(void)
{
    test_iteration_matches_child_at();
    test_empty_child_list();
    test_iterator_preconditions();
    test_iterator_observes_append();
    return syntax89_test_report("test_iter");
}
