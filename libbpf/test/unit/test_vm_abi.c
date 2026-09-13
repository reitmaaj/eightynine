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

static void regs3(bpf_region_cfg *c)
{
    c[0] = mk(BPF_RINPUT, 0x1000, 0x40);
    c[1] = mk(BPF_RMEM, 0x100000, 0x2000);
    c[2] = mk(BPF_RSTACK, 0xF0000, 0x2000);
}

static bpf_vm_state run_prog(bpf_program *prog, bpf_u64 budget, bpf_u64 *r0)
{
    bpf_region_cfg c[3];
    bpf_vm *vm;
    bpf_vm_state st;
    bpf_err e;

    regs3(c);
    e = bpf_vm_create(prog, c, 3, budget, &vm);
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

/* Run and expect state RETURNED with r0 == want. */
static void expect_ret(bpf_byte *buf, bpf_u32 len, bpf_u64 want,
                       const char *name)
{
    bpf_program *prog;
    bpf_u64 r0;
    bpf_vm_state st;

    cases = cases + 1;
    prog = load_prog(buf, len);
    if (prog == 0)
    {
        ck(0, name);
        return;
    }
    r0 = 0;
    st = run_prog(prog, 10000, &r0);
    ck(st == BPF_VM_RETURNED && r0 == want, name);
    bpf_program_destroy(prog);
}

/* Run and expect a trap (used for call-depth overrun). */
static void expect_trap(bpf_byte *buf, bpf_u32 len, const char *name)
{
    bpf_program *prog;
    bpf_u64 r0;
    bpf_vm_state st;

    cases = cases + 1;
    prog = load_prog(buf, len);
    if (prog == 0)
    {
        ck(0, name);
        return;
    }
    r0 = 0;
    st = run_prog(prog, 100000, &r0);
    ck(st == BPF_VM_TRAPPED, name);
    bpf_program_destroy(prog);
}

/* Caller sets r6-r9; callee zeroes them; after return the caller sees them
 * restored. r0 is derived from the preserved r6. */
static void test_preserved_across_call(void)
{
    bpf_byte buf[128];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 6, 11, 0); /* idx0 r6 = 11 */
    basic(buf, &o, 0xb7, 0, 7, 22, 0); /* idx1 r7 = 22 */
    basic(buf, &o, 0xb7, 0, 8, 33, 0); /* idx2 r8 = 33 */
    basic(buf, &o, 0xb7, 0, 9, 44, 0); /* idx3 r9 = 44 */
    basic(buf, &o, 0x85, 1, 0, 2, 0);  /* idx4 call imm2 -> idx7 */
    basic(buf, &o, 0xbf, 6, 0, 0, 0);  /* idx5 r0 = r6 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);  /* idx6 exit */
    basic(buf, &o, 0xb7, 0, 6, 0, 0);  /* idx7 callee clobbers r6-r9 */
    basic(buf, &o, 0xb7, 0, 7, 0, 0);
    basic(buf, &o, 0xb7, 0, 8, 0, 0);
    basic(buf, &o, 0xb7, 0, 9, 0, 0);
    basic(buf, &o, 0xb7, 0, 0, 999, 0); /* idx11 callee sets r0 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);   /* idx12 callee exit */
    expect_ret(buf, o, 11, "r6 restored across a call");
}

/* Verify r9 (not just r6) is preserved too. */
static void test_preserved_r9(void)
{
    bpf_byte buf[128];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 9, 77, 0); /* idx0 r9 = 77 */
    basic(buf, &o, 0x85, 1, 0, 2, 0);  /* idx1 call imm2 -> idx3 */
    basic(buf, &o, 0xbf, 9, 0, 0, 0);  /* idx2 r0 = r9 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);  /* idx3 exit */
    basic(buf, &o, 0xb7, 0, 9, 0, 0);  /* idx4 callee clobbers r9 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);  /* idx5 callee exit */
    expect_ret(buf, o, 77, "r9 restored across a call");
}

/* r0 carries the callee return value into the caller. */
static void test_return_value(void)
{
    bpf_byte buf[128];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 1, 0);  /* idx0 r0 = 1 */
    basic(buf, &o, 0x85, 1, 0, 1, 0);  /* idx1 call imm1 -> idx3 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);  /* idx2 exit */
    basic(buf, &o, 0xb7, 0, 0, 42, 0); /* idx3 callee r0 = 42 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);  /* idx4 callee exit */
    expect_ret(buf, o, 42, "callee r0 reaches the caller");
}

/* Nested calls preserve each level's saved registers. */
static void test_nested_preserve(void)
{
    bpf_byte buf[128];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 6, 5, 0); /* idx0 outer r6 = 5 */
    basic(buf, &o, 0x85, 1, 0, 2, 0); /* idx1 call -> idx4 (middle) */
    basic(buf, &o, 0xbf, 6, 0, 0, 0); /* idx2 r0 = r6 */
    basic(buf, &o, 0x95, 0, 0, 0, 0); /* idx3 exit */
    basic(buf, &o, 0xb7, 0, 6, 9, 0); /* idx4 middle r6 = 9 */
    basic(buf, &o, 0x85, 1, 0, 1, 0); /* idx5 middle call -> idx7 (inner) */
    basic(buf, &o, 0x95, 0, 0, 0, 0); /* idx6 middle exit */
    basic(buf, &o, 0xb7, 0, 6, 0, 0); /* idx7 inner r6 = 0 */
    basic(buf, &o, 0x95, 0, 0, 0, 0); /* idx8 inner exit */
    expect_ret(buf, o, 5, "nested calls restore outer r6");
}

static void test_depth_overrun(void)
{
    bpf_byte buf[32];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 0, 0);  /* idx0 */
    basic(buf, &o, 0x85, 1, 0, -1, 0); /* idx1 call imm -1 -> idx0 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);  /* idx2 exit (unreachable) */
    expect_trap(buf, o, "call depth overrun traps");
}

int main(void)
{
    test_preserved_across_call();
    test_preserved_r9();
    test_return_value();
    test_nested_preserve();
    test_depth_overrun();
    if (fails)
    {
        (void)fprintf(stderr, "test_vm_abi: %d failures (%d cases)\n", fails,
                      cases);
        return 1;
    }
    (void)printf("ok: test_vm_abi (%d cases)\n", cases);
    return 0;
}
