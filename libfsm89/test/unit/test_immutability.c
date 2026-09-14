/* test_immutability.c - no public operation mutates a definition. */

#include "fsm89_fixtures.h"
#include "fsm89_test.h"

static int same_effects(fsm89_effects a, fsm89_effects b)
{
    size_t i;

    if (a.n != b.n)
    {
        return 0;
    }
    for (i = 0; i < a.n; ++i)
    {
        if (a.v[i] != b.v[i])
        {
            return 0;
        }
    }
    return 1;
}

static int same_defs(const fsm89_def *a, const fsm89_def *b)
{
    size_t i;

    if (a->state_count != b->state_count)
    {
        return 0;
    }
    if (a->edge_count != b->edge_count)
    {
        return 0;
    }
    if (a->initial != b->initial)
    {
        return 0;
    }
    for (i = 0; i < a->state_count; ++i)
    {
        if (a->states[i].id != b->states[i].id)
        {
            return 0;
        }
        if (a->states[i].accepting != b->states[i].accepting)
        {
            return 0;
        }
        if (same_effects(a->states[i].leave, b->states[i].leave) == 0)
        {
            return 0;
        }
        if (same_effects(a->states[i].enter, b->states[i].enter) == 0)
        {
            return 0;
        }
    }
    for (i = 0; i < a->edge_count; ++i)
    {
        if (a->edges[i].from != b->edges[i].from)
        {
            return 0;
        }
        if (a->edges[i].event != b->edges[i].event)
        {
            return 0;
        }
        if (a->edges[i].to != b->edges[i].to)
        {
            return 0;
        }
        if (same_effects(a->edges[i].effects, b->edges[i].effects) == 0)
        {
            return 0;
        }
    }
    return 1;
}

static int expected_accepting(const fsm89_def *def)
{
    size_t i;

    for (i = 0; i < def->state_count; ++i)
    {
        if (def->states[i].id == def->initial)
        {
            return def->states[i].accepting;
        }
    }
    return 0;
}

static void exercise(const fsm89_def *def)
{
    fsm89_state state;
    fsm89_effects enter;
    fsm89_step_result r;

    fsm89_test_check_status(fsm89_validate(def), FSM89_OK,
                            "immutability: validate");
    fsm89_test_check_status(fsm89_start(def, &state, &enter), FSM89_OK,
                            "immutability: start");
    fsm89_test_check_status(fsm89_step(def, def->initial, 0, &r), FSM89_OK,
                            "immutability: step success");
    fsm89_test_check_status(fsm89_step(def, def->initial, 4242UL, &r),
                            FSM89_NO_TRANSITION, "immutability: no transition");
    fsm89_test_check_status(fsm89_step(def, 4242UL, 0, &r), FSM89_ESTATE,
                            "immutability: unknown state");
    fsm89_test_check(fsm89_accepting(def, def->initial) ==
                         expected_accepting(def),
                     "immutability: accepting");
}

static void test_m1(void)
{
    static const fsm89_effect a_leave[] = {10, 11};
    static const fsm89_effect edge_fx[] = {20, 21};
    static const fsm89_effect b_enter[] = {30, 31};
    static const fsm89_state_def states[] = {{0, {a_leave, 2}, {NULL, 0}, 0},
                                             {1, {NULL, 0}, {b_enter, 2}, 0}};
    static const fsm89_edge edges[] = {{0, 0, 1, {edge_fx, 2}}};
    fsm89_def clone;

    clone.states = states;
    clone.state_count = 2;
    clone.edges = edges;
    clone.edge_count = 1;
    clone.initial = 0;

    fsm89_test_check(same_defs(&fsm89_fixture_m1, &clone) != 0,
                     "immutability: m1 clone matches");
    exercise(&fsm89_fixture_m1);
    fsm89_test_check(same_defs(&fsm89_fixture_m1, &clone) != 0,
                     "immutability: m1 unchanged after exercise");
}

static void test_m2(void)
{
    static const fsm89_effect leave_fx[] = {10};
    static const fsm89_effect edge_fx[] = {20};
    static const fsm89_effect enter_fx[] = {30};
    static const fsm89_state_def states[] = {
        {0, {leave_fx, 1}, {enter_fx, 1}, 0}};
    static const fsm89_edge edges[] = {{0, 0, 0, {edge_fx, 1}}};
    fsm89_def clone;

    clone.states = states;
    clone.state_count = 1;
    clone.edges = edges;
    clone.edge_count = 1;
    clone.initial = 0;

    exercise(&fsm89_fixture_m2);
    fsm89_test_check(same_defs(&fsm89_fixture_m2, &clone) != 0,
                     "immutability: m2 unchanged after exercise");
}

static void test_m3(void)
{
    static const fsm89_state_def states[] = {{0, {NULL, 0}, {NULL, 0}, 0},
                                             {1, {NULL, 0}, {NULL, 0}, 1},
                                             {2, {NULL, 0}, {NULL, 0}, 1}};
    static const fsm89_edge edges[] = {{0, 0, 1, {NULL, 0}},
                                       {1, 0, 1, {NULL, 0}},
                                       {1, 1, 2, {NULL, 0}},
                                       {2, 0, 2, {NULL, 0}}};
    fsm89_def clone;

    clone.states = states;
    clone.state_count = 3;
    clone.edges = edges;
    clone.edge_count = 4;
    clone.initial = 0;

    exercise(&fsm89_fixture_m3);
    fsm89_test_check(same_defs(&fsm89_fixture_m3, &clone) != 0,
                     "immutability: m3 unchanged after exercise");
}

int main(void)
{
    test_m1();
    test_m2();
    test_m3();
    return fsm89_test_report();
}
