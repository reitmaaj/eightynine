#include <stdio.h>

#include "mem.h"

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

static bpf_region_cfg mk(bpf_byte kind, bpf_off64 base, bpf_off64 len)
{
    bpf_region_cfg c;

    c.kind = kind;
    c.guest = base;
    c.len = len;
    c.init = 0;
    c.init_len = 0;
    return c;
}

/* The canonical four-region layout used by most probes. */
static bpf_memory *make_mem(void)
{
    bpf_region_cfg c[4];
    bpf_memory *m;
    bpf_err e;

    c[0] = mk(BPF_RINPUT, 0x1000, 0x40);
    c[1] = mk(BPF_RCONST, 0x2000, 0x20);
    c[2] = mk(BPF_RMEM, 0x100000, 0x2000);
    c[3] = mk(BPF_RSTACK, 0xF0000, 0x2000);
    e = bpf_memory_create(c, 4, &m);
    if (e != BPF_OK)
    {
        return 0;
    }
    return m;
}

/* A fully independent reference model of the canonical layout. */
static bpf_err reference(bpf_off64 addr, bpf_off64 bytes, bpf_byte perm)
{
    static const bpf_off64 bases[4] = {0x1000, 0x2000, 0x100000, 0xF0000};
    static const bpf_off64 lens[4] = {0x40, 0x20, 0x2000, 0x2000};
    static const bpf_byte rperm[4] = {
        BPF_MEM_R, BPF_MEM_R, BPF_MEM_R | BPF_MEM_W, BPF_MEM_R | BPF_MEM_W};
    bpf_u32 i;
    bpf_err r;

    r = BPF_EADDR;
    i = 0;
    while (i < 4u)
    {
        bpf_off64 base;
        bpf_off64 len;
        bpf_byte acc;
        bpf_off64 delta;

        base = bases[i];
        len = lens[i];
        acc = rperm[i];
        if (addr >= base)
        {
            delta = addr - base;
            if (delta <= len)
            {
                if (bytes <= len - delta)
                {
                    if ((acc & perm) == perm)
                    {
                        r = BPF_OK;
                    }
                    else
                    {
                        r = BPF_EPERM;
                    }
                }
            }
        }
        i = i + 1;
    }
    return r;
}

static void expect(bpf_memory *m, bpf_off64 a, bpf_off64 n, bpf_byte perm)
{
    bpf_byte *h;
    bpf_err got;
    bpf_err want;

    want = reference(a, n, perm);
    got = bpf_mem_resolve(m, a, n, perm, &h);
    cases = cases + 1;
    if (got != want)
    {
        (void)fprintf(stderr, "FAIL: resolve(0x%lx,%lu,%u) got %d want %d\n",
                      (unsigned long)a, (unsigned long)n, perm, (int)got,
                      (int)want);
        fails = fails + 1;
    }
}

/* Sweep every byte and many lengths inside and around the read-only input
 * region, comparing the resolver against the independent reference. */
static void test_sweep_input(void)
{
    bpf_memory *m;
    bpf_off64 a;
    bpf_off64 n;

    m = make_mem();
    if (m == 0)
    {
        ck(0, "memory for sweep");
        return;
    }
    for (a = 0xFF0; a <= 0x1050; ++a)
    {
        for (n = 1u; n <= 0x50u; ++n)
        {
            expect(m, a, n, BPF_MEM_R);
            expect(m, a, n, BPF_MEM_W);
        }
    }
    bpf_memory_destroy(m);
}

/* Sweep a large read/write region for read and write at granular steps. */
static void test_sweep_working(void)
{
    bpf_memory *m;
    bpf_off64 a;

    m = make_mem();
    if (m == 0)
    {
        ck(0, "memory for working sweep");
        return;
    }
    for (a = 0xFFF0; a <= 0x100100; a = a + 7u)
    {
        expect(m, a, 1, BPF_MEM_R);
        expect(m, a, 1, BPF_MEM_W);
        expect(m, a, 8, BPF_MEM_R);
        expect(m, a, 8, BPF_MEM_W);
    }
    bpf_memory_destroy(m);
}

/* Permissions: every region kind is probed for read and write. */
static void test_permission_matrix(void)
{
    bpf_memory *m;

    m = make_mem();
    if (m == 0)
    {
        ck(0, "memory for permission matrix");
        return;
    }
    expect(m, 0x1000, 1, BPF_MEM_R);   /* input read ok */
    expect(m, 0x1000, 1, BPF_MEM_W);   /* input write denied */
    expect(m, 0x2000, 1, BPF_MEM_R);   /* const read ok */
    expect(m, 0x2000, 1, BPF_MEM_W);   /* const write denied */
    expect(m, 0x100000, 1, BPF_MEM_R); /* working read ok */
    expect(m, 0x100000, 1, BPF_MEM_W); /* working write ok */
    expect(m, 0xF0000, 1, BPF_MEM_R);  /* stack read ok */
    expect(m, 0xF0000, 1, BPF_MEM_W);  /* stack write ok */
    bpf_memory_destroy(m);
}

