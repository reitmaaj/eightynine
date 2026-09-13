#include "eval.h"

static bpf_u64 se_i32(int v)
{
    bpf_i64 x;

    x = (bpf_i64)v;
    return (bpf_u64)x;
}

static bpf_u16 sw16(bpf_u16 v)
{
    bpf_u16 lo;
    bpf_u16 hi;

    lo = (bpf_u16)(v << 8);
    hi = (bpf_u16)(v >> 8);
    return (bpf_u16)(lo | hi);
}

static bpf_u32 sw32(bpf_u32 v)
{
    bpf_u32 a;
    bpf_u32 b;
    bpf_u32 c;
    bpf_u32 d;
    bpf_u32 r;

    a = v & 0x000000FFu;
    b = v & 0x0000FF00u;
    c = v & 0x00FF0000u;
    d = v & 0xFF000000u;
    r = (a << 24) | (b << 8) | (c >> 8) | (d >> 24);
    return r;
}

static bpf_u64 sw64(bpf_u64 v)
{
    bpf_u32 hi;
    bpf_u32 lo;
    bpf_u32 nhi;
    bpf_u32 nlo;
    bpf_u64 r;
    bpf_u64 hw;
    bpf_u64 lw;

    hi = (bpf_u32)(v >> 32);
    lo = (bpf_u32)v;
    nhi = sw32(hi);
    nlo = sw32(lo);
    hw = (bpf_u64)nhi;
    lw = (bpf_u64)nlo;
    r = (lw << 32) | hw;
    return r;
}

static bpf_u32 op32_lsh(bpf_u32 a, bpf_u32 b)
{
    bpf_u32 s;
    bpf_u32 r;

    s = b & 0x1Fu;
    r = a << s;
    return r;
}

static bpf_u32 op32_rsh(bpf_u32 a, bpf_u32 b)
{
    bpf_u32 s;
    bpf_u32 r;

    s = b & 0x1Fu;
    r = a >> s;
    return r;
}

static bpf_u32 op32_arsh(bpf_u32 a, bpf_u32 b)
{
    bpf_i32 v;
    bpf_u32 s;
    bpf_u32 r;
    bpf_i32 sh;
    bpf_u32 u;

    v = (bpf_i32)a;
    s = b & 0x1Fu;
    sh = v >> s;
    u = (bpf_u32)sh;
    r = u;
    return r;
}

static bpf_u32 op32_div(bpf_u32 a, bpf_u32 b)
{
    bpf_u32 r;

    r = 0;
    if (b != 0)
    {
        r = a / b;
    }
    return r;
}

static bpf_u32 op32_mod(bpf_u32 a, bpf_u32 b)
{
    bpf_u32 r;

    r = a;
    if (b != 0)
    {
        r = a % b;
    }
    return r;
}

static bpf_u32 op32_sdiv(bpf_i32 a, bpf_i32 b)
{
    bpf_u32 r;
    bpf_u32 na;
    bpf_i32 q;
    bpf_u32 u;

    r = 0;
    if (b == 0)
    {
        return r;
    }
    if (b == -1)
    {
        na = (bpf_u32)a;
        return 0u - na;
    }
    q = a / b;
    u = (bpf_u32)q;
    r = u;
    return r;
}

static bpf_u32 op32_smod(bpf_i32 a, bpf_i32 b)
{
    bpf_u32 r;
    bpf_i32 q;
    bpf_u32 u;

    r = (bpf_u32)a;
    if (b == 0)
    {
        return r;
    }
    if (b == -1)
    {
        return 0;
    }
    q = a % b;
    u = (bpf_u32)q;
    r = u;
    return r;
}

static bpf_u64 op64_lsh(bpf_u64 a, bpf_u64 b)
{
    bpf_u32 s;
    bpf_u32 t;
    bpf_u64 r;

    t = (bpf_u32)b;
    s = t & 0x3Fu;
    r = a << s;
    return r;
}

static bpf_u64 op64_rsh(bpf_u64 a, bpf_u64 b)
{
    bpf_u32 s;
    bpf_u32 t;
    bpf_u64 r;

    t = (bpf_u32)b;
    s = t & 0x3Fu;
    r = a >> s;
    return r;
}

static bpf_u64 op64_arsh(bpf_u64 a, bpf_u64 b)
{
    bpf_i64 v;
    bpf_u32 s;
    bpf_u32 t;
    bpf_u64 r;
    bpf_i64 sh;
    bpf_u64 u;

    v = (bpf_i64)a;
    t = (bpf_u32)b;
    s = t & 0x3Fu;
    sh = v >> s;
    u = (bpf_u64)sh;
    r = u;
    return r;
}

