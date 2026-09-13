#include <stddef.h>
#include <stdlib.h>

#include "program.h"

/* Private per-instruction metadata kept in the opaque program. The decoded
 * instruction array is kept as a contiguous bpf_insn array so the structural
 * validator and conformance fold run directly over it. Each p_rec records the
 * instruction's start 64-bit slot, its resolved target index, and a flag byte
 * (bit 1 = has_target, bit 2 = is_call_local). The public bpf_prog_insn is
 * produced on demand by bpf_program_get. */
typedef struct p_rec
{
    bpf_u32 slot;
    bpf_u32 target;
    bpf_byte flag;
} p_rec;

struct bpf_program
{
    bpf_u32 n;
    bpf_u32 conf;
    bpf_insn *ins;
    p_rec *rec;
};

static bpf_u32 insn_slots(const bpf_insn *in)
{
    bpf_u32 s;

    bpf_byte wide;

    s = 1;
    wide = in->is_wide;
    if (wide)
    {
        s = 2;
    }
    return s;
}

static bpf_byte is_alu_or_jmp(bpf_byte cls)
{
    bpf_byte r;

    r = 0;
    if (cls == BPF_CLS_ALU)
    {
        r = 1;
    }
    if (cls == BPF_CLS_ALU64)
    {
        r = 1;
    }
    if (cls == BPF_CLS_JMP)
    {
        r = 1;
    }
    if (cls == BPF_CLS_JMP32)
    {
        r = 1;
    }
    return r;
}

/* Reject reserved ALU/JMP operation codes (0xE, 0xF). */
static bpf_err check_opcode(const bpf_insn *in)
{
    bpf_err e;
    bpf_byte code;
    bpf_byte cls;
    bpf_byte am;

    e = BPF_OK;
    code = in->code;
    cls = in->class;
    am = is_alu_or_jmp(cls);
    if (am)
    {
        if (code >= 0x0Eu)
        {
            e = BPF_EUNSUP;
        }
    }
    return e;
}

/* 1 if an atomic operation writes its source register (fetch and XCHG). */
static bpf_byte atomic_writes_src(const bpf_insn *in)
{
    bpf_byte r;
    int im;
    bpf_u32 op;
    bpf_byte fetch;
    bpf_u32 masked;

    r = 0;
    im = in->imm;
    op = (bpf_u32)im;
    fetch = (bpf_byte)(op & 0x01u);
    masked = op & 0xFEu;
    if (masked == 0xE0u)
    {
        r = 1;
    }
    if (fetch)
    {
        if (masked != 0xF0u)
        {
            r = 1;
        }
    }
    return r;
}

/* 1 if an instruction writes r10, directly or implicitly (atomic fetch). */
static bpf_byte writes_r10(const bpf_insn *in)
{
    bpf_byte r;
    bpf_byte cls;
    bpf_byte mode;
    bpf_byte dst;
    bpf_byte src;
    bpf_byte aws;

    r = 0;
    cls = in->class;
    mode = in->mode;
    dst = in->dst_reg;
    src = in->src_reg;
    if (cls == BPF_CLS_ALU)
    {
        if (dst == 10u)
        {
            r = 1;
        }
    }
    if (cls == BPF_CLS_ALU64)
    {
        if (dst == 10u)
        {
            r = 1;
        }
    }
    if (cls == BPF_CLS_LD)
    {
        if (dst == 10u)
        {
            r = 1;
        }
    }
    if (cls == BPF_CLS_LDX)
    {
        if (dst == 10u)
        {
            r = 1;
        }
    }
    if (cls == BPF_CLS_STX)
    {
        if (mode == BPF_MODE_ATOMIC)
        {
            aws = atomic_writes_src(in);
            if (aws)
            {
                if (src == 10u)
                {
                    r = 1;
                }
            }
        }
    }
    return r;
}

