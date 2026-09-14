/* test_determinism.c - identical construction yields identical observations. */

#include "syntax89_fixtures.h"
#include "syntax89_invariants.h"
#include "syntax89_test.h"

struct log
{
    syntax89_id ids[16];
    unsigned long count;
};

static syntax89_status record(void *ctx, syntax89_id node)
{
    struct log *l;

    l = ctx;
    l->ids[l->count] = node;
    l->count += 1;
    return SYNTAX89_OK;
}

static void compare_graphs(const syntax89_graph *a, const syntax89_graph *b)
{
    syntax89_node_info ia;
    syntax89_node_info ib;
    syntax89_role ra;
    syntax89_role rb;
    syntax89_id ca;
    syntax89_id cb;
    unsigned long i;
    unsigned long j;

    T_EQ_UL(syntax89_node_count(a), syntax89_node_count(b));
    T_EQ_UL(syntax89_edge_count(a), syntax89_edge_count(b));
    T_EQ_UL(syntax89_root(a), syntax89_root(b));
    for (i = 1; i <= syntax89_node_count(a); ++i)
    {
        T_OK(syntax89_node(a, i, &ia));
        T_OK(syntax89_node(b, i, &ib));
        T_EQ_UL(ia.kind, ib.kind);
        T_EQ_UL(ia.span.source, ib.span.source);
        T_EQ_UL(ia.span.begin, ib.span.begin);
        T_EQ_UL(ia.span.end, ib.span.end);
        T_EQ_UL(syntax89_child_count(a, i), syntax89_child_count(b, i));
        for (j = 0; j < syntax89_child_count(a, i); ++j)
        {
            T_OK(syntax89_child_at(a, i, j, &ra, &ca));
            T_OK(syntax89_child_at(b, i, j, &rb, &cb));
            T_EQ_UL(ra, rb);
            T_EQ_UL(ca, cb);
        }
    }
}

static void test_identical_construction(void)
{
    syntax89_graph g1;
    syntax89_graph g2;
    struct log l1;
    struct log l2;

    syntax89_fixture_build(&g1, FX_F4, NULL);
    syntax89_fixture_build(&g2, FX_F4, NULL);
    compare_graphs(&g1, &g2);
    l1.count = 0;
    l2.count = 0;
    T_OK(syntax89_walk_nodes_pre(&g1, 1, record, &l1));
    T_OK(syntax89_walk_nodes_pre(&g2, 1, record, &l2));
    T_EQ_UL(l1.count, l2.count);
    T_EQ_UL(l1.ids[0], l2.ids[0]);
    T_EQ_UL(l1.ids[3], l2.ids[3]);
    T_EQ_UL(l1.ids[6], l2.ids[6]);
    T_EQ_LONG(syntax89_validate(&g1, NULL), syntax89_validate(&g2, NULL));
    T_OK(syntax89_freeze(&g1));
    T_OK(syntax89_freeze(&g2));
    compare_graphs(&g1, &g2);
    syntax89_destroy(&g1);
    syntax89_destroy(&g2);
}

static void test_freeze_preserves_syntax(void)
{
    syntax89_graph g;
    syntax89_node_info before;
    syntax89_node_info after;
    syntax89_role role_before;
    syntax89_role role_after;
    syntax89_id child_before;
    syntax89_id child_after;

    syntax89_fixture_build(&g, FX_F2, NULL);
    T_OK(syntax89_node(&g, 3, &before));
    T_OK(syntax89_child_at(&g, 1, 2, &role_before, &child_before));
    T_OK(syntax89_freeze(&g));
    T_OK(syntax89_node(&g, 3, &after));
    T_OK(syntax89_child_at(&g, 1, 2, &role_after, &child_after));
    T_EQ_UL(before.kind, after.kind);
    T_EQ_UL(before.span.begin, after.span.begin);
    T_EQ_UL(before.span.end, after.span.end);
    T_EQ_UL(role_before, role_after);
    T_EQ_UL(child_before, child_after);
    T_EQ_UL(syntax89_node_count(&g), 5);
    T_EQ_UL(syntax89_edge_count(&g), 4);
    syntax89_destroy(&g);
}

int main(void)
{
    test_identical_construction();
    test_freeze_preserves_syntax();
    return syntax89_test_report("test_determinism");
}