static bpf_u64 op64_div(bpf_u64 a, bpf_u64 b)
{
    bpf_u64 r;

    r = 0;
    if (b != 0)
    {
        r = a / b;
    }
    return r;
}

static bpf_u64 op64_mod(bpf_u64 a, bpf_u64 b)
{
    bpf_u64 r;

    r = a;
    if (b != 0)
    {
        r = a % b;
    }
    return r;
}

static bpf_u64 op64_sdiv(bpf_i64 a, bpf_i64 b)
{
    bpf_u64 r;
    bpf_u64 na;
    bpf_i64 q;
    bpf_u64 u;

    r = 0;
    if (b == 0)
    {
        return r;
    }
    if (b == -1)
    {
        na = (bpf_u64)a;
        return 0u - na;
    }
    q = a / b;
    u = (bpf_u64)q;
    r = u;
    return r;
}

static bpf_u64 op64_smod(bpf_i64 a, bpf_i64 b)
{
    bpf_u64 r;
    bpf_i64 q;
    bpf_u64 u;

    r = (bpf_u64)a;
    if (b == 0)
    {
        return r;
    }
    if (b == -1)
    {
        return 0;
    }
    q = a % b;
    u = (bpf_u64)q;
    r = u;
    return r;
}

static bpf_u64 sx64(bpf_u64 v, int width)
{
    bpf_byte b;
    bpf_u16 h;
    bpf_u32 w;
    bpf_u64 r;
    bpf_i8 sb;
    bpf_i16 sh;
    bpf_i32 sw;
    bpf_i64 t;
    bpf_u64 u;

    r = v;
    b = (bpf_byte)(v & 0xFFu);
    h = (bpf_u16)(v & 0xFFFFu);
    w = (bpf_u32)v;
    sb = (bpf_i8)b;
    sh = (bpf_i16)h;
    sw = (bpf_i32)w;
    if (width == 8)
    {
        t = (bpf_i64)sb;
        u = (bpf_u64)t;
        r = u;
    }
    if (width == 16)
    {
        t = (bpf_i64)sh;
        u = (bpf_u64)t;
        r = u;
    }
    if (width == 32)
    {
        t = (bpf_i64)sw;
        u = (bpf_u64)t;
        r = u;
    }
    return r;
}

static bpf_u32 sx32(bpf_u32 v, int width)
{
    bpf_byte b;
    bpf_u16 h;
    bpf_u32 r;
    bpf_i8 sb;
    bpf_i16 sh;
    bpf_i32 t;
    bpf_u32 u;

    r = v;
    b = (bpf_byte)(v & 0xFFu);
    h = (bpf_u16)(v & 0xFFFFu);
    sb = (bpf_i8)b;
    sh = (bpf_i16)h;
    if (width == 8)
    {
        t = (bpf_i32)sb;
        u = (bpf_u32)t;
        r = u;
    }
    if (width == 16)
    {
        t = (bpf_i32)sh;
        u = (bpf_u32)t;
        r = u;
    }
    return r;
}

static bpf_u64 do_end(bpf_u64 dst, bpf_byte is64, bpf_byte be, int width)
{
    bpf_u64 r;
    bpf_u32 w32;
    bpf_u16 w16;
    bpf_u32 sw;
    bpf_u16 sh;

    r = dst;
    w32 = (bpf_u32)dst;
    w16 = (bpf_u16)dst;
    sw = sw32(w32);
    sh = sw16(w16);
    if (width == 64)
    {
        if (is64)
        {
            r = sw64(dst);
        }
        else if (be)
        {
            r = sw64(dst);
        }
        return r;
    }
    if (width == 32)
    {
        if (is64)
        {
            r = (bpf_u64)sw;
        }
        else if (be)
        {
            r = (bpf_u64)sw;
        }
        else
        {
            r = dst & 0xFFFFFFFFu;
        }
        return r;
    }
    if (width == 16)
    {
        if (is64)
        {
            r = (bpf_u64)sh;
        }
        else if (be)
        {
            r = (bpf_u64)sh;
        }
        else
        {
            r = dst & 0xFFFFu;
        }
        return r;
    }
    return r;
}

