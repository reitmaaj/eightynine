#include <stdlib.h>

#include "vm.h"

#define BPF_VM_CAPS 64

struct bpf_vm
{
    const bpf_program *prog;
    bpf_u32 n;
    bpf_u32 pc;
    bpf_u64 budget;
    bpf_regs regs;
    bpf_u32 retp[BPF_VM_CALL_DEPTH];
    bpf_u64 s6[BPF_VM_CALL_DEPTH];
    bpf_u64 s7[BPF_VM_CALL_DEPTH];
    bpf_u64 s8[BPF_VM_CALL_DEPTH];
    bpf_u64 s9[BPF_VM_CALL_DEPTH];
    bpf_u64 s10[BPF_VM_CALL_DEPTH];
    bpf_u32 retn;
    bpf_vm_state state;
    bpf_off64 ctx_base;
    bpf_off64 ctx_len;
    bpf_memory *mem;
    bpf_caps *caps;
    bpf_u32 req_type;
    bpf_u32 req_rights;
    bpf_byte has_req;
    bpf_u32 pending_import;
};

static bpf_u64 se_i32(int v)
{
    bpf_i64 x;

    x = (bpf_i64)v;
    return (bpf_u64)x;
}

static bpf_off64 size_bytes(bpf_byte size)
{
    bpf_off64 r;

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

static bpf_u64 se_load(bpf_u64 v, bpf_byte size)
{
    bpf_u64 r;
    bpf_byte b;
    bpf_u16 h;
    bpf_u32 w;
    bpf_i8 sb;
    bpf_i16 sh;
    bpf_i32 sw;
    bpf_i64 t;

    r = v;
    if (size == BPF_SIZE_B)
    {
        b = (bpf_byte)v;
        sb = (bpf_i8)b;
        t = (bpf_i64)sb;
        r = (bpf_u64)t;
    }
    if (size == BPF_SIZE_H)
    {
        h = (bpf_u16)v;
        sh = (bpf_i16)h;
        t = (bpf_i64)sh;
        r = (bpf_u64)t;
    }
    if (size == BPF_SIZE_W)
    {
        w = (bpf_u32)v;
        sw = (bpf_i32)w;
        t = (bpf_i64)sw;
        r = (bpf_u64)t;
    }
    return r;
}

static bpf_off64 eff_addr(const bpf_u64 *rr, bpf_u32 reg, int off)
{
    bpf_u64 base;
    bpf_i64 so;
    bpf_u64 sox;
    bpf_u64 ad;

    base = rr[reg];
    so = (bpf_i64)off;
    sox = (bpf_u64)so;
    ad = base + sox;
    return (bpf_off64)ad;
}

static void step_advance(bpf_vm *vm)
{
    bpf_u32 p;

    p = vm->pc;
    p = p + 1;
    vm->pc = p;
}

bpf_err bpf_vm_create(const bpf_program *prog, const bpf_region_cfg *cfg,
                      bpf_u32 ncfg, bpf_u64 budget, bpf_vm **out)
{
    bpf_vm *vm;
    bpf_memory *mem;
    bpf_err e;
    bpf_u32 i;
    bpf_u32 r;
    bpf_u32 cnt;
    bpf_off64 ctxb;
    bpf_off64 ctxl;
    bpf_off64 stk;
    bpf_caps *caps;

    ctxb = 0;
    ctxl = 0;
    stk = 0;
    i = 0;
    while (i < ncfg)
    {
        const bpf_region_cfg *rc;
        bpf_byte kind;

        rc = cfg + i;
        kind = rc->kind;
        if (kind == BPF_RINPUT)
        {
            ctxb = rc->guest;
            ctxl = rc->len;
        }
        if (kind == BPF_RSTACK)
        {
            stk = rc->guest;
        }
        i = i + 1;
    }
    mem = 0;
    e = bpf_memory_create(cfg, ncfg, &mem);
    if (e != BPF_OK)
    {
        return e;
    }
    vm = malloc(sizeof(bpf_vm));
    if (vm == 0)
    {
        bpf_memory_destroy(mem);
        return BPF_EADDR;
    }
    cnt = bpf_program_count(prog);
    vm->prog = prog;
    vm->n = cnt;
    vm->pc = 0;
    vm->budget = budget;
    vm->retn = 0;
    vm->state = BPF_VM_RUNNING;
    vm->ctx_base = ctxb;
    vm->ctx_len = ctxl;
    vm->mem = mem;
    caps = 0;
    e = bpf_caps_create(BPF_VM_CAPS, &caps);
    if (e != BPF_OK)
    {
        bpf_memory_destroy(mem);
        free(vm);
        return e;
    }
    vm->caps = caps;
    vm->has_req = 0;
    vm->req_type = 0;
    vm->req_rights = 0;
    vm->pending_import = 0;
    r = 0;
    while (r < BPF_NREG)
    {
        vm->regs.r[r] = 0;
        r = r + 1;
    }
    vm->regs.r[1] = ctxb;
    vm->regs.r[2] = ctxl;
    vm->regs.r[10] = stk;
    *out = vm;
    return BPF_OK;
}

void bpf_vm_destroy(bpf_vm *vm)
{
    bpf_memory *mem;
    bpf_caps *caps;

    if (vm == 0)
    {
        return;
    }
    mem = vm->mem;
    bpf_memory_destroy(mem);
    caps = vm->caps;
    bpf_caps_destroy(caps);
    free(vm);
}

bpf_vm_state bpf_vm_get_state(const bpf_vm *vm)
{
    return vm->state;
}

bpf_u64 bpf_vm_reg(const bpf_vm *vm, bpf_u32 idx)
{
    const bpf_u64 *rr;
    bpf_u64 v;

    rr = vm->regs.r;
    v = rr[idx];
    return v;
}

void bpf_vm_set_budget(bpf_vm *vm, bpf_u64 budget)
{
    vm->budget = budget;
}

bpf_err bpf_vm_reg_cap(bpf_vm *vm, bpf_u32 type, bpf_u32 rights, void *host,
                       bpf_handle *out)
{
    bpf_caps *caps;
    bpf_err e;

    caps = vm->caps;
    e = bpf_caps_alloc(caps, type, rights, host, out);
    return e;
}

bpf_err bpf_vm_revoke_cap(bpf_vm *vm, bpf_handle h)
{
    bpf_caps *caps;
    bpf_err e;

    caps = vm->caps;
    e = bpf_caps_revoke(caps, h);
    return e;
}

bpf_err bpf_vm_set_import_gate(bpf_vm *vm, bpf_u32 type, bpf_u32 rights)
{
    vm->req_type = type;
    vm->req_rights = rights;
    vm->has_req = 1;
    return BPF_OK;
}

bpf_u32 bpf_vm_pending_import(const bpf_vm *vm)
{
    return vm->pending_import;
}

bpf_err bpf_vm_complete(bpf_vm *vm, bpf_u64 status)
{
    bpf_vm_state st;
    bpf_u64 *rr;
    bpf_u32 r;

    st = vm->state;
    if (st != BPF_VM_WAITING)
    {
        return BPF_ESTALE;
    }
    rr = vm->regs.r;
    rr[0] = status;
    r = 1;
    while (r <= 5u)
    {
        rr[r] = 0;
        r = r + 1;
    }
    vm->state = BPF_VM_RUNNING;
    step_advance(vm);
    return BPF_OK;
}

/* Decide whether a helper (import) call with id `im` may suspend, gating on
 * the capability handle in r1 when a requirement is set. */
static bpf_vm_state exec_helper(bpf_vm *vm, bpf_u32 im)
{
    bpf_caps *caps;
    bpf_u64 *rr;
    bpf_byte gate;
    bpf_u32 ty;
    bpf_u32 rq;
    bpf_u32 rtype;
    bpf_u32 rrights;
    bpf_byte ok;
    bpf_err e;
    bpf_handle h;
    bpf_u64 hv;

    caps = vm->caps;
    rr = vm->regs.r;
    gate = vm->has_req;
    rtype = vm->req_type;
    rrights = vm->req_rights;
    ok = 1;
    if (gate)
    {
        ok = 0;
        hv = rr[1];
        h = (bpf_handle)hv;
        ty = 0;
        rq = 0;
        e = bpf_caps_lookup(caps, h, &ty, &rq, 0);
        if (e == BPF_OK)
        {
            if (ty == rtype)
            {
                if ((rq & rrights) == rrights)
                {
                    ok = 1;
                }
            }
        }
    }
    if (!ok)
    {
        return BPF_VM_TRAPPED;
    }
    vm->pending_import = im;
    vm->state = BPF_VM_WAITING;
    return BPF_VM_WAITING;
}

/* ------------------------------------------------------------------ */

static bpf_vm_state exec_alu(bpf_vm *vm, const bpf_insn *in)
{
    bpf_regs *rp;
    bpf_u32 dst;

    dst = in->dst_reg;
    if (dst == 10u)
    {
        return BPF_VM_TRAPPED;
    }
    rp = &vm->regs;
    bpf_alu(rp, in);
    step_advance(vm);
    return BPF_VM_RUNNING;
}

static bpf_vm_state exec_ldimm(bpf_vm *vm, const bpf_insn *in)
{
    bpf_u64 *rr;
    bpf_u32 dst;
    int imm;
    bpf_u32 nxti;
    bpf_u32 lo;
    bpf_u64 hi;
    bpf_u64 lo64;
    bpf_u64 val;

    rr = vm->regs.r;
    dst = in->dst_reg;
    if (dst == 10u)
    {
        return BPF_VM_TRAPPED;
    }
    imm = in->imm;
    nxti = in->next_imm;
    lo = (bpf_u32)imm;
    hi = (bpf_u64)nxti;
    lo64 = (bpf_u64)lo;
    val = (hi << 32) | lo64;
    rr[dst] = val;
    step_advance(vm);
    return BPF_VM_RUNNING;
}

static bpf_vm_state exec_ldx(bpf_vm *vm, const bpf_insn *in)
{
    bpf_u64 *rr;
    bpf_u32 dst;
    bpf_u32 base;
    bpf_byte size;
    bpf_byte mode;
    int off;
    bpf_off64 addr;
    bpf_off64 bytes;
    bpf_byte wb;
    bpf_u64 val;
    bpf_err e;
    bpf_memory *mem;

    rr = vm->regs.r;
    dst = in->dst_reg;
    base = in->src_reg;
    size = in->size;
    mode = in->mode;
    off = in->offset;
    if (dst == 10u)
    {
        return BPF_VM_TRAPPED;
    }
    addr = eff_addr(rr, base, off);
    bytes = size_bytes(size);
    wb = (bpf_byte)bytes;
    mem = vm->mem;
    val = 0;
    e = bpf_mem_load(mem, addr, wb, &val);
    if (e != BPF_OK)
    {
        return BPF_VM_TRAPPED;
    }
    if (mode == BPF_MODE_MEMSX)
    {
        val = se_load(val, size);
    }
    rr[dst] = val;
    step_advance(vm);
    return BPF_VM_RUNNING;
}

static bpf_vm_state exec_store(bpf_vm *vm, const bpf_insn *in, bpf_u64 val)
{
    bpf_u64 *rr;
    bpf_u32 base;
    bpf_byte size;
    int off;
    bpf_off64 addr;
    bpf_off64 bytes;
    bpf_byte wb;
    bpf_err e;
    bpf_memory *mem;

    rr = vm->regs.r;
    base = in->dst_reg;
    size = in->size;
    off = in->offset;
    addr = eff_addr(rr, base, off);
    bytes = size_bytes(size);
    wb = (bpf_byte)bytes;
    mem = vm->mem;
    e = bpf_mem_store(mem, addr, wb, val);
    if (e != BPF_OK)
    {
        return BPF_VM_TRAPPED;
    }
    step_advance(vm);
    return BPF_VM_RUNNING;
}

static bpf_u64 jmp_imm(bpf_byte is32, int imm)
{
    bpf_u32 low;
    bpf_u64 s;

    if (is32)
    {
        low = (bpf_u32)imm;
        s = (bpf_u64)low;
    }
    else
    {
        s = se_i32(imm);
    }
    return s;
}

static bpf_vm_state exec_jmp(bpf_vm *vm, const bpf_insn *in,
                             const bpf_prog_insn *pi, bpf_byte is32)
{
    bpf_u64 *rr;
    bpf_byte code;
    bpf_byte source;
    bpf_u32 dst;
    bpf_u32 src;
    bpf_u32 src_reg;
    int imm;
    bpf_u64 d;
    bpf_u64 s;
    bpf_byte taken;
    bpf_u32 retn;
    bpf_u32 target;
    bpf_u32 rv;
    bpf_u32 nv;
    bpf_u32 nxtp;
    bpf_u32 f;
    bpf_u64 sv;

    rr = vm->regs.r;
    code = in->code;
    source = in->source;
    dst = in->dst_reg;
    src = in->src_reg;
    src_reg = in->src_reg;
    imm = in->imm;
    if (code == BPF_JMP_JA)
    {
        target = pi->target;
        vm->pc = target;
        return BPF_VM_RUNNING;
    }
    if (code == BPF_JMP_EXIT)
    {
        retn = vm->retn;
        if (retn > 0u)
        {
            retn = retn - 1;
            vm->retn = retn;
            f = retn;
            sv = vm->s6[f];
            rr[6] = sv;
            sv = vm->s7[f];
            rr[7] = sv;
            sv = vm->s8[f];
            rr[8] = sv;
            sv = vm->s9[f];
            rr[9] = sv;
            sv = vm->s10[f];
            rr[10] = sv;
            rv = vm->retp[f];
            vm->pc = rv;
            return BPF_VM_RUNNING;
        }
        vm->state = BPF_VM_RETURNED;
        return BPF_VM_RETURNED;
    }
    if (code == BPF_JMP_CALL)
    {
        if (src_reg == 1u)
        {
            retn = vm->retn;
            if (retn >= BPF_VM_CALL_DEPTH)
            {
                return BPF_VM_TRAPPED;
            }
            f = retn;
            nxtp = vm->pc;
            nxtp = nxtp + 1;
            vm->retp[f] = nxtp;
            sv = rr[6];
            vm->s6[f] = sv;
            sv = rr[7];
            vm->s7[f] = sv;
            sv = rr[8];
            vm->s8[f] = sv;
            sv = rr[9];
            vm->s9[f] = sv;
            sv = rr[10];
            vm->s10[f] = sv;
            nv = retn + 1;
            vm->retn = nv;
            target = pi->target;
            vm->pc = target;
            return BPF_VM_RUNNING;
        }
        {
            bpf_u32 imid;

            imid = (bpf_u32)imm;
            return exec_helper(vm, imid);
        }
    }
    d = rr[dst];
    if (source == BPF_SRC_K)
    {
        s = jmp_imm(is32, imm);
    }
    else
    {
        s = rr[src];
    }
    taken = bpf_cond_taken(code, is32, d, s);
    if (taken)
    {
        target = pi->target;
        vm->pc = target;
    }
    else
    {
        step_advance(vm);
    }
    return BPF_VM_RUNNING;
}

/* ------------------------------------------------------------------ */

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
    if (op == 0xA0u)
    {
        r = old ^ src;
    }
    if (op == 0xE0u)
    {
        r = src;
    }
    if (op == 0xF0u)
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
    if (op == 0xA0u)
    {
        r = old ^ src;
    }
    if (op == 0xE0u)
    {
        r = src;
    }
    if (op == 0xF0u)
    {
        if (old == r0)
        {
            r = src;
        }
    }
    return r;
}

