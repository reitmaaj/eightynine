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

static bpf_status run1(bpf_insn *in, bpf_regs *r, bpf_byte *mem, bpf_u64 msz)
{
    bpf_machine m;
    bpf_status st;

    bpf_machine_init(&m, in, 1);
    m.mem = mem;
    m.mem_size = msz;
    m.regs = *r;
    st = bpf_step(&m);
    *r = m.regs;
    return st;
}

static void test_exact_end(void)
{
    bpf_byte mem[16];
    bpf_insn in;
    bpf_regs r;
    bpf_status st;
    int i;

    for (i = 0; i < 16; ++i)
    {
        mem[i] = (bpf_byte)i;
    }
    /* DW load at addr 8 fits exactly (8..15). */
    r.r[0] = 0;
    r.r[1] = 8;
    r.r[2] = 0;
    in = mk(0x79, 1, 2, 0, 0); /* LDX DW */
    st = run1(&in, &r, mem, 16);
    ck(st == BPF_STAT_RUNNING && r.r[2] == 0x0F0E0D0C0B0A0908UL,
       "ldx DW exact end");

    /* One past end traps. */
    r.r[1] = 9;
    r.r[2] = 0;
    st = run1(&in, &r, mem, 16);
    ck(st == BPF_STAT_TRAP, "ldx DW one past end traps");

    /* Store B at the last byte fits. */
    in = mk(0x72, 2, 1, 0xAB, 0); /* ST B imm 0xAB at r1+0 */
    r.r[1] = 15;
    r.r[2] = 0;
    st = run1(&in, &r, mem, 16);
    ck(st == BPF_STAT_RUNNING && mem[15] == 0xAB, "st B last byte");

    /* Store DW at addr 12 traps (12..19 > 16). */
    in = mk(0x7b, 0, 1, 0, 0); /* ST DW imm */
    in.mode = BPF_MODE_MEM;
    r.r[1] = 12;
    st = run1(&in, &r, mem, 16);
    ck(st == BPF_STAT_TRAP, "st DW past end traps");
}

static void test_zero_and_negative_offsets(void)
{
    bpf_byte mem[8];
    bpf_insn in;
    bpf_regs r;
    bpf_status st;
    int i;

    for (i = 0; i < 8; ++i)
    {
        mem[i] = 0;
    }
    mem[2] = 0x42;
    r.r[0] = 0;
    r.r[1] = 4;
    r.r[2] = 0;
    in = mk(0x71, 1, 2, 0, 0); /* LDX B at r1 + 0 */
    st = run1(&in, &r, mem, 8);
    ck(st == BPF_STAT_RUNNING && r.r[2] == mem[4], "ldx B offset 0");

    in = mk(0x71, 1, 2, 0, -2); /* LDX B at r1 - 2 = 2 */
    st = run1(&in, &r, mem, 8);
    ck(st == BPF_STAT_RUNNING && r.r[2] == 0x42, "ldx B negative offset");

    in = mk(0x71, 1, 2, 0, -5); /* r1 - 5 = -1 < 0 */
    st = run1(&in, &r, mem, 8);
    ck(st == BPF_STAT_TRAP, "ldx B negative address traps");
}

static void test_sizes(void)
{
    bpf_byte mem[16];
    bpf_insn in;
    bpf_regs r;
    bpf_status st;
    int i;

    for (i = 0; i < 16; ++i)
    {
        mem[i] = (bpf_byte)(0xA0 + i);
    }
    r.r[0] = 0;
    r.r[1] = 0;
    r.r[2] = 0;
    in = mk(0x71, 1, 2, 0, 0); /* LDX B -> 0xA0 */
    st = run1(&in, &r, mem, 16);
    ck(st == BPF_STAT_RUNNING && r.r[2] == 0xA0, "ldx B");
    in = mk(0x69, 1, 2, 0, 0); /* LDX H -> bytes a0 a1 -> 0xA1A0 */
    st = run1(&in, &r, mem, 16);
    ck(st == BPF_STAT_RUNNING && r.r[2] == 0xA1A0, "ldx H");
    in = mk(0x61, 1, 2, 0, 0); /* LDX W */
    st = run1(&in, &r, mem, 16);
    ck(st == BPF_STAT_RUNNING && r.r[2] == 0xA3A2A1A0, "ldx W");
    in = mk(0x79, 1, 2, 0, 0); /* LDX DW */
    st = run1(&in, &r, mem, 16);
    ck(st == BPF_STAT_RUNNING && r.r[2] == 0xA7A6A5A4A3A2A1A0UL, "ldx DW");
}

static void test_memsx_sizes(void)
{
    bpf_byte mem[16];
    bpf_insn in;
    bpf_regs r;
    bpf_status st;
    int i;

    for (i = 0; i < 16; ++i)
    {
        mem[i] = 0;
    }
    mem[0] = 0x80;
    mem[1] = 0x80;
    mem[2] = 0x80;
    mem[3] = 0x80;
    r.r[0] = 0;
    r.r[1] = 0;
    r.r[2] = 0;
    in = mk(0x91, 1, 2, 0, 0); /* MEMSX B -> -128 */
    st = run1(&in, &r, mem, 16);
    ck(st == BPF_STAT_RUNNING && (bpf_i64)r.r[2] == -128, "memsx B");
    in = mk(0x89, 1, 2, 0, 0); /* MEMSX H -> 0x8080 -> -32512 */
    st = run1(&in, &r, mem, 16);
    ck(st == BPF_STAT_RUNNING && (bpf_i64)r.r[2] == -32640, "memsx H");
    in = mk(0x81, 1, 2, 0, 0); /* MEMSX W -> 0x80808080 -> -2139062144 */
    st = run1(&in, &r, mem, 16);
    ck(st == BPF_STAT_RUNNING && (bpf_i64)r.r[2] == -2139062144, "memsx W");
}

int main(void)
{
    test_exact_end();
    test_zero_and_negative_offsets();
    test_sizes();
    test_memsx_sizes();
    if (fails)
    {
        (void)fprintf(stderr, "test_mem_bound: %d failures\n", fails);
        return 1;
    }
    (void)printf("ok: test_mem_bound\n");
    return 0;
}
