/* test_counts.c - exact node and edge counts per fixture. */

#include "syntax89_fixtures.h"
#include "syntax89_invariants.h"
#include "syntax89_test.h"

static void check_fixture(int fixture, unsigned long nodes, unsigned long edges)
{
    syntax89_graph g;

    syntax89_fixture_build(&g, fixture, NULL);
    T_EQ_UL(syntax89_node_count(&g), nodes);
    T_EQ_UL(syntax89_edge_count(&g), edges);
    syntax89_test_check(&g);
    syntax89_destroy(&g);
}

static void test_fixture_counts(void)
{
    check_fixture(FX_F0, 1, 0);
    check_fixture(FX_F1, 3, 2);
    check_fixture(FX_F2, 5, 4);
    check_fixture(FX_F3, 2, 2);
    check_fixture(FX_F4, 7, 6);
    check_fixture(FX_F5, 3, 1);
    check_fixture(FX_F6, 1, 1);
    check_fixture(FX_F7, 3, 3);
}

static void test_counts_unchanged_by_failed_mutators(void)
{
    syntax89_graph g;
    syntax89_id id;
    syntax89_span span;

    syntax89_fixture_build(&g, FX_F1, NULL);
    span.source = 1;
    span.begin = 0;
    span.end = 1;
    id = SYNTAX89_ID_NONE;
    T_EQ_LONG(syntax89_add_child(&g, 1, R_ITEM, 99), SYNTAX89_ENODE);
    T_EQ_LONG(syntax89_add_child(&g, 99, R_ITEM, 1), SYNTAX89_ENODE);
    T_EQ_LONG(syntax89_set_root(&g, 99), SYNTAX89_ENODE);
    T_EQ_UL(syntax89_node_count(&g), 3);
    T_EQ_UL(syntax89_edge_count(&g), 2);
    T_EQ_LONG(syntax89_add_node(&g, K_INT, span, &id), SYNTAX89_OK);
    T_EQ_LONG(syntax89_add_child(&g, 1, R_ITEM, id), SYNTAX89_OK);
    T_EQ_UL(syntax89_node_count(&g), 4);
    T_EQ_UL(syntax89_edge_count(&g), 3);
    T_OK(syntax89_freeze(&g));
    T_EQ_LONG(syntax89_add_child(&g, 1, R_ITEM, 2), SYNTAX89_ESTATE);
    T_EQ_LONG(syntax89_add_node(&g, K_INT, span, &id), SYNTAX89_ESTATE);
    T_EQ_LONG(syntax89_set_root(&g, 2), SYNTAX89_ESTATE);
    T_EQ_UL(syntax89_node_count(&g), 4);
    T_EQ_UL(syntax89_edge_count(&g), 3);
    syntax89_destroy(&g);
}

int main(void)
{
    test_fixture_counts();
    test_counts_unchanged_by_failed_mutators();
    return syntax89_test_report("test_counts");
}
