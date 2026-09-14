/* test_reachability.c - unreachable states are valid. */

#include "fsm89_fixtures.h"
#include "fsm89_test.h"

static void test_fixture(void)
{
    fsm89_status rc;

    rc = fsm89_validate(&fsm89_fixture_m4);
    fsm89_test_check_status(rc, FSM89_OK, "reachability: m4 validates");
}

static void test_unreachable_accepting(void)
{
    static const fsm89_effect enter_fx[] = {77};
    static const fsm89_state_def states[] = {{0, {NULL, 0}, {NULL, 0}, 0},
                                             {1, {NULL, 0}, {enter_fx, 1}, 1}};
    static const fsm89_edge edges[] = {{0, 0, 0, {NULL, 0}}};
    fsm89_def def;
    fsm89_status rc;

    def.states = states;
    def.state_count = 2;
    def.edges = edges;
    def.edge_count = 1;
    def.initial = 0;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_OK,
                            "reachability: unreachable accepting with effects");
    fsm89_test_check(fsm89_accepting(&def, 1) != 0,
                     "reachability: unreachable state is queryable");
}

static void test_unreachable_scc(void)
{
    static const fsm89_state_def states[] = {{0, {NULL, 0}, {NULL, 0}, 0},
                                             {1, {NULL, 0}, {NULL, 0}, 0},
                                             {2, {NULL, 0}, {NULL, 0}, 0}};
    static const fsm89_edge edges[] = {{0, 0, 1, {NULL, 0}},
                                       {2, 0, 2, {NULL, 0}}};
    fsm89_def def;
    fsm89_status rc;

    def.states = states;
    def.state_count = 3;
    def.edges = edges;
    def.edge_count = 2;
    def.initial = 0;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_OK,
                            "reachability: unreachable self-loop validates");
}

static void test_unreachable_step(void)
{
    fsm89_step_result r;
    fsm89_status rc;

    rc = fsm89_step(&fsm89_fixture_m4, 2, 1, &r);
    fsm89_test_check_status(rc, FSM89_OK, "reachability: unreachable step");
    fsm89_test_check_ul(r.to, 3, "reachability: unreachable destination");
}

int main(void)
{
    test_fixture();
    test_unreachable_accepting();
    test_unreachable_scc();
    test_unreachable_step();
    return fsm89_test_report();
}
