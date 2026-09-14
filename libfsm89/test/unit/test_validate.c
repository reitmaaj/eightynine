/* test_validate.c - definition-level invariants. */

#include "fsm89_fixtures.h"
#include "fsm89_test.h"

static void test_one_state(void)
{
    fsm89_status rc;

    rc = fsm89_validate(&fsm89_fixture_m0);
    fsm89_test_check_status(rc, FSM89_OK, "validate: one state, no edges");
}

static void test_null_def(void)
{
    fsm89_status rc;

    rc = fsm89_validate(NULL);
    fsm89_test_check_status(rc, FSM89_EDEF, "validate: NULL def");
}

static void test_zero_state_count(void)
{
    static const fsm89_state_def states[] = {{0, {NULL, 0}, {NULL, 0}, 0}};
    fsm89_def def;
    fsm89_status rc;

    def.states = states;
    def.state_count = 0;
    def.edges = NULL;
    def.edge_count = 0;
    def.initial = 0;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_EDEF, "validate: zero states");
}

static void test_null_states(void)
{
    fsm89_def def;
    fsm89_status rc;

    def.states = NULL;
    def.state_count = 1;
    def.edges = NULL;
    def.edge_count = 0;
    def.initial = 0;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_EDEF, "validate: NULL states");
}

static void test_zero_edges(void)
{
    static const fsm89_state_def states[] = {{0, {NULL, 0}, {NULL, 0}, 0}};
    static const fsm89_edge edges[] = {{0, 0, 0, {NULL, 0}}};
    fsm89_def def;
    fsm89_status rc;

    def.states = states;
    def.state_count = 1;
    def.edges = NULL;
    def.edge_count = 0;
    def.initial = 0;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_OK, "validate: NULL edges, zero count");

    def.edges = edges;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_OK,
                            "validate: non-NULL edges, zero count");
}

static void test_edges_null(void)
{
    static const fsm89_state_def states[] = {{0, {NULL, 0}, {NULL, 0}, 0}};
    fsm89_def def;
    fsm89_status rc;

    def.states = states;
    def.state_count = 1;
    def.edges = NULL;
    def.edge_count = 1;
    def.initial = 0;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_EDEF, "validate: NULL edges, count 1");
}

static void test_bad_state_leave_span(void)
{
    static const fsm89_state_def states[] = {{0, {NULL, 1}, {NULL, 0}, 0}};
    fsm89_def def;
    fsm89_status rc;

    def.states = states;
    def.state_count = 1;
    def.edges = NULL;
    def.edge_count = 0;
    def.initial = 0;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_EEFFECTS,
                            "validate: NULL leave with n > 0");
}

static void test_bad_state_enter_span(void)
{
    static const fsm89_state_def states[] = {{0, {NULL, 0}, {NULL, 1}, 0}};
    fsm89_def def;
    fsm89_status rc;

    def.states = states;
    def.state_count = 1;
    def.edges = NULL;
    def.edge_count = 0;
    def.initial = 0;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_EEFFECTS,
                            "validate: NULL enter with n > 0");
}

static void test_bad_edge_span(void)
{
    static const fsm89_state_def states[] = {{0, {NULL, 0}, {NULL, 0}, 0},
                                             {1, {NULL, 0}, {NULL, 0}, 0}};
    static const fsm89_edge edges[] = {{0, 0, 1, {NULL, 1}}};
    fsm89_def def;
    fsm89_status rc;

    def.states = states;
    def.state_count = 2;
    def.edges = edges;
    def.edge_count = 1;
    def.initial = 0;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_EEFFECTS,
                            "validate: NULL edge span with n > 0");
}

static void test_empty_span_nonnull(void)
{
    static const fsm89_effect effect = 7;
    static const fsm89_state_def states[] = {
        {0, {&effect, 0}, {&effect, 0}, 0}};
    fsm89_def def;
    fsm89_status rc;

    def.states = states;
    def.state_count = 1;
    def.edges = NULL;
    def.edge_count = 0;
    def.initial = 0;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_OK,
                            "validate: empty span with non-NULL v");
}

static void test_good_state_spans(void)
{
    static const fsm89_effect effect = 1;
    static const fsm89_state_def states[] = {
        {0, {&effect, 1}, {&effect, 1}, 0}};
    fsm89_def def;
    fsm89_status rc;

    def.states = states;
    def.state_count = 1;
    def.edges = NULL;
    def.edge_count = 0;
    def.initial = 0;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_OK,
                            "validate: non-NULL leave and enter storage");
}

