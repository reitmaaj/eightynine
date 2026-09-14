/* test_missing.c - missing transitions, unknown states, unchanged results. */

#include "fsm89_fixtures.h"
#include "fsm89_test.h"

static void prefill(fsm89_step_result *r)
{
    r->from = 0x1111UL;
    r->event = 0x2222UL;
    r->to = 0x3333UL;
    r->leave.v = (const fsm89_effect *)r;
    r->leave.n = 0x4444UL;
    r->edge.v = (const fsm89_effect *)r;
    r->edge.n = 0x5555UL;
    r->enter.v = (const fsm89_effect *)r;
    r->enter.n = 0x6666UL;
}

static void test_no_outgoing(void)
{
    fsm89_step_result r;
    fsm89_test_snapshot before;
    fsm89_status rc;

    prefill(&r);
    fsm89_test_snapshot_take(&before, &r);
    rc = fsm89_step(&fsm89_fixture_m0, 0, 0, &r);
    fsm89_test_check_status(rc, FSM89_NO_TRANSITION,
                            "missing: state without outgoing edges");
    fsm89_test_unchanged(&r, &before, "missing: result unchanged");
}

static void test_unmatched_event(void)
{
    fsm89_step_result r;
    fsm89_test_snapshot before;
    fsm89_status rc;

    prefill(&r);
    fsm89_test_snapshot_take(&before, &r);
    rc = fsm89_step(&fsm89_fixture_m1, 0, 1, &r);
    fsm89_test_check_status(rc, FSM89_NO_TRANSITION,
                            "missing: unmatched event");
    fsm89_test_unchanged(&r, &before, "missing: unmatched result unchanged");
}

static void test_accepting_unmatched(void)
{
    fsm89_step_result r;
    fsm89_status rc;

    rc = fsm89_step(&fsm89_fixture_m3, 1, 9, &r);
    fsm89_test_check_status(rc, FSM89_NO_TRANSITION,
                            "missing: accepting state, unmatched event");
}

static void test_self_loop_unmatched(void)
{
    fsm89_step_result r;
    fsm89_status rc;

    rc = fsm89_step(&fsm89_fixture_m2, 0, 9, &r);
    fsm89_test_check_status(rc, FSM89_NO_TRANSITION,
                            "missing: self-loop state, unmatched event");
}

static void test_boundary_events(void)
{
    fsm89_step_result r;
    fsm89_status rc;

    rc = fsm89_step(&fsm89_fixture_m0, 0, 0, &r);
    fsm89_test_check_status(rc, FSM89_NO_TRANSITION, "missing: event 0");
    rc = fsm89_step(&fsm89_fixture_m1, 0, (unsigned long)-1, &r);
    fsm89_test_check_status(rc, FSM89_NO_TRANSITION,
                            "missing: ULONG_MAX event");
}

static void test_unknown_state(void)
{
    fsm89_step_result r;
    fsm89_test_snapshot before;
    fsm89_status rc;

    prefill(&r);
    fsm89_test_snapshot_take(&before, &r);
    rc = fsm89_step(&fsm89_fixture_m1, 2, 0, &r);
    fsm89_test_check_status(rc, FSM89_ESTATE, "missing: unknown state 2");
    fsm89_test_unchanged(&r, &before, "missing: ESTATE result unchanged");

    rc = fsm89_step(&fsm89_fixture_m1, (unsigned long)-1, 0, &r);
    fsm89_test_check_status(rc, FSM89_ESTATE, "missing: ULONG_MAX state");
}

static void test_unknown_state_gap(void)
{
    static const fsm89_state_def states[] = {{1, {NULL, 0}, {NULL, 0}, 0},
                                             {3, {NULL, 0}, {NULL, 0}, 0}};
    static const fsm89_edge edges[] = {{1, 0, 3, {NULL, 0}}};
    fsm89_def def;
    fsm89_step_result r;
    fsm89_status rc;

    def.states = states;
    def.state_count = 2;
    def.edges = edges;
    def.edge_count = 1;
    def.initial = 1;
    rc = fsm89_step(&def, 2, 0, &r);
    fsm89_test_check_status(rc, FSM89_ESTATE, "missing: state gap");
    rc = fsm89_step(&def, 3, 1, &r);
    fsm89_test_check_status(rc, FSM89_NO_TRANSITION,
                            "missing: valid state, absent event");
}

static void test_distinction(void)
{
    fsm89_step_result r;
    fsm89_status unknown;
    fsm89_status absent;

    unknown = fsm89_step(&fsm89_fixture_m1, 2, 0, &r);
    absent = fsm89_step(&fsm89_fixture_m1, 0, 2, &r);
    fsm89_test_check(unknown != absent, "missing: codes are distinct");
    fsm89_test_check_status(unknown, FSM89_ESTATE, "missing: unknown state");
    fsm89_test_check_status(absent, FSM89_NO_TRANSITION,
                            "missing: absent transition");
}

int main(void)
{
    test_no_outgoing();
    test_unmatched_event();
    test_accepting_unmatched();
    test_self_loop_unmatched();
    test_boundary_events();
    test_unknown_state();
    test_unknown_state_gap();
    test_distinction();
    return fsm89_test_report();
}
