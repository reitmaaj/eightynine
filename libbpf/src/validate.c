#include "validate.h"

static bpf_byte is_divmod(bpf_byte code)
{
    bpf_byte r;

    r = 0;
    if (code == BPF_ALU_DIV)
    {
        r = 1;
    }
    if (code == BPF_ALU_MOD)
    {
        r = 1;
    }
    if (code == BPF_ALU_MUL)
    {
        r = 1;
    }
    return r;
}

static bpf_byte end_width_ok(int imm)
{
    bpf_byte r;

    r = 0;
    if (imm == BPF_END_16)
    {
        r = 1;
    }
    if (imm == BPF_END_32)
    {
        r = 1;
    }
    if (imm == BPF_END_64)
    {
        r = 1;
    }
    return r;
}

static bpf_byte movsx_width_ok(bpf_byte cls, int off)
{
    bpf_byte r;

    r = 0;
    if (off == 8)
    {
        r = 1;
    }
    if (off == 16)
    {
        r = 1;
    }
    if (cls == BPF_CLS_ALU64)
    {
        if (off == 32)
        {
            r = 1;
        }
    }
    return r;
}

static bpf_err reg_err(const bpf_insn *in)
{
    bpf_byte src;
    bpf_byte dst;

    src = in->src_reg;
    dst = in->dst_reg;
    if (src > 10u)
    {
        return BPF_EREG;
    }
    if (dst > 10u)
    {
        return BPF_EREG;
    }
    return BPF_OK;
}

static bpf_err validate_alu(const bpf_insn *in)
{
    bpf_byte cls;
    bpf_byte code;
    bpf_byte source;
    bpf_byte end_w;
    bpf_byte moff;
    int off;
    int imm;

    cls = in->class;
    code = in->code;
    source = in->source;
    off = in->offset;
    imm = in->imm;
    end_w = end_width_ok(imm);
    moff = movsx_width_ok(cls, off);
    if (code == BPF_ALU_END)
    {
        if (cls == BPF_CLS_ALU64)
        {
            if (source != 0)
            {
                return BPF_EEND;
            }
        }
        if (!end_w)
        {
            return BPF_EEND;
        }
        return BPF_OK;
    }
    if (code == BPF_ALU_NEG)
    {
        if (source != BPF_SRC_K)
        {
            return BPF_ENEG;
        }
        return BPF_OK;
    }
    if (code == BPF_ALU_MOV)
    {
        if (off == 0)
        {
            return BPF_OK;
        }
        if (!moff)
        {
            return BPF_EMOVSX;
        }
        if (source != BPF_SRC_X)
        {
            return BPF_EMOVSX;
        }
        return BPF_OK;
    }
    return BPF_OK;
}

static bpf_err validate_jmp(const bpf_insn *in)
{
    bpf_byte code;
    bpf_byte src;
    int off;
    int imm;

    code = in->code;
    src = in->src_reg;
    off = in->offset;
    imm = in->imm;
    if (code == BPF_JMP_CALL)
    {
        if (src == 0)
        {
            return BPF_OK;
        }
        if (src == 1)
        {
            return BPF_OK;
        }
        if (src == 2)
        {
            return BPF_OK;
        }
        return BPF_ECALL;
    }
    if (code == BPF_JMP_EXIT)
    {
        if (src != 0)
        {
            return BPF_EEXIT;
        }
        if (off != 0)
        {
            return BPF_EEXIT;
        }
        if (imm != 0)
        {
            return BPF_EEXIT;
        }
        return BPF_OK;
    }
    return BPF_OK;
}

static bpf_err validate_mem(const bpf_insn *in)
{
    bpf_byte cls;
    bpf_byte mode;
    bpf_byte size;
    bpf_byte src;
    bpf_byte is_abs;
    bpf_byte is_ind;

    cls = in->class;
    mode = in->mode;
    size = in->size;
    src = in->src_reg;
    is_abs = (bpf_byte)(mode == BPF_MODE_ABS);
    is_ind = (bpf_byte)(mode == BPF_MODE_IND);
    if (is_abs)
    {
        return BPF_EDEPRECATED;
    }
    if (is_ind)
    {
        return BPF_EDEPRECATED;
    }
    if (mode == BPF_MODE_IMM)
    {
        if (cls != BPF_CLS_LD)
        {
            return BPF_ESIZE;
        }
        if (src != 0)
        {
            return BPF_EIMM;
        }
        return BPF_OK;
    }
    if (mode == BPF_MODE_ATOMIC)
    {
        if (size == BPF_SIZE_B)
        {
            return BPF_ESIZE;
        }
        if (size == BPF_SIZE_H)
        {
            return BPF_ESIZE;
        }
        return BPF_OK;
    }
    if (mode == BPF_MODE_MEMSX)
    {
        if (size == BPF_SIZE_DW)
        {
            return BPF_ESIZE;
        }
        return BPF_OK;
    }
    return BPF_OK;
}

