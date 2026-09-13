#include <stdio.h>

#include "eval.h"

static int fails;

static void ck(int cond, const char *msg)
{
    if (!cond)
    {
        (void)fprintf(stderr, "FAIL: %s\n", msg);
        fails = fails + 1;
    }
}

static bpf_insn mk(bpf_byte op, bpf_byte src, bpf_byte dst, int imm, int off)
{
    bpf_insn i;

    i.opcode = op;
    i.class = (bpf_byte)(op & 0x07u);
    i.code = (bpf_byte)(op >> 4);
    i.source = (bpf_byte)((op >> 3) & 0x01u);
    i.mode = 0;
    i.size = 0;
    i.src_reg = src;
    i.dst_reg = dst;
    i.offset = off;
    i.imm = imm;
    i.is_wide = 0;
    i.next_imm = 0;
    return i;
}

static void test_helper_calls(void)
{
    bpf_insn prog[2];
    bpf_machine m;
    bpf_status st;

    prog[0] = mk(0x85, 0, 0, 7, 0); /* CALL src0 helper 7 */
    prog[1] = mk(0x95, 0, 0, 0, 0);
    bpf_machine_init(&m, prog, 2);
    st = bpf_step(&m);
    ck(st == BPF_STAT_HOSTCALL && m.helper == 7 && m.pc == 1,
       "helper static id");

    prog[0] = mk(0x85, 2, 0, 99, 0); /* CALL src2 BTF helper 99 */
    bpf_machine_init(&m, prog, 2);
    st = bpf_step(&m);
    ck(st == BPF_STAT_HOSTCALL && m.helper == 99, "helper BTF id");
}

static void test_local_call_return(void)
{
    bpf_insn prog[6];
    bpf_machine m;
    bpf_status st;
    int i;

    for (i = 0; i < 6; ++i)
    {
        prog[i] = mk(0xb7, 0, (bpf_byte)i, 0, 0);
    }
    prog[0] = mk(0x85, 1, 0, 3, 0);  /* CALL local to pc 4 */
    prog[4] = mk(0xb7, 0, 1, 77, 0); /* r1 = 77 */
    prog[5] = mk(0x95, 0, 0, 0, 0);  /* EXIT returns to pc 1 */
    bpf_machine_init(&m, prog, 6);
    st = bpf_step(&m); /* CALL */
    ck(st == BPF_STAT_RUNNING && m.pc == 4, "local call jumps to 4");
    st = bpf_step(&m); /* r1=77 */
    ck(st == BPF_STAT_RUNNING && m.regs.r[1] == 77, "local body runs");
    st = bpf_step(&m); /* EXIT */
    ck(st == BPF_STAT_RUNNING && m.pc == 1, "local exit returns to 1");
}

static void test_nested_calls(void)
{
    bpf_insn prog[8];
    bpf_machine m;
    bpf_status st;
    int i;

    for (i = 0; i < 8; ++i)
    {
        prog[i] = mk(0xb7, 0, (bpf_byte)i, 0, 0);
    }
    prog[0] = mk(0x85, 1, 0, 3, 0); /* CALL -> 4 */
    prog[4] = mk(0x85, 1, 0, 1, 0); /* CALL -> 6 */
    prog[6] = mk(0x95, 0, 0, 0, 0); /* EXIT -> back to 5 */
    prog[5] = mk(0x95, 0, 0, 0, 0); /* EXIT -> back to 1 */
    bpf_machine_init(&m, prog, 8);
    st = bpf_step(&m); /* CALL->4, push 1 */
    ck(st == BPF_STAT_RUNNING && m.pc == 4 && m.stack_n == 1, "nested call1");
    st = bpf_step(&m); /* CALL->6, push 5 */
    ck(st == BPF_STAT_RUNNING && m.pc == 6 && m.stack_n == 2, "nested call2");
    st = bpf_step(&m); /* EXIT -> pop to 5 */
    ck(st == BPF_STAT_RUNNING && m.pc == 5 && m.stack_n == 1, "nested exit1");
    st = bpf_step(&m); /* EXIT -> pop to 1 */
    ck(st == BPF_STAT_RUNNING && m.pc == 1 && m.stack_n == 0, "nested exit2");
}

static void test_stack_overflow(void)
{
    bpf_insn prog[2];
    bpf_machine m;
    bpf_status st;
    int i;

    prog[0] = mk(0x85, 1, 0, 0, 0);  /* CALL local to self (pc+1+0 = 1) */
    prog[1] = mk(0x85, 1, 0, -1, 0); /* CALL local back to 0 */
    bpf_machine_init(&m, prog, 2);
    st = BPF_STAT_RUNNING;
    for (i = 0; i < 200; ++i)
    {
        st = bpf_step(&m);
        if (st == BPF_STAT_ERR)
        {
            break;
        }
    }
    ck(st == BPF_STAT_ERR, "stack overflow -> ERR");
}

static void test_budget_boundary(void)
{
    bpf_insn prog[2];
    bpf_machine m;
    bpf_status st;

    prog[0] = mk(0x95, 0, 0, 0, 0); /* EXIT immediately */
    prog[1] = mk(0x95, 0, 0, 0, 0);
    bpf_machine_init(&m, prog, 2);
    m.budget = 1;
    st = bpf_step(&m);
    ck(st == BPF_STAT_RETURNED, "budget 1 runs one then returns");
}

int main(void)
{
    test_helper_calls();
    test_local_call_return();
    test_nested_calls();
    test_stack_overflow();
    test_budget_boundary();
    if (fails)
    {
        (void)fprintf(stderr, "test_call_exit: %d failures\n", fails);
        return 1;
    }
    (void)printf("ok: test_call_exit\n");
    return 0;
}
