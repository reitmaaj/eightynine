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

static bpf_insn alu(bpf_byte code, bpf_byte cls, bpf_byte src, bpf_byte dst,
                    int imm, int off)
{
    bpf_insn i;

    i.opcode = (bpf_byte)(((unsigned)code << 4) | ((src & 1u) << 3) | cls);
    i.class = cls;
    i.code = code;
    i.source = (bpf_byte)(src & 1u);
    i.mode = 0;
    i.size = 0;
    i.src_reg = src ? (bpf_byte)9 : 0;
    i.dst_reg = dst;
    i.offset = off;
    i.imm = imm;
    i.is_wide = 0;
    i.next_imm = 0;
    return i;
}

static bpf_u64 run(bpf_byte code, bpf_byte cls, bpf_byte src, bpf_u64 a,
                   bpf_u64 b, int imm, int off)
{
    bpf_insn in;
    bpf_regs r;

    in = alu(code, cls, src, 1, imm, off);
    r.r[0] = 0;
    r.r[1] = a;
    r.r[9] = b;
    (void)bpf_alu(&r, &in);
    return r.r[1];
}

#define MAX 0xFFFFFFFFFFFFFFFFUL
#define MIN 0x8000000000000000UL
#define I32MAX 0x7FFFFFFFUL
#define I32MIN 0x80000000UL

static void test_add_edges(void)
{
    ck(run(BPF_ALU_ADD, BPF_CLS_ALU64, BPF_SRC_K, MAX, 0, 0, 0) == MAX,
       "max+0");
    ck(run(BPF_ALU_ADD, BPF_CLS_ALU64, BPF_SRC_K, MAX, 0, 1, 0) == 0,
       "max+1 wraps");
    ck(run(BPF_ALU_ADD, BPF_CLS_ALU64, BPF_SRC_K, MAX, 0, -1, 0) == MAX - 1,
       "max-1");
    ck(run(BPF_ALU_ADD, BPF_CLS_ALU64, BPF_SRC_K, 1, 0, -1, 0) == 0, "1-1");
    ck(run(BPF_ALU_ADD, BPF_CLS_ALU64, BPF_SRC_X, MIN, MIN, 0, 0) == 0,
       "min+min wraps to 0");
    ck(run(BPF_ALU_ADD, BPF_CLS_ALU, BPF_SRC_K, I32MAX, 0, 1, 0) ==
           (bpf_u64)I32MIN,
       "i32max+1 -> i32min");
    ck(run(BPF_ALU_ADD, BPF_CLS_ALU, BPF_SRC_X, 0x80000000UL, 0x80000000UL, 0,
           0) == 0,
       "32 wrap");
}

static void test_sub_edges(void)
{
    ck(run(BPF_ALU_SUB, BPF_CLS_ALU64, BPF_SRC_K, 0, 0, 1, 0) == MAX, "0-1");
    ck(run(BPF_ALU_SUB, BPF_CLS_ALU64, BPF_SRC_K, MIN, 0, 1, 0) == MIN - 1,
       "min-1");
    ck(run(BPF_ALU_SUB, BPF_CLS_ALU64, BPF_SRC_K, 0, 0, -1, 0) == 1, "0-(-1)");
    ck(run(BPF_ALU_SUB, BPF_CLS_ALU64, BPF_SRC_X, MAX, MAX, 0, 0) == 0,
       "max-max");
}

static void test_mul_edges(void)
{
    ck(run(BPF_ALU_MUL, BPF_CLS_ALU64, BPF_SRC_K, 0, 0, 5, 0) == 0, "0*5");
    ck(run(BPF_ALU_MUL, BPF_CLS_ALU64, BPF_SRC_K, MAX, 0, 1, 0) == MAX,
       "max*1");
    ck(run(BPF_ALU_MUL, BPF_CLS_ALU64, BPF_SRC_K, MAX, 0, 2, 0) == MAX - 1,
       "max*2 wraps to max-1");
    ck(run(BPF_ALU_MUL, BPF_CLS_ALU64, BPF_SRC_X, 0x100000000UL, 0x100000000UL,
           0, 0) == 0,
       "2^32 * 2^32 = 0 (64)");
    ck(run(BPF_ALU_MUL, BPF_CLS_ALU, BPF_SRC_K, 0x10000UL, 0, 0x10000UL, 0) ==
           0,
       "32-bit mul wrap");
}

static void test_neg_edges(void)
{
    ck(run(BPF_ALU_NEG, BPF_CLS_ALU64, BPF_SRC_K, MIN, 0, 0, 0) == MIN,
       "neg(min) = min");
    ck(run(BPF_ALU_NEG, BPF_CLS_ALU64, BPF_SRC_K, 1, 0, 0, 0) == MAX, "neg(1)");
    ck(run(BPF_ALU_NEG, BPF_CLS_ALU64, BPF_SRC_K, MAX, 0, 0, 0) == 1,
       "neg(max)");
    ck(run(BPF_ALU_NEG, BPF_CLS_ALU, BPF_SRC_K, 0x80000000UL, 0, 0, 0) ==
           0x80000000UL,
       "neg32(i32min) = i32min");
}

static void test_shift_edges(void)
{
    ck(run(BPF_ALU_LSH, BPF_CLS_ALU64, BPF_SRC_K, 1, 0, 0, 0) == 1, "lsh0");
    ck(run(BPF_ALU_LSH, BPF_CLS_ALU64, BPF_SRC_K, 1, 0, 64, 0) == 1,
       "lsh64->0");
    ck(run(BPF_ALU_LSH, BPF_CLS_ALU64, BPF_SRC_K, 1, 0, 127, 0) ==
           0x8000000000000000UL,
       "lsh127->63");
    ck(run(BPF_ALU_RSH, BPF_CLS_ALU64, BPF_SRC_K, MAX, 0, 63, 0) == 1, "rsh63");
    ck(run(BPF_ALU_RSH, BPF_CLS_ALU64, BPF_SRC_K, MAX, 0, 64, 0) == MAX,
       "rsh64->0 keeps max");
    ck(run(BPF_ALU_ARSH, BPF_CLS_ALU64, BPF_SRC_K, MIN, 0, 63, 0) == MAX,
       "arsh(min,63) fills sign");
}

static void test_mov_edges(void)
{
    ck(run(BPF_ALU_MOV, BPF_CLS_ALU64, BPF_SRC_K, 9, 0, 0x7FFFFFFF, 0) ==
           0x7FFFFFFFUL,
       "mov K positive");
    ck(run(BPF_ALU_MOV, BPF_CLS_ALU64, BPF_SRC_K, 9, 0, -2147483647 - 1, 0) ==
           0xFFFFFFFF80000000UL,
       "mov K i32min se");
    ck(run(BPF_ALU_MOV, BPF_CLS_ALU, BPF_SRC_X, MAX, 0xFFFFFFFFUL, 0, 0) ==
           0xFFFFFFFFUL,
       "mov32 X all ones");
    ck(run(BPF_ALU_MOV, BPF_CLS_ALU64, BPF_SRC_X, 0, MAX, 0, 0) == MAX,
       "mov X copy max");
}

int main(void)
{
    test_add_edges();
    test_sub_edges();
    test_mul_edges();
    test_neg_edges();
    test_shift_edges();
    test_mov_edges();
    if (fails)
    {
        (void)fprintf(stderr, "test_edge: %d failures\n", fails);
        return 1;
    }
    (void)printf("ok: test_edge\n");
    return 0;
}