bpf_err bpf_alu(bpf_regs *regs, const bpf_insn *in)
{
    bpf_byte cls;
    bpf_byte code;
    bpf_byte source;
    bpf_byte dst_idx;
    bpf_byte src_idx;
    bpf_byte is64;
    bpf_u64 dst;
    bpf_u64 src;
    bpf_u64 res64;
    bpf_u32 dst32;
    bpf_u32 src32;
    bpf_u32 res32;
    int imm;
    int off;
    bpf_u64 cur;

    cls = in->class;
    code = in->code;
    source = in->source;
    dst_idx = in->dst_reg;
    src_idx = in->src_reg;
    imm = in->imm;
    off = in->offset;
    is64 = (bpf_byte)(cls == BPF_CLS_ALU64);
    cur = regs->r[dst_idx];
    dst = cur;
    dst32 = (bpf_u32)cur;
    if (source == BPF_SRC_K)
    {
        src = se_i32(imm);
        src32 = (bpf_u32)imm;
    }
    else
    {
        cur = regs->r[src_idx];
        src = cur;
        src32 = (bpf_u32)cur;
    }
    if (code == BPF_ALU_END)
    {
        bpf_byte be;

        be = source;
        res64 = do_end(dst, is64, be, imm);
        regs->r[dst_idx] = res64;
        return BPF_OK;
    }
    if (code == BPF_ALU_NEG)
    {
        if (is64)
        {
            res64 = 0u - dst;
        }
        else
        {
            res32 = (bpf_u32)(0u - dst32);
            res64 = (bpf_u64)res32;
        }
        regs->r[dst_idx] = res64;
        return BPF_OK;
    }
    if (code == BPF_ALU_MOV)
    {
        if (off == 0)
        {
            if (is64)
            {
                if (source == BPF_SRC_K)
                {
                    res64 = se_i32(imm);
                }
                else
                {
                    res64 = src;
                }
            }
            else
            {
                if (source == BPF_SRC_K)
                {
                    res32 = (bpf_u32)imm;
                }
                else
                {
                    res32 = src32;
                }
                res64 = (bpf_u64)res32;
            }
            regs->r[dst_idx] = res64;
            return BPF_OK;
        }
        if (is64)
        {
            res64 = sx64(src, off);
        }
        else
        {
            res32 = sx32(src32, off);
            res64 = (bpf_u64)res32;
        }
        regs->r[dst_idx] = res64;
        return BPF_OK;
    }
    if (code == BPF_ALU_ADD)
    {
        if (is64)
        {
            res64 = dst + src;
        }
        else
        {
            res32 = dst32 + src32;
            res64 = (bpf_u64)res32;
        }
    }
    if (code == BPF_ALU_SUB)
    {
        if (is64)
        {
            res64 = dst - src;
        }
        else
        {
            res32 = dst32 - src32;
            res64 = (bpf_u64)res32;
        }
    }
    if (code == BPF_ALU_MUL)
    {
        if (is64)
        {
            res64 = dst * src;
        }
        else
        {
            res32 = dst32 * src32;
            res64 = (bpf_u64)res32;
        }
    }
    if (code == BPF_ALU_DIV)
    {
        bpf_i64 da;
        bpf_i64 sa;
        bpf_i32 d32;
        bpf_i32 s32;

        da = (bpf_i64)dst;
        sa = (bpf_i64)src;
        d32 = (bpf_i32)dst32;
        s32 = (bpf_i32)src32;
        if (off == 1)
        {
            if (is64)
            {
                res64 = op64_sdiv(da, sa);
            }
            else
            {
                res32 = op32_sdiv(d32, s32);
                res64 = (bpf_u64)res32;
            }
        }
        else
        {
            if (is64)
            {
                res64 = op64_div(dst, src);
            }
            else
            {
                res32 = op32_div(dst32, src32);
                res64 = (bpf_u64)res32;
            }
        }
    }
    if (code == BPF_ALU_MOD)
    {
        bpf_i64 da;
        bpf_i64 sa;
        bpf_i32 d32;
        bpf_i32 s32;

        da = (bpf_i64)dst;
        sa = (bpf_i64)src;
        d32 = (bpf_i32)dst32;
        s32 = (bpf_i32)src32;
        if (off == 1)
        {
            if (is64)
            {
                res64 = op64_smod(da, sa);
            }
            else
            {
                res32 = op32_smod(d32, s32);
                res64 = (bpf_u64)res32;
            }
        }
        else
        {
            if (is64)
            {
                res64 = op64_mod(dst, src);
            }
            else
            {
                res32 = op32_mod(dst32, src32);
                res64 = (bpf_u64)res32;
            }
        }
    }
    if (code == BPF_ALU_OR)
    {
        if (is64)
        {
            res64 = dst | src;
        }
        else
        {
            res32 = dst32 | src32;
            res64 = (bpf_u64)res32;
        }
    }
    if (code == BPF_ALU_AND)
    {
        if (is64)
        {
            res64 = dst & src;
        }
        else
        {
            res32 = dst32 & src32;
            res64 = (bpf_u64)res32;
        }
    }
    if (code == BPF_ALU_XOR)
    {
        if (is64)
        {
            res64 = dst ^ src;
        }
        else
        {
            res32 = dst32 ^ src32;
            res64 = (bpf_u64)res32;
        }
    }
    if (code == BPF_ALU_LSH)
    {
        if (is64)
        {
            res64 = op64_lsh(dst, src);
        }
        else
        {
            res32 = op32_lsh(dst32, src32);
            res64 = (bpf_u64)res32;
        }
    }
    if (code == BPF_ALU_RSH)
    {
        if (is64)
        {
            res64 = op64_rsh(dst, src);
        }
        else
        {
            res32 = op32_rsh(dst32, src32);
            res64 = (bpf_u64)res32;
        }
    }
    if (code == BPF_ALU_ARSH)
    {
        if (is64)
        {
            res64 = op64_arsh(dst, src);
        }
        else
        {
            res32 = op32_arsh(dst32, src32);
            res64 = (bpf_u64)res32;
        }
    }
    regs->r[dst_idx] = res64;
    return BPF_OK;
}

