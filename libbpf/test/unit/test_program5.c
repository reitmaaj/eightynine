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

/* r0 = 0 if r1==r2 else 1 (via JEQ). */
static void test_equality(void)
{
    bpf_insn prog[7];
    bpf_u64 r0;
    bpf_status st;

    prog[0] = mk(0xb7, 0, 1, 7, 0);
    prog[1] = mk(0xb7, 0, 2, 7, 0);
    prog[2] = mk(0x1d, 2, 1, 0, 2); /* JEQ X r1==r2 -> pc5 */
    prog[3] = mk(0xb7, 0, 0, 1, 0); /* r0=1 (not equal) */
    prog[4] = mk(0x05, 0, 0, 0, 1); /* ja to 6 */
    prog[5] = mk(0xb7, 0, 0, 0, 0); /* r0=0 (equal) */
    prog[6] = mk(0x95, 0, 0, 0, 0);
    st = run(prog, 7, 0, 0, &r0);
    ck(st == BPF_STAT_RETURNED && r0 == 0, "equality equal -> 0");

    prog[1] = mk(0xb7, 0, 2, 8, 0);
    st = run(prog, 7, 0, 0, &r0);
    ck(st == BPF_STAT_RETURNED && r0 == 1, "equality unequal -> 1");
}

static void test_abs(void)
{
    bpf_insn prog[7];
    bpf_u64 r0;
    bpf_status st;

    prog[0] = mk(0xb7, 0, 1, -5, 0); /* r1 = -5 */
    prog[1] = mk(0x75, 2, 1, 0, 3);  /* JSGE X r1>=0 -> pc5 */
    prog[2] = mk(0xb7, 0, 0, 0, 0);  /* r0 = 0 */
    prog[3] = mk(0x1f, 1, 0, 0, 0);  /* r0 = r0 - r1 = 5 */
    prog[4] = mk(0x05, 0, 0, 0, 1);  /* ja to 6 */
    prog[5] = mk(0xbf, 1, 0, 0, 0);  /* r0 = r1 */
    prog[6] = mk(0x95, 0, 0, 0, 0);  /* exit */
    st = run(prog, 7, 0, 0, &r0);
    ck(st == BPF_STAT_RETURNED && r0 == 5, "abs(-5) == 5");

    prog[0] = mk(0xb7, 0, 1, 3, 0); /* r1 = 3 */
    st = run(prog, 7, 0, 0, &r0);
    ck(st == BPF_STAT_RETURNED && r0 == 3, "abs(3) == 3");
}

static void test_pow2_var(void)
{
    bpf_insn prog[8];
    bpf_u64 r0;
    bpf_status st;

    prog[0] = mk(0xb7, 0, 0, 1, 0);  /* r0 = 1 */
    prog[1] = mk(0xb7, 0, 1, 4, 0);  /* r1 = 4 */
    prog[2] = mk(0x27, 0, 0, 2, 0);  /* r0 *= 2 */
    prog[3] = mk(0x17, 0, 1, 1, 0);  /* r1 -= 1 */
    prog[4] = mk(0x15, 0, 1, 0, 1);  /* if r1==0 goto 6 */
    prog[5] = mk(0x05, 0, 0, 0, -4); /* ja to 2 */
    prog[6] = mk(0x95, 0, 0, 0, 0);
    prog[7] = mk(0x95, 0, 0, 0, 0);
    st = run(prog, 8, 0, 0, &r0);
    ck(st == BPF_STAT_RETURNED && r0 == 16, "2^4 == 16");
}

/* r0 = (r1 & 0xF) | (r2 << 4). */
static void test_bit_ops(void)
{
    bpf_insn prog[7];
    bpf_u64 r0;
    bpf_status st;

    prog[0] = mk(0xb7, 0, 1, 0xAB, 0); /* r1 = 0xAB */
    prog[1] = mk(0xb7, 0, 2, 0x02, 0); /* r2 = 0x02 */
    prog[2] = mk(0x57, 0, 1, 0x0F, 0); /* r1 &= 0xF */
    prog[3] = mk(0x67, 0, 2, 4, 0);    /* r2 <<= 4 */
    prog[4] = mk(0xbf, 1, 0, 0, 0);    /* r0 = r1 */
    prog[5] = mk(0x4f, 2, 0, 0, 0);    /* r0 |= r2 */
    prog[6] = mk(0x95, 0, 0, 0, 0);
    st = run(prog, 7, 0, 0, &r0);
    ck(st == BPF_STAT_RETURNED && r0 == 0x2B, "bit ops 0x2B");
}

int main(void)
{
    test_equality();
    test_abs();
    test_pow2_var();
    test_bit_ops();
    if (fails)
    {
        (void)fprintf(stderr, "test_program5: %d failures\n", fails);
        return 1;
    }
    (void)printf("ok: test_program5\n");
    return 0;
}
