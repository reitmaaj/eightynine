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

static bpf_caps *mk(bpf_u32 max)
{
    bpf_caps *c;
    bpf_err e;

    cases = cases + 1;
    e = bpf_caps_create(max, &c);
    if (e != BPF_OK)
    {
        return 0;
    }
    return c;
}

/* Handles handed out over an interleaved alloc/revoke schedule are strictly
 * increasing and never repeat. */
static void test_interleaved_no_reuse(void)
{
    bpf_caps *c;
    bpf_handle h;
    bpf_err e;
    bpf_u32 k;
    bpf_u32 last;

    c = mk(32);
    if (c == 0)
    {
        ck(0, "interleaved table");
        return;
    }
    last = 0;
    for (k = 0u; k < 12u; ++k)
    {
        e = bpf_caps_alloc(c, k, 1, 0, &h);
        cases = cases + 1;
        ck(e == BPF_OK && h == k, "alloc h == k");
        if (k % 2u == 0u)
        {
            e = bpf_caps_revoke(c, h);
            ck(e == BPF_OK, "revoke even handle");
        }
        else
        {
            last = h;
            (void)last;
        }
    }
    /* Counter is 12; no handle ever repeats. */
    for (k = 0u; k < 5u; ++k)
    {
        e = bpf_caps_alloc(c, k, 1, 0, &h);
        cases = cases + 1;
        ck(e == BPF_OK && h == 12u + k,
           "continues monotonically after revokes");
    }
    bpf_caps_destroy(c);
}

/* Revoking a low handle does not let a later allocation reuse its slot, so
 * the live count equals the number of not-yet-revoked allocations. */
static void test_live_count_after_revokes(void)
{
    bpf_caps *c;
    bpf_handle h[10];
    bpf_u32 i;

    c = mk(16);
    if (c == 0)
    {
        ck(0, "live-count table");
        return;
    }
    for (i = 0u; i < 10u; ++i)
    {
        bpf_caps_alloc(c, i, i, 0, &h[i]);
    }
    cases = cases + 1;
    ck(bpf_caps_live(c) == 10, "ten allocated");
    bpf_caps_revoke(c, h[0]);
    bpf_caps_revoke(c, h[3]);
    bpf_caps_revoke(c, h[5]);
    cases = cases + 1;
    ck(bpf_caps_live(c) == 7, "seven live after three revokes");
    bpf_caps_revoke(c, h[1]);
    bpf_caps_revoke(c, h[2]);
    bpf_caps_revoke(c, h[4]);
    bpf_caps_revoke(c, h[6]);
    bpf_caps_revoke(c, h[7]);
    bpf_caps_revoke(c, h[8]);
    bpf_caps_revoke(c, h[9]);
    cases = cases + 1;
    ck(bpf_caps_live(c) == 0, "none live after revoking all");
    bpf_caps_destroy(c);
}

/* Lookup with selectively null output pointers still works. */
static void test_lookup_partial_output(void)
{
    bpf_caps *c;
    bpf_handle h;
    bpf_u32 t;
    bpf_err e;

    c = mk(8);
    if (c == 0)
    {
        ck(0, "partial-output table");
        return;
    }
    bpf_caps_alloc(c, 3, 0x7u, 0, &h);
    /* type only */
    t = 0;
    e = bpf_caps_lookup(c, h, &t, 0, 0);
    cases = cases + 1;
    ck(e == BPF_OK && t == 3, "type-only lookup");
    /* rights only */
    {
        bpf_u32 r;

        r = 0;
        e = bpf_caps_lookup(c, h, 0, &r, 0);
        cases = cases + 1;
        ck(e == BPF_OK && r == 0x7u, "rights-only lookup");
    }
    /* all null output is a liveness probe */
    cases = cases + 1;
    e = bpf_caps_lookup(c, h, 0, 0, 0);
    ck(e == BPF_OK, "liveness probe on live handle");
    bpf_caps_destroy(c);
}

/* Distinct host pointers are preserved through lookup. */
static void test_host_identity(void)
{
    static int a;
    static int b;
    bpf_caps *c;
    bpf_handle ha;
    bpf_handle hb;
    void *host;
    bpf_err e;

    c = mk(8);
    if (c == 0)
    {
        ck(0, "host-identity table");
        return;
    }
    bpf_caps_alloc(c, 1, 1, (void *)&a, &ha);
    bpf_caps_alloc(c, 1, 1, (void *)&b, &hb);
    host = 0;
    e = bpf_caps_lookup(c, ha, 0, 0, &host);
    cases = cases + 1;
    ck(e == BPF_OK && host == (void *)&a, "host object a preserved");
    host = 0;
    e = bpf_caps_lookup(c, hb, 0, 0, &host);
    cases = cases + 1;
    ck(e == BPF_OK && host == (void *)&b, "host object b preserved");
    bpf_caps_destroy(c);
}

/* A wide spread of type and rights values round-trips. */
static void test_fields_matrix(void)
{
    bpf_caps *c;
    bpf_handle h[8];
    bpf_u32 t;
    bpf_u32 r;
    bpf_err e;
    bpf_u32 i;

    c = mk(8);
    if (c == 0)
    {
        ck(0, "fields-matrix table");
        return;
    }
    for (i = 0u; i < 8u; ++i)
    {
        bpf_u32 want;

        want = (i << 4) | 0xA0u;
        bpf_caps_alloc(c, i + 1u, want, 0, &h[i]);
    }
    for (i = 0u; i < 8u; ++i)
    {
        t = 0;
        r = 0;
        e = bpf_caps_lookup(c, h[i], &t, &r, 0);
        cases = cases + 1;
        ck(e == BPF_OK && t == i + 1u && r == ((i << 4) | 0xA0u),
           "type/rights round trip");
    }
    bpf_caps_destroy(c);
}

