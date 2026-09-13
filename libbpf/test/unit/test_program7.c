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

static bpf_status run(const bpf_insn *prog, bpf_u32 n, bpf_u64 *r0)
{
    bpf_machine m;
    bpf_status st;

    bpf_machine_init(&m, prog, n);
    st = BPF_STAT_RUNNING;
    while (st == BPF_STAT_RUNNING)
    {
        st = bpf_step(&m);
    }
    *r0 = m.regs.r[0];
    return st;
}

/* r0 = minimum of r1, r2, r3 using JSLT. */
static void test_min_three(void)
{
    bpf_insn prog[11];
    bpf_u64 r0;
    bpf_status st;

    prog[0] = mk(0xb7, 0, 1, 5, 0);  /* r1 = 5 */
    prog[1] = mk(0xb7, 0, 2, 3, 0);  /* r2 = 3 */
    prog[2] = mk(0xb7, 0, 3, 9, 0);  /* r3 = 9 */
    prog[3] = mk(0xcd, 2, 1, 0, 2);  /* JSLT X r1<r2 -> pc6 */
    prog[4] = mk(0xbf, 2, 4, 0, 0);  /* r4 = r2 */
    prog[5] = mk(0x05, 0, 0, 0, 1);  /* ja to 7 */
    prog[6] = mk(0xbf, 1, 4, 0, 0);  /* r4 = r1 */
    prog[7] = mk(0xcd, 3, 4, 0, 1);  /* JSLT X r4<r3 -> pc9 */
    prog[8] = mk(0xbf, 3, 0, 0, 0);  /* r0 = r3 */
    prog[9] = mk(0xbf, 4, 0, 0, 0);  /* r0 = r4 */
    prog[10] = mk(0x95, 0, 0, 0, 0); /* exit */
    st = run(prog, 11, &r0);
    ck(st == BPF_STAT_RETURNED && r0 == 3, "min(5,3,9) == 3");
}

/* r0 = count of set bits across r1 and r2 (popcount via shifts). */
static void test_total_popcount(void)
{
    bpf_insn prog[14];
    bpf_u64 r0;
    bpf_status st;

    prog[0] = mk(0xb7, 0, 0, 0, 0);    /* r0 = 0 */
    prog[1] = mk(0xb7, 0, 1, 0x03, 0); /* r1 = 0x03 (2 bits) */
    prog[2] = mk(0xb7, 0, 2, 64, 0);   /* r2 = 64 */
    prog[3] = mk(0xbf, 1, 3, 0, 0);    /* r3 = r1 */
    prog[4] = mk(0x57, 0, 3, 1, 0);    /* r3 &= 1 */
    prog[5] = mk(0x0f, 3, 0, 0, 0);    /* r0 += r3 */
    prog[6] = mk(0x77, 0, 1, 1, 0);    /* r1 >>= 1 */
    prog[7] = mk(0x17, 0, 2, 1, 0);    /* r2 -= 1 */
    prog[8] = mk(0x15, 0, 2, 0, 1);    /* if r2==0 goto 10 */
    prog[9] = mk(0x05, 0, 0, 0, -7);   /* ja to 3 */
    prog[10] = mk(0x95, 0, 0, 0, 0);   /* exit */
    st = run(prog, 11, &r0);
    ck(st == BPF_STAT_RETURNED && r0 == 2, "popcount(0x03) == 2");
}

/* r0 = r1 + r2 - r3 via a straight-line sequence. */
static void test_linear_arith(void)
{
    bpf_insn prog[8];
    bpf_u64 r0;
    bpf_status st;

    prog[0] = mk(0xb7, 0, 1, 10, 0); /* r1 = 10 */
    prog[1] = mk(0xb7, 0, 2, 20, 0); /* r2 = 20 */
    prog[2] = mk(0xb7, 0, 3, 5, 0);  /* r3 = 5 */
    prog[3] = mk(0xbf, 1, 0, 0, 0);  /* r0 = r1 */
    prog[4] = mk(0x0f, 2, 0, 0, 0);  /* r0 += r2 */
    prog[5] = mk(0x1f, 3, 0, 0, 0);  /* r0 -= r3 */
    prog[6] = mk(0x95, 0, 0, 0, 0);  /* exit */
    st = run(prog, 7, &r0);
    ck(st == BPF_STAT_RETURNED && r0 == 25, "10+20-5 == 25");
}

/* r0 = r1 << r2 (variable shift) computed by a loop. */
static void test_var_shift(void)
{
    bpf_insn prog[9];
    bpf_u64 r0;
    bpf_status st;

    prog[0] = mk(0xb7, 0, 0, 1, 0);  /* r0 = 1 */
    prog[1] = mk(0xb7, 0, 1, 3, 0);  /* r1 = 3 (shift count) */
    prog[2] = mk(0x67, 0, 0, 1, 0);  /* r0 <<= 1 */
    prog[3] = mk(0x17, 0, 1, 1, 0);  /* r1 -= 1 */
    prog[4] = mk(0x15, 0, 1, 0, 1);  /* if r1==0 goto 6 */
    prog[5] = mk(0x05, 0, 0, 0, -4); /* ja to 2 */
    prog[6] = mk(0x95, 0, 0, 0, 0);  /* exit */
    prog[7] = mk(0x95, 0, 0, 0, 0);
    prog[8] = mk(0x95, 0, 0, 0, 0);
    st = run(prog, 9, &r0);
    ck(st == BPF_STAT_RETURNED && r0 == 8, "1<<3 == 8");
}

/* r0 = sum of an array stored in memory (bytes at mem[0..3]). */
static void test_array_sum(void)
{
    bpf_insn prog[10];
    bpf_machine m;
    bpf_status st;
    bpf_byte mem[16];
    int i;

    for (i = 0; i < 16; ++i)
    {
        mem[i] = 0;
    }
    mem[0] = 1;
    mem[1] = 2;
    mem[2] = 3;
    mem[3] = 4;
    prog[0] = mk(0xb7, 0, 0, 0, 0);  /* r0 = 0 (sum) */
    prog[1] = mk(0xb7, 0, 1, 0, 0);  /* r1 = 0 (index) */
    prog[2] = mk(0xb7, 0, 2, 4, 0);  /* r2 = 4 (count) */
    prog[3] = mk(0x71, 1, 3, 0, 0);  /* r3 = mem[r1] */
    prog[4] = mk(0x0f, 3, 0, 0, 0);  /* r0 += r3 */
    prog[5] = mk(0x07, 0, 1, 1, 0);  /* r1 += 1 */
    prog[6] = mk(0x17, 0, 2, 1, 0);  /* r2 -= 1 */
    prog[7] = mk(0x15, 0, 2, 0, 1);  /* if r2==0 goto 9 */
    prog[8] = mk(0x05, 0, 0, 0, -6); /* ja to 3 */
    prog[9] = mk(0x95, 0, 0, 0, 0);  /* exit */
    bpf_machine_init(&m, prog, 10);
    m.mem = mem;
    m.mem_size = 16;
    st = BPF_STAT_RUNNING;
    while (st == BPF_STAT_RUNNING)
    {
        st = bpf_step(&m);
    }
    ck(st == BPF_STAT_RETURNED && m.regs.r[0] == 10, "array sum 1+2+3+4 == 10");
}

int main(void)
{
    test_min_three();
    test_total_popcount();
    test_linear_arith();
    test_var_shift();
    test_array_sum();
    if (fails)
    {
        (void)fprintf(stderr, "test_program7: %d failures\n", fails);
        return 1;
    }
    (void)printf("ok: test_program7\n");
    return 0;
}
