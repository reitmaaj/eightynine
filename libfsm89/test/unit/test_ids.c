/* test_ids.c - identifier domain and sparse IDs. */

#include "fsm89_test.h"

static void test_zero_ids(void)
{
    static const fsm89_effect effect = 0;
    static const fsm89_state_def states[] = {
        {0, {&effect, 1}, {&effect, 1}, 1}};
    static const fsm89_edge edges[] = {{0, 0, 0, {&effect, 1}}};
    fsm89_def def;
    fsm89_step_result r;
    fsm89_status rc;

    def.states = states;
    def.state_count = 1;
    def.edges = edges;
    def.edge_count = 1;
    def.initial = 0;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_OK, "ids: zero identifiers validate");
    rc = fsm89_step(&def, 0, 0, &r);
    fsm89_test_check_status(rc, FSM89_OK, "ids: zero identifiers step");
    fsm89_test_check_ul(r.to, 0, "ids: zero destination");
    fsm89_test_check(fsm89_accepting(&def, 0) != 0, "ids: zero accepting");
}

static void test_max_ids(void)
{
    static const fsm89_state_def states[] = {
        {(unsigned long)-1, {NULL, 0}, {NULL, 0}, 0},
        {(unsigned long)-2, {NULL, 0}, {NULL, 0}, 0}};
    static const fsm89_edge edges[] = {
        {(unsigned long)-1, (unsigned long)-1, (unsigned long)-2, {NULL, 0}}};
    fsm89_def def;
    fsm89_step_result r;
    fsm89_status rc;

    def.states = states;
    def.state_count = 2;
    def.edges = edges;
    def.edge_count = 1;
    def.initial = (unsigned long)-1;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_OK, "ids: maximum identifiers validate");
    rc = fsm89_step(&def, (unsigned long)-1, (unsigned long)-1, &r);
    fsm89_test_check_status(rc, FSM89_OK, "ids: maximum identifiers step");
    fsm89_test_check_ul(r.to, (unsigned long)-2, "ids: maximum destination");
}

static void test_sparse_ids(void)
{
    static const fsm89_state_def states[] = {{1, {NULL, 0}, {NULL, 0}, 0},
                                             {100, {NULL, 0}, {NULL, 0}, 1},
                                             {100000, {NULL, 0}, {NULL, 0}, 0}};
    static const fsm89_edge edges[] = {{100, 1, 1, {NULL, 0}},
                                       {1, 0, 100000, {NULL, 0}}};
    fsm89_def def;
    fsm89_step_result r;
    fsm89_status rc;

    def.states = states;
    def.state_count = 3;
    def.edges = edges;
    def.edge_count = 2;
    def.initial = 100;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_OK, "ids: sparse identifiers validate");

    rc = fsm89_step(&def, 100, 1, &r);
    fsm89_test_check_status(rc, FSM89_OK, "ids: sparse state 100");
    fsm89_test_check_ul(r.to, 1, "ids: sparse destination 1");

    rc = fsm89_step(&def, 1, 0, &r);
    fsm89_test_check_status(rc, FSM89_OK, "ids: sparse state 1");
    fsm89_test_check_ul(r.to, 100000, "ids: sparse destination 100000");

    rc = fsm89_step(&def, 100000, 0, &r);
    fsm89_test_check_status(rc, FSM89_NO_TRANSITION, "ids: sparse no edge");

    fsm89_test_check(fsm89_accepting(&def, 100) != 0,
                     "ids: sparse accepting state");
    fsm89_test_check(fsm89_accepting(&def, 1) == 0,
                     "ids: sparse non-accepting state");
    fsm89_test_check(fsm89_accepting(&def, 2) == 0,
                     "ids: gap state is unknown");
}

int main(void)
{
    test_zero_ids();
    test_max_ids();
    test_sparse_ids();
    return fsm89_test_report();
}
