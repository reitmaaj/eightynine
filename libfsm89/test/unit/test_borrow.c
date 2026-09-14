/* test_borrow.c - returned spans borrow definition storage. */

#include "fsm89_fixtures.h"
#include "fsm89_test.h"

static void test_ordinary(void)
{
    fsm89_step_result r;
    fsm89_status rc;

    rc = fsm89_step(&fsm89_fixture_m1, 0, 0, &r);
    fsm89_test_check_status(rc, FSM89_OK, "borrow: ordinary");
    fsm89_test_check_span(r.leave, fsm89_fixture_m1.states[0].leave.v, 2,
                          "borrow: leave storage");
    fsm89_test_check_span(r.edge, fsm89_fixture_m1.edges[0].effects.v, 2,
                          "borrow: edge storage");
    fsm89_test_check_span(r.enter, fsm89_fixture_m1.states[1].enter.v, 2,
                          "borrow: enter storage");
}

static void test_startup(void)
{
    fsm89_state state;
    fsm89_effects enter;
    fsm89_status rc;

    rc = fsm89_start(&fsm89_fixture_m0, &state, &enter);
    fsm89_test_check_status(rc, FSM89_OK, "borrow: startup");
    fsm89_test_check_span(enter, fsm89_fixture_m0.states[0].enter.v, 1,
                          "borrow: startup enter storage");
}

static void test_self(void)
{
    fsm89_step_result r;
    fsm89_status rc;

    rc = fsm89_step(&fsm89_fixture_m2, 0, 0, &r);
    fsm89_test_check_status(rc, FSM89_OK, "borrow: self");
    fsm89_test_check_span(r.leave, fsm89_fixture_m2.states[0].leave.v, 1,
                          "borrow: self leave storage");
    fsm89_test_check_span(r.enter, fsm89_fixture_m2.states[0].enter.v, 1,
                          "borrow: self enter storage");
}

static void test_stable_across_calls(void)
{
    fsm89_step_result first;
    fsm89_step_result second;
    fsm89_status rc;

    rc = fsm89_step(&fsm89_fixture_m1, 0, 0, &first);
    fsm89_test_check_status(rc, FSM89_OK, "borrow: first call");
    rc = fsm89_step(&fsm89_fixture_m2, 0, 0, &second);
    fsm89_test_check_status(rc, FSM89_OK, "borrow: second call");
    fsm89_test_check_span(first.leave, fsm89_fixture_m1.states[0].leave.v, 2,
                          "borrow: first leave still borrowed");
    fsm89_test_check_span(first.edge, fsm89_fixture_m1.edges[0].effects.v, 2,
                          "borrow: first edge still borrowed");
    fsm89_test_check_span(first.enter, fsm89_fixture_m1.states[1].enter.v, 2,
                          "borrow: first enter still borrowed");
    fsm89_test_check_span(second.leave, fsm89_fixture_m2.states[0].leave.v, 1,
                          "borrow: second leave borrowed");
}

int main(void)
{
    test_ordinary();
    test_startup();
    test_self();
    test_stable_across_calls();
    return fsm89_test_report();
}
