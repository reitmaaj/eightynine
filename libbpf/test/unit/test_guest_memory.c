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

static bpf_memory *make_four(void)
{
    static const bpf_byte in[16] = {1, 2,  3,  4,  5,  6,  7,  8,
                                    9, 10, 11, 12, 13, 14, 15, 16};
    bpf_region_cfg c[4];
    bpf_memory *m;
    bpf_err e;

    c[0] = mk(BPF_RINPUT, 0x1000, 0x40);
    c[0].init = in;
    c[0].init_len = 16;
    c[1] = mk(BPF_RCONST, 0x2000, 0x20);
    c[2] = mk(BPF_RMEM, 0x100000, 0x2000);
    c[3] = mk(BPF_RSTACK, 0xF0000, 0x2000);
    e = bpf_memory_create(c, 4, &m);
    ck(e == BPF_OK, "four-region memory created");
    return m;
}

static void test_create_ok(void)
{
    bpf_memory *m;

    m = make_four();
    if (m == 0)
    {
        return;
    }
    ck(bpf_memory_nregions(m) == 4, "four regions recorded");
    bpf_memory_destroy(m);
}

static void test_overlap_rejected(void)
{
    bpf_region_cfg c[2];
    bpf_memory *m;
    bpf_err e;

    c[0] = mk(BPF_RMEM, 0x1000, 0x40);
    c[1] = mk(BPF_RMEM, 0x1010, 0x40);
    cases = cases + 1;
    m = 0;
    e = bpf_memory_create(c, 2, &m);
    ck(e == BPF_EADDR && m == 0, "overlapping regions rejected");
}

static void test_adjacent_ok(void)
{
    bpf_region_cfg c[2];
    bpf_memory *m;
    bpf_err e;

    c[0] = mk(BPF_RINPUT, 0x1000, 0x40);
    c[1] = mk(BPF_RMEM, 0x1040, 0x40);
    cases = cases + 1;
    m = 0;
    e = bpf_memory_create(c, 2, &m);
    ck(e == BPF_OK, "adjacent non-overlapping regions accepted");
    if (e == BPF_OK)
    {
        bpf_memory_destroy(m);
    }
}

static void test_zero_base_rejected(void)
{
    bpf_region_cfg c[1];
    bpf_memory *m;
    bpf_err e;

    c[0] = mk(BPF_RMEM, 0, 0x100);
    cases = cases + 1;
    m = 0;
    e = bpf_memory_create(c, 1, &m);
    ck(e == BPF_EADDR && m == 0, "guest base zero rejected");
}

static void test_zero_len_rejected(void)
{
    bpf_region_cfg c[1];
    bpf_memory *m;
    bpf_err e;

    c[0] = mk(BPF_RMEM, 0x1000, 0);
    cases = cases + 1;
    m = 0;
    e = bpf_memory_create(c, 1, &m);
    ck(e == BPF_EADDR && m == 0, "zero-length region rejected");
}

static void test_too_many_rejected(void)
{
    bpf_region_cfg c[BPF_MEM_MAX + 1];
    bpf_memory *m;
    bpf_err e;
    bpf_u32 i;

    for (i = 0u; i < (bpf_u32)(BPF_MEM_MAX + 1); ++i)
    {
        c[i] = mk(BPF_RMEM, 0x10000u * (bpf_u64)(i + 1), 0x100);
    }
    cases = cases + 1;
    m = 0;
    e = bpf_memory_create(c, (bpf_u32)(BPF_MEM_MAX + 1), &m);
    ck(e == BPF_EADDR && m == 0, "too many regions rejected");
}

static void test_overflow_rejected(void)
{
    bpf_region_cfg c[1];
    bpf_memory *m;
    bpf_err e;

    c[0] = mk(BPF_RMEM, (bpf_off64)0 - 31, 64);
    cases = cases + 1;
    m = 0;
    e = bpf_memory_create(c, 1, &m);
    ck(e == BPF_EADDR && m == 0, "overflowing base+len rejected");
}

static void test_init_len_rejected(void)
{
    bpf_region_cfg c[1];
    static const bpf_byte in[64] = {0};
    bpf_memory *m;
    bpf_err e;

    c[0] = mk(BPF_RINPUT, 0x1000, 16);
    c[0].init = in;
    c[0].init_len = 32;
    cases = cases + 1;
    m = 0;
    e = bpf_memory_create(c, 1, &m);
    ck(e == BPF_EADDR && m == 0, "init longer than region rejected");
}

/* ------------------------------------------------------------------ */

static bpf_err acc(bpf_memory *m, bpf_off64 a, bpf_off64 n, bpf_byte perm,
                   bpf_byte **h)
{
    cases = cases + 1;
    return bpf_mem_resolve(m, a, n, perm, h);
}

static void test_read_input(void)
{
    bpf_memory *m;
    bpf_byte *h;
    bpf_err e;

    m = make_four();
    if (m == 0)
    {
        return;
    }
    e = acc(m, 0x1000, 4, BPF_MEM_R, &h);
    ck(e == BPF_OK && h[0] == 1 && h[1] == 2, "read input region bytes");
    e = acc(m, 0x1000 + 14, 2, BPF_MEM_R, &h);
    ck(e == BPF_OK && h[0] == 15 && h[1] == 16, "read trailing input bytes");
    bpf_memory_destroy(m);
}

