/* test_start.c - startup semantics. */

#include "fsm89_fixtures.h"
#include "fsm89_test.h"

static void test_initial_state(void)
{
    fsm89_state state;
    fsm89_effects enter;
    fsm89_status rc;

    rc = fsm89_start(&fsm89_fixture_m1, &state, &enter);
    fsm89_test_check_status(rc, FSM89_OK, "start: m1 succeeds");
    fsm89_test_check_ul(state, 0, "start: initial state is 0");
}

static void test_enter_span(void)
{
    fsm89_state state;
    fsm89_effects enter;
    fsm89_status rc;

    rc = fsm89_start(&fsm89_fixture_m0, &state, &enter);
    fsm89_test_check_status(rc, FSM89_OK, "start: m0 succeeds");
    fsm89_test_check_ul(state, 0, "start: m0 initial state");
    fsm89_test_check_size(enter.n, 1, "start: m0 enter count");
    fsm89_test_check_span(enter, fsm89_fixture_m0.states[0].enter.v, 1,
                          "start: m0 enter span is borrowed");
    fsm89_test_check_ul(enter.v[0], 10, "start: m0 enter content");
}

static void test_empty_enter(void)
{
    fsm89_state state;
    fsm89_effects enter;
    fsm89_status rc;

    rc = fsm89_start(&fsm89_fixture_m1, &state, &enter);
    fsm89_test_check_status(rc, FSM89_OK, "start: empty enter succeeds");
    fsm89_test_check_size(enter.n, 0, "start: empty enter count");
}

static void test_no_leave_emission(void)
{
    fsm89_state state;
    fsm89_effects enter;
    fsm89_status rc;

    rc = fsm89_start(&fsm89_fixture_m0, &state, &enter);
    fsm89_test_check_status(rc, FSM89_OK, "start: m0");
    fsm89_test_check_ul(enter.v[0], 10, "start: enter is not the leave span");
    fsm89_test_check(enter.v != fsm89_fixture_m0.states[0].leave.v,
                     "start: leave storage is not returned");
}

static void test_self_loop_initial(void)
{
    fsm89_state state;
    fsm89_effects enter;
    fsm89_status rc;

    rc = fsm89_start(&fsm89_fixture_m2, &state, &enter);
    fsm89_test_check_status(rc, FSM89_OK, "start: self-loop machine");
    fsm89_test_check_ul(state, 0, "start: self-loop initial state");
    fsm89_test_check_size(enter.n, 1, "start: self-loop enter count");
    fsm89_test_check_ul(enter.v[0], 30, "start: no traversal occurs");
}

static void test_initial_with_outgoing(void)
{
    fsm89_state state;
    fsm89_effects enter;
    fsm89_status rc;

    rc = fsm89_start(&fsm89_fixture_m5, &state, &enter);
    fsm89_test_check_status(rc, FSM89_OK, "start: initial with outgoing edge");
    fsm89_test_check_size(enter.n, 0, "start: outgoing edge emits nothing");
}

static void test_accepting_initial(void)
{
    static const fsm89_effect enter_fx[] = {42};
    static const fsm89_state_def states[] = {{5, {NULL, 0}, {enter_fx, 1}, 1}};
    fsm89_def def;
    fsm89_state state;
    fsm89_effects enter;
    fsm89_status rc;

    def.states = states;
    def.state_count = 1;
    def.edges = NULL;
    def.edge_count = 0;
    def.initial = 5;
    rc = fsm89_start(&def, &state, &enter);
    fsm89_test_check_status(rc, FSM89_OK, "start: accepting initial");
    fsm89_test_check_ul(state, 5, "start: accepting initial state");
    fsm89_test_check_ul(enter.v[0], 42, "start: accepting initial enter");
}

static void test_no_edges(void)
{
    fsm89_state state;
    fsm89_effects enter;
    fsm89_status rc;

    rc = fsm89_start(&fsm89_fixture_m0, &state, &enter);
    fsm89_test_check_status(rc, FSM89_OK, "start: machine without edges");
}

static void test_initial_not_first(void)
{
    static const fsm89_effect first_enter[] = {1};
    static const fsm89_effect last_enter[] = {9};
    static const fsm89_state_def states[] = {
        {10, {NULL, 0}, {first_enter, 1}, 0},
        {20, {NULL, 0}, {last_enter, 1}, 0}};
    fsm89_def def;
    fsm89_state state;
    fsm89_effects enter;
    fsm89_status rc;

    def.states = states;
    def.state_count = 2;
    def.edges = NULL;
    def.edge_count = 0;
    def.initial = 20;
    rc = fsm89_start(&def, &state, &enter);
    fsm89_test_check_status(rc, FSM89_OK, "start: initial not first");
    fsm89_test_check_ul(state, 20, "start: initial not first state");
    fsm89_test_check_size(enter.n, 1, "start: initial not first enter count");
    fsm89_test_check_ul(enter.v[0], 9, "start: initial not first enter value");
}

int main(void)
{
    test_initial_state();
    test_enter_span();
    test_empty_enter();
    test_no_leave_emission();
    test_self_loop_initial();
    test_initial_with_outgoing();
    test_accepting_initial();
    test_no_edges();
    test_initial_not_first();
    return fsm89_test_report();
}
