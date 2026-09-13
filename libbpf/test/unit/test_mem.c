#include <stdio.h>

#include "eval.h"

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

static int test_ld_imm_wide(void)
{
    bpf_insn prog[1];
    bpf_machine m;
    bpf_status st;
    bpf_u64 want;

    prog[0] = mk(0x18, 0, 1, 0x11111111, 0);
    prog[0].is_wide = 1;
    prog[0].next_imm = 0x22222222u;
    bpf_machine_init(&m, prog, 1);
    st = bpf_step(&m);
    want = (0x22222222UL << 32) | 0x11111111UL;
    if (st != BPF_STAT_RUNNING)
    {
        (void)fprintf(stderr, "FAIL: ldwide status\n");
        return 1;
    }
    if (m.regs.r[1] != want)
    {
        (void)fprintf(stderr, "FAIL: ldwide value %lx\n", m.regs.r[1]);
        return 1;
    }
    return 0;
}

static int test_ldx_load(void)
{
    bpf_byte mem[16];
    bpf_insn prog[1];
    bpf_machine m;
    bpf_status st;
    int i;

    for (i = 0; i < 16; ++i)
    {
        mem[i] = (bpf_byte)i;
    }
    prog[0] = mk(0x61, 1, 2, 0, 0); /* LDX W: mode MEM(3)<<5, W(0), LDX(1) */
    bpf_machine_init(&m, prog, 1);
    m.mem = mem;
    m.mem_size = 16;
    m.regs.r[1] = 4UL;
    st = bpf_step(&m);
    if (st != BPF_STAT_RUNNING)
    {
        (void)fprintf(stderr, "FAIL: ldx status %d\n", (int)st);
        return 1;
    }
    if (m.regs.r[2] != 0x07060504UL)
    {
        (void)fprintf(stderr, "FAIL: ldx W value %lx\n", m.regs.r[2]);
        return 1;
    }
    return 0;
}

static int test_store(void)
{
    bpf_byte mem[16];
    bpf_insn prog[1];
    bpf_machine m;
    bpf_status st;
    int i;

    for (i = 0; i < 16; ++i)
    {
        mem[i] = 0;
    }
    prog[0] = mk(0x63, 2, 1, 0, 0); /* STX W: mode MEM, W, STX */
    bpf_machine_init(&m, prog, 1);
    m.mem = mem;
    m.mem_size = 16;
    m.regs.r[1] = 8UL;
    m.regs.r[2] = 0xAABBCCDDUL;
    st = bpf_step(&m);
    if (st != BPF_STAT_RUNNING)
    {
        (void)fprintf(stderr, "FAIL: stx status\n");
        return 1;
    }
    if (mem[8] != 0xDD || mem[9] != 0xCC || mem[10] != 0xBB || mem[11] != 0xAA)
    {
        (void)fprintf(stderr, "FAIL: stx bytes\n");
        return 1;
    }
    return 0;
}

static int test_memsx(void)
{
    bpf_byte mem[16];
    bpf_insn prog[1];
    bpf_machine m;
    bpf_status st;
    int i;

    for (i = 0; i < 16; ++i)
    {
        mem[i] = 0;
    }
    mem[0] = 0x80;
    prog[0] = mk(0x91, 1, 2, 0, 0); /* LDX MEMSX B: mode(4)<<5, B(2), LDX */
    bpf_machine_init(&m, prog, 1);
    m.mem = mem;
    m.mem_size = 16;
    m.regs.r[1] = 0UL;
    st = bpf_step(&m);
    if (st != BPF_STAT_RUNNING)
    {
        (void)fprintf(stderr, "FAIL: memsx status\n");
        return 1;
    }
    if ((bpf_i64)m.regs.r[2] != -128)
    {
        (void)fprintf(stderr, "FAIL: memsx sign extend\n");
        return 1;
    }
    return 0;
}

static int test_oob(void)
{
    bpf_byte mem[8];
    bpf_insn prog[1];
    bpf_machine m;
    bpf_status st;
    int i;

    for (i = 0; i < 8; ++i)
    {
        mem[i] = 0;
    }
    prog[0] = mk(0x61, 1, 2, 0, 0); /* LDX W */
    bpf_machine_init(&m, prog, 1);
    m.mem = mem;
    m.mem_size = 8;
    m.regs.r[1] = 8UL;
    st = bpf_step(&m);
    if (st != BPF_STAT_TRAP)
    {
        (void)fprintf(stderr, "FAIL: oob expected TRAP got %d\n", (int)st);
        return 1;
    }

    prog[0].offset = -4;
    bpf_machine_init(&m, prog, 1);
    m.mem = mem;
    m.mem_size = 8;
    m.regs.r[1] = 0UL;
    st = bpf_step(&m);
    if (st != BPF_STAT_TRAP)
    {
        (void)fprintf(stderr, "FAIL: negative expected TRAP got %d\n", (int)st);
        return 1;
    }
    return 0;
}

int main(void)
{
    int fail;

    fail = 0;
    fail |= test_ld_imm_wide();
    fail |= test_ldx_load();
    fail |= test_store();
    fail |= test_memsx();
    fail |= test_oob();
    if (fail)
    {
        (void)fprintf(stderr, "test_mem: FAILED\n");
        return 1;
    }
    (void)printf("ok: test_mem\n");
    return 0;
}
