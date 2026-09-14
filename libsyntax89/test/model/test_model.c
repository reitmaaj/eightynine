/* test_model.c - randomized differential test against the reference model. */

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

static void run_sequence(unsigned long seed)
{
    struct syntax89_rng rng;
    struct ref_syntax89 ref;
    syntax89_graph g;
    syntax89_id id;
    syntax89_span span;
    syntax89_kind kind;
    syntax89_role role;
    unsigned long parent;
    unsigned long child;
    unsigned long root;
    unsigned long op;
    unsigned long i;

    syntax89_rng_seed(&rng, seed);
    ref_init(&ref);
    T_OK(syntax89_init(&g, NULL));
    for (i = 0; i < 400; ++i)
    {
        op = syntax89_rng_below(&rng, 10);
        if (op < 4)
        {
            if (ref.node_count < REF_MAX_NODES)
            {
                span = span_of(i, i + syntax89_rng_below(&rng, 3));
                kind = syntax89_rng_below(&rng, 4);
                id = SYNTAX89_ID_NONE;
                T_OK(syntax89_add_node(&g, kind, span, &id));
                T_EQ_UL(id, ref_add_node(&ref, kind, span));
            }
        }
        else if (op < 9)
        {
            if (ref.node_count > 0)
            {
                if (ref.edge_count < REF_MAX_EDGES)
                {
                    parent = 1 + syntax89_rng_below(&rng, ref.node_count);
                    child = 1 + syntax89_rng_below(&rng, ref.node_count);
                    role = syntax89_rng_below(&rng, 3);
                    T_OK(syntax89_add_child(&g, parent, role, child));
                    T_EQ_LONG(ref_add_child(&ref, parent, role, child), 0);
                }
            }
        }
        else
        {
            root = 0;
            if (ref.node_count > 0)
            {
                if (syntax89_rng_below(&rng, 4) != 0)
                {
                    root = 1 + syntax89_rng_below(&rng, ref.node_count);
                }
            }
            T_OK(syntax89_set_root(&g, root));
            ref_set_root(&ref, root);
        }
        T_ASSERT(ref_equals_graph(&ref, &g));
        syntax89_test_check(&g);
        if (i % 25 == 0)
        {
            T_EQ_LONG(syntax89_validate(&g, NULL), ref_validate(&ref));
        }
    }
    T_EQ_LONG(syntax89_validate(&g, NULL), ref_validate(&ref));
    if (ref_validate(&ref) == SYNTAX89_OK)
    {
        T_OK(syntax89_freeze(&g));
        T_ASSERT(ref_equals_graph(&ref, &g));
        T_OK(syntax89_validate(&g, NULL));
    }
    else
    {
        T_EQ_LONG(syntax89_freeze(&g), ref_validate(&ref));
    }
    syntax89_destroy(&g);
}

int main(void)
{
    run_sequence(1);
    run_sequence(0x1234);
    run_sequence(0xdeadbeef);
    run_sequence(0x9e3779b9);
    return syntax89_test_report("test_model");
}
