/* test_atomicity.c - unchanged-on-failure across mutators. */

#include "syntax89_fault_alloc.h"
#include "syntax89_fixtures.h"
#include "syntax89_invariants.h"
#include "syntax89_test.h"

static syntax89_status visit_noop(void *ctx, syntax89_id node)
{
    unsigned long *n;

    n = ctx;
    (void)node;
    *n += 1;
    return SYNTAX89_OK;
}

static syntax89_span span_of(unsigned long begin, unsigned long end)
{
    syntax89_span span;

    span.source = 1;
    span.begin = begin;
    span.end = end;
    return span;
}

static void test_add_node_enomem(void)
{
    syntax89_graph g;
    struct syntax89_fault_alloc f;
    syntax89_allocator a;
    struct syntax89_test_snapshot *before;
    syntax89_id id;

    syntax89_fault_alloc_init(&f);
    syntax89_fault_alloc_use(&f, &a);
    T_OK(syntax89_init(&g, &a));
    T_ASSERT(syntax89_test_snapshot_take(&g, &before) == 0);
    syntax89_fault_alloc_fail_at(&f, 1);
    id = SYNTAX89_ID_NONE;
    T_EQ_LONG(syntax89_add_node(&g, 1, span_of(0, 1), &id), SYNTAX89_ENOMEM);
    T_EQ_UL(id, SYNTAX89_ID_NONE);
    T_ASSERT(syntax89_test_snapshot_equal(before, &g) != 0);
    syntax89_test_snapshot_free(before);
    syntax89_fault_alloc_disable(&f);
    T_OK(syntax89_add_node(&g, 1, span_of(0, 1), &id));
    syntax89_test_check(&g);
    syntax89_destroy(&g);
    T_EQ_UL(f.live, 0);
}

static void test_add_node_growth_enomem(void)
{
    syntax89_graph g;
    struct syntax89_fault_alloc f;
    syntax89_allocator a;
    struct syntax89_test_snapshot *before;
    syntax89_id id;
    unsigned long i;

    syntax89_fault_alloc_init(&f);
    syntax89_fault_alloc_use(&f, &a);
    T_OK(syntax89_init(&g, &a));
    for (i = 0; i < 8; ++i)
    {
        id = SYNTAX89_ID_NONE;
        T_OK(syntax89_add_node(&g, i, span_of(0, 1), &id));
    }
    T_ASSERT(syntax89_test_snapshot_take(&g, &before) == 0);
    syntax89_fault_alloc_fail_at(&f, 1);
    id = SYNTAX89_ID_NONE;
    T_EQ_LONG(syntax89_add_node(&g, 9, span_of(0, 1), &id), SYNTAX89_ENOMEM);
    T_EQ_UL(id, SYNTAX89_ID_NONE);
    T_ASSERT(syntax89_test_snapshot_equal(before, &g) != 0);
    syntax89_test_snapshot_free(before);
    syntax89_fault_alloc_disable(&f);
    T_OK(syntax89_add_node(&g, 9, span_of(0, 1), &id));
    syntax89_test_check(&g);
    syntax89_destroy(&g);
    T_EQ_UL(f.live, 0);
}

static void test_add_child_enomem(void)
{
    syntax89_graph g;
    struct syntax89_fault_alloc f;
    syntax89_allocator a;
    struct syntax89_test_snapshot *before;
    syntax89_id a_id;
    syntax89_id b_id;
    syntax89_id c_id;

    syntax89_fault_alloc_init(&f);
    syntax89_fault_alloc_use(&f, &a);
    T_OK(syntax89_init(&g, &a));
    a_id = syntax89_fixture_node(&g, K_NAME, 0, 1);
    b_id = syntax89_fixture_node(&g, K_NAME, 1, 2);
    c_id = syntax89_fixture_node(&g, K_NAME, 2, 3);
    T_ASSERT(syntax89_test_snapshot_take(&g, &before) == 0);
    syntax89_fault_alloc_fail_at(&f, 1);
    T_EQ_LONG(syntax89_add_child(&g, a_id, R_LEFT, b_id), SYNTAX89_ENOMEM);
    T_ASSERT(syntax89_test_snapshot_equal(before, &g) != 0);
    syntax89_test_snapshot_free(before);
    syntax89_fault_alloc_disable(&f);
    T_OK(syntax89_add_child(&g, a_id, R_LEFT, b_id));
    T_OK(syntax89_add_child(&g, a_id, R_LEFT, c_id));
    syntax89_test_check(&g);
    syntax89_destroy(&g);
    T_EQ_UL(f.live, 0);
}

