#include <stdio.h>

#include "eval.h"

static int fails;
static int count;

static int mem_eq(bpf_byte *a, bpf_byte *b, int n)
{
    int i;

    for (i = 0; i < n; ++i)
    {
        if (a[i] != b[i])
        {
            return 0;
        }
    }
    return 1;
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

static void test_byte_load_store(void)
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
    /* LDX B r2 = mem[5] -> 5 */
    r.r[0] = 0;
    r.r[1] = 5;
    r.r[2] = 0;
    in.opcode = 0x71;
    in.class = BPF_CLS_LDX;
    in.mode = BPF_MODE_MEM;
    in.size = BPF_SIZE_B;
    in.src_reg = 1;
    in.dst_reg = 2;
    in.offset = 0;
    in.imm = 0;
    in.is_wide = 0;
    in.next_imm = 0;
    st = run1(&in, &r, mem, 16);
    count = count + 1;
    if (st != BPF_STAT_RUNNING || r.r[2] != 5)
    {
        (void)fprintf(stderr, "FAIL: ldx b\n");
        fails = fails + 1;
    }

    /* STX B mem[10] = r2 (5) */
    r.r[1] = 10;
    in.opcode = 0x73;
    in.class = BPF_CLS_STX;
    in.src_reg = 2;
    in.dst_reg = 1;
    st = run1(&in, &r, mem, 16);
    count = count + 1;
    if (st != BPF_STAT_RUNNING || mem[10] != 5)
    {
        (void)fprintf(stderr, "FAIL: stx b\n");
        fails = fails + 1;
    }
}

static void test_store_load_roundtrip(void)
{
    bpf_byte mem[32];
    bpf_insn in;
    bpf_regs r;
    bpf_status st;
    int i;

    for (i = 0; i < 32; ++i)
    {
        mem[i] = 0;
    }
    /* STX W mem[4] = 0x12345678; LDX W r3 = mem[4] */
    r.r[0] = 0;
    r.r[1] = 4;
    r.r[2] = 0x12345678UL;
    r.r[3] = 0;
    in.opcode = 0x63;
    in.class = BPF_CLS_STX;
    in.mode = BPF_MODE_MEM;
    in.size = BPF_SIZE_W;
    in.src_reg = 2;
    in.dst_reg = 1;
    in.offset = 0;
    in.imm = 0;
    in.is_wide = 0;
    in.next_imm = 0;
    st = run1(&in, &r, mem, 32);
    count = count + 1;
    if (st != BPF_STAT_RUNNING)
    {
        (void)fprintf(stderr, "FAIL: stx w\n");
        fails = fails + 1;
    }
    in.opcode = 0x61;
    in.class = BPF_CLS_LDX;
    in.src_reg = 1;
    in.dst_reg = 3;
    st = run1(&in, &r, mem, 32);
    count = count + 1;
    if (st != BPF_STAT_RUNNING || r.r[3] != 0x12345678UL)
    {
        (void)fprintf(stderr, "FAIL: roundtrip w\n");
        fails = fails + 1;
    }
    {
        bpf_byte want[4];

        want[0] = 0x78;
        want[1] = 0x56;
        want[2] = 0x34;
        want[3] = 0x12;
        if (mem_eq(mem + 4, want, 4) == 0)
        {
            (void)fprintf(stderr, "FAIL: roundtrip bytes\n");
            fails = fails + 1;
        }
    }
}

int main(void)
{
    test_byte_load_store();
    test_store_load_roundtrip();
    if (fails)
    {
        (void)fprintf(stderr, "test_mem4: %d/%d failed\n", fails, count);
        return 1;
    }
    (void)printf("ok: test_mem4 (%d cases)\n", count);
    return 0;
}
