/* test_interleave.c - two machines interleaved behave as in isolation. */

#include "fsm89_fixtures.h"
#include "fsm89_test.h"

static void test_interleaved(void)
{
    static const fsm89_effect want1[] = {10, 11, 20, 21, 30, 31};
    static const fsm89_effect want2[] = {10, 20, 30};
    fsm89_step_result r1;
    fsm89_step_result r2;
    fsm89_state state;
    fsm89_effects enter;
    fsm89_status rc;

    rc = fsm89_start(&fsm89_fixture_m1, &state, &enter);
    fsm89_test_check_status(rc, FSM89_OK, "interleave: start m1");
    fsm89_test_check_ul(state, 0, "interleave: m1 initial");

    rc = fsm89_step(&fsm89_fixture_m1, 0, 0, &r1);
    fsm89_test_check_status(rc, FSM89_OK, "interleave: step m1");
    fsm89_test_check_ul(r1.to, 1, "interleave: m1 destination");
    fsm89_test_expect_seq(&r1, want1, 6, "interleave: m1 sequence");

    rc = fsm89_start(&fsm89_fixture_m2, &state, &enter);
    fsm89_test_check_status(rc, FSM89_OK, "interleave: start m2");
    fsm89_test_check_ul(state, 0, "interleave: m2 initial");

    rc = fsm89_step(&fsm89_fixture_m2, 0, 0, &r2);
    fsm89_test_check_status(rc, FSM89_OK, "interleave: step m2");
    fsm89_test_check_ul(r2.to, 0, "interleave: m2 destination");
    fsm89_test_expect_seq(&r2, want2, 3, "interleave: m2 sequence");

    fsm89_test_check(fsm89_accepting(&fsm89_fixture_m3, 1) != 0,
                     "interleave: m3 accepting");

    rc = fsm89_step(&fsm89_fixture_m1, 1, 0, &r1);
    fsm89_test_check_status(rc, FSM89_NO_TRANSITION,
                            "interleave: m1 terminal state");

    rc = fsm89_step(&fsm89_fixture_m2, 0, 9, &r2);
    fsm89_test_check_status(rc, FSM89_NO_TRANSITION,
                            "interleave: m2 unmatched event");

    rc = fsm89_step(&fsm89_fixture_m1, 0, 0, &r1);
    fsm89_test_check_status(rc, FSM89_OK, "interleave: m1 third step");
    fsm89_test_expect_seq(&r1, want1, 6, "interleave: m1 sequence stable");
}

int main(void)
{
    test_interleaved();
    return fsm89_test_report();
}
