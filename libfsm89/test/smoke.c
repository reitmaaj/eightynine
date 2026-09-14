/* smoke.c - one end-to-end path: validate, start, step, flatten, accept. */

#include "fsm89_fixtures.h"
#include "fsm89_test.h"

int main(void)
{
    static const fsm89_effect want[] = {10, 11, 20, 21, 30, 31};
    fsm89_status rc;
    fsm89_state state;
    fsm89_effects enter;
    fsm89_step_result r;

    rc = fsm89_validate(&fsm89_fixture_m1);
    fsm89_test_check_status(rc, FSM89_OK, "smoke: m1 validates");

    rc = fsm89_start(&fsm89_fixture_m1, &state, &enter);
    fsm89_test_check_status(rc, FSM89_OK, "smoke: start succeeds");
    fsm89_test_check_ul(state, 0, "smoke: start returns the initial state");
    fsm89_test_check_size(enter.n, 0, "smoke: initial enter span is empty");

    rc = fsm89_step(&fsm89_fixture_m1, state, 0, &r);
    fsm89_test_check_status(rc, FSM89_OK, "smoke: step succeeds");
    fsm89_test_check_ul(r.to, 1, "smoke: step reaches the destination");
    fsm89_test_expect_seq(&r, want, 6, "smoke: emission order");

    rc = fsm89_step(&fsm89_fixture_m1, state, 1, &r);
    fsm89_test_check_status(rc, FSM89_NO_TRANSITION, "smoke: unmatched event");

    rc = fsm89_validate(&fsm89_fixture_m3);
    fsm89_test_check_status(rc, FSM89_OK, "smoke: m3 validates");
    fsm89_test_check(fsm89_accepting(&fsm89_fixture_m3, 1) != 0,
                     "smoke: accepting state");
    fsm89_test_check(fsm89_accepting(&fsm89_fixture_m3, 0) == 0,
                     "smoke: non-accepting state");

    return fsm89_test_report();
}
