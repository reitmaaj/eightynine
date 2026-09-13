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

/* A single read/write region over [0x1000, 0x1020): 32 bytes. */
static bpf_memory *make_single(void)
{
    bpf_region_cfg c[1];
    bpf_memory *m;
    bpf_err e;

    c[0] = mk(BPF_RMEM, 0x1000, 32);
    e = bpf_memory_create(c, 1, &m);
    if (e != BPF_OK)
    {
        return 0;
    }
    return m;
}

/* Exhaustive: for every start in [0xFE0,0x1020) and length [1,48], the access
 * resolves iff start >= 0x1000 and start + len <= 0x1020. */
static void test_single_region_matrix(void)
{
    bpf_memory *m;
    bpf_off64 a;
    bpf_off64 n;

    m = make_single();
    if (m == 0)
    {
        ck(0, "single-region memory");
        return;
    }
    for (a = 0xFE0; a <= 0x1020; ++a)
    {
        for (n = 1u; n <= 48u; ++n)
        {
            bpf_byte *h;
            bpf_err r;
            bpf_byte inside;

            r = bpf_mem_resolve(m, a, n, BPF_MEM_R, &h);
            cases = cases + 1;
            inside = 0;
            if (a >= 0x1000)
            {
                if (a + n <= 0x1020)
                {
                    inside = 1;
                }
            }
            if (inside)
            {
                ck(r == BPF_OK, "inside resolves");
            }
            else
            {
                ck(r == BPF_EADDR, "outside rejected");
            }
        }
    }
    bpf_memory_destroy(m);
}

/* Bytewise read/write round trip across the working region. */
static void test_byte_round_trip(void)
{
    bpf_memory *m;
    bpf_byte *h;
    bpf_err e;
    bpf_u32 i;

    m = make_single();
    if (m == 0)
    {
        ck(0, "memory for round trip");
        return;
    }
    e = bpf_mem_resolve(m, 0x1000, 32, BPF_MEM_W, &h);
    ck(e == BPF_OK, "working region resolves for write");
    if (e != BPF_OK)
    {
        bpf_memory_destroy(m);
        return;
    }
    for (i = 0u; i < 32u; ++i)
    {
        h[i] = (bpf_byte)(i * 3u + 1u);
    }
    e = bpf_mem_resolve(m, 0x1000, 32, BPF_MEM_R, &h);
    ck(e == BPF_OK, "working region resolves for read");
    for (i = 0u; i < 32u; ++i)
    {
        cases = cases + 1;
        if (h[i] != (bpf_byte)(i * 3u + 1u))
        {
            ck(0, "byte round trip value");
        }
    }
    bpf_memory_destroy(m);
}

/* Newly created working memory is zero-filled. */
static void test_zero_fill(void)
{
    bpf_memory *m;
    bpf_byte *h;
    bpf_err e;
    bpf_u32 i;

    m = make_single();
    if (m == 0)
    {
        ck(0, "memory for zero fill");
        return;
    }
    e = bpf_mem_resolve(m, 0x1000, 32, BPF_MEM_R, &h);
    ck(e == BPF_OK, "read resolves for zero-fill check");
    for (i = 0u; i < 32u; ++i)
    {
        cases = cases + 1;
        if (h[i] != 0)
        {
            ck(0, "working memory starts zeroed");
        }
    }
    bpf_memory_destroy(m);
}

/* Input data is copied; the tail beyond the copied prefix is zeroed. */
static void test_init_copy_tail_zero(void)
{
    static const bpf_byte in[6] = {1, 2, 3, 4, 5, 6};
    bpf_region_cfg c[1];
    bpf_memory *m;
    bpf_byte *h;
    bpf_err e;
    bpf_u32 i;

    c[0] = mk(BPF_RMEM, 0x1000, 32);
    c[0].init = in;
    c[0].init_len = 6;
    cases = cases + 1;
    e = bpf_memory_create(c, 1, &m);
    ck(e == BPF_OK, "region with init prefix");
    if (e != BPF_OK)
    {
        return;
    }
    e = bpf_mem_resolve(m, 0x1000, 32, BPF_MEM_R, &h);
    ck(e == BPF_OK, "resolve init region");
    for (i = 0u; i < 6u; ++i)
    {
        cases = cases + 1;
        if (h[i] != in[i])
        {
            ck(0, "init prefix copied");
        }
    }
    for (i = 6u; i < 32u; ++i)
    {
        cases = cases + 1;
        if (h[i] != 0)
        {
            ck(0, "tail beyond init is zeroed");
        }
    }
    bpf_memory_destroy(m);
}

/* Reads never mutate the backing; a write then a read observes the write. */
static void test_write_then_read(void)
{
    bpf_memory *m;
    bpf_byte *w;
    bpf_byte *r;
    bpf_err e;

    m = make_single();
    if (m == 0)
    {
        ck(0, "memory for write-then-read");
        return;
    }
    e = bpf_mem_resolve(m, 0x1000, 4, BPF_MEM_W, &w);
    ck(e == BPF_OK, "write resolve");
    if (e == BPF_OK)
    {
        w[0] = 0x11;
        w[1] = 0x22;
        w[2] = 0x33;
        w[3] = 0x44;
    }
    e = bpf_mem_resolve(m, 0x1000, 4, BPF_MEM_R, &r);
    ck(e == BPF_OK && r[0] == 0x11 && r[3] == 0x44,
       "read observes prior write");
    bpf_memory_destroy(m);
}

/* Overlap is rejected regardless of read-only or read/write mixing. */
static void test_overlap_kind_matrix(void)
{
    bpf_region_cfg c[2];
    bpf_memory *m;
    bpf_err e;
    bpf_u32 i;
    bpf_u32 j;

    for (i = 0u; i < 4u; ++i)
    {
        for (j = 0u; j < 4u; ++j)
        {
            bpf_byte ka;
            bpf_byte kb;

            ka = (bpf_byte)(BPF_RINPUT + i);
            kb = (bpf_byte)(BPF_RINPUT + j);
            c[0] = mk(ka, 0x1000, 0x40);
            c[1] = mk(kb, 0x1010, 0x40);
            cases = cases + 1;
            m = 0;
            e = bpf_memory_create(c, 2, &m);
            ck(e == BPF_EADDR && m == 0, "overlap rejected across kinds");
        }
    }
}

/* A read of zero bytes at an unmapped address is rejected; at a mapped one it
 * is allowed. */
static void test_zero_bytes_probe(void)
{
    bpf_memory *m;
    bpf_byte *h;
    bpf_err e;

    m = make_single();
    if (m == 0)
    {
        ck(0, "memory for zero-byte probe");
        return;
    }
    cases = cases + 1;
    e = bpf_mem_resolve(m, 0x1000, 0, BPF_MEM_R, &h);
    ck(e == BPF_OK, "zero-byte read at region base resolves");
    cases = cases + 1;
    e = bpf_mem_resolve(m, 0x9000, 0, BPF_MEM_R, &h);
    ck(e == BPF_EADDR, "zero-byte read at unmapped rejected");
    bpf_memory_destroy(m);
}

int main(void)
{
    test_single_region_matrix();
    test_byte_round_trip();
    test_zero_fill();
    test_init_copy_tail_zero();
    test_write_then_read();
    test_overlap_kind_matrix();
    test_zero_bytes_probe();
    if (fails)
    {
        (void)fprintf(stderr,
                      "test_guest_memory_more: %d failures (%d "
                      "cases)\n",
                      fails, cases);
        return 1;
    }
    (void)printf("ok: test_guest_memory_more (%d cases)\n", cases);
    return 0;
}
