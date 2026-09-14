/* ref_fsm89.c - independent linear-scan reference model. */

#include "ref_fsm89.h"

static int ref_span_bad(fsm89_effects span)
{
    if (span.n == 0)
    {
        return 0;
    }
    if (span.v == NULL)
    {
        return 1;
    }
    return 0;
}

static int ref_has_state(const fsm89_def *def, fsm89_state id)
{
    size_t i;

    for (i = 0; i < def->state_count; ++i)
    {
        if (def->states[i].id == id)
        {
            return 1;
        }
    }
    return 0;
}

static int ref_dup_state(const fsm89_def *def)
{
    size_t i;
    size_t j;

    for (i = 0; i < def->state_count; ++i)
    {
        for (j = 0; j < def->state_count; ++j)
        {
            if (i == j)
            {
                continue;
            }
            if (def->states[i].id == def->states[j].id)
            {
                return 1;
            }
        }
    }
    return 0;
}

static int ref_dup_edge(const fsm89_def *def)
{
    size_t i;
    size_t j;

    for (i = 0; i < def->edge_count; ++i)
    {
        for (j = 0; j < def->edge_count; ++j)
        {
            if (i == j)
            {
                continue;
            }
            if (def->edges[i].from != def->edges[j].from)
            {
                continue;
            }
            if (def->edges[i].event == def->edges[j].event)
            {
                return 1;
            }
        }
    }
    return 0;
}

int ref_validate(const fsm89_def *def)
{
    size_t i;

    if (def == NULL)
    {
        return FSM89_EDEF;
    }
    if (def->state_count == 0)
    {
        return FSM89_EDEF;
    }
    if (def->states == NULL)
    {
        return FSM89_EDEF;
    }
    if (def->edge_count > 0)
    {
        if (def->edges == NULL)
        {
            return FSM89_EDEF;
        }
    }
    for (i = 0; i < def->state_count; ++i)
    {
        if (ref_span_bad(def->states[i].leave) != 0)
        {
            return FSM89_EEFFECTS;
        }
        if (ref_span_bad(def->states[i].enter) != 0)
        {
            return FSM89_EEFFECTS;
        }
    }
    if (ref_dup_state(def) != 0)
    {
        return FSM89_EDUP_STATE;
    }
    if (ref_has_state(def, def->initial) == 0)
    {
        return FSM89_EINITIAL;
    }
    for (i = 0; i < def->edge_count; ++i)
    {
        if (ref_span_bad(def->edges[i].effects) != 0)
        {
            return FSM89_EEFFECTS;
        }
    }
    for (i = 0; i < def->edge_count; ++i)
    {
        if (ref_has_state(def, def->edges[i].from) == 0)
        {
            return FSM89_EEDGE_FROM;
        }
    }
    for (i = 0; i < def->edge_count; ++i)
    {
        if (ref_has_state(def, def->edges[i].to) == 0)
        {
            return FSM89_EEDGE_TO;
        }
    }
    if (ref_dup_edge(def) != 0)
    {
        return FSM89_EDUP_EDGE;
    }
    return FSM89_OK;
}

static const fsm89_state_def *ref_source(const fsm89_def *def,
                                         fsm89_state state)
{
    size_t i;

    for (i = 0; i < def->state_count; ++i)
    {
        if (def->states[i].id == state)
        {
            return &def->states[i];
        }
    }
    return NULL;
}

static const fsm89_edge *ref_edge(const fsm89_def *def, fsm89_state from,
                                  fsm89_event event)
{
    size_t i;

    for (i = 0; i < def->edge_count; ++i)
    {
        if (def->edges[i].from != from)
        {
            continue;
        }
        if (def->edges[i].event == event)
        {
            return &def->edges[i];
        }
    }
    return NULL;
}

void ref_step_eval(const fsm89_def *def, fsm89_state state, fsm89_event event,
                   ref_step *out)
{
    out->found = 0;
    out->to = 0;
    out->source = NULL;
    out->edge = NULL;
    out->dest = NULL;

    out->source = ref_source(def, state);
    if (out->source == NULL)
    {
        out->found = -1;
        return;
    }
    out->edge = ref_edge(def, state, event);
    if (out->edge == NULL)
    {
        return;
    }
    out->found = 1;
    out->to = out->edge->to;
    out->dest = ref_source(def, out->edge->to);
}

int ref_accepting(const fsm89_def *def, fsm89_state state)
{
    const fsm89_state_def *s;

    s = ref_source(def, state);
    if (s == NULL)
    {
        return 0;
    }
    return s->accepting;
}