static bpf_byte cond_taken(bpf_byte code, bpf_byte is32, bpf_u64 d, bpf_u64 s)
{
    bpf_byte r;
    bpf_u32 d32;
    bpf_u32 s32;
    bpf_i64 sd;
    bpf_i64 ss;
    bpf_i32 sd32;
    bpf_i32 ss32;

    r = 0;
    d32 = (bpf_u32)d;
    s32 = (bpf_u32)s;
    sd = (bpf_i64)d;
    ss = (bpf_i64)s;
    sd32 = (bpf_i32)d32;
    ss32 = (bpf_i32)s32;
    if (is32)
    {
        if (code == BPF_JMP_JEQ)
        {
            if (d32 == s32)
            {
                r = 1;
            }
        }
        if (code == BPF_JMP_JNE)
        {
            if (d32 != s32)
            {
                r = 1;
            }
        }
        if (code == BPF_JMP_JGT)
        {
            if (d32 > s32)
            {
                r = 1;
            }
        }
        if (code == BPF_JMP_JGE)
        {
            if (d32 >= s32)
            {
                r = 1;
            }
        }
        if (code == BPF_JMP_JLT)
        {
            if (d32 < s32)
            {
                r = 1;
            }
        }
        if (code == BPF_JMP_JLE)
        {
            if (d32 <= s32)
            {
                r = 1;
            }
        }
        if (code == BPF_JMP_JSET)
        {
            if ((d32 & s32) != 0u)
            {
                r = 1;
            }
        }
        if (code == BPF_JMP_JSGT)
        {
            if (sd32 > ss32)
            {
                r = 1;
            }
        }
        if (code == BPF_JMP_JSGE)
        {
            if (sd32 >= ss32)
            {
                r = 1;
            }
        }
        if (code == BPF_JMP_JSLT)
        {
            if (sd32 < ss32)
            {
                r = 1;
            }
        }
        if (code == BPF_JMP_JSLE)
        {
            if (sd32 <= ss32)
            {
                r = 1;
            }
        }
    }
    else
    {
        if (code == BPF_JMP_JEQ)
        {
            if (d == s)
            {
                r = 1;
            }
        }
        if (code == BPF_JMP_JNE)
        {
            if (d != s)
            {
                r = 1;
            }
        }
        if (code == BPF_JMP_JGT)
        {
            if (d > s)
            {
                r = 1;
            }
        }
        if (code == BPF_JMP_JGE)
        {
            if (d >= s)
            {
                r = 1;
            }
        }
        if (code == BPF_JMP_JLT)
        {
            if (d < s)
            {
                r = 1;
            }
        }
        if (code == BPF_JMP_JLE)
        {
            if (d <= s)
            {
                r = 1;
            }
        }
        if (code == BPF_JMP_JSET)
        {
            if ((d & s) != 0u)
            {
                r = 1;
            }
        }
        if (code == BPF_JMP_JSGT)
        {
            if (sd > ss)
            {
                r = 1;
            }
        }
        if (code == BPF_JMP_JSGE)
        {
            if (sd >= ss)
            {
                r = 1;
            }
        }
        if (code == BPF_JMP_JSLT)
        {
            if (sd < ss)
            {
                r = 1;
            }
        }
        if (code == BPF_JMP_JSLE)
        {
            if (sd <= ss)
            {
                r = 1;
            }
        }
    }
    return r;
}

