#include <stdio.h>

#include "eval.h"

static int fails;
static int count;

static int mem_bytes(bpf_byte *mem, int off, int n, bpf_u64 val)
{
    int i;
    int bad;

    bad = 0;
    for (i = 0; i < n; ++i)
    {
        if (mem[off + i] != (bpf_byte)((val >> (8 * i)) & 0xFFu))
        {
            bad = 1;
        }
    }
    return bad;
}

/* Store a value of `size` at base+off; check bytes or trap. */
static void t_store(bpf_byte size, bpf_byte cls, bpf_u64 base, int off,
                    bpf_u64 val, int is_imm, bpf_u64 msz, int want_st)
{
    bpf_insn in;
    bpf_machine m;
    bpf_status st;
    bpf_byte mem[64];
    int i;
    int n;

    for (i = 0; i < 64; ++i)
    {
        mem[i] = 0xEE;
    }
    n = 1;
    if (size == BPF_SIZE_H)
    {
        n = 2;
    }
    if (size == BPF_SIZE_W)
    {
        n = 4;
    }
    if (size == BPF_SIZE_DW)
    {
        n = 8;
    }
    in.opcode = (bpf_byte)((BPF_MODE_MEM << 5) | ((unsigned)size << 3) | cls);
    in.class = cls;
    in.mode = BPF_MODE_MEM;
    in.size = size;
    in.code = 0;
    in.source = 0;
    in.src_reg = is_imm ? 0 : 2;
    in.dst_reg = 1;
    in.offset = off;
    in.imm = is_imm ? (int)val : 0;
    in.is_wide = 0;
    in.next_imm = 0;
    bpf_machine_init(&m, &in, 1);
    m.mem = mem;
    m.mem_size = msz;
    m.regs.r[1] = base;
    m.regs.r[2] = val;
    st = bpf_step(&m);
    count = count + 1;
    if (st != (bpf_status)want_st)
    {
        (void)fprintf(stderr,
                      "FAIL: store size=%u cls=%u base=%lu off=%d"
                      " st=%d want=%d\n",
                      size, cls, base, off, (int)st, want_st);
        fails = fails + 1;
        return;
    }
    if (want_st == BPF_STAT_RUNNING && mem_bytes(mem, (int)base + off, n, val))
    {
        (void)fprintf(stderr, "FAIL: store bytes size=%u base=%lu off=%d\n",
                      size, base, off);
        fails = fails + 1;
    }
}

#define ST BPF_CLS_ST
#define STX BPF_CLS_STX
#define B BPF_SIZE_B
#define H BPF_SIZE_H
#define W BPF_SIZE_W
#define D BPF_SIZE_DW

static void test_store_boundaries(void)
{
    t_store(B, ST, 0, 0, 0xAB, 1, 64, 0);
    t_store(H, ST, 0, 0, 0xABCD, 1, 64, 0);
    t_store(W, ST, 0, 0, 0x11223344UL, 1, 64, 0);
    t_store(D, ST, 0, 0, 0x0000000011223344UL, 1, 64, 0);
    t_store(B, STX, 0, 0, 0xCD, 0, 64, 0);
    t_store(H, STX, 0, 0, 0xCDEF, 0, 64, 0);
    t_store(W, STX, 0, 0, 0x89ABCDEFUL, 0, 64, 0);
    t_store(D, STX, 0, 0, 0xFEDCBA9876543210UL, 0, 64, 0);
    /* with offsets */
    t_store(W, STX, 4, 0, 0x01020304UL, 0, 64, 0);
    t_store(W, STX, 8, -4, 0x05060708UL, 0, 64, 0);
    /* boundary: exactly fits */
    t_store(B, ST, 63, 0, 0x42, 1, 64, 0);
    t_store(H, ST, 62, 0, 0x4243, 1, 64, 0);
    t_store(W, ST, 60, 0, 0x42434445UL, 1, 64, 0);
    t_store(D, ST, 56, 0, 0x0000000046474849UL, 1, 64, 0);
    /* one past -> trap */
    t_store(B, ST, 64, 0, 0, 1, 64, 4);
    t_store(H, ST, 63, 0, 0, 1, 64, 4);
    t_store(W, ST, 61, 0, 0, 1, 64, 4);
    t_store(D, ST, 57, 0, 0, 1, 64, 4);
    /* negative address -> trap */
    t_store(B, ST, 0, -1, 0, 1, 64, 4);
    t_store(W, STX, 0, -1, 0, 0, 64, 4);
    /* empty memory -> trap */
    t_store(B, ST, 0, 0, 0, 1, 0, 4);
    t_store(D, STX, 0, 0, 0, 0, 0, 4);
}

static void test_sign_extended_imm_store(void)
{
    bpf_insn in;
    bpf_machine m;
    bpf_status st;
    bpf_byte mem[16];
    int i;

    for (i = 0; i < 16; ++i)
    {
        mem[i] = 0;
    }
    /* ST DW imm = -1 -> stores all 0xFF */
    in.opcode = (bpf_byte)((BPF_MODE_MEM << 5) | (BPF_SIZE_DW << 3) | ST);
    in.class = ST;
    in.mode = BPF_MODE_MEM;
    in.size = BPF_SIZE_DW;
    in.code = 0;
    in.source = 0;
    in.src_reg = 0;
    in.dst_reg = 1;
    in.offset = 0;
    in.imm = -1;
    in.is_wide = 0;
    in.next_imm = 0;
    bpf_machine_init(&m, &in, 1);
    m.mem = mem;
    m.mem_size = 16;
    m.regs.r[1] = 0;
    st = bpf_step(&m);
    count = count + 1;
    if (st != BPF_STAT_RUNNING)
    {
        (void)fprintf(stderr, "FAIL: st dw imm -1\n");
        fails = fails + 1;
        return;
    }
    for (i = 0; i < 8; ++i)
    {
        if (mem[i] != 0xFF)
        {
            (void)fprintf(stderr, "FAIL: st dw imm -1 byte %d\n", i);
            fails = fails + 1;
        }
    }
}

int main(void)
{
    test_store_boundaries();
    test_sign_extended_imm_store();
    if (fails)
    {
        (void)fprintf(stderr, "test_mem_store: %d/%d failed\n", fails, count);
        return 1;
    }
    (void)printf("ok: test_mem_store (%d cases)\n", count);
    return 0;
}
