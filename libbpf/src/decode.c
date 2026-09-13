#include "decode.h"

static bpf_u32 read_u16le(const bpf_byte *p)
{
    bpf_byte b0;
    bpf_byte b1;
    bpf_u32 lo;
    bpf_u32 hi;
    bpf_u32 v;

    b0 = p[0];
    b1 = p[1];
    lo = (bpf_u32)b0;
    hi = (bpf_u32)b1;
    v = lo | (hi << 8);
    return v;
}

static bpf_u32 read_u32le(const bpf_byte *p)
{
    bpf_byte b0;
    bpf_byte b1;
    bpf_byte b2;
    bpf_byte b3;
    bpf_u32 lo;
    bpf_u32 b1w;
    bpf_u32 b2w;
    bpf_u32 b3w;
    bpf_u32 v;

    b0 = p[0];
    b1 = p[1];
    b2 = p[2];
    b3 = p[3];
    lo = (bpf_u32)b0;
    b1w = (bpf_u32)b1;
    b2w = (bpf_u32)b2;
    b3w = (bpf_u32)b3;
    v = lo | (b1w << 8) | (b2w << 16) | (b3w << 24);
    return v;
}

static bpf_byte class_of(bpf_byte opcode)
{
    bpf_byte c;

    c = (bpf_byte)(opcode & 0x07u);
    return c;
}

/* 1 if this LD opcode uses the wide (IMM) encoding, else 0. */
static bpf_byte is_wide_ld(bpf_byte opcode)
{
    bpf_byte cls;
    bpf_byte mode;
    bpf_byte imm;

    cls = class_of(opcode);
    if (cls != BPF_CLS_LD)
    {
        return 0;
    }
    mode = (bpf_byte)(opcode >> 5);
    imm = (bpf_byte)(mode == BPF_MODE_IMM);
    return imm;
}

bpf_err bpf_decode(const bpf_byte *buf, bpf_u32 len, bpf_insn *out, bpf_u32 cap,
                   bpf_u32 *n)
{
    bpf_u32 consumed;
    bpf_u32 count;
    bpf_u32 u16;
    bpf_u32 u32;
    bpf_byte op;
    bpf_byte cls;
    bpf_byte regs;
    bpf_byte src;
    bpf_byte dst;
    bpf_byte wide;
    bpf_byte ccode;
    bpf_byte csource;
    bpf_byte cmode;
    bpf_byte csize;
    bpf_u32 cnext;
    int coff;
    int cimm;

    consumed = 0;
    count = 0;
    while (consumed < len)
    {
        const bpf_byte *p;
        bpf_u32 remaining;

        remaining = len - consumed;
        if (remaining < BPF_INS_BASIC)
        {
            return BPF_ETRUNC;
        }
        p = buf + consumed;
        op = p[0];
        regs = p[1];
        src = (bpf_byte)(regs >> 4);
        dst = (bpf_byte)(regs & 0x0Fu);
        u16 = read_u16le(p + 2);
        if (u16 & 0x8000u)
        {
            u16 = u16 | 0xFFFF0000u;
        }
        u32 = read_u32le(p + 4);
        coff = (int)u16;
        cimm = (int)u32;
        wide = 0;
        cls = class_of(op);
        if (cls == BPF_CLS_LD)
        {
            wide = is_wide_ld(op);
        }
        if (count >= cap)
        {
            return BPF_ECOUNT;
        }
        out[count].opcode = op;
        out[count].class = cls;
        out[count].src_reg = src;
        out[count].dst_reg = dst;
        out[count].offset = coff;
        out[count].imm = cimm;
        if (wide)
        {
            if (remaining < BPF_INS_WIDE)
            {
                return BPF_ETRUNC;
            }
            csize = (bpf_byte)((op >> 3) & 0x03u);
            ccode = (bpf_byte)(op >> 5);
            cnext = read_u32le(p + 12);
            out[count].is_wide = 1;
            out[count].next_imm = cnext;
            out[count].mode = BPF_MODE_IMM;
            out[count].size = csize;
            out[count].code = ccode;
            out[count].source = 0;
            consumed = consumed + BPF_INS_WIDE;
        }
        else
        {
            int is_alu;

            is_alu = 0;
            if (cls == BPF_CLS_ALU)
            {
                is_alu = 1;
            }
            if (cls == BPF_CLS_ALU64)
            {
                is_alu = 1;
            }
            if (cls == BPF_CLS_JMP)
            {
                is_alu = 1;
            }
            if (cls == BPF_CLS_JMP32)
            {
                is_alu = 1;
            }
            out[count].is_wide = 0;
            out[count].next_imm = 0;
            if (is_alu)
            {
                ccode = (bpf_byte)(op >> 4);
                csource = (bpf_byte)((op >> 3) & 0x01u);
                out[count].code = ccode;
                out[count].source = csource;
                out[count].mode = 0;
                out[count].size = 0;
            }
            else
            {
                cmode = (bpf_byte)(op >> 5);
                csize = (bpf_byte)((op >> 3) & 0x03u);
                out[count].mode = cmode;
                out[count].size = csize;
                out[count].code = 0;
                out[count].source = 0;
            }
            consumed = consumed + BPF_INS_BASIC;
        }
        count = count + 1;
    }
    *n = count;
    return BPF_OK;
}
