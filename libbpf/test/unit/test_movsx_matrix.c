#include <stdio.h>

#include "eval.h"

static int fails;
static int count;

typedef struct
{
    bpf_byte cls;
    int width;
    bpf_u64 in;
    bpf_u64 out;
} mx_case;

static void run_case(const mx_case *c)
{
    bpf_insn in;
    bpf_regs r;
    bpf_err e;

    in.opcode = (bpf_byte)((BPF_ALU_MOV << 4) | (BPF_SRC_X << 3) | c->cls);
    in.class = c->cls;
    in.code = BPF_ALU_MOV;
    in.source = BPF_SRC_X;
    in.mode = 0;
    in.size = 0;
    in.src_reg = 9;
    in.dst_reg = 1;
    in.offset = c->width;
    in.imm = 0;
    in.is_wide = 0;
    in.next_imm = 0;
    r.r[0] = 0;
    r.r[1] = 0;
    r.r[9] = c->in;
    e = bpf_alu(&r, &in);
    count = count + 1;
    if (e != BPF_OK || r.r[1] != c->out)
    {
        (void)fprintf(stderr, "FAIL: cls=%u w=%d in=%lx got=%lx want=%lx\n",
                      c->cls, c->width, c->in, r.r[1], c->out);
        fails = fails + 1;
    }
}

#define A64 BPF_CLS_ALU64
#define A32 BPF_CLS_ALU
#define SGN64 0xFFFFFFFFFFFFFFFFUL

static const mx_case CASES[] = {
    /* MOVSX64 widths 8/16/32 */
    {A64, 8, 0x00, 0x00},
    {A64, 8, 0x7F, 0x7F},
    {A64, 8, 0x80, 0xFFFFFFFFFFFFFF80UL},
    {A64, 8, 0xFF, SGN64},
    {A64, 16, 0x0000, 0x0000},
    {A64, 16, 0x7FFF, 0x7FFF},
    {A64, 16, 0x8000, 0xFFFFFFFFFFFF8000UL},
    {A64, 16, 0xFFFF, SGN64},
    {A64, 32, 0x00000000, 0x00000000},
    {A64, 32, 0x7FFFFFFFUL, 0x7FFFFFFFUL},
    {A64, 32, 0x80000000UL, 0xFFFFFFFF80000000UL},
    {A64, 32, 0xFFFFFFFFUL, SGN64},
    {A64, 8, 0x123456789ABCDE81UL, 0xFFFFFFFFFFFFFF81UL},
    {A64, 16, 0x123456789ABC81FFUL, 0xFFFFFFFFFFFF81FFUL},
    /* MOVSX32 widths 8/16 (zero-extended result) */
    {A32, 8, 0x80, 0xFFFFFF80UL},
    {A32, 8, 0x7F, 0x7F},
    {A32, 8, 0xFF, 0xFFFFFFFFUL},
    {A32, 16, 0x8000, 0xFFFF8000UL},
    {A32, 16, 0x7FFF, 0x7FFF},
    {A32, 16, 0xFFFF, 0xFFFFFFFFUL},
    {A32, 8, 0x12345678, 0x00000078UL},
    {A32, 16, 0x12345678, 0x00005678UL},
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
        (void)fprintf(stderr, "test_movsx_matrix: %d/%d failed\n", fails,
                      count);
        return 1;
    }
    (void)printf("ok: test_movsx_matrix (%d cases)\n", count);
    return 0;
}
