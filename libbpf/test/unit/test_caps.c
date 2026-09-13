#include <stdio.h>

#include "cap.h"

static int fails;
static int cases;

static void ck(int cond, const char *msg)
{
    if (!cond)
    {
        (void)fprintf(stderr, "FAIL: %s\n", msg);
        fails = fails + 1;
    }
}

static bpf_err make(bpf_u32 max, bpf_caps **c)
{
    bpf_err e;

    cases = cases + 1;
    e = bpf_caps_create(max, c);
    return e;
}

static void test_create_ok(void)
{
    bpf_caps *c;
    bpf_err e;

    e = make(8, &c);
    ck(e == BPF_OK && c != 0 && bpf_caps_live(c) == 0, "table created empty");
    if (e == BPF_OK)
    {
        bpf_caps_destroy(c);
    }
}

static void test_zero_max(void)
{
    bpf_caps *c;
    bpf_err e;

    e = bpf_caps_create(0, &c);
    ck(e == BPF_OK && bpf_caps_live(c) == 0, "zero-max table created");
    if (e == BPF_OK)
    {
        bpf_caps_destroy(c);
    }
}

static void test_monotonic(void)
{
    bpf_caps *c;
    bpf_handle h;
    bpf_err e;
    bpf_u32 i;

    e = make(16, &c);
    ck(e == BPF_OK, "table for monotonic");
    if (e != BPF_OK)
    {
        return;
    }
    for (i = 0u; i < 5u; ++i)
    {
        e = bpf_caps_alloc(c, i, i, 0, &h);
        cases = cases + 1;
        ck(e == BPF_OK && h == i, "handle equals index (monotonic)");
    }
    ck(bpf_caps_live(c) == 5, "five live handles");
    bpf_caps_destroy(c);
}

static void test_no_reuse_after_revoke(void)
{
    bpf_caps *c;
    bpf_handle h;
    bpf_handle h2;
    bpf_err e;

    e = make(16, &c);
    ck(e == BPF_OK, "table for no-reuse");
    if (e != BPF_OK)
    {
        return;
    }
    e = bpf_caps_alloc(c, 1, 1, 0, &h);
    ck(e == BPF_OK && h == 0, "first handle 0");
    e = bpf_caps_revoke(c, h);
    ck(e == BPF_OK, "revoke succeeds");
    e = bpf_caps_alloc(c, 2, 2, 0, &h2);
    cases = cases + 1;
    ck(e == BPF_OK && h2 == 1, "revoked handle 0 is not reused");
    bpf_caps_destroy(c);
}

static void test_exhaustion(void)
{
    bpf_caps *c;
    bpf_handle h;
    bpf_err e;
    bpf_u32 i;

    e = make(3, &c);
    ck(e == BPF_OK, "table of size 3");
    if (e != BPF_OK)
    {
        return;
    }
    for (i = 0u; i < 3u; ++i)
    {
        e = bpf_caps_alloc(c, 0, 0, 0, &h);
        ck(e == BPF_OK, "alloc within capacity");
    }
    cases = cases + 1;
    e = bpf_caps_alloc(c, 0, 0, 0, &h);
    ck(e == BPF_EFULL, "exhaustion returns EFULL (no wrap)");
    bpf_caps_destroy(c);
}

static void test_exhaustion_no_wrap_after_revoke(void)
{
    bpf_caps *c;
    bpf_handle h;
    bpf_err e;
    bpf_u32 i;

    e = make(2, &c);
    ck(e == BPF_OK, "table of size 2");
    if (e != BPF_OK)
    {
        return;
    }
    for (i = 0u; i < 2u; ++i)
    {
        bpf_caps_alloc(c, 0, 0, 0, &h);
    }
    bpf_caps_revoke(c, 0); /* frees slot 0 but the counter must not wrap */
    cases = cases + 1;
    e = bpf_caps_alloc(c, 0, 0, 0, &h);
    ck(e == BPF_EFULL, "no reuse even with a revoked slot free");
    bpf_caps_destroy(c);
}

