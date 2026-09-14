/* test_preconditions.c - documented NULL policy. */

#include "syntax89_fixtures.h"
#include "syntax89_invariants.h"
#include "syntax89_test.h"

static void test_lifecycle_nulls(void)
{
    syntax89_span span;

    span.source = 1;
    span.begin = 0;
    span.end = 1;
    T_EQ_LONG(syntax89_init(NULL, NULL), SYNTAX89_EINVAL);
    syntax89_destroy(NULL);
    T_EQ_LONG(syntax89_freeze(NULL), SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_validate(NULL, NULL), SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_add_node(NULL, 1, span, NULL), SYNTAX89_EINVAL);
}

static void test_construction_nulls(void)
{
    syntax89_graph g;
    syntax89_span span;

    syntax89_fixture_build(&g, FX_F1, NULL);
    span.source = 1;
    span.begin = 0;
    span.end = 1;
    T_EQ_LONG(syntax89_add_node(&g, 1, span, NULL), SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_add_child(NULL, 1, R_LEFT, 2), SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_set_root(NULL, 1), SYNTAX89_EINVAL);
    syntax89_destroy(&g);
}

static void test_query_nulls(void)
{
    syntax89_graph g;
    syntax89_node_info info;
    syntax89_role role;
    syntax89_id child;
    syntax89_child_iter it;

    syntax89_fixture_build(&g, FX_F1, NULL);
    T_EQ_LONG(syntax89_node(NULL, 1, &info), SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_node(&g, 1, NULL), SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_child_at(NULL, 1, 0, &role, &child), SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_child_at(&g, 1, 0, NULL, &child), SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_child_at(&g, 1, 0, &role, NULL), SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_child_at_role(NULL, 1, R_LEFT, 0, &child),
              SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_child_at_role(&g, 1, R_LEFT, 0, NULL), SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_children_begin(NULL, 1, &it), SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_children_begin(&g, 1, NULL), SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_children_next(NULL, &role, &child), SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_children_next(&it, NULL, &child), SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_children_next(&it, &role, NULL), SYNTAX89_EINVAL);
    syntax89_destroy(&g);
}

static void test_walk_nulls(void)
{
    syntax89_graph g;
    unsigned long seen;

    syntax89_fixture_build(&g, FX_F1, NULL);
    seen = 0;
    T_EQ_LONG(syntax89_walk_nodes_pre(NULL, 1, NULL, &seen), SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_walk_nodes_pre(&g, 1, NULL, &seen), SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_walk_nodes_post(&g, 1, NULL, &seen), SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_walk_edges_pre(&g, 1, NULL, &seen), SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_walk_edges_post(&g, 1, NULL, &seen), SYNTAX89_EINVAL);
    syntax89_destroy(&g);
}

static void test_value_queries_tolerate_null(void)
{
    T_EQ_UL(syntax89_node_count(NULL), 0);
    T_EQ_UL(syntax89_edge_count(NULL), 0);
    T_EQ_UL(syntax89_root(NULL), SYNTAX89_ID_NONE);
    T_EQ_UL(syntax89_child_count(NULL, 1), 0);
    T_EQ_UL(syntax89_child_count_role(NULL, 1, R_LEFT), 0);
    T_EQ_LONG(syntax89_is_frozen(NULL), 0);
}

static void test_validation_result_optional(void)
{
    syntax89_graph g;

    syntax89_fixture_build(&g, FX_F1, NULL);
    T_OK(syntax89_validate(&g, NULL));
    syntax89_destroy(&g);
}

int main(void)
{
    test_lifecycle_nulls();
    test_construction_nulls();
    test_query_nulls();
    test_walk_nulls();
    test_value_queries_tolerate_null();
    test_validation_result_optional();
    return syntax89_test_report("test_preconditions");
}
