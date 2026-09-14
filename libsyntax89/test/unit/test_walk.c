/* test_walk.c - traversal T01..T09. */

#include <string.h>

#include "syntax89_fixtures.h"
#include "syntax89_invariants.h"
#include "syntax89_test.h"

struct visit_log
{
    syntax89_id ids[16];
    unsigned long count;
    unsigned long stop_after;
    syntax89_status result;
};

static syntax89_status visit_record(void *ctx, syntax89_id node)
{
    struct visit_log *log;

    log = ctx;
    log->ids[log->count] = node;
    log->count += 1;
    if (log->stop_after != 0)
    {
        if (log->count >= log->stop_after)
        {
            return log->result;
        }
    }
    return SYNTAX89_OK;
}

static void test_single_node(void)
{
    syntax89_graph g;
    struct visit_log log;

    syntax89_fixture_build(&g, FX_F0, NULL);
    log.count = 0;
    log.stop_after = 0;
    log.result = SYNTAX89_OK;
    T_OK(syntax89_walk_nodes_pre(&g, 1, visit_record, &log));
    T_EQ_UL(log.count, 1);
    T_EQ_UL(log.ids[0], 1);
    log.count = 0;
    T_OK(syntax89_walk_nodes_post(&g, 1, visit_record, &log));
    T_EQ_UL(log.count, 1);
    T_EQ_UL(log.ids[0], 1);
    syntax89_destroy(&g);
}

static void test_tree_orders(void)
{
    syntax89_graph g;
    struct visit_log log;

    syntax89_fixture_build(&g, FX_F1, NULL);
    log.count = 0;
    log.stop_after = 0;
    log.result = SYNTAX89_OK;
    T_OK(syntax89_walk_nodes_pre(&g, 1, visit_record, &log));
    T_EQ_UL(log.count, 3);
    T_EQ_UL(log.ids[0], 1);
    T_EQ_UL(log.ids[1], 2);
    T_EQ_UL(log.ids[2], 3);
    log.count = 0;
    T_OK(syntax89_walk_nodes_post(&g, 1, visit_record, &log));
    T_EQ_UL(log.count, 3);
    T_EQ_UL(log.ids[0], 2);
    T_EQ_UL(log.ids[1], 3);
    T_EQ_UL(log.ids[2], 1);
    syntax89_destroy(&g);
}

static void test_dag_node_once(void)
{
    syntax89_graph g;
    syntax89_id a;
    syntax89_id b;
    syntax89_id c;
    syntax89_id d;
    struct visit_log log;

    T_OK(syntax89_init(&g, NULL));
    a = syntax89_fixture_node(&g, K_ADD, 0, 1);
    b = syntax89_fixture_node(&g, K_NAME, 1, 2);
    c = syntax89_fixture_node(&g, K_NAME, 2, 3);
    d = syntax89_fixture_node(&g, K_NAME, 3, 4);
    T_OK(syntax89_add_child(&g, a, R_LEFT, b));
    T_OK(syntax89_add_child(&g, a, R_RIGHT, c));
    T_OK(syntax89_add_child(&g, b, R_LEFT, d));
    T_OK(syntax89_add_child(&g, c, R_LEFT, d));
    log.count = 0;
    log.stop_after = 0;
    log.result = SYNTAX89_OK;
    T_OK(syntax89_walk_nodes_pre(&g, a, visit_record, &log));
    T_EQ_UL(log.count, 4);
    T_EQ_UL(log.ids[0], a);
    T_EQ_UL(log.ids[1], b);
    T_EQ_UL(log.ids[2], d);
    T_EQ_UL(log.ids[3], c);
    log.count = 0;
    T_OK(syntax89_walk_nodes_post(&g, a, visit_record, &log));
    T_EQ_UL(log.count, 4);
    T_EQ_UL(log.ids[0], d);
    T_EQ_UL(log.ids[1], b);
    T_EQ_UL(log.ids[2], c);
    T_EQ_UL(log.ids[3], a);
    syntax89_destroy(&g);
}

static void test_dag_occurrences(void)
{
    syntax89_graph g;
    syntax89_id a;
    syntax89_id b;
    syntax89_id c;
    syntax89_id d;
    struct visit_log log;

    T_OK(syntax89_init(&g, NULL));
    a = syntax89_fixture_node(&g, K_ADD, 0, 1);
    b = syntax89_fixture_node(&g, K_NAME, 1, 2);
    c = syntax89_fixture_node(&g, K_NAME, 2, 3);
    d = syntax89_fixture_node(&g, K_NAME, 3, 4);
    T_OK(syntax89_add_child(&g, a, R_LEFT, b));
    T_OK(syntax89_add_child(&g, a, R_RIGHT, c));
    T_OK(syntax89_add_child(&g, b, R_LEFT, d));
    T_OK(syntax89_add_child(&g, c, R_LEFT, d));
    log.count = 0;
    log.stop_after = 0;
    log.result = SYNTAX89_OK;
    T_OK(syntax89_walk_edges_pre(&g, a, visit_record, &log));
    T_EQ_UL(log.count, 5);
    T_EQ_UL(log.ids[0], a);
    T_EQ_UL(log.ids[1], b);
    T_EQ_UL(log.ids[2], d);
    T_EQ_UL(log.ids[3], c);
    T_EQ_UL(log.ids[4], d);
    log.count = 0;
    T_OK(syntax89_walk_edges_post(&g, a, visit_record, &log));
    T_EQ_UL(log.count, 5);
    T_EQ_UL(log.ids[0], d);
    T_EQ_UL(log.ids[1], b);
    T_EQ_UL(log.ids[2], d);
    T_EQ_UL(log.ids[3], c);
    T_EQ_UL(log.ids[4], a);
    syntax89_destroy(&g);
}

