#include <stdio.h>
#include <string.h>

#include "print.h"

const char *bpf_err_name(bpf_err e)
{
    const char *s;

    s = "error";
    if (e == BPF_ETRUNC)
    {
        s = "truncated instruction";
    }
    if (e == BPF_ECOUNT)
    {
        s = "too many instructions";
    }
    if (e == BPF_EREG)
    {
        s = "register out of range";
    }
    if (e == BPF_EDEPRECATED)
    {
        s = "deprecated packet access";
    }
    if (e == BPF_EIMM)
    {
        s = "unsupported 64-bit immediate subtype";
    }
    if (e == BPF_EEND)
    {
        s = "invalid byte-swap";
    }
    if (e == BPF_ENEG)
    {
        s = "NEG with register source";
    }
    if (e == BPF_EMOVSX)
    {
        s = "invalid MOVSX";
    }
    if (e == BPF_ECALL)
    {
        s = "invalid CALL";
    }
    if (e == BPF_EEXIT)
    {
        s = "invalid EXIT";
    }
    if (e == BPF_ESIZE)
    {
        s = "invalid size for mode";
    }
    if (e == BPF_EEMPTY)
    {
        s = "no instructions";
    }
    if (e == BPF_ERES)
    {
        s = "reserved field set";
    }
    if (e == BPF_EUNSUP)
    {
        s = "unsupported operation";
    }
    if (e == BPF_ER10)
    {
        s = "write to frame pointer";
    }
    if (e == BPF_EBRANCH)
    {
        s = "bad branch target";
    }
    if (e == BPF_EPROF)
    {
        s = "group not in profile";
    }
    if (e == BPF_EADDR)
    {
        s = "guest address out of range";
    }
    if (e == BPF_EPERM)
    {
        s = "memory permission denied";
    }
    if (e == BPF_EHANDLE)
    {
        s = "invalid capability handle";
    }
    if (e == BPF_EFULL)
    {
        s = "capability handle space exhausted";
    }
    if (e == BPF_ESTALE)
    {
        s = "completion for a request not pending";
    }
    if (e == BPF_ESIG)
    {
        s = "invalid FFI signature";
    }
    return s;
}

bpf_byte bpf_regs_byte(bpf_byte src, bpf_byte dst)
{
    unsigned s;
    unsigned d;
    unsigned r;
    bpf_byte b;

    s = (unsigned)src;
    d = (unsigned)dst;
    r = (s << 4) | (d & 0x0Fu);
    b = (bpf_byte)r;
    return b;
}

static void dis_alu(char *out, bpf_byte dst, bpf_byte src, int off, int imm,
                    bpf_byte s, bpf_byte code, bpf_byte cls)
{
    const char *n;
    const char *suf;
    const char *wsrc;
    int w;

    n = "op";
    if (code == BPF_ALU_ADD)
    {
        n = "add";
    }
    if (code == BPF_ALU_SUB)
    {
        n = "sub";
    }
    if (code == BPF_ALU_MUL)
    {
        n = "mul";
    }
    if (code == BPF_ALU_DIV)
    {
        n = "div";
        if (off == 1)
        {
            n = "sdiv";
        }
    }
    if (code == BPF_ALU_OR)
    {
        n = "or";
    }
    if (code == BPF_ALU_AND)
    {
        n = "and";
    }
    if (code == BPF_ALU_LSH)
    {
        n = "lsh";
    }
    if (code == BPF_ALU_RSH)
    {
        n = "rsh";
    }
    if (code == BPF_ALU_NEG)
    {
        n = "neg";
    }
    if (code == BPF_ALU_MOD)
    {
        n = "mod";
        if (off == 1)
        {
            n = "smod";
        }
    }
    if (code == BPF_ALU_XOR)
    {
        n = "xor";
    }
    if (code == BPF_ALU_MOV)
    {
        n = "movsx";
        if (off == 0)
        {
            n = "mov";
        }
    }
    if (code == BPF_ALU_ARSH)
    {
        n = "arsh";
    }
    if (code == BPF_ALU_END)
    {
        n = "end";
    }
    suf = "32";
    if (cls == BPF_CLS_ALU64)
    {
        suf = "64";
    }
    wsrc = "K";
    if (s == BPF_SRC_X)
    {
        wsrc = "X";
    }
    w = sprintf(out, "%s%s %s r%u, r%u, off=%d imm=%d", n, suf, wsrc, dst, src,
                off, imm);
    (void)w;
}

