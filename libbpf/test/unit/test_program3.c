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

/* r0 = r1 * r2 via repeated addition (mul). */
static void test_mul_loop(void)
{
    bpf_insn prog[9];
    bpf_u64 r0;
    bpf_status st;

    prog[0] = mk(0xb7, 0, 0, 0, 0);  /* r0 = 0 (acc) */
    prog[1] = mk(0xb7, 0, 1, 6, 0);  /* r1 = 6 (multiplicand) */
    prog[2] = mk(0xb7, 0, 2, 7, 0);  /* r2 = 7 (count) */
    prog[3] = mk(0x0f, 1, 0, 0, 0);  /* r0 += r1 */
    prog[4] = mk(0x17, 0, 2, 1, 0);  /* r2 -= 1 */
    prog[5] = mk(0x15, 0, 2, 0, 1);  /* if r2==0 goto 7 */
    prog[6] = mk(0x05, 0, 0, 0, -4); /* ja to 3 */
    prog[7] = mk(0x95, 0, 0, 0, 0);  /* exit */
    st = run(prog, 8, &r0);
    ck(st == BPF_STAT_RETURNED && r0 == 42, "6*7 via loop == 42");
}

/* r0 = gcd(r1, r2) via Euclid. */
static void test_gcd(void)
{
    bpf_insn prog[10];
    bpf_u64 r0;
    bpf_status st;

    prog[0] = mk(0xb7, 0, 1, 48, 0); /* r1 = 48 */
    prog[1] = mk(0xb7, 0, 2, 36, 0); /* r2 = 36 */
    prog[2] = mk(0x15, 0, 2, 0, 5);  /* if r2==0 goto 8 */
    prog[3] = mk(0xbf, 1, 3, 0, 0);  /* r3 = r1 */
    prog[4] = mk(0x9f, 2, 3, 0, 0);  /* r3 %= r2 */
    prog[5] = mk(0xbf, 2, 1, 0, 0);  /* r1 = r2 */
    prog[6] = mk(0xbf, 3, 2, 0, 0);  /* r2 = r3 */
    prog[7] = mk(0x05, 0, 0, 0, -6); /* ja to 2 */
    prog[8] = mk(0xbf, 1, 0, 0, 0);  /* r0 = r1 */
    prog[9] = mk(0x95, 0, 0, 0, 0);  /* exit */
    st = run(prog, 10, &r0);
    ck(st == BPF_STAT_RETURNED && r0 == 12, "gcd(48,36) == 12");
}

static void test_factorial(void)
{
    bpf_insn prog[9];
    bpf_u64 r0;
    bpf_status st;

    prog[0] = mk(0xb7, 0, 0, 1, 0);  /* r0 = 1 (acc) */
    prog[1] = mk(0xb7, 0, 1, 5, 0);  /* r1 = 5 */
    prog[2] = mk(0x2f, 1, 0, 0, 0);  /* r0 *= r1 */
    prog[3] = mk(0x17, 0, 1, 1, 0);  /* r1 -= 1 */
    prog[4] = mk(0x15, 0, 1, 0, 1);  /* if r1==0 goto 6 */
    prog[5] = mk(0x05, 0, 0, 0, -4); /* ja to 2 */
    prog[6] = mk(0x95, 0, 0, 0, 0);  /* exit */
    prog[7] = mk(0x95, 0, 0, 0, 0);
    prog[8] = mk(0x95, 0, 0, 0, 0);
    st = run(prog, 9, &r0);
    ck(st == BPF_STAT_RETURNED && r0 == 120, "5! == 120");
}

/* r0 = count of even numbers in 0..r1. */
static void test_count_even(void)
{
    bpf_insn prog[11];
    bpf_u64 r0;
    bpf_status st;

    prog[0] = mk(0xb7, 0, 0, 0, 0);  /* r0 = 0 (count) */
    prog[1] = mk(0xb7, 0, 1, 10, 0); /* r1 = 10 */
    prog[2] = mk(0x15, 0, 1, 0, 7);  /* if r1==0 goto 10 */
    prog[3] = mk(0xbf, 1, 3, 0, 0);  /* r3 = r1 */
    prog[4] = mk(0x57, 0, 3, 1, 0);  /* r3 &= 1 */
    prog[5] = mk(0x15, 0, 3, 0, 1);  /* if r3==0 goto 7 (even) */
    prog[6] = mk(0x05, 0, 0, 0, 1);  /* skip increment */
    prog[7] = mk(0x07, 0, 0, 1, 0);  /* r0 += 1 */
    prog[8] = mk(0x17, 0, 1, 1, 0);  /* r1 -= 1 */
    prog[9] = mk(0x05, 0, 0, 0, -8); /* ja to 2 */
    prog[10] = mk(0x95, 0, 0, 0, 0); /* exit */
    st = run(prog, 11, &r0);
    ck(st == BPF_STAT_RETURNED && r0 == 5, "count even 1..10 == 5");
}

int main(void)
{
    test_mul_loop();
    test_gcd();
    test_factorial();
    test_count_even();
    if (fails)
    {
        (void)fprintf(stderr, "test_program3: %d failures\n", fails);
        return 1;
    }
    (void)printf("ok: test_program3\n");
    return 0;
}