/* Construction-failure matrix. */
static void test_construct_matrix(void)
{
    bpf_region_cfg c[BPF_MEM_MAX];
    bpf_memory *m;
    bpf_err e;
    bpf_u32 i;

    /* Pairwise overlap of a sliding second region. */
    for (i = 0u; i < 0x60u; ++i)
    {
        c[0] = mk(BPF_RMEM, 0x1000, 0x40);
        c[1] = mk(BPF_RMEM, 0x1000 + i, 0x40);
        cases = cases + 1;
        m = 0;
        e = bpf_memory_create(c, 2, &m);
        if (i < 0x40u)
        {
            ck(e == BPF_EADDR && m == 0, "overlap rejected");
        }
        else
        {
            ck(e == BPF_OK, "disjoint accepted");
            if (e == BPF_OK)
            {
                bpf_memory_destroy(m);
            }
        }
    }
    /* First region must not be guest address zero. */
    c[0] = mk(BPF_RMEM, 0, 8);
    cases = cases + 1;
    m = 0;
    e = bpf_memory_create(c, 1, &m);
    ck(e == BPF_EADDR, "zero base rejected");
    /* Each kind accepted at a valid address. */
    for (i = 0u; i < 4u; ++i)
    {
        bpf_byte kind;

        kind = BPF_RINPUT + (bpf_byte)i;
        c[0] = mk(kind, 0x4000 + (bpf_off64)i * 0x100, 0x80);
        cases = cases + 1;
        m = 0;
        e = bpf_memory_create(c, 1, &m);
        ck(e == BPF_OK, "single region of each kind accepted");
        if (e == BPF_OK)
        {
            bpf_memory_destroy(m);
        }
    }
}

/* Empty configuration produces an empty (zero-region) memory. */
static void test_empty_cfg(void)
{
    bpf_memory *m;
    bpf_err e;

    cases = cases + 1;
    m = 0;
    e = bpf_memory_create(0, 0, &m);
    ck(e == BPF_OK && m != 0 && bpf_memory_nregions(m) == 0,
       "empty configuration yields zero regions");
    if (e == BPF_OK)
    {
        bpf_memory_destroy(m);
    }
}

/* A data-dependent boundary: access exactly at base and at end. */
static void test_exact_boundaries(void)
{
    bpf_memory *m;

    m = make_mem();
    if (m == 0)
    {
        ck(0, "memory for boundaries");
        return;
    }
    expect(m, 0x1000, 0x40, BPF_MEM_R);     /* whole input fits */
    expect(m, 0x1000, 0x40, BPF_MEM_W);     /* whole input denied */
    expect(m, 0x1000, 0x41, BPF_MEM_R);     /* one past end denied */
    expect(m, 0x1000, 0x40 + 1, BPF_MEM_R); /* also denied */
    expect(m, 0x100000, 0x2000, BPF_MEM_R); /* whole working read ok */
    expect(m, 0x100000, 0x2000, BPF_MEM_W); /* whole working write ok */
    expect(m, 0x100000, 0x2001, BPF_MEM_R); /* one past working end denied */
    bpf_memory_destroy(m);
}

/* An empty memory rejects every access. */
static void test_empty_rejects_access(void)
{
    bpf_memory *m;
    bpf_byte *h;
    bpf_err e;

    cases = cases + 1;
    m = 0;
    e = bpf_memory_create(0, 0, &m);
    ck(e == BPF_OK, "empty memory create");
    if (e != BPF_OK)
    {
        return;
    }
    e = bpf_mem_resolve(m, 0x1000, 1, BPF_MEM_R, &h);
    ck(e == BPF_EADDR, "empty memory denies all reads");
    bpf_memory_destroy(m);
}

/* Two adjacent regions: crossing the shared boundary must never resolve. */
static void test_adjacent_cross_sweep(void)
{
    bpf_region_cfg c[2];
    bpf_memory *m;
    bpf_err e;
    bpf_off64 a;

    c[0] = mk(BPF_RMEM, 0x1000, 0x100);
    c[1] = mk(BPF_RMEM, 0x1100, 0x100);
    cases = cases + 1;
    e = bpf_memory_create(c, 2, &m);
    ck(e == BPF_OK, "adjacent regions");
    if (e != BPF_OK)
    {
        return;
    }
    for (a = 0x10F0; a <= 0x1110; ++a)
    {
        bpf_off64 n;

        for (n = 0x10u; n <= 0x30u; ++n)
        {
            bpf_byte *h;
            bpf_err r;
            bpf_byte fit0;
            bpf_byte fit1;

            r = bpf_mem_resolve(m, a, n, BPF_MEM_R, &h);
            cases = cases + 1;
            fit0 = 0;
            if (a + n <= 0x1100)
            {
                fit0 = 1;
            }
            fit1 = 0;
            if (a >= 0x1100)
            {
                if (a + n <= 0x1200)
                {
                    fit1 = 1;
                }
            }
            if (fit0 || fit1)
            {
                ck(r == BPF_OK, "window fully inside one region resolves");
            }
            else
            {
                ck(r == BPF_EADDR,
                   "window straddling a boundary never resolves");
            }
        }
    }
    bpf_memory_destroy(m);
}

int main(void)
{
    test_sweep_input();
    test_sweep_working();
    test_permission_matrix();
    test_construct_matrix();
    test_empty_cfg();
    test_exact_boundaries();
    test_empty_rejects_access();
    test_adjacent_cross_sweep();
    if (fails)
    {
        (void)fprintf(stderr,
                      "test_guest_memory_matrix: %d failures (%d "
                      "cases)\n",
                      fails, cases);
        return 1;
    }
    (void)printf("ok: test_guest_memory_matrix (%d cases)\n", cases);
    return 0;
}
