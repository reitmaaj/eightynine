/* fsm89_state.c - startup and acceptance queries. */

#include "fsm89_internal.h"

fsm89_status fsm89_start(const fsm89_def *def, fsm89_state *state,
                         fsm89_effects *enter)
{
    const fsm89_state_def *initial;

    initial = fsm89__find_state(def, def->initial);
    *state = def->initial;
    enter->v = initial->enter.v;
    enter->n = initial->enter.n;
    return FSM89_OK;
}

int fsm89_accepting(const fsm89_def *def, fsm89_state state)
{
    const fsm89_state_def *s;

    s = fsm89__find_state(def, state);
    if (s == NULL)
    {
        return 0;
    }
    return s->accepting;
}
