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

/* ATOMIC W STX: mode(6)<<5, W(0), STX(3) = 0xC3. */
#define OP_W 0xC3
/* ATOMIC DW STX: mode(6)<<5, DW(3), STX(3) = 0xDB. */
#define OP_DW 0xDB

static int test_simple_fetch(void)
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
    prog[0] = mk(OP_W, 2, 1, 0x01, 0); /* ATOMIC ADD W + FETCH */
    bpf_machine_init(&m, prog, 1);
    m.mem = mem;
    m.mem_size = 16;
    m.regs.r[1] = 4UL;
    m.regs.r[2] = 10UL;
    st = bpf_step(&m);
    if (st != BPF_STAT_RUNNING)
    {
        (void)fprintf(stderr, "FAIL: add fetch status\n");
        return 1;
    }
    if (mem[4] != 10 || mem[5] != 0 || mem[6] != 0 || mem[7] != 0)
    {
        (void)fprintf(stderr, "FAIL: add memory %d %d %d %d\n", mem[4], mem[5],
                      mem[6], mem[7]);
        return 1;
    }
    if (m.regs.r[2] != 0) /* fetch: prior memory value 0 */
    {
        (void)fprintf(stderr, "FAIL: add fetch prior %lx\n", m.regs.r[2]);
        return 1;
    }
    return 0;
}

static int test_xchg(void)
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
    prog[0] = mk(OP_W, 2, 1, 0xE1, 0); /* XCHG W (0xe0 | FETCH) */
    bpf_machine_init(&m, prog, 1);
    m.mem = mem;
    m.mem_size = 16;
    m.regs.r[1] = 0UL;
    m.regs.r[2] = 0x11223344UL;
    mem[0] = 0x99;
    st = bpf_step(&m);
    if (st != BPF_STAT_RUNNING)
    {
        (void)fprintf(stderr, "FAIL: xchg status\n");
        return 1;
    }
    if (mem[0] != 0x44 || m.regs.r[2] != 0x99)
    {
        (void)fprintf(stderr, "FAIL: xchg result mem=%x r2=%lx\n", mem[0],
                      m.regs.r[2]);
        return 1;
    }
    return 0;
}

static int test_cmpxchg(void)
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
    prog[0] = mk(OP_W, 2, 1, 0xF1, 0); /* CMPXCHG W (0xf0 | FETCH) */
    bpf_machine_init(&m, prog, 1);
    m.mem = mem;
    m.mem_size = 16;
    m.regs.r[1] = 0UL;
    m.regs.r[0] = 5UL; /* expected value */
    m.regs.r[2] = 9UL; /* new value */
    mem[0] = 5;        /* memory == expected -> exchange */
    st = bpf_step(&m);
    if (st != BPF_STAT_RUNNING)
    {
        (void)fprintf(stderr, "FAIL: cmpxchg status\n");
        return 1;
    }
    if (mem[0] != 9 || m.regs.r[0] != 5)
    {
        (void)fprintf(stderr, "FAIL: cmpxchg match mem=%d r0=%lx\n", mem[0],
                      m.regs.r[0]);
        return 1;
    }

    mem[0] = 7;
    m.regs.r[0] = 5UL; /* expected 5, memory 7 -> no exchange */
    m.regs.r[2] = 9UL;
    bpf_machine_init(&m, prog, 1);
    m.mem = mem;
    m.mem_size = 16;
    m.regs.r[1] = 0UL;
    m.regs.r[0] = 5UL;
    m.regs.r[2] = 9UL;
    st = bpf_step(&m);
    if (st != BPF_STAT_RUNNING)
    {
        (void)fprintf(stderr, "FAIL: cmpxchg mismatch status\n");
        return 1;
    }
    if (mem[0] != 7 || m.regs.r[0] != 7)
    {
        (void)fprintf(stderr, "FAIL: cmpxchg mismatch mem=%d r0=%lx\n", mem[0],
                      m.regs.r[0]);
        return 1;
    }
    return 0;
}

static int test_64bit(void)
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
    prog[0] = mk(OP_DW, 2, 1, 0x00, 0); /* ADD DW, no fetch */
    bpf_machine_init(&m, prog, 1);
    m.mem = mem;
    m.mem_size = 16;
    m.regs.r[1] = 0UL;
    m.regs.r[2] = 0x100000000UL;
    mem[0] = 0x01;
    mem[1] = 0x02;
    st = bpf_step(&m);
    if (st != BPF_STAT_RUNNING)
    {
        (void)fprintf(stderr, "FAIL: 64bit status\n");
        return 1;
    }
    /* old = 0x0201, add 0x100000000 => 0x100000201 */
    if (mem[0] != 0x01 || mem[4] != 0x01)
    {
        (void)fprintf(stderr, "FAIL: 64bit add\n");
        return 1;
    }
    return 0;
}

static int test_atomic_oob(void)
{
    bpf_byte mem[8];
    bpf_insn prog[1];
    bpf_machine m;
    bpf_status st;
    int i;

    for (i = 0; i < 8; ++i)
    {
        mem[i] = 0xEE;
    }
    prog[0] = mk(OP_W, 2, 1, 0x00, 0); /* ADD W at r1+0 */
    bpf_machine_init(&m, prog, 1);
    m.mem = mem;
    m.mem_size = 8;
    m.regs.r[1] = 8UL; /* OOB */
    m.regs.r[2] = 1UL;
    st = bpf_step(&m);
    if (st != BPF_STAT_TRAP)
    {
        (void)fprintf(stderr, "FAIL: atomic oob expected TRAP got %d\n",
                      (int)st);
        return 1;
    }
    if (mem[0] != 0xEE)
    {
        (void)fprintf(stderr, "FAIL: atomic oob modified memory\n");
        return 1;
    }
    return 0;
}

int main(void)
{
    int fail;

    fail = 0;
    fail |= test_simple_fetch();
    fail |= test_xchg();
    fail |= test_cmpxchg();
    fail |= test_64bit();
    fail |= test_atomic_oob();
    if (fail)
    {
        (void)fprintf(stderr, "test_atomic: FAILED\n");
        return 1;
    }
    (void)printf("ok: test_atomic\n");
    return 0;
}
