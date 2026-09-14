/* test_oracle.c - exhaustive small graphs and random oracle comparison. */

#include "ref_syntax89.h"
#include "syntax89_gen.h"
#include "syntax89_test.h"

static void run_exhaustive(unsigned long n)
{
    syntax89_graph g;
    struct ref_syntax89 ref;
    syntax89_span span;
    syntax89_id id;
    unsigned long edges;
    unsigned long masks;
    unsigned long mask;
    unsigned long count;
    unsigned long e;
    unsigned long i;
    unsigned long j;

    span.source = 1;
    span.begin = 0;
    span.end = 1;
    edges = n * n;
    masks = 1UL << edges;
    for (mask = 0; mask < masks; ++mask)
    {
        T_OK(syntax89_init(&g, NULL));
        ref_init(&ref);
        for (i = 0; i < n; ++i)
        {
            id = SYNTAX89_ID_NONE;
            T_OK(syntax89_add_node(&g, i + 1, span, &id));
            T_EQ_UL(id, ref_add_node(&ref, i + 1, span));
        }
        count = 0;
        e = 0;
        for (i = 0; i < n; ++i)
        {
            for (j = 0; j < n; ++j)
            {
                if ((mask & (1UL << e)) != 0)
                {
                    T_OK(syntax89_add_child(&g, i + 1, 1, j + 1));
                    T_EQ_LONG(ref_add_child(&ref, i + 1, 1, j + 1), 0);
                    count += 1;
                }
                e += 1;
            }
        }
        T_OK(syntax89_set_root(&g, 1));
        ref_set_root(&ref, 1);
        T_EQ_UL(syntax89_edge_count(&g), count);
        T_EQ_LONG(syntax89_validate(&g, NULL), ref_validate(&ref));
        syntax89_destroy(&g);
    }
}

static void run_random_oracle(unsigned long seed, unsigned long n)
{
    struct syntax89_rng rng;
    syntax89_graph g;
    struct ref_syntax89 ref;
    syntax89_span span;
    syntax89_id id;
    unsigned long parent;
    unsigned long child;
    unsigned long root;
    unsigned long i;

    syntax89_rng_seed(&rng, seed);
    span.source = 1;
    span.begin = 0;
    span.end = 1;
    T_OK(syntax89_init(&g, NULL));
    ref_init(&ref);
    for (i = 0; i < n; ++i)
    {
        id = SYNTAX89_ID_NONE;
        T_OK(syntax89_add_node(&g, 1, span, &id));
        T_EQ_UL(id, ref_add_node(&ref, 1, span));
    }
    for (i = 0; i < n * 2; ++i)
    {
        parent = 1 + syntax89_rng_below(&rng, n);
        child = 1 + syntax89_rng_below(&rng, n);
        T_OK(syntax89_add_child(&g, parent, 1, child));
        T_EQ_LONG(ref_add_child(&ref, parent, 1, child), 0);
    }
    root = 1 + syntax89_rng_below(&rng, n);
    T_OK(syntax89_set_root(&g, root));
    ref_set_root(&ref, root);
    T_EQ_LONG(syntax89_validate(&g, NULL), ref_validate(&ref));
    syntax89_destroy(&g);
}

int main(void)
{
    unsigned long seed;

    run_exhaustive(1);
    run_exhaustive(2);
    run_exhaustive(3);
    run_exhaustive(4);
    for (seed = 1; seed <= 8; ++seed)
    {
        run_random_oracle(seed, 5 + seed % 4);
    }
    return syntax89_test_report("test_oracle");
}