static void test_duplicate_state(void)
{
    static const fsm89_state_def states[] = {{0, {NULL, 0}, {NULL, 0}, 0},
                                             {0, {NULL, 0}, {NULL, 0}, 0}};
    fsm89_def def;
    fsm89_status rc;

    def.states = states;
    def.state_count = 2;
    def.edges = NULL;
    def.edge_count = 0;
    def.initial = 0;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_EDUP_STATE,
                            "validate: identical duplicate state");
}

static void test_duplicate_state_different(void)
{
    static const fsm89_effect effect = 1;
    static const fsm89_state_def states[] = {{5, {NULL, 0}, {NULL, 0}, 0},
                                             {5, {&effect, 1}, {NULL, 0}, 1}};
    fsm89_def def;
    fsm89_status rc;

    def.states = states;
    def.state_count = 2;
    def.edges = NULL;
    def.edge_count = 0;
    def.initial = 5;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_EDUP_STATE,
                            "validate: differing duplicate state");
}

static void test_missing_initial(void)
{
    fsm89_def def;
    fsm89_status rc;

    def.states = fsm89_fixture_m1.states;
    def.state_count = fsm89_fixture_m1.state_count;
    def.edges = fsm89_fixture_m1.edges;
    def.edge_count = fsm89_fixture_m1.edge_count;
    def.initial = 99;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_EINITIAL, "validate: missing initial");
}

static void test_missing_edge_from(void)
{
    static const fsm89_state_def states[] = {{0, {NULL, 0}, {NULL, 0}, 0},
                                             {1, {NULL, 0}, {NULL, 0}, 0}};
    static const fsm89_edge edges[] = {{99, 0, 1, {NULL, 0}}};
    fsm89_def def;
    fsm89_status rc;

    def.states = states;
    def.state_count = 2;
    def.edges = edges;
    def.edge_count = 1;
    def.initial = 0;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_EEDGE_FROM, "validate: missing source");
}

static void test_missing_edge_to(void)
{
    static const fsm89_state_def states[] = {{0, {NULL, 0}, {NULL, 0}, 0},
                                             {1, {NULL, 0}, {NULL, 0}, 0}};
    static const fsm89_edge edges[] = {{0, 0, 99, {NULL, 0}}};
    fsm89_def def;
    fsm89_status rc;

    def.states = states;
    def.state_count = 2;
    def.edges = edges;
    def.edge_count = 1;
    def.initial = 0;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_EEDGE_TO,
                            "validate: missing destination");
}

static void test_duplicate_edge_same_to(void)
{
    static const fsm89_state_def states[] = {{0, {NULL, 0}, {NULL, 0}, 0},
                                             {1, {NULL, 0}, {NULL, 0}, 0}};
    static const fsm89_edge edges[] = {{0, 0, 1, {NULL, 0}},
                                       {0, 0, 1, {NULL, 0}}};
    fsm89_def def;
    fsm89_status rc;

    def.states = states;
    def.state_count = 2;
    def.edges = edges;
    def.edge_count = 2;
    def.initial = 0;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_EDUP_EDGE,
                            "validate: duplicate key, same target");
}

static void test_duplicate_edge_different_to(void)
{
    static const fsm89_state_def states[] = {{0, {NULL, 0}, {NULL, 0}, 0},
                                             {1, {NULL, 0}, {NULL, 0}, 0},
                                             {2, {NULL, 0}, {NULL, 0}, 0}};
    static const fsm89_edge edges[] = {{0, 0, 1, {NULL, 0}},
                                       {0, 0, 2, {NULL, 0}}};
    fsm89_def def;
    fsm89_status rc;

    def.states = states;
    def.state_count = 3;
    def.edges = edges;
    def.edge_count = 2;
    def.initial = 0;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_EDUP_EDGE,
                            "validate: duplicate key, different targets");
}

static void test_duplicate_edge_different_effects(void)
{
    static const fsm89_effect p[] = {1};
    static const fsm89_effect q[] = {2};
    static const fsm89_state_def states[] = {{0, {NULL, 0}, {NULL, 0}, 0},
                                             {1, {NULL, 0}, {NULL, 0}, 0}};
    static const fsm89_edge edges[] = {{0, 0, 1, {p, 1}}, {0, 0, 1, {q, 1}}};
    fsm89_def def;
    fsm89_status rc;

    def.states = states;
    def.state_count = 2;
    def.edges = edges;
    def.edge_count = 2;
    def.initial = 0;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_EDUP_EDGE,
                            "validate: duplicate key, different effects");
}