static void dis_jmp(char *out, bpf_byte dst, bpf_byte src, int off, int imm,
                    bpf_byte code)
{
    const char *n;
    const char *suf;
    int w;

    n = "jmp";
    if (code == BPF_JMP_JA)
    {
        n = "ja";
    }
    if (code == BPF_JMP_JEQ)
    {
        n = "jeq";
    }
    if (code == BPF_JMP_JGT)
    {
        n = "jgt";
    }
    if (code == BPF_JMP_JGE)
    {
        n = "jge";
    }
    if (code == BPF_JMP_JSET)
    {
        n = "jset";
    }
    if (code == BPF_JMP_JNE)
    {
        n = "jne";
    }
    if (code == BPF_JMP_JSGT)
    {
        n = "jsgt";
    }
    if (code == BPF_JMP_JSGE)
    {
        n = "jsge";
    }
    if (code == BPF_JMP_CALL)
    {
        n = "call";
    }
    if (code == BPF_JMP_EXIT)
    {
        n = "exit";
    }
    if (code == BPF_JMP_JLT)
    {
        n = "jlt";
    }
    if (code == BPF_JMP_JLE)
    {
        n = "jle";
    }
    if (code == BPF_JMP_JSLT)
    {
        n = "jslt";
    }
    if (code == BPF_JMP_JSLE)
    {
        n = "jsle";
    }
    suf = "32";
    if (code == BPF_JMP_JA)
    {
        suf = "";
    }
    if (code == BPF_JMP_CALL)
    {
        suf = "";
    }
    if (code == BPF_JMP_EXIT)
    {
        suf = "";
    }
    w = sprintf(out, "%s%s r%u, r%u, off=%d imm=%d", n, suf, dst, src, off,
                imm);
    (void)w;
}

static void dis_mem(char *out, bpf_byte dst, bpf_byte src, int off, int imm,
                    bpf_byte mode, bpf_byte size, bpf_byte cls)
{
    const char *m;
    const char *z;
    const char *x;
    int w;

    m = "mem";
    if (mode == BPF_MODE_IMM)
    {
        m = "imm";
    }
    if (mode == BPF_MODE_MEMSX)
    {
        m = "memsx";
    }
    if (mode == BPF_MODE_ATOMIC)
    {
        m = "atomic";
    }
    z = "w";
    if (size == BPF_SIZE_H)
    {
        z = "h";
    }
    if (size == BPF_SIZE_B)
    {
        z = "b";
    }
    if (size == BPF_SIZE_DW)
    {
        z = "dw";
    }
    x = "";
    if (cls == BPF_CLS_LDX)
    {
        x = "x";
    }
    w = sprintf(out, "%s%s.%s r%u, r%u, off=%d imm=%d", m, x, z, dst, src, off,
                imm);
    (void)w;
}

void bpf_dis_one(const bpf_insn *in, char *out)
{
    bpf_byte cls;
    bpf_byte code;
    bpf_byte mode;
    bpf_byte size;
    bpf_byte s;
    bpf_byte dst;
    bpf_byte src;
    int off;
    int imm;

    cls = in->class;
    code = in->code;
    mode = in->mode;
    size = in->size;
    s = in->source;
    dst = in->dst_reg;
    src = in->src_reg;
    off = in->offset;
    imm = in->imm;
    if (cls == BPF_CLS_ALU)
    {
        dis_alu(out, dst, src, off, imm, s, code, cls);
        return;
    }
    if (cls == BPF_CLS_ALU64)
    {
        dis_alu(out, dst, src, off, imm, s, code, cls);
        return;
    }
    if (cls == BPF_CLS_JMP)
    {
        dis_jmp(out, dst, src, off, imm, code);
        return;
    }
    if (cls == BPF_CLS_JMP32)
    {
        dis_jmp(out, dst, src, off, imm, code);
        return;
    }
    dis_mem(out, dst, src, off, imm, mode, size, cls);
}

void bpf_groups_into(bpf_u32 conf, char *out, size_t cap)
{
    size_t used;
    char *p;

    used = 0;
    out[0] = '\0';
    used = strlen(out);
    if (used + 7 < cap)
    {
        p = out + used;
        sprintf(p, "base32\n");
    }
    if ((conf & BPF_CONF_BASE64) != 0u)
    {
        used = strlen(out);
        if (used + 7 < cap)
        {
            p = out + used;
            sprintf(p, "base64\n");
        }
    }
    if ((conf & BPF_CONF_DIVMUL32) != 0u)
    {
        used = strlen(out);
        if (used + 9 < cap)
        {
            p = out + used;
            sprintf(p, "divmul32\n");
        }
    }
    if ((conf & BPF_CONF_DIVMUL64) != 0u)
    {
        used = strlen(out);
        if (used + 9 < cap)
        {
            p = out + used;
            sprintf(p, "divmul64\n");
        }
    }
    if ((conf & BPF_CONF_ATOMIC32) != 0u)
    {
        used = strlen(out);
        if (used + 9 < cap)
        {
            p = out + used;
            sprintf(p, "atomic32\n");
        }
    }
    if ((conf & BPF_CONF_ATOMIC64) != 0u)
    {
        used = strlen(out);
        if (used + 9 < cap)
        {
            p = out + used;
            sprintf(p, "atomic64\n");
        }
    }
    if ((conf & BPF_CONF_PACKET) != 0u)
    {
        used = strlen(out);
        if (used + 7 < cap)
        {
            p = out + used;
            sprintf(p, "packet\n");
        }
    }
}
