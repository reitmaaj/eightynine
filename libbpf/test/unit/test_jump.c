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

/* Run a single conditional jump with dst and src in registers, return taken. */
static int taken(bpf_byte code, int is32, int use_src, bpf_u64 dst,
                 bpf_u64 srcv, int imm, int off)
{
    bpf_insn prog[2];
    bpf_machine m;
    bpf_status st;
    bpf_byte op;

    op = (bpf_byte)(((unsigned)code << 4) |
                    (((unsigned)(use_src ? 1 : 0)) << 3) |
                    (is32 ? BPF_CLS_JMP32 : BPF_CLS_JMP));
    prog[0] = mk(op, use_src ? (bpf_byte)9 : 0, 1, imm, off);
    prog[1] = mk(0x95, 0, 0, 0, 0); /* EXIT */
    bpf_machine_init(&m, prog, 2);
    m.regs.r[1] = dst;
    m.regs.r[9] = srcv;
    st = bpf_step(&m);
    if (st != BPF_STAT_RUNNING)
    {
        return -1;
    }
    if (m.pc == 1)
    {
        return 0;
    }
    if ((int)m.pc == 1 + off)
    {
        return 1;
    }
    return -1;
}

#define BIG 0xFFFFFFFFFFFFFFFFUL

static void test_eq_ne(void)
{
    ck(taken(BPF_JMP_JEQ, 0, 0, 5, 0, 5, 3) == 1, "jeq64 K equal");
    ck(taken(BPF_JMP_JEQ, 0, 0, 5, 0, 6, 3) == 0, "jeq64 K not equal");
    ck(taken(BPF_JMP_JEQ, 0, 1, 5, 5, 0, 3) == 1, "jeq64 X equal");
    ck(taken(BPF_JMP_JNE, 0, 1, 5, 6, 0, 3) == 1, "jne64 X diff");
    ck(taken(BPF_JMP_JNE, 0, 1, 5, 5, 0, 3) == 0, "jne64 X same");
    ck(taken(BPF_JMP_JEQ, 1, 1, 0x0000000100000002UL, 2, 0, 3) == 1,
       "jeq32 low32 compare");
}

static void test_unsigned_gt_ge_lt_le(void)
{
    ck(taken(BPF_JMP_JGT, 0, 0, 10, 0, 5, 3) == 1, "jgt64 10>5");
    ck(taken(BPF_JMP_JGT, 0, 0, BIG, 0, -1, 3) == 0,
       "jgt64 max vs -1 unsigned false");
    ck(taken(BPF_JMP_JGT, 0, 1, BIG, 1, 0, 3) == 1, "jgt64 max > 1");
    ck(taken(BPF_JMP_JGE, 0, 1, 5, 5, 0, 3) == 1, "jge64 equal");
    ck(taken(BPF_JMP_JLT, 0, 0, 3, 0, 7, 3) == 1, "jlt64 3<7");
    ck(taken(BPF_JMP_JLT, 0, 0, BIG - 1, 0, -1, 3) == 1,
       "jlt64 max-1 < -1(unsigned)");
    ck(taken(BPF_JMP_JLE, 0, 1, 5, 5, 0, 3) == 1, "jle64 equal");
    ck(taken(BPF_JMP_JLE, 0, 1, 6, 5, 0, 3) == 0, "jle64 6<=5 false");
}

static void test_signed_gt_ge_lt_le(void)
{
    /* -1 (0xFFF..) as i64 < 1 */
    ck(taken(BPF_JMP_JSGT, 0, 1, BIG, 1, 0, 3) == 0,
       "jsgt64 -1 > 1 false (signed)");
    ck(taken(BPF_JMP_JSGT, 0, 1, BIG, BIG - 1, 0, 3) == 1,
       "jsgt64 -1 > -2 true");
    ck(taken(BPF_JMP_JSGT, 0, 1, BIG - 1, BIG, 0, 3) == 0,
       "jsgt64 -2 > -1 false");
    ck(taken(BPF_JMP_JSGE, 0, 1, BIG, BIG, 0, 3) == 1, "jsge64 equal negative");
    ck(taken(BPF_JMP_JSLT, 0, 1, BIG, 1, 0, 3) == 1, "jslt64 -1 < 1 true");
    ck(taken(BPF_JMP_JSLT, 0, 1, 1, BIG, 0, 3) == 0, "jslt64 1 < -1 false");
    ck(taken(BPF_JMP_JSLE, 0, 1, BIG, BIG, 0, 3) == 1, "jsle64 equal negative");
    ck(taken(BPF_JMP_JSLE, 0, 1, 0, BIG, 0, 3) == 0, "jsle64 0 <= -1 false");
}

static void test_jset(void)
{
    ck(taken(BPF_JMP_JSET, 0, 0, 0x06, 0, 0x04, 3) == 1, "jset64 overlap");
    ck(taken(BPF_JMP_JSET, 0, 0, 0x06, 0, 0x08, 3) == 0, "jset64 no overlap");
    ck(taken(BPF_JMP_JSET, 1, 1, 0x06, 0x04, 0, 3) == 1, "jset32 X overlap");
}

static void test_jmp32_signed(void)
{
    /* 32-bit signed: 0xFFFFFFFF as i32 = -1 */
    ck(taken(BPF_JMP_JSLT, 1, 1, 0xFFFFFFFFUL, 1, 0, 3) == 1, "jslt32 -1 < 1");
    ck(taken(BPF_JMP_JSGT, 1, 1, 0xFFFFFFFFUL, 1, 0, 3) == 0,
       "jsgt32 -1 > 1 false");
    ck(taken(BPF_JMP_JGT, 1, 1, 0xFFFFFFFFUL, 1, 0, 3) == 1,
       "jgt32 unsigned 0xFFFFFFFF > 1");
}

static void test_jmp32_true_branches(void)
{
    ck(taken(BPF_JMP_JNE, 1, 1, 5, 6, 0, 3) == 1, "jne32 X diff true");
    ck(taken(BPF_JMP_JGE, 1, 1, 5, 3, 0, 3) == 1, "jge32 5>=3 true");
    ck(taken(BPF_JMP_JLT, 1, 1, 3, 7, 0, 3) == 1, "jlt32 3<7 true");
    ck(taken(BPF_JMP_JLE, 1, 1, 5, 5, 0, 3) == 1, "jle32 5<=5 true");
    ck(taken(BPF_JMP_JSGE, 1, 1, 0xFFFFFFFFUL, 0xFFFFFFFEUL, 0, 3) == 1,
       "jsge32 -1>=-2 true");
    ck(taken(BPF_JMP_JSLE, 1, 1, 0xFFFFFFFFUL, 1, 0, 3) == 1,
       "jsle32 -1<=1 true");
}

int main(void)
{
    test_eq_ne();
    test_unsigned_gt_ge_lt_le();
    test_signed_gt_ge_lt_le();
    test_jset();
    test_jmp32_signed();
    test_jmp32_true_branches();
    if (fails)
    {
        (void)fprintf(stderr, "test_jump: %d failures\n", fails);
        return 1;
    }
    (void)printf("ok: test_jump\n");
    return 0;
}
