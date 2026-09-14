/* fsm89_test.c - shared test harness for libfsm89. */

#include <stdio.h>

#include "fsm89_test.h"

int fsm89_test_failures = 0;
int fsm89_test_checks = 0;

void fsm89_test_check(int cond, const char *what)
{
    fsm89_test_checks += 1;
    if (cond == 0)
    {
        fsm89_test_failures += 1;
        fprintf(stderr, "FAIL: %s\n", what);
    }
}

void fsm89_test_check_status(int got, int want, const char *what)
{
    fsm89_test_checks += 1;
    if (got != want)
    {
        fsm89_test_failures += 1;
        fprintf(stderr, "FAIL: %s (got %d, want %d)\n", what, got, want);
    }
}

void fsm89_test_check_ul(unsigned long got, unsigned long want,
                         const char *what)
{
    fsm89_test_checks += 1;
    if (got != want)
    {
        fsm89_test_failures += 1;
        fprintf(stderr, "FAIL: %s (got %lu, want %lu)\n", what, got, want);
    }
}

void fsm89_test_check_size(size_t got, size_t want, const char *what)
{
    fsm89_test_checks += 1;
    if (got != want)
    {
        fsm89_test_failures += 1;
        fprintf(stderr, "FAIL: %s (got %lu, want %lu)\n", what,
                (unsigned long)got, (unsigned long)want);
    }
}

void fsm89_test_check_span(fsm89_effects got, const fsm89_effect *v, size_t n,
                           const char *what)
{
    fsm89_test_check(got.n == n, what);
    if (got.n != n)
    {
        return;
    }
    if (n == 0)
    {
        return;
    }
    fsm89_test_check(got.v == v, what);
}

void fsm89_test_check_span_eq(fsm89_effects got, const fsm89_effect *v,
                              size_t n, const char *what)
{
    size_t i;
    int ok;

    fsm89_test_check(got.n == n, what);
    if (got.n != n)
    {
        return;
    }
    ok = 1;
    for (i = 0; i < n; ++i)
    {
        if (got.v[i] != v[i])
        {
            ok = 0;
        }
    }
    fsm89_test_check(ok, what);
}

int fsm89_test_span_equal(fsm89_effects a, fsm89_effects b)
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

void fsm89_test_snapshot_take(fsm89_test_snapshot *out,
                              const fsm89_step_result *r)
{
    out->from = r->from;
    out->event = r->event;
    out->to = r->to;
    out->leave_v = r->leave.v;
    out->leave_n = r->leave.n;
    out->edge_v = r->edge.v;
    out->edge_n = r->edge.n;
    out->enter_v = r->enter.v;
    out->enter_n = r->enter.n;
}

void fsm89_test_unchanged(const fsm89_step_result *r,
                          const fsm89_test_snapshot *before, const char *what)
{
    fsm89_test_check(r->from == before->from, what);
    fsm89_test_check(r->event == before->event, what);
    fsm89_test_check(r->to == before->to, what);
    fsm89_test_check(r->leave.v == before->leave_v, what);
    fsm89_test_check(r->leave.n == before->leave_n, what);
    fsm89_test_check(r->edge.v == before->edge_v, what);
    fsm89_test_check(r->edge.n == before->edge_n, what);
    fsm89_test_check(r->enter.v == before->enter_v, what);
    fsm89_test_check(r->enter.n == before->enter_n, what);
}

static size_t copy_span(fsm89_effect *out, size_t at, size_t cap,
                        fsm89_effects span)
{
    size_t i;

    for (i = 0; i < span.n; ++i)
    {
        if (at + i < cap)
        {
            out[at + i] = span.v[i];
        }
    }
    return at + span.n;
}

size_t fsm89_test_flatten(const fsm89_step_result *r, fsm89_effect *out,
                          size_t cap)
{
    size_t at;

    at = 0;
    at = copy_span(out, at, cap, r->leave);
    at = copy_span(out, at, cap, r->edge);
    at = copy_span(out, at, cap, r->enter);
    return at;
}

void fsm89_test_expect_seq(const fsm89_step_result *r, const fsm89_effect *want,
                           size_t n, const char *what)
{
    fsm89_effect got[FSM89_TEST_MAX_EFFECTS];
    size_t got_n;
    size_t i;
    int ok;

    got_n = fsm89_test_flatten(r, got, FSM89_TEST_MAX_EFFECTS);
    ok = 1;
    if (got_n != n)
    {
        ok = 0;
    }
    for (i = 0; i < n; ++i)
    {
        if (i >= got_n)
        {
            break;
        }
        if (got[i] != want[i])
        {
            ok = 0;
        }
    }
    fsm89_test_check(ok, what);
}

unsigned long fsm89_test_rand(unsigned long *state)
{
    unsigned long x;

    x = *state;
    x = x ^ (x << 13);
    x = x ^ (x >> 7);
    x = x ^ (x << 17);
    *state = x;
    return x;
}

int fsm89_test_report(void)
{
    if (fsm89_test_failures != 0)
    {
        fprintf(stderr, "%d/%d checks FAILED\n", fsm89_test_failures,
                fsm89_test_checks);
        return 1;
    }
    printf("all %d checks passed\n", fsm89_test_checks);
    return 0;
}
