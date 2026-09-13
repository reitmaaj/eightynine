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

#define RSTREAM 1u
#define RMEM 2u
#define ACC_R 0x01u
#define ACC_W 0x02u

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

/* Program: mov r1, <handle>; helper call <id> (src 0); exit. */
static void prog_import(bpf_byte *buf, bpf_u32 *o, int handle, int id)
{
    basic(buf, o, 0xb7, 0, 1, handle, 0);
    basic(buf, o, 0x85, 0, 0, id, 0);
    basic(buf, o, 0x95, 0, 0, 0, 0);
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

/* Run a program that performs a helper call under an optional capability gate
 * and optional registered capability. Returns the terminal state. */
static bpf_vm_state run_import(bpf_program *prog, int do_cap, bpf_u32 ctype,
                               bpf_u32 crights, int revoke, int do_gate,
                               bpf_u32 gtype, bpf_u32 grights, bpf_u32 *pending)
{
    bpf_region_cfg c[3];
    bpf_vm *vm;
    bpf_vm_state st;
    bpf_err e;
    bpf_handle h;

    regs3(c);
    e = bpf_vm_create(prog, c, 3, 1000, &vm);
    if (e != BPF_OK)
    {
        return BPF_VM_TRAPPED;
    }
    if (do_cap)
    {
        e = bpf_vm_reg_cap(vm, ctype, crights, (void *)0x1, &h);
        if (e != BPF_OK)
        {
            bpf_vm_destroy(vm);
            return BPF_VM_TRAPPED;
        }
        if (revoke)
        {
            bpf_vm_revoke_cap(vm, h);
        }
    }
    if (do_gate)
    {
        bpf_vm_set_import_gate(vm, gtype, grights);
    }
    st = BPF_VM_RUNNING;
    while (st == BPF_VM_RUNNING)
    {
        st = bpf_vm_step(vm);
    }
    *pending = bpf_vm_pending_import(vm);
    bpf_vm_destroy(vm);
    return st;
}

static void test_valid_cap_suspends(void)
{
    bpf_byte buf[32];
    bpf_program *prog;
    bpf_vm_state st;
    bpf_u32 pending;
    bpf_u32 o;

    o = 0;
    prog_import(buf, &o, 0, 5);
    cases = cases + 1;
    prog = load_prog(buf, o);
    ck(prog != 0, "import program loads");
    if (prog == 0)
    {
        return;
    }
    pending = 0;
    st = run_import(prog, 1, RSTREAM, ACC_R, 0, 1, RSTREAM, ACC_R, &pending);
    ck(st == BPF_VM_WAITING && pending == 5,
       "satisfying capability lets helper suspend with its id");
    bpf_program_destroy(prog);
}

static void test_wrong_type_traps(void)
{
    bpf_byte buf[32];
    bpf_program *prog;
    bpf_vm_state st;
    bpf_u32 pending;
    bpf_u32 o;

    o = 0;
    prog_import(buf, &o, 0, 5);
    cases = cases + 1;
    prog = load_prog(buf, o);
    ck(prog != 0, "wrong-type program loads");
    if (prog == 0)
    {
        return;
    }
    pending = 0;
    st = run_import(prog, 1, RMEM, ACC_R, 0, 1, RSTREAM, ACC_R, &pending);
    ck(st == BPF_VM_TRAPPED, "capability of the wrong type traps");
    bpf_program_destroy(prog);
}

static void test_missing_right_traps(void)
{
    bpf_byte buf[32];
    bpf_program *prog;
    bpf_vm_state st;
    bpf_u32 pending;
    bpf_u32 o;

    o = 0;
    prog_import(buf, &o, 0, 5);
    cases = cases + 1;
    prog = load_prog(buf, o);
    ck(prog != 0, "rights program loads");
    if (prog == 0)
    {
        return;
    }
    pending = 0;
    st = run_import(prog, 1, RSTREAM, ACC_W, 0, 1, RSTREAM, ACC_R, &pending);
    ck(st == BPF_VM_TRAPPED, "capability lacking READ traps");
    bpf_program_destroy(prog);
}

static void test_revoked_handle_traps(void)
{
    bpf_byte buf[32];
    bpf_program *prog;
    bpf_vm_state st;
    bpf_u32 pending;
    bpf_u32 o;

    o = 0;
    prog_import(buf, &o, 0, 5);
    cases = cases + 1;
    prog = load_prog(buf, o);
    ck(prog != 0, "revoked program loads");
    if (prog == 0)
    {
        return;
    }
    pending = 0;
    st = run_import(prog, 1, RSTREAM, ACC_R, 1, 1, RSTREAM, ACC_R, &pending);
    ck(st == BPF_VM_TRAPPED, "revoked capability traps");
    bpf_program_destroy(prog);
}

static void test_no_registered_cap_traps(void)
{
    bpf_byte buf[32];
    bpf_program *prog;
    bpf_vm_state st;
    bpf_u32 pending;
    bpf_u32 o;

    o = 0;
    prog_import(buf, &o, 0, 5);
    cases = cases + 1;
    prog = load_prog(buf, o);
    ck(prog != 0, "no-registered program loads");
    if (prog == 0)
    {
        return;
    }
    pending = 0;
    st = run_import(prog, 0, 0, 0, 0, 1, RSTREAM, ACC_R, &pending);
    ck(st == BPF_VM_TRAPPED, "gate with no registered capability traps");
    bpf_program_destroy(prog);
}

static void test_ungated_suspends(void)
{
    bpf_byte buf[32];
    bpf_program *prog;
    bpf_vm_state st;
    bpf_u32 pending;
    bpf_u32 o;

    o = 0;
    prog_import(buf, &o, 123, 9); /* r1 arbitrary; no gate declared */
    cases = cases + 1;
    prog = load_prog(buf, o);
    ck(prog != 0, "ungated program loads");
    if (prog == 0)
    {
        return;
    }
    pending = 0;
    st = run_import(prog, 0, 0, 0, 0, 0, 0, 0, &pending);
    ck(st == BPF_VM_WAITING && pending == 9,
       "ungated helper suspends regardless of r1");
    bpf_program_destroy(prog);
}

static void test_permission_superset_ok(void)
{
    bpf_byte buf[32];
    bpf_program *prog;
    bpf_vm_state st;
    bpf_u32 pending;
    bpf_u32 o;

    o = 0;
    prog_import(buf, &o, 0, 3);
    cases = cases + 1;
    prog = load_prog(buf, o);
    ck(prog != 0, "superset program loads");
    if (prog == 0)
    {
        return;
    }
    pending = 0;
    st = run_import(prog, 1, RSTREAM, ACC_R | ACC_W, 0, 1, RSTREAM, ACC_R,
                    &pending);
    ck(st == BPF_VM_WAITING && pending == 3,
       "capability granting more than required still suspends");
    bpf_program_destroy(prog);
}

static void test_multi_bit_require_traps(void)
{
    bpf_byte buf[32];
    bpf_program *prog;
    bpf_vm_state st;
    bpf_u32 pending;
    bpf_u32 o;

    o = 0;
    prog_import(buf, &o, 0, 4);
    cases = cases + 1;
    prog = load_prog(buf, o);
    ck(prog != 0, "multibit program loads");
    if (prog == 0)
    {
        return;
    }
    pending = 0;
    st = run_import(prog, 1, RSTREAM, ACC_R, 0, 1, RSTREAM, ACC_R | ACC_W,
                    &pending);
    ck(st == BPF_VM_TRAPPED, "requiring READ|WRITE with READ only traps");
    bpf_program_destroy(prog);
}
int main(void)
{
    test_valid_cap_suspends();
    test_wrong_type_traps();
    test_missing_right_traps();
    test_revoked_handle_traps();
    test_no_registered_cap_traps();
    test_ungated_suspends();
    test_permission_superset_ok();
    test_multi_bit_require_traps();
    if (fails)
    {
        (void)fprintf(stderr, "test_vm_import: %d failures (%d cases)\n", fails,
                      cases);
        return 1;
    }
    (void)printf("ok: test_vm_import (%d cases)\n", cases);
    return 0;
}
