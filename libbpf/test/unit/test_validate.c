#include <stdio.h>

#include "validate.h"

static int check(int cond, const char *msg)
{
    if (!cond)
    {
        (void)fprintf(stderr, "FAIL: %s\n", msg);
        return 1;
    }
    return 0;
}

static bpf_insn mk(bpf_byte opcode, bpf_byte src, bpf_byte dst, int imm)
{
    bpf_insn i;

    i.opcode = opcode;
    i.class = (bpf_byte)(opcode & 0x07u);
    i.src_reg = src;
    i.dst_reg = dst;
    i.offset = 0;
    i.imm = imm;
    i.is_wide = 0;
    i.next_imm = 0;
    i.code = (bpf_byte)(opcode >> 4);
    i.source = (bpf_byte)((opcode >> 3) & 0x01u);
    i.mode = (bpf_byte)(opcode >> 5);
    i.size = (bpf_byte)((opcode >> 3) & 0x03u);
    return i;
}

static int test_register_range(void)
{
    bpf_insn ins[1];
    bpf_u32 conf;
    bpf_err e;
    int fail;

    fail = 0;
    ins[0] = mk(0xb7, 0, 11, 0); /* MOV64 K r11 */
    e = bpf_validate(ins, 1, &conf);
    fail |= check(e == BPF_EREG, "reg: dst 11 rejected");

    ins[0] = mk(0xbf, 11, 0, 0); /* MOV64 X, src 11 */
    e = bpf_validate(ins, 1, &conf);
    fail |= check(e == BPF_EREG, "reg: src 11 rejected");

    ins[0] = mk(0xb7, 0, 10, 0); /* MOV64 K r10 (fp) */
    e = bpf_validate(ins, 1, &conf);
    fail |= check(e == BPF_OK, "reg: r10 accepted");
    return fail;
}

static int test_packet(void)
{
    bpf_insn ins[1];
    bpf_u32 conf;
    bpf_err e;
    int fail;

    fail = 0;
    ins[0] = mk(0x20, 0, 0, 0); /* mode ABS(1)<<5, size W(0), class LD */
    e = bpf_validate(ins, 1, &conf);
    fail |= check(e == BPF_EDEPRECATED, "packet: ABS rejected");
    fail |= check((conf & BPF_CONF_PACKET) != 0u, "packet: group set");
    return fail;
}

static int test_imm_subtypes(void)
{
    bpf_insn ins[1];
    bpf_u32 conf;
    bpf_err e;
    int fail;

    fail = 0;
    ins[0] = mk(0x18, 0, 1, 0); /* LD IMM DW, subtype 0 (pure imm) */
    ins[0].is_wide = 1;
    e = bpf_validate(ins, 1, &conf);
    fail |= check(e == BPF_OK, "imm: subtype 0 accepted");

    ins[0] = mk(0x18, 1, 1, 0); /* subtype 1 = map_by_fd */
    ins[0].is_wide = 1;
    e = bpf_validate(ins, 1, &conf);
    fail |= check(e == BPF_EIMM, "imm: subtype 1 rejected");
    return fail;
}

static int test_end_width(void)
{
    bpf_insn ins[1];
    bpf_u32 conf;
    bpf_err e;
    int fail;

    fail = 0;
    ins[0] = mk(0xdc, 0, 0, 32); /* ALU END (code 0xd, class 4), LE width 32 */
    e = bpf_validate(ins, 1, &conf);
    fail |= check(e == BPF_OK, "end: width 32 ok");

    ins[0] = mk(0xdc, 0, 0, 8); /* width 8 invalid */
    e = bpf_validate(ins, 1, &conf);
    fail |= check(e == BPF_EEND, "end: width 8 rejected");

    ins[0] = mk(0xdf, 0, 0, 32); /* ALU64 END with source bit set */
    e = bpf_validate(ins, 1, &conf);
    fail |= check(e == BPF_EEND, "end: ALU64 source bit rejected");
    return fail;
}

static int test_neg_movsx(void)
{
    bpf_insn ins[1];
    bpf_u32 conf;
    bpf_err e;
    int fail;

    fail = 0;
    ins[0] = mk(0xbf, 0, 1, 0); /* MOV64 X */
    e = bpf_validate(ins, 1, &conf);
    fail |= check(e == BPF_OK, "neg: K ok");

    ins[0] = mk(0x8f, 1, 1, 0); /* NEG X ALU64 */
    e = bpf_validate(ins, 1, &conf);
    fail |= check(e == BPF_ENEG, "neg: X rejected");

    ins[0] = mk(0xb7, 0, 1, 0); /* code MOV, source K, offset 8 => MOVSX K */
    ins[0].offset = 8;
    e = bpf_validate(ins, 1, &conf);
    fail |= check(e == BPF_EMOVSX, "movsx: K source rejected");
    return fail;
}

static int test_call_exit(void)
{
    bpf_insn ins[1];
    bpf_u32 conf;
    bpf_err e;
    int fail;

    fail = 0;
    ins[0] = mk(0x85, 0, 0, 0); /* CALL src 0 */
    e = bpf_validate(ins, 1, &conf);
    fail |= check(e == BPF_OK, "call: src0 ok");

    ins[0] = mk(0x85, 3, 0, 0); /* CALL src 3 (invalid) */
    e = bpf_validate(ins, 1, &conf);
    fail |= check(e == BPF_ECALL, "call: src3 rejected");

    ins[0] = mk(0x95, 0, 0, 0); /* EXIT */
    e = bpf_validate(ins, 1, &conf);
    fail |= check(e == BPF_OK, "exit: ok");

    ins[0] = mk(0x95, 0, 0, 5); /* EXIT with nonzero imm */
    e = bpf_validate(ins, 1, &conf);
    fail |= check(e == BPF_EEXIT, "exit: nonzero imm rejected");
    return fail;
}

