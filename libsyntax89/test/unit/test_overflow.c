/* test_overflow.c - checked arithmetic and injected limits OV01..OV05. */

#include <limits.h>

#include "syntax89_fault_alloc.h"
#include "syntax89_fixtures.h"
#include "syntax89_internal.h"
#include "syntax89_invariants.h"
#include "syntax89_test.h"

static void test_size_add(void)
{
    unsigned long out;

    out = 123;
    T_EQ_LONG(syntax89__size_add(1, 2, &out), SYNTAX89_OK);
    T_EQ_UL(out, 3);
    out = 123;
    T_EQ_LONG(syntax89__size_add(ULONG_MAX, 1, &out), SYNTAX89_EOVERFLOW);
    T_EQ_UL(out, 123);
    out = 123;
    T_EQ_LONG(syntax89__size_add(ULONG_MAX, ULONG_MAX, &out),
              SYNTAX89_EOVERFLOW);
    T_EQ_UL(out, 123);
}

static void test_size_mul(void)
{
    unsigned long out;

    out = 123;
    T_EQ_LONG(syntax89__size_mul(6, 7, &out), SYNTAX89_OK);
    T_EQ_UL(out, 42);
    out = 123;
    T_EQ_LONG(syntax89__size_mul(0, ULONG_MAX, &out), SYNTAX89_OK);
    T_EQ_UL(out, 0);
    out = 123;
    T_EQ_LONG(syntax89__size_mul(ULONG_MAX, 2, &out), SYNTAX89_EOVERFLOW);
    T_EQ_UL(out, 123);
    out = 123;
    T_EQ_LONG(syntax89__size_mul(ULONG_MAX / 2 + 1, 2, &out),
              SYNTAX89_EOVERFLOW);
    T_EQ_UL(out, 123);
}

static void test_node_limit(void)
{
    syntax89_graph g;
    syntax89_id id;
    syntax89_span span;
    struct syntax89_test_snapshot *before;

    span.source = 1;
    span.begin = 0;
    span.end = 1;
    T_OK(syntax89_init(&g, NULL));
    T_OK(syntax89_add_node(&g, 1, span, &id));
    syntax89__set_limit_nodes(&g, 1);
    T_ASSERT(syntax89_test_snapshot_take(&g, &before) == 0);
    id = SYNTAX89_ID_NONE;
    T_EQ_LONG(syntax89_add_node(&g, 1, span, &id), SYNTAX89_EOVERFLOW);
    T_EQ_UL(id, SYNTAX89_ID_NONE);
    T_ASSERT(syntax89_test_snapshot_equal(before, &g) != 0);
    syntax89_test_snapshot_free(before);
    syntax89__set_limit_nodes(&g, 0);
    T_OK(syntax89_add_node(&g, 1, span, &id));
    syntax89_destroy(&g);
}

static void test_edge_limit(void)
{
    syntax89_graph g;
    syntax89_id a;
    syntax89_id b;
    struct syntax89_test_snapshot *before;

    T_OK(syntax89_init(&g, NULL));
    a = syntax89_fixture_node(&g, K_NAME, 0, 1);
    b = syntax89_fixture_node(&g, K_NAME, 1, 2);
    T_OK(syntax89_add_child(&g, a, R_LEFT, b));
    syntax89__set_limit_edges(&g, 1);
    T_ASSERT(syntax89_test_snapshot_take(&g, &before) == 0);
    T_EQ_LONG(syntax89_add_child(&g, a, R_LEFT, b), SYNTAX89_EOVERFLOW);
    T_ASSERT(syntax89_test_snapshot_equal(before, &g) != 0);
    syntax89_test_snapshot_free(before);
    syntax89__set_limit_edges(&g, 0);
    T_OK(syntax89_add_child(&g, a, R_LEFT, b));
    syntax89_destroy(&g);
}

