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

static void decode1(bpf_byte op, bpf_byte regs, int off, unsigned imm, int *ok,
                    bpf_insn *out)
{
    bpf_byte buf[8];
    bpf_u32 n;
    bpf_err e;

    buf[0] = op;
    buf[1] = regs;
    put16(buf + 2, off);
    put32(buf + 4, imm);
    e = bpf_decode(buf, 8, out, 1, &n);
    count = count + 1;
    *ok = (e == BPF_OK && n == 1);
}

static void test_field_roundtrip(void)
{
    bpf_insn ins;
    int ok;

    decode1(0xb7, 0x12, 0x0FFF, 0x89ABCDEFu, &ok, &ins);
    if (!ok)
    {
        (void)fprintf(stderr, "FAIL: decode1\n");
        fails = fails + 1;
        return;
    }
    if (ins.src_reg != 1 || ins.dst_reg != 2)
    {
        (void)fprintf(stderr, "FAIL: regs 0x12\n");
        fails = fails + 1;
    }
    if (ins.offset != 0x0FFF)
    {
        (void)fprintf(stderr, "FAIL: offset\n");
        fails = fails + 1;
    }
    if (ins.imm != (int)0x89ABCDEFu)
    {
        (void)fprintf(stderr, "FAIL: imm\n");
        fails = fails + 1;
    }
}

static void test_sign_extension(void)
{
    bpf_insn ins;
    int ok;

    decode1(0x05, 0, 0xFFFF, 0xFFFFFFFFu, &ok, &ins);
    if (!ok || ins.offset != -1)
    {
        (void)fprintf(stderr, "FAIL: offset -1\n");
        fails = fails + 1;
    }
    decode1(0x05, 0, 0x8000, 0x80000000u, &ok, &ins);
    if (!ok || ins.offset != -32768)
    {
        (void)fprintf(stderr, "FAIL: offset -32768\n");
        fails = fails + 1;
    }
    decode1(0xb7, 0, 0, 0x80000000u, &ok, &ins);
    if (!ok || ins.imm != (int)0x80000000u)
    {
        (void)fprintf(stderr, "FAIL: imm sign\n");
        fails = fails + 1;
    }
}

static void test_sequential_decode(void)
{
    bpf_byte buf[24];
    bpf_insn ins[3];
    bpf_u32 n;
    bpf_err e;
    int i;

    for (i = 0; i < 3; ++i)
    {
        bpf_byte *p;

        p = buf + 8 * i;
        p[0] = 0xb7;
        p[1] = (bpf_byte)i;
        put16(p + 2, 0);
        put32(p + 4, (unsigned)(10 + i));
    }
    e = bpf_decode(buf, 24, ins, 3, &n);
    count = count + 1;
    if (e != BPF_OK || n != 3)
    {
        (void)fprintf(stderr, "FAIL: sequential\n");
        fails = fails + 1;
        return;
    }
    for (i = 0; i < 3; ++i)
    {
        if (ins[i].imm != 10 + i || ins[i].dst_reg != (bpf_byte)i)
        {
            (void)fprintf(stderr, "FAIL: seq insn %d\n", i);
            fails = fails + 1;
        }
    }
}

static void test_wide_sequential(void)
{
    bpf_byte buf[24];
    bpf_insn ins[2];
    bpf_u32 n;
    bpf_err e;

    /* wide LD (16 bytes) then a basic MOV (8 bytes) */
    buf[0] = 0x18;
    buf[1] = 0x00;
    put16(buf + 2, 0);
    put32(buf + 4, 0x11111111u);
    put32(buf + 8, 0);
    put32(buf + 12, 0x22222222u);
    buf[16] = 0xb7;
    buf[17] = 0x01;
    put16(buf + 18, 0);
    put32(buf + 20, 42);
    e = bpf_decode(buf, 24, ins, 2, &n);
    count = count + 1;
    if (e != BPF_OK || n != 2)
    {
        (void)fprintf(stderr, "FAIL: wide seq count=%u\n", n);
        fails = fails + 1;
        return;
    }
    if (!ins[0].is_wide || ins[0].imm != (int)0x11111111u ||
        ins[0].next_imm != 0x22222222u)
    {
        (void)fprintf(stderr, "FAIL: wide first\n");
        fails = fails + 1;
    }
    if (ins[1].is_wide || ins[1].imm != 42)
    {
        (void)fprintf(stderr, "FAIL: basic second\n");
        fails = fails + 1;
    }
}

static void test_max_capacity(void)
{
    bpf_byte buf[16];
    bpf_insn ins[1];
    bpf_u32 n;
    bpf_err e;

    /* two instructions, capacity 1 -> ECOUNT */
    buf[0] = 0xb7;
    buf[1] = 0;
    put16(buf + 2, 0);
    put32(buf + 4, 1);
    buf[8] = 0xb7;
    buf[9] = 0;
    put16(buf + 10, 0);
    put32(buf + 12, 2);
    e = bpf_decode(buf, 16, ins, 1, &n);
    count = count + 1;
    if (e != BPF_ECOUNT)
    {
        (void)fprintf(stderr, "FAIL: ECOUNT\n");
        fails = fails + 1;
    }
}

int main(void)
{
    test_field_roundtrip();
    test_sign_extension();
    test_sequential_decode();
    test_wide_sequential();
    test_max_capacity();
    if (fails)
    {
        (void)fprintf(stderr, "test_decode3: %d/%d failed\n", fails, count);
        return 1;
    }
    (void)printf("ok: test_decode3 (%d cases)\n", count);
    return 0;
}
