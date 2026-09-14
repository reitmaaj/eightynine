/* fsm89_gen.c - deterministic machine generation and differential checks. */

#include "fsm89_gen.h"
#include "fsm89_test.h"
#include "ref_fsm89.h"

static void fill_span(unsigned long *seed, fsm89_effects *span,
                      fsm89_effect *buf, size_t cap)
{
    size_t n;
    size_t i;

    n = (size_t)(fsm89_test_rand(seed) % (unsigned long)(cap + 1));
    for (i = 0; i < n; ++i)
    {
        buf[i] = (fsm89_effect)(fsm89_test_rand(seed) % 10UL);
    }
    span->n = n;
    span->v = buf;
    if (n == 0)
    {
        if ((fsm89_test_rand(seed) & 1UL) != 0)
        {
            span->v = NULL;
        }
    }
}

static void set_state_spans(fsm89_gen *g, size_t s)
{
    size_t n;

    n = s % 3;
    g->state_fx[s][0][0] = (fsm89_effect)(s + 1);
    g->state_fx[s][0][1] = (fsm89_effect)(s + 1);
    g->states[s].leave.n = n;
    g->states[s].leave.v = g->state_fx[s][0];
    if (n == 0)
    {
        g->states[s].leave.v = NULL;
    }
    n = (s + 1) % 3;
    g->state_fx[s][1][0] = (fsm89_effect)(s + 2);
    g->state_fx[s][1][1] = (fsm89_effect)(s + 2);
    g->states[s].enter.n = n;
    g->states[s].enter.v = g->state_fx[s][1];
    if (n == 0)
    {
        g->states[s].enter.v = NULL;
    }
}

void fsm89_gen_from_code(fsm89_gen *g, size_t nstates, size_t nevents,
                         unsigned long code)
{
    unsigned long digit[FSM89_GEN_MAX_STATES * FSM89_GEN_MAX_EVENTS];
    size_t cell;
    size_t s;
    size_t e;
    size_t k;
    size_t n;
    size_t base;

    base = nstates + 1;
    for (cell = 0; cell < nstates * nevents; ++cell)
    {
        digit[cell] = code % (unsigned long)base;
        code = code / (unsigned long)base;
    }
    for (s = 0; s < nstates; ++s)
    {
        g->states[s].id = (fsm89_state)s;
        g->states[s].accepting = (int)(s % 2);
        set_state_spans(g, s);
    }
    k = 0;
    for (cell = 0; cell < nstates * nevents; ++cell)
    {
        if (digit[cell] == 0)
        {
            continue;
        }
        s = cell / nevents;
        e = cell % nevents;
        g->edges[k].from = (fsm89_state)s;
        g->edges[k].event = (fsm89_event)e;
        g->edges[k].to = (fsm89_state)(digit[cell] - 1);
        n = k % 3;
        g->edge_fx[k][0] = (fsm89_effect)(k + 1);
        g->edge_fx[k][1] = (fsm89_effect)(k + 1);
        g->edges[k].effects.n = n;
        g->edges[k].effects.v = g->edge_fx[k];
        if (n == 0)
        {
            g->edges[k].effects.v = NULL;
        }
        k += 1;
    }
    g->def.states = g->states;
    g->def.state_count = nstates;
    g->def.edges = g->edges;
    g->def.edge_count = k;
    g->def.initial = 0;
}

void fsm89_gen_reorder(fsm89_gen *dst, const fsm89_gen *src, size_t nstates)
{
    size_t i;
    size_t k;

    for (i = 0; i < nstates; ++i)
    {
        dst->states[i] = src->states[nstates - 1 - i];
        dst->state_fx[i][0][0] = src->state_fx[nstates - 1 - i][0][0];
        dst->state_fx[i][0][1] = src->state_fx[nstates - 1 - i][0][1];
        dst->state_fx[i][1][0] = src->state_fx[nstates - 1 - i][1][0];
        dst->state_fx[i][1][1] = src->state_fx[nstates - 1 - i][1][1];
        if (dst->states[i].leave.n != 0)
        {
            dst->states[i].leave.v = dst->state_fx[i][0];
        }
        if (dst->states[i].enter.n != 0)
        {
            dst->states[i].enter.v = dst->state_fx[i][1];
        }
    }
    for (k = 0; k < src->def.edge_count; ++k)
    {
        dst->edges[k] = src->edges[src->def.edge_count - 1 - k];
        dst->edge_fx[k][0] = src->edge_fx[src->def.edge_count - 1 - k][0];
        dst->edge_fx[k][1] = src->edge_fx[src->def.edge_count - 1 - k][1];
        if (dst->edges[k].effects.n != 0)
        {
            dst->edges[k].effects.v = dst->edge_fx[k];
        }
    }
    dst->def.states = dst->states;
    dst->def.state_count = nstates;
    dst->def.edges = dst->edges;
    dst->def.edge_count = src->def.edge_count;
    dst->def.initial = src->def.initial;
}