/* An atomic read-modify-write over exclusively owned guest memory. base is
 * dst_reg, operand is src_reg; op/width come from the immediate and size. */
static bpf_vm_state exec_atomic(bpf_vm *vm, const bpf_insn *in)
{
    bpf_u64 *rr;
    bpf_u32 base;
    bpf_u32 sreg;
    bpf_byte size;
    int im;
    int off;
    bpf_u32 opraw;
    bpf_byte fetch;
    bpf_u32 op;
    bpf_off64 addr;
    bpf_off64 bytes;
    bpf_byte wb;
    bpf_u64 srcv;
    bpf_u64 r0v;
    bpf_u64 old;
    bpf_u64 newv;
    bpf_u64 oldret;
    bpf_err e;
    bpf_memory *mem;
    bpf_byte is_cmp;
    bpf_byte is32;

    rr = vm->regs.r;
    base = in->dst_reg;
    sreg = in->src_reg;
    size = in->size;
    im = in->imm;
    off = in->offset;
    addr = eff_addr(rr, base, off);
    mem = vm->mem;
    opraw = (bpf_u32)im;
    fetch = (bpf_byte)(opraw & 0x01u);
    op = opraw & 0xFEu;
    srcv = rr[sreg];
    r0v = rr[0];
    is32 = (bpf_byte)(size == BPF_SIZE_W);
    if (is32)
    {
        bytes = 4;
    }
    else
    {
        bytes = 8;
    }
    wb = (bpf_byte)bytes;
    old = 0;
    e = bpf_mem_load(mem, addr, wb, &old);
    if (e != BPF_OK)
    {
        return BPF_VM_TRAPPED;
    }
    if (is32)
    {
        bpf_u32 oldw;
        bpf_u32 srcw;
        bpf_u32 r0w;
        bpf_u32 nw;

        oldw = (bpf_u32)old;
        srcw = (bpf_u32)srcv;
        r0w = (bpf_u32)r0v;
        nw = atom32(op, oldw, srcw, r0w);
        newv = (bpf_u64)nw;
        oldret = (bpf_u64)oldw;
    }
    else
    {
        newv = atom64(op, old, srcv, r0v);
        oldret = old;
    }
    e = bpf_mem_store(mem, addr, wb, newv);
    if (e != BPF_OK)
    {
        return BPF_VM_TRAPPED;
    }
    is_cmp = (bpf_byte)(op == 0xF0u);
    if (is_cmp)
    {
        rr[0] = oldret;
    }
    if (fetch)
    {
        if (!is_cmp)
        {
            rr[sreg] = oldret;
        }
    }
    step_advance(vm);
    return BPF_VM_RUNNING;
}

