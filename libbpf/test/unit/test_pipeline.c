#include <stdio.h>

#include "decode.h"
#include "eval.h"
#include "validate.h"

static int fails;
static int count;

/* Decode + validate + run a raw little-endian bytecode buffer. */
static bpf_status pipeline(const bpf_byte *buf, bpf_u32 len, bpf_byte *mem,
                           bpf_u64 msz, bpf_u64 *r0, bpf_err *de, bpf_err *ve)
{
    bpf_insn ins[64];
    bpf_u32 n;
    bpf_u32 conf;
    bpf_err e;
    bpf_machine m;
    bpf_status st;

    e = bpf_decode(buf, len, ins, 64, &n);
    *de = e;
    if (e != BPF_OK)
    {
        return BPF_STAT_ERR;
    }
    e = bpf_validate(ins, n, &conf);
    *ve = e;
    if (e != BPF_OK)
    {
        return BPF_STAT_ERR;
    }
    bpf_machine_init(&m, ins, n);
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

static void put32(bpf_byte *p, int v)
{
    p[0] = (bpf_byte)(v & 0xFF);
    p[1] = (bpf_byte)((v >> 8) & 0xFF);
    p[2] = (bpf_byte)((v >> 16) & 0xFF);
    p[3] = (bpf_byte)((v >> 24) & 0xFF);
}

static void put16(bpf_byte *p, int v)
{
    p[0] = (bpf_byte)(v & 0xFF);
    p[1] = (bpf_byte)((v >> 8) & 0xFF);
}

static void insn(bpf_byte *p, bpf_byte op, bpf_byte regs, int off, int imm)
{
    p[0] = op;
    p[1] = regs;
    put16(p + 2, off);
    put32(p + 4, imm);
}

static void test_hello_world(void)
{
    bpf_byte buf[16];
    bpf_u64 r0;
    bpf_err de;
    bpf_err ve;
    bpf_status st;

    /* r0 = 42; exit */
    insn(buf, 0xb7, 0x00, 0, 42);
    insn(buf + 8, 0x95, 0x00, 0, 0);
    st = pipeline(buf, 16, 0, 0, &r0, &de, &ve);
    count = count + 1;
    if (de != BPF_OK || ve != BPF_OK || st != BPF_STAT_RETURNED || r0 != 42)
    {
        (void)fprintf(stderr, "FAIL: hello world\n");
        fails = fails + 1;
    }
}

static void test_arithmetic_program(void)
{
    bpf_byte buf[48];
    bpf_u64 r0;
    bpf_err de;
    bpf_err ve;
    bpf_status st;

    /* r0 = (7 + 3) * 4 - 8 = 32 */
    insn(buf, 0xb7, 0x00, 0, 7);
    insn(buf + 8, 0x07, 0x00, 0, 3);
    insn(buf + 16, 0x27, 0x00, 0, 4);
    insn(buf + 24, 0x17, 0x00, 0, 8);
    insn(buf + 32, 0x95, 0x00, 0, 0);
    st = pipeline(buf, 40, 0, 0, &r0, &de, &ve);
    count = count + 1;
    if (st != BPF_STAT_RETURNED || r0 != 32)
    {
        (void)fprintf(stderr, "FAIL: arithmetic program r0=%lx\n", r0);
        fails = fails + 1;
    }
}

static void test_conditional_program(void)
{
    bpf_byte buf[48];
    bpf_u64 r0;
    bpf_err de;
    bpf_err ve;
    bpf_status st;

    /* r1 = 5; r2 = 3; if r1 > r2 goto taken (r0=1) else r0=2; exit */
    insn(buf, 0xb7, 0x01, 0, 5);
    insn(buf + 8, 0xb7, 0x02, 0, 3);
    insn(buf + 16, 0x2d, 0x21, 0, 2); /* JGT X r1>r2 -> +2 = pc4 */
    insn(buf + 24, 0xb7, 0x00, 0, 2); /* r0 = 2 (not taken) */
    insn(buf + 32, 0xb7, 0x00, 0, 1); /* r0 = 1 (taken) */
    insn(buf + 40, 0x95, 0x00, 0, 0);
    st = pipeline(buf, 48, 0, 0, &r0, &de, &ve);
    count = count + 1;
    if (st != BPF_STAT_RETURNED || r0 != 1)
    {
        (void)fprintf(stderr, "FAIL: conditional r0=%lx\n", r0);
        fails = fails + 1;
    }
}

static void test_memory_program(void)
{
    bpf_byte buf[48];
    bpf_byte mem[8];
    bpf_u64 r0;
    bpf_err de;
    bpf_err ve;
    bpf_status st;
    int i;

    for (i = 0; i < 8; ++i)
    {
        mem[i] = 0;
    }
    /* r1=0; r2=9; STX B mem[0]=r2; LDX B r0=mem[0]; r0+=1; exit */
    insn(buf, 0xb7, 0x01, 0, 0);
    insn(buf + 8, 0xb7, 0x02, 0, 9);
    insn(buf + 16, 0x73, 0x21, 0, 0);
    insn(buf + 24, 0x71, 0x10, 0, 0);
    insn(buf + 32, 0x07, 0x00, 0, 1);
    insn(buf + 40, 0x95, 0x00, 0, 0);
    st = pipeline(buf, 48, mem, 8, &r0, &de, &ve);
    count = count + 1;
    if (st != BPF_STAT_RETURNED || r0 != 10)
    {
        (void)fprintf(stderr, "FAIL: memory program r0=%lx\n", r0);
        fails = fails + 1;
    }
}

static void test_invalid_program(void)
{
    bpf_byte buf[16];
    bpf_u64 r0;
    bpf_err de;
    bpf_err ve;
    bpf_status st;

    /* register 11 out of range -> validate error */
    insn(buf, 0xb7, 0x0b, 0, 1);
    insn(buf + 8, 0x95, 0x00, 0, 0);
    st = pipeline(buf, 16, 0, 0, &r0, &de, &ve);
    count = count + 1;
    (void)st;
    if (ve != BPF_EREG)
    {
        (void)fprintf(stderr, "FAIL: invalid reg not rejected\n");
        fails = fails + 1;
    }
}

static void test_truncated_program(void)
{
    bpf_byte buf[8];
    bpf_u64 r0;
    bpf_err de;
    bpf_err ve;
    bpf_status st;

    /* truncated wide LD (8 bytes only) */
    insn(buf, 0x18, 0x01, 0, 1);
    st = pipeline(buf, 8, 0, 0, &r0, &de, &ve);
    count = count + 1;
    (void)st;
    if (de != BPF_ETRUNC)
    {
        (void)fprintf(stderr, "FAIL: truncated not rejected\n");
        fails = fails + 1;
    }
}

int main(void)
{
    test_hello_world();
    test_arithmetic_program();
    test_conditional_program();
    test_memory_program();
    test_invalid_program();
    test_truncated_program();
    if (fails)
    {
        (void)fprintf(stderr, "test_pipeline: %d/%d failed\n", fails, count);
        return 1;
    }
    (void)printf("ok: test_pipeline (%d cases)\n", count);
    return 0;
}
