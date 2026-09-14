/* test_order.c - table position carries no semantics. */

#include "fsm89_fixtures.h"
#include "fsm89_test.h"

static void compare_pair(const fsm89_def *a, const fsm89_def *b,
                         fsm89_state state, fsm89_event event, const char *what)
{
    fsm89_step_result ra;
    fsm89_step_result rb;
    fsm89_status rca;
    fsm89_status rcb;

    rca = fsm89_step(a, state, event, &ra);
    rcb = fsm89_step(b, state, event, &rb);
    fsm89_test_check_status(rca, rcb, what);
    if (rca != FSM89_OK)
    {
        return;
    }
    fsm89_test_check_ul(ra.to, rb.to, what);
    fsm89_test_check(fsm89_test_span_equal(ra.leave, rb.leave) != 0, what);
    fsm89_test_check(fsm89_test_span_equal(ra.edge, rb.edge) != 0, what);
    fsm89_test_check(fsm89_test_span_equal(ra.enter, rb.enter) != 0, what);
}

static void test_state_reorder(void)
{
    static const fsm89_effect a_leave[] = {10, 11};
    static const fsm89_effect edge_fx[] = {20, 21};
    static const fsm89_effect b_enter[] = {30, 31};
    static const fsm89_state_def states[] = {{1, {NULL, 0}, {b_enter, 2}, 0},
                                             {0, {a_leave, 2}, {NULL, 0}, 0}};
    static const fsm89_edge edges[] = {{0, 0, 1, {edge_fx, 2}}};
    fsm89_def reordered;
    fsm89_status rc;

    reordered.states = states;
    reordered.state_count = 2;
    reordered.edges = edges;
    reordered.edge_count = 1;
    reordered.initial = 0;
    rc = fsm89_validate(&reordered);
    fsm89_test_check_status(rc, FSM89_OK, "order: reordered states validate");
    compare_pair(&fsm89_fixture_m1, &reordered, 0, 0,
                 "order: reordered states behave identically");
}

static void test_edge_reorder(void)
{
    static const fsm89_state_def states[] = {{0, {NULL, 0}, {NULL, 0}, 0},
                                             {1, {NULL, 0}, {NULL, 0}, 0},
                                             {2, {NULL, 0}, {NULL, 0}, 0}};
    static const fsm89_edge edges[] = {
        {1, 0, 2, {NULL, 0}}, {0, 1, 2, {NULL, 0}}, {0, 0, 2, {NULL, 0}}};
    fsm89_def reordered;
    fsm89_status rc;

    reordered.states = states;
    reordered.state_count = 3;
    reordered.edges = edges;
    reordered.edge_count = 3;
    reordered.initial = 0;
    rc = fsm89_validate(&reordered);
    fsm89_test_check_status(rc, FSM89_OK, "order: reordered edges validate");
    compare_pair(&fsm89_fixture_m6, &reordered, 0, 0, "order: edge 0");
    compare_pair(&fsm89_fixture_m6, &reordered, 0, 1, "order: edge 1");
    compare_pair(&fsm89_fixture_m6, &reordered, 1, 0, "order: edge 2");
    compare_pair(&fsm89_fixture_m6, &reordered, 2, 0, "order: no edge");
}

int main(void)
{
    test_state_reorder();
    test_edge_reorder();
    return fsm89_test_report();
}