bpf_byte bpf_cond_taken(bpf_byte code, bpf_byte is32, bpf_u64 d, bpf_u64 s)
{
    bpf_byte r;

    r = cond_taken(code, is32, d, s);
    return r;
}

static bpf_u64 size_bytes(bpf_byte size)
{
    bpf_u64 r;

    r = 4;
    if (size == BPF_SIZE_H)
    {
        r = 2;
    }
    if (size == BPF_SIZE_B)
    {
        r = 1;
    }
    if (size == BPF_SIZE_DW)
    {
        r = 8;
    }
    return r;
}

static bpf_byte mem_ok(bpf_u64 addr, bpf_u64 size, bpf_u64 mem_size)
{
    bpf_byte r;
    bpf_u64 rem;

    r = 0;
    if (addr > mem_size)
    {
        return r;
    }
    rem = mem_size - addr;
    if (rem < size)
    {
        return r;
    }
    r = 1;
    return r;
}

static bpf_u64 rd_unsigned(const bpf_byte *mem, bpf_u64 addr, bpf_byte size)
{
    bpf_u64 v;
    bpf_byte b;
    bpf_u64 w;
    bpf_u64 a1;
    bpf_u64 a2;
    bpf_u64 a3;
    bpf_u64 a4;
    bpf_u64 a5;
    bpf_u64 a6;
    bpf_u64 a7;

    v = 0;
    a1 = addr + 1;
    a2 = addr + 2;
    a3 = addr + 3;
    a4 = addr + 4;
    a5 = addr + 5;
    a6 = addr + 6;
    a7 = addr + 7;
    b = mem[addr];
    w = (bpf_u64)b;
    v = w;
    if (size == BPF_SIZE_H)
    {
        b = mem[a1];
        w = (bpf_u64)b;
        v = v | (w << 8);
    }
    if (size == BPF_SIZE_W)
    {
        b = mem[a1];
        w = (bpf_u64)b;
        v = v | (w << 8);
        b = mem[a2];
        w = (bpf_u64)b;
        v = v | (w << 16);
        b = mem[a3];
        w = (bpf_u64)b;
        v = v | (w << 24);
    }
    if (size == BPF_SIZE_DW)
    {
        b = mem[a1];
        w = (bpf_u64)b;
        v = v | (w << 8);
        b = mem[a2];
        w = (bpf_u64)b;
        v = v | (w << 16);
        b = mem[a3];
        w = (bpf_u64)b;
        v = v | (w << 24);
        b = mem[a4];
        w = (bpf_u64)b;
        v = v | (w << 32);
        b = mem[a5];
        w = (bpf_u64)b;
        v = v | (w << 40);
        b = mem[a6];
        w = (bpf_u64)b;
        v = v | (w << 48);
        b = mem[a7];
        w = (bpf_u64)b;
        v = v | (w << 56);
    }
    return v;
}

static bpf_u64 rd_signed(const bpf_byte *mem, bpf_u64 addr, bpf_byte size)
{
    bpf_u64 u;
    bpf_u64 r;
    bpf_byte ub;
    bpf_u16 uh;
    bpf_u32 uw;
    bpf_i8 s8;
    bpf_i16 s16;
    bpf_i32 s32;
    bpf_i64 t;

    u = rd_unsigned(mem, addr, size);
    r = u;
    if (size == BPF_SIZE_B)
    {
        ub = (bpf_byte)u;
        s8 = (bpf_i8)ub;
        t = (bpf_i64)s8;
        r = (bpf_u64)t;
    }
    if (size == BPF_SIZE_H)
    {
        uh = (bpf_u16)u;
        s16 = (bpf_i16)uh;
        t = (bpf_i64)s16;
        r = (bpf_u64)t;
    }
    if (size == BPF_SIZE_W)
    {
        uw = (bpf_u32)u;
        s32 = (bpf_i32)uw;
        t = (bpf_i64)s32;
        r = (bpf_u64)t;
    }
    return r;
}

