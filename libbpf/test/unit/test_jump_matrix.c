#include <stdio.h>

#include "eval.h"

static int fails;
static int count;

typedef struct
{
    bpf_byte code;
    int is32;
    bpf_byte source;
    bpf_u64 a;
    bpf_u64 b;
    int imm;
    int want;
} jmp_case;

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

static void run_case(const jmp_case *c)
{
    bpf_insn prog[2];
    bpf_machine m;
    bpf_status st;
    bpf_byte op;
    int got;

    op = (bpf_byte)(((unsigned)c->code << 4) |
                    (((unsigned)c->source & 1u) << 3) |
                    (c->is32 ? BPF_CLS_JMP32 : BPF_CLS_JMP));
    prog[0] = mk(op, c->source ? (bpf_byte)9 : 0, 1, c->imm, 3);
    prog[1] = mk(0x95, 0, 0, 0, 0);
    bpf_machine_init(&m, prog, 2);
    m.regs.r[1] = c->a;
    m.regs.r[9] = c->b;
    st = bpf_step(&m);
    count = count + 1;
    if (st != BPF_STAT_RUNNING)
    {
        got = -1;
    }
    else if (m.pc == 1)
    {
        got = 0;
    }
    else if ((int)m.pc == 4)
    {
        got = 1;
    }
    else
    {
        got = -1;
    }
    if (got != c->want)
    {
        (void)fprintf(stderr,
                      "FAIL: code=%u is32=%d src=%d a=%lx b=%lx"
                      " imm=%d got=%d want=%d\n",
                      c->code, c->is32, c->source, c->a, c->b, c->imm, got,
                      c->want);
        fails = fails + 1;
    }
}

#define K 0
#define X 1
#define BIG 0xFFFFFFFFFFFFFFFFUL
#define NEG 0xFFFFFFFFFFFFFFFFUL

static const jmp_case CASES[] = {
    /* JEQ */
    {BPF_JMP_JEQ, 0, K, 5, 0, 5, 1},
    {BPF_JMP_JEQ, 0, K, 5, 0, 6, 0},
    {BPF_JMP_JEQ, 0, X, 5, 5, 0, 1},
    {BPF_JMP_JEQ, 0, X, 5, 6, 0, 0},
    {BPF_JMP_JEQ, 1, X, 0x0000000100000002UL, 2, 0, 1},
    {BPF_JMP_JEQ, 1, X, 0x0000000100000003UL, 2, 0, 0},
    /* JNE */
    {BPF_JMP_JNE, 0, K, 5, 0, 5, 0},
    {BPF_JMP_JNE, 0, K, 5, 0, 6, 1},
    {BPF_JMP_JNE, 0, X, 5, 5, 0, 0},
    {BPF_JMP_JNE, 1, X, 3, 3, 0, 0},
    /* JGT unsigned */
    {BPF_JMP_JGT, 0, K, 10, 0, 5, 1},
    {BPF_JMP_JGT, 0, K, 5, 0, 10, 0},
    {BPF_JMP_JGT, 0, K, 5, 0, 5, 0},
    {BPF_JMP_JGT, 0, X, BIG, 1, 0, 1},
    {BPF_JMP_JGT, 0, X, 1, BIG, 0, 0},
    {BPF_JMP_JGT, 1, X, 0xFFFFFFFFUL, 1, 0, 1},
    /* JGE */
    {BPF_JMP_JGE, 0, X, 5, 5, 0, 1},
    {BPF_JMP_JGE, 0, X, 5, 6, 0, 0},
    {BPF_JMP_JGE, 0, X, 6, 5, 0, 1},
    /* JLT */
    {BPF_JMP_JLT, 0, K, 3, 0, 7, 1},
    {BPF_JMP_JLT, 0, K, 7, 0, 7, 0},
    {BPF_JMP_JLT, 0, X, BIG - 1, BIG, 0, 1},
    /* JLE */
    {BPF_JMP_JLE, 0, X, 5, 5, 0, 1},
    {BPF_JMP_JLE, 0, X, 6, 5, 0, 0},
    {BPF_JMP_JLE, 0, X, 5, 6, 0, 1},
    /* JSGT signed */
    {BPF_JMP_JSGT, 0, X, NEG, 1, 0, 0},
    {BPF_JMP_JSGT, 0, X, NEG, NEG - 1, 0, 1},
    {BPF_JMP_JSGT, 0, X, 1, NEG, 0, 1},
    {BPF_JMP_JSGT, 0, X, 1, 2, 0, 0},
    {BPF_JMP_JSGT, 1, X, 0xFFFFFFFFUL, 1, 0, 0},
    {BPF_JMP_JSGT, 1, X, 1, 0xFFFFFFFFUL, 0, 1},
    /* JSGE */
    {BPF_JMP_JSGE, 0, X, NEG, NEG, 0, 1},
    {BPF_JMP_JSGE, 0, X, NEG, NEG - 1, 0, 1},
    {BPF_JMP_JSGE, 0, X, NEG - 1, NEG, 0, 0},
    /* JSLT */
    {BPF_JMP_JSLT, 0, X, NEG, 1, 0, 1},
    {BPF_JMP_JSLT, 0, X, 1, NEG, 0, 0},
    {BPF_JMP_JSLT, 0, X, 1, 2, 0, 1},
    {BPF_JMP_JSLT, 1, X, 0xFFFFFFFFUL, 1, 0, 1},
    {BPF_JMP_JSLT, 1, X, 1, 0xFFFFFFFFUL, 0, 0},
    /* JSLE */
    {BPF_JMP_JSLE, 0, X, NEG, NEG, 0, 1},
    {BPF_JMP_JSLE, 0, X, NEG, NEG - 1, 0, 0},
    {BPF_JMP_JSLE, 0, X, NEG - 1, NEG, 0, 1},
    {BPF_JMP_JSLE, 0, X, 1, 2, 0, 1},
    {BPF_JMP_JSLE, 1, X, 1, 0xFFFFFFFFUL, 0, 0},
    /* JSET */
    {BPF_JMP_JSET, 0, K, 0x06, 0, 0x04, 1},
    {BPF_JMP_JSET, 0, K, 0x06, 0, 0x08, 0},
    {BPF_JMP_JSET, 0, X, 0x06, 0x04, 0, 1},
    {BPF_JMP_JSET, 0, X, 0x06, 0x08, 0, 0},
    {BPF_JMP_JSET, 1, X, 0x06, 0x04, 0, 1},
};

int main(void)
{
    unsigned i;

    for (i = 0; i < sizeof(CASES) / sizeof(CASES[0]); ++i)
    {
        run_case(&CASES[i]);
    }
    if (fails)
    {
        (void)fprintf(stderr, "test_jump_matrix: %d/%d failed\n", fails, count);
        return 1;
    }
    (void)printf("ok: test_jump_matrix (%d cases)\n", count);
    return 0;
}
