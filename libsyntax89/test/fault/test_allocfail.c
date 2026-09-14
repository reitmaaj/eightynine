/* test_allocfail.c - allocation failure at every allocation point. */

#include "syntax89_fault_alloc.h"
#include "syntax89_fixtures.h"
#include "syntax89_invariants.h"
#include "syntax89_test.h"

struct fail_ctx
{
    struct syntax89_fault_alloc f;
    syntax89_allocator a;
    syntax89_graph g;
};

static syntax89_span span_of(unsigned long begin, unsigned long end)
{
    syntax89_span span;

    span.source = 1;
    span.begin = begin;
    span.end = end;
    return span;
}

static void test_init_never_allocates(void)
{
    struct fail_ctx c;

    syntax89_fault_alloc_init(&c.f);
    syntax89_fault_alloc_fail_at(&c.f, 1);
    syntax89_fault_alloc_use(&c.f, &c.a);
    T_OK(syntax89_init(&c.g, &c.a));
    T_EQ_UL(c.f.calls, 0);
    syntax89_destroy(&c.g);
    T_EQ_UL(c.f.live, 0);
}

static void test_add_node_first_allocation(void)
{
    struct fail_ctx c;
    struct syntax89_test_snapshot *before;
    syntax89_id id;

    syntax89_fault_alloc_init(&c.f);
    syntax89_fault_alloc_use(&c.f, &c.a);
    T_OK(syntax89_init(&c.g, &c.a));
    T_ASSERT(syntax89_test_snapshot_take(&c.g, &before) == 0);
    syntax89_fault_alloc_fail_at(&c.f, 1);
    id = SYNTAX89_ID_NONE;
    T_EQ_LONG(syntax89_add_node(&c.g, 1, span_of(0, 1), &id), SYNTAX89_ENOMEM);
    T_EQ_UL(id, SYNTAX89_ID_NONE);
    T_ASSERT(syntax89_test_snapshot_equal(before, &c.g) != 0);
    syntax89_test_snapshot_free(before);
    syntax89_fault_alloc_disable(&c.f);
    T_OK(syntax89_add_node(&c.g, 1, span_of(0, 1), &id));
    syntax89_destroy(&c.g);
    T_EQ_UL(c.f.live, 0);
}

static void test_add_node_growth_allocation(void)
{
    struct fail_ctx c;
    struct syntax89_test_snapshot *before;
    syntax89_id id;
    unsigned long i;

    syntax89_fault_alloc_init(&c.f);
    syntax89_fault_alloc_use(&c.f, &c.a);
    T_OK(syntax89_init(&c.g, &c.a));
    for (i = 0; i < 8; ++i)
    {
        id = SYNTAX89_ID_NONE;
        T_OK(syntax89_add_node(&c.g, i, span_of(0, 1), &id));
    }
    T_ASSERT(syntax89_test_snapshot_take(&c.g, &before) == 0);
    syntax89_fault_alloc_fail_at(&c.f, 1);
    id = SYNTAX89_ID_NONE;
    T_EQ_LONG(syntax89_add_node(&c.g, 9, span_of(0, 1), &id), SYNTAX89_ENOMEM);
    T_EQ_UL(id, SYNTAX89_ID_NONE);
    T_ASSERT(syntax89_test_snapshot_equal(before, &c.g) != 0);
    syntax89_test_snapshot_free(before);
    syntax89_fault_alloc_disable(&c.f);
    T_OK(syntax89_add_node(&c.g, 9, span_of(0, 1), &id));
    T_EQ_UL(syntax89_node_count(&c.g), 9);
    syntax89_destroy(&c.g);
    T_EQ_UL(c.f.live, 0);
}

static void test_add_child_first_allocation(void)
{
    struct fail_ctx c;
    struct syntax89_test_snapshot *before;
    syntax89_id parent;
    syntax89_id child;

    syntax89_fault_alloc_init(&c.f);
    syntax89_fault_alloc_use(&c.f, &c.a);
    T_OK(syntax89_init(&c.g, &c.a));
    parent = syntax89_fixture_node(&c.g, K_CALL, 0, 1);
    child = syntax89_fixture_node(&c.g, K_NAME, 1, 2);
    T_ASSERT(syntax89_test_snapshot_take(&c.g, &before) == 0);
    syntax89_fault_alloc_fail_at(&c.f, 1);
    T_EQ_LONG(syntax89_add_child(&c.g, parent, R_ARG, child), SYNTAX89_ENOMEM);
    T_ASSERT(syntax89_test_snapshot_equal(before, &c.g) != 0);
    syntax89_test_snapshot_free(before);
    syntax89_fault_alloc_disable(&c.f);
    T_OK(syntax89_add_child(&c.g, parent, R_ARG, child));
    syntax89_destroy(&c.g);
    T_EQ_UL(c.f.live, 0);
}

