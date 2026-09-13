#include <stdio.h>

#include "eval.h"

static int fails;
static int count;

typedef struct
{
    bpf_byte code;
    bpf_byte cls;
    bpf_byte source;
    bpf_u64 a;
    bpf_u64 b;
    int imm;
    int off;
    bpf_u64 exp;
} alu_case;

static void run_case(const alu_case *c)
{
    bpf_insn in;
    bpf_regs r;
    bpf_err e;

    in.opcode = (bpf_byte)(((unsigned)c->code << 4) |
                           (((unsigned)c->source & 1u) << 3) | c->cls);
    in.class = c->cls;
    in.code = c->code;
    in.source = (bpf_byte)((unsigned)c->source & 1u);
    in.mode = 0;
    in.size = 0;
    in.src_reg = c->source ? (bpf_byte)9 : 0;
    in.dst_reg = 1;
    in.offset = c->off;
    in.imm = c->imm;
    in.is_wide = 0;
    in.next_imm = 0;
    r.r[0] = 0;
    r.r[1] = c->a;
    r.r[9] = c->b;
    e = bpf_alu(&r, &in);
    count = count + 1;
    if (e != BPF_OK || r.r[1] != c->exp)
    {
        (void)fprintf(stderr,
                      "FAIL: code=%u cls=%u src=%d a=%lx b=%lx imm=%d"
                      " off=%d got=%lx want=%lx\n",
                      c->code, c->cls, c->source, c->a, c->b, c->imm, c->off,
                      r.r[1], c->exp);
        fails = fails + 1;
    }
}

#define K 0
#define X 1
#define A64 BPF_CLS_ALU64
#define A32 BPF_CLS_ALU
#define M 0xFFFFFFFFFFFFFFFFUL

