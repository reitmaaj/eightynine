/* test_hardening.c - length/arithmetic boundaries (acceptance G, P02) and
 * allocation-failure sweeps (acceptance H, P03) for the static transforms.
 *
 * The library's allocations route through bwt89_malloc/bwt89_realloc so the
 * test build can (a) count them and (b) make the kth attempt fail. Every
 * failure must return BWT89_NOMEM and leave zero live allocations and the
 * caller's output untouched. The direct SA-IS sweep also regresses the
 * recursive-arena leak (P03). */
#include <limits.h>
#include <stddef.h>

#include "test.h"

#include <bwt89_internal.h>

static unsigned long rng = 0x9E3779B9UL;

static unsigned long rnd(void)
{
    rng = rng * 1664525UL + 1013904223UL;
    return rng;
}

static void fill_ax(unsigned char *b, size_t n)
{
    size_t i;
    for (i = 0; i < n; ++i)
    {
        b[i] = (unsigned char)0xA5;
    }
}

static int same_bytes(const unsigned char *a, const unsigned char *b, size_t n)
{
    size_t i;
    for (i = 0; i < n; ++i)
    {
        if (a[i] != b[i])
        {
            return 0;
        }
    }
    return 1;
}

/* --- G02/G03: one above the maximum is rejected before access/allocation. */

static void expect_too_large_one(size_t n)
{
    unsigned char in[1];
    unsigned char out[2];
    unsigned char ref[2];
    size_t index;
    enum bwt89_status st;
    in[0] = 0x42;
    index = (size_t)-1;
    fill_ax(out, 2);
    memcpy(ref, out, 2);

    bwt89_test_reset_stats();
    st = bwt89_bwt(in, n, &index, out);
    CHECK(st == BWT89_TOO_LARGE);
    CHECK(bwt89_test_alloc_calls() == 0);
    CHECK(index == (size_t)-1);
    CHECK(same_bytes(out, ref, 2));

    bwt89_test_reset_stats();
    st = bwt89_ibwt(in, n, 0, out);
    CHECK(st == BWT89_TOO_LARGE);
    CHECK(bwt89_test_alloc_calls() == 0);
    CHECK(same_bytes(out, ref, 2));

    bwt89_test_reset_stats();
    st = bwt89_bbwt(in, n, out);
    CHECK(st == BWT89_TOO_LARGE);
    CHECK(bwt89_test_alloc_calls() == 0);
    CHECK(same_bytes(out, ref, 2));

    bwt89_test_reset_stats();
    st = bwt89_ibbwt(in, n, out);
    CHECK(st == BWT89_TOO_LARGE);
    CHECK(bwt89_test_alloc_calls() == 0);
    CHECK(same_bytes(out, ref, 2));
}

static void test_one_above_max(void)
{
    expect_too_large_one(BWT89_MAX_N + 1);
    expect_too_large_one((size_t)-1); /* SIZE_MAX */
}

/* --- G04: exactly BWT89_MAX_N passes size validation. Under a forced
 * first-allocation failure the huge request is shimmed to NULL, so the call
 * must report BWT89_NOMEM (not BWT89_TOO_LARGE) without touching the system
 * allocator for the enormous size. */

static void test_max_n_allocates(void)
{
    unsigned char in[1];
    unsigned char out[2];
    size_t index;
    enum bwt89_status st;
    in[0] = 0x01;
    index = 0;

    bwt89_test_reset_stats();
    bwt89_test_fail_alloc_at(1);
    st = bwt89_bwt(in, BWT89_MAX_N, &index, out);
    CHECK(st == BWT89_NOMEM);

    bwt89_test_reset_stats();
    bwt89_test_fail_alloc_at(1);
    st = bwt89_ibwt(in, BWT89_MAX_N, 0, out);
    CHECK(st == BWT89_NOMEM);

    bwt89_test_reset_stats();
    bwt89_test_fail_alloc_at(1);
    st = bwt89_bbwt(in, BWT89_MAX_N, out);
    CHECK(st == BWT89_NOMEM);

    bwt89_test_reset_stats();
    bwt89_test_fail_alloc_at(1);
    st = bwt89_ibbwt(in, BWT89_MAX_N, out);
    CHECK(st == BWT89_NOMEM);
    bwt89_test_disable_alloc_failure();
}

/* --- allocation-failure sweep core --------------------------------------- */

struct ctx_buf
{
    unsigned char data[256];
    size_t n;
    size_t index;
};

static enum bwt89_status run_bwt(void *p, size_t *index, unsigned char *out)
{
    struct ctx_buf *c;
    c = (struct ctx_buf *)p;
    return bwt89_bwt(c->data, c->n, index, out);
}

static enum bwt89_status run_ibwt(void *p, size_t *index, unsigned char *out)
{
    struct ctx_buf *c;
    (void)index;
    c = (struct ctx_buf *)p;
    return bwt89_ibwt(c->data, c->n, c->index, out);
}

static enum bwt89_status run_bbwt(void *p, size_t *index, unsigned char *out)
{
    struct ctx_buf *c;
    (void)index;
    c = (struct ctx_buf *)p;
    return bwt89_bbwt(c->data, c->n, out);
}

