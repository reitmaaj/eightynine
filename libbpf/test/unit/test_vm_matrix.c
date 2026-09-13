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

static void expect_r0(bpf_byte *buf, bpf_u32 len, bpf_u64 want,
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
    st = run_prog(prog, 1000, &r0);
    ck(st == BPF_VM_RETURNED && r0 == want, name);
    bpf_program_destroy(prog);
}

/* Build: mov r1,C; <cond op on r1 against I> ; mov r0,100 ; exit. The
 * fall-through leaves r0=100; a taken branch (off +1) lands on exit with
 * r0 left at its previous value (we pre-set r0 via an extra param). */
static void cond_probe(bpf_u32 op, int const1, int imm, int want,
                       const char *name)
{
    bpf_byte buf[64];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 7, 0);      /* r0 = 7 */
    basic(buf, &o, 0xb7, 0, 1, const1, 0); /* r1 = const1 */
    basic(buf, &o, op, 0, 1, imm, 1);      /* cond r1, imm -> +1 */
    basic(buf, &o, 0xb7, 0, 0, 100, 0);    /* fall-through */
    basic(buf, &o, 0x95, 0, 0, 0, 0);      /* exit (taken target) */
    if (want == 100)
    {
        expect_r0(buf, o, 100, name);
    }
    else
    {
        expect_r0(buf, o, 7, name);
    }
}

static void test_cond_matrix(void)
{
    /* jeq r1,5 ; r1=5 -> taken (7); r1=6 -> not (100) */
    cond_probe(0x15, 5, 5, 7, "jeq equal taken");
    cond_probe(0x15, 6, 5, 100, "jeq unequal not taken");
    /* jne */
    cond_probe(0x55, 6, 5, 7, "jne unequal taken");
    cond_probe(0x55, 5, 5, 100, "jne equal not taken");
    /* jgt unsigned */
    cond_probe(0x25, 7, 5, 7, "jgt greater taken");
    cond_probe(0x25, 3, 5, 100, "jgt smaller not taken");
    /* jge */
    cond_probe(0x35, 5, 5, 7, "jge equal taken");
    cond_probe(0x35, 4, 5, 100, "jge smaller not taken");
    /* jlt */
    cond_probe(0xa5, 3, 5, 7, "jlt smaller taken");
    cond_probe(0xa5, 7, 5, 100, "jlt greater not taken");
    /* jle */
    cond_probe(0xb5, 5, 5, 7, "jle equal taken");
    cond_probe(0xb5, 9, 5, 100, "jle greater not taken");
    /* jset */
    cond_probe(0x45, 0x08, 0x08, 7, "jset bit set taken");
    cond_probe(0x45, 0x04, 0x08, 100, "jset bit clear not taken");
    /* jsgt signed */
    cond_probe(0x65, 3, 0, 7, "jsgt positive taken");
    cond_probe(0x65, -3, 0, 100, "jsgt negative not taken");
    /* jsge signed */
    cond_probe(0x75, 0, 0, 7, "jsge zero taken");
    /* jslt signed */
    cond_probe(0xc5, -3, 0, 7, "jslt negative taken");
    /* jsle */
    cond_probe(0xd5, -1, 0, 7, "jsle negative taken");
}

static void test_terminal_persistence(void)
{
    bpf_byte buf[32];
    bpf_region_cfg c[3];
    bpf_program *prog;
    bpf_vm *vm;
    bpf_vm_state st;
    bpf_err e;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 5, 0);
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    cases = cases + 1;
    prog = load_prog(buf, o);
    ck(prog != 0, "persistence program loads");
    if (prog == 0)
    {
        return;
    }
    regs3(c);
    e = bpf_vm_create(prog, c, 3, 100, &vm);
    ck(e == BPF_OK, "vm create");
    if (e != BPF_OK)
    {
        bpf_program_destroy(prog);
        return;
    }
    st = bpf_vm_step(vm);
    ck(st == BPF_VM_RUNNING, "first step running");
    st = bpf_vm_step(vm);
    ck(st == BPF_VM_RETURNED, "second step returns");
    st = bpf_vm_step(vm);
    ck(st == BPF_VM_RETURNED, "returned stays returned across steps");
    ck(bpf_vm_get_state(vm) == BPF_VM_RETURNED, "state reports returned");
    bpf_vm_destroy(vm);
    bpf_program_destroy(prog);
}

