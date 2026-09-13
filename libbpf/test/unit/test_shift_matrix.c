#include <stdio.h>

#include "eval.h"

static int fails;
static int count;

typedef struct
{
    bpf_byte code;
    bpf_byte cls;
    int imm;
    bpf_u64 a;
    bpf_u64 out;
} sh_case;

static void run_case(const sh_case *c)
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
    in.offset = 0;
    in.imm = c->imm;
    in.is_wide = 0;
    in.next_imm = 0;
    r.r[0] = 0;
    r.r[1] = c->a;
    r.r[9] = 0;
    e = bpf_alu(&r, &in);
    count = count + 1;
    if (e != BPF_OK || r.r[1] != c->out)
    {
        (void)fprintf(stderr,
                      "FAIL: code=%u cls=%u imm=%d a=%lx got=%lx"
                      " want=%lx\n",
                      c->code, c->cls, c->imm, c->a, r.r[1], c->out);
        fails = fails + 1;
    }
}

#define A64 BPF_CLS_ALU64
#define A32 BPF_CLS_ALU
#define M 0xFFFFFFFFFFFFFFFFUL

static const sh_case CASES[] = {
    /* LSH64 */
    {BPF_ALU_LSH, A64, 0, 1, 1},
    {BPF_ALU_LSH, A64, 1, 1, 2},
    {BPF_ALU_LSH, A64, 31, 1, 0x80000000UL},
    {BPF_ALU_LSH, A64, 32, 1, 0x100000000UL},
    {BPF_ALU_LSH, A64, 63, 1, 0x8000000000000000UL},
    {BPF_ALU_LSH, A64, 64, 1, 1},
    {BPF_ALU_LSH, A64, 65, 1, 2},
    {BPF_ALU_LSH, A64, 70, 1, 64},
    {BPF_ALU_LSH, A64, 127, 1, 0x8000000000000000UL},
    {BPF_ALU_LSH, A64, -1, 1, 0x8000000000000000UL}, /* 0xFF... -> 63 */
    {BPF_ALU_LSH, A64, 0, M, M},
    /* RSH64 */
    {BPF_ALU_RSH, A64, 0, 0x10, 0x10},
    {BPF_ALU_RSH, A64, 1, 0x10, 0x08},
    {BPF_ALU_RSH, A64, 4, 0x10, 0x01},
    {BPF_ALU_RSH, A64, 63, 0x8000000000000000UL, 1},
    {BPF_ALU_RSH, A64, 64, 0x8000000000000000UL, 0x8000000000000000UL},
    {BPF_ALU_RSH, A64, 70, 0x4000, 0x100},
    {BPF_ALU_RSH, A64, 0, M, M},
    /* ARSH64 */
    {BPF_ALU_ARSH, A64, 1, 0xFFFFFFFFFFFFFF80UL, 0xFFFFFFFFFFFFFFC0UL},
    {BPF_ALU_ARSH, A64, 4, 0x10, 1},
    {BPF_ALU_ARSH, A64, 63, 0x8000000000000000UL, M},
    {BPF_ALU_ARSH, A64, 0, 0x80, 0x80},
    {BPF_ALU_ARSH, A64, 1, 0x7F, 0x3F},
    {BPF_ALU_ARSH, A64, 8, 0xFFFFFFFFFFFFFFFFUL, 0xFFFFFFFFFFFFFFFFUL},
    /* LSH32 */
    {BPF_ALU_LSH, A32, 0, 1, 1},
    {BPF_ALU_LSH, A32, 31, 1, 0x80000000UL},
    {BPF_ALU_LSH, A32, 32, 1, 1},
    {BPF_ALU_LSH, A32, 34, 1, 4},
    {BPF_ALU_LSH, A32, 63, 1, 0x80000000UL},
    {BPF_ALU_LSH, A32, 70, 1, 0x40},
    /* RSH32 */
    {BPF_ALU_RSH, A32, 4, 0x100, 0x10},
    {BPF_ALU_RSH, A32, 31, 0x80000000UL, 1},
    {BPF_ALU_RSH, A32, 32, 0x80000000UL, 0x80000000UL},
    {BPF_ALU_RSH, A32, 34, 0x100, 0x40},
    /* ARSH32 */
    {BPF_ALU_ARSH, A32, 4, 0xFFFFFFFFUL, 0xFFFFFFFFUL},
    {BPF_ALU_ARSH, A32, 4, 0x10, 1},
    {BPF_ALU_ARSH, A32, 31, 0x80000000UL, 0xFFFFFFFFUL},
    {BPF_ALU_ARSH, A32, 1, 0x80000000UL, 0xC0000000UL},
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
        (void)fprintf(stderr, "test_shift_matrix: %d/%d failed\n", fails,
                      count);
        return 1;
    }
    (void)printf("ok: test_shift_matrix (%d cases)\n", count);
    return 0;
}
