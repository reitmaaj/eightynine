/* test_step.c - successful transition results. */

#include "fsm89_fixtures.h"
#include "fsm89_test.h"

static void test_ordinary(void)
{
    fsm89_step_result r;
    fsm89_status rc;

    rc = fsm89_step(&fsm89_fixture_m1, 0, 0, &r);
    fsm89_test_check_status(rc, FSM89_OK, "step: ordinary");
    fsm89_test_check_ul(r.from, 0, "step: ordinary from");
    fsm89_test_check_ul(r.event, 0, "step: ordinary event");
    fsm89_test_check_ul(r.to, 1, "step: ordinary to");
    fsm89_test_check_span(r.leave, fsm89_fixture_m1.states[0].leave.v, 2,
                          "step: ordinary leave span");
    fsm89_test_check_span(r.edge, fsm89_fixture_m1.edges[0].effects.v, 2,
                          "step: ordinary edge span");
    fsm89_test_check_span(r.enter, fsm89_fixture_m1.states[1].enter.v, 2,
                          "step: ordinary enter span");
}

static void test_self(void)
{
    fsm89_step_result r;
    fsm89_status rc;

    rc = fsm89_step(&fsm89_fixture_m2, 0, 0, &r);
    fsm89_test_check_status(rc, FSM89_OK, "step: self-transition");
    fsm89_test_check_ul(r.from, 0, "step: self from");
    fsm89_test_check_ul(r.to, 0, "step: self to");
    fsm89_test_check_span(r.leave, fsm89_fixture_m2.states[0].leave.v, 1,
                          "step: self leave span");
    fsm89_test_check_span(r.edge, fsm89_fixture_m2.edges[0].effects.v, 1,
                          "step: self edge span");
    fsm89_test_check_span(r.enter, fsm89_fixture_m2.states[0].enter.v, 1,
                          "step: self enter span");
}

static void test_silent(void)
{
    fsm89_step_result r;
    fsm89_status rc;

    rc = fsm89_step(&fsm89_fixture_m5, 0, 0, &r);
    fsm89_test_check_status(rc, FSM89_OK, "step: silent transition");
    fsm89_test_check_ul(r.to, 1, "step: silent destination");
    fsm89_test_check_size(r.leave.n, 0, "step: silent leave empty");
    fsm89_test_check_size(r.edge.n, 0, "step: silent edge empty");
    fsm89_test_check_size(r.enter.n, 0, "step: silent enter empty");
}

static void test_empty_leave(void)
{
    static const fsm89_effect edge_fx[] = {20};
    static const fsm89_effect enter_fx[] = {30};
    static const fsm89_state_def states[] = {{0, {NULL, 0}, {NULL, 0}, 0},
                                             {1, {NULL, 0}, {enter_fx, 1}, 0}};
    static const fsm89_edge edges[] = {{0, 0, 1, {edge_fx, 1}}};
    static const fsm89_effect want[] = {20, 30};
    fsm89_def def;
    fsm89_step_result r;
    fsm89_status rc;

    def.states = states;
    def.state_count = 2;
    def.edges = edges;
    def.edge_count = 1;
    def.initial = 0;
    rc = fsm89_step(&def, 0, 0, &r);
    fsm89_test_check_status(rc, FSM89_OK, "step: empty leave");
    fsm89_test_expect_seq(&r, want, 2, "step: empty leave order");
}

static void test_empty_edge(void)
{
    static const fsm89_effect leave_fx[] = {10};
    static const fsm89_effect enter_fx[] = {30};
    static const fsm89_state_def states[] = {{0, {leave_fx, 1}, {NULL, 0}, 0},
                                             {1, {NULL, 0}, {enter_fx, 1}, 0}};
    static const fsm89_edge edges[] = {{0, 0, 1, {NULL, 0}}};
    static const fsm89_effect want[] = {10, 30};
    fsm89_def def;
    fsm89_step_result r;
    fsm89_status rc;

    def.states = states;
    def.state_count = 2;
    def.edges = edges;
    def.edge_count = 1;
    def.initial = 0;
    rc = fsm89_step(&def, 0, 0, &r);
    fsm89_test_check_status(rc, FSM89_OK, "step: empty edge");
    fsm89_test_expect_seq(&r, want, 2, "step: empty edge order");
}