static bpf_err get_insn(const bpf_vm *vm, bpf_u32 idx, bpf_prog_insn *pi)
{
    bpf_err e;
    const bpf_program *prog;

    prog = vm->prog;
    e = bpf_program_get(prog, idx, pi);
    return e;
}

bpf_vm_state bpf_vm_step(bpf_vm *vm)
{
    bpf_u32 pc;
    bpf_u32 n;
    bpf_prog_insn pi;
    bpf_insn *in;
    bpf_byte cls;
    bpf_u64 *rr;
    bpf_err e;
    bpf_u64 bud;
    bpf_vm_state st;
    bpf_u32 src;
    bpf_u64 val;
    int imm;

    st = vm->state;
    if (st == BPF_VM_RETURNED)
    {
        return BPF_VM_RETURNED;
    }
    if (st == BPF_VM_TRAPPED)
    {
        return BPF_VM_TRAPPED;
    }
    if (st == BPF_VM_WAITING)
    {
        return BPF_VM_WAITING;
    }
    pc = vm->pc;
    n = vm->n;
    if (pc >= n)
    {
        vm->state = BPF_VM_TRAPPED;
        return BPF_VM_TRAPPED;
    }
    bud = vm->budget;
    if (bud == 0u)
    {
        vm->state = BPF_VM_EXHAUSTED;
        return BPF_VM_EXHAUSTED;
    }
    bud = bud - 1u;
    vm->budget = bud;
    e = get_insn(vm, pc, &pi);
    if (e != BPF_OK)
    {
        vm->state = BPF_VM_TRAPPED;
        return BPF_VM_TRAPPED;
    }
    in = &pi.in;
    rr = vm->regs.r;
    cls = in->class;
    if (cls == BPF_CLS_ALU)
    {
        return exec_alu(vm, in);
    }
    if (cls == BPF_CLS_ALU64)
    {
        return exec_alu(vm, in);
    }
    if (cls == BPF_CLS_LD)
    {
        return exec_ldimm(vm, in);
    }
    if (cls == BPF_CLS_LDX)
    {
        return exec_ldx(vm, in);
    }
    if (cls == BPF_CLS_ST)
    {
        imm = in->imm;
        val = se_i32(imm);
        return exec_store(vm, in, val);
    }
    if (cls == BPF_CLS_STX)
    {
        bpf_byte mde;

        mde = in->mode;
        if (mde == BPF_MODE_ATOMIC)
        {
            return exec_atomic(vm, in);
        }
        src = in->src_reg;
        val = rr[src];
        return exec_store(vm, in, val);
    }
    if (cls == BPF_CLS_JMP)
    {
        return exec_jmp(vm, in, &pi, 0);
    }
    if (cls == BPF_CLS_JMP32)
    {
        return exec_jmp(vm, in, &pi, 1);
    }
    vm->state = BPF_VM_TRAPPED;
    return BPF_VM_TRAPPED;
}
