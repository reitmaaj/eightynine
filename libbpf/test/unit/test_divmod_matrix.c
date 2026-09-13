#include <stdio.h>

#include "eval.h"

static int fails;
static int count;

typedef struct
{
    bpf_byte code;
    bpf_byte cls;
    int off;
    bpf_u64 a;
    bpf_u64 b;
    int imm;
    bpf_u64 exp;
} dm_case;

static void run_case(const dm_case *c)
{
    bpf_insn in;
    bpf_regs r;
    bpf_err e;

    in.opcode =
        (bpf_byte)(((unsigned)c->code << 4) | (BPF_SRC_K << 3) | c->cls);
    in.class = c->cls;
    in.code = c->code;
    in.source = BPF_SRC_K;
    in.mode = 0;
    in.size = 0;
    in.src_reg = 0;
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
                      "FAIL: code=%u cls=%u off=%d a=%lx imm=%d got=%lx"
                      " want=%lx\n",
                      c->code, c->cls, c->off, c->a, c->imm, r.r[1], c->exp);
        fails = fails + 1;
    }
}

#define A64 BPF_CLS_ALU64
#define A32 BPF_CLS_ALU
#define M 0xFFFFFFFFFFFFFFFFUL
#define I64MIN 0x8000000000000000UL
#define I32MIN 0x80000000UL

static const dm_case CASES[] = {
    /* unsigned DIV */
    {BPF_ALU_DIV, A64, 0, 42, 0, 6, 7},
    {BPF_ALU_DIV, A64, 0, 42, 0, 0, 0},
    {BPF_ALU_DIV, A64, 0, M, 0, 1, M},
    {BPF_ALU_DIV, A64, 0, M, 0, 2, 0x7FFFFFFFFFFFFFFFUL},
    {BPF_ALU_DIV, A64, 0, 0, 0, 7, 0},
    {BPF_ALU_DIV, A32, 0, 0x100000000UL, 0, 2, 0},
    {BPF_ALU_DIV, A32, 0, 0xFFFFFFFFUL, 0, 1, 0xFFFFFFFFUL},
    /* signed DIV */
    {BPF_ALU_DIV, A64, 1, (bpf_u64)(bpf_i64)-42, 0, 6, (bpf_u64)(bpf_i64)-7},
    {BPF_ALU_DIV, A64, 1, (bpf_u64)(bpf_i64)-42, 0, -6, (bpf_u64)(bpf_i64)7},
    {BPF_ALU_DIV, A64, 1, (bpf_u64)(bpf_i64)42, 0, -6, (bpf_u64)(bpf_i64)-7},
    {BPF_ALU_DIV, A64, 1, I64MIN, 0, -1, I64MIN},
    {BPF_ALU_DIV, A64, 1, 42, 0, 0, 0},
    {BPF_ALU_DIV, A64, 1, I64MIN, 0, 2, 0xC000000000000000UL},
    {BPF_ALU_DIV, A32, 1, I32MIN, 0, -1, I32MIN},
    /* unsigned MOD */
    {BPF_ALU_MOD, A64, 0, 17, 0, 5, 2},
    {BPF_ALU_MOD, A64, 0, 17, 0, 0, 17},
    {BPF_ALU_MOD, A64, 0, 20, 0, 5, 0},
    {BPF_ALU_MOD, A32, 0, 0x100000000UL, 0, 3, 0},
    /* signed MOD (truncated) */
    {BPF_ALU_MOD, A64, 1, (bpf_u64)(bpf_i64)-13, 0, 3, (bpf_u64)(bpf_i64)-1},
    {BPF_ALU_MOD, A64, 1, (bpf_u64)(bpf_i64)-13, 0, -3, (bpf_u64)(bpf_i64)-1},
    {BPF_ALU_MOD, A64, 1, (bpf_u64)(bpf_i64)13, 0, -3, (bpf_u64)(bpf_i64)1},
    {BPF_ALU_MOD, A64, 1, (bpf_u64)(bpf_i64)13, 0, 3, (bpf_u64)(bpf_i64)1},
    {BPF_ALU_MOD, A64, 1, I64MIN, 0, -1, 0},
    {BPF_ALU_MOD, A64, 1, 42, 0, 0, 42},
    {BPF_ALU_MOD, A32, 1, (bpf_u64)(bpf_u32)0x80000000u, 0, 2,
     (bpf_u64)(bpf_u32)0u},
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
        (void)fprintf(stderr, "test_divmod_matrix: %d/%d failed\n", fails,
                      count);
        return 1;
    }
    (void)printf("ok: test_divmod_matrix (%d cases)\n", count);
    return 0;
}
