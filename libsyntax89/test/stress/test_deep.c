/* test_deep.c - deep chains must not consume C stack. */

#include "syntax89_fixtures.h"
#include "syntax89_invariants.h"
#include "syntax89_test.h"

#ifdef SYNTAX89_STRESS_DEEP
#define DEEP_DEPTH 100000UL
#else
#define DEEP_DEPTH 1000UL
#endif

static syntax89_status count_visit(void *ctx, syntax89_id node)
{
    unsigned long *n;

    n = ctx;
    (void)node;
    *n += 1;
    return SYNTAX89_OK;
}

static void run_chain(unsigned long depth)
{
    syntax89_graph g;
    syntax89_id prev;
    syntax89_id id;
    unsigned long i;
    unsigned long seen;

    T_OK(syntax89_init(&g, NULL));
    prev = syntax89_fixture_node(&g, K_NAME, 0, 1);
    T_OK(syntax89_set_root(&g, prev));
    for (i = 0; i < depth; ++i)
    {
        id = syntax89_fixture_node(&g, K_NAME, i + 1, i + 2);
        T_OK(syntax89_add_child(&g, prev, R_ITEM, id));
        prev = id;
    }
    T_EQ_UL(syntax89_node_count(&g), depth + 1);
    T_EQ_UL(syntax89_edge_count(&g), depth);
    T_OK(syntax89_validate(&g, NULL));
    T_OK(syntax89_freeze(&g));
    seen = 0;
    T_OK(syntax89_walk_nodes_pre(&g, 1, count_visit, &seen));
    T_EQ_UL(seen, depth + 1);
    seen = 0;
    T_OK(syntax89_walk_nodes_post(&g, 1, count_visit, &seen));
    T_EQ_UL(seen, depth + 1);
    seen = 0;
    T_OK(syntax89_walk_edges_post(&g, 1, count_visit, &seen));
    T_EQ_UL(seen, depth + 1);
    syntax89_destroy(&g);
}

int main(void)
{
    run_chain(1);
    run_chain(10);
    run_chain(100);
    run_chain(DEEP_DEPTH);
    return syntax89_test_report("test_deep");
}
