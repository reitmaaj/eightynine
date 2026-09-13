#include <stdio.h>

#include "validate.h"

static int fails;
static int count;

typedef struct
{
    bpf_byte op;
    bpf_byte src;
    bpf_byte dst;
    int imm;
    int off;
    bpf_err want;
} v_case;

static void run_case(const v_case *c)
{
    bpf_insn in;
    bpf_u32 conf;
    bpf_err e;

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
    e = bpf_validate(&in, 1, &conf);
    count = count + 1;
    if (e != c->want)
    {
        (void)fprintf(stderr,
                      "FAIL: op=%02x src=%u dst=%u off=%d got=%d"
                      " want=%d\n",
                      c->op, c->src, c->dst, c->off, (int)e, (int)c->want);
        fails = fails + 1;
    }
}

static const v_case CASES[] = {
    /* register range */
    {0xb7, 0, 10, 0, 0, BPF_OK},
    {0xb7, 0, 11, 0, 0, BPF_EREG},
    {0xbf, 11, 0, 0, 0, BPF_EREG},
    {0xbf, 0, 11, 0, 0, BPF_EREG},
    /* deprecated packet */
    {0x20, 0, 0, 0, 0, BPF_EDEPRECATED}, /* ABS W */
    {0x40, 0, 0, 0, 0, BPF_EDEPRECATED}, /* IND W */
    {0x28, 0, 0, 0, 0, BPF_EDEPRECATED}, /* ABS H */
    {0x48, 0, 0, 0, 0, BPF_EDEPRECATED}, /* IND H */
    /* unsupported 64-bit imm subtype */
    {0x18, 1, 1, 0, 0, BPF_EIMM},
    {0x18, 2, 1, 0, 0, BPF_EIMM},
    {0x18, 6, 1, 0, 0, BPF_EIMM},
    /* END widths */
    {0xdc, 0, 0, 16, 0, BPF_OK},
    {0xdc, 0, 0, 32, 0, BPF_OK},
    {0xdc, 0, 0, 64, 0, BPF_OK},
    {0xdc, 0, 0, 8, 0, BPF_EEND},
    {0xdc, 0, 0, 128, 0, BPF_EEND},
    {0xdf, 0, 0, 32, 0, BPF_EEND}, /* ALU64 END source bit */
    /* NEG */
    {0x87, 0, 0, 0, 0, BPF_OK},
    {0x8f, 0, 0, 0, 0, BPF_ENEG},
    /* MOVSX */
    {0xbf, 1, 0, 0, 8, BPF_OK},
    {0xbf, 1, 0, 0, 16, BPF_OK},
    {0xbf, 1, 0, 0, 32, BPF_OK},
    {0xb7, 0, 0, 0, 8, BPF_EMOVSX},
    {0xbf, 1, 0, 0, 4, BPF_EMOVSX},
    {0xbc, 1, 0, 0, 8, BPF_OK},
    {0xb4, 1, 0, 0, 32, BPF_EMOVSX},
    /* CALL */
    {0x85, 0, 0, 0, 0, BPF_OK},
    {0x85, 1, 0, 0, 0, BPF_OK},
    {0x85, 2, 0, 0, 0, BPF_OK},
    {0x85, 3, 0, 0, 0, BPF_ECALL},
    /* EXIT */
    {0x95, 0, 0, 0, 0, BPF_OK},
    {0x95, 1, 0, 0, 0, BPF_EEXIT},
    {0x95, 0, 0, 1, 0, BPF_EEXIT},
    {0x95, 0, 0, 0, 1, BPF_EEXIT},
    /* memory size errors */
    {0xD3, 0, 0, 0, 0, BPF_ESIZE}, /* atomic B */
    {0xCB, 0, 0, 0, 0, BPF_ESIZE}, /* atomic H */
    {0x99, 0, 0, 0, 0, BPF_ESIZE}, /* memsx DW */
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
        (void)fprintf(stderr, "test_validate_matrix: %d/%d failed\n", fails,
                      count);
        return 1;
    }
    (void)printf("ok: test_validate_matrix (%d cases)\n", count);
    return 0;
}
