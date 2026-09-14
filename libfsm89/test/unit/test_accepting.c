/* test_accepting.c - accepting is a state property, not terminality. */

#include "fsm89_fixtures.h"
#include "fsm89_test.h"

static void test_basic(void)
{
    fsm89_test_check(fsm89_accepting(&fsm89_fixture_m3, 0) == 0,
                     "accepting: non-accepting state");
    fsm89_test_check(fsm89_accepting(&fsm89_fixture_m3, 1) != 0,
                     "accepting: accepting state");
    fsm89_test_check(fsm89_accepting(&fsm89_fixture_m3, 2) != 0,
                     "accepting: second accepting state");
}

static void test_accepting_with_outgoing(void)
{
    fsm89_status rc;

    rc = fsm89_validate(&fsm89_fixture_m3);
    fsm89_test_check_status(rc, FSM89_OK, "accepting: outgoing validates");
    fsm89_test_check(fsm89_accepting(&fsm89_fixture_m3, 1) != 0,
                     "accepting: outgoing state still accepting");
}

static void test_accepting_self_loop(void)
{
    static const fsm89_state_def states[] = {{0, {NULL, 0}, {NULL, 0}, 1}};
    static const fsm89_edge edges[] = {{0, 0, 0, {NULL, 0}}};
    fsm89_def def;
    fsm89_status rc;

    def.states = states;
    def.state_count = 1;
    def.edges = edges;
    def.edge_count = 1;
    def.initial = 0;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_OK, "accepting: self-loop validates");
    fsm89_test_check(fsm89_accepting(&def, 0) != 0,
                     "accepting: self-loop state accepting");
}

static void test_initial(void)
{
    static const fsm89_state_def states[] = {{7, {NULL, 0}, {NULL, 0}, 1}};
    fsm89_def def;

    def.states = states;
    def.state_count = 1;
    def.edges = NULL;
    def.edge_count = 0;
    def.initial = 7;
    fsm89_test_check(fsm89_accepting(&def, 7) != 0,
                     "accepting: accepting initial state");
}

static void test_transitions_do_not_change_acceptance(void)
{
    fsm89_step_result r;
    fsm89_status rc;

    rc = fsm89_step(&fsm89_fixture_m3, 0, 0, &r);
    fsm89_test_check_status(rc, FSM89_OK,
                            "accepting: nonaccepting to accepting");
    fsm89_test_check_ul(r.to, 1, "accepting: destination");
    fsm89_test_check(fsm89_accepting(&fsm89_fixture_m3, 0) == 0,
                     "accepting: source unchanged");

    rc = fsm89_step(&fsm89_fixture_m3, 1, 1, &r);
    fsm89_test_check_status(rc, FSM89_OK, "accepting: accepting to accepting");
    fsm89_test_check(fsm89_accepting(&fsm89_fixture_m3, 2) != 0,
                     "accepting: destination accepting");
}

static void test_unknown_state(void)
{
    fsm89_test_check(fsm89_accepting(&fsm89_fixture_m1, 9) == 0,
                     "accepting: unknown state");
    fsm89_test_check(fsm89_accepting(&fsm89_fixture_m1, (unsigned long)-1) == 0,
                     "accepting: ULONG_MAX state");
}

int main(void)
{
    test_basic();
    test_accepting_with_outgoing();
    test_accepting_self_loop();
    test_initial();
    test_transitions_do_not_change_acceptance();
    test_unknown_state();
    return fsm89_test_report();
}
