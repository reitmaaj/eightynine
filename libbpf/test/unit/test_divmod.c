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

/* Run DIV/MOD; off=0 unsigned, off=1 signed. Returns resulting dst. */
static bpf_u64 rnd(bpf_byte code, int off, bpf_byte dst_reg, bpf_u64 dst,
                   int imm, bpf_byte *ok)
{
    bpf_insn in;
    bpf_regs r;
    bpf_err e;

    in =
        mk((bpf_byte)(((unsigned)code << 4) | (BPF_SRC_K << 3) | BPF_CLS_ALU64),
           0, dst_reg, imm, off);
    r.r[dst_reg] = dst;
    e = bpf_alu(&r, &in);
    *ok = (bpf_byte)(e == BPF_OK);
    return r.r[dst_reg];
}

static bpf_u64 rnd32(bpf_byte code, int off, bpf_byte dst_reg, bpf_u64 dst,
                     int imm, bpf_byte *ok)
{
    bpf_insn in;
    bpf_regs r;
    bpf_err e;

    in = mk((bpf_byte)(((unsigned)code << 4) | (BPF_SRC_K << 3) | BPF_CLS_ALU),
            0, dst_reg, imm, off);
    r.r[dst_reg] = dst;
    e = bpf_alu(&r, &in);
    *ok = (bpf_byte)(e == BPF_OK);
    return r.r[dst_reg];
}

#define I64MIN 0x8000000000000000UL
#define I32MIN 0x80000000UL

static void test_udiv_umod(void)
{
    bpf_u64 r;
    bpf_byte ok;

    r = rnd(BPF_ALU_DIV, 0, 1, 42, 6, &ok);
    ck(ok && r == 7, "udiv 42/6");
    r = rnd(BPF_ALU_DIV, 0, 1, 42, 0, &ok);
    ck(ok && r == 0, "udiv by zero -> 0");
    r = rnd(BPF_ALU_DIV, 0, 1, 0xFFFFFFFFFFFFFFFFUL, 1, &ok);
    ck(ok && r == 0xFFFFFFFFFFFFFFFFUL, "udiv max/1");
    r = rnd(BPF_ALU_MOD, 0, 1, 17, 5, &ok);
    ck(ok && r == 2, "umod 17%5");
    r = rnd(BPF_ALU_MOD, 0, 1, 17, 0, &ok);
    ck(ok && r == 17, "umod by zero unchanged");
    r = rnd32(BPF_ALU_DIV, 0, 1, 0x100000000UL, 2, &ok);
    ck(ok && r == 0, "udiv32 upper zeroed");
}

static void test_sdiv_smod(void)
{
    bpf_u64 r;
    bpf_byte ok;

    r = rnd(BPF_ALU_DIV, 1, 1, (bpf_u64)(bpf_i64)-42, 6, &ok);
    ck(ok && (bpf_i64)r == -7, "sdiv -42/6");
    r = rnd(BPF_ALU_DIV, 1, 1, (bpf_u64)(bpf_i64)-42, -6, &ok);
    ck(ok && (bpf_i64)r == 7, "sdiv -42/-6");
    r = rnd(BPF_ALU_DIV, 1, 1, I64MIN, -1, &ok);
    ck(ok && r == I64MIN, "sdiv INT64_MIN/-1 defined");
    r = rnd(BPF_ALU_DIV, 1, 1, 42, 0, &ok);
    ck(ok && r == 0, "sdiv by zero -> 0");
    r = rnd(BPF_ALU_MOD, 1, 1, (bpf_u64)(bpf_i64)-13, 3, &ok);
    ck(ok && (bpf_i64)r == -1, "smod -13%3 == -1");
    r = rnd(BPF_ALU_MOD, 1, 1, (bpf_u64)(bpf_i64)-13, -3, &ok);
    ck(ok && (bpf_i64)r == -1, "smod -13%-3 == -1");
    r = rnd(BPF_ALU_MOD, 1, 1, 13, -3, &ok);
    ck(ok && (bpf_i64)r == 1, "smod 13%-3 == 1");
    r = rnd(BPF_ALU_MOD, 1, 1, 42, 0, &ok);
    ck(ok && r == 42, "smod by zero unchanged");
    r = rnd(BPF_ALU_MOD, 1, 1, I64MIN, -1, &ok);
    ck(ok && r == 0, "smod INT64_MIN%-1 == 0");
    r = rnd32(BPF_ALU_DIV, 1, 1, I32MIN, -1, &ok);
    ck(ok && r == I32MIN, "sdiv32 INT32_MIN/-1 defined");
    r = rnd32(BPF_ALU_DIV, 1, 1, 42, 3, &ok);
    ck(ok && r == 14, "sdiv32 42/3");
    r = rnd32(BPF_ALU_DIV, 1, 1, (bpf_u64)(bpf_i32)-42, 3, &ok);
    ck(ok && (bpf_i32)r == -14, "sdiv32 -42/3");
    r = rnd32(BPF_ALU_DIV, 1, 1, 42, 0, &ok);
    ck(ok && r == 0, "sdiv32 by zero -> 0");
    r = rnd32(BPF_ALU_MOD, 1, 1, 13, -1, &ok);
    ck(ok && r == 0, "smod32 13%-1 == 0");
    r = rnd32(BPF_ALU_MOD, 1, 1, 42, 0, &ok);
    ck(ok && r == 42, "smod32 by zero unchanged");
}

int main(void)
{
    test_udiv_umod();
    test_sdiv_smod();
    if (fails)
    {
        (void)fprintf(stderr, "test_divmod: %d failures\n", fails);
        return 1;
    }
    (void)printf("ok: test_divmod\n");
    return 0;
}
