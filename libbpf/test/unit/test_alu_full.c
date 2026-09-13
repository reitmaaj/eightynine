#include <stdio.h>

#include "eval.h"

static int fails;
static int count;

typedef struct
{
    bpf_byte code;
    bpf_byte cls;
    bpf_byte src;
    bpf_u64 a;
    bpf_u64 b;
    int imm;
    int off;
    bpf_u64 out;
} full_case;

static void run_case(const full_case *c)
{
    bpf_insn in;
    bpf_regs r;
    bpf_err e;

    in.opcode = (bpf_byte)(((unsigned)c->code << 4) |
                           (((unsigned)c->src & 1u) << 3) | c->cls);
    in.class = c->cls;
    in.code = c->code;
    in.source = (bpf_byte)((unsigned)c->src & 1u);
    in.mode = 0;
    in.size = 0;
    in.src_reg = ((unsigned)c->src & 1u) ? (bpf_byte)9 : 0;
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
    if (e != BPF_OK || r.r[1] != c->out)
    {
        (void)fprintf(stderr,
                      "FAIL: code=%u cls=%u src=%u a=%lx b=%lx"
                      " imm=%d off=%d got=%lx want=%lx\n",
                      c->code, c->cls, c->src, c->a, c->b, c->imm, c->off,
                      r.r[1], c->out);
        fails = fails + 1;
    }
}

#define K BPF_SRC_K
#define X BPF_SRC_X
#define A64 BPF_CLS_ALU64
#define A32 BPF_CLS_ALU
#define M 0xFFFFFFFFFFFFFFFFUL
#define HI 0xFFFFFFFF00000000UL

static const full_case CASES[] = {
    /* ADD */
    {BPF_ALU_ADD, A64, K, 1, 0, 2, 0, 3},
    {BPF_ALU_ADD, A64, K, 0, 0, 0, 0, 0},
    {BPF_ALU_ADD, A64, K, M, 0, 1, 0, 0},
    {BPF_ALU_ADD, A64, X, 0x0F, 0xF0, 0, 0, 0xFF},
    {BPF_ALU_ADD, A64, X, HI, 1, 0, 0, 0xFFFFFFFF00000001UL},
    {BPF_ALU_ADD, A32, K, HI, 0, 5, 0, 5},
    {BPF_ALU_ADD, A32, X, 0xFFFFFFFFUL, 1, 0, 0, 0},
    /* SUB */
    {BPF_ALU_SUB, A64, K, 10, 0, 3, 0, 7},
    {BPF_ALU_SUB, A64, K, 0, 0, 1, 0, M},
    {BPF_ALU_SUB, A64, X, 5, 5, 0, 0, 0},
    {BPF_ALU_SUB, A64, X, 5, 10, 0, 0, M - 4},
    {BPF_ALU_SUB, A32, K, 0, 0, 1, 0, 0xFFFFFFFFUL},
    {BPF_ALU_SUB, A32, X, 0, 0xFFFFFFFFUL, 0, 0, 1},
    /* MUL */
    {BPF_ALU_MUL, A64, K, 7, 0, 6, 0, 42},
    {BPF_ALU_MUL, A64, K, 0, 0, 9, 0, 0},
    {BPF_ALU_MUL, A64, K, M, 0, 1, 0, M},
    {BPF_ALU_MUL, A64, X, 3, 4, 0, 0, 12},
    {BPF_ALU_MUL, A64, X, 0x100000000UL, 0x100000000UL, 0, 0, 0},
    {BPF_ALU_MUL, A32, K, 0x10000UL, 0, 0x10000UL, 0, 0},
    /* OR */
    {BPF_ALU_OR, A64, K, 0, 0, 0xFF, 0, 0xFF},
    {BPF_ALU_OR, A64, K, 0xFF00, 0, 0x00FF, 0, 0xFFFF},
    {BPF_ALU_OR, A64, X, 0x0F0F, 0x00F0, 0, 0, 0x0FFF},
    {BPF_ALU_OR, A32, K, HI, 0, -1, 0, 0xFFFFFFFFUL},
    /* AND */
    {BPF_ALU_AND, A64, K, M, 0, 0x1234, 0, 0x1234},
    {BPF_ALU_AND, A64, K, 0x1234, 0, 0xFFFF, 0, 0x1234},
    {BPF_ALU_AND, A64, X, 0xFF0F, 0x0FF0, 0, 0, 0x0F00},
    {BPF_ALU_AND, A32, K, 0xFFFFFFFFUL, 0, -1, 0, 0xFFFFFFFFUL},
    /* XOR */
    {BPF_ALU_XOR, A64, K, 0x0F0F, 0, 0x00FF, 0, 0x0FF0},
    {BPF_ALU_XOR, A64, K, 0, 0, 0, 0, 0},
    {BPF_ALU_XOR, A64, X, M, M, 0, 0, 0},
    {BPF_ALU_XOR, A64, X, M, 0, 0, 0, M},
    {BPF_ALU_XOR, A32, K, 0xFFFFFFFFUL, 0, -1, 0, 0},
    /* NEG */
    {BPF_ALU_NEG, A64, K, 1, 0, 0, 0, M},
    {BPF_ALU_NEG, A64, K, M, 0, 0, 0, 1},
    {BPF_ALU_NEG, A64, K, 0x8000000000000000UL, 0, 0, 0, 0x8000000000000000UL},
    {BPF_ALU_NEG, A32, K, 0xFFFFFFFFUL, 0, 0, 0, 1},
    {BPF_ALU_NEG, A32, K, 0x80000000UL, 0, 0, 0, 0x80000000UL},
    /* MOV */
    {BPF_ALU_MOV, A64, K, 99, 0, -1, 0, M},
    {BPF_ALU_MOV, A64, K, 99, 0, 0x7FFFFFFF, 0, 0x7FFFFFFFUL},
    {BPF_ALU_MOV, A64, X, 0, M, 0, 0, M},
    {BPF_ALU_MOV, A64, X, 0, 0x100000001UL, 0, 0, 0x100000001UL},
    {BPF_ALU_MOV, A32, K, M, 0, 0x12345678, 0, 0x12345678UL},
    {BPF_ALU_MOV, A32, X, M, 0x00000000FFFFFFFFUL, 0, 0, 0xFFFFFFFFUL},
    /* DIV / MOD via off */
    {BPF_ALU_DIV, A64, K, 42, 0, 6, 0, 7},
    {BPF_ALU_DIV, A64, K, 42, 0, 0, 0, 0},
    {BPF_ALU_MOD, A64, K, 17, 0, 5, 0, 2},
    {BPF_ALU_MOD, A64, K, 17, 0, 0, 0, 17},
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
        (void)fprintf(stderr, "test_alu_full: %d/%d failed\n", fails, count);
        return 1;
    }
    (void)printf("ok: test_alu_full (%d cases)\n", count);
    return 0;
}
