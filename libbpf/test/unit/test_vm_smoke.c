#include <stdio.h>

#include "vm.h"

static int fails;
static int cases;

static void ck(int cond, const char *msg)
{
    if (!cond)
    {
        (void)fprintf(stderr, "FAIL: %s\n", msg);
        fails = fails + 1;
    }
}

/* Raw bytecode builder. */
static void e8(bpf_byte *buf, bpf_u32 *o, bpf_u32 v)
{
    buf[*o] = (bpf_byte)(v & 0xFFu);
    *o = *o + 1;
}

static void e16(bpf_byte *buf, bpf_u32 *o, bpf_u32 v)
{
    buf[*o] = (bpf_byte)(v & 0xFFu);
    buf[*o + 1] = (bpf_byte)((v >> 8) & 0xFFu);
    *o = *o + 2;
}

static void e32(bpf_byte *buf, bpf_u32 *o, bpf_u32 v)
{
    buf[*o] = (bpf_byte)(v & 0xFFu);
    buf[*o + 1] = (bpf_byte)((v >> 8) & 0xFFu);
    buf[*o + 2] = (bpf_byte)((v >> 16) & 0xFFu);
    buf[*o + 3] = (bpf_byte)((v >> 24) & 0xFFu);
    *o = *o + 4;
}

static void basic(bpf_byte *buf, bpf_u32 *o, bpf_u32 op, bpf_u32 src,
                  bpf_u32 dst, int imm, int off)
{
    e8(buf, o, op);
    e8(buf, o, (src << 4) | dst);
    e16(buf, o, (bpf_u32)(unsigned short)off);
    e32(buf, o, (bpf_u32)imm);
}

static void wide(bpf_byte *buf, bpf_u32 *o, bpf_u32 dst, bpf_u32 lo, bpf_u32 hi)
{
    basic(buf, o, 0x18u, 0, dst, (int)lo, 0);
    e8(buf, o, 0);
    e8(buf, o, 0);
    e16(buf, o, 0);
    e32(buf, o, hi);
}

static bpf_program *load_prog(bpf_byte *buf, bpf_u32 len)
{
    bpf_profile pr;
    bpf_program *p;
    bpf_err e;

    pr.allowed = 0;
    pr.max_insn = 0;
    p = 0;
    e = bpf_program_load(buf, len, &pr, &p);
    if (e != BPF_OK)
    {
        return 0;
    }
    return p;
}

static bpf_region_cfg mk(bpf_byte kind, bpf_off64 base, bpf_off64 len)
{
    bpf_region_cfg c;

    c.kind = kind;
    c.guest = base;
    c.len = len;
    c.init = 0;
    c.init_len = 0;
    return c;
}

static bpf_region_cfg *vm_regions(bpf_region_cfg *c)
{
    c[0] = mk(BPF_RINPUT, 0x1000, 0x40);
    c[1] = mk(BPF_RMEM, 0x100000, 0x2000);
    c[2] = mk(BPF_RSTACK, 0xF0000, 0x2000);
    return c;
}

/* Run a program to a terminal state; returns state, sets *r0. */
static bpf_vm_state run_prog(bpf_program *prog, bpf_u64 budget, bpf_u64 *r0)
{
    bpf_region_cfg c[3];
    bpf_vm *vm;
    bpf_vm_state st;
    bpf_err e;

    e = bpf_vm_create(prog, vm_regions(c), 3, budget, &vm);
    if (e != BPF_OK)
    {
        return BPF_VM_TRAPPED;
    }
    st = BPF_VM_RUNNING;
    while (st == BPF_VM_RUNNING)
    {
        st = bpf_vm_step(vm);
    }
    *r0 = bpf_vm_reg(vm, 0);
    bpf_vm_destroy(vm);
    return st;
}

static void test_return_immediate(void)
{
    bpf_byte buf[32];
    bpf_program *prog;
    bpf_u64 r0;
    bpf_vm_state st;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 42, 0); /* mov r0, 42 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);  /* exit */
    cases = cases + 1;
    prog = load_prog(buf, o);
    ck(prog != 0, "immediate program loads");
    if (prog == 0)
    {
        return;
    }
    st = run_prog(prog, 100, &r0);
    ck(st == BPF_VM_RETURNED && r0 == 42, "mov r0,42 returns 42");
    bpf_program_destroy(prog);
}

/* The reproduced defect: a conditional branch whose slot target crosses a
 * wide instruction must return 42, not 0. */
static void test_wide_cross_returns_42(void)
{
    bpf_byte buf[64];
    bpf_program *prog;
    bpf_u64 r0;
    bpf_vm_state st;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 42, 0); /* mov r0, 42 */
    basic(buf, &o, 0xb7, 0, 1, 1, 0);  /* mov r1, 1 */
    basic(buf, &o, 0x15, 0, 1, 1, 3);  /* jeq r1,1 -> +3 (past wide) */
    wide(buf, &o, 2, 99, 0);           /* lddw r2,99 : crossed */
    basic(buf, &o, 0xb7, 0, 0, 0, 0);  /* mov r0, 0 (must be skipped) */
    basic(buf, &o, 0x95, 0, 0, 0, 0);  /* exit */
    cases = cases + 1;
    prog = load_prog(buf, o);
    ck(prog != 0, "wide-cross program loads");
    if (prog == 0)
    {
        return;
    }
    st = run_prog(prog, 100, &r0);
    ck(st == BPF_VM_RETURNED && r0 == 42,
       "branch across a wide returns 42 (not the skipped 0)");
    bpf_program_destroy(prog);
}

int main(void)
{
    test_return_immediate();
    test_wide_cross_returns_42();
    if (fails)
    {
        (void)fprintf(stderr, "test_vm_smoke: %d failures\n", fails);
        return 1;
    }
    (void)printf("ok: test_vm_smoke (%d cases)\n", cases);
    return 0;
}