static void wr(bpf_byte *mem, bpf_u64 addr, bpf_u64 val, bpf_byte size)
{
    bpf_byte b;
    bpf_u64 sh;
    bpf_u64 a1;
    bpf_u64 a2;
    bpf_u64 a3;
    bpf_u64 a4;
    bpf_u64 a5;
    bpf_u64 a6;
    bpf_u64 a7;

    a1 = addr + 1;
    a2 = addr + 2;
    a3 = addr + 3;
    a4 = addr + 4;
    a5 = addr + 5;
    a6 = addr + 6;
    a7 = addr + 7;
    b = (bpf_byte)val;
    mem[addr] = b;
    if (size == BPF_SIZE_H)
    {
        sh = val >> 8;
        b = (bpf_byte)sh;
        mem[a1] = b;
    }
    if (size == BPF_SIZE_W)
    {
        sh = val >> 8;
        b = (bpf_byte)sh;
        mem[a1] = b;
        sh = val >> 16;
        b = (bpf_byte)sh;
        mem[a2] = b;
        sh = val >> 24;
        b = (bpf_byte)sh;
        mem[a3] = b;
    }
    if (size == BPF_SIZE_DW)
    {
        sh = val >> 8;
        b = (bpf_byte)sh;
        mem[a1] = b;
        sh = val >> 16;
        b = (bpf_byte)sh;
        mem[a2] = b;
        sh = val >> 24;
        b = (bpf_byte)sh;
        mem[a3] = b;
        sh = val >> 32;
        b = (bpf_byte)sh;
        mem[a4] = b;
        sh = val >> 40;
        b = (bpf_byte)sh;
        mem[a5] = b;
        sh = val >> 48;
        b = (bpf_byte)sh;
        mem[a6] = b;
        sh = val >> 56;
        b = (bpf_byte)sh;
        mem[a7] = b;
    }
}

static bpf_u32 atom32(bpf_u32 op, bpf_u32 old, bpf_u32 src, bpf_u32 r0)
{
    bpf_u32 r;

    r = old;
    if (op == 0x00u)
    {
        r = old + src;
    }
    if (op == 0x40u)
    {
        r = old | src;
    }
    if (op == 0x50u)
    {
        r = old & src;
    }
    if (op == 0xa0u)
    {
        r = old ^ src;
    }
    if (op == 0xe0u)
    {
        r = src;
    }
    if (op == 0xf0u)
    {
        if (old == r0)
        {
            r = src;
        }
    }
    return r;
}

static bpf_u64 atom64(bpf_u32 op, bpf_u64 old, bpf_u64 src, bpf_u64 r0)
{
    bpf_u64 r;

    r = old;
    if (op == 0x00u)
    {
        r = old + src;
    }
    if (op == 0x40u)
    {
        r = old | src;
    }
    if (op == 0x50u)
    {
        r = old & src;
    }
    if (op == 0xa0u)
    {
        r = old ^ src;
    }
    if (op == 0xe0u)
    {
        r = src;
    }
    if (op == 0xf0u)
    {
        if (old == r0)
        {
            r = src;
        }
    }
    return r;
}

