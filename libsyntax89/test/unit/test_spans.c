/* test_spans.c - span semantics S01..S08. */

#include <limits.h>

#include "syntax89_invariants.h"
#include "syntax89_test.h"

static void test_empty_span(void)
{
    syntax89_graph g;
    syntax89_id id;
    syntax89_node_info info;
    syntax89_span span;

    span.source = 1;
    span.begin = 0;
    span.end = 0;
    id = SYNTAX89_ID_NONE;
    T_OK(syntax89_init(&g, NULL));
    T_OK(syntax89_add_node(&g, 1, span, &id));
    T_OK(syntax89_node(&g, id, &info));
    T_EQ_UL(info.span.begin, 0);
    T_EQ_UL(info.span.end, 0);
    syntax89_destroy(&g);
}

static void test_one_byte_span(void)
{
    syntax89_graph g;
    syntax89_id id;
    syntax89_node_info info;
    syntax89_span span;

    span.source = 1;
    span.begin = 0;
    span.end = 1;
    id = SYNTAX89_ID_NONE;
    T_OK(syntax89_init(&g, NULL));
    T_OK(syntax89_add_node(&g, 1, span, &id));
    T_OK(syntax89_node(&g, id, &info));
    T_EQ_UL(info.span.begin, 0);
    T_EQ_UL(info.span.end, 1);
    syntax89_destroy(&g);
}

static void test_point_span(void)
{
    syntax89_graph g;
    syntax89_id id;
    syntax89_node_info info;
    syntax89_span span;

    span.source = 1;
    span.begin = 10;
    span.end = 10;
    id = SYNTAX89_ID_NONE;
    T_OK(syntax89_init(&g, NULL));
    T_OK(syntax89_add_node(&g, 1, span, &id));
    T_OK(syntax89_node(&g, id, &info));
    T_EQ_UL(info.span.begin, 10);
    T_EQ_UL(info.span.end, 10);
    syntax89_destroy(&g);
}

static void test_wide_span(void)
{
    syntax89_graph g;
    syntax89_id id;
    syntax89_node_info info;
    syntax89_span span;

    span.source = 1;
    span.begin = 10;
    span.end = 20;
    id = SYNTAX89_ID_NONE;
    T_OK(syntax89_init(&g, NULL));
    T_OK(syntax89_add_node(&g, 1, span, &id));
    T_OK(syntax89_node(&g, id, &info));
    T_EQ_UL(info.span.begin, 10);
    T_EQ_UL(info.span.end, 20);
    syntax89_destroy(&g);
}

static void test_source_zero_and_large(void)
{
    syntax89_graph g;
    syntax89_id a;
    syntax89_id b;
    syntax89_node_info info;
    syntax89_span span;

    a = SYNTAX89_ID_NONE;
    b = SYNTAX89_ID_NONE;
    T_OK(syntax89_init(&g, NULL));
    span.source = 0;
    span.begin = 0;
    span.end = 0;
    T_OK(syntax89_add_node(&g, 1, span, &a));
    span.source = ULONG_MAX;
    span.begin = 0;
    span.end = 1;
    T_OK(syntax89_add_node(&g, 1, span, &b));
    T_OK(syntax89_node(&g, a, &info));
    T_EQ_UL(info.span.source, 0);
    T_OK(syntax89_node(&g, b, &info));
    T_EQ_UL(info.span.source, ULONG_MAX);
    syntax89_destroy(&g);
}

static void test_max_offsets(void)
{
    syntax89_graph g;
    syntax89_id id;
    syntax89_node_info info;
    syntax89_span span;

    span.source = 1;
    span.begin = ULONG_MAX;
    span.end = ULONG_MAX;
    id = SYNTAX89_ID_NONE;
    T_OK(syntax89_init(&g, NULL));
    T_OK(syntax89_add_node(&g, 1, span, &id));
    T_OK(syntax89_node(&g, id, &info));
    T_EQ_UL(info.span.begin, ULONG_MAX);
    T_EQ_UL(info.span.end, ULONG_MAX);
    syntax89_destroy(&g);
}

static void test_reversed_span_rejected(void)
{
    syntax89_graph g;
    syntax89_id id;
    struct syntax89_test_snapshot *before;
    syntax89_span span;

    T_OK(syntax89_init(&g, NULL));
    T_ASSERT(syntax89_test_snapshot_take(&g, &before) == 0);
    span.source = 1;
    span.begin = 5;
    span.end = 4;
    id = 123;
    T_EQ_LONG(syntax89_add_node(&g, 1, span, &id), SYNTAX89_EINVAL);
    T_EQ_UL(id, 123);
    T_EQ_UL(syntax89_node_count(&g), 0);
    T_ASSERT(syntax89_test_snapshot_equal(before, &g) != 0);
    syntax89_test_snapshot_free(before);
    syntax89_destroy(&g);
}

int main(void)
{
    test_empty_span();
    test_one_byte_span();
    test_point_span();
    test_wide_span();
    test_source_zero_and_large();
    test_max_offsets();
    test_reversed_span_rejected();
    return syntax89_test_report("test_spans");
}
