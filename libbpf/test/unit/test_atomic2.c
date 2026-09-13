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

#define OPW 0xC3  /* ATOMIC W STX */
#define OPDW 0xDB /* ATOMIC DW STX */

static void run_atomic(bpf_insn *in, bpf_regs *r, bpf_byte *mem, bpf_u64 msz,
                       bpf_status *st)
{
    bpf_machine m;

    bpf_machine_init(&m, in, 1);
    m.mem = mem;
    m.mem_size = msz;
    m.regs = *r;
    *st = bpf_step(&m);
    *r = m.regs;
}

static void test_all_ops_w(void)
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
    mem[0] = 10;
    r.r[0] = 0;
    r.r[1] = 0;
    r.r[2] = 3;
    in = mk(OPW, 2, 1, 0x00, 0); /* ADD W */
    run_atomic(&in, &r, mem, 16, &st);
    ck(st == BPF_STAT_RUNNING && mem[0] == 13, "atomic ADD W");

    mem[0] = 0x0F;
    r.r[2] = 0xF0;
    in.imm = 0x40; /* OR */
    run_atomic(&in, &r, mem, 16, &st);
    ck(st == BPF_STAT_RUNNING && mem[0] == 0xFF, "atomic OR W");

    mem[0] = 0x0F;
    r.r[2] = 0x33;
    in.imm = 0x50; /* AND */
    run_atomic(&in, &r, mem, 16, &st);
    ck(st == BPF_STAT_RUNNING && mem[0] == 0x03, "atomic AND W");

    mem[0] = 0x0F;
    r.r[2] = 0x33;
    in.imm = 0xA0; /* XOR */
    run_atomic(&in, &r, mem, 16, &st);
    ck(st == BPF_STAT_RUNNING && mem[0] == 0x3C, "atomic XOR W");
}

static void test_fetch_and_xchg_w(void)
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
    mem[0] = 5;
    r.r[0] = 0;
    r.r[1] = 0;
    r.r[2] = 100;
    in = mk(OPW, 2, 1, 0x01, 0); /* ADD W + FETCH */
    run_atomic(&in, &r, mem, 16, &st);
    ck(st == BPF_STAT_RUNNING && mem[0] == 105 && r.r[2] == 5,
       "atomic ADD FETCH prior in src");

    mem[0] = 7;
    r.r[2] = 9;
    in.imm = 0xE1; /* XCHG W */
    run_atomic(&in, &r, mem, 16, &st);
    ck(st == BPF_STAT_RUNNING && mem[0] == 9 && r.r[2] == 7, "atomic XCHG W");
}

static void test_cmpxchg_w(void)
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
    mem[0] = 5;
    r.r[0] = 5; /* expected match */
    r.r[1] = 0;
    r.r[2] = 9;
    in = mk(OPW, 2, 1, 0xF1, 0); /* CMPXCHG W */
    run_atomic(&in, &r, mem, 16, &st);
    ck(st == BPF_STAT_RUNNING && mem[0] == 9 && r.r[0] == 5,
       "cmpxchg W match writes");

    mem[0] = 5;
    r.r[0] = 6; /* expected mismatch */
    r.r[2] = 9;
    run_atomic(&in, &r, mem, 16, &st);
    ck(st == BPF_STAT_RUNNING && mem[0] == 5 && r.r[0] == 5,
       "cmpxchg W mismatch keeps mem, r0=old");
}

static void test_atomic_dw(void)
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
    /* store 0x100000000 as a DW at addr 0 */
    mem[4] = 0x01;
    r.r[0] = 0;
    r.r[1] = 0;
    r.r[2] = 0x200000000UL;
    in = mk(OPDW, 2, 1, 0x00, 0); /* ADD DW */
    run_atomic(&in, &r, mem, 16, &st);
    /* old = 0x100000000, + 0x200000000 = 0x300000000 */
    ck(st == BPF_STAT_RUNNING && mem[4] == 0x03, "atomic ADD DW");

    r.r[2] = 0x8000000000000000UL;
    mem[0] = 0x01;
    mem[4] = 0x01;
    in.imm = 0xE1; /* XCHG DW */
    run_atomic(&in, &r, mem, 16, &st);
    ck(st == BPF_STAT_RUNNING && r.r[2] == 0x0000000100000001UL,
       "atomic XCHG DW prior");
}

static void test_atomic_bounds(void)
{
    bpf_byte mem[4];
    bpf_insn in;
    bpf_regs r;
    bpf_status st;
    int i;

    for (i = 0; i < 4; ++i)
    {
        mem[i] = 0xEE;
    }
    r.r[0] = 0;
    r.r[1] = 2;
    r.r[2] = 1;
    in = mk(OPW, 2, 1, 0x00, 0); /* ADD W at addr 2..5 -> OOB */
    run_atomic(&in, &r, mem, 4, &st);
    ck(st == BPF_STAT_TRAP, "atomic OOB traps");
    ck(mem[0] == 0xEE && mem[2] == 0xEE, "atomic OOB leaves mem");

    r.r[1] = 4;
    in = mk(OPDW, 2, 1, 0x00, 0); /* ADD DW at addr 4..11 -> OOB */
    run_atomic(&in, &r, mem, 4, &st);
    ck(st == BPF_STAT_TRAP, "atomic DW OOB traps");
}

int main(void)
{
    test_all_ops_w();
    test_fetch_and_xchg_w();
    test_cmpxchg_w();
    test_atomic_dw();
    test_atomic_bounds();
    if (fails)
    {
        (void)fprintf(stderr, "test_atomic2: %d failures\n", fails);
        return 1;
    }
    (void)printf("ok: test_atomic2\n");
    return 0;
}
