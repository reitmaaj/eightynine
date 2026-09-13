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

/* r0 = sum 1..N via loop. */
static void test_sum(void)
{
    bpf_insn prog[8];
    bpf_u64 r0;
    bpf_status st;

    prog[0] = mk(0xb7, 0, 0, 0, 0);   /* r0 = 0 (sum) */
    prog[1] = mk(0xb7, 0, 1, 100, 0); /* r1 = 100 (counter) */
    prog[2] = mk(0x0f, 1, 0, 0, 0);   /* r0 += r1 */
    prog[3] = mk(0x17, 0, 1, 1, 0);   /* r1 -= 1 */
    prog[4] = mk(0x15, 0, 1, 0, 1);   /* if r1==0 goto 6 */
    prog[5] = mk(0x05, 0, 0, 0, -4);  /* ja to 2 */
    prog[6] = mk(0x95, 0, 0, 0, 0);   /* exit */
    st = run(prog, 7, &r0);
    ck(st == BPF_STAT_RETURNED && r0 == 5050, "sum 1..100 == 5050");
}

/* r0 = 2^10 via repeated multiply. */
static void test_pow2(void)
{
    bpf_insn prog[7];
    bpf_u64 r0;
    bpf_status st;

    prog[0] = mk(0xb7, 0, 0, 1, 0);  /* r0 = 1 */
    prog[1] = mk(0xb7, 0, 1, 10, 0); /* r1 = 10 */
    prog[2] = mk(0x27, 0, 0, 2, 0);  /* r0 *= 2 */
    prog[3] = mk(0x17, 0, 1, 1, 0);  /* r1 -= 1 */
    prog[4] = mk(0x15, 0, 1, 0, 1);  /* if r1==0 goto 6 */
    prog[5] = mk(0x05, 0, 0, 0, -4); /* ja to 2 */
    prog[6] = mk(0x95, 0, 0, 0, 0);  /* exit */
    st = run(prog, 7, &r0);
    ck(st == BPF_STAT_RETURNED && r0 == 1024, "2^10 == 1024");
}

/* r0 = number of set bits in r1 (popcount) via loop. */
static void test_popcount(void)
{
    bpf_insn prog[12];
    bpf_u64 r0;
    bpf_status st;

    prog[0] = mk(0xb7, 0, 0, 0, 0);      /* r0 = 0 (count) */
    prog[1] = mk(0xb7, 0, 1, 0x0101, 0); /* r1 = value */
    prog[2] = mk(0xb7, 0, 2, 64, 0);     /* r2 = 64 (iterations) */
    prog[3] = mk(0xbf, 1, 3, 0, 0);      /* r3 = r1 */
    prog[4] = mk(0x57, 0, 3, 1, 0);      /* r3 &= 1 */
    prog[5] = mk(0x0f, 3, 0, 0, 0);      /* r0 += r3 */
    prog[6] = mk(0x77, 0, 1, 1, 0);      /* r1 >>= 1 */
    prog[7] = mk(0x17, 0, 2, 1, 0);      /* r2 -= 1 */
    prog[8] = mk(0x15, 0, 2, 0, 2);      /* if r2==0 goto 11 */
    prog[9] = mk(0x05, 0, 0, 0, -7);     /* ja to 3 */
    prog[10] = mk(0x95, 0, 0, 0, 0);     /* exit */
    prog[11] = mk(0x95, 0, 0, 0, 0);
    st = run(prog, 12, &r0);
    ck(st == BPF_STAT_RETURNED && r0 == 2, "popcount(0x0101) == 2");
}

static void test_max(void)
{
    bpf_insn prog[7];
    bpf_u64 r0;
    bpf_status st;

    prog[0] = mk(0xb7, 0, 1, 7, 0); /* r1 = 7 */
    prog[1] = mk(0xb7, 0, 2, 3, 0); /* r2 = 3 */
    prog[2] = mk(0x6d, 2, 1, 0, 1); /* JSGE X JMP: if (i64)r1>=(i64)r2 goto 4 */
    prog[3] = mk(0xbf, 2, 0, 0, 0); /* r0 = r2 */
    prog[4] = mk(0xbf, 1, 0, 0, 0); /* r0 = r1 */
    prog[5] = mk(0x95, 0, 0, 0, 0); /* exit */
    prog[6] = mk(0x95, 0, 0, 0, 0);
    st = run(prog, 7, &r0);
    ck(st == BPF_STAT_RETURNED && r0 == 7, "max(7,3)==7");
}

/* r0 = reverse the low 32 bits of r1 using shifts. */
static void test_reverse32(void)
{
    bpf_insn prog[13];
    bpf_u64 r0;
    bpf_status st;

    prog[0] = mk(0xb7, 0, 0, 0, 0);          /* r0 = 0 (result) */
    prog[1] = mk(0xb7, 0, 1, 0x12345678, 0); /* r1 = value */
    prog[2] = mk(0xb7, 0, 2, 32, 0);         /* r2 = 32 */
    prog[3] = mk(0xbf, 1, 3, 0, 0);          /* r3 = r1 */
    prog[4] = mk(0x57, 0, 3, 1, 0);          /* r3 &= 1 */
    prog[5] = mk(0x27, 0, 0, 2, 0);          /* r0 *= 2 */
    prog[6] = mk(0x0f, 3, 0, 0, 0);          /* r0 += r3 */
    prog[7] = mk(0x77, 0, 1, 1, 0);          /* r1 >>= 1 */
    prog[8] = mk(0x17, 0, 2, 1, 0);          /* r2 -= 1 */
    prog[9] = mk(0x15, 0, 2, 0, 2);          /* if r2==0 goto 12 */
    prog[10] = mk(0x05, 0, 0, 0, -8);        /* ja to 3 */
    prog[11] = mk(0x95, 0, 0, 0, 0);         /* exit */
    prog[12] = mk(0x95, 0, 0, 0, 0);
    st = run(prog, 13, &r0);
    ck(st == BPF_STAT_RETURNED && r0 == 0x1E6A2C48UL, "reverse32(0x12345678)");
}

int main(void)
{
    test_sum();
    test_pow2();
    test_popcount();
    test_max();
    test_reverse32();
    if (fails)
    {
        (void)fprintf(stderr, "test_program2: %d failures\n", fails);
        return 1;
    }
    (void)printf("ok: test_program2\n");
    return 0;
}
