/* test_validate_order.c - documented first-error precedence. */

#include "fsm89_test.h"

static void test_def_beats_effects(void)
{
    static const fsm89_state_def states[] = {{0, {NULL, 1}, {NULL, 0}, 0}};
    fsm89_def def;
    fsm89_status rc;

    def.states = states;
    def.state_count = 0;
    def.edges = NULL;
    def.edge_count = 0;
    def.initial = 0;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_EDEF, "order: EDEF before EEFFECTS");
}

static void test_effects_beats_dup_state(void)
{
    static const fsm89_state_def states[] = {{0, {NULL, 1}, {NULL, 0}, 0},
                                             {0, {NULL, 0}, {NULL, 0}, 0}};
    fsm89_def def;
    fsm89_status rc;

    def.states = states;
    def.state_count = 2;
    def.edges = NULL;
    def.edge_count = 0;
    def.initial = 0;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_EEFFECTS,
                            "order: state span before duplicate state");
}

static void test_dup_state_beats_initial(void)
{
    static const fsm89_state_def states[] = {{0, {NULL, 0}, {NULL, 0}, 0},
                                             {0, {NULL, 0}, {NULL, 0}, 0}};
    fsm89_def def;
    fsm89_status rc;

    def.states = states;
    def.state_count = 2;
    def.edges = NULL;
    def.edge_count = 0;
    def.initial = 7;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_EDUP_STATE,
                            "order: duplicate state before missing initial");
}

static void test_initial_beats_edge_span(void)
{
    static const fsm89_state_def states[] = {{0, {NULL, 0}, {NULL, 0}, 0}};
    static const fsm89_edge edges[] = {{0, 0, 0, {NULL, 1}}};
    fsm89_def def;
    fsm89_status rc;

    def.states = states;
    def.state_count = 1;
    def.edges = edges;
    def.edge_count = 1;
    def.initial = 7;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_EINITIAL,
                            "order: missing initial before edge span");
}

static void test_edge_span_beats_edge_from(void)
{
    static const fsm89_state_def states[] = {{0, {NULL, 0}, {NULL, 0}, 0}};
    static const fsm89_edge edges[] = {{9, 0, 0, {NULL, 1}}};
    fsm89_def def;
    fsm89_status rc;

    def.states = states;
    def.state_count = 1;
    def.edges = edges;
    def.edge_count = 1;
    def.initial = 0;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_EEFFECTS,
                            "order: edge span before missing source");
}

static void test_edge_from_beats_edge_to(void)
{
    static const fsm89_state_def states[] = {{0, {NULL, 0}, {NULL, 0}, 0},
                                             {1, {NULL, 0}, {NULL, 0}, 0}};
    static const fsm89_edge edges[] = {{9, 0, 1, {NULL, 0}},
                                       {0, 0, 9, {NULL, 0}}};
    fsm89_def def;
    fsm89_status rc;

    def.states = states;
    def.state_count = 2;
    def.edges = edges;
    def.edge_count = 2;
    def.initial = 0;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_EEDGE_FROM,
                            "order: missing source before missing destination");
}

static void test_edge_to_beats_dup_edge(void)
{
    static const fsm89_state_def states[] = {{0, {NULL, 0}, {NULL, 0}, 0},
                                             {1, {NULL, 0}, {NULL, 0}, 0}};
    static const fsm89_edge edges[] = {{0, 0, 9, {NULL, 0}},
                                       {0, 0, 1, {NULL, 0}}};
    fsm89_def def;
    fsm89_status rc;

    def.states = states;
    def.state_count = 2;
    def.edges = edges;
    def.edge_count = 2;
    def.initial = 0;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_EEDGE_TO,
                            "order: missing destination before duplicate edge");
}

static void test_full_stack(void)
{
    static const fsm89_state_def states[] = {{0, {NULL, 0}, {NULL, 0}, 0},
                                             {0, {NULL, 0}, {NULL, 0}, 0}};
    static const fsm89_edge edges[] = {{0, 0, 0, {NULL, 0}},
                                       {0, 0, 0, {NULL, 0}}};
    fsm89_def def;
    fsm89_status rc;

    def.states = states;
    def.state_count = 2;
    def.edges = edges;
    def.edge_count = 2;
    def.initial = 7;
    rc = fsm89_validate(&def);
    fsm89_test_check_status(rc, FSM89_EDUP_STATE,
                            "order: duplicate state wins over the rest");
}

int main(void)
{
    test_def_beats_effects();
    test_effects_beats_dup_state();
    test_dup_state_beats_initial();
    test_initial_beats_edge_span();
    test_edge_span_beats_edge_from();
    test_edge_from_beats_edge_to();
    test_edge_to_beats_dup_edge();
    test_full_stack();
    return fsm89_test_report();
}