void fsm89_gen_rename(fsm89_gen *dst, const fsm89_gen *src, size_t nstates,
                      size_t nevents, unsigned long state_delta,
                      unsigned long event_delta, unsigned long effect_delta)
{
    size_t i;
    size_t k;
    size_t j;

    (void)nevents;
    for (i = 0; i < nstates; ++i)
    {
        dst->states[i] = src->states[i];
        dst->states[i].id = src->states[i].id + state_delta;
        for (j = 0; j < 2; ++j)
        {
            dst->state_fx[i][0][j] = src->state_fx[i][0][j] + effect_delta;
            dst->state_fx[i][1][j] = src->state_fx[i][1][j] + effect_delta;
        }
        if (dst->states[i].leave.n != 0)
        {
            dst->states[i].leave.v = dst->state_fx[i][0];
        }
        if (dst->states[i].enter.n != 0)
        {
            dst->states[i].enter.v = dst->state_fx[i][1];
        }
    }
    for (k = 0; k < src->def.edge_count; ++k)
    {
        dst->edges[k] = src->edges[k];
        dst->edges[k].from = src->edges[k].from + state_delta;
        dst->edges[k].event = src->edges[k].event + event_delta;
        dst->edges[k].to = src->edges[k].to + state_delta;
        for (j = 0; j < 2; ++j)
        {
            dst->edge_fx[k][j] = src->edge_fx[k][j] + effect_delta;
        }
        if (dst->edges[k].effects.n != 0)
        {
            dst->edges[k].effects.v = dst->edge_fx[k];
        }
    }
    dst->def.states = dst->states;
    dst->def.state_count = nstates;
    dst->def.edges = dst->edges;
    dst->def.edge_count = src->def.edge_count;
    dst->def.initial = src->def.initial + state_delta;
}

void fsm89_gen_random(fsm89_gen *g, unsigned long *seed, size_t nstates,
                      size_t nevents)
{
    size_t s;
    size_t e;
    size_t n;

    n = 0;
    for (s = 0; s < nstates; ++s)
    {
        g->states[s].id = (fsm89_state)s;
        g->states[s].accepting = (int)(fsm89_test_rand(seed) % 2UL);
        fill_span(seed, &g->states[s].leave, g->state_fx[s][0],
                  FSM89_GEN_MAX_FX);
        fill_span(seed, &g->states[s].enter, g->state_fx[s][1],
                  FSM89_GEN_MAX_FX);
        for (e = 0; e < nevents; ++e)
        {
            if ((fsm89_test_rand(seed) % 3UL) == 0)
            {
                continue;
            }
            g->edges[n].from = (fsm89_state)s;
            g->edges[n].event = (fsm89_event)e;
            g->edges[n].to =
                (fsm89_state)(fsm89_test_rand(seed) % (unsigned long)nstates);
            fill_span(seed, &g->edges[n].effects, g->edge_fx[n],
                      FSM89_GEN_MAX_FX);
            n += 1;
        }
    }
    g->def.states = g->states;
    g->def.state_count = nstates;
    g->def.edges = g->edges;
    g->def.edge_count = n;
    g->def.initial =
        (fsm89_state)(fsm89_test_rand(seed) % (unsigned long)nstates);
}

static int span_matches(fsm89_effects got, const fsm89_effect *want, size_t n)
{
    size_t i;

    if (got.n != n)
    {
        return 0;
    }
    for (i = 0; i < n; ++i)
    {
        if (got.v[i] != want[i])
        {
            return 0;
        }
    }
    return 1;
}

void fsm89_gen_compare(const fsm89_def *def, size_t nstates, size_t nevents,
                       const char *label)
{
    fsm89_step_result r;
    ref_step ref;
    fsm89_status rc;
    size_t s;
    size_t e;
    int ok;

    for (s = 0; s <= nstates; ++s)
    {
        for (e = 0; e <= nevents; ++e)
        {
            rc = fsm89_step(def, (fsm89_state)s, (fsm89_event)e, &r);
            ref_step_eval(def, (fsm89_state)s, (fsm89_event)e, &ref);
            ok = 0;
            if (ref.found < 0)
            {
                if (rc == FSM89_ESTATE)
                {
                    ok = 1;
                }
            }
            else if (ref.found == 0)
            {
                if (rc == FSM89_NO_TRANSITION)
                {
                    ok = 1;
                }
            }
            else
            {
                if (rc == FSM89_OK)
                {
                    ok = 1;
                }
            }
            fsm89_test_check(ok != 0, label);
            if (rc != FSM89_OK)
            {
                continue;
            }
            fsm89_test_check(r.from == (fsm89_state)s, label);
            fsm89_test_check(r.event == (fsm89_event)e, label);
            fsm89_test_check(r.to == ref.to, label);
            fsm89_test_check(span_matches(r.leave, ref.source->leave.v,
                                          ref.source->leave.n) != 0,
                             label);
            fsm89_test_check(span_matches(r.edge, ref.edge->effects.v,
                                          ref.edge->effects.n) != 0,
                             label);
            fsm89_test_check(span_matches(r.enter, ref.dest->enter.v,
                                          ref.dest->enter.n) != 0,
                             label);
        }
    }
    for (s = 0; s < nstates; ++s)
    {
        fsm89_test_check(fsm89_accepting(def, (fsm89_state)s) ==
                             ref_accepting(def, (fsm89_state)s),
                         label);
    }
    fsm89_test_check(fsm89_accepting(def, (fsm89_state)nstates) == 0, label);
}
