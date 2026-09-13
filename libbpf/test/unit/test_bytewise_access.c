#include <stdio.h>
#include <string.h>

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

/* Read-only region at 0x1000 with a known little-endian prefix. */
static bpf_memory *make_ro(void)
{
    static const bpf_byte in[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    bpf_region_cfg c[1];
    bpf_memory *m;
    bpf_err e;

    c[0] = mk(BPF_RINPUT, 0x1000, 0x40);
    c[0].init = in;
    c[0].init_len = 8;
    e = bpf_memory_create(c, 1, &m);
    if (e != BPF_OK)
    {
        return 0;
    }
    return m;
}

/* Read/write region at 0x2000. */
static bpf_memory *make_rw(void)
{
    bpf_region_cfg c[1];
    bpf_memory *m;
    bpf_err e;

    c[0] = mk(BPF_RMEM, 0x2000, 0x200);
    e = bpf_memory_create(c, 1, &m);
    if (e != BPF_OK)
    {
        return 0;
    }
    return m;
}

static void test_le_store_load_roundtrip(void)
{
    static const bpf_u64 vals[4] = {0x0UL, 0xFFFFFFFFFFFFFFFFUL,
                                    0x123456789ABCDEF0UL, 0x80UL};
    bpf_memory *m;
    bpf_u32 v;

    m = make_rw();
    if (m == 0)
    {
        ck(0, "rw memory for roundtrip");
        return;
    }
    for (v = 0u; v < 4u; ++v)
    {
        bpf_u64 val;
        bpf_u64 got;
        bpf_err e;
        bpf_byte w;
        bpf_byte wl;

        val = vals[v];
        for (wl = 0u; wl < 4u; ++wl)
        {
            w = 1u;
            if (wl == 1u)
            {
                w = 2u;
            }
            if (wl == 2u)
            {
                w = 4u;
            }
            if (wl == 3u)
            {
                w = 8u;
            }
            cases = cases + 1;
            e = bpf_mem_store(m, 0x2000, w, val);
            if (e != BPF_OK)
            {
                ck(0, "store should succeed in rw region");
                continue;
            }
            got = 0;
            e = bpf_mem_load(m, 0x2000, w, &got);
            if (e == BPF_OK)
            {
                bpf_u64 want;

                want = val;
                if (w != 8u)
                {
                    bpf_u64 bits;
                    bpf_u64 ones;

                    bits = (bpf_u64)(8u * (bpf_u32)w);
                    ones = ((bpf_u64)1 << bits) - 1u;
                    want = val & ones;
                }
                ck(got == want, "load mirrors stored (truncated) value");
            }
            else
            {
                ck(0, "load should succeed after store");
            }
        }
    }
    bpf_memory_destroy(m);
}

static void test_le_byte_order_store(void)
{
    bpf_memory *m;
    bpf_byte *h;
    bpf_err e;

    m = make_rw();
    if (m == 0)
    {
        ck(0, "rw memory for byte order");
        return;
    }
    cases = cases + 1;
    e = bpf_mem_store(m, 0x2000, 4, 0x04030201UL);
    ck(e == BPF_OK, "store u32");
    e = bpf_mem_resolve(m, 0x2000, 4, BPF_MEM_R, &h);
    ck(e == BPF_OK && h[0] == 0x01 && h[1] == 0x02 && h[2] == 0x03 &&
           h[3] == 0x04,
       "u32 stored least-significant byte first");
    cases = cases + 1;
    e = bpf_mem_store(m, 0x2000, 8, 0x0807060504030201UL);
    ck(e == BPF_OK, "store u64");
    e = bpf_mem_resolve(m, 0x2000, 8, BPF_MEM_R, &h);
    ck(e == BPF_OK && h[0] == 0x01 && h[7] == 0x08,
       "u64 stored least-significant byte first");
    bpf_memory_destroy(m);
}

static void test_le_load_from_read_only(void)
{
    bpf_memory *m;
    bpf_u64 got;
    bpf_err e;

    m = make_ro();
    if (m == 0)
    {
        ck(0, "ro memory");
        return;
    }
    got = 0;
    cases = cases + 1;
    e = bpf_mem_load(m, 0x1000, 1, &got);
    ck(e == BPF_OK && got == 1, "load u8 from input");
    got = 0;
    e = bpf_mem_load(m, 0x1000, 2, &got);
    ck(e == BPF_OK && got == 0x0201UL, "load u16 little-endian from input");
    got = 0;
    e = bpf_mem_load(m, 0x1000, 4, &got);
    ck(e == BPF_OK && got == 0x04030201UL, "load u32 little-endian from input");
    got = 0;
    e = bpf_mem_load(m, 0x1000, 8, &got);
    ck(e == BPF_OK && got == 0x0807060504030201UL,
       "load u64 little-endian from input");
    bpf_memory_destroy(m);
}

static void test_store_to_read_only_denied(void)
{
    bpf_memory *m;
    bpf_byte *h;
    bpf_err e;
    bpf_u32 w;
    bpf_u32 v;

    m = make_ro();
    if (m == 0)
    {
        ck(0, "ro memory");
        return;
    }
    for (v = 0u; v < 4u; ++v)
    {
        w = 1u;
        if (v == 1u)
        {
            w = 2u;
        }
        if (v == 2u)
        {
            w = 4u;
        }
        if (v == 3u)
        {
            w = 8u;
        }
        cases = cases + 1;
        e = bpf_mem_store(m, 0x1000, (bpf_byte)w, 0xEEUL);
        ck(e == BPF_EPERM, "store to read-only region denied");
    }
    e = bpf_mem_resolve(m, 0x1000, 4, BPF_MEM_R, &h);
    ck(e == BPF_OK && h[0] == 1,
       "read-only region unchanged after denied store");
    bpf_memory_destroy(m);
}

static void test_boundary_crossing(void)
{
    bpf_memory *m;
    bpf_u64 got;
    bpf_err e;

    m = make_rw();
    if (m == 0)
    {
        ck(0, "rw memory");
        return;
    }
    /* Region 0x2000..0x2200. */
    got = 0;
    cases = cases + 1;
    e = bpf_mem_load(m, 0x21FF, 1, &got);
    ck(e == BPF_OK, "load last byte ok");
    got = 0;
    e = bpf_mem_load(m, 0x21FF, 2, &got);
    ck(e == BPF_EADDR, "load width 2 crossing end denied");
    got = 0;
    e = bpf_mem_load(m, 0x21F8, 8, &got);
    ck(e == BPF_OK, "load u64 wholly inside ok");
    got = 0;
    e = bpf_mem_load(m, 0x21F9, 8, &got);
    ck(e == BPF_EADDR, "load u64 crossing end denied");
    e = bpf_mem_store(m, 0x21FF, 2, 1UL);
    ck(e == BPF_EADDR, "store crossing end denied");
    bpf_memory_destroy(m);
}

static void test_invalid_widths(void)
{
    bpf_memory *m;
    bpf_u64 got;
    bpf_err e;
    bpf_u32 w;

    m = make_rw();
    if (m == 0)
    {
        ck(0, "rw memory");
        return;
    }
    for (w = 3u; w < 16u; ++w)
    {
        bpf_byte wb;
        bpf_byte valid;

        wb = (bpf_byte)w;
        valid = 0;
        if (wb == 1u || wb == 2u || wb == 4u || wb == 8u)
        {
            valid = 1;
        }
        if (!valid)
        {
            cases = cases + 1;
            got = 0;
            e = bpf_mem_load(m, 0x2000, wb, &got);
            ck(e == BPF_ESIZE, "invalid load width rejected");
            e = bpf_mem_store(m, 0x2000, wb, 0UL);
            ck(e == BPF_ESIZE, "invalid store width rejected");
        }
    }
    bpf_memory_destroy(m);
}

/* Partial-width loads take only the least-significant bytes. */
static void test_width_truncation(void)
{
    bpf_memory *m;
    bpf_u64 got;
    bpf_err e;

    m = make_rw();
    if (m == 0)
    {
        ck(0, "rw memory");
        return;
    }
    e = bpf_mem_store(m, 0x2000, 8, 0xFEDCBA9876543210UL);
    ck(e == BPF_OK, "store full");
    got = 0;
    e = bpf_mem_load(m, 0x2000, 1, &got);
    ck(e == BPF_OK && got == 0x10UL, "u8 reads low byte");
    got = 0;
    e = bpf_mem_load(m, 0x2000, 2, &got);
    ck(e == BPF_OK && got == 0x3210UL, "u16 reads low 16 bits");
    got = 0;
    e = bpf_mem_load(m, 0x2000, 4, &got);
    ck(e == BPF_OK && got == 0x76543210UL, "u32 reads low 32 bits");
    bpf_memory_destroy(m);
}

/* A fixed byte pattern is written and read back byte-by-byte. */
static void test_pattern_bytes(void)
{
    bpf_memory *m;
    bpf_byte pattern[16];
    bpf_u32 i;

    for (i = 0u; i < 16u; ++i)
    {
        pattern[i] = (bpf_byte)(i * 7u + 3u);
    }
    m = make_rw();
    if (m == 0)
    {
        ck(0, "rw memory");
        return;
    }
    for (i = 0u; i < 16u; ++i)
    {
        bpf_err e;

        cases = cases + 1;
        e = bpf_mem_store(m, 0x2000 + i, 1, pattern[i]);
        ck(e == BPF_OK, "store single byte");
    }
    for (i = 0u; i < 16u; ++i)
    {
        bpf_u64 got;
        bpf_err e;

        got = 0;
        cases = cases + 1;
        e = bpf_mem_load(m, 0x2000 + i, 1, &got);
        ck(e == BPF_OK && got == pattern[i], "load single byte matches");
    }
    bpf_memory_destroy(m);
}

/* Adjacent stores at successive offsets do not alias. */
static void test_offset_independence(void)
{
    bpf_memory *m;
    bpf_u64 a;
    bpf_u64 b;
    bpf_err e;

    m = make_rw();
    if (m == 0)
    {
        ck(0, "rw memory");
        return;
    }
    e = bpf_mem_store(m, 0x2000, 2, 0x0102UL);
    ck(e == BPF_OK, "store at 0x2000");
    e = bpf_mem_store(m, 0x2004, 2, 0x0304UL);
    ck(e == BPF_OK, "store at 0x2004");
    a = 0;
    b = 0;
    e = bpf_mem_load(m, 0x2000, 2, &a);
    ck(e == BPF_OK && a == 0x0102UL, "first offset intact");
    e = bpf_mem_load(m, 0x2004, 2, &b);
    ck(e == BPF_OK && b == 0x0304UL, "second offset intact");
    bpf_memory_destroy(m);
}

int main(void)
{
    test_le_store_load_roundtrip();
    test_le_byte_order_store();
    test_le_load_from_read_only();
    test_store_to_read_only_denied();
    test_boundary_crossing();
    test_invalid_widths();
    test_width_truncation();
    test_pattern_bytes();
    test_offset_independence();
    if (fails)
    {
        (void)fprintf(stderr,
                      "test_bytewise_access: %d failures (%d "
                      "cases)\n",
                      fails, cases);
        return 1;
    }
    (void)printf("ok: test_bytewise_access (%d cases)\n", cases);
    return 0;
}
