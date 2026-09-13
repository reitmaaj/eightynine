#include <stdio.h>

#include "decode.h"

static int fails;
static int count;

static void put16(bpf_byte *p, int v)
{
    p[0] = (bpf_byte)(v & 0xFF);
    p[1] = (bpf_byte)((v >> 8) & 0xFF);
}

static void put32(bpf_byte *p, unsigned v)
{
    p[0] = (bpf_byte)(v & 0xFFu);
    p[1] = (bpf_byte)((v >> 8) & 0xFFu);
    p[2] = (bpf_byte)((v >> 16) & 0xFFu);
    p[3] = (bpf_byte)((v >> 24) & 0xFFu);
}

static void t_decode(const bpf_byte *buf, bpf_u32 len, bpf_insn *ins,
                     bpf_u32 cap, bpf_u32 *n, bpf_err want)
{
    bpf_err e;

    e = bpf_decode(buf, len, ins, cap, n);
    count = count + 1;
    if (e != want)
    {
        (void)fprintf(stderr, "FAIL: len=%u want=%d got=%d\n", len, (int)want,
                      (int)e);
        fails = fails + 1;
    }
}

static void test_trailing_bytes(void)
{
    bpf_byte buf[8];
    bpf_insn ins[4];
    bpf_u32 n;

    /* lengths 1..7 cannot form an instruction -> ETRUNC */
    buf[0] = 0x95;
    t_decode(buf, 1, ins, 4, &n, BPF_ETRUNC);
    t_decode(buf, 4, ins, 4, &n, BPF_ETRUNC);
    t_decode(buf, 7, ins, 4, &n, BPF_ETRUNC);
}

static void test_wide_needs_16(void)
{
    bpf_byte buf[15];
    bpf_insn ins[4];
    bpf_u32 n;
    int i;

    for (i = 0; i < 15; ++i)
    {
        buf[i] = 0;
    }
    buf[0] = 0x18; /* wide LD */
    /* 15 bytes is not enough for the 16-byte wide instruction */
    t_decode(buf, 15, ins, 4, &n, BPF_ETRUNC);
    t_decode(buf, 8, ins, 4, &n, BPF_ETRUNC);
}

static void test_exact_wide(void)
{
    bpf_byte buf[16];
    bpf_insn ins[2];
    bpf_u32 n;

    buf[0] = 0x18;
    buf[1] = 0x10;
    put16(buf + 2, 0);
    put32(buf + 4, 0xAAAAAAAAu);
    put32(buf + 8, 0);
    put32(buf + 12, 0x55555555u);
    t_decode(buf, 16, ins, 2, &n, BPF_OK);
    if (n != 1 || !ins[0].is_wide || ins[0].src_reg != 1 ||
        ins[0].dst_reg != 0 || ins[0].imm != (int)0xAAAAAAAAu ||
        ins[0].next_imm != 0x55555555u)
    {
        (void)fprintf(stderr, "FAIL: exact wide fields\n");
        fails = fails + 1;
    }
}

static void test_zero_length_buffer(void)
{
    bpf_insn ins[4];
    bpf_u32 n;

    t_decode(0, 0, ins, 4, &n, BPF_OK);
    if (n != 0)
    {
        (void)fprintf(stderr, "FAIL: zero len count\n");
        fails = fails + 1;
    }
}

static void test_many_instructions(void)
{
    bpf_byte buf[64];
    bpf_insn ins[8];
    bpf_u32 n;
    bpf_err e;
    int i;

    for (i = 0; i < 8; ++i)
    {
        bpf_byte *p;

        p = buf + 8 * i;
        p[0] = 0xb7;
        p[1] = (bpf_byte)i;
        put16(p + 2, 0);
        put32(p + 4, (unsigned)(100 + i));
    }
    e = bpf_decode(buf, 64, ins, 8, &n);
    count = count + 1;
    if (e != BPF_OK || n != 8)
    {
        (void)fprintf(stderr, "FAIL: many insns\n");
        fails = fails + 1;
        return;
    }
    for (i = 0; i < 8; ++i)
    {
        if (ins[i].imm != 100 + i)
        {
            (void)fprintf(stderr, "FAIL: many insn %d\n", i);
            fails = fails + 1;
        }
    }
}

static void test_small_capacity(void)
{
    bpf_byte buf[32];
    bpf_insn ins[3];
    bpf_u32 n;
    int i;

    for (i = 0; i < 4; ++i)
    {
        bpf_byte *p;

        p = buf + 8 * i;
        p[0] = 0xb7;
        p[1] = 0;
        put16(p + 2, 0);
        put32(p + 4, 1);
    }
    t_decode(buf, 32, ins, 3, &n, BPF_ECOUNT);
}

int main(void)
{
    test_trailing_bytes();
    test_wide_needs_16();
    test_exact_wide();
    test_zero_length_buffer();
    test_many_instructions();
    test_small_capacity();
    if (fails)
    {
        (void)fprintf(stderr, "test_decode4: %d/%d failed\n", fails, count);
        return 1;
    }
    (void)printf("ok: test_decode4 (%d cases)\n", count);
    return 0;
}
