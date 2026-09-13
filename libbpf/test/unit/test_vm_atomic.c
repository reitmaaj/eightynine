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

static void regs4(bpf_region_cfg *c)
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

    regs4(c);
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
    st = run_prog(prog, 10000, &r0);
    ck(st == BPF_VM_TRAPPED, name);
    bpf_program_destroy(prog);
}

/* Pre-store value V at [r1+0] then run the given continuation. */
static void with_store(bpf_byte *buf, bpf_u32 *o, int v, int dw)
{
    bpf_u32 opst;

    basic(buf, o, 0xb7, 0, 1, 0x100000, 0);
    basic(buf, o, 0xb7, 0, 2, v, 0);
    opst = 0x63;
    if (dw)
    {
        opst = 0x7b;
    }
    basic(buf, o, opst, 2, 1, 0, 0);
}

static void test_atomic_add_w(void)
{
    bpf_byte buf[96];
    bpf_u32 o;

    o = 0;
    with_store(buf, &o, 5, 0);
    basic(buf, &o, 0xb7, 0, 2, 3, 0);    /* operand 3 */
    basic(buf, &o, 0xc3, 2, 1, 0x00, 0); /* atomic ADD W mem += 3 */
    basic(buf, &o, 0x61, 1, 0, 0, 0);    /* r0 = mem */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_ret(buf, o, 8, "atomic add word in place");
}

static void test_atomic_add_dw(void)
{
    bpf_byte buf[96];
    bpf_u32 o;

    o = 0;
    with_store(buf, &o, 5, 1);
    basic(buf, &o, 0xb7, 0, 2, 3, 0);
    basic(buf, &o, 0xdb, 2, 1, 0x00, 0); /* atomic ADD DW */
    basic(buf, &o, 0x79, 1, 0, 0, 0);
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_ret(buf, o, 8, "atomic add double-word in place");
}

static void test_atomic_or_and_xor_w(void)
{
    bpf_byte buf[96];
    bpf_u32 o;

    o = 0;
    with_store(buf, &o, 0x0F, 0);
    basic(buf, &o, 0xb7, 0, 2, 0x30, 0);
    basic(buf, &o, 0xc3, 2, 1, 0x40, 0); /* OR */
    basic(buf, &o, 0x61, 1, 0, 0, 0);
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_ret(buf, o, 0x3F, "atomic or word");

    o = 0;
    with_store(buf, &o, 0x3F, 0);
    basic(buf, &o, 0xb7, 0, 2, 0x0F, 0);
    basic(buf, &o, 0xc3, 2, 1, 0x50, 0); /* AND */
    basic(buf, &o, 0x61, 1, 0, 0, 0);
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_ret(buf, o, 0x0F, "atomic and word");

    o = 0;
    with_store(buf, &o, 0xFF, 0);
    basic(buf, &o, 0xb7, 0, 2, 0x0F, 0);
    basic(buf, &o, 0xc3, 2, 1, 0xA0, 0); /* XOR */
    basic(buf, &o, 0x61, 1, 0, 0, 0);
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_ret(buf, o, 0xF0, "atomic xor word");
}

static void test_atomic_fetch_returns_old(void)
{
    bpf_byte buf[96];
    bpf_u32 o;

    o = 0;
    with_store(buf, &o, 5, 0);
    basic(buf, &o, 0xb7, 0, 2, 3, 0);
    basic(buf, &o, 0xc3, 2, 1, 0x01, 0); /* atomic fetch ADD W */
    basic(buf, &o, 0xbf, 2, 0, 0, 0);    /* r0 = r2 (old) */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_ret(buf, o, 5, "fetch atomic returns old value in source");
}

static void test_atomic_xchg(void)
{
    bpf_byte buf[96];
    bpf_u32 o;

    o = 0;
    with_store(buf, &o, 7, 0);
    basic(buf, &o, 0xb7, 0, 2, 9, 0);
    basic(buf, &o, 0xc3, 2, 1, 0xE1, 0); /* XCHG W (fetch) */
    basic(buf, &o, 0xbf, 2, 0, 0, 0);    /* r0 = r2 = old 7 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_ret(buf, o, 7, "xchg returns old and stores operand");
}

static void test_atomic_cmpxchg_match(void)
{
    bpf_byte buf[96];
    bpf_u32 o;

    o = 0;
    with_store(buf, &o, 7, 0);
    basic(buf, &o, 0xb7, 0, 0, 7, 0); /* expected r0 = 7 */
    basic(buf, &o, 0xb7, 0, 2, 9, 0);
    basic(buf, &o, 0xc3, 2, 1, 0xF0, 0); /* CMPXCHG W */
    basic(buf, &o, 0x61, 1, 0, 0, 0);    /* r0 = mem = 9 (stored) */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_ret(buf, o, 9, "cmpxchg stores when equal");
}

static void test_atomic_cmpxchg_no_match(void)
{
    bpf_byte buf[96];
    bpf_u32 o;

    o = 0;
    with_store(buf, &o, 7, 0);
    basic(buf, &o, 0xb7, 0, 0, 8, 0); /* expected r0 = 8 (mismatch) */
    basic(buf, &o, 0xb7, 0, 2, 9, 0);
    basic(buf, &o, 0xc3, 2, 1, 0xF0, 0); /* CMPXCHG W */
    basic(buf, &o, 0x61, 1, 0, 0, 0);    /* r0 = mem = 7 (unchanged) */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_ret(buf, o, 7, "cmpxchg leaves memory on mismatch");
}

static void test_atomic_word_truncation(void)
{
    bpf_byte buf[96];
    bpf_u32 o;

    o = 0;
    with_store(buf, &o, -1, 1); /* mem dw = all ones */
    basic(buf, &o, 0xb7, 0, 2, 1, 0);
    basic(buf, &o, 0xc3, 2, 1, 0x00, 0); /* atomic ADD W (low word += 1) */
    basic(buf, &o, 0x79, 1, 0, 0, 0);    /* r0 = mem dw */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_ret(buf, o, 0xFFFFFFFF00000000UL,
               "word atomic touches low word only");
}

static void test_atomic_unmapped_traps(void)
{
    bpf_byte buf[96];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 1, 0x300000, 0); /* unmapped base */
    basic(buf, &o, 0xb7, 0, 2, 1, 0);
    basic(buf, &o, 0xc3, 2, 1, 0x00, 0);
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_trap(buf, o, "atomic over unmapped address traps");
}

static void test_atomic_readonly_traps(void)
{
    bpf_byte buf[96];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 1, 0x1000, 0); /* input (read-only) base */
    basic(buf, &o, 0xb7, 0, 2, 1, 0);
    basic(buf, &o, 0xc3, 2, 1, 0x00, 0);
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_trap(buf, o, "atomic over read-only region traps");
}

int main(void)
{
    test_atomic_add_w();
    test_atomic_add_dw();
    test_atomic_or_and_xor_w();
    test_atomic_fetch_returns_old();
    test_atomic_xchg();
    test_atomic_cmpxchg_match();
    test_atomic_cmpxchg_no_match();
    test_atomic_word_truncation();
    test_atomic_unmapped_traps();
    test_atomic_readonly_traps();
    if (fails)
    {
        (void)fprintf(stderr, "test_vm_atomic: %d failures (%d cases)\n", fails,
                      cases);
        return 1;
    }
    (void)printf("ok: test_vm_atomic (%d cases)\n", cases);
    return 0;
}
