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

static bpf_insn mk(bpf_byte op, bpf_byte src, bpf_byte dst, int imm, int off)
{
    bpf_insn i;

    i.opcode = op;
    i.class = (bpf_byte)(op & 0x07u);
    i.code = (bpf_byte)(op >> 4);
    i.source = (bpf_byte)((op >> 3) & 0x01u);
    i.mode = 0;
    i.size = 0;
    i.src_reg = src;
    i.dst_reg = dst;
    i.offset = off;
    i.imm = imm;
    i.is_wide = 0;
    i.next_imm = 0;
    return i;
}

/* Run an ALU/ALU64 op over dst/src and return the resulting register value. */
static bpf_u64 run64(bpf_byte code, bpf_byte source, bpf_byte dst_reg,
                     bpf_u64 dst, bpf_u64 srcv, int imm, int off, bpf_byte *ok)
{
    bpf_insn in;
    bpf_regs r;
    bpf_err e;

    in = mk((bpf_byte)(((unsigned)code << 4) | (((unsigned)source & 1u) << 3) |
                       BPF_CLS_ALU64),
            source ? (bpf_byte)9 : 0, dst_reg, imm, off);
    r.r[dst_reg] = dst;
    r.r[9] = srcv;
    e = bpf_alu(&r, &in);
    *ok = (bpf_byte)(e == BPF_OK);
    return r.r[dst_reg];
}

static bpf_u64 run32(bpf_byte code, bpf_byte source, bpf_byte dst_reg,
                     bpf_u64 dst, bpf_u64 srcv, int imm, int off, bpf_byte *ok)
{
    bpf_insn in;
    bpf_regs r;
    bpf_err e;

    in = mk((bpf_byte)(((unsigned)code << 4) | (((unsigned)source & 1u) << 3) |
                       BPF_CLS_ALU),
            source ? (bpf_byte)9 : 0, dst_reg, imm, off);
    r.r[dst_reg] = dst;
    r.r[9] = srcv;
    e = bpf_alu(&r, &in);
    *ok = (bpf_byte)(e == BPF_OK);
    return r.r[dst_reg];
}

static void test_add_sub(void)
{
    bpf_u64 r;
    bpf_byte ok;
    bpf_u64 max;
    bpf_u64 min;

    max = 0xFFFFFFFFFFFFFFFFUL;
    min = 0x8000000000000000UL;
    r = run64(BPF_ALU_ADD, BPF_SRC_K, 1, 5, 0, 3, 0, &ok);
    ck(ok && r == 8, "add64 K 5+3");
    r = run64(BPF_ALU_ADD, BPF_SRC_X, 1, max, 1, 0, 0, &ok);
    ck(ok && r == 0, "add64 wrap max+1");
    r = run64(BPF_ALU_SUB, BPF_SRC_K, 1, 5, 0, 10, 0, &ok);
    ck(ok && r == max - 4, "sub64 wrap 5-10");
    r = run64(BPF_ALU_SUB, BPF_SRC_X, 1, min, 1, 0, 0, &ok);
    ck(ok && r == min - 1, "sub64 min-1");
    r = run32(BPF_ALU_ADD, BPF_SRC_K, 1, 0xFFFFFFFFFFFFFFFFUL, 0, 3, 0, &ok);
    ck(ok && r == 2, "add32 zero-extend (wrap 0xFFFFFFFF+3)");
    r = run32(BPF_ALU_SUB, BPF_SRC_K, 1, 0, 0, 1, 0, &ok);
    ck(ok && r == 0xFFFFFFFFUL, "sub32 wrap 0-1");
}

static void test_mul_or_and_xor(void)
{
    bpf_u64 r;
    bpf_byte ok;

    r = run64(BPF_ALU_MUL, BPF_SRC_K, 1, 6, 0, 7, 0, &ok);
    ck(ok && r == 42, "mul64");
    r = run64(BPF_ALU_MUL, BPF_SRC_X, 1, 0x100000000UL, 0x100000000UL, 0, 0,
              &ok);
    ck(ok && r == 0, "mul64 2^32*2^32 wraps to 0");
    r = run64(BPF_ALU_OR, BPF_SRC_K, 1, 0x0F00UL, 0, 0x00F0UL, 0, &ok);
    ck(ok && r == 0x0FF0UL, "or64");
    r = run64(BPF_ALU_AND, BPF_SRC_K, 1, 0xFF0FUL, 0, 0x0FF0UL, 0, &ok);
    ck(ok && r == 0x0F00UL, "and64");
    r = run64(BPF_ALU_XOR, BPF_SRC_K, 1, 0x0F0FUL, 0, 0x00FFUL, 0, &ok);
    ck(ok && r == 0x0FF0UL, "xor64");
    r = run32(BPF_ALU_MUL, BPF_SRC_K, 1, 0x10000UL, 0, 0x10000UL, 0, &ok);
    ck(ok && r == 0UL, "mul32 2^16*2^16=0 (zero-extend)");
    r = run32(BPF_ALU_OR, BPF_SRC_K, 1, 0xFFFFFFFFUL, 0, 0x00000000UL, 0, &ok);
    ck(ok && r == 0xFFFFFFFFUL, "or32");
    r = run32(BPF_ALU_XOR, BPF_SRC_K, 1, 0xFFFFFFFFUL, 0, -1, 0, &ok);
    ck(ok && r == 0UL, "xor32 self");
}

static void test_neg_mov(void)
{
    bpf_u64 r;
    bpf_byte ok;

    r = run64(BPF_ALU_NEG, BPF_SRC_K, 1, 5, 0, 0, 0, &ok);
    ck(ok && r == 0xFFFFFFFFFFFFFFFBUL, "neg64 5");
    r = run64(BPF_ALU_NEG, BPF_SRC_K, 1, 0, 0, 0, 0, &ok);
    ck(ok && r == 0, "neg64 0");
    r = run64(BPF_ALU_MOV, BPF_SRC_K, 1, 99, 0, -1, 0, &ok);
    ck(ok && r == 0xFFFFFFFFFFFFFFFFUL, "mov64 K -1 se");
    r = run64(BPF_ALU_MOV, BPF_SRC_X, 1, 0, 0xABCDUL, 0, 0, &ok);
    ck(ok && r == 0xABCDUL, "mov64 X copy");
    r = run32(BPF_ALU_NEG, BPF_SRC_K, 1, 0xFFFFFFFFUL, 0, 0, 0, &ok);
    ck(ok && r == 1, "neg32 0xFFFFFFFF -> 1");
    r = run32(BPF_ALU_MOV, BPF_SRC_X, 1, 0xFFFFFFFFFFFFFFFFUL, 0x12345678UL, 0,
              0, &ok);
    ck(ok && r == 0x12345678UL, "mov32 X zero-extend");
}

int main(void)
{
    test_add_sub();
    test_mul_or_and_xor();
    test_neg_mov();
    if (fails)
    {
        (void)fprintf(stderr, "test_alu: %d failures\n", fails);
        return 1;
    }
    (void)printf("ok: test_alu\n");
    return 0;
}
