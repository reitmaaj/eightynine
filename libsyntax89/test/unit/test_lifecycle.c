/* test_lifecycle.c - lifecycle transitions L01..L13. */

#include <string.h>

#include "syntax89_fault_alloc.h"
#include "syntax89_fixtures.h"
#include "syntax89_invariants.h"
#include "syntax89_test.h"

static void test_init_allocates_nothing(void)
{
    syntax89_graph g;
    struct syntax89_fault_alloc f;
    syntax89_allocator a;

    syntax89_fault_alloc_init(&f);
    syntax89_fault_alloc_fail_at(&f, 1);
    syntax89_fault_alloc_use(&f, &a);
    memset(&g, 0xAA, sizeof(g));
    T_OK(syntax89_init(&g, &a));
    T_EQ_UL(syntax89_node_count(&g), 0);
    T_EQ_UL(syntax89_edge_count(&g), 0);
    T_EQ_UL(syntax89_root(&g), SYNTAX89_ID_NONE);
    T_ASSERT(syntax89_is_frozen(&g) == 0);
    T_EQ_UL(f.calls, 0);
    syntax89_test_check(&g);
    syntax89_destroy(&g);
    T_EQ_UL(f.live, 0);
}

static void test_init_null(void)
{
    T_EQ_LONG(syntax89_init(NULL, NULL), SYNTAX89_EINVAL);
}

static void test_destroy_zeroed(void)
{
    syntax89_graph g;

    memset(&g, 0, sizeof(g));
    syntax89_destroy(&g);
    syntax89_destroy(&g);
    syntax89_destroy(NULL);
}

static void test_destroy_populated(void)
{
    syntax89_graph g;
    struct syntax89_fault_alloc f;
    syntax89_allocator a;

    syntax89_fault_alloc_init(&f);
    syntax89_fault_alloc_use(&f, &a);
    syntax89_fixture_build(&g, FX_F4, &a);
    T_ASSERT(f.live > 0);
    syntax89_destroy(&g);
    T_EQ_UL(f.live, 0);
    T_EQ_UL(syntax89_node_count(&g), 0);
    T_EQ_UL(syntax89_root(&g), SYNTAX89_ID_NONE);
}

static void test_uninit_operations(void)
{
    syntax89_graph g;
    syntax89_node_info info;
    syntax89_role role;
    syntax89_id child;
    syntax89_child_iter it;
    syntax89_span span;

    memset(&g, 0, sizeof(g));
    span.source = 1;
    span.begin = 0;
    span.end = 1;
    T_EQ_LONG(syntax89_add_node(&g, K_INT, span, &child), SYNTAX89_ESTATE);
    T_EQ_LONG(syntax89_add_child(&g, 1, R_LEFT, 1), SYNTAX89_ESTATE);
    T_EQ_LONG(syntax89_set_root(&g, 1), SYNTAX89_ESTATE);
    T_EQ_LONG(syntax89_freeze(&g), SYNTAX89_ESTATE);
    T_EQ_LONG(syntax89_validate(&g, NULL), SYNTAX89_ESTATE);
    T_EQ_LONG(syntax89_node(&g, 1, &info), SYNTAX89_ESTATE);
    T_EQ_LONG(syntax89_child_at(&g, 1, 0, &role, &child), SYNTAX89_ESTATE);
    T_EQ_LONG(syntax89_child_at_role(&g, 1, R_LEFT, 0, &child),
              SYNTAX89_ESTATE);
    T_EQ_LONG(syntax89_children_begin(&g, 1, &it), SYNTAX89_ESTATE);
    T_EQ_UL(syntax89_child_count(&g, 1), 0);
    T_EQ_UL(syntax89_child_count_role(&g, 1, R_LEFT), 0);
    T_EQ_UL(syntax89_node_count(&g), 0);
    T_EQ_UL(syntax89_edge_count(&g), 0);
    T_ASSERT(syntax89_is_frozen(&g) == 0);
}

