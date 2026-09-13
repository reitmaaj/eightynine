#include <stdio.h>

#include "eval.h"

static int fails;
static int count;

typedef struct
{
    unsigned imm;
    unsigned next;
    bpf_u64 out;
} ld_case;

static void run_case(const ld_case *c)
{
    bpf_insn in;
    bpf_machine m;
    bpf_status st;

    in.opcode = 0x18;
    in.class = BPF_CLS_LD;
    in.mode = BPF_MODE_IMM;
    in.size = BPF_SIZE_DW;
    in.code = 0;
    in.source = 0;
    in.src_reg = 0;
    in.dst_reg = 1;
    in.offset = 0;
    in.imm = (int)c->imm;
    in.is_wide = 1;
    in.next_imm = c->next;
    bpf_machine_init(&m, &in, 1);
    st = bpf_step(&m);
    count = count + 1;
    if (st != BPF_STAT_RUNNING || m.regs.r[1] != c->out)
    {
        (void)fprintf(stderr, "FAIL: imm=%x next=%x got=%lx want=%lx\n",
                      (unsigned)c->imm, c->next, m.regs.r[1], c->out);
        fails = fails + 1;
    }
}

static const ld_case CASES[] = {
    {0x00000000, 0x00000000u, 0x0000000000000000UL},
    {0x00000001, 0x00000000u, 0x0000000000000001UL},
    {0xFFFFFFFF, 0x00000000u, 0x00000000FFFFFFFFUL},
    {0x00000000, 0x00000001u, 0x0000000100000000UL},
    {0x12345678, 0x00000000u, 0x0000000012345678UL},
    {0x00000000, 0xFFFFFFFFu, 0xFFFFFFFF00000000UL},
    {0xFFFFFFFF, 0xFFFFFFFFu, 0xFFFFFFFFFFFFFFFFUL},
    {0x89ABCDEF, 0x01234567u, 0x0123456789ABCDEFUL},
    {0x11223344, 0x55667788u, 0x5566778811223344UL},
    {0xAABBCCDD, 0x12345678u, 0x12345678AABBCCDDUL},
    {0, 0x80000000u, 0x8000000000000000UL},
    {0x7FFFFFFF, 0x7FFFFFFFu, 0x7FFFFFFF7FFFFFFFUL},
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
        (void)fprintf(stderr, "test_ldimm_matrix: %d/%d failed\n", fails,
                      count);
        return 1;
    }
    (void)printf("ok: test_ldimm_matrix (%d cases)\n", count);
    return 0;
}