static void test_empty_enter(void)
{
    static const fsm89_effect leave_fx[] = {10};
    static const fsm89_effect edge_fx[] = {20};
    static const fsm89_state_def states[] = {{0, {leave_fx, 1}, {NULL, 0}, 0},
                                             {1, {NULL, 0}, {NULL, 0}, 0}};
    static const fsm89_edge edges[] = {{0, 0, 1, {edge_fx, 1}}};
    static const fsm89_effect want[] = {10, 20};
    fsm89_def def;
    fsm89_step_result r;
    fsm89_status rc;

    def.states = states;
    def.state_count = 2;
    def.edges = edges;
    def.edge_count = 1;
    def.initial = 0;
    rc = fsm89_step(&def, 0, 0, &r);
    fsm89_test_check_status(rc, FSM89_OK, "step: empty enter");
    fsm89_test_expect_seq(&r, want, 2, "step: empty enter order");
}

static void test_only_edge(void)
{
    static const fsm89_effect edge_fx[] = {20};
    static const fsm89_state_def states[] = {{0, {NULL, 0}, {NULL, 0}, 0},
                                             {1, {NULL, 0}, {NULL, 0}, 0}};
    static const fsm89_edge edges[] = {{0, 0, 1, {edge_fx, 1}}};
    static const fsm89_effect want[] = {20};
    fsm89_def def;
    fsm89_step_result r;
    fsm89_status rc;

    def.states = states;
    def.state_count = 2;
    def.edges = edges;
    def.edge_count = 1;
    def.initial = 0;
    rc = fsm89_step(&def, 0, 0, &r);
    fsm89_test_check_status(rc, FSM89_OK, "step: Mealy-only edge effect");
    fsm89_test_expect_seq(&r, want, 1, "step: Mealy order");
}

static void test_only_enter(void)
{
    static const fsm89_effect enter_fx[] = {30};
    static const fsm89_state_def states[] = {{0, {NULL, 0}, {NULL, 0}, 0},
                                             {1, {NULL, 0}, {enter_fx, 1}, 0}};
    static const fsm89_edge edges[] = {{0, 0, 1, {NULL, 0}}};
    static const fsm89_effect want[] = {30};
    fsm89_def def;
    fsm89_step_result r;
    fsm89_status rc;

    def.states = states;
    def.state_count = 2;
    def.edges = edges;
    def.edge_count = 1;
    def.initial = 0;
    rc = fsm89_step(&def, 0, 0, &r);
    fsm89_test_check_status(rc, FSM89_OK, "step: Moore-only enter effect");
    fsm89_test_expect_seq(&r, want, 1, "step: Moore order");
}

static void test_repeated(void)
{
    fsm89_step_result first;
    fsm89_step_result second;
    fsm89_test_snapshot snap;
    fsm89_status rc;

    rc = fsm89_step(&fsm89_fixture_m1, 0, 0, &first);
    fsm89_test_check_status(rc, FSM89_OK, "step: repeated first");
    fsm89_test_snapshot_take(&snap, &first);
    rc = fsm89_step(&fsm89_fixture_m1, 0, 0, &second);
    fsm89_test_check_status(rc, FSM89_OK, "step: repeated second");
    fsm89_test_check_ul(second.from, snap.from, "step: repeated from");
    fsm89_test_check_ul(second.event, snap.event, "step: repeated event");
    fsm89_test_check_ul(second.to, snap.to, "step: repeated to");
    fsm89_test_check_span(second.leave, snap.leave_v, snap.leave_n,
                          "step: repeated leave");
    fsm89_test_check_span(second.edge, snap.edge_v, snap.edge_n,
                          "step: repeated edge");
    fsm89_test_check_span(second.enter, snap.enter_v, snap.enter_n,
                          "step: repeated enter");
}

int main(void)
{
    test_ordinary();
    test_self();
    test_silent();
    test_empty_leave();
    test_empty_edge();
    test_empty_enter();
    test_only_edge();
    test_only_enter();
    test_repeated();
    return fsm89_test_report();
}