static void test_sibling_order(void)
{
    syntax89_graph g;
    struct visit_log log;

    syntax89_fixture_build(&g, FX_F2, NULL);
    log.count = 0;
    log.stop_after = 0;
    log.result = SYNTAX89_OK;
    T_OK(syntax89_walk_nodes_pre(&g, 1, visit_record, &log));
    T_EQ_UL(log.count, 5);
    T_EQ_UL(log.ids[0], 1);
    T_EQ_UL(log.ids[1], 2);
    T_EQ_UL(log.ids[2], 3);
    T_EQ_UL(log.ids[3], 4);
    T_EQ_UL(log.ids[4], 5);
    syntax89_destroy(&g);
}

static void test_callback_early_stop(void)
{
    syntax89_graph g;
    struct visit_log log;

    syntax89_fixture_build(&g, FX_F2, NULL);
    log.count = 0;
    log.stop_after = 2;
    log.result = SYNTAX89_END;
    T_OK(syntax89_walk_nodes_pre(&g, 1, visit_record, &log));
    T_EQ_UL(log.count, 2);
    log.count = 0;
    log.stop_after = 1;
    log.result = SYNTAX89_END;
    T_OK(syntax89_walk_nodes_pre(&g, 1, visit_record, &log));
    T_EQ_UL(log.count, 1);
    log.count = 0;
    T_OK(syntax89_walk_edges_pre(&g, 1, visit_record, &log));
    T_EQ_UL(log.count, 1);
    syntax89_destroy(&g);
}

static void test_callback_error(void)
{
    syntax89_graph g;
    struct visit_log log;

    syntax89_fixture_build(&g, FX_F2, NULL);
    log.count = 0;
    log.stop_after = 2;
    log.result = SYNTAX89_EINVAL;
    T_EQ_LONG(syntax89_walk_nodes_pre(&g, 1, visit_record, &log),
              SYNTAX89_EINVAL);
    T_EQ_UL(log.count, 2);
    syntax89_destroy(&g);
}

static void test_callback_other_positive(void)
{
    syntax89_graph g;
    struct visit_log log;

    syntax89_fixture_build(&g, FX_F2, NULL);
    log.count = 0;
    log.stop_after = 1;
    log.result = 7;
    T_EQ_LONG(syntax89_walk_nodes_pre(&g, 1, visit_record, &log),
              SYNTAX89_EINVAL);
    syntax89_destroy(&g);
}

static void test_walk_preconditions(void)
{
    syntax89_graph g;
    struct visit_log log;

    syntax89_fixture_build(&g, FX_F2, NULL);
    log.count = 0;
    log.stop_after = 0;
    log.result = SYNTAX89_OK;
    T_EQ_LONG(syntax89_walk_nodes_pre(NULL, 1, visit_record, &log),
              SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_walk_nodes_pre(&g, 1, NULL, &log), SYNTAX89_EINVAL);
    T_EQ_LONG(syntax89_walk_nodes_pre(&g, SYNTAX89_ID_NONE, visit_record, &log),
              SYNTAX89_ENODE);
    T_EQ_LONG(syntax89_walk_nodes_pre(&g, 99, visit_record, &log),
              SYNTAX89_ENODE);
    T_EQ_LONG(syntax89_walk_edges_pre(&g, 99, visit_record, &log),
              SYNTAX89_ENODE);
    syntax89_destroy(&g);
}

static void test_walk_cycle(void)
{
    syntax89_graph g;
    struct visit_log log;

    syntax89_fixture_build(&g, FX_F7, NULL);
    log.count = 0;
    log.stop_after = 0;
    log.result = SYNTAX89_OK;
    T_EQ_LONG(syntax89_walk_nodes_pre(&g, 1, visit_record, &log),
              SYNTAX89_ECYCLE);
    log.count = 0;
    T_EQ_LONG(syntax89_walk_nodes_post(&g, 1, visit_record, &log),
              SYNTAX89_ECYCLE);
    log.count = 0;
    T_EQ_LONG(syntax89_walk_edges_pre(&g, 1, visit_record, &log),
              SYNTAX89_ECYCLE);
    log.count = 0;
    T_EQ_LONG(syntax89_walk_edges_post(&g, 1, visit_record, &log),
              SYNTAX89_ECYCLE);
    syntax89_destroy(&g);
}

static void test_walk_uninit(void)
{
    syntax89_graph g;
    struct visit_log log;

    memset(&g, 0, sizeof(g));
    log.count = 0;
    log.stop_after = 0;
    log.result = SYNTAX89_OK;
    T_EQ_LONG(syntax89_walk_nodes_pre(&g, 1, visit_record, &log),
              SYNTAX89_ESTATE);
    T_EQ_LONG(syntax89_walk_edges_post(&g, 1, visit_record, &log),
              SYNTAX89_ESTATE);
}

static void test_postorder_callback_error(void)
{
    syntax89_graph g;
    struct visit_log log;

    syntax89_fixture_build(&g, FX_F2, NULL);
    log.count = 0;
    log.stop_after = 1;
    log.result = SYNTAX89_EINVAL;
    T_EQ_LONG(syntax89_walk_nodes_post(&g, 1, visit_record, &log),
              SYNTAX89_EINVAL);
    T_EQ_UL(log.count, 1);
    syntax89_destroy(&g);
}

int main(void)
{
    test_single_node();
    test_tree_orders();
    test_dag_node_once();
    test_dag_occurrences();
    test_sibling_order();
    test_callback_early_stop();
    test_callback_error();
    test_callback_other_positive();
    test_walk_preconditions();
    test_walk_cycle();
    test_walk_uninit();
    test_postorder_callback_error();
    return syntax89_test_report("test_walk");
}