static void test_add_child_growth_enomem(void)
{
    syntax89_graph g;
    struct syntax89_fault_alloc f;
    syntax89_allocator a;
    struct syntax89_test_snapshot *before;
    syntax89_id parent;
    syntax89_id child;
    unsigned long i;

    syntax89_fault_alloc_init(&f);
    syntax89_fault_alloc_use(&f, &a);
    T_OK(syntax89_init(&g, &a));
    parent = syntax89_fixture_node(&g, K_NAME, 0, 1);
    for (i = 0; i < 4; ++i)
    {
        child = syntax89_fixture_node(&g, K_NAME, i + 1, i + 2);
        T_OK(syntax89_add_child(&g, parent, R_ITEM, child));
    }
    child = syntax89_fixture_node(&g, K_NAME, 9, 10);
    T_ASSERT(syntax89_test_snapshot_take(&g, &before) == 0);
    syntax89_fault_alloc_fail_at(&f, 1);
    T_EQ_LONG(syntax89_add_child(&g, parent, R_ITEM, child), SYNTAX89_ENOMEM);
    T_ASSERT(syntax89_test_snapshot_equal(before, &g) != 0);
    syntax89_test_snapshot_free(before);
    syntax89_fault_alloc_disable(&f);
    T_OK(syntax89_add_child(&g, parent, R_ITEM, child));
    syntax89_test_check(&g);
    syntax89_destroy(&g);
    T_EQ_UL(f.live, 0);
}

static void test_set_root_never_allocates(void)
{
    syntax89_graph g;
    struct syntax89_fault_alloc f;
    syntax89_allocator a;
    syntax89_id id;

    syntax89_fault_alloc_init(&f);
    syntax89_fault_alloc_use(&f, &a);
    T_OK(syntax89_init(&g, &a));
    id = syntax89_fixture_node(&g, K_NAME, 0, 1);
    syntax89_fault_alloc_fail_at(&f, 1);
    T_OK(syntax89_set_root(&g, id));
    T_EQ_UL(f.calls, 0);
    syntax89_destroy(&g);
    T_EQ_UL(f.live, 0);
}

static void test_walk_enomem_is_atomic(void)
{
    syntax89_graph g;
    struct syntax89_fault_alloc f;
    syntax89_allocator a;
    struct syntax89_test_snapshot *before;
    unsigned long seen;

    syntax89_fault_alloc_init(&f);
    syntax89_fault_alloc_use(&f, &a);
    syntax89_fixture_build(&g, FX_F1, &a);
    T_ASSERT(syntax89_test_snapshot_take(&g, &before) == 0);
    syntax89_fault_alloc_fail_at(&f, 1);
    seen = 0;
    T_EQ_LONG(syntax89_walk_nodes_pre(&g, 1, visit_noop, &seen),
              SYNTAX89_ENOMEM);
    T_EQ_UL(seen, 0);
    T_ASSERT(syntax89_test_snapshot_equal(before, &g) != 0);
    syntax89_test_snapshot_free(before);
    syntax89_fault_alloc_disable(&f);
    T_OK(syntax89_walk_nodes_pre(&g, 1, visit_noop, &seen));
    syntax89_destroy(&g);
    T_EQ_UL(f.live, 0);
}

int main(void)
{
    test_add_node_enomem();
    test_add_node_growth_enomem();
    test_add_child_enomem();
    test_add_child_growth_enomem();
    test_set_root_never_allocates();
    test_walk_enomem_is_atomic();
    return syntax89_test_report("test_atomicity");
}
