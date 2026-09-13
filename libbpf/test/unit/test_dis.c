#include <stdio.h>
#include <string.h>

#include "print.h"

static int fails;
static int count;

typedef struct
{
    bpf_byte op;
    bpf_byte src;
    bpf_byte dst;
    int imm;
    int off;
    const char *needle;
} dis_case;

static void run_case(const dis_case *c)
{
    bpf_insn in;
    char out[200];

    in.opcode = c->op;
    in.class = (bpf_byte)(c->op & 0x07u);
    in.code = (bpf_byte)(c->op >> 4);
    in.source = (bpf_byte)((c->op >> 3) & 0x01u);
    in.mode = (bpf_byte)(c->op >> 5);
    in.size = (bpf_byte)((c->op >> 3) & 0x03u);
    in.src_reg = c->src;
    in.dst_reg = c->dst;
    in.offset = c->off;
    in.imm = c->imm;
    in.is_wide = 0;
    in.next_imm = 0;
    bpf_dis_one(&in, out);
    count = count + 1;
    if (strstr(out, c->needle) == 0)
    {
        (void)fprintf(stderr, "FAIL: op=%02x want=[%s] got=[%s]\n", c->op,
                      c->needle, out);
        fails = fails + 1;
    }
}

static const dis_case CASES[] = {
    {0x07, 0, 1, 2, 0, "add64"},    {0x04, 0, 1, 2, 0, "add32"},
    {0x17, 0, 1, 2, 0, "sub64"},    {0x27, 0, 1, 2, 0, "mul64"},
    {0x37, 0, 1, 2, 0, "div64"},    {0x37, 0, 1, 2, 1, "sdiv64"},
    {0x47, 0, 1, 2, 0, "or64"},     {0x57, 0, 1, 2, 0, "and64"},
    {0x67, 0, 1, 2, 0, "lsh64"},    {0x77, 0, 1, 2, 0, "rsh64"},
    {0x87, 0, 1, 0, 0, "neg64"},    {0x97, 0, 1, 2, 0, "mod64"},
    {0x97, 0, 1, 2, 1, "smod64"},   {0xa7, 0, 1, 2, 0, "xor64"},
    {0xb7, 0, 1, 2, 0, "mov64"},    {0xbf, 1, 1, 0, 8, "movsx64"},
    {0xc7, 0, 1, 2, 0, "arsh64"},   {0xd7, 0, 1, 0, 0, "end64"},
    {0x05, 0, 0, 0, 2, "ja"},       {0x15, 0, 1, 5, 2, "jeq"},
    {0x25, 0, 1, 5, 2, "jgt"},      {0x35, 0, 1, 5, 2, "jge"},
    {0x45, 0, 1, 5, 2, "jset"},     {0x55, 0, 1, 5, 2, "jne"},
    {0x65, 0, 1, 5, 2, "jsgt"},     {0x75, 0, 1, 5, 2, "jsge"},
    {0x85, 0, 0, 3, 0, "call"},     {0x95, 0, 0, 0, 0, "exit"},
    {0xa5, 0, 1, 5, 2, "jlt"},      {0xb5, 0, 1, 5, 2, "jle"},
    {0xc5, 0, 1, 5, 2, "jslt"},     {0xd5, 0, 1, 5, 2, "jsle"},
    {0x06, 0, 0, 5, 0, "ja"},       {0xce, 2, 1, 0, 2, "jslt32"},
    {0x71, 1, 2, 0, 0, "memx.b"},   {0x69, 1, 2, 0, 0, "memx.h"},
    {0x61, 1, 2, 0, 0, "memx.w"},   {0x79, 1, 2, 0, 0, "memx.dw"},
    {0x91, 1, 2, 0, 0, "memsx"},    {0x73, 2, 1, 0, 0, "mem.b"},
    {0x72, 0, 1, 0xAB, 0, "mem.b"}, {0x7a, 0, 1, 1, 0, "mem.dw"},
    {0xc3, 2, 1, 0, 0, "atomic.w"}, {0xdb, 2, 1, 0, 0, "atomic.dw"},
    {0x18, 0, 1, 0, 0, "imm"},
};

int main(void)
{
    unsigned i;

    for (i = 0; i < sizeof(CASES) / sizeof(CASES[0]); ++i)
    {
        run_case(&CASES[i]);
    }
    if (fails)
    {
        (void)fprintf(stderr, "test_dis: %d/%d failed\n", fails, count);
        return 1;
    }
    (void)printf("ok: test_dis (%d cases)\n", count);
    return 0;
}
