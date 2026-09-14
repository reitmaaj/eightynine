/* test_shared.c - layered sharing must not enumerate paths. */

#include "syntax89_fixtures.h"
#include "syntax89_invariants.h"
#include "syntax89_test.h"

#define LAYERS 30
#define WIDTH 3

static void run_layered(void)
{
    syntax89_graph g;
    syntax89_id nodes[LAYERS][WIDTH];
    syntax89_id root;
    unsigned long l;
    unsigned long i;
    unsigned long j;

    T_OK(syntax89_init(&g, NULL));
    for (l = 0; l < LAYERS; ++l)
    {
        for (i = 0; i < WIDTH; ++i)
        {
            nodes[l][i] = syntax89_fixture_node(&g, K_NAME, l, l + 1);
        }
    }
    root = syntax89_fixture_node(&g, K_BLOCK, 0, 1);
    for (i = 0; i < WIDTH; ++i)
    {
        T_OK(syntax89_add_child(&g, root, R_ITEM, nodes[0][i]));
    }
    for (l = 0; l + 1 < LAYERS; ++l)
    {
        for (i = 0; i < WIDTH; ++i)
        {
            for (j = 0; j < WIDTH; ++j)
            {
                T_OK(syntax89_add_child(&g, nodes[l][i], R_ITEM,
                                        nodes[l + 1][j]));
            }
        }
    }
    T_OK(syntax89_set_root(&g, root));
    T_EQ_UL(syntax89_node_count(&g), LAYERS * WIDTH + 1);
    T_EQ_UL(syntax89_edge_count(&g), (LAYERS - 1) * WIDTH * WIDTH + WIDTH);
    T_OK(syntax89_validate(&g, NULL));
    T_OK(syntax89_freeze(&g));
    syntax89_destroy(&g);
}

static void run_fan_in(void)
{
    syntax89_graph g;
    syntax89_id root;
    syntax89_id leaf;
    syntax89_id id;
    unsigned long i;

    T_OK(syntax89_init(&g, NULL));
    root = syntax89_fixture_node(&g, K_CALL, 0, 1);
    leaf = syntax89_fixture_node(&g, K_NAME, 1, 2);
    for (i = 0; i < 10000; ++i)
    {
        id = syntax89_fixture_node(&g, K_INT, i, i + 1);
        T_OK(syntax89_add_child(&g, root, R_ARG, id));
        T_OK(syntax89_add_child(&g, id, R_ITEM, leaf));
    }
    T_OK(syntax89_set_root(&g, root));
    T_OK(syntax89_validate(&g, NULL));
    T_OK(syntax89_freeze(&g));
    T_EQ_UL(syntax89_edge_count(&g), 20000);
    syntax89_destroy(&g);
}

int main(void)
{
    run_layered();
    run_fan_in();
    return syntax89_test_report("test_shared");
}