static void test_growth_capacity_overflow(void)
{
    syntax89_graph g;
    struct syntax89_fault_alloc f;
    syntax89_allocator a;

    syntax89_fault_alloc_init(&f);
    syntax89_fault_alloc_fail_at(&f, 1);
    syntax89_fault_alloc_use(&f, &a);
    T_OK(syntax89_init(&g, &a));
    T_EQ_LONG(syntax89__grow_nodes(&g, ULONG_MAX), SYNTAX89_EOVERFLOW);
    T_EQ_UL(g.node_count, 0);
    T_EQ_UL(g.node_capacity, 0);
    T_EQ_UL(f.calls, 0);
    syntax89_destroy(&g);
}

static void test_size_mul_byte_overflow(void)
{
    syntax89_graph g;
    struct syntax89_fault_alloc f;
    syntax89_allocator a;

    syntax89_fault_alloc_init(&f);
    syntax89_fault_alloc_fail_at(&f, 1);
    syntax89_fault_alloc_use(&f, &a);
    T_OK(syntax89_init(&g, &a));
    g.node_capacity = ULONG_MAX / 2 + 1;
    T_EQ_LONG(syntax89__grow_nodes(&g, ULONG_MAX / 2 + 2), SYNTAX89_EOVERFLOW);
    T_EQ_UL(f.calls, 0);
    g.node_capacity = 0;
    syntax89_destroy(&g);
}

static void test_growth_byte_overflow(void)
{
    syntax89_graph g;
    struct syntax89_fault_alloc f;
    syntax89_allocator a;
    struct syntax89_node *p;
    syntax89_id id;
    syntax89_span span;

    span.source = 1;
    span.begin = 0;
    span.end = 1;
    syntax89_fault_alloc_init(&f);
    syntax89_fault_alloc_use(&f, &a);
    T_OK(syntax89_init(&g, &a));
    g.node_capacity = ULONG_MAX / 32;
    T_EQ_LONG(syntax89__grow_nodes(&g, ULONG_MAX / 32 + 1), SYNTAX89_EOVERFLOW);
    g.node_capacity = 0;
    T_EQ_UL(f.calls, 0);
    T_OK(syntax89_add_node(&g, 1, span, &id));
    p = syntax89__node_mut(&g, id);
    T_ASSERT(p != NULL);
    p->edge_capacity = ULONG_MAX / 2 + 1;
    T_EQ_LONG(syntax89__grow_edges(&g, p, ULONG_MAX), SYNTAX89_EOVERFLOW);
    p->edge_capacity = ULONG_MAX / 16 + 1;
    T_EQ_LONG(syntax89__grow_edges(&g, p, ULONG_MAX / 16 + 2),
              SYNTAX89_EOVERFLOW);
    p->edge_capacity = 0;
    T_EQ_UL(f.calls, 1);
    syntax89_destroy(&g);
}

static void test_internal_branches(void)
{
    syntax89_graph g;
    syntax89_id id;
    syntax89_span span;

    span.source = 1;
    span.begin = 0;
    span.end = 1;
    syntax89__set_limit_nodes(NULL, 1);
    syntax89__set_limit_edges(NULL, 1);
    T_ASSERT(syntax89__node_at(NULL, 1) == NULL);
    T_ASSERT(syntax89__node_mut(NULL, 1) == NULL);
    T_OK(syntax89_init(&g, NULL));
    id = SYNTAX89_ID_NONE;
    T_OK(syntax89_add_node(&g, 1, span, &id));
    g.node_count = ULONG_MAX;
    id = SYNTAX89_ID_NONE;
    T_EQ_LONG(syntax89_add_node(&g, 1, span, &id), SYNTAX89_EOVERFLOW);
    T_EQ_UL(id, SYNTAX89_ID_NONE);
    g.node_count = 1;
    g.edge_count = ULONG_MAX;
    T_EQ_LONG(syntax89_add_child(&g, 1, 1, 1), SYNTAX89_EOVERFLOW);
    g.edge_count = 0;
    syntax89_destroy(&g);
}

int main(void)
{
    test_size_add();
    test_size_mul();
    test_node_limit();
    test_edge_limit();
    test_growth_capacity_overflow();
    test_size_mul_byte_overflow();
    test_growth_byte_overflow();
    test_internal_branches();
    return syntax89_test_report("test_overflow");
}