static bpf_status mem_step(bpf_machine *m, const bpf_insn *in)
{
    bpf_byte cls;
    bpf_byte mode;
    bpf_byte size;
    bpf_byte src_reg;
    bpf_byte dst_reg;
    bpf_byte is_sx;
    bpf_u64 base;
    bpf_u64 addr;
    bpf_u64 val;
    bpf_u64 sz;
    bpf_u64 hi;
    bpf_u32 lo;
    bpf_u64 ni;
    bpf_u64 hw;
    bpf_u64 lw;
    bpf_i64 sb;
    bpf_i64 sa;
    bpf_i64 of;
    bpf_u32 np;
    bpf_u32 pc;
    bpf_u64 msz;
    bpf_byte ok;
    bpf_byte *mb;
    int off;
    int imm;

    cls = in->class;
    mode = in->mode;
    size = in->size;
    src_reg = in->src_reg;
    dst_reg = in->dst_reg;
    off = in->offset;
    imm = in->imm;
    pc = m->pc;
    if (cls == BPF_CLS_LD)
    {
        ni = in->next_imm;
        hw = (bpf_u64)ni;
        hi = hw << 32;
        lo = (bpf_u32)imm;
        lw = (bpf_u64)lo;
        val = hi | lw;
        m->regs.r[dst_reg] = val;
        np = pc + 1;
        m->pc = np;
        return BPF_STAT_RUNNING;
    }
    if (cls == BPF_CLS_LDX)
    {
        base = m->regs.r[src_reg];
        mb = m->mem;
        msz = m->mem_size;
        sb = (bpf_i64)base;
        of = (bpf_i64)off;
        sa = sb + of;
        if (sa < 0)
        {
            return BPF_STAT_TRAP;
        }
        addr = (bpf_u64)sa;
        sz = size_bytes(size);
        ok = mem_ok(addr, sz, msz);
        if (!ok)
        {
            return BPF_STAT_TRAP;
        }
        is_sx = (bpf_byte)(mode == BPF_MODE_MEMSX);
        if (is_sx)
        {
            val = rd_signed(mb, addr, size);
        }
        else
        {
            val = rd_unsigned(mb, addr, size);
        }
        m->regs.r[dst_reg] = val;
        np = pc + 1;
        m->pc = np;
        return BPF_STAT_RUNNING;
    }
    if (cls == BPF_CLS_ST)
    {
        base = m->regs.r[dst_reg];
        mb = m->mem;
        msz = m->mem_size;
        sb = (bpf_i64)base;
        of = (bpf_i64)off;
        sa = sb + of;
        if (sa < 0)
        {
            return BPF_STAT_TRAP;
        }
        addr = (bpf_u64)sa;
        sz = size_bytes(size);
        ok = mem_ok(addr, sz, msz);
        if (!ok)
        {
            return BPF_STAT_TRAP;
        }
        val = se_i32(imm);
        wr(mb, addr, val, size);
        np = pc + 1;
        m->pc = np;
        return BPF_STAT_RUNNING;
    }
    if (cls == BPF_CLS_STX)
    {
        base = m->regs.r[dst_reg];
        mb = m->mem;
        msz = m->mem_size;
        sb = (bpf_i64)base;
        of = (bpf_i64)off;
        sa = sb + of;
        if (sa < 0)
        {
            return BPF_STAT_TRAP;
        }
        addr = (bpf_u64)sa;
        sz = size_bytes(size);
        ok = mem_ok(addr, sz, msz);
        if (!ok)
        {
            return BPF_STAT_TRAP;
        }
        if (mode == BPF_MODE_ATOMIC)
        {
            bpf_u64 old;
            bpf_u64 nv;
            bpf_u64 srcv;
            bpf_u64 r0;
            bpf_u32 op;
            bpf_u32 wsrc;
            bpf_u32 wold;
            bpf_u32 wn;
            bpf_u32 wr0;
            bpf_u64 tmp;
            bpf_u64 sv;
            bpf_u64 r0v;
            bpf_u64 wv;
            int im;
            bpf_byte fetch;
            bpf_byte is_cmp;

            im = in->imm;
            op = (bpf_u32)im;
            fetch = (bpf_byte)(op & 0x01u);
            op = op & 0xFEu;
            if (size == BPF_SIZE_W)
            {
                tmp = rd_unsigned(mb, addr, size);
                wold = (bpf_u32)tmp;
                sv = m->regs.r[src_reg];
                wsrc = (bpf_u32)sv;
                r0v = m->regs.r[0];
                wr0 = (bpf_u32)r0v;
                wn = atom32(op, wold, wsrc, wr0);
                wv = (bpf_u64)wn;
                wr(mb, addr, wv, size);
                old = (bpf_u64)wold;
            }
            else
            {
                old = rd_unsigned(mb, addr, size);
                srcv = m->regs.r[src_reg];
                r0 = m->regs.r[0];
                nv = atom64(op, old, srcv, r0);
                wr(mb, addr, nv, size);
            }
            is_cmp = (bpf_byte)(op == 0xF0u);
            if (is_cmp)
            {
                m->regs.r[0] = old;
            }
            if (fetch)
            {
                if (!is_cmp)
                {
                    m->regs.r[src_reg] = old;
                }
            }
            np = pc + 1;
            m->pc = np;
            return BPF_STAT_RUNNING;
        }
        val = m->regs.r[src_reg];
        wr(mb, addr, val, size);
        np = pc + 1;
        m->pc = np;
        return BPF_STAT_RUNNING;
    }
    return BPF_STAT_ERR;
}

void bpf_machine_init(bpf_machine *m, const bpf_insn *prog, bpf_u32 n)
{
    int i;

    for (i = 0; i < BPF_NREG; ++i)
    {
        m->regs.r[i] = 0;
    }
    m->prog = prog;
    m->n = n;
    m->pc = 0;
    m->stack_n = 0;
    m->budget = 100000;
    m->helper = 0;
    m->halted = 0;
    m->mem = 0;
    m->mem_size = 0;
}

