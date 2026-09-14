/* test_effects.c - exact emission order and the self-transition matrix. */

#include "fsm89_fixtures.h"
#include "fsm89_test.h"

static void test_all_sites(void)
{
    static const fsm89_effect want[] = {10, 11, 20, 21, 30, 31};
    fsm89_step_result r;
    fsm89_status rc;

    rc = fsm89_step(&fsm89_fixture_m1, 0, 0, &r);
    fsm89_test_check_status(rc, FSM89_OK, "effects: all sites");
    fsm89_test_expect_seq(&r, want, 6, "effects: leave edge enter order");
}

static void test_self_order(void)
{
    static const fsm89_effect want[] = {10, 20, 30};
    fsm89_step_result r;
    fsm89_status rc;

    rc = fsm89_step(&fsm89_fixture_m2, 0, 0, &r);
    fsm89_test_check_status(rc, FSM89_OK, "effects: self");
    fsm89_test_expect_seq(&r, want, 3, "effects: self leave edge enter");
}

static void test_duplicates(void)
{
    static const fsm89_effect leave_fx[] = {7, 7};
    static const fsm89_effect edge_fx[] = {7};
    static const fsm89_effect enter_fx[] = {7, 7};
    static const fsm89_effect want[] = {7, 7, 7, 7, 7};
    static const fsm89_state_def states[] = {{0, {leave_fx, 2}, {NULL, 0}, 0},
                                             {1, {NULL, 0}, {enter_fx, 2}, 0}};
    static const fsm89_edge edges[] = {{0, 0, 1, {edge_fx, 1}}};
    fsm89_def def;
    fsm89_step_result r;
    fsm89_status rc;

    def.states = states;
    def.state_count = 2;
    def.edges = edges;
    def.edge_count = 1;
    def.initial = 0;
    rc = fsm89_step(&def, 0, 0, &r);
    fsm89_test_check_status(rc, FSM89_OK, "effects: duplicates");
    fsm89_test_expect_seq(&r, want, 5, "effects: no deduplication");
}

static void check_self_combo(int leave_on, int edge_on, int enter_on,
                             const char *what)
{
    fsm89_effect leave_buf[1];
    fsm89_effect edge_buf[1];
    fsm89_effect enter_buf[1];
    fsm89_effect want[3];
    fsm89_state_def state;
    fsm89_edge edge;
    fsm89_def def;
    fsm89_step_result r;
    fsm89_status rc;
    size_t n;

    leave_buf[0] = 10;
    edge_buf[0] = 20;
    enter_buf[0] = 30;

    state.id = 0;
    state.leave.v = NULL;
    state.leave.n = 0;
    state.enter.v = NULL;
    state.enter.n = 0;
    state.accepting = 0;
    if (leave_on != 0)
    {
        state.leave.v = leave_buf;
        state.leave.n = 1;
    }
    if (enter_on != 0)
    {
        state.enter.v = enter_buf;
        state.enter.n = 1;
    }
    edge.from = 0;
    edge.event = 0;
    edge.to = 0;
    edge.effects.v = NULL;
    edge.effects.n = 0;
    if (edge_on != 0)
    {
        edge.effects.v = edge_buf;
        edge.effects.n = 1;
    }
    def.states = &state;
    def.state_count = 1;
    def.edges = &edge;
    def.edge_count = 1;
    def.initial = 0;

    n = 0;
    if (leave_on != 0)
    {
        want[n] = 10;
        n += 1;
    }
    if (edge_on != 0)
    {
        want[n] = 20;
        n += 1;
    }
    if (enter_on != 0)
    {
        want[n] = 30;
        n += 1;
    }

    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_OK, what);
    rc = fsm89_step(&def, 0, 0, &r);
    fsm89_test_check_status(rc, FSM89_OK, what);
    fsm89_test_expect_seq(&r, want, n, what);
}

static void test_self_matrix(void)
{
    check_self_combo(0, 0, 0, "self matrix: none");
    check_self_combo(1, 0, 0, "self matrix: leave only");
    check_self_combo(0, 1, 0, "self matrix: edge only");
    check_self_combo(0, 0, 1, "self matrix: enter only");
    check_self_combo(1, 1, 0, "self matrix: leave and edge");
    check_self_combo(1, 0, 1, "self matrix: leave and enter");
    check_self_combo(0, 1, 1, "self matrix: edge and enter");
    check_self_combo(1, 1, 1, "self matrix: all sites");
}

int main(void)
{
    test_all_sites();
    test_self_order();
    test_duplicates();
    test_self_matrix();
    return fsm89_test_report();
}
