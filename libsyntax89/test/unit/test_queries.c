/* test_queries.c - node and child query contract Q01..Q17. */

#include <limits.h>

#include "syntax89_fixtures.h"
#include "syntax89_invariants.h"
#include "syntax89_test.h"

static void test_node_queries(void)
{
    syntax89_graph g;
    syntax89_node_info info;
    syntax89_node_info keep;

    syntax89_fixture_build(&g, FX_F1, NULL);
    keep.kind = 77;
    keep.span.source = 77;
    keep.span.begin = 77;
    keep.span.end = 77;
    T_OK(syntax89_node(&g, 1, &info));
    T_EQ_UL(info.kind, K_ADD);
    T_EQ_LONG(syntax89_node(&g, SYNTAX89_ID_NONE, &info), SYNTAX89_ENODE);
    T_EQ_LONG(syntax89_node(&g, 4, &info), SYNTAX89_ENODE);
    T_EQ_LONG(syntax89_node(&g, ULONG_MAX, &info), SYNTAX89_ENODE);
    info = keep;
    T_EQ_LONG(syntax89_node(&g, 4, &info), SYNTAX89_ENODE);
    T_EQ_UL(info.kind, 77);
    T_EQ_LONG(syntax89_node(NULL, 1, &info), SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_node(&g, 1, NULL), SYNTAX89_EINVAL);
    syntax89_destroy(&g);
}

static void test_child_at_queries(void)
{
    syntax89_graph g;
    syntax89_role role;
    syntax89_id child;
    syntax89_role keep_role;
    syntax89_id keep_child;

    syntax89_fixture_build(&g, FX_F2, NULL);
    T_OK(syntax89_child_at(&g, 1, 0, &role, &child));
    T_EQ_UL(role, R_CALLEE);
    T_EQ_UL(child, 2);
    T_OK(syntax89_child_at(&g, 1, 3, &role, &child));
    T_EQ_UL(role, R_ARG);
    T_EQ_UL(child, 5);
    T_EQ_LONG(syntax89_child_at(&g, 1, 4, &role, &child), SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_child_at(&g, 1, ULONG_MAX, &role, &child),
              SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_child_at(&g, 2, 0, &role, &child), SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_child_at(&g, 9, 0, &role, &child), SYNTAX89_ENODE);
    T_EQ_LONG(syntax89_child_at(&g, SYNTAX89_ID_NONE, 0, &role, &child),
              SYNTAX89_ENODE);
    T_EQ_LONG(syntax89_child_at(NULL, 1, 0, &role, &child), SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_child_at(&g, 1, 0, NULL, &child), SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_child_at(&g, 1, 0, &role, NULL), SYNTAX89_EINVAL);
    keep_role = 5;
    keep_child = 6;
    T_EQ_LONG(syntax89_child_at(&g, 1, 4, &keep_role, &keep_child),
              SYNTAX89_EINVAL);
    T_EQ_UL(keep_role, 5);
    T_EQ_UL(keep_child, 6);
    syntax89_destroy(&g);
}

static void test_role_queries(void)
{
    syntax89_graph g;
    syntax89_id child;

    syntax89_fixture_build(&g, FX_F2, NULL);
    T_EQ_UL(syntax89_child_count_role(&g, 1, R_ARG), 3);
    T_EQ_UL(syntax89_child_count_role(&g, 1, R_CALLEE), 1);
    T_EQ_UL(syntax89_child_count_role(&g, 1, R_LEFT), 0);
    T_EQ_UL(syntax89_child_count_role(&g, 9, R_ARG), 0);
    T_EQ_UL(syntax89_child_count_role(NULL, 1, R_ARG), 0);
    T_EQ_UL(syntax89_child_count(&g, 9), 0);
    T_EQ_UL(syntax89_child_count(&g, SYNTAX89_ID_NONE), 0);
    T_OK(syntax89_child_at_role(&g, 1, R_ARG, 0, &child));
    T_EQ_UL(child, 3);
    T_OK(syntax89_child_at_role(&g, 1, R_ARG, 1, &child));
    T_EQ_UL(child, 4);
    T_OK(syntax89_child_at_role(&g, 1, R_ARG, 2, &child));
    T_EQ_UL(child, 5);
    T_EQ_LONG(syntax89_child_at_role(&g, 1, R_ARG, 3, &child), SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_child_at_role(&g, 9, R_ARG, 0, &child), SYNTAX89_ENODE);
    T_EQ_LONG(syntax89_child_at_role(NULL, 1, R_ARG, 0, &child),
              SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_child_at_role(&g, 1, R_ARG, 0, NULL), SYNTAX89_EINVAL);
    syntax89_destroy(&g);
}

static void test_null_counts(void)
{
    T_EQ_UL(syntax89_node_count(NULL), 0);
    T_EQ_UL(syntax89_edge_count(NULL), 0);
    T_EQ_UL(syntax89_child_count(NULL, 1), 0);
    T_EQ_UL(syntax89_root(NULL), SYNTAX89_ID_NONE);
    T_EQ_LONG(syntax89_is_frozen(NULL), 0);
}

int main(void)
{
    test_node_queries();
    test_child_at_queries();
    test_role_queries();
    test_null_counts();
    return syntax89_test_report("test_queries");
}
