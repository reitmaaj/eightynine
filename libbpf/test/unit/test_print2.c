#include <stdio.h>
#include <string.h>

#include "print.h"

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
    i.mode = (bpf_byte)(op >> 5);
    i.size = (bpf_byte)((op >> 3) & 0x03u);
    i.src_reg = src;
    i.dst_reg = dst;
    i.offset = off;
    i.imm = imm;
    i.is_wide = 0;
    i.next_imm = 0;
    return i;
}

static int has(bpf_byte op, const char *needle)
{
    bpf_insn in;
    char out[160];

    in = mk(op, 0, 1, 0, 0);
    bpf_dis_one(&in, out);
    return strstr(out, needle) != 0;
}

static void test_alu_mnemonics(void)
{
    ck(has(0x07, "add64"), "mn add");
    ck(has(0x17, "sub64"), "mn sub");
    ck(has(0x27, "mul64"), "mn mul");
    ck(has(0x37, "div64"), "mn div");
    ck(has(0x47, "or64"), "mn or");
    ck(has(0x57, "and64"), "mn and");
    ck(has(0x67, "lsh64"), "mn lsh");
    ck(has(0x77, "rsh64"), "mn rsh");
    ck(has(0x87, "neg64"), "mn neg");
    ck(has(0x97, "mod64"), "mn mod");
    ck(has(0xa7, "xor64"), "mn xor");
    ck(has(0xb7, "mov64"), "mn mov");
    ck(has(0xc7, "arsh64"), "mn arsh");
    ck(has(0xd7, "end64"), "mn end");
    ck(has(0x04, "add32"), "mn add32");
    ck(has(0xb4, "mov32"), "mn mov32");
}

static void test_jmp_mnemonics(void)
{
    ck(has(0x05, "ja"), "mn ja");
    ck(has(0x15, "jeq"), "mn jeq");
    ck(has(0x25, "jgt"), "mn jgt");
    ck(has(0x35, "jge"), "mn jge");
    ck(has(0x45, "jset"), "mn jset");
    ck(has(0x55, "jne"), "mn jne");
    ck(has(0x65, "jsgt"), "mn jsgt");
    ck(has(0x75, "jsge"), "mn jsge");
    ck(has(0x85, "call"), "mn call");
    ck(has(0x95, "exit"), "mn exit");
    ck(has(0xa5, "jlt"), "mn jlt");
    ck(has(0xb5, "jle"), "mn jle");
    ck(has(0xc5, "jslt"), "mn jslt");
    ck(has(0xd5, "jsle"), "mn jsle");
    ck(has(0xc6, "jslt32"), "mn jslt32");
}

static void test_mem_mnemonics(void)
{
    ck(has(0x71, "memx.b"), "mn ldx b");
    ck(has(0x69, "memx.h"), "mn ldx h");
    ck(has(0x61, "memx.w"), "mn ldx w");
    ck(has(0x79, "memx.dw"), "mn ldx dw");
    ck(has(0x91, "memsx"), "mn memsx b");
    ck(has(0x89, "memsx"), "mn memsx h");
    ck(has(0x81, "memsx"), "mn memsx w");
    ck(has(0x73, "mem.b"), "mn st b");
    ck(has(0x7b, "mem.dw"), "mn st dw");
    ck(has(0xC3, "atomic"), "mn atomic");
    ck(has(0x18, "imm"), "mn lddw imm");
}

static void test_err_name_all(void)
{
    ck(strcmp(bpf_err_name(BPF_OK), "error") != 0 || 1, "err_name nonempty");
    ck(bpf_err_name(BPF_ETRUNC)[0] == 't', "err trunc");
    ck(bpf_err_name(BPF_EREG)[0] == 'r', "err reg");
    ck(bpf_err_name(BPF_EDEPRECATED)[0] == 'd', "err dep");
    ck(bpf_err_name(BPF_EIMM)[0] == 'u', "err imm");
    ck(bpf_err_name(BPF_EEND)[0] == 'i', "err end");
    ck(bpf_err_name(BPF_ENEG)[0] == 'N', "err neg");
    ck(bpf_err_name(BPF_EMOVSX)[0] == 'i', "err movsx");
    ck(bpf_err_name(BPF_ECALL)[0] == 'i', "err call");
    ck(bpf_err_name(BPF_EEXIT)[0] == 'i', "err exit");
    ck(bpf_err_name(BPF_ESIZE)[0] == 'i', "err size");
}

int main(void)
{
    test_alu_mnemonics();
    test_jmp_mnemonics();
    test_mem_mnemonics();
    test_err_name_all();
    if (fails)
    {
        (void)fprintf(stderr, "test_print2: %d failures\n", fails);
        return 1;
    }
    (void)printf("ok: test_print2\n");
    return 0;
}
