#include <stdio.h>

#include "eval.h"

static int fails;

static void ck(int cond, const char *msg)
{
    if (!cond)
    {
        (void)fprintf(stderr, "FAIL: %s\n", msg);
        fails = fails + 1;
    }
}

static bpf_insn mk(bpf_byte code, bpf_byte source, bpf_byte cls, int imm)
{
    bpf_insn i;

    i.opcode = (bpf_byte)(((unsigned)code << 4) |
                          (((unsigned)source & 1u) << 3) | cls);
    i.class = cls;
    i.code = code;
    i.source = source;
    i.mode = 0;
    i.size = 0;
    i.src_reg = source ? (bpf_byte)9 : 0;
    i.dst_reg = 1;
    i.offset = 0;
    i.imm = imm;
    i.is_wide = 0;
    i.next_imm = 0;
    return i;
}

static bpf_u64 run64(bpf_byte code, int count, bpf_u64 dst, bpf_byte *ok)
{
    bpf_insn in;
    bpf_regs r;
    bpf_err e;

    in = mk(code, BPF_SRC_K, BPF_CLS_ALU64, count);
    r.r[0] = 0;
    r.r[1] = dst;
    e = bpf_alu(&r, &in);
    *ok = (bpf_byte)(e == BPF_OK);
    return r.r[1];
}

static bpf_u64 run32(bpf_byte code, int count, bpf_u64 dst, bpf_byte *ok)
{
    bpf_insn in;
    bpf_regs r;
    bpf_err e;

    in = mk(code, BPF_SRC_K, BPF_CLS_ALU, count);
    r.r[0] = 0;
    r.r[1] = dst;
    e = bpf_alu(&r, &in);
    *ok = (bpf_byte)(e == BPF_OK);
    return r.r[1];
}

static void test_lsh_mask64(void)
{
    bpf_u64 r;
    bpf_byte ok;

    r = run64(BPF_ALU_LSH, 0, 1, &ok);
    ck(ok && r == 1, "lsh64 count 0");
    r = run64(BPF_ALU_LSH, 1, 1, &ok);
    ck(ok && r == 2, "lsh64 count 1");
    r = run64(BPF_ALU_LSH, 63, 1, &ok);
    ck(ok && r == 0x8000000000000000UL, "lsh64 count 63");
    r = run64(BPF_ALU_LSH, 64, 1, &ok);
    ck(ok && r == 1, "lsh64 count 64 masked to 0");
    r = run64(BPF_ALU_LSH, 70, 1, &ok);
    ck(ok && r == 64, "lsh64 count 70 masked to 6");
}

static void test_rsh_arsh64(void)
{
    bpf_u64 r;
    bpf_byte ok;

    r = run64(BPF_ALU_RSH, 2, 0x10, &ok);
    ck(ok && r == 4, "rsh64");
    r = run64(BPF_ALU_RSH, 70, 0x4000, &ok);
    ck(ok && r == 0x100, "rsh64 masked");
    r = run64(BPF_ALU_ARSH, 4, 0xFFFFFFFFFFFFFF80UL, &ok);
    ck(ok && r == 0xFFFFFFFFFFFFFFF8UL, "arsh64 sign extend");
    r = run64(BPF_ALU_ARSH, 4, 0x10, &ok);
    ck(ok && r == 1, "arsh64 positive");
    r = run64(BPF_ALU_ARSH, 70, 0x8000000000000000UL, &ok);
    ck(ok && r == 0xFE00000000000000UL, "arsh64 masked signed");
}

static void test_shift32_mask(void)
{
    bpf_u64 r;
    bpf_byte ok;

    r = run32(BPF_ALU_LSH, 31, 1, &ok);
    ck(ok && r == 0x80000000UL, "lsh32 count 31");
    r = run32(BPF_ALU_LSH, 32, 1, &ok);
    ck(ok && r == 1, "lsh32 count 32 masked to 0");
    r = run32(BPF_ALU_LSH, 34, 1, &ok);
    ck(ok && r == 4, "lsh32 count 34 masked to 2");
    r = run32(BPF_ALU_RSH, 2, 0x100, &ok);
    ck(ok && r == 0x40, "rsh32");
    r = run32(BPF_ALU_ARSH, 4, 0xFFFFFFFFUL, &ok);
    ck(ok && r == 0xFFFFFFFFUL, "arsh32 sign fill");
    r = run32(BPF_ALU_ARSH, 4, 0x10, &ok);
    ck(ok && r == 1, "arsh32 positive");
}

int main(void)
{
    test_lsh_mask64();
    test_rsh_arsh64();
    test_shift32_mask();
    if (fails)
    {
        (void)fprintf(stderr, "test_shift: %d failures\n", fails);
        return 1;
    }
    (void)printf("ok: test_shift\n");
    return 0;
}
