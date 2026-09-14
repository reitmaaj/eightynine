/* test_mutations.c - generated mutation sequences with oracle comparison. */

#include "ref_syntax89.h"
#include "syntax89_gen.h"
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

static void test_mutation_sequence(unsigned long seed)
{
    struct syntax89_rng rng;
    syntax89_graph g;
    struct ref_syntax89 ref;
    struct syntax89_test_snapshot *before;
    syntax89_id id;
    syntax89_kind kind;
    syntax89_role role;
    syntax89_span span;
    syntax89_span bad;
    unsigned long n;
    unsigned long m;
    unsigned long parent;
    unsigned long child;
    unsigned long root;
    unsigned long i;

    syntax89_rng_seed(&rng, seed);
    T_OK(syntax89_init(&g, NULL));
    ref_init(&ref);
    n = 4 + syntax89_rng_below(&rng, 6);
    for (i = 0; i < n; ++i)
    {
        kind = 1 + syntax89_rng_below(&rng, 3);
        span = span_of(i, i + 1);
        id = SYNTAX89_ID_NONE;
        T_OK(syntax89_add_node(&g, kind, span, &id));
        T_EQ_UL(id, ref_add_node(&ref, kind, span));
    }
    m = syntax89_rng_below(&rng, 3 * n);
    for (i = 0; i < m; ++i)
    {
        parent = 1 + syntax89_rng_below(&rng, n);
        child = 1 + syntax89_rng_below(&rng, n);
        role = 1 + syntax89_rng_below(&rng, 3);
        T_OK(syntax89_add_child(&g, parent, role, child));
        T_EQ_LONG(ref_add_child(&ref, parent, role, child), 0);
    }
    root = 1 + syntax89_rng_below(&rng, n);
    T_OK(syntax89_set_root(&g, root));
    ref_set_root(&ref, root);
    T_ASSERT(ref_equals_graph(&ref, &g));
    T_EQ_LONG(syntax89_validate(&g, NULL), ref_validate(&ref));

    T_ASSERT(syntax89_test_snapshot_take(&g, &before) == 0);
    bad = span_of(5, 4);
    id = 123;
    T_EQ_LONG(syntax89_add_node(&g, 1, bad, &id), SYNTAX89_EINVAL);
    T_EQ_UL(id, 123);
    T_EQ_LONG(syntax89_add_child(&g, n + 1, 1, 1), SYNTAX89_ENODE);
    T_EQ_LONG(syntax89_add_child(&g, 1, 1, n + 1), SYNTAX89_ENODE);
    T_EQ_LONG(syntax89_set_root(&g, n + 1), SYNTAX89_ENODE);
    T_ASSERT(syntax89_test_snapshot_equal(before, &g) != 0);
    syntax89_test_snapshot_free(before);

    id = SYNTAX89_ID_NONE;
    T_OK(syntax89_add_node(&g, 7, span_of(0, 0), &id));
    T_EQ_UL(id, ref_add_node(&ref, 7, span_of(0, 0)));
    T_OK(syntax89_add_child(&g, root, 9, id));
    T_EQ_LONG(ref_add_child(&ref, root, 9, id), 0);
    T_ASSERT(ref_equals_graph(&ref, &g));
    T_EQ_LONG(syntax89_validate(&g, NULL), ref_validate(&ref));
    if (ref_validate(&ref) == SYNTAX89_OK)
    {
        T_OK(syntax89_freeze(&g));
        T_ASSERT(ref_equals_graph(&ref, &g));
        T_EQ_LONG(syntax89_add_node(&g, 1, span_of(0, 1), &id),
                  SYNTAX89_ESTATE);
    }
    syntax89_destroy(&g);
}

int main(void)
{
    unsigned long seed;

    for (seed = 1; seed <= 24; ++seed)
    {
        test_mutation_sequence(seed);
    }
    return syntax89_test_report("test_mutations");
}
