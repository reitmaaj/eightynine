#include <stdio.h>

#include "eval.h"

static int check(int cond, const char *msg)
{
    if (!cond)
    {
        (void)fprintf(stderr, "FAIL: %s\n", msg);
        return 1;
    }
    return 0;
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

static int test_ja_jmp(void)
{
    bpf_insn prog[8];
    bpf_machine m;
    bpf_status st;
    int fail;
    int i;

    fail = 0;
    for (i = 0; i < 8; ++i)
    {
        prog[i] = mk(0xb7, 0, (bpf_byte)i, 0, 0);
    }
    prog[0] = mk(0x05, 0, 0, 0, 2); /* JA JMP offset 2 */
    bpf_machine_init(&m, prog, 8);
    st = bpf_step(&m);
    fail |= check(st == BPF_STAT_RUNNING, "ja: running");
    fail |= check(m.pc == 3, "ja: pc 3");
    return fail;
}
static int test_ja_jmp32(void)
{
    bpf_insn prog[8];
    bpf_machine m;
    bpf_status st;
    int fail;
    int i;

    fail = 0;
    for (i = 0; i < 8; ++i)
    {
        prog[i] = mk(0xb7, 0, (bpf_byte)i, 0, 0);
    }
    prog[0] = mk(0x06, 0, 0, 5, 0); /* JA JMP32 imm 5 */
    bpf_machine_init(&m, prog, 8);
    st = bpf_step(&m);
    fail |= check(st == BPF_STAT_RUNNING, "ja32: running");
    fail |= check(m.pc == 6, "ja32: pc 6");
    return fail;
}

static int test_jeq(void)
{
    bpf_insn prog[8];
    bpf_machine m;
    bpf_status st;
    int fail;
    int i;

    fail = 0;
    for (i = 0; i < 8; ++i)
    {
        prog[i] = mk(0xb7, 0, (bpf_byte)i, 0, 0);
    }
    prog[0] = mk(0x15, 0, 1, 5, 3); /* JEQ K r1, imm 5, offset 3 */
    bpf_machine_init(&m, prog, 8);
    m.regs.r[1] = 5UL;
    st = bpf_step(&m);
    fail |= check(st == BPF_STAT_RUNNING, "jeq: running");
    fail |= check(m.pc == 4, "jeq: equal branches to 4");

    bpf_machine_init(&m, prog, 8);
    m.regs.r[1] = 9UL;
    st = bpf_step(&m);
    fail |= check(st == BPF_STAT_RUNNING, "jeq: running2");
    fail |= check(m.pc == 1, "jeq: unequal falls through");
    return fail;
}

static int test_jmp32_signed(void)
{
    bpf_insn prog[8];
    bpf_machine m;
    bpf_status st;
    int fail;
    int i;

    fail = 0;
    for (i = 0; i < 8; ++i)
    {
        prog[i] = mk(0xb7, 0, (bpf_byte)i, 0, 0);
    }
    prog[0] = mk(0xc6, 2, 1, 0, 2); /* JSLT X JMP32: (i32)r1 < (i32)r2 */
    bpf_machine_init(&m, prog, 8);
    m.regs.r[1] = 0xFFFFFFFFUL; /* -1 as i32 */
    m.regs.r[2] = 1UL;
    st = bpf_step(&m);
    fail |= check(st == BPF_STAT_RUNNING, "jslt: running");
    fail |= check(m.pc == 3, "jslt: signed branch taken");
    return fail;
}

static int test_exit(void)
{
    bpf_insn prog[2];
    bpf_machine m;
    bpf_status st;
    int fail;

    fail = 0;
    prog[0] = mk(0xb7, 0, 0, 42, 0); /* MOV64 K r0, 42 */
    prog[1] = mk(0x95, 0, 0, 0, 0);  /* EXIT */
    bpf_machine_init(&m, prog, 2);
    st = bpf_step(&m);
    fail |= check(st == BPF_STAT_RUNNING, "exit: first running");
    st = bpf_step(&m);
    fail |= check(st == BPF_STAT_RETURNED, "exit: returned");
    fail |= check(m.regs.r[0] == 42UL, "exit: r0 == 42");
    return fail;
}

static int test_call_local(void)
{
    bpf_insn prog[4];
    bpf_machine m;
    bpf_status st;
    int fail;
    int i;

    fail = 0;
    for (i = 0; i < 4; ++i)
    {
        prog[i] = mk(0xb7, 0, (bpf_byte)i, 0, 0);
    }
    prog[0] = mk(0x85, 1, 0, 1, 0); /* CALL src1 imm 1 -> target pc 2 */
    prog[2] = mk(0x95, 0, 0, 0, 0); /* EXIT */
    bpf_machine_init(&m, prog, 4);
    st = bpf_step(&m); /* CALL */
    fail |= check(st == BPF_STAT_RUNNING, "call: running");
    fail |= check(m.pc == 2, "call: jumped to 2");
    fail |= check(m.stack_n == 1, "call: pushed return");
    st = bpf_step(&m); /* EXIT */
    fail |= check(st == BPF_STAT_RUNNING, "call: exit returns");
    fail |= check(m.pc == 1, "call: returned to pc 1");
    return fail;
}

static int test_helper_call(void)
{
    bpf_insn prog[2];
    bpf_machine m;
    bpf_status st;
    int fail;

    fail = 0;
    prog[0] = mk(0x85, 0, 0, 7, 0); /* CALL src0 helper 7 */
    prog[1] = mk(0x95, 0, 0, 0, 0); /* EXIT */
    bpf_machine_init(&m, prog, 2);
    st = bpf_step(&m);
    fail |= check(st == BPF_STAT_HOSTCALL, "helper: hostcall");
    fail |= check(m.helper == 7, "helper: id 7");
    fail |= check(m.pc == 1, "helper: pc past call");
    return fail;
}

static int test_exhausted(void)
{
    bpf_insn prog[1];
    bpf_machine m;
    bpf_status st;
    int i;

    prog[0] = mk(0x05, 0, 0, 0, -1); /* JA offset -1 => jump to self */
    bpf_machine_init(&m, prog, 1);
    m.budget = 10;
    st = BPF_STAT_RUNNING;
    i = 0;
    while (st == BPF_STAT_RUNNING)
    {
        st = bpf_step(&m);
        i = i + 1;
    }
    if (st != BPF_STAT_EXHAUSTED)
    {
        (void)fprintf(stderr, "FAIL: expected EXHAUSTED, got %d\n", (int)st);
        return 1;
    }
    if (i != 11)
    {
        (void)fprintf(stderr, "FAIL: exhausted after %d steps\n", i);
        return 1;
    }
    return 0;
}

static int test_falloff(void)
{
    bpf_insn prog[1];
    bpf_machine m;
    bpf_status st;

    prog[0] = mk(0xb7, 0, 0, 0, 0); /* MOV64 r0 (only instruction) */
    bpf_machine_init(&m, prog, 1);
    st = bpf_step(&m);
    if (st != BPF_STAT_RUNNING)
    {
        (void)fprintf(stderr, "FAIL: first step not running\n");
        return 1;
    }
    st = bpf_step(&m);
    if (st != BPF_STAT_ERR)
    {
        (void)fprintf(stderr, "FAIL: expected ERR falling off end\n");
        return 1;
    }
    return 0;
}

int main(void)
{
    int fail;

    fail = 0;
    fail |= test_ja_jmp();
    fail |= test_ja_jmp32();
    fail |= test_jeq();
    fail |= test_jmp32_signed();
    fail |= test_exit();
    fail |= test_call_local();
    fail |= test_helper_call();
    fail |= test_exhausted();
    fail |= test_falloff();
    if (fail)
    {
        (void)fprintf(stderr, "test_flow: FAILED\n");
        return 1;
    }
    (void)printf("ok: test_flow\n");
    return 0;
}
