#include <stdio.h>
#include <string.h>

#include "print.h"

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

static int test_err_names(void)
{
    bpf_err es[16];
    int n;
    int i;
    int fail;

    fail = 0;
    es[0] = BPF_OK;
    es[1] = BPF_ETRUNC;
    es[2] = BPF_ECOUNT;
    es[3] = BPF_EREG;
    es[4] = BPF_EDEPRECATED;
    es[5] = BPF_EIMM;
    es[6] = BPF_EEND;
    es[7] = BPF_ENEG;
    es[8] = BPF_EMOVSX;
    es[9] = BPF_ECALL;
    es[10] = BPF_EEXIT;
    es[11] = BPF_ESIZE;
    n = 12;
    for (i = 0; i < n; ++i)
    {
        const char *s;

        s = bpf_err_name(es[i]);
        if (s == 0)
        {
            (void)fprintf(stderr, "FAIL: err_name NULL for %d\n", (int)es[i]);
            return 1;
        }
        if (s[0] == '\0')
        {
            (void)fprintf(stderr, "FAIL: err_name empty for %d\n", (int)es[i]);
            return 1;
        }
    }
    fail |= check(bpf_err_name(BPF_EREG)[0] != '\0', "errnames: populated");
    return fail;
}

static int test_regs_byte(void)
{
    int fail;

    fail = 0;
    fail |= check(bpf_regs_byte(1, 2) == 0x12, "regsbyte: 1<<4|2");
    fail |= check(bpf_regs_byte(0, 9) == 0x09, "regsbyte: 0|9");
    fail |= check(bpf_regs_byte(10, 10) == 0xAA, "regsbyte: A|A");
    return fail;
}

static int test_dis_alu(void)
{
    bpf_insn in;
    char out[128];
    int fail;

    fail = 0;
    in = mk(0x07, 0, 1, 2, 0); /* ADD K ALU64 */
    bpf_dis_one(&in, out);
    fail |= check(strstr(out, "add64") != 0, "dis: add64 present");
    fail |= check(strstr(out, "r1") != 0, "dis: dst r1");

    in = mk(0x34, 0, 1, 0, 0); /* DIV K ALU (32-bit) */
    bpf_dis_one(&in, out);
    fail |= check(strstr(out, "div32") != 0, "dis: div32 present");
    return fail;
}

static int test_dis_jmp(void)
{
    bpf_insn in;
    char out[128];
    int fail;

    fail = 0;
    in = mk(0xc6, 2, 1, 0, 2); /* JSLT X JMP32 */
    bpf_dis_one(&in, out);
    fail |= check(strstr(out, "jslt32") != 0, "dis: jslt32 present");

    in = mk(0x95, 0, 0, 0, 0); /* EXIT */
    bpf_dis_one(&in, out);
    fail |= check(strstr(out, "exit") != 0, "dis: exit present");
    return fail;
}

static int test_dis_mem(void)
{
    bpf_insn in;
    char out[128];
    int fail;

    fail = 0;
    in = mk(0x79, 0, 1, 0, 0); /* LDX MEM DW */
    bpf_dis_one(&in, out);
    fail |= check(strstr(out, "memx.dw") != 0, "dis: memx.dw present");

    in = mk(0x91, 1, 2, 0, 0); /* LDX MEMSX B */
    bpf_dis_one(&in, out);
    fail |= check(strstr(out, "memsx") != 0, "dis: memsx present");
    return fail;
}

static int test_groups_into(void)
{
    char out[256];
    int fail;

    fail = 0;
    out[0] = '\0';
    bpf_groups_into(BPF_CONF_BASE32 | BPF_CONF_BASE64, out, sizeof(out));
    fail |=
        check(strcmp(out, "base32\nbase64\n") == 0, "groups: base32,base64");

    out[0] = '\0';
    bpf_groups_into(0, out, sizeof(out));
    fail |= check(strcmp(out, "base32\n") == 0, "groups: only base32");
    return fail;
}

int main(void)
{
    int fail;

    fail = 0;
    fail |= test_err_names();
    fail |= test_regs_byte();
    fail |= test_dis_alu();
    fail |= test_dis_jmp();
    fail |= test_dis_mem();
    fail |= test_groups_into();
    if (fail)
    {
        (void)fprintf(stderr, "test_print: FAILED\n");
        return 1;
    }
    (void)printf("ok: test_print\n");
    return 0;
}
