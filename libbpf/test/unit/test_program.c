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

/* Run a program to completion; return status and set *r0. */
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

/* r0 = 0; loop 100 times r0++; exit -> r0 = 100. */
static void test_count_loop(void)
{
    bpf_insn prog[8];
    bpf_u64 r0;
    bpf_status st;

    prog[0] = mk(0xb7, 0, 0, 0, 0);   /* r0 = 0 */
    prog[1] = mk(0xb7, 0, 1, 100, 0); /* r1 = 100 (counter) */
    prog[2] = mk(0x07, 0, 0, 1, 0);   /* r0 += 1 */
    prog[3] = mk(0x17, 0, 1, 1, 0);   /* r1 -= 1 */
    prog[4] = mk(0x15, 0, 1, 0, 1);   /* if r1 == 0 goto pc6 (exit) */
    prog[5] = mk(0x05, 0, 0, 0, -4);  /* ja back to pc2 (loop) */
    prog[6] = mk(0x95, 0, 0, 0, 0);   /* exit */
    st = run(prog, 7, 0, 0, &r0);
    ck(st == BPF_STAT_RETURNED && r0 == 100, "count loop -> 100");
}

/* r0 = fib(n) via recursion-free loop. */
static void test_fib(void)
{
    bpf_insn prog[16];
    bpf_u64 r0;
    bpf_status st;
    bpf_u64 a;
    bpf_u64 b;
    int i;

    /* r2 = 0 (a), r3 = 1 (b), r1 = 10 (n), r0 = a */
    prog[0] = mk(0xb7, 0, 0, 0, 0);
    prog[1] = mk(0xb7, 0, 1, 10, 0);
    prog[2] = mk(0xb7, 0, 2, 0, 0);
    prog[3] = mk(0xb7, 0, 3, 1, 0);
    /* loop: if r1 == 0 goto exit */
    prog[4] = mk(0x15, 0, 1, 0, 7);   /* if r1==0 goto pc12 (exit) */
    prog[5] = mk(0xbf, 2, 4, 0, 0);   /* r4 = a */
    prog[6] = mk(0x0f, 3, 4, 0, 0);   /* r4 += b */
    prog[7] = mk(0xbf, 3, 2, 0, 0);   /* a = b */
    prog[8] = mk(0xbf, 4, 3, 0, 0);   /* b = r4 */
    prog[9] = mk(0xbf, 2, 0, 0, 0);   /* r0 = a */
    prog[10] = mk(0x17, 0, 1, 1, 0);  /* r1 -= 1 */
    prog[11] = mk(0x05, 0, 0, 0, -8); /* ja back to pc4 (re-check) */
    prog[12] = mk(0x95, 0, 0, 0, 0);  /* exit */
    st = run(prog, 13, 0, 0, &r0);

    a = 0;
    b = 1;
    for (i = 0; i < 10; ++i)
    {
        bpf_u64 c;
        c = a + b;
        a = b;
        b = c;
    }
    ck(st == BPF_STAT_RETURNED && r0 == a, "fib(10)");
}

/* Store 5 to mem[0], load it back, add 2, exit with r0. */
static void test_memory_program(void)
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
    prog[0] = mk(0xb7, 0, 1, 0, 0); /* r1 = 0 (address) */
    prog[1] = mk(0xb7, 0, 2, 5, 0); /* r2 = 5 */
    prog[2] = mk(0x73, 2, 1, 0, 0); /* ST B *(r1+0) = r2 */
    prog[3] = mk(0x71, 1, 0, 0, 0); /* LDX B r0 = *(u8*)(r1+0) */
    prog[4] = mk(0x07, 0, 0, 2, 0); /* r0 += 2 */
    prog[5] = mk(0x95, 0, 0, 0, 0); /* exit */
    st = run(prog, 6, mem, 16, &r0);
    ck(st == BPF_STAT_RETURNED && r0 == 7, "memory read-modify-write -> 7");
}

/* Signed comparison: r0 = (r1 < r2 as signed) where r1=-1, r2=1. */
static void test_signed_compare_program(void)
{
    bpf_insn prog[8];
    bpf_u64 r0;
    bpf_status st;

    prog[0] = mk(0xb7, 0, 1, -1, 0); /* r1 = -1 */
    prog[1] = mk(0xb7, 0, 2, 1, 0);  /* r2 = 1 */
    prog[2] = mk(0xCE, 2, 1, 0, 2);  /* JSLT X JMP32 -> pc5 if taken */
    prog[3] = mk(0xb7, 0, 0, 0, 0);  /* r0 = 0 (not taken) */
    prog[4] = mk(0x05, 0, 0, 0, 1);  /* ja -> pc6 (exit) */
    prog[5] = mk(0xb7, 0, 0, 1, 0);  /* r0 = 1 (taken) */
    prog[6] = mk(0x95, 0, 0, 0, 0);  /* exit */
    st = run(prog, 7, 0, 0, &r0);
    ck(st == BPF_STAT_RETURNED && r0 == 1, "signed compare branch taken");
}

/* Atomic counter: add 3 to mem W twice with fetch. */
static void test_atomic_counter_program(void)
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
    prog[0] = mk(0xb7, 0, 1, 0, 0);    /* r1 = 0 */
    prog[1] = mk(0xb7, 0, 2, 3, 0);    /* r2 = 3 */
    prog[2] = mk(0xC3, 2, 1, 0x00, 0); /* ATOMIC ADD W mem[0] += 3 */
    prog[3] = mk(0xC3, 2, 1, 0x00, 0); /* ATOMIC ADD W again */
    prog[4] = mk(0x61, 1, 0, 0, 0);    /* LDX W r0 = mem[0] */
    prog[5] = mk(0x95, 0, 0, 0, 0);    /* exit */
    st = run(prog, 6, mem, 16, &r0);
    ck(st == BPF_STAT_RETURNED && r0 == 6, "atomic counter -> 6");
}

int main(void)
{
    test_count_loop();
    test_fib();
    test_memory_program();
    test_signed_compare_program();
    test_atomic_counter_program();
    if (fails)
    {
        (void)fprintf(stderr, "test_program: %d failures\n", fails);
        return 1;
    }
    (void)printf("ok: test_program\n");
    return 0;
}
