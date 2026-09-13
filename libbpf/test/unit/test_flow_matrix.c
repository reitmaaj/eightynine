#include <stdio.h>

#include "eval.h"

static int fails;
static int count;

typedef struct
{
    bpf_byte op;
    bpf_byte src;
    int imm;
    int off;
    int steps;
    bpf_u32 want_pc;
    bpf_status want_st;
} f_case;

static bpf_insn mk(bpf_byte op, bpf_byte src, bpf_byte dst, int imm, int off)
{
    bpf_insn i;

    i.opcode = op;
    i.class = (bpf_byte)(op & 0x07u);
    i.code = (bpf_byte)(op >> 4);
    i.source = (bpf_byte)((op >> 3) & 0x01u);
    i.mode = (bpf_byte)(op >> 5);
    i.size = (bpf_byte)((op >> 3) & 0x03u);
    i.src_reg = src;
    i.dst_reg = dst;
    i.offset = off;
    i.imm = imm;
    i.is_wide = 0;
    i.next_imm = 0;
    return i;
}

static void run_case(const f_case *c)
{
    bpf_insn prog[16];
    bpf_machine m;
    bpf_status st;
    bpf_u32 pc;
    int i;

    for (i = 0; i < 16; ++i)
    {
        prog[i] = mk(0xb7, 0, (bpf_byte)i, 0, 0);
    }
    prog[0] = mk(c->op, c->src, 0, c->imm, c->off);
    bpf_machine_init(&m, prog, 16);
    st = BPF_STAT_RUNNING;
    pc = 0;
    for (i = 0; i < c->steps && st == BPF_STAT_RUNNING; ++i)
    {
        st = bpf_step(&m);
        pc = m.pc;
    }
    count = count + 1;
    if (st != c->want_st)
    {
        (void)fprintf(stderr, "FAIL: op=%02x st=%d want=%d\n", c->op, (int)st,
                      (int)c->want_st);
        fails = fails + 1;
        return;
    }
    if (c->want_st == BPF_STAT_RUNNING && pc != c->want_pc)
    {
        (void)fprintf(stderr, "FAIL: op=%02x pc=%u want=%u\n", c->op, pc,
                      c->want_pc);
        fails = fails + 1;
    }
}

static const f_case CASES[] = {
    /* JA JMP uses 16-bit offset */
    {0x05, 0, 0, 5, 1, 6, BPF_STAT_RUNNING},
    {0x05, 0, 0, -1, 1, 0, BPF_STAT_RUNNING},
    {0x05, 0, 0, 3, 1, 4, BPF_STAT_RUNNING},
    /* JA JMP32 uses 32-bit imm */
    {0x06, 0, 7, 0, 1, 8, BPF_STAT_RUNNING},
    {0x06, 0, -1, 0, 1, 0, BPF_STAT_RUNNING},
    /* EXIT at top level returns */
    {0x95, 0, 0, 0, 1, 0, BPF_STAT_RETURNED},
    /* CALL helper src0/2 -> HOSTCALL */
    {0x85, 0, 7, 0, 1, 1, BPF_STAT_HOSTCALL},
    {0x85, 2, 99, 0, 1, 1, BPF_STAT_HOSTCALL},
    /* CALL local src1 -> jump */
    {0x85, 1, 5, 0, 1, 6, BPF_STAT_RUNNING},
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
        (void)fprintf(stderr, "test_flow_matrix: %d/%d failed\n", fails, count);
        return 1;
    }
    (void)printf("ok: test_flow_matrix (%d cases)\n", count);
    return 0;
}