static void test_write_input_rejected(void)
{
    bpf_memory *m;
    bpf_byte *h;
    bpf_err e;

    m = make_four();
    if (m == 0)
    {
        return;
    }
    e = acc(m, 0x1000, 1, BPF_MEM_W, &h);
    ck(e == BPF_EPERM, "write to read-only input rejected");
    e = acc(m, 0x2000, 1, BPF_MEM_W, &h);
    ck(e == BPF_EPERM, "write to read-only constant rejected");
    bpf_memory_destroy(m);
}

static void test_mem_region_rw(void)
{
    bpf_memory *m;
    bpf_byte *h;
    bpf_err e;

    m = make_four();
    if (m == 0)
    {
        return;
    }
    e = acc(m, 0x100000, 8, BPF_MEM_W, &h);
    ck(e == BPF_OK, "write working memory resolves");
    if (e == BPF_OK)
    {
        h[0] = 0xAB;
        h[1] = 0xCD;
    }
    e = acc(m, 0x100000, 2, BPF_MEM_R, &h);
    ck(e == BPF_OK && h[0] == 0xAB && h[1] == 0xCD, "read back working memory");
    bpf_memory_destroy(m);
}

static void test_stack_region_rw(void)
{
    bpf_memory *m;
    bpf_byte *h;
    bpf_err e;

    m = make_four();
    if (m == 0)
    {
        return;
    }
    e = acc(m, 0xF0000, 1, BPF_MEM_W, &h);
    ck(e == BPF_OK && h[0] == 0, "stack resolves and starts zeroed");
    bpf_memory_destroy(m);
}

static void test_addr_zero_unmapped(void)
{
    bpf_memory *m;
    bpf_byte *h;
    bpf_err e;

    m = make_four();
    if (m == 0)
    {
        return;
    }
    e = acc(m, 0, 1, BPF_MEM_R, &h);
    ck(e == BPF_EADDR, "guest address zero is unmapped");
    bpf_memory_destroy(m);
}

static void test_unmapped_gap(void)
{
    bpf_memory *m;
    bpf_byte *h;
    bpf_err e;

    m = make_four();
    if (m == 0)
    {
        return;
    }
    e = acc(m, 0x1200, 1, BPF_MEM_R, &h);
    ck(e == BPF_EADDR, "access to unmapped gap rejected");
    bpf_memory_destroy(m);
}

static void test_below_region(void)
{
    bpf_memory *m;
    bpf_byte *h;
    bpf_err e;

    m = make_four();
    if (m == 0)
    {
        return;
    }
    e = acc(m, 0xFFF, 1, BPF_MEM_R, &h);
    ck(e == BPF_EADDR, "address below region base rejected");
    bpf_memory_destroy(m);
}

static void test_region_end_exact(void)
{
    bpf_memory *m;
    bpf_byte *h;
    bpf_err e;

    m = make_four();
    if (m == 0)
    {
        return;
    }
    /* Last byte of input region (0x1000..0x1040). */
    e = acc(m, 0x103F, 1, BPF_MEM_R, &h);
    ck(e == BPF_OK, "access to the very last byte resolves");
    /* One byte past the region end must not resolve. */
    e = acc(m, 0x1040, 1, BPF_MEM_R, &h);
    ck(e == BPF_EADDR, "access exactly past region end rejected");
    bpf_memory_destroy(m);
}

static void test_spans_boundary(void)
{
    bpf_region_cfg c[2];
    bpf_memory *m;
    bpf_byte *h;
    bpf_err e;

    c[0] = mk(BPF_RINPUT, 0x1000, 0x40);
    c[1] = mk(BPF_RMEM, 0x1040, 0x40);
    cases = cases + 1;
    e = bpf_memory_create(c, 2, &m);
    ck(e == BPF_OK, "adjacent regions for span test");
    if (e != BPF_OK)
    {
        return;
    }
    /* A read crossing from region 0 into region 1 must be rejected. */
    e = acc(m, 0x1030, 0x20, BPF_MEM_R, &h);
    ck(e == BPF_EADDR, "interval spanning two regions rejected");
    /* Fully inside region 0 is fine. */
    e = acc(m, 0x1000, 0x40, BPF_MEM_R, &h);
    ck(e == BPF_OK, "interval wholly inside one region resolves");
    bpf_memory_destroy(m);
}

static void test_bytes_overrun(void)
{
    bpf_memory *m;
    bpf_byte *h;
    bpf_err e;

    m = make_four();
    if (m == 0)
    {
        return;
    }
    /* A byte count larger than the remaining region space is rejected. */
    e = acc(m, 0x1000, 0x41, BPF_MEM_R, &h);
    ck(e == BPF_EADDR, "byte count exceeding region remaining rejected");
    e = acc(m, 0x1000, 0x1000, BPF_MEM_R, &h);
    ck(e == BPF_EADDR, "large byte count rejected");
    bpf_memory_destroy(m);
}

int main(void)
{
    test_create_ok();
    test_overlap_rejected();
    test_adjacent_ok();
    test_zero_base_rejected();
    test_zero_len_rejected();
    test_too_many_rejected();
    test_overflow_rejected();
    test_init_len_rejected();
    test_read_input();
    test_write_input_rejected();
    test_mem_region_rw();
    test_stack_region_rw();
    test_addr_zero_unmapped();
    test_unmapped_gap();
    test_below_region();
    test_region_end_exact();
    test_spans_boundary();
    test_bytes_overrun();
    if (fails)
    {
        (void)fprintf(stderr, "test_guest_memory: %d failures (%d cases)\n",
                      fails, cases);
        return 1;
    }
    (void)printf("ok: test_guest_memory (%d cases)\n", cases);
    return 0;
}
