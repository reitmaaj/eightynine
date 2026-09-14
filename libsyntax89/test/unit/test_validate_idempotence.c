/* test_validate_idempotence.c - validation never mutates V01..V06. */

#include "syntax89_fixtures.h"
#include "syntax89_invariants.h"
#include "syntax89_test.h"

static void check_validate_stable(syntax89_graph *g, syntax89_status want)
{
    struct syntax89_test_snapshot *before;
    unsigned long i;

    T_ASSERT(syntax89_test_snapshot_take(g, &before) == 0);
    for (i = 0; i < 3; ++i)
    {
        T_EQ_LONG(syntax89_validate(g, NULL), want);
        T_ASSERT(syntax89_test_snapshot_equal(before, g) != 0);
    }
    syntax89_test_snapshot_free(before);
}

static void test_validate_building_valid(void)
{
    syntax89_graph g;

    syntax89_fixture_build(&g, FX_F1, NULL);
    check_validate_stable(&g, SYNTAX89_OK);
    syntax89_destroy(&g);
}

static void test_validate_building_invalid(void)
{
    syntax89_graph g;

    syntax89_fixture_build(&g, FX_F6, NULL);
    check_validate_stable(&g, SYNTAX89_ECYCLE);
    syntax89_destroy(&g);
}

static void test_validate_frozen_repeatedly(void)
{
    syntax89_graph g;

    syntax89_fixture_build(&g, FX_F4, NULL);
    T_OK(syntax89_freeze(&g));
    check_validate_stable(&g, SYNTAX89_OK);
    syntax89_destroy(&g);
}

static void test_validate_then_freeze(void)
{
    syntax89_graph g;

    syntax89_fixture_build(&g, FX_F2, NULL);
    T_OK(syntax89_validate(&g, NULL));
    T_OK(syntax89_freeze(&g));
    T_OK(syntax89_validate(&g, NULL));
    syntax89_destroy(&g);
}

static void test_freeze_then_validate(void)
{
    syntax89_graph g;

    syntax89_fixture_build(&g, FX_F2, NULL);
    T_OK(syntax89_freeze(&g));
    T_OK(syntax89_validate(&g, NULL));
    syntax89_destroy(&g);
}

int main(void)
{
    test_validate_building_valid();
    test_validate_building_invalid();
    test_validate_frozen_repeatedly();
    test_validate_then_freeze();
    test_freeze_then_validate();
    return syntax89_test_report("test_validate_idempotence");
}
