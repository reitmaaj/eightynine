/* test_nodes.c - node creation N01..N08. */

#include <limits.h>

#include "syntax89_invariants.h"
#include "syntax89_test.h"

static syntax89_span span_of(unsigned long begin, unsigned long end)
{
    syntax89_span span;

    span.source = 7;
    span.begin = begin;
    span.end = end;
    return span;
}

static void test_first_node(void)
{
    syntax89_graph g;
    syntax89_id id;
    syntax89_node_info info;

    id = SYNTAX89_ID_NONE;
    T_OK(syntax89_init(&g, NULL));
    T_OK(syntax89_add_node(&g, 42, span_of(0, 0), &id));
    T_ASSERT(id != SYNTAX89_ID_NONE);
    T_EQ_UL(syntax89_node_count(&g), 1);
    T_OK(syntax89_node(&g, id, &info));
    T_EQ_UL(info.kind, 42);
    T_EQ_UL(info.span.source, 7);
    T_EQ_UL(info.span.begin, 0);
    T_EQ_UL(info.span.end, 0);
    syntax89_destroy(&g);
}

static void test_distinct_ids(void)
{
    syntax89_graph g;
    syntax89_id a;
    syntax89_id b;

    a = SYNTAX89_ID_NONE;
    b = SYNTAX89_ID_NONE;
    T_OK(syntax89_init(&g, NULL));
    T_OK(syntax89_add_node(&g, 1, span_of(0, 1), &a));
    T_OK(syntax89_add_node(&g, 2, span_of(1, 2), &b));
    T_ASSERT(a != b);
    T_EQ_UL(syntax89_node_count(&g), 2);
    syntax89_destroy(&g);
}

static void test_hundred_nodes(void)
{
    syntax89_graph g;
    syntax89_id id;
    unsigned long i;
    unsigned long j;

    T_OK(syntax89_init(&g, NULL));
    for (i = 0; i < 100; ++i)
    {
        id = SYNTAX89_ID_NONE;
        T_OK(syntax89_add_node(&g, i, span_of(i, i + 1), &id));
        T_EQ_UL(syntax89_node_count(&g), i + 1);
        for (j = 1; j <= i; ++j)
        {
            T_ASSERT(id != j);
        }
    }
    syntax89_test_check(&g);
    syntax89_destroy(&g);
}

static void test_kind_and_span_preserved(void)
{
    syntax89_graph g;
    syntax89_id id;
    syntax89_node_info info;
    syntax89_span span;

    T_OK(syntax89_init(&g, NULL));
    span.source = 99;
    span.begin = 10;
    span.end = 20;
    id = SYNTAX89_ID_NONE;
    T_OK(syntax89_add_node(&g, 123456789UL, span, &id));
    T_OK(syntax89_node(&g, id, &info));
    T_EQ_UL(info.kind, 123456789UL);
    T_EQ_UL(info.span.source, 99);
    T_EQ_UL(info.span.begin, 10);
    T_EQ_UL(info.span.end, 20);
    syntax89_destroy(&g);
}

static void test_zero_kind_and_max_kind(void)
{
    syntax89_graph g;
    syntax89_id a;
    syntax89_id b;
    syntax89_node_info info;

    a = SYNTAX89_ID_NONE;
    b = SYNTAX89_ID_NONE;
    T_OK(syntax89_init(&g, NULL));
    T_OK(syntax89_add_node(&g, 0, span_of(0, 0), &a));
    T_OK(syntax89_add_node(&g, ULONG_MAX, span_of(0, 0), &b));
    T_OK(syntax89_node(&g, a, &info));
    T_EQ_UL(info.kind, 0);
    T_OK(syntax89_node(&g, b, &info));
    T_EQ_UL(info.kind, ULONG_MAX);
    syntax89_destroy(&g);
}

int main(void)
{
    test_first_node();
    test_distinct_ids();
    test_hundred_nodes();
    test_kind_and_span_preserved();
    test_zero_kind_and_max_kind();
    return syntax89_test_report("test_nodes");
}