static void test_freeze_valid(void)
{
    syntax89_graph g;
    syntax89_validation v;

    syntax89_fixture_build(&g, FX_F1, NULL);
    T_EQ_LONG(syntax89_is_frozen(&g), 0);
    T_OK(syntax89_freeze(&g));
    T_ASSERT(syntax89_is_frozen(&g) != 0);
    T_OK(syntax89_validate(&g, &v));
    T_EQ_LONG(v.error, SYNTAX89_OK);
    T_EQ_UL(v.node, SYNTAX89_ID_NONE);
    syntax89_destroy(&g);
}

static void test_freeze_invalid_remains_building(void)
{
    syntax89_graph g;
    struct syntax89_test_snapshot *before;

    syntax89_fixture_build(&g, FX_F6, NULL);
    T_ASSERT(syntax89_test_snapshot_take(&g, &before) == 0);
    T_EQ_LONG(syntax89_freeze(&g), SYNTAX89_ECYCLE);
    T_ASSERT(syntax89_is_frozen(&g) == 0);
    T_ASSERT(syntax89_test_snapshot_equal(before, &g) != 0);
    syntax89_test_snapshot_free(before);
    syntax89_destroy(&g);
}

static void test_freeze_twice(void)
{
    syntax89_graph g;
    struct syntax89_test_snapshot *before;

    syntax89_fixture_build(&g, FX_F3, NULL);
    T_OK(syntax89_freeze(&g));
    T_ASSERT(syntax89_test_snapshot_take(&g, &before) == 0);
    T_OK(syntax89_freeze(&g));
    T_ASSERT(syntax89_test_snapshot_equal(before, &g) != 0);
    syntax89_test_snapshot_free(before);
    syntax89_destroy(&g);
}

static void test_frozen_rejects_mutators(void)
{
    syntax89_graph g;
    struct syntax89_test_snapshot *before;
    syntax89_id id;
    syntax89_span span;

    syntax89_fixture_build(&g, FX_F1, NULL);
    T_OK(syntax89_freeze(&g));
    T_ASSERT(syntax89_test_snapshot_take(&g, &before) == 0);
    span.source = 1;
    span.begin = 0;
    span.end = 1;
    id = SYNTAX89_ID_NONE;
    T_EQ_LONG(syntax89_add_node(&g, K_INT, span, &id), SYNTAX89_ESTATE);
    T_EQ_UL(id, SYNTAX89_ID_NONE);
    T_EQ_LONG(syntax89_add_child(&g, 1, R_LEFT, 2), SYNTAX89_ESTATE);
    T_EQ_LONG(syntax89_set_root(&g, 2), SYNTAX89_ESTATE);
    T_ASSERT(syntax89_test_snapshot_equal(before, &g) != 0);
    syntax89_test_snapshot_free(before);
    syntax89_destroy(&g);
}

static void test_inspection_same_before_and_after_freeze(void)
{
    syntax89_graph g;
    syntax89_node_info before;
    syntax89_node_info after;
    syntax89_role role_before;
    syntax89_role role_after;
    syntax89_id child_before;
    syntax89_id child_after;

    syntax89_fixture_build(&g, FX_F1, NULL);
    T_OK(syntax89_node(&g, 2, &before));
    T_OK(syntax89_child_at(&g, 1, 0, &role_before, &child_before));
    T_OK(syntax89_freeze(&g));
    T_OK(syntax89_node(&g, 2, &after));
    T_OK(syntax89_child_at(&g, 1, 0, &role_after, &child_after));
    T_EQ_UL(before.kind, after.kind);
    T_EQ_UL(before.span.begin, after.span.begin);
    T_EQ_UL(before.span.end, after.span.end);
    T_EQ_UL(role_before, role_after);
    T_EQ_UL(child_before, child_after);
    T_EQ_UL(syntax89_root(&g), 1);
    syntax89_destroy(&g);
}

int main(void)
{
    test_init_allocates_nothing();
    test_init_null();
    test_destroy_zeroed();
    test_destroy_populated();
    test_uninit_operations();
    test_freeze_valid();
    test_freeze_invalid_remains_building();
    test_freeze_twice();
    test_frozen_rejects_mutators();
    test_inspection_same_before_and_after_freeze();
    return syntax89_test_report("test_lifecycle");
}
