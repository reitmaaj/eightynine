#include <stdio.h>

#include "decode.h"

static int fails;

static void ck(int cond, const char *msg)
{
    if (!cond)
    {
        (void)fprintf(stderr, "FAIL: %s\n", msg);
        fails = fails + 1;
    }
}

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

static void basic(bpf_byte *b, bpf_byte op, bpf_byte regs, int off,
                  unsigned imm)
{
    b[0] = op;
    b[1] = regs;
    put16(b + 2, off);
    put32(b + 4, imm);
}

static void test_empty_buffer(void)
{
    bpf_insn ins[4];
    bpf_u32 n;
    bpf_err e;

    e = bpf_decode(0, 0, ins, 4, &n);
    ck(e == BPF_OK && n == 0, "empty buffer ok, zero insns");
}

static void test_ecoount(void)
{
    bpf_byte buf[16];
    bpf_insn ins[1];
    bpf_u32 n;
    bpf_err e;

    basic(buf, 0xb7, 0x01, 0, 1);
    basic(buf + 8, 0xb7, 0x02, 0, 2);
    e = bpf_decode(buf, 16, ins, 1, &n);
    ck(e == BPF_ECOUNT, "ECOUNT when capacity too small");
}

static void test_partial_trailing(void)
{
    bpf_byte buf[16];
    bpf_insn ins[4];
    bpf_u32 n;
    bpf_err e;

    basic(buf, 0xb7, 0x01, 0, 1);
    /* Only 4 partial bytes of a second instruction follow. */
    buf[8] = 0xb7;
    buf[9] = 0x02;
    put16(buf + 10, 0);
    e = bpf_decode(buf, 12, ins, 4, &n);
    ck(e == BPF_ETRUNC, "partial trailing instruction rejected");
}

static void test_offset_imm_extremes(void)
{
    bpf_byte buf[8];
    bpf_insn ins[1];
    bpf_u32 n;
    bpf_err e;

    basic(buf, 0x05, 0, -1, 0xFFFFFFFFu); /* JA off -1, imm 0xFFFFFFFF */
    e = bpf_decode(buf, 8, ins, 1, &n);
    ck(e == BPF_OK && ins[0].offset == -1, "offset sign-extended");
    ck(e == BPF_OK && ins[0].imm == -1, "imm sign-extended");

    basic(buf, 0x05, 0, 32767, 0x7FFFFFFFu);
    e = bpf_decode(buf, 8, ins, 1, &n);
    ck(e == BPF_OK && ins[0].offset == 32767, "offset max positive");
    ck(e == BPF_OK && ins[0].imm == 0x7FFFFFFF, "imm max positive");
}

static void test_regs_all_values(void)
{
    bpf_byte buf[8];
    bpf_insn ins[1];
    bpf_u32 n;
    bpf_err e;
    int i;

    for (i = 0; i < 256; ++i)
    {
        basic(buf, 0xbf, (bpf_byte)i, 0, 0);
        e = bpf_decode(buf, 8, ins, 1, &n);
        ck(e == BPF_OK && ins[0].src_reg == (bpf_byte)(i >> 4),
           "src_reg from high nibble");
        ck(e == BPF_OK && ins[0].dst_reg == (bpf_byte)(i & 0x0F),
           "dst_reg from low nibble");
    }
}

static void test_wide_variants(void)
{
    bpf_byte buf[16];
    bpf_insn ins[1];
    bpf_u32 n;
    bpf_err e;
    int subtype;

    for (subtype = 0; subtype <= 6; ++subtype)
    {
        basic(buf, 0x18, (bpf_byte)((subtype << 4) | 1), 0, 0x11111111u);
        put32(buf + 8, 0);            /* reserved */
        put32(buf + 12, 0x22222222u); /* next_imm */
        e = bpf_decode(buf, 16, ins, 1, &n);
        ck(e == BPF_OK && ins[0].is_wide == 1, "wide detected");
        ck(e == BPF_OK && ins[0].src_reg == (bpf_byte)subtype,
           "wide subtype captured");
        ck(e == BPF_OK && ins[0].imm == (int)0x11111111u, "wide imm");
        ck(e == BPF_OK && ins[0].next_imm == 0x22222222u, "wide next_imm");
    }
}

static void test_non_ld_imm_not_wide(void)
{
    bpf_byte buf[8];
    bpf_insn ins[1];
    bpf_u32 n;
    bpf_err e;

    /* STX class (3) with mode IMM(0) is a basic instruction, not wide. */
    basic(buf, 0x03, 0, 0, 0);
    e = bpf_decode(buf, 8, ins, 1, &n);
    ck(e == BPF_OK && ins[0].is_wide == 0, "non-LD IMM is not wide");
}

static void test_alu_class_decomposition(void)
{
    bpf_byte buf[8];
    bpf_insn ins[1];
    bpf_u32 n;
    bpf_err e;
    int code;
    int sbit;

    for (code = 0; code <= 15; ++code)
    {
        for (sbit = 0; sbit <= 1; ++sbit)
        {
            basic(buf,
                  (bpf_byte)(((unsigned)code << 4) | ((unsigned)sbit << 3) | 7),
                  0, 0, 0);
            e = bpf_decode(buf, 8, ins, 1, &n);
            ck(e == BPF_OK && ins[0].class == BPF_CLS_ALU64, "alu64 class");
            ck(e == BPF_OK && ins[0].code == (bpf_byte)code, "alu code");
            ck(e == BPF_OK && ins[0].source == (bpf_byte)sbit, "alu source");
        }
    }
}

int main(void)
{
    test_empty_buffer();
    test_ecoount();
    test_partial_trailing();
    test_offset_imm_extremes();
    test_regs_all_values();
    test_wide_variants();
    test_non_ld_imm_not_wide();
    test_alu_class_decomposition();
    if (fails)
    {
        (void)fprintf(stderr, "test_decode2: %d failures\n", fails);
        return 1;
    }
    (void)printf("ok: test_decode2\n");
    return 0;
}