/* Reject atomic encodings outside the supported operation set. */
static bpf_err check_atomic(const bpf_insn *in)
{
    bpf_err e;
    int im;
    bpf_u32 op;
    bpf_u32 masked;
    bpf_byte cls;
    bpf_byte mode;

    e = BPF_OK;
    cls = in->class;
    mode = in->mode;
    if (cls == BPF_CLS_STX)
    {
        if (mode == BPF_MODE_ATOMIC)
        {
            im = in->imm;
            op = (bpf_u32)im;
            masked = op & 0xFEu;
            if (masked != 0x00u)
            {
                if (masked != 0x40u)
                {
                    if (masked != 0x50u)
                    {
                        if (masked != 0xA0u)
                        {
                            if (masked != 0xE0u)
                            {
                                if (masked != 0xF0u)
                                {
                                    e = BPF_EUNSUP;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return e;
}

/* Check that reserved continuation fields of a wide instruction are zero. */
static bpf_err check_wide_res(const bpf_byte *raw, bpf_u32 pos)
{
    bpf_err e;
    bpf_byte b;
    bpf_u32 k;

    e = BPF_OK;
    k = pos + 8;
    b = raw[k];
    if (b != 0u)
    {
        e = BPF_ERES;
    }
    if (e == BPF_OK)
    {
        k = pos + 9;
        b = raw[k];
        if (b != 0u)
        {
            e = BPF_ERES;
        }
    }
    if (e == BPF_OK)
    {
        k = pos + 10;
        b = raw[k];
        if (b != 0u)
        {
            e = BPF_ERES;
        }
    }
    if (e == BPF_OK)
    {
        k = pos + 11;
        b = raw[k];
        if (b != 0u)
        {
            e = BPF_ERES;
        }
    }
    return e;
}

static bpf_byte is_local_call(const bpf_insn *in)
{
    bpf_byte r;
    bpf_byte cls;
    bpf_byte code;
    bpf_byte src;

    r = 0;
    cls = in->class;
    code = in->code;
    src = in->src_reg;
    if (cls == BPF_CLS_JMP)
    {
        if (code == BPF_JMP_CALL)
        {
            if (src == 1u)
            {
                r = 1;
            }
        }
    }
    return r;
}

/* Returns 1 if this instruction carries a jump/loop/conditional target and
 * sets *imm_delta when the target is the 32-bit immediate form (JMP32 JA). */
static bpf_byte branch_delta(const bpf_insn *in, bpf_byte *is_imm)
{
    bpf_byte cls;
    bpf_byte code;
    bpf_byte r;
    bpf_byte ll;

    r = 0;
    cls = in->class;
    code = in->code;
    *is_imm = 0;
    if (cls == BPF_CLS_JMP)
    {
        if (code == BPF_JMP_JA)
        {
            r = 1;
        }
        if (code == BPF_JMP_CALL)
        {
            ll = is_local_call(in);
            if (ll)
            {
                r = 1;
                *is_imm = 1;
            }
        }
        if (code == BPF_JMP_EXIT)
        {
            r = 0;
        }
        if (code != BPF_JMP_JA)
        {
            if (code != BPF_JMP_CALL)
            {
                if (code != BPF_JMP_EXIT)
                {
                    r = 1;
                }
            }
        }
    }
    if (cls == BPF_CLS_JMP32)
    {
        if (code == BPF_JMP_JA)
        {
            r = 1;
            *is_imm = 1;
        }
        else
        {
            r = 1;
        }
    }
    return r;
}

/* 1 unless a byte count overflows the host size type. */
static bpf_byte sizes_ok(size_t need)
{
    size_t lim;
    bpf_byte r;

    r = 1;
    lim = (size_t)(-1);
    if (need > lim)
    {
        r = 0;
    }
    return r;
}

/* Return the instruction index whose start slot equals `target`, or -1 if no
 * instruction starts there (an interior slot of a wide instruction, or an
 * unmapped slot). */
static int slot_index(const p_rec *rec, bpf_u32 n, bpf_u32 target)
{
    bpf_u32 j;
    int r;

    r = -1;
    j = 0;
    while (j < n)
    {
        bpf_u32 sl;

        sl = rec[j].slot;
        if (sl == target)
        {
            r = (int)j;
        }
        j = j + 1;
    }
    return r;
}

bpf_err bpf_program_load(const bpf_byte *raw, bpf_u32 len,
                         const bpf_profile *prof, bpf_program **out)
{
    bpf_program *p;
    p_rec *rec;
    bpf_insn *ins;
    bpf_u32 cap;
    bpf_u32 n;
    bpf_u32 total;
    bpf_u32 conf;
    bpf_err e;
    bpf_u32 cursor;
    bpf_u32 i;
    bpf_u32 dummy;
    bpf_u32 allowed;
    bpf_u32 mx;
    size_t need;
    bpf_byte cont;
    bpf_byte ok;

    if (len == 0u)
    {
        return BPF_EEMPTY;
    }
    cont = (bpf_byte)(len & 0x07u);
    if (cont != 0u)
    {
        return BPF_ETRUNC;
    }

    cap = len >> 3;
    need = cap * sizeof(bpf_insn);
    ok = sizes_ok(need);
    if (!ok)
    {
        return BPF_ETRUNC;
    }
    ins = malloc(need);
    if (ins == 0)
    {
        return BPF_ETRUNC;
    }
    e = bpf_decode(raw, len, ins, cap, &n);
    if (e != BPF_OK)
    {
        free(ins);
        return e;
    }
    mx = 0;
    if (prof != 0)
    {
        mx = prof->max_insn;
    }
    if (mx != 0u)
    {
        if (n > mx)
        {
            free(ins);
            return BPF_ECOUNT;
        }
    }
    need = n * sizeof(p_rec);
    ok = sizes_ok(need);
    if (!ok)
    {
        free(ins);
        return BPF_ETRUNC;
    }
    p = malloc(sizeof(bpf_program));
    if (p == 0)
    {
        free(ins);
        return BPF_ETRUNC;
    }
    rec = malloc(need);
    if (rec == 0)
    {
        free(ins);
        free(p);
        return BPF_ETRUNC;
    }
    p->ins = ins;
    p->rec = rec;
    p->n = n;
    p->conf = 0;
    /* Slot pass: record each instruction's start slot, tag local calls, and
     * validate wide reserved continuation bytes against the raw input. */
    cursor = 0;
    i = 0;
    while (i < n)
    {
        bpf_insn *cur;
        bpf_byte wide;
        bpf_byte flags;
        bpf_byte cl;

        bpf_u32 sv;

        cur = ins + i;
        sv = cursor >> 3;
        rec[i].slot = sv;
        rec[i].target = 0;
        flags = 0;
        cl = is_local_call(cur);
        if (cl)
        {
            flags = 0x04u;
        }
        rec[i].flag = flags;
        wide = cur->is_wide;
        if (wide)
        {
            e = check_wide_res(raw, cursor);
            if (e != BPF_OK)
            {
                free(rec);
                free(ins);
                free(p);
                return e;
            }
            cursor = cursor + BPF_INS_WIDE;
        }
        else
        {
            cursor = cursor + BPF_INS_BASIC;
        }
        i = i + 1;
    }
    total = len >> 3;
    conf = 0;
    i = 0;
    while (i < n)
    {
        bpf_insn *cur;
        bpf_u32 c;

        cur = ins + i;
        c = bpf_insn_conf(cur);
        conf = conf | c;
        i = i + 1;
    }
    p->conf = conf;
    e = bpf_validate(ins, n, &dummy);
    if (e != BPF_OK)
    {
        free(rec);
        free(ins);
        free(p);
        return e;
    }
    i = 0;
    while (i < n)
    {
        bpf_insn *cur;
        bpf_err oe;
        bpf_byte wr;

        cur = ins + i;
        oe = check_opcode(cur);
        if (oe != BPF_OK)
        {
            free(rec);
            free(ins);
            free(p);
            return oe;
        }
        oe = check_atomic(cur);
        if (oe != BPF_OK)
        {
            free(rec);
            free(ins);
            free(p);
            return oe;
        }
        wr = writes_r10(cur);
        if (wr)
        {
            free(rec);
            free(ins);
            free(p);
            return BPF_ER10;
        }
        i = i + 1;
    }
    allowed = 0;
    if (prof != 0)
    {
        allowed = prof->allowed;
    }
    if (allowed != 0u)
    {
        bpf_u32 notall;

        notall = conf & ~allowed;
        if (notall != 0u)
        {
            free(rec);
            free(ins);
            free(p);
            return BPF_EPROF;
        }
    }
    i = 0;
    while (i < n)
    {
        bpf_insn *cur;
        bpf_byte is_imm;
        bpf_byte bd;
        int delta;
        bpf_u32 sl;
        bpf_u32 wsl;
        bpf_u32 ns;
        bpf_u64 nxt;
        bpf_i64 d;
        bpf_i64 ts;
        bpf_i64 tt;
        bpf_u32 sidx;
        int idx;
        bpf_byte flags;
        bpf_u32 tu;
        bpf_byte fb;
        bpf_i64 dd;
        bpf_u32 ti;

        cur = ins + i;
        bd = branch_delta(cur, &is_imm);
        if (bd)
        {
            delta = 0;
            if (is_imm)
            {
                delta = cur->imm;
            }
            else
            {
                delta = cur->offset;
            }
            sl = rec[i].slot;
            wsl = insn_slots(cur);
            ns = sl + wsl;
            nxt = (bpf_u64)ns;
            d = (bpf_i64)nxt;
            dd = (bpf_i64)delta;
            d = d + dd;
            ts = d;
            if (ts < 0)
            {
                free(rec);
                free(ins);
                free(p);
                return BPF_EBRANCH;
            }
            tt = (bpf_i64)total;
            if (ts >= tt)
            {
                free(rec);
                free(ins);
                free(p);
                return BPF_EBRANCH;
            }
            sidx = (bpf_u32)ts;
            idx = slot_index(rec, n, sidx);
            if (idx < 0)
            {
                free(rec);
                free(ins);
                free(p);
                return BPF_EBRANCH;
            }
            ti = (bpf_u32)idx;
            rec[i].target = ti;
            flags = rec[i].flag;
            tu = (bpf_u32)flags;
            tu = tu | 0x02u;
            fb = (bpf_byte)tu;
            rec[i].flag = fb;
        }
        i = i + 1;
    }
    *out = p;
    return BPF_OK;
}

void bpf_program_destroy(bpf_program *prog)
{
    p_rec *r2;
    bpf_insn *i2;

    if (prog == 0)
    {
        return;
    }
    r2 = prog->rec;
    free(r2);
    i2 = prog->ins;
    free(i2);
    free(prog);
}

bpf_u32 bpf_program_count(const bpf_program *prog)
{
    return prog->n;
}

bpf_u32 bpf_program_conformance(const bpf_program *prog)
{
    return prog->conf;
}

bpf_err bpf_program_get(const bpf_program *prog, bpf_u32 idx,
                        bpf_prog_insn *out)
{
    bpf_u32 n;
    const bpf_insn *cur;
    const p_rec *r;
    bpf_u32 flag;
    bpf_byte wide;
    bpf_byte has;
    bpf_byte is_call_local;
    bpf_insn *bi;
    const p_rec *rr;
    bpf_byte bo;
    bpf_byte bc;
    bpf_byte bk;
    bpf_byte bs;
    bpf_byte bm;
    bpf_byte bz;
    bpf_byte br;
    bpf_byte bd;
    int iv;
    bpf_u32 nxti;
    bpf_u32 sl;
    bpf_u32 tg;

    n = prog->n;
    if (idx >= n)
    {
        return BPF_EREG;
    }
    bi = prog->ins;
    cur = bi + idx;
    rr = prog->rec;
    r = rr + idx;
    bo = cur->opcode;
    out->in.opcode = bo;
    bc = cur->class;
    out->in.class = bc;
    bk = cur->code;
    out->in.code = bk;
    bs = cur->source;
    out->in.source = bs;
    bm = cur->mode;
    out->in.mode = bm;
    bz = cur->size;
    out->in.size = bz;
    br = cur->src_reg;
    out->in.src_reg = br;
    bd = cur->dst_reg;
    out->in.dst_reg = bd;
    iv = cur->offset;
    out->in.offset = iv;
    iv = cur->imm;
    out->in.imm = iv;
    nxti = cur->next_imm;
    out->in.next_imm = nxti;
    wide = cur->is_wide;
    out->in.is_wide = wide;
    sl = r->slot;
    out->slot = sl;
    tg = r->target;
    out->target = tg;
    flag = r->flag;
    has = 0;
    is_call_local = 0;
    if ((flag & 0x04u) != 0u)
    {
        is_call_local = 1;
    }
    if ((flag & 0x02u) != 0u)
    {
        has = 1;
    }
    out->is_wide = wide;
    out->has_target = has;
    out->is_call_local = is_call_local;
    return BPF_OK;
}