/* Reusing the table structure across a destroy leaves no dangling liveness. */
static void test_recreate(void)
{
    bpf_caps *c;
    bpf_handle h;
    bpf_err e;
    bpf_u32 i;

    for (i = 0u; i < 3u; ++i)
    {
        c = mk(4);
        if (c == 0)
        {
            ck(0, "recreate table");
            return;
        }
        e = bpf_caps_alloc(c, 0, 0, 0, &h);
        ck(e == BPF_OK && h == 0, "fresh table starts at handle 0");
        cases = cases + 1;
        bpf_caps_destroy(c);
    }
}

/* A single-element table allocates once then exhausts. */
static void test_single_slot(void)
{
    bpf_caps *c;
    bpf_handle h;
    bpf_err e;

    c = mk(1);
    if (c == 0)
    {
        ck(0, "single-slot table");
        return;
    }
    cases = cases + 1;
    e = bpf_caps_alloc(c, 9, 9, 0, &h);
    ck(e == BPF_OK && h == 0, "single handle 0");
    cases = cases + 1;
    e = bpf_caps_alloc(c, 9, 9, 0, &h);
    ck(e == BPF_EFULL, "single slot exhausts");
    bpf_caps_revoke(c, 0);
    cases = cases + 1;
    e = bpf_caps_alloc(c, 9, 9, 0, &h);
    ck(e == BPF_EFULL, "revoked single slot is not reused");
    bpf_caps_destroy(c);
}

/* Revoking handles does not disturb other live entries. */
static void test_revoke_isolation(void)
{
    bpf_caps *c;
    bpf_handle h[6];
    bpf_u32 t;
    bpf_err e;
    bpf_u32 i;

    c = mk(16);
    if (c == 0)
    {
        ck(0, "revoke-isolation table");
        return;
    }
    for (i = 0u; i < 6u; ++i)
    {
        bpf_caps_alloc(c, i, i * 3u, 0, &h[i]);
    }
    for (i = 1u; i < 6u; i = i + 2u)
    {
        bpf_caps_revoke(c, h[i]);
    }
    for (i = 0u; i < 6u; i = i + 2u)
    {
        t = 0;
        e = bpf_caps_lookup(c, h[i], &t, 0, 0);
        cases = cases + 1;
        ck(e == BPF_OK && t == i, "even handle unaffected by odd revokes");
    }
    bpf_caps_destroy(c);
}

static void test_exhaust_midway(void)
{
    bpf_caps *c;
    bpf_handle h;
    bpf_err e;
    bpf_u32 i;

    c = mk(5);
    if (c == 0)
    {
        ck(0, "exhaust-midway table");
        return;
    }
    for (i = 0u; i < 5u; ++i)
    {
        cases = cases + 1;
        e = bpf_caps_alloc(c, i, 1, 0, &h);
        ck(e == BPF_OK && h == i, "alloc up to capacity");
    }
    cases = cases + 1;
    e = bpf_caps_alloc(c, 0, 1, 0, &h);
    ck(e == BPF_EFULL, "fifth allocation exhausts");
    bpf_caps_destroy(c);
}

static void test_revoke_nonexistent_returns_error(void)
{
    bpf_caps *c;
    bpf_err e;

    c = mk(4);
    if (c == 0)
    {
        ck(0, "revoke-nonexistent table");
        return;
    }
    cases = cases + 1;
    e = bpf_caps_revoke(c, 7);
    ck(e == BPF_EHANDLE, "revoke out-of-range fails");
    cases = cases + 1;
    e = bpf_caps_revoke(c, 0);
    ck(e == BPF_EHANDLE, "revoke never-allocated fails");
    bpf_caps_destroy(c);
}

static void test_alloc_null_host_ok(void)
{
    bpf_caps *c;
    bpf_handle h;
    void *host;
    bpf_err e;

    c = mk(4);
    if (c == 0)
    {
        ck(0, "null-host table");
        return;
    }
    cases = cases + 1;
    e = bpf_caps_alloc(c, 1, 1, 0, &h);
    ck(e == BPF_OK, "null host accepted");
    host = (void *)1;
    cases = cases + 1;
    e = bpf_caps_lookup(c, h, 0, 0, &host);
    ck(e == BPF_OK && host == 0, "lookup reports null host");
    bpf_caps_destroy(c);
}

int main(void)
{
    test_interleaved_no_reuse();
    test_live_count_after_revokes();
    test_lookup_partial_output();
    test_host_identity();
    test_fields_matrix();
    test_recreate();
    test_single_slot();
    test_revoke_isolation();
    test_exhaust_midway();
    test_revoke_nonexistent_returns_error();
    test_alloc_null_host_ok();
    if (fails)
    {
        (void)fprintf(stderr, "test_caps_matrix: %d failures (%d cases)\n",
                      fails, cases);
        return 1;
    }
    (void)printf("ok: test_caps_matrix (%d cases)\n", cases);
    return 0;
}