static void test_lookup_fields(void)
{
    bpf_caps *c;
    bpf_handle h;
    bpf_u32 t;
    bpf_u32 r;
    void *host;
    bpf_err e;

    e = make(4, &c);
    ck(e == BPF_OK, "table for lookup");
    if (e != BPF_OK)
    {
        return;
    }
    e = bpf_caps_alloc(c, 7, 0x5u, (void *)0x1234, &h);
    ck(e == BPF_OK, "alloc with fields");
    t = 0;
    r = 0;
    host = 0;
    e = bpf_caps_lookup(c, h, &t, &r, &host);
    cases = cases + 1;
    ck(e == BPF_OK && t == 7 && r == 0x5u && host == (void *)0x1234,
       "lookup returns type, rights, host");
    bpf_caps_destroy(c);
}

static void test_lookup_revoked_fails(void)
{
    bpf_caps *c;
    bpf_handle h;
    bpf_err e;

    e = make(4, &c);
    ck(e == BPF_OK, "table for revoked lookup");
    if (e != BPF_OK)
    {
        return;
    }
    bpf_caps_alloc(c, 1, 1, 0, &h);
    bpf_caps_revoke(c, h);
    cases = cases + 1;
    e = bpf_caps_lookup(c, h, 0, 0, 0);
    ck(e == BPF_EHANDLE, "revoked handle lookup fails");
    cases = cases + 1;
    e = bpf_caps_revoke(c, h);
    ck(e == BPF_EHANDLE, "double revoke fails");
    bpf_caps_destroy(c);
}

static void test_lookup_unallocated_fails(void)
{
    bpf_caps *c;
    bpf_err e;

    e = make(4, &c);
    ck(e == BPF_OK, "table for out-of-range lookup");
    if (e != BPF_OK)
    {
        return;
    }
    cases = cases + 1;
    e = bpf_caps_lookup(c, 99, 0, 0, 0);
    ck(e == BPF_EHANDLE, "out-of-range handle lookup fails");
    cases = cases + 1;
    e = bpf_caps_lookup(c, 0, 0, 0, 0);
    ck(e == BPF_EHANDLE, "never-allocated handle lookup fails");
    bpf_caps_destroy(c);
}

static void test_revoke_updates_live_count(void)
{
    bpf_caps *c;
    bpf_handle h0;
    bpf_handle h1;
    bpf_err e;

    e = make(4, &c);
    ck(e == BPF_OK, "table for live count");
    if (e != BPF_OK)
    {
        return;
    }
    bpf_caps_alloc(c, 1, 1, 0, &h0);
    bpf_caps_alloc(c, 1, 1, 0, &h1);
    ck(bpf_caps_live(c) == 2, "two live");
    bpf_caps_revoke(c, h0);
    cases = cases + 1;
    ck(bpf_caps_live(c) == 1, "one live after revoke");
    (void)h1;
    bpf_caps_destroy(c);
}

static void test_destroy_null(void)
{
    bpf_caps_destroy(0);
    cases = cases + 1;
    ck(1, "destroy null is a no-op");
}

/* A large table can hand out many distinct handles. */
static void test_many_handles(void)
{
    bpf_caps *c;
    bpf_handle h;
    bpf_err e;
    bpf_u32 i;

    e = make(100, &c);
    ck(e == BPF_OK, "large table");
    if (e != BPF_OK)
    {
        return;
    }
    for (i = 0u; i < 100u; ++i)
    {
        cases = cases + 1;
        e = bpf_caps_alloc(c, i & 0xFFu, 1u, 0, &h);
        if (e != BPF_OK || h != i)
        {
            ck(0, "many handles monotonic");
        }
    }
    ck(bpf_caps_live(c) == 100, "all handles live");
    cases = cases + 1;
    e = bpf_caps_alloc(c, 0, 0, 0, &h);
    ck(e == BPF_EFULL, "large table exhausts");
    bpf_caps_destroy(c);
}

int main(void)
{
    test_create_ok();
    test_zero_max();
    test_monotonic();
    test_no_reuse_after_revoke();
    test_exhaustion();
    test_exhaustion_no_wrap_after_revoke();
    test_lookup_fields();
    test_lookup_revoked_fails();
    test_lookup_unallocated_fails();
    test_revoke_updates_live_count();
    test_destroy_null();
    test_many_handles();
    if (fails)
    {
        (void)fprintf(stderr, "test_caps: %d failures (%d cases)\n", fails,
                      cases);
        return 1;
    }
    (void)printf("ok: test_caps (%d cases)\n", cases);
    return 0;
}