static const alu_case CASES[] = {
    /* ADD */
    {BPF_ALU_ADD, A64, K, 0, 0, 1, 0, 1},
    {BPF_ALU_ADD, A64, K, M, 0, 1, 0, 0},
    {BPF_ALU_ADD, A64, K, M, 0, -1, 0, M - 1},
    {BPF_ALU_ADD, A64, X, 5, 7, 0, 0, 12},
    {BPF_ALU_ADD, A64, X, M, 1, 0, 0, 0},
    {BPF_ALU_ADD, A64, X, 0x8000000000000000UL, 0x8000000000000000UL, 0, 0, 0},
    {BPF_ALU_ADD, A32, K, 0xFFFFFFFF00000000UL, 0, 1, 0, 1},
    {BPF_ALU_ADD, A32, K, 0xFFFFFFFFUL, 0, 1, 0, 0},
    {BPF_ALU_ADD, A32, X, 0x00000000FFFFFFFFUL, 0x0000000000000001UL, 0, 0, 0},
    /* SUB */
    {BPF_ALU_SUB, A64, K, 10, 0, 3, 0, 7},
    {BPF_ALU_SUB, A64, K, 0, 0, 1, 0, M},
    {BPF_ALU_SUB, A64, X, 10, 20, 0, 0, M - 9},
    {BPF_ALU_SUB, A64, X, M, 1, 0, 0, M - 1},
    {BPF_ALU_SUB, A32, K, 0, 0, 1, 0, 0xFFFFFFFFUL},
    {BPF_ALU_SUB, A32, X, 0, 1, 0, 0, 0xFFFFFFFFUL},
    /* MUL */
    {BPF_ALU_MUL, A64, K, 6, 0, 7, 0, 42},
    {BPF_ALU_MUL, A64, K, 0x100000000UL, 0, 0x10000, 0, 0x1000000000000UL},
    {BPF_ALU_MUL, A64, X, 0x100000000UL, 0x100000000UL, 0, 0, 0},
    {BPF_ALU_MUL, A32, K, 0x10000UL, 0, 0x10000, 0, 0},
    {BPF_ALU_MUL, A32, X, 0x7FFFFFFFUL, 2, 0, 0, 0xFFFFFFFEUL},
    /* OR */
    {BPF_ALU_OR, A64, K, 0x0F00, 0, 0x00F0, 0, 0x0FF0},
    {BPF_ALU_OR, A64, X, 0, M, 0, 0, M},
    {BPF_ALU_OR, A32, K, 0x0000FFFFUL, 0, -65536, 0, 0xFFFFFFFFUL},
    /* AND */
    {BPF_ALU_AND, A64, K, 0xFF0F, 0, 0x0FF0, 0, 0x0F00},
    {BPF_ALU_AND, A64, X, M, 0x1234, 0, 0, 0x1234},
    {BPF_ALU_AND, A32, K, 0xFFFFUL, 0, 0x00FF, 0, 0x00FFUL},
    /* XOR */
    {BPF_ALU_XOR, A64, K, 0x0F0F, 0, 0x00FF, 0, 0x0FF0},
    {BPF_ALU_XOR, A64, X, M, M, 0, 0, 0},
    {BPF_ALU_XOR, A64, X, 0, M, 0, 0, M},
    {BPF_ALU_XOR, A32, K, 0xFFFFFFFFUL, 0, -1, 0, 0},
    /* LSH */
    {BPF_ALU_LSH, A64, K, 1, 0, 0, 0, 1},
    {BPF_ALU_LSH, A64, K, 1, 0, 1, 0, 2},
    {BPF_ALU_LSH, A64, K, 1, 0, 63, 0, 0x8000000000000000UL},
    {BPF_ALU_LSH, A64, K, 1, 0, 64, 0, 1},
    {BPF_ALU_LSH, A64, X, 1, 70, 0, 0, 64},
    {BPF_ALU_LSH, A32, K, 1, 0, 31, 0, 0x80000000UL},
    {BPF_ALU_LSH, A32, X, 1, 34, 0, 0, 4},
    /* RSH */
    {BPF_ALU_RSH, A64, K, 0x10, 0, 4, 0, 1},
    {BPF_ALU_RSH, A64, K, 0x8000000000000000UL, 0, 63, 0, 1},
    {BPF_ALU_RSH, A64, X, 0x4000, 70, 0, 0, 0x100},
    {BPF_ALU_RSH, A32, K, 0x100, 0, 8, 0, 1},
    {BPF_ALU_RSH, A32, X, 0x80000000UL, 31, 0, 0, 1},
    /* ARSH */
    {BPF_ALU_ARSH, A64, K, 0xFFFFFFFFFFFFFF80UL, 0, 4, 0, 0xFFFFFFFFFFFFFFF8UL},
    {BPF_ALU_ARSH, A64, K, 0x10, 0, 4, 0, 1},
    {BPF_ALU_ARSH, A64, X, 0x8000000000000000UL, 70, 0, 0,
     0xFE00000000000000UL},
    {BPF_ALU_ARSH, A32, K, 0xFFFFFFFFUL, 0, 4, 0, 0xFFFFFFFFUL},
    {BPF_ALU_ARSH, A32, X, 0x80000000UL, 1, 0, 0, 0xC0000000UL},
    /* MOV */
    {BPF_ALU_MOV, A64, K, 99, 0, 42, 0, 42},
    {BPF_ALU_MOV, A64, K, 99, 0, -1, 0, M},
    {BPF_ALU_MOV, A64, X, 0, 0x1234, 0, 0, 0x1234},
    {BPF_ALU_MOV, A32, K, M, 0, 5, 0, 5},
    {BPF_ALU_MOV, A32, X, M, 0x12345678UL, 0, 0, 0x12345678UL},
    /* NEG */
    {BPF_ALU_NEG, A64, K, 5, 0, 0, 0, M - 4},
    {BPF_ALU_NEG, A64, K, 0, 0, 0, 0, 0},
    {BPF_ALU_NEG, A64, K, M, 0, 0, 0, 1},
    {BPF_ALU_NEG, A32, K, 0xFFFFFFFFUL, 0, 0, 0, 1},
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
        (void)fprintf(stderr, "test_alu_matrix: %d/%d failed\n", fails, count);
        return 1;
    }
    (void)printf("ok: test_alu_matrix (%d cases)\n", count);
    return 0;
}