static void test_add_child_growth_allocation(void)
{
    struct fail_ctx c;
    struct syntax89_test_snapshot *before;
    syntax89_id parent;
    syntax89_id child;
    unsigned long i;

    syntax89_fault_alloc_init(&c.f);
    syntax89_fault_alloc_use(&c.f, &c.a);
    T_OK(syntax89_init(&c.g, &c.a));
    parent = syntax89_fixture_node(&c.g, K_CALL, 0, 1);
    for (i = 0; i < 4; ++i)
    {
        child = syntax89_fixture_node(&c.g, K_NAME, i + 1, i + 2);
        T_OK(syntax89_add_child(&c.g, parent, R_ARG, child));
    }
    child = syntax89_fixture_node(&c.g, K_NAME, 9, 10);
    T_ASSERT(syntax89_test_snapshot_take(&c.g, &before) == 0);
    syntax89_fault_alloc_fail_at(&c.f, 1);
    T_EQ_LONG(syntax89_add_child(&c.g, parent, R_ARG, child), SYNTAX89_ENOMEM);
    T_ASSERT(syntax89_test_snapshot_equal(before, &c.g) != 0);
    syntax89_test_snapshot_free(before);
    syntax89_fault_alloc_disable(&c.f);
    T_OK(syntax89_add_child(&c.g, parent, R_ARG, child));
    T_EQ_UL(syntax89_child_count(&c.g, parent), 5);
    syntax89_destroy(&c.g);
    T_EQ_UL(c.f.live, 0);
}

static void test_validate_scratch_points(void)
{
    struct fail_ctx c;
    struct syntax89_test_snapshot *before;
    unsigned long k;
    unsigned long live;

    for (k = 1; k <= 2; ++k)
    {
        syntax89_fault_alloc_init(&c.f);
        syntax89_fault_alloc_use(&c.f, &c.a);
        syntax89_fixture_build(&c.g, FX_F4, &c.a);
        T_ASSERT(syntax89_test_snapshot_take(&c.g, &before) == 0);
        live = c.f.live;
        syntax89_fault_alloc_fail_at(&c.f, k);
        T_EQ_LONG(syntax89_validate(&c.g, NULL), SYNTAX89_ENOMEM);
        T_EQ_UL(c.f.live, live);
        T_ASSERT(syntax89_test_snapshot_equal(before, &c.g) != 0);
        syntax89_test_snapshot_free(before);
        syntax89_fault_alloc_disable(&c.f);
        T_OK(syntax89_validate(&c.g, NULL));
        syntax89_destroy(&c.g);
        T_EQ_UL(c.f.live, 0);
    }
}

static void test_freeze_scratch_points(void)
{
    struct fail_ctx c;
    struct syntax89_test_snapshot *before;
    unsigned long k;
    unsigned long live;

    for (k = 1; k <= 2; ++k)
    {
        syntax89_fault_alloc_init(&c.f);
        syntax89_fault_alloc_use(&c.f, &c.a);
        syntax89_fixture_build(&c.g, FX_F4, &c.a);
        T_ASSERT(syntax89_test_snapshot_take(&c.g, &before) == 0);
        live = c.f.live;
        syntax89_fault_alloc_fail_at(&c.f, k);
        T_EQ_LONG(syntax89_freeze(&c.g), SYNTAX89_ENOMEM);
        T_EQ_UL(c.f.live, live);
        T_ASSERT(syntax89_test_snapshot_equal(before, &c.g) != 0);
        syntax89_test_snapshot_free(before);
        syntax89_fault_alloc_disable(&c.f);
        T_OK(syntax89_freeze(&c.g));
        syntax89_destroy(&c.g);
        T_EQ_UL(c.f.live, 0);
    }
}

static syntax89_status count_visit(void *ctx, syntax89_id node)
{
    unsigned long *n;

    n = ctx;
    (void)node;
    *n += 1;
    return SYNTAX89_OK;
}

static void test_walk_scratch_points(void)
{
    struct fail_ctx c;
    struct syntax89_test_snapshot *before;
    unsigned long k;
    unsigned long live;
    unsigned long seen;

    for (k = 1; k <= 2; ++k)
    {
        syntax89_fault_alloc_init(&c.f);
        syntax89_fault_alloc_use(&c.f, &c.a);
        syntax89_fixture_build(&c.g, FX_F4, &c.a);
        T_ASSERT(syntax89_test_snapshot_take(&c.g, &before) == 0);
        live = c.f.live;
        syntax89_fault_alloc_fail_at(&c.f, k);
        seen = 0;
        T_EQ_LONG(syntax89_walk_nodes_pre(&c.g, 1, count_visit, &seen),
                  SYNTAX89_ENOMEM);
        T_EQ_UL(seen, 0);
        T_EQ_UL(c.f.live, live);
        T_ASSERT(syntax89_test_snapshot_equal(before, &c.g) != 0);
        syntax89_test_snapshot_free(before);
        syntax89_fault_alloc_disable(&c.f);
        T_OK(syntax89_walk_nodes_pre(&c.g, 1, count_visit, &seen));
        T_EQ_UL(seen, 7);
        syntax89_destroy(&c.g);
        T_EQ_UL(c.f.live, 0);
    }
}

int main(void)
{
    test_init_never_allocates();
    test_add_node_first_allocation();
    test_add_node_growth_allocation();
    test_add_child_first_allocation();
    test_add_child_growth_allocation();
    test_validate_scratch_points();
    test_freeze_scratch_points();
    test_walk_scratch_points();
    return syntax89_test_report("test_allocfail");
}