static int test_conformance(void)
{
    bpf_insn ins[1];
    bpf_u32 conf;
    bpf_err e;
    int fail;

    fail = 0;
    ins[0] = mk(0xb4, 0, 0, 0); /* MOV32 ALU (class 4), base32 only */
    e = bpf_validate(ins, 1, &conf);
    fail |= check(e == BPF_OK, "conf: alu32 ok");
    fail |= check((conf & BPF_CONF_BASE32) != 0u, "conf: base32");
    fail |= check((conf & BPF_CONF_BASE64) == 0u, "conf: no base64");

    ins[0] = mk(0xb7, 0, 0, 0); /* MOV64 ALU64 (class 7) -> base64 */
    e = bpf_validate(ins, 1, &conf);
    fail |= check(e == BPF_OK, "conf: alu64 ok");
    fail |= check((conf & BPF_CONF_BASE64) != 0u, "conf: base64");

    ins[0] = mk(0x34, 0, 0, 0); /* code DIV K ALU (class 4) -> divmul32 */
    ins[0].code = BPF_ALU_DIV;
    e = bpf_validate(ins, 1, &conf);
    fail |= check((conf & BPF_CONF_DIVMUL32) != 0u, "conf: divmul32");

    ins[0] = mk(0x37, 0, 0, 0); /* ALU64 DIV -> divmul64 */
    ins[0].code = BPF_ALU_DIV;
    e = bpf_validate(ins, 1, &conf);
    fail |= check((conf & BPF_CONF_DIVMUL64) != 0u, "conf: divmul64");

    ins[0] =
        mk(0x71, 0, 0, 0); /* class LDX(1), MEM(3)<<5, B(2)<<3 = 0x71 base32 */
    e = bpf_validate(ins, 1, &conf);
    fail |= check((conf & BPF_CONF_BASE64) == 0u, "conf: b mem no base64");

    ins[0] = mk(0x79, 0, 0, 0); /* class LDX, MEM, DW(3)<<3 = 0x79 -> base64 */
    e = bpf_validate(ins, 1, &conf);
    fail |= check((conf & BPF_CONF_BASE64) != 0u, "conf: dw mem base64");
    return fail;
}

static int test_atomic(void)
{
    bpf_insn ins[1];
    bpf_u32 conf;
    bpf_err e;
    int fail;

    fail = 0;
    ins[0] = mk(0xc3, 0, 0, 0); /* ATOMIC(6)<<5, W(0), STX(3) = 0xc3 */
    e = bpf_validate(ins, 1, &conf);
    fail |= check(e == BPF_OK, "atomic: W ok");
    fail |= check((conf & BPF_CONF_ATOMIC32) != 0u, "atomic: atomic32");

    ins[0] = mk(0xdb, 0, 0, 0); /* ATOMIC(6)<<5, DW(3), STX(3) = 0xdb */
    e = bpf_validate(ins, 1, &conf);
    fail |= check(e == BPF_OK, "atomic: DW ok");
    fail |= check((conf & BPF_CONF_ATOMIC64) != 0u, "atomic: atomic64");
    return fail;
}

static int test_mem_size_modes(void)
{
    bpf_insn ins[1];
    bpf_u32 conf;
    bpf_err e;
    int fail;

    fail = 0;
    ins[0] = mk(0x03, 0, 0, 0); /* STX, mode IMM(0)<<5, class != LD */
    e = bpf_validate(ins, 1, &conf);
    fail |= check(e == BPF_ESIZE, "mem: IMM mode non-LD rejected");

    ins[0] = mk(0x61, 0, 0, 0); /* LDX MEM(3)<<5, W */
    e = bpf_validate(ins, 1, &conf);
    fail |= check(e == BPF_OK, "mem: MEM mode ok");

    ins[0] = mk(0x99, 0, 0, 0); /* LDX MEMSX(4)<<5, DW -> invalid */
    e = bpf_validate(ins, 1, &conf);
    fail |= check(e == BPF_ESIZE, "mem: MEMSX DW rejected");

    ins[0] = mk(0x91, 0, 0, 0); /* LDX MEMSX(4)<<5, B -> valid */
    e = bpf_validate(ins, 1, &conf);
    fail |= check(e == BPF_OK, "mem: MEMSX B ok");

    ins[0] = mk(0x86, 3, 0, 0); /* JMP32 CALL src 3 (invalid) */
    e = bpf_validate(ins, 1, &conf);
    fail |= check(e == BPF_ECALL, "jmp32: CALL src3 rejected");
    return fail;
}

int main(void)
{
    int fail;

    fail = 0;
    fail |= test_register_range();
    fail |= test_packet();
    fail |= test_imm_subtypes();
    fail |= test_end_width();
    fail |= test_neg_movsx();
    fail |= test_call_exit();
    fail |= test_conformance();
    fail |= test_atomic();
    fail |= test_mem_size_modes();
    if (fail)
    {
        (void)fprintf(stderr, "test_validate: FAILED\n");
        return 1;
    }
    (void)printf("ok: test_validate\n");
    return 0;
}