bpf_status bpf_step(bpf_machine *m)
{
    bpf_u32 pc;
    bpf_byte cls;
    bpf_byte code;
    bpf_byte source;
    bpf_byte src_reg;
    bpf_byte dst_reg;
    bpf_byte taken;
    bpf_u64 d;
    bpf_u64 s;
    bpf_u64 u;
    bpf_u32 uo;
    bpf_u32 ui;
    bpf_u32 np;
    bpf_u32 ret;
    bpf_u32 sn;
    bpf_u32 hp;
    bpf_u32 n;
    bpf_byte halted;
    bpf_i64 b;
    bpf_insn insn;
    int off;
    int imm;
    bpf_regs *regs_p;

    halted = m->halted;
    if (halted)
    {
        return BPF_STAT_RETURNED;
    }
    pc = m->pc;
    n = m->n;
    if (pc >= n)
    {
        return BPF_STAT_ERR;
    }
    b = m->budget;
    if (b <= 0)
    {
        return BPF_STAT_EXHAUSTED;
    }
    b = b - 1;
    m->budget = b;
    insn = m->prog[pc];
    regs_p = &m->regs;
    cls = insn.class;
    if (cls == BPF_CLS_ALU)
    {
        np = pc + 1;
        bpf_alu(regs_p, &insn);
        m->pc = np;
        return BPF_STAT_RUNNING;
    }
    if (cls == BPF_CLS_ALU64)
    {
        np = pc + 1;
        bpf_alu(regs_p, &insn);
        m->pc = np;
        return BPF_STAT_RUNNING;
    }
    if (cls == BPF_CLS_JMP)
    {
        code = insn.code;
        source = insn.source;
        src_reg = insn.src_reg;
        dst_reg = insn.dst_reg;
        off = insn.offset;
        imm = insn.imm;
        uo = (bpf_u32)off;
        ui = (bpf_u32)imm;
        if (code == BPF_JMP_JA)
        {
            np = pc + 1 + uo;
            m->pc = np;
            return BPF_STAT_RUNNING;
        }
        if (code == BPF_JMP_EXIT)
        {
            sn = m->stack_n;
            if (sn > 0)
            {
                sn = sn - 1;
                m->stack_n = sn;
                ret = m->stack[sn];
                m->pc = ret;
                return BPF_STAT_RUNNING;
            }
            m->halted = 1;
            return BPF_STAT_RETURNED;
        }
        if (code == BPF_JMP_CALL)
        {
            if (src_reg == 1)
            {
                sn = m->stack_n;
                if (sn >= BPF_STACK_N)
                {
                    return BPF_STAT_ERR;
                }
                np = pc + 1;
                m->stack[sn] = np;
                sn = sn + 1;
                m->stack_n = sn;
                np = pc + 1 + ui;
                m->pc = np;
                return BPF_STAT_RUNNING;
            }
            hp = (bpf_u32)imm;
            m->helper = hp;
            np = pc + 1;
            m->pc = np;
            return BPF_STAT_HOSTCALL;
        }
        d = m->regs.r[dst_reg];
        if (source == BPF_SRC_K)
        {
            s = se_i32(imm);
        }
        else
        {
            s = m->regs.r[src_reg];
        }
        taken = cond_taken(code, 0, d, s);
        np = pc + 1;
        if (taken)
        {
            np = pc + 1 + uo;
        }
        m->pc = np;
        return BPF_STAT_RUNNING;
    }
    if (cls == BPF_CLS_JMP32)
    {
        code = insn.code;
        source = insn.source;
        src_reg = insn.src_reg;
        dst_reg = insn.dst_reg;
        off = insn.offset;
        imm = insn.imm;
        uo = (bpf_u32)off;
        ui = (bpf_u32)imm;
        if (code == BPF_JMP_JA)
        {
            np = pc + 1 + ui;
            m->pc = np;
            return BPF_STAT_RUNNING;
        }
        d = m->regs.r[dst_reg];
        if (source == BPF_SRC_K)
        {
            u = (bpf_u32)imm;
            s = u;
        }
        else
        {
            s = m->regs.r[src_reg];
        }
        taken = cond_taken(code, 1, d, s);
        np = pc + 1;
        if (taken)
        {
            np = pc + 1 + uo;
        }
        m->pc = np;
        return BPF_STAT_RUNNING;
    }
    if (cls == BPF_CLS_LD)
    {
        return mem_step(m, &insn);
    }
    if (cls == BPF_CLS_LDX)
    {
        return mem_step(m, &insn);
    }
    if (cls == BPF_CLS_ST)
    {
        return mem_step(m, &insn);
    }
    if (cls == BPF_CLS_STX)
    {
        return mem_step(m, &insn);
    }
    return BPF_STAT_ERR;
}