static enum bwt89_status run_ibbwt(void *p, size_t *index, unsigned char *out)
{
    struct ctx_buf *c;
    (void)index;
    c = (struct ctx_buf *)p;
    return bwt89_ibbwt(c->data, c->n, out);
}

/* Fail each allocation ordinal of a transform whose success path performs
 * `run`. On every injected failure the call must report BWT89_NOMEM, leave
 * zero live allocations, and leave index and out byte-identical. */
static void sweep_transform(enum bwt89_status (*run)(void *, size_t *,
                                                     unsigned char *),
                            struct ctx_buf *ctx, size_t n)
{
    unsigned char out[256];
    unsigned char gold[256];
    unsigned long total;
    unsigned long k;
    size_t idx;
    enum bwt89_status st;
    fill_ax(out, n);
    bwt89_test_reset_stats();
    st = run(ctx, &idx, out);
    CHECK(st == BWT89_OK);
    total = bwt89_test_alloc_calls();
    CHECK(total > 0);
    memcpy(gold, out, n);
    for (k = 1; k <= total; ++k)
    {
        size_t index;
        memcpy(out, gold, n);
        index = (size_t)-1;
        bwt89_test_reset_stats();
        bwt89_test_fail_alloc_at(k);
        st = run(ctx, &index, out);
        CHECK(st == BWT89_NOMEM);
        CHECK(bwt89_test_live_allocs() == 0);
        CHECK(same_bytes(out, gold, n));
        CHECK(index == (size_t)-1);
    }
    bwt89_test_reset_stats();
    bwt89_test_fail_alloc_at(total + 1);
    st = run(ctx, &idx, out);
    CHECK(st == BWT89_OK);
    bwt89_test_disable_alloc_failure();
}

static void test_alloc_sweeps(void)
{
    struct ctx_buf c;
    unsigned char text[256];
    unsigned char tf[256];
    size_t n;
    size_t i;
    size_t idx;
    enum bwt89_status st;
    for (i = 0; i < sizeof c.data; ++i)
    {
        text[i] = (unsigned char)(rnd() & 0xFF);
    }
    c.n = 0;
    for (n = 1; n <= 128; ++n)
    {
        memcpy(c.data, text, n);
        c.n = n;
        sweep_transform(run_bwt, &c, n);
        sweep_transform(run_bbwt, &c, n);

        /* ibwt wants the forward transform bytes and the true index. */
        idx = 0;
        st = bwt89_bwt(text, n, &idx, tf);
        CHECK(st == BWT89_OK);
        memcpy(c.data, tf, n);
        c.index = idx;
        sweep_transform(run_ibwt, &c, n);

        /* ibbwt wants the bijective transform bytes as input. */
        st = bwt89_bbwt(text, n, tf);
        CHECK(st == BWT89_OK);
        memcpy(c.data, tf, n);
        sweep_transform(run_ibbwt, &c, n);
    }
}

/* --- SA-IS recursion sweep (H05, P03) ------------------------------------- */

static void test_sa_sweep(void)
{
    static const int fixture[] = {3, 3, 2, 2, 1, 1, 3, 3, 2, 2, 1, 1, 1, 0};
    int s[160];
    int sa[160];
    int K;
    int n;
    int it;
    int r;
    int j;
    unsigned long total;
    unsigned long k;
    unsigned long recursive;
    recursive = 0;
    for (it = 0; it < 400; ++it)
    {
        n = 2 + (int)(rnd() % 120);
        K = 1 + (int)(rnd() % 5);
        for (j = 0; j < n; ++j)
        {
            s[j] = 1 + (int)(rnd() % (unsigned)K);
        }

        s[n] = 0;
        ++n;
        bwt89_test_reset_stats();
        r = bwt89_sa_is(s, sa, n, K);
        CHECK(r == 0);
        total = bwt89_test_alloc_calls();
        if (total > 1)
        {
            ++recursive;
        }
        for (k = 1; k <= total; ++k)
        {
            bwt89_test_reset_stats();
            bwt89_test_fail_alloc_at(k);
            r = bwt89_sa_is(s, sa, n, K);
            CHECK(r != 0);
            CHECK(bwt89_test_live_allocs() == 0);
        }
        bwt89_test_disable_alloc_failure();
    }
    n = (int)(sizeof fixture / sizeof fixture[0]);
    bwt89_test_reset_stats();
    r = bwt89_sa_is(fixture, sa, n, 3);
    CHECK(r == 0);
    total = bwt89_test_alloc_calls();
    for (k = 1; k <= total; ++k)
    {
        bwt89_test_reset_stats();
        bwt89_test_fail_alloc_at(k);
        r = bwt89_sa_is(fixture, sa, n, 3);
        CHECK(r != 0);
        CHECK(bwt89_test_live_allocs() == 0);
    }
    bwt89_test_disable_alloc_failure();
    CHECK(recursive > 0);
}

int main(void)
{
    test_one_above_max();
    test_max_n_allocates();
    test_alloc_sweeps();
    test_sa_sweep();
    if (test_failures != 0)
    {
        fprintf(stderr, "%d check(s) failed\n", test_failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
