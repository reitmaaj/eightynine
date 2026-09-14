/* fsm89_step.c - one transition evaluation. */

#include "fsm89_internal.h"

fsm89_status fsm89_step(const fsm89_def *def, fsm89_state state,
                        fsm89_event event, fsm89_step_result *result)
{
    const fsm89_state_def *source;
    const fsm89_state_def *dest;
    const fsm89_edge *edge;

    source = fsm89__find_state(def, state);
    if (source == NULL)
    {
        return FSM89_ESTATE;
    }
    edge = fsm89__find_edge(def, state, event);
    if (edge == NULL)
    {
        return FSM89_NO_TRANSITION;
    }
    dest = fsm89__find_state(def, edge->to);
    result->from = state;
    result->event = event;
    result->to = edge->to;
    result->leave.v = source->leave.v;
    result->leave.n = source->leave.n;
    result->edge.v = edge->effects.v;
    result->edge.n = edge->effects.n;
    result->enter.v = dest->enter.v;
    result->enter.n = dest->enter.n;
    return FSM89_OK;
}