bpf_u32 bpf_insn_conf(const bpf_insn *in)
{
    bpf_u32 c;
    bpf_byte cls;
    bpf_byte code;
    bpf_byte mode;
    bpf_byte size;
    int imm;
    bpf_byte alu;
    bpf_byte alu64;
    bpf_byte jmp;
    bpf_byte jmp32;
    bpf_byte atom;
    bpf_byte packet;
    bpf_byte dm;
    bpf_byte is_abs;
    bpf_byte is_ind;

    c = 0;
    cls = in->class;
    code = in->code;
    mode = in->mode;
    size = in->size;
    imm = in->imm;
    dm = is_divmod(code);
    alu = (bpf_byte)(cls == BPF_CLS_ALU);
    alu64 = (bpf_byte)(cls == BPF_CLS_ALU64);
    jmp = (bpf_byte)(cls == BPF_CLS_JMP);
    jmp32 = (bpf_byte)(cls == BPF_CLS_JMP32);
    atom = (bpf_byte)(mode == BPF_MODE_ATOMIC);
    is_abs = (bpf_byte)(mode == BPF_MODE_ABS);
    is_ind = (bpf_byte)(mode == BPF_MODE_IND);
    packet = is_abs;
    if (is_ind)
    {
        packet = 1;
    }
    if (alu)
    {
        c = c | BPF_CONF_BASE32;
    }
    if (alu64)
    {
        c = c | BPF_CONF_BASE64;
    }
    if (jmp)
    {
        c = c | BPF_CONF_BASE64;
    }
    if (jmp32)
    {
        c = c | BPF_CONF_BASE32;
    }
    if (dm)
    {
        if (alu)
        {
            c = c | BPF_CONF_DIVMUL32;
        }
        if (alu64)
        {
            c = c | BPF_CONF_DIVMUL64;
        }
    }
    if (code == BPF_ALU_END)
    {
        if (imm == BPF_END_64)
        {
            c = c | BPF_CONF_BASE64;
        }
    }
    if (atom)
    {
        if (size == BPF_SIZE_W)
        {
            c = c | BPF_CONF_ATOMIC32;
        }
        if (size == BPF_SIZE_DW)
        {
            c = c | BPF_CONF_ATOMIC64;
        }
    }
    if (packet)
    {
        c = c | BPF_CONF_PACKET;
    }
    if (!alu)
    {
        if (!alu64)
        {
            if (!jmp)
            {
                if (!jmp32)
                {
                    if (size == BPF_SIZE_DW)
                    {
                        c = c | BPF_CONF_BASE64;
                    }
                    else
                    {
                        c = c | BPF_CONF_BASE32;
                    }
                }
            }
        }
    }
    /* Fold supersets: base64 includes base32, divmul64 includes divmul32,
     * atomic64 includes atomic32 (RFC 9669 section 7.1). */
    if (c & BPF_CONF_BASE64)
    {
        c = c | BPF_CONF_BASE32;
    }
    if (c & BPF_CONF_DIVMUL64)
    {
        c = c | BPF_CONF_DIVMUL32;
    }
    if (c & BPF_CONF_ATOMIC64)
    {
        c = c | BPF_CONF_ATOMIC32;
    }
    return c;
}

bpf_err bpf_validate(const bpf_insn *ins, bpf_u32 n, bpf_u32 *conf)
{
    bpf_u32 total;
    bpf_u32 i;
    bpf_byte cls;
    bpf_byte is_alu;
    bpf_byte is_alu64;
    bpf_byte is_jmp;
    bpf_byte is_jmp32;

    total = 0;
    i = 0;
    while (i < n)
    {
        bpf_err e;
        const bpf_insn *in;
        bpf_u32 c;

        in = ins + i;
        c = bpf_insn_conf(in);
        total = total | c;
        e = reg_err(in);
        if (e != BPF_OK)
        {
            *conf = total;
            return e;
        }
        cls = in->class;
        is_alu = (bpf_byte)(cls == BPF_CLS_ALU);
        is_alu64 = (bpf_byte)(cls == BPF_CLS_ALU64);
        is_jmp = (bpf_byte)(cls == BPF_CLS_JMP);
        is_jmp32 = (bpf_byte)(cls == BPF_CLS_JMP32);
        if (is_alu)
        {
            e = validate_alu(in);
            if (e != BPF_OK)
            {
                *conf = total;
                return e;
            }
        }
        if (is_alu64)
        {
            e = validate_alu(in);
            if (e != BPF_OK)
            {
                *conf = total;
                return e;
            }
        }
        if (is_jmp)
        {
            e = validate_jmp(in);
            if (e != BPF_OK)
            {
                *conf = total;
                return e;
            }
        }
        if (is_jmp32)
        {
            e = validate_jmp(in);
            if (e != BPF_OK)
            {
                *conf = total;
                return e;
            }
        }
        if (!is_alu)
        {
            if (!is_alu64)
            {
                if (!is_jmp)
                {
                    if (!is_jmp32)
                    {
                        e = validate_mem(in);
                        if (e != BPF_OK)
                        {
                            *conf = total;
                            return e;
                        }
                    }
                }
            }
        }
        i = i + 1;
    }
    *conf = total;
    return BPF_OK;
}
