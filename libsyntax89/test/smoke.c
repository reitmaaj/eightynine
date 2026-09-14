/* smoke.c - one end-to-end path: build, freeze, query, walk, destroy. */

#include "syntax89_fixtures.h"
#include "syntax89_invariants.h"
#include "syntax89_test.h"

static syntax89_id pre_order[8];
static syntax89_id post_order[8];

static syntax89_status record_pre(void *ctx, syntax89_id node)
{
    unsigned long *n;

    n = ctx;
    pre_order[*n] = node;
    *n += 1;
    return SYNTAX89_OK;
}

static syntax89_status record_post(void *ctx, syntax89_id node)
{
    unsigned long *n;

    n = ctx;
    post_order[*n] = node;
    *n += 1;
    return SYNTAX89_OK;
}

int main(void)
{
    syntax89_graph g;
    syntax89_id root;
    syntax89_node_info info;
    syntax89_role role;
    syntax89_id child;
    unsigned long pre_count;
    unsigned long post_count;

    root = syntax89_fixture_build(&g, FX_F2, NULL);
    syntax89_test_check(&g);
    T_EQ_UL(root, 1);
    T_EQ_UL(syntax89_root(&g), root);
    T_EQ_UL(syntax89_node_count(&g), 5);
    T_EQ_UL(syntax89_edge_count(&g), 4);
    T_OK(syntax89_node(&g, root, &info));
    T_EQ_UL(info.kind, K_CALL);
    T_EQ_UL(info.span.begin, 0);
    T_EQ_UL(info.span.end, 8);
    T_EQ_UL(syntax89_child_count(&g, root), 4);
    T_OK(syntax89_child_at(&g, root, 0, &role, &child));
    T_EQ_UL(role, R_CALLEE);
    T_OK(syntax89_child_at_role(&g, root, R_ARG, 1, &child));
    T_EQ_UL(child, 4);
    T_EQ_UL(syntax89_child_count_role(&g, root, R_ARG), 3);
    T_OK(syntax89_freeze(&g));
    T_ASSERT(syntax89_is_frozen(&g) != 0);
    syntax89_test_check(&g);
    pre_count = 0;
    post_count = 0;
    T_OK(syntax89_walk_nodes_pre(&g, root, record_pre, &pre_count));
    T_OK(syntax89_walk_nodes_post(&g, root, record_post, &post_count));
    T_EQ_UL(pre_count, 5);
    T_EQ_UL(post_count, 5);
    T_EQ_UL(pre_order[0], 1);
    T_EQ_UL(pre_order[4], 5);
    T_EQ_UL(post_order[0], 2);
    T_EQ_UL(post_order[4], 1);
    syntax89_destroy(&g);
    return syntax89_test_report("smoke");
}
