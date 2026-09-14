/* test_generated.c - exhaustive small-machine enumeration. */

#include "fsm89_gen.h"
#include "fsm89_test.h"

#ifdef FSM89_GENERATED_DEEP
#define SWEEP_N 3
#define SWEEP_E 3
#else
#define SWEEP_N 3
#define SWEEP_E 2
#endif

static void compare_machines(const fsm89_def *a, const fsm89_def *b,
                             size_t nstates, size_t nevents, const char *label)
{
    fsm89_step_result ra;
    fsm89_step_result rb;
    fsm89_status rca;
    fsm89_status rcb;
    size_t s;
    size_t e;

    for (s = 0; s <= nstates; ++s)
    {
        for (e = 0; e <= nevents; ++e)
        {
            rca = fsm89_step(a, (fsm89_state)s, (fsm89_event)e, &ra);
            rcb = fsm89_step(b, (fsm89_state)s, (fsm89_event)e, &rb);
            fsm89_test_check_status(rcb, rca, label);
            if (rca != FSM89_OK)
            {
                continue;
            }
            fsm89_test_check_ul(rb.to, ra.to, label);
            fsm89_test_check(fsm89_test_span_equal(ra.leave, rb.leave) != 0,
                             label);
            fsm89_test_check(fsm89_test_span_equal(ra.edge, rb.edge) != 0,
                             label);
            fsm89_test_check(fsm89_test_span_equal(ra.enter, rb.enter) != 0,
                             label);
        }
    }
    for (s = 0; s < nstates; ++s)
    {
        fsm89_test_check(fsm89_accepting(a, (fsm89_state)s) ==
                             fsm89_accepting(b, (fsm89_state)s),
                         label);
    }
}

static int mapped_span(fsm89_effects a, fsm89_effects b, unsigned long delta)
{
    size_t i;

    if (a.n != b.n)
    {
        return 0;
    }
    for (i = 0; i < a.n; ++i)
    {
        if (b.v[i] != a.v[i] + delta)
        {
            return 0;
        }
    }
    return 1;
}

static void compare_renamed(const fsm89_def *a, const fsm89_def *b,
                            size_t nstates, size_t nevents, const char *label)
{
    fsm89_step_result ra;
    fsm89_step_result rb;
    fsm89_status rca;
    fsm89_status rcb;
    size_t s;
    size_t e;

    for (s = 0; s < nstates; ++s)
    {
        for (e = 0; e < nevents; ++e)
        {
            rca = fsm89_step(a, (fsm89_state)s, (fsm89_event)e, &ra);
            rcb = fsm89_step(b, (fsm89_state)(s + 100), (fsm89_event)(e + 1000),
                             &rb);
            fsm89_test_check_status(rcb, rca, label);
            if (rca != FSM89_OK)
            {
                continue;
            }
            fsm89_test_check_ul(rb.to, ra.to + 100, label);
            fsm89_test_check(mapped_span(ra.leave, rb.leave, 7) != 0, label);
            fsm89_test_check(mapped_span(ra.edge, rb.edge, 7) != 0, label);
            fsm89_test_check(mapped_span(ra.enter, rb.enter, 7) != 0, label);
        }
    }
}

static void run_machine(const fsm89_gen *m, size_t nstates, size_t nevents)
{
    fsm89_gen reordered;
    fsm89_gen renamed;
    fsm89_state state;
    fsm89_effects enter;
    fsm89_status rc;

    rc = fsm89_validate(&m->def);
    fsm89_test_check_status(rc, FSM89_OK, "generated: validates");
    fsm89_gen_compare(&m->def, nstates, nevents, "generated: reference");

    rc = fsm89_start(&m->def, &state, &enter);
    fsm89_test_check_status(rc, FSM89_OK, "generated: start");
    fsm89_test_check_ul(state, 0, "generated: start state");
    fsm89_test_check(fsm89_test_span_equal(enter, m->states[0].enter) != 0,
                     "generated: start enter span");

    fsm89_gen_reorder(&reordered, m, nstates);
    rc = fsm89_validate(&reordered.def);
    fsm89_test_check_status(rc, FSM89_OK, "generated: reordered validates");
    compare_machines(&m->def, &reordered.def, nstates, nevents,
                     "generated: reorder equivalence");

    fsm89_gen_rename(&renamed, m, nstates, nevents, 100, 1000, 7);
    rc = fsm89_validate(&renamed.def);
    fsm89_test_check_status(rc, FSM89_OK, "generated: renamed validates");
    compare_renamed(&m->def, &renamed.def, nstates, nevents,
                    "generated: rename equivalence");
}

static void sweep(size_t max_nstates, size_t max_nevents)
{
    fsm89_gen m;
    unsigned long total;
    unsigned long code;
    size_t nstates;
    size_t nevents;
    size_t i;
    size_t base;

    for (nstates = 1; nstates <= max_nstates; ++nstates)
    {
        for (nevents = 1; nevents <= max_nevents; ++nevents)
        {
            base = nstates + 1;
            total = 1;
            for (i = 0; i < nstates * nevents; ++i)
            {
                total = total * (unsigned long)base;
            }
            for (code = 0; code < total; ++code)
            {
                fsm89_gen_from_code(&m, nstates, nevents, code);
                run_machine(&m, nstates, nevents);
            }
        }
    }
}

int main(void)
{
    sweep(SWEEP_N, SWEEP_E);
    return fsm89_test_report();
}
