/* test_ids.c - stable identity ID01..ID07. */

#include "syntax89_fixtures.h"
#include "syntax89_internal.h"
#include "syntax89_invariants.h"
#include "syntax89_test.h"

static syntax89_span span_of(unsigned long begin, unsigned long end)
{
    syntax89_span span;

    span.source = 1;
    span.begin = begin;
    span.end = end;
    return span;
}

static void test_ids_never_none(void)
{
    syntax89_graph g;
    syntax89_id id;
    unsigned long i;

    T_OK(syntax89_init(&g, NULL));
    for (i = 0; i < 64; ++i)
    {
        id = SYNTAX89_ID_NONE;
        T_OK(syntax89_add_node(&g, i, span_of(0, 1), &id));
        T_ASSERT(id != SYNTAX89_ID_NONE);
    }
    syntax89_destroy(&g);
}

static void test_ids_survive_growth(void)
{
    syntax89_graph g;
    syntax89_id first;
    syntax89_id second;
    syntax89_id id;
    unsigned long i;

    first = SYNTAX89_ID_NONE;
    second = SYNTAX89_ID_NONE;
    T_OK(syntax89_init(&g, NULL));
    T_OK(syntax89_add_node(&g, 1, span_of(0, 1), &first));
    T_OK(syntax89_add_node(&g, 2, span_of(1, 2), &second));
    for (i = 0; i < 5000; ++i)
    {
        id = SYNTAX89_ID_NONE;
        T_OK(syntax89_add_node(&g, 3, span_of(0, 1), &id));
    }
    T_ASSERT(syntax89__node_at(&g, first) != NULL);
    T_EQ_UL(syntax89__node_at(&g, first)->kind, 1);
    T_EQ_UL(syntax89__node_at(&g, second)->kind, 2);
    T_EQ_UL(syntax89_node_count(&g), 5002);
    syntax89_destroy(&g);
}

static void test_ids_stable_across_operations(void)
{
    syntax89_graph g;
    syntax89_id root;
    syntax89_id a;
    syntax89_id b;
    struct syntax89_test_snapshot *snap;
    unsigned long i;

    root = syntax89_fixture_build(&g, FX_F1, NULL);
    a = 2;
    b = 3;
    T_ASSERT(syntax89_test_snapshot_take(&g, &snap) == 0);
    T_OK(syntax89_add_child(&g, root, R_ITEM, a));
    T_OK(syntax89_add_child(&g, root, R_ITEM, b));
    T_OK(syntax89_set_root(&g, a));
    T_OK(syntax89_set_root(&g, root));
    for (i = 0; i < 32; ++i)
    {
        T_EQ_UL(syntax89__node_at(&g, root)->kind, K_ADD);
        T_EQ_UL(syntax89__node_at(&g, a)->kind, K_INT);
        T_EQ_UL(syntax89__node_at(&g, b)->kind, K_INT);
    }
    T_OK(syntax89_freeze(&g));
    T_EQ_UL(syntax89__node_at(&g, root)->kind, K_ADD);
    T_EQ_UL(syntax89__node_at(&g, a)->kind, K_INT);
    T_EQ_UL(syntax89__node_at(&g, b)->kind, K_INT);
    syntax89_test_snapshot_free(snap);
    syntax89_destroy(&g);
}

static void test_query_is_repeatable(void)
{
    syntax89_graph g;
    syntax89_node_info info;
    syntax89_node_info again;
    syntax89_id id;

    T_OK(syntax89_init(&g, NULL));
    id = SYNTAX89_ID_NONE;
    T_OK(syntax89_add_node(&g, 5, span_of(2, 4), &id));
    T_OK(syntax89_node(&g, id, &info));
    T_OK(syntax89_node(&g, id, &again));
    T_EQ_UL(info.kind, again.kind);
    T_EQ_UL(info.span.source, again.span.source);
    T_EQ_UL(info.span.begin, again.span.begin);
    T_EQ_UL(info.span.end, again.span.end);
    syntax89_destroy(&g);
}

int main(void)
{
    test_ids_never_none();
    test_ids_survive_growth();
    test_ids_stable_across_operations();
    test_query_is_repeatable();
    return syntax89_test_report("test_ids");
}
