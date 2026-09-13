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

static bpf_vm *new_vm(bpf_program *prog)
{
    bpf_region_cfg c[3];
    bpf_vm *vm;
    bpf_err e;

    regs3(c);
    e = bpf_vm_create(prog, c, 3, 10000, &vm);
    if (e != BPF_OK)
    {
        return 0;
    }
    return vm;
}

/* Step until a non-RUNNING state; return it. */
static bpf_vm_state run_until_stop(bpf_vm *vm)
{
    bpf_vm_state st;

    st = BPF_VM_RUNNING;
    while (st == BPF_VM_RUNNING)
    {
        st = bpf_vm_step(vm);
    }
    return st;
}

/* Program: r1=100; helper call 5; r0 += r1; exit. After a completion with
 * status S and cleared r1, r0 ends at S. */
static bpf_program *make_wait_prog(bpf_u32 *olen)
{
    bpf_byte buf[64];
    bpf_u32 o;
    bpf_program *p;

    o = 0;
    basic(buf, &o, 0xb7, 0, 1, 100, 0); /* r1 = 100 */
    basic(buf, &o, 0x85, 0, 0, 5, 0);   /* helper call 5 -> WAITING */
    basic(buf, &o, 0x0f, 1, 0, 0, 0);   /* r0 += r1 */
    basic(buf, &o, 0x95, 0, 0, 0, 0);   /* exit */
    p = load_prog(buf, o);
    *olen = o;
    return p;
}

static void test_complete_resumes(void)
{
    bpf_u32 o;
    bpf_program *prog;
    bpf_vm *vm;
    bpf_vm_state st;
    bpf_err e;

    prog = make_wait_prog(&o);
    cases = cases + 1;
    ck(prog != 0, "wait program loads");
    if (prog == 0)
    {
        return;
    }
    vm = new_vm(prog);
    ck(vm != 0, "vm created");
    if (vm == 0)
    {
        bpf_program_destroy(prog);
        return;
    }
    st = run_until_stop(vm);
    ck(st == BPF_VM_WAITING, "machine waits on the helper");
    e = bpf_vm_complete(vm, 7);
    ck(e == BPF_OK, "completion accepted");
    ck(bpf_vm_get_state(vm) == BPF_VM_RUNNING,
       "machine resumed after completion");
    st = run_until_stop(vm);
    ck(st == BPF_VM_RETURNED && bpf_vm_reg(vm, 0) == 7,
       "r0 equals completion status and r1 was cleared");
    bpf_vm_destroy(vm);
    bpf_program_destroy(prog);
}

static void test_complete_running_rejected(void)
{
    bpf_byte buf[32];
    bpf_u32 o;
    bpf_program *prog;
    bpf_vm *vm;
    bpf_err e;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 5, 0);
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    cases = cases + 1;
    prog = load_prog(buf, o);
    ck(prog != 0, "return program loads");
    if (prog == 0)
    {
        return;
    }
    vm = new_vm(prog);
    ck(vm != 0, "vm created (running)");
    if (vm == 0)
    {
        bpf_program_destroy(prog);
        return;
    }
    /* state RUNNING, not waiting */
    e = bpf_vm_complete(vm, 1);
    cases = cases + 1;
    ck(e == BPF_ESTALE, "completing a running machine is stale");
    ck(bpf_vm_get_state(vm) == BPF_VM_RUNNING, "state unchanged on reject");
    bpf_vm_destroy(vm);
    bpf_program_destroy(prog);
}

static void test_complete_returned_rejected(void)
{
    bpf_byte buf[32];
    bpf_u32 o;
    bpf_program *prog;
    bpf_vm *vm;
    bpf_vm_state st;
    bpf_err e;

    o = 0;
    basic(buf, &o, 0xb7, 0, 0, 5, 0);
    basic(buf, &o, 0x95, 0, 0, 0, 0);
    cases = cases + 1;
    prog = load_prog(buf, o);
    ck(prog != 0, "returned program loads");
    if (prog == 0)
    {
        return;
    }
    vm = new_vm(prog);
    if (vm == 0)
    {
        bpf_program_destroy(prog);
        return;
    }
    st = run_until_stop(vm);
    ck(st == BPF_VM_RETURNED, "machine returned");
    e = bpf_vm_complete(vm, 1);
    cases = cases + 1;
    ck(e == BPF_ESTALE, "completing a returned machine is stale");
    ck(bpf_vm_get_state(vm) == BPF_VM_RETURNED && bpf_vm_reg(vm, 0) == 5,
       "rejected completion leaves state and r0 unchanged");
    bpf_vm_destroy(vm);
    bpf_program_destroy(prog);
}

static void test_duplicate_completion_rejected(void)
{
    bpf_u32 o;
    bpf_program *prog;
    bpf_vm *vm;
    bpf_vm_state st;
    bpf_err e;

    prog = make_wait_prog(&o);
    cases = cases + 1;
    ck(prog != 0, "wait program loads (dup)");
    if (prog == 0)
    {
        return;
    }
    vm = new_vm(prog);
    if (vm == 0)
    {
        bpf_program_destroy(prog);
        return;
    }
    st = run_until_stop(vm);
    ck(st == BPF_VM_WAITING, "waits (dup)");
    e = bpf_vm_complete(vm, 3);
    ck(e == BPF_OK, "first completion ok");
    e = bpf_vm_complete(vm, 4);
    cases = cases + 1;
    ck(e == BPF_ESTALE, "second completion of the same request is stale");
    bpf_vm_destroy(vm);
    bpf_program_destroy(prog);
}

int main(void)
{
    test_complete_resumes();
    test_complete_running_rejected();
    test_complete_returned_rejected();
    test_duplicate_completion_rejected();
    if (fails)
    {
        (void)fprintf(stderr, "test_vm_complete: %d failures (%d cases)\n",
                      fails, cases);
        return 1;
    }
    (void)printf("ok: test_vm_complete (%d cases)\n", cases);
    return 0;
}