static void test_valid_keys(void)
{
    fsm89_status rc;

    rc = fsm89_validate(&fsm89_fixture_m6);
    fsm89_test_check_status(rc, FSM89_OK,
                            "validate: shared event, different sources");
    rc = fsm89_validate(&fsm89_fixture_m2);
    fsm89_test_check_status(rc, FSM89_OK, "validate: self-transition");
}

static void test_policy(void)
{
    fsm89_status rc;

    rc = fsm89_validate(&fsm89_fixture_m4);
    fsm89_test_check_status(rc, FSM89_OK, "validate: unreachable component");
    rc = fsm89_validate(&fsm89_fixture_m3);
    fsm89_test_check_status(rc, FSM89_OK, "validate: accepting with outgoing");
    rc = fsm89_validate(&fsm89_fixture_m5);
    fsm89_test_check_status(rc, FSM89_OK, "validate: silent transition");
}

static void test_accepting_initial(void)
{
    static const fsm89_state_def states[] = {{0, {NULL, 0}, {NULL, 0}, 1}};
    fsm89_def def;
    fsm89_status rc;

    def.states = states;
    def.state_count = 1;
    def.edges = NULL;
    def.edge_count = 0;
    def.initial = 0;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_OK, "validate: accepting initial");
}

static void test_nonaccepting_terminal(void)
{
    static const fsm89_state_def states[] = {{0, {NULL, 0}, {NULL, 0}, 0},
                                             {1, {NULL, 0}, {NULL, 0}, 0}};
    static const fsm89_edge edges[] = {{0, 0, 1, {NULL, 0}}};
    fsm89_def def;
    fsm89_status rc;

    def.states = states;
    def.state_count = 2;
    def.edges = edges;
    def.edge_count = 1;
    def.initial = 0;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_OK,
                            "validate: non-accepting terminal state");
}

static void test_id_domain(void)
{
    static const fsm89_effect effect = 0;
    static const fsm89_state_def zero[] = {{0, {&effect, 1}, {&effect, 1}, 1}};
    static const fsm89_edge zero_edges[] = {{0, 0, 0, {&effect, 1}}};
    static const fsm89_state_def max_states[] = {
        {(unsigned long)-1, {NULL, 0}, {NULL, 0}, 0},
        {(unsigned long)-2, {NULL, 0}, {NULL, 0}, 0}};
    static const fsm89_edge max_edges[] = {
        {(unsigned long)-1, (unsigned long)-1, (unsigned long)-2, {NULL, 0}}};
    fsm89_def def;
    fsm89_status rc;

    def.states = zero;
    def.state_count = 1;
    def.edges = zero_edges;
    def.edge_count = 1;
    def.initial = 0;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_OK, "validate: zero identifiers");

    def.states = max_states;
    def.state_count = 2;
    def.edges = max_edges;
    def.edge_count = 1;
    def.initial = (unsigned long)-1;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_OK, "validate: maximum identifiers");
}

static void test_sparse_ids(void)
{
    static const fsm89_state_def states[] = {{1, {NULL, 0}, {NULL, 0}, 0},
                                             {100, {NULL, 0}, {NULL, 0}, 0},
                                             {100000, {NULL, 0}, {NULL, 0}, 0}};
    static const fsm89_edge edges[] = {{1, 0, 100000, {NULL, 0}},
                                       {100, 1, 1, {NULL, 0}}};
    fsm89_def def;
    fsm89_status rc;

    def.states = states;
    def.state_count = 3;
    def.edges = edges;
    def.edge_count = 2;
    def.initial = 100;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_OK, "validate: sparse identifiers");
}

int main(void)
{
    test_one_state();
    test_null_def();
    test_zero_state_count();
    test_null_states();
    test_zero_edges();
    test_edges_null();
    test_bad_state_leave_span();
    test_bad_state_enter_span();
    test_bad_edge_span();
    test_empty_span_nonnull();
    test_good_state_spans();
    test_duplicate_state();
    test_duplicate_state_different();
    test_missing_initial();
    test_missing_edge_from();
    test_missing_edge_to();
    test_duplicate_edge_same_to();
    test_duplicate_edge_different_to();
    test_duplicate_edge_different_effects();
    test_valid_keys();
    test_policy();
    test_accepting_initial();
    test_nonaccepting_terminal();
    test_id_domain();
    test_sparse_ids();
    return fsm89_test_report();
}