static void test_entry_context_regs(void)
{
    bpf_byte buf[32];
    bpf_region_cfg c[3];
    bpf_program *prog;
    bpf_vm *vm;
    bpf_err e;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0x95, 0, 0, 0, 0); /* exit immediately */
    cases = cases + 1;
    prog = load_prog(buf, o);
    ck(prog != 0, "entry program loads");
    if (prog == 0)
    {
        return;
    }
    regs3(c);
    e = bpf_vm_create(prog, c, 3, 100, &vm);
    ck(e == BPF_OK, "create for entry context");
    if (e == BPF_OK)
    {
        ck(bpf_vm_reg(vm, 1) == 0x1000, "r1 holds input (context) base");
        ck(bpf_vm_reg(vm, 2) == 0x40, "r2 holds input (context) length");
        ck(bpf_vm_reg(vm, 10) == 0xF0000, "r10 holds stack base");
        ck(bpf_vm_reg(vm, 0) == 0, "r0 starts zero");
        bpf_vm_destroy(vm);
    }
    bpf_program_destroy(prog);
}

static void test_budget_varied(void)
{
    bpf_byte buf[64];
    bpf_region_cfg c[3];
    bpf_program *prog;
    bpf_vm *vm;
    bpf_vm_state st;
    bpf_err e;
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0x05, 0, 0, 0, -1); /* infinite loop */
    cases = cases + 1;
    prog = load_prog(buf, o);
    ck(prog != 0, "loop loads for budget");
    if (prog == 0)
    {
        return;
    }
    regs3(c);
    e = bpf_vm_create(prog, c, 3, 3, &vm);
    ck(e == BPF_OK, "create small budget");
    if (e == BPF_OK)
    {
        st = bpf_vm_step(vm);
        ck(st == BPF_VM_RUNNING, "budget step 1");
        bpf_vm_set_budget(vm, 0);
        st = bpf_vm_step(vm);
        ck(st == BPF_VM_EXHAUSTED, "budget 0 exhausts");
        bpf_vm_destroy(vm);
    }
    bpf_program_destroy(prog);
}

static void test_wide_immediate_matrix(void)
{
    static const bpf_u32 his[4] = {0x0u, 0xFFFFFFFFu, 0x12345678u, 0x1u};
    static const bpf_u32 los[4] = {0x0u, 0xFFFFFFFFu, 0x9ABCDEF0u, 0x7u};
    bpf_u32 i;

    for (i = 0u; i < 4u; ++i)
    {
        bpf_byte buf[32];
        bpf_u32 o;
        bpf_u64 want;

        o = 0;
        wide(buf, &o, 0, los[i], his[i]);
        basic(buf, &o, 0x95, 0, 0, 0, 0);
        want = ((bpf_u64)his[i] << 32) | (bpf_u64)los[i];
        expect_r0(buf, o, want, "wide immediate matrix");
    }
}

static void test_mem_widths(void)
{
    bpf_u32 v;

    for (v = 0u; v < 4u; ++v)
    {
        bpf_u32 width;
        bpf_byte opld;
        bpf_byte buf[64];
        bpf_u32 o;
        bpf_u64 want;

        width = 1u;
        opld = 0x71;
        if (v == 1u)
        {
            width = 2u;
            opld = 0x69;
        }
        if (v == 2u)
        {
            width = 4u;
            opld = 0x61;
        }
        if (v == 3u)
        {
            width = 8u;
            opld = 0x79;
        }
        o = 0;
        basic(buf, &o, 0xb7, 0, 1, 0x100000, 0);
        basic(buf, &o, 0xb7, 0, 2, -1, 0); /* r2 = all ones */
        basic(buf, &o, 0x7b, 2, 1, 0, 0);  /* stx dw mem = -1 */
        basic(buf, &o, opld, 1, 0, 0, 0);  /* ldx width r0 */
        basic(buf, &o, 0x95, 0, 0, 0, 0);
        if (width == 8u)
        {
            want = (bpf_u64)0 - 1u;
        }
        else
        {
            bpf_u64 bits;
            bpf_u64 ones;

            bits = 8u * (bpf_u64)width;
            ones = ((bpf_u64)1 << bits) - 1u;
            want = ones;
        }
        expect_r0(buf, o, want, "load of each width returns low ones");
    }
}

static void test_add_sub_branch_math(void)
{
    bpf_byte buf[64];
    bpf_u32 o;

    o = 0;
    basic(buf, &o, 0xb7, 0, 1, 20, 0);
    basic(buf, &o, 0xb7, 0, 2, 22, 0);
    basic(buf, &o, 0xbf, 2, 3, 0, 0); /* r3 = r2 */
    basic(buf, &o, 0x1f, 1, 3, 0, 0); /* r3 = r3 - r1 = 2 */
    basic(buf, &o, 0xbf, 3, 0, 0, 0); /* r0 = r3 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    expect_r0(buf, o, 2, "register subtraction");
}

int main(void)
{
    test_cond_matrix();
    test_terminal_persistence();
    test_entry_context_regs();
    test_budget_varied();
    test_wide_immediate_matrix();
    test_mem_widths();
    test_add_sub_branch_math();
    if (fails)
    {
        (void)fprintf(stderr, "test_vm_matrix: %d failures (%d cases)\n", fails,
                      cases);
        return 1;
    }
    (void)printf("ok: test_vm_matrix (%d cases)\n", cases);
    return 0;
}
