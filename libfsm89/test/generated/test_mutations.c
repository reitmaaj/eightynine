/* test_mutations.c - generated invalid machines and error precedence. */

#include "fsm89_gen.h"
#include "fsm89_test.h"

static unsigned long full_code(size_t nstates, size_t nevents)
{
    unsigned long code;
    unsigned long mul;
    size_t i;

    code = 0;
    mul = 1;
    for (i = 0; i < nstates * nevents; ++i)
    {
        code += mul;
        mul *= (unsigned long)(nstates + 1);
    }
    return code;
}

static void base_machine(fsm89_gen *m)
{
    fsm89_gen_from_code(m, 3, 2, full_code(3, 2));
}

static void test_single_defects(void)
{
    fsm89_gen m;

    base_machine(&m);
    m.states[1].id = m.states[0].id;
    fsm89_test_check_status(fsm89_validate(&m.def), FSM89_EDUP_STATE,
                            "mutation: duplicate state");

    base_machine(&m);
    m.def.initial = 99;
    fsm89_test_check_status(fsm89_validate(&m.def), FSM89_EINITIAL,
                            "mutation: missing initial");

    base_machine(&m);
    m.edges[0].from = 99;
    fsm89_test_check_status(fsm89_validate(&m.def), FSM89_EEDGE_FROM,
                            "mutation: missing source");

    base_machine(&m);
    m.edges[0].to = 99;
    fsm89_test_check_status(fsm89_validate(&m.def), FSM89_EEDGE_TO,
                            "mutation: missing destination");

    base_machine(&m);
    m.edges[1] = m.edges[0];
    fsm89_test_check_status(fsm89_validate(&m.def), FSM89_EDUP_EDGE,
                            "mutation: duplicate edge");

    base_machine(&m);
    m.states[0].leave.v = NULL;
    m.states[0].leave.n = 1;
    fsm89_test_check_status(fsm89_validate(&m.def), FSM89_EEFFECTS,
                            "mutation: NULL leave span");

    base_machine(&m);
    m.states[0].enter.v = NULL;
    m.states[0].enter.n = 1;
    fsm89_test_check_status(fsm89_validate(&m.def), FSM89_EEFFECTS,
                            "mutation: NULL enter span");

    base_machine(&m);
    m.edges[0].effects.v = NULL;
    m.edges[0].effects.n = 1;
    fsm89_test_check_status(fsm89_validate(&m.def), FSM89_EEFFECTS,
                            "mutation: NULL edge span");

    base_machine(&m);
    m.def.state_count = 0;
    fsm89_test_check_status(fsm89_validate(&m.def), FSM89_EDEF,
                            "mutation: zero states");

    base_machine(&m);
    m.def.edges = NULL;
    fsm89_test_check_status(fsm89_validate(&m.def), FSM89_EDEF,
                            "mutation: NULL edges");
}

static void test_precedence(void)
{
    fsm89_gen m;

    base_machine(&m);
    m.states[1].id = m.states[0].id;
    m.def.initial = 99;
    m.edges[1] = m.edges[0];
    fsm89_test_check_status(fsm89_validate(&m.def), FSM89_EDUP_STATE,
                            "mutation: duplicate state wins");

    base_machine(&m);
    m.def.initial = 99;
    m.edges[1] = m.edges[0];
    fsm89_test_check_status(fsm89_validate(&m.def), FSM89_EINITIAL,
                            "mutation: missing initial wins");

    base_machine(&m);
    m.edges[0].effects.v = NULL;
    m.edges[0].effects.n = 1;
    m.edges[0].from = 99;
    fsm89_test_check_status(fsm89_validate(&m.def), FSM89_EEFFECTS,
                            "mutation: bad edge span wins");

    base_machine(&m);
    m.edges[0].from = 99;
    m.edges[1].to = 99;
    fsm89_test_check_status(fsm89_validate(&m.def), FSM89_EEDGE_FROM,
                            "mutation: missing source wins");

    base_machine(&m);
    m.edges[0].to = 99;
    m.edges[1] = m.edges[0];
    fsm89_test_check_status(fsm89_validate(&m.def), FSM89_EEDGE_TO,
                            "mutation: missing destination wins");
}

int main(void)
{
    test_single_defects();
    test_precedence();
    return fsm89_test_report();
}
