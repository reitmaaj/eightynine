/* test_model.c - differential tests against the reference model. */

#include "fsm89_fixtures.h"
#include "fsm89_gen.h"
#include "fsm89_test.h"
#include "ref_fsm89.h"

static void compare_validate(const fsm89_def *def, const char *label)
{
    fsm89_test_check_status(fsm89_validate(def), ref_validate(def), label);
}

static void test_fixtures(void)
{
    compare_validate(&fsm89_fixture_m0, "model: m0 validate");
    compare_validate(&fsm89_fixture_m1, "model: m1 validate");
    compare_validate(&fsm89_fixture_m2, "model: m2 validate");
    compare_validate(&fsm89_fixture_m3, "model: m3 validate");
    compare_validate(&fsm89_fixture_m4, "model: m4 validate");
    compare_validate(&fsm89_fixture_m5, "model: m5 validate");
    compare_validate(&fsm89_fixture_m6, "model: m6 validate");

    fsm89_gen_compare(&fsm89_fixture_m0, 1, 1, "model: m0 step");
    fsm89_gen_compare(&fsm89_fixture_m1, 2, 1, "model: m1 step");
    fsm89_gen_compare(&fsm89_fixture_m2, 1, 1, "model: m2 step");
    fsm89_gen_compare(&fsm89_fixture_m3, 3, 2, "model: m3 step");
    fsm89_gen_compare(&fsm89_fixture_m4, 4, 2, "model: m4 step");
    fsm89_gen_compare(&fsm89_fixture_m5, 2, 1, "model: m5 step");
    fsm89_gen_compare(&fsm89_fixture_m6, 3, 2, "model: m6 step");
}

static void test_random(void)
{
    fsm89_gen gen;
    unsigned long seed;
    size_t nstates;
    size_t nevents;
    int i;

    seed = 0x9E3779B9UL;
    for (i = 0; i < 200; ++i)
    {
        nstates = 1 + (size_t)(fsm89_test_rand(&seed) %
                               (unsigned long)FSM89_GEN_MAX_STATES);
        nevents = 1 + (size_t)(fsm89_test_rand(&seed) %
                               (unsigned long)FSM89_GEN_MAX_EVENTS);
        fsm89_gen_random(&gen, &seed, nstates, nevents);
        fsm89_test_check_status(fsm89_validate(&gen.def), FSM89_OK,
                                "model: random machine validates");
        fsm89_test_check_status(ref_validate(&gen.def), FSM89_OK,
                                "model: reference agrees on random machine");
        fsm89_gen_compare(&gen.def, nstates, nevents, "model: random step");
    }
}

int main(void)
{
    test_fixtures();
    test_random();
    return fsm89_test_report();
}
