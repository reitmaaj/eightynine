/* test_metamorphic.c - renaming and extension invariants. */

#include "fsm89_fixtures.h"
#include "fsm89_test.h"

static int mapped_spans(fsm89_effects a, fsm89_effects b, unsigned long delta)
{
    size_t i;

    if (a.n != b.n)
    {
        return 0;
    }
    for (i = 0; i < a.n; ++i)
    {
        if (b.v[i] != a.v[i] + delta)
        {
            return 0;
        }
    }
    return 1;
}

static void test_renaming(void)
{
    static const fsm89_effect orig_leave[] = {10};
    static const fsm89_effect orig_enter[] = {20};
    static const fsm89_effect orig_edge[] = {10, 20};
    static const fsm89_state_def orig_states[] = {
        {0, {orig_leave, 1}, {NULL, 0}, 0},
        {1, {NULL, 0}, {orig_enter, 1}, 0},
        {2, {NULL, 0}, {NULL, 0}, 0}};
    static const fsm89_edge orig_edges[] = {{0, 0, 1, {orig_edge, 2}},
                                            {1, 1, 2, {NULL, 0}},
                                            {2, 0, 2, {orig_enter, 1}}};
    static const fsm89_effect new_leave[] = {17};
    static const fsm89_effect new_enter[] = {27};
    static const fsm89_effect new_edge[] = {17, 27};
    static const fsm89_state_def new_states[] = {
        {1000, {new_leave, 1}, {NULL, 0}, 0},
        {1001, {NULL, 0}, {new_enter, 1}, 0},
        {1002, {NULL, 0}, {NULL, 0}, 0}};
    static const fsm89_edge new_edges[] = {{1000, 500, 1001, {new_edge, 2}},
                                           {1001, 501, 1002, {NULL, 0}},
                                           {1002, 500, 1002, {new_enter, 1}}};
    static const fsm89_state map_s[] = {1000, 1001, 1002};
    static const fsm89_event map_e[] = {500, 501};
    fsm89_def orig;
    fsm89_def renamed;
    fsm89_step_result ra;
    fsm89_step_result rb;
    fsm89_status rca;
    fsm89_status rcb;
    size_t s;
    size_t e;

    orig.states = orig_states;
    orig.state_count = 3;
    orig.edges = orig_edges;
    orig.edge_count = 3;
    orig.initial = 0;
    renamed.states = new_states;
    renamed.state_count = 3;
    renamed.edges = new_edges;
    renamed.edge_count = 3;
    renamed.initial = 1000;

    fsm89_test_check_status(fsm89_validate(&orig), FSM89_OK,
                            "metamorphic: original validates");
    fsm89_test_check_status(fsm89_validate(&renamed), FSM89_OK,
                            "metamorphic: renamed validates");

    for (s = 0; s < 3; ++s)
    {
        for (e = 0; e < 2; ++e)
        {
            rca = fsm89_step(&orig, (fsm89_state)s, (fsm89_event)e, &ra);
            rcb = fsm89_step(&renamed, map_s[s], map_e[e], &rb);
            fsm89_test_check_status(rcb, rca, "metamorphic: status");
            if (rca != FSM89_OK)
            {
                continue;
            }
            fsm89_test_check_ul(rb.to, map_s[ra.to],
                                "metamorphic: destination image");
            fsm89_test_check(mapped_spans(ra.leave, rb.leave, 7) != 0,
                             "metamorphic: leave image");
            fsm89_test_check(mapped_spans(ra.edge, rb.edge, 7) != 0,
                             "metamorphic: edge image");
            fsm89_test_check(mapped_spans(ra.enter, rb.enter, 7) != 0,
                             "metamorphic: enter image");
        }
    }
}

static void test_add_unreachable_state(void)
{
    static const fsm89_effect a_leave[] = {10, 11};
    static const fsm89_effect edge_fx[] = {20, 21};
    static const fsm89_effect b_enter[] = {30, 31};
    static const fsm89_state_def states[] = {{0, {a_leave, 2}, {NULL, 0}, 0},
                                             {1, {NULL, 0}, {b_enter, 2}, 0},
                                             {99, {NULL, 0}, {NULL, 0}, 1}};
    static const fsm89_edge edges[] = {{0, 0, 1, {edge_fx, 2}}};
    fsm89_def extended;
    fsm89_step_result ra;
    fsm89_step_result rb;
    fsm89_status rc;

    extended.states = states;
    extended.state_count = 3;
    extended.edges = edges;
    extended.edge_count = 1;
    extended.initial = 0;
    rc = fsm89_validate(&extended);
    fsm89_test_check_status(rc, FSM89_OK,
                            "metamorphic: unreachable state validates");
    rc = fsm89_step(&extended, 0, 0, &rb);
    fsm89_test_check_status(rc, FSM89_OK, "metamorphic: extended step");
    rc = fsm89_step(&fsm89_fixture_m1, 0, 0, &ra);
    fsm89_test_check_status(rc, FSM89_OK, "metamorphic: original step");
    fsm89_test_check_ul(rb.to, ra.to, "metamorphic: destination unchanged");
    fsm89_test_check(fsm89_test_span_equal(ra.leave, rb.leave) != 0,
                     "metamorphic: leave unchanged");
    fsm89_test_check(fsm89_test_span_equal(ra.edge, rb.edge) != 0,
                     "metamorphic: edge unchanged");
    fsm89_test_check(fsm89_test_span_equal(ra.enter, rb.enter) != 0,
                     "metamorphic: enter unchanged");
    fsm89_test_check(fsm89_accepting(&extended, 99) != 0,
                     "metamorphic: unreachable accepting state");
}

static void test_add_unreachable_component(void)
{
    static const fsm89_state_def states[] = {{0, {NULL, 0}, {NULL, 0}, 0},
                                             {1, {NULL, 0}, {NULL, 0}, 0},
                                             {2, {NULL, 0}, {NULL, 0}, 0},
                                             {3, {NULL, 0}, {NULL, 0}, 0}};
    static const fsm89_edge edges[] = {
        {0, 0, 1, {NULL, 0}}, {2, 0, 3, {NULL, 0}}, {3, 0, 2, {NULL, 0}}};
    fsm89_def extended;
    fsm89_step_result ra;
    fsm89_step_result rb;
    fsm89_status rc;

    extended.states = states;
    extended.state_count = 4;
    extended.edges = edges;
    extended.edge_count = 3;
    extended.initial = 0;
    rc = fsm89_validate(&extended);
    fsm89_test_check_status(rc, FSM89_OK,
                            "metamorphic: unreachable SCC validates");
    rc = fsm89_step(&extended, 0, 0, &rb);
    fsm89_test_check_status(rc, FSM89_OK, "metamorphic: extended step");
    rc = fsm89_step(&fsm89_fixture_m5, 0, 0, &ra);
    fsm89_test_check_status(rc, FSM89_OK, "metamorphic: silent step");
    fsm89_test_check_ul(rb.to, ra.to,
                        "metamorphic: reachable behavior unchanged");
}

int main(void)
{
    test_renaming();
    test_add_unreachable_state();
    test_add_unreachable_component();
    return fsm89_test_report();
}
