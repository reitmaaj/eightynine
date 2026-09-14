/* test_freeze_atomic.c - freeze failure atomicity. */

#include "syntax89_fault_alloc.h"
#include "syntax89_fixtures.h"
#include "syntax89_invariants.h"
#include "syntax89_test.h"

static void test_freeze_defect_leaves_building(void)
{
    syntax89_graph g;
    struct syntax89_test_snapshot *before;
    int fixture;

    for (fixture = FX_F5; fixture <= FX_F7; ++fixture)
    {
        syntax89_fixture_build(&g, fixture, NULL);
        T_ASSERT(syntax89_test_snapshot_take(&g, &before) == 0);
        T_ASSERT(syntax89_freeze(&g) != SYNTAX89_OK);
        T_ASSERT(syntax89_is_frozen(&g) == 0);
        T_ASSERT(syntax89_test_snapshot_equal(before, &g) != 0);
        syntax89_test_snapshot_free(before);
        syntax89_destroy(&g);
    }
}

static void test_freeze_enomem_first_scratch(void)
{
    syntax89_graph g;
    struct syntax89_fault_alloc f;
    syntax89_allocator a;
    struct syntax89_test_snapshot *before;
    unsigned long live_before;

    syntax89_fault_alloc_init(&f);
    syntax89_fault_alloc_use(&f, &a);
    syntax89_fixture_build(&g, FX_F1, &a);
    T_ASSERT(syntax89_test_snapshot_take(&g, &before) == 0);
    live_before = f.live;
    syntax89_fault_alloc_fail_at(&f, 1);
    T_EQ_LONG(syntax89_freeze(&g), SYNTAX89_ENOMEM);
    T_ASSERT(syntax89_is_frozen(&g) == 0);
    T_EQ_UL(f.live, live_before);
    T_ASSERT(syntax89_test_snapshot_equal(before, &g) != 0);
    syntax89_test_snapshot_free(before);
    syntax89_fault_alloc_disable(&f);
    T_OK(syntax89_freeze(&g));
    syntax89_destroy(&g);
    T_EQ_UL(f.live, 0);
}

static void test_freeze_enomem_second_scratch(void)
{
    syntax89_graph g;
    struct syntax89_fault_alloc f;
    syntax89_allocator a;
    struct syntax89_test_snapshot *before;
    unsigned long live_before;

    syntax89_fault_alloc_init(&f);
    syntax89_fault_alloc_use(&f, &a);
    syntax89_fixture_build(&g, FX_F1, &a);
    T_ASSERT(syntax89_test_snapshot_take(&g, &before) == 0);
    live_before = f.live;
    syntax89_fault_alloc_fail_at(&f, 2);
    T_EQ_LONG(syntax89_freeze(&g), SYNTAX89_ENOMEM);
    T_ASSERT(syntax89_is_frozen(&g) == 0);
    T_EQ_UL(f.live, live_before);
    T_ASSERT(syntax89_test_snapshot_equal(before, &g) != 0);
    syntax89_test_snapshot_free(before);
    syntax89_fault_alloc_disable(&f);
    T_OK(syntax89_freeze(&g));
    syntax89_destroy(&g);
    T_EQ_UL(f.live, 0);
}

static void test_validate_enomem_is_atomic(void)
{
    syntax89_graph g;
    struct syntax89_fault_alloc f;
    syntax89_allocator a;
    struct syntax89_test_snapshot *before;

    syntax89_fault_alloc_init(&f);
    syntax89_fault_alloc_use(&f, &a);
    syntax89_fixture_build(&g, FX_F1, &a);
    T_ASSERT(syntax89_test_snapshot_take(&g, &before) == 0);
    syntax89_fault_alloc_fail_at(&f, 1);
    T_EQ_LONG(syntax89_validate(&g, NULL), SYNTAX89_ENOMEM);
    T_ASSERT(syntax89_test_snapshot_equal(before, &g) != 0);
    syntax89_test_snapshot_free(before);
    syntax89_fault_alloc_disable(&f);
    T_OK(syntax89_validate(&g, NULL));
    syntax89_destroy(&g);
    T_EQ_UL(f.live, 0);
}

int main(void)
{
    test_freeze_defect_leaves_building();
    test_freeze_enomem_first_scratch();
    test_freeze_enomem_second_scratch();
    test_validate_enomem_is_atomic();
    return syntax89_test_report("test_freeze_atomic");
}
