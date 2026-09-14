/* fsm89_lookup.c - pure span and table lookups shared by the library. */

#include "fsm89_internal.h"

int fsm89__span_ok(fsm89_effects span)
{
    if (span.n == 0)
    {
        return 1;
    }
    if (span.v == NULL)
    {
        return 0;
    }
    return 1;
}

const fsm89_state_def *fsm89__find_state(const fsm89_def *def, fsm89_state id)
{
    size_t i;

    for (i = 0; i < def->state_count; ++i)
    {
        if (def->states[i].id == id)
        {
            return &def->states[i];
        }
    }
    return NULL;
}

const fsm89_edge *fsm89__find_edge(const fsm89_def *def, fsm89_state from,
                                   fsm89_event event)
{
    size_t i;

    for (i = 0; i < def->edge_count; ++i)
    {
        if (def->edges[i].from == from)
        {
            if (def->edges[i].event == event)
            {
                return &def->edges[i];
            }
        }
    }
    return NULL;
}
