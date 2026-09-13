#include <stdio.h>

#include "eval.h"

static int fails;

static void ck(int cond, const char *msg)
{
    if (!cond)
    {
        (void)fprintf(stderr, "FAIL: %s\n", msg);
        fails = fails + 1;
    }
}

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

static bpf_status run(const bpf_insn *prog, bpf_u32 n, bpf_byte *mem,
                      bpf_u64 msz, bpf_u64 *r0)
{
    bpf_machine m;
    bpf_status st;

    bpf_machine_init(&m, prog, n);
    m.mem = mem;
    m.mem_size = msz;
    st = BPF_STAT_RUNNING;
    while (st == BPF_STAT_RUNNING)
    {
        st = bpf_step(&m);
    }
    *r0 = m.regs.r[0];
    return st;
}

/* Store 0x11223344 as W, load it back, swap bytes via END, exit with r0. */
static void test_endian_memory(void)
{
    bpf_insn prog[8];
    bpf_byte mem[16];
    bpf_u64 r0;
    bpf_status st;
    int i;

    for (i = 0; i < 16; ++i)
    {
        mem[i] = 0;
    }
    prog[0] = mk(0xb7, 0, 1, 0, 0);          /* r1 = 0 */
    prog[1] = mk(0xb7, 0, 2, 0x11223344, 0); /* r2 = value */
    prog[2] = mk(0x63, 2, 1, 0, 0);          /* STX W mem[0] = r2 */
    prog[3] = mk(0x61, 1, 0, 0, 0);          /* LDX W r0 = mem[0] */
    prog[4] = mk(0xdc, 0, 0, 32, 0);         /* END32 -> byte swap */
    prog[5] = mk(0x95, 0, 0, 0, 0);          /* exit */
    st = run(prog, 6, mem, 16, &r0);
    ck(st == BPF_STAT_RETURNED && r0 == 0x44332211, "endian memory swap");
}

/* Atomic counter with fetch, accumulate into r0. */
static void test_atomic_accumulate(void)
{
    bpf_insn prog[6];
    bpf_byte mem[16];
    bpf_u64 r0;
    bpf_status st;
    int i;

    for (i = 0; i < 16; ++i)
    {
        mem[i] = 0;
    }
    prog[0] = mk(0xb7, 0, 1, 0, 0);    /* r1 = 0 (addr) */
    prog[1] = mk(0xb7, 0, 0, 0, 0);    /* r0 = 0 */
    prog[2] = mk(0xb7, 0, 2, 5, 0);    /* r2 = 5 */
    prog[3] = mk(0xC3, 2, 1, 0x01, 0); /* ATOMIC ADD W + FETCH into r2 */
    prog[4] = mk(0x0f, 2, 0, 0, 0);    /* r0 += r2 (prior mem value) */
    prog[5] = mk(0x95, 0, 0, 0, 0);    /* exit */
    st = run(prog, 6, mem, 16, &r0);
    ck(st == BPF_STAT_RETURNED && r0 == 0, "atomic fetch prior is 0");
}

/* Copy 4 bytes from mem[0..3] to mem[8..11] byte by byte. */
static void test_mem_copy(void)
{
    bpf_insn prog[12];
    bpf_byte mem[16];
    bpf_u64 r0;
    bpf_status st;
    int i;

    for (i = 0; i < 16; ++i)
    {
        mem[i] = 0;
    }
    mem[0] = 0xAA;
    mem[1] = 0xBB;
    prog[0] = mk(0xb7, 0, 1, 0, 0); /* r1 = 0 (src) */
    prog[1] = mk(0xb7, 0, 2, 8, 0); /* r2 = 8 (dst) */
    prog[2] = mk(0x71, 1, 3, 0, 0); /* r3 = mem[r1] */
    prog[3] = mk(0x73, 3, 2, 0, 0); /* mem[r2] = r3 */
    prog[4] = mk(0x07, 0, 1, 1, 0); /* r1 += 1 */
    prog[5] = mk(0x07, 0, 2, 1, 0); /* r2 += 1 */
    prog[6] = mk(0x71, 1, 3, 0, 0); /* r3 = mem[r1] */
    prog[7] = mk(0x73, 3, 2, 0, 0); /* mem[r2] = r3 */
    prog[8] = mk(0x95, 0, 0, 0, 0); /* exit */
    st = run(prog, 9, mem, 16, &r0);
    ck(st == BPF_STAT_RETURNED && mem[8] == 0xAA && mem[9] == 0xBB, "mem copy");
}

/* Compute r0 = (mem[0] << 8) | mem[1] from two bytes. */
static void test_load_compose(void)
{
    bpf_insn prog[8];
    bpf_byte mem[16];
    bpf_u64 r0;
    bpf_status st;
    int i;

    for (i = 0; i < 16; ++i)
    {
        mem[i] = 0;
    }
    mem[0] = 0x12;
    mem[1] = 0x34;
    prog[0] = mk(0xb7, 0, 1, 0, 0); /* r1 = 0 */
    prog[1] = mk(0x71, 1, 2, 0, 0); /* r2 = mem[0] */
    prog[2] = mk(0x67, 0, 2, 8, 0); /* r2 <<= 8 */
    prog[3] = mk(0x71, 1, 3, 0, 1); /* r3 = mem[1] */
    prog[4] = mk(0x4f, 3, 2, 0, 0); /* r2 |= r3 */
    prog[5] = mk(0xbf, 2, 0, 0, 0); /* r0 = r2 */
    prog[6] = mk(0x95, 0, 0, 0, 0); /* exit */
    st = run(prog, 7, mem, 16, &r0);
    ck(st == BPF_STAT_RETURNED && r0 == 0x1234, "load compose 0x1234");
}

int main(void)
{
    test_endian_memory();
    test_atomic_accumulate();
    test_mem_copy();
    test_load_compose();
    if (fails)
    {
        (void)fprintf(stderr, "test_program4: %d failures\n", fails);
        return 1;
    }
    (void)printf("ok: test_program4\n");
    return 0;
}
