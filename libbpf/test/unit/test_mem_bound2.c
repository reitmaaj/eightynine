#include <stdio.h>

#include "eval.h"

static int fails;
static int count;

/* Load size `size` (B/H/W/DW) at base `base` + off `off`; expect st and reg. */
static void t_load(bpf_byte size, bpf_u64 base, int off, bpf_u64 msz,
                   int want_st, bpf_u64 want_reg)
{
    bpf_insn in;
    bpf_machine m;
    bpf_status st;
    bpf_byte mem[64];
    int i;

    for (i = 0; i < 64; ++i)
    {
        mem[i] = (bpf_byte)i;
    }
    in.opcode =
        (bpf_byte)((BPF_MODE_MEM << 5) | ((unsigned)size << 3) | BPF_CLS_LDX);
    in.class = BPF_CLS_LDX;
    in.mode = BPF_MODE_MEM;
    in.size = size;
    in.code = 0;
    in.source = 0;
    in.src_reg = 1;
    in.dst_reg = 2;
    in.offset = off;
    in.imm = 0;
    in.is_wide = 0;
    in.next_imm = 0;
    bpf_machine_init(&m, &in, 1);
    m.mem = mem;
    m.mem_size = msz;
    m.regs.r[1] = base;
    m.regs.r[2] = 0;
    st = bpf_step(&m);
    count = count + 1;
    if (st != (bpf_status)want_st)
    {
        (void)fprintf(stderr,
                      "FAIL: load size=%u base=%lu off=%d st=%d"
                      " want=%d\n",
                      size, base, off, (int)st, want_st);
        fails = fails + 1;
        return;
    }
    if (want_st == BPF_STAT_RUNNING && m.regs.r[2] != want_reg)
    {
        (void)fprintf(stderr,
                      "FAIL: load size=%u base=%lu off=%d got=%lx"
                      " want=%lx\n",
                      size, base, off, m.regs.r[2], want_reg);
        fails = fails + 1;
    }
}

#define B BPF_SIZE_B
#define H BPF_SIZE_H
#define W BPF_SIZE_W
#define D BPF_SIZE_DW

static void test_boundary_grid(void)
{
    /* 64-byte mem, mem[i]=i. */
    t_load(B, 0, 0, 64, 0, 0);
    t_load(B, 63, 0, 64, 0, 63);
    t_load(B, 64, 0, 64, 4, 0); /* exactly at end: 1 byte needs addr<64 */
    t_load(B, 63, 1, 64, 4, 0); /* 63+1=64 OOB */
    t_load(H, 62, 0, 64, 0, 0x3F3E);
    t_load(H, 63, 0, 64, 4, 0); /* 2 bytes at 63 OOB */
    t_load(W, 60, 0, 64, 0, 0x3F3E3D3C);
    t_load(W, 61, 0, 64, 4, 0);
    t_load(D, 56, 0, 64, 0, 0x3F3E3D3C3B3A3938UL);
    t_load(D, 57, 0, 64, 4, 0);
    t_load(D, 0, 8, 64, 0, 0x0F0E0D0C0B0A0908UL);
    t_load(W, 8, -4, 64, 0, 0x07060504UL);
    t_load(B, 0, -1, 64, 4, 0); /* negative address */
    t_load(D, 0, -1, 64, 4, 0);
    t_load(H, 8, 55, 64, 4, 0); /* 8+55+2 > 64 */
    t_load(B, 40, 23, 64, 0, 63);
    t_load(W, 16, 48, 64, 4, 0); /* 16+48+4 > 64 */
    t_load(D, 0, 63, 64, 4, 0);  /* 0+63+8 > 64 */
}

static void test_zero_mem(void)
{
    t_load(B, 0, 0, 0, 4, 0); /* empty memory always traps */
    t_load(W, 0, 0, 0, 4, 0);
    t_load(D, 0, 0, 0, 4, 0);
}

static void test_large_base(void)
{
    /* base near 2^63, off negative => large but within? not representable */
    t_load(B, 0x8000000000000000UL, 0, 64, 4, 0);
    t_load(D, 0xFFFFFFFFFFFFFF00UL, 0, 64, 4, 0);
}

int main(void)
{
    test_boundary_grid();
    test_zero_mem();
    test_large_base();
    if (fails)
    {
        (void)fprintf(stderr, "test_mem_bound2: %d/%d failed\n", fails, count);
        return 1;
    }
    (void)printf("ok: test_mem_bound2 (%d cases)\n", count);
    return 0;
}
