/* fsm89_validate.c - the eight-stage definition validator. */

#include "fsm89_internal.h"

static fsm89_status check_top(const fsm89_def *def)
{
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
    if (def->edge_count == 0)
    {
        return FSM89_OK;
    }
    if (def->edges == NULL)
    {
        return FSM89_EDEF;
    }
    return FSM89_OK;
}

static fsm89_status check_state_spans(const fsm89_def *def)
{
    size_t i;

    for (i = 0; i < def->state_count; ++i)
    {
        if (fsm89__span_ok(def->states[i].leave) == 0)
        {
            return FSM89_EEFFECTS;
        }
        if (fsm89__span_ok(def->states[i].enter) == 0)
        {
            return FSM89_EEFFECTS;
        }
    }
    return FSM89_OK;
}

static fsm89_status check_dup_states(const fsm89_def *def)
{
    size_t i;
    size_t j;

    for (i = 0; i < def->state_count; ++i)
    {
        for (j = i + 1; j < def->state_count; ++j)
        {
            if (def->states[i].id == def->states[j].id)
            {
                return FSM89_EDUP_STATE;
            }
        }
    }
    return FSM89_OK;
}

static fsm89_status check_initial(const fsm89_def *def)
{
    size_t i;

    for (i = 0; i < def->state_count; ++i)
    {
        if (def->states[i].id == def->initial)
        {
            return FSM89_OK;
        }
    }
    return FSM89_EINITIAL;
}

static fsm89_status check_edge_spans(const fsm89_def *def)
{
    size_t i;

    for (i = 0; i < def->edge_count; ++i)
    {
        if (fsm89__span_ok(def->edges[i].effects) == 0)
        {
            return FSM89_EEFFECTS;
        }
    }
    return FSM89_OK;
}

static fsm89_status check_edge_from(const fsm89_def *def)
{
    size_t i;

    for (i = 0; i < def->edge_count; ++i)
    {
        if (fsm89__find_state(def, def->edges[i].from) == NULL)
        {
            return FSM89_EEDGE_FROM;
        }
    }
    return FSM89_OK;
}

static fsm89_status check_edge_to(const fsm89_def *def)
{
    size_t i;

    for (i = 0; i < def->edge_count; ++i)
    {
        if (fsm89__find_state(def, def->edges[i].to) == NULL)
        {
            return FSM89_EEDGE_TO;
        }
    }
    return FSM89_OK;
}

static fsm89_status check_dup_edges(const fsm89_def *def)
{
    size_t i;
    size_t j;

    for (i = 0; i < def->edge_count; ++i)
    {
        for (j = i + 1; j < def->edge_count; ++j)
        {
            if (def->edges[i].from == def->edges[j].from)
            {
                if (def->edges[i].event == def->edges[j].event)
                {
                    return FSM89_EDUP_EDGE;
                }
            }
        }
    }
    return FSM89_OK;
}

fsm89_status fsm89_validate(const fsm89_def *def)
{
    fsm89_status st;

    st = check_top(def);
    if (st != FSM89_OK)
    {
        return st;
    }
    st = check_state_spans(def);
    if (st != FSM89_OK)
    {
        return st;
    }
    st = check_dup_states(def);
    if (st != FSM89_OK)
    {
        return st;
    }
    st = check_initial(def);
    if (st != FSM89_OK)
    {
        return st;
    }
    st = check_edge_spans(def);
    if (st != FSM89_OK)
    {
        return st;
    }
    st = check_edge_from(def);
    if (st != FSM89_OK)
    {
        return st;
    }
    st = check_edge_to(def);
    if (st != FSM89_OK)
    {
        return st;
    }
    st = check_dup_edges(def);
    if (st != FSM89_OK)
    {
        return st;
    }
    return FSM89_OK;
}
