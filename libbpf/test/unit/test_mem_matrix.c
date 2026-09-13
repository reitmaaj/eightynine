#include <stdio.h>

#include "eval.h"

static int fails;
static int count;

static int put_le_check(bpf_byte *p, bpf_u64 v, int n);

/* Run one memory/store instruction with r1=base, r2=src. want_st: 0=RUNNING,
 * 4=TRAP. */
static void run1(bpf_byte op, bpf_byte src, bpf_byte dst, int imm, int off,
                 bpf_u64 base, bpf_u64 srcv, bpf_byte *mem, bpf_u64 msz,
                 int want_st, bpf_u64 want_reg)
{
    bpf_insn in;
    bpf_machine m;
    bpf_status st;

    in.opcode = op;
    in.class = (bpf_byte)(op & 0x07u);
    in.code = (bpf_byte)(op >> 4);
    in.source = (bpf_byte)((op >> 3) & 0x01u);
    in.mode = (bpf_byte)(op >> 5);
    in.size = (bpf_byte)((op >> 3) & 0x03u);
    in.src_reg = src;
    in.dst_reg = dst;
    in.offset = off;
    in.imm = imm;
    in.is_wide = 0;
    in.next_imm = 0;
    bpf_machine_init(&m, &in, 1);
    m.mem = mem;
    m.mem_size = msz;
    m.regs.r[1] = base;
    m.regs.r[2] = srcv;
    m.regs.r[0] = 0;
    st = bpf_step(&m);
    count = count + 1;
    if (st != (bpf_status)want_st)
    {
        (void)fprintf(stderr, "FAIL: op=%02x want_st=%d got_st=%d\n", op,
                      want_st, (int)st);
        fails = fails + 1;
        return;
    }
    if (want_st == 0 && m.regs.r[dst] != want_reg)
    {
        (void)fprintf(stderr, "FAIL: op=%02x r%u got=%lx want=%lx\n", op, dst,
                      m.regs.r[dst], want_reg);
        fails = fails + 1;
    }
}

static void test_loads(void)
{
    bpf_byte mem[32];
    int i;

    for (i = 0; i < 32; ++i)
    {
        mem[i] = (bpf_byte)i;
    }
    run1(0x71, 1, 2, 0, 0, 0, 0, mem, 32, 0, 0);
    run1(0x69, 1, 2, 0, 0, 0, 0, mem, 32, 0, 0x0100);
    run1(0x61, 1, 2, 0, 0, 0, 0, mem, 32, 0, 0x03020100UL);
    run1(0x79, 1, 2, 0, 0, 0, 0, mem, 32, 0, 0x0706050403020100UL);
    run1(0x69, 1, 2, 0, 4, 0, 0, mem, 32, 0, 0x0504);
    run1(0x61, 1, 2, 0, 8, 0, 0, mem, 32, 0, 0x0B0A0908UL);
    run1(0x61, 1, 2, 0, -4, 8, 0, mem, 32, 0, 0x07060504UL);
    run1(0x79, 1, 2, 0, 0, 30, 0, mem, 32, 4, 0);
    run1(0x71, 1, 2, 0, 0, 32, 0, mem, 32, 4, 0);
    run1(0x71, 1, 2, 0, -1, 0, 0, mem, 32, 4, 0);
}

static void test_memsx(void)
{
    bpf_byte mem[16];
    int i;

    for (i = 0; i < 16; ++i)
    {
        mem[i] = 0x80;
    }
    run1(0x91, 1, 2, 0, 0, 0, 0, mem, 16, 0, (bpf_u64)(bpf_i64)-128);
    run1(0x89, 1, 2, 0, 0, 0, 0, mem, 16, 0, (bpf_u64)(bpf_i64)-32640);
    run1(0x81, 1, 2, 0, 0, 0, 0, mem, 16, 0, (bpf_u64)(bpf_i64)-2139062144);
    mem[0] = 0x7F;
    run1(0x91, 1, 2, 0, 0, 0, 0, mem, 16, 0, 0x7F);
}

static void test_stores(void)
{
    bpf_byte mem[16];
    int i;

    for (i = 0; i < 16; ++i)
    {
        mem[i] = 0;
    }
    run1(0x72, 0, 1, 0xAB, 0, 0, 0, mem, 16, 0, 0);
    if (mem[0] != 0xAB)
    {
        (void)fprintf(stderr, "FAIL: st b imm\n");
        fails = fails + 1;
    }
    run1(0x62, 0, 1, 0x11223344, 0, 4, 0, mem, 16, 0, 4);
    if (put_le_check(mem + 4, 0x11223344UL, 4))
    {
        (void)fprintf(stderr, "FAIL: st w imm\n");
        fails = fails + 1;
    }
    run1(0x7a, 0, 1, -1, 0, 8, 0, mem, 16, 0, 8);
    if (put_le_check(mem + 8, 0xFFFFFFFFFFFFFFFFUL, 8))
    {
        (void)fprintf(stderr, "FAIL: st dw imm\n");
        fails = fails + 1;
    }
    run1(0x73, 2, 1, 0, 0, 12, 0xCD, mem, 16, 0, 12);
    if (mem[12] != 0xCD)
    {
        (void)fprintf(stderr, "FAIL: stx b\n");
        fails = fails + 1;
    }
    run1(0x7a, 0, 1, -1, 0, 14, 0, mem, 16, 4, 0);
}

static int put_le_check(bpf_byte *p, bpf_u64 v, int n)
{
    int i;
    int bad;

    bad = 0;
    for (i = 0; i < n; ++i)
    {
        if (p[i] != (bpf_byte)(v & 0xFFu))
        {
            bad = 1;
        }
        v = v >> 8;
    }
    return bad;
}

static void test_ld_imm(void)
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
    in.imm = 0x11111111;
    in.is_wide = 1;
    in.next_imm = 0x22222222u;
    bpf_machine_init(&m, &in, 1);
    st = bpf_step(&m);
    count = count + 1;
    if (st != BPF_STAT_RUNNING || m.regs.r[1] != 0x2222222211111111UL)
    {
        (void)fprintf(stderr, "FAIL: ld imm 64\n");
        fails = fails + 1;
    }

    in.imm = 0;
    in.next_imm = 0xFFFFFFFFu;
    bpf_machine_init(&m, &in, 1);
    st = bpf_step(&m);
    if (st != BPF_STAT_RUNNING || m.regs.r[1] != 0xFFFFFFFF00000000UL)
    {
        (void)fprintf(stderr, "FAIL: ld imm high\n");
        fails = fails + 1;
    }
}

int main(void)
{
    test_loads();
    test_memsx();
    test_stores();
    test_ld_imm();
    if (fails)
    {
        (void)fprintf(stderr, "test_mem_matrix: %d/%d failed\n", fails, count);
        return 1;
    }
    (void)printf("ok: test_mem_matrix (%d cases)\n", count);
    return 0;
}
