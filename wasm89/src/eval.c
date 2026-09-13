#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eval.h"

#define W89_I32_MIN 0x80000000u
#define W89_I32_NEG1 0xFFFFFFFFu
#define W89_I64_MIN 0x8000000000000000UL
#define W89_I64_NEG1 0xFFFFFFFFFFFFFFFFUL

static void frame_free(w89_frame *f);

static int match_ft_x(const w89_typeenv *ae, const w89_ft *a,
                      const w89_typeenv *be, const w89_ft *b);

static w89_err grow(void **pp, w89_u32 *cap, w89_u32 need, w89_u32 size);

/* Value/ref constructors. */

w89_value w89_value_num(w89_u64 n)
{
    w89_value v;
    memset(&v, 0, sizeof(w89_value));
    v.u.num = n;
    return v;
}

w89_value w89_value_v128(const w89_v128 *w)
{
    w89_value v;
    w89_v128 vec;
    memset(&v, 0, sizeof(w89_value));
    vec = *w;
    v.u.vec = vec;
    return v;
}

w89_value w89_value_ref(const w89_ref *r)
{
    w89_value v;
    w89_ref rv;
    memset(&v, 0, sizeof(w89_value));
    v.is_ref = 1;
    rv = *r;
    v.u.ref = rv;
    return v;
}

w89_ref w89_ref_null(void)
{
    w89_ref r;
    memset(&r, 0, sizeof(w89_ref));
    r.kind = W89_RK_NULL;
    return r;
}

w89_ref w89_ref_func(w89_funcinst *f)
{
    w89_ref r;
    memset(&r, 0, sizeof(w89_ref));
    r.kind = W89_RK_FUNC;
    r.u.func = f;
    return r;
}

w89_ref w89_ref_extern(w89_u64 e)
{
    w89_ref r;
    memset(&r, 0, sizeof(w89_ref));
    r.kind = W89_RK_EXTERN;
    r.u.ext = e;
    return r;
}

w89_ref w89_ref_exn(w89_exn *e)
{
    w89_ref r;
    memset(&r, 0, sizeof(w89_ref));
    r.kind = W89_RK_EXN;
    r.u.exn = e;
    return r;
}

w89_exn *w89_exn_alloc(w89_store *s, w89_taginst *tag, w89_value *args,
                       w89_u32 nargs)
{
    w89_exn *e;
    w89_err err;
    void **pp;
    w89_u32 *cp;
    w89_u32 ne;
    w89_u32 n1;
    w89_u32 idx;
    w89_u32 cnt;
    w89_exn ***q;
    w89_value *ea;
    e = malloc(sizeof(w89_exn));
    if (e == 0) {
        free(args);
        return NULL;
    }
    memset(e, 0, sizeof(w89_exn));
    e->tag = tag;
    e->args = args;
    e->nargs = nargs;
    q = &s->exns;
    pp = (void **)q;
    cp = &s->cexns;
    ne = s->nexns;
    n1 = ne + 1;
    err = grow(pp, cp, n1, sizeof(w89_exn *));
    if (err != W89_ERR_NONE) {
        ea = e->args;
        free(ea);
        free(e);
        return NULL;
    }
    idx = s->nexns;
    s->exns[idx] = e;
    cnt = s->nexns;
    cnt = cnt + 1;
    s->nexns = cnt;
    return e;
}

int w89_ref_is_null(const w89_ref *r)
{
    w89_u32 k;
    k = r->kind;
    return k == W89_RK_NULL;
}

static int eq_ref(const w89_ref *a, const w89_ref *b)
{
    w89_u32 ka;
    w89_u32 kb;
    w89_funcinst *fa;
    w89_funcinst *fb;
    w89_u64 ea;
    w89_u64 eb;
    w89_exn *xa;
    w89_exn *xb;
    ka = a->kind;
    kb = b->kind;
    if (ka != kb) {
        return 0;
    }
    switch (ka) {
    case W89_RK_NULL:
        return 1;
    case W89_RK_FUNC:
        fa = a->u.func;
        fb = b->u.func;
        return fa == fb;
    case W89_RK_EXTERN:
        ea = a->u.ext;
        eb = b->u.ext;
        return ea == eb;
    case W89_RK_EXN:
        xa = a->u.exn;
        xb = b->u.exn;
        return xa == xb;
    }
    return 0;
}

/* Pure control-flow helpers. */

w89_err w89_block_extent(const w89_instr *items, w89_u32 n, w89_u32 i,
                         w89_u32 *body_start, w89_u32 *body_end,
                         w89_u32 *has_else, w89_u32 *else_pos)
{
    w89_u32 depth;
    w89_u32 j;
    w89_u32 bs;
    w89_u32 op;
    depth = 0;
    bs = i + 1;
    *body_start = bs;
    *has_else = 0;
    *else_pos = n;
    *body_end = n;
    for (j = i + 1; j < n; j = j + 1) {
        op = items[j].op;
        if (op == 0x02) {
            depth = depth + 1;
        } else if (op == 0x03) {
            depth = depth + 1;
        } else if (op == 0x04) {
            depth = depth + 1;
        } else if (op == 0x1F) {
            depth = depth + 1;
        } else if (op == 0x05) {
            if (depth == 0) {
                *has_else = 1;
                *else_pos = j;
            }
        } else if (op == 0x0B) {
            if (depth == 0) {
                *body_end = j;
                return W89_ERR_NONE;
            }
            depth = depth - 1;
        }
    }
    return W89_ERR_EOF_SECTION;
}

void w89_blocktype_arity(const w89_typeenv *env, const w89_blocktype *bt,
                         w89_u32 *nparams, w89_u32 *nresults)
{
    w89_u32 is_ti;
    w89_u32 idx;
    const w89_subtype *sub;
    w89_u32 is_ref;
    w89_u32 num;
    w89_u32 np;
    w89_u32 nr;
    w89_u32 ntypes;
    is_ti = bt->is_typeidx;
    if (is_ti != 0) {
        if (env != 0) {
            idx = bt->typeidx;
            ntypes = env->ntypes;
            if (idx < ntypes) {
                sub = env->types[idx].sub;
                if (sub != 0) {
                    np = sub->ft.nparams;
                    nr = sub->ft.nresults;
                    *nparams = np;
                    *nresults = nr;
                } else {
                    *nparams = 0;
                    *nresults = 0;
                }
            } else {
                *nparams = 0;
                *nresults = 0;
            }
        } else {
            *nparams = 0;
            *nresults = 0;
        }
    } else {
        is_ref = bt->vt.is_ref;
        num = bt->vt.num;
        if (is_ref == 0) {
            if (num == 0) {
                *nparams = 0;
                *nresults = 0;
            } else {
                *nparams = 0;
                *nresults = 1;
            }
        } else {
            *nparams = 0;
            *nresults = 1;
        }
    }
}

/* Growable arrays. */

static w89_err grow(void **pp, w89_u32 *cap, w89_u32 need, w89_u32 size)
{
    w89_u32 ncap;
    void *p;
    void *p0;
    w89_u32 c0;
    size_t ts;
    size_t nb;
    c0 = *cap;
    if (need <= c0) {
        return W89_ERR_NONE;
    }
    if (c0 == 0) {
        ncap = 8;
    } else {
        ncap = c0 * 2;
    }
    while (ncap < need) {
        ncap = ncap * 2;
    }
    ts = (size_t)ncap;
    nb = ts * size;
    p0 = *pp;
    p = realloc(p0, nb);
    if (p == 0) {
        return W89_ERR_OUT_OF_MEMORY;
    }
    *pp = p;
    *cap = ncap;
    return W89_ERR_NONE;
}

/* Code deque: front (LIFO, prepended by step results) + back (the flat
 * instruction range, indexed by bhead). Prepend is O(1); nothing shifts. */

static w89_err code_front_push(w89_code *code, const w89_ainstr *a)
{
    w89_err e;
    void **pp;
    w89_u32 *cp;
    w89_u32 ne;
    w89_u32 n1;
    w89_u32 fn;
    w89_ainstr **q;
    w89_ainstr av;
    q = &code->front;
    pp = (void **)q;
    cp = &code->fcap;
    ne = code->fn;
    n1 = ne + 1;
    e = grow(pp, cp, n1, sizeof(w89_ainstr));
    if (e != W89_ERR_NONE) {
        return e;
    }
    av = *a;
    code->front[ne] = av;
    fn = code->fn;
    fn = fn + 1;
    code->fn = fn;
    return W89_ERR_NONE;
}

static w89_err code_back_push(w89_code *code, const w89_ainstr *a)
{
    w89_err e;
    void **pp;
    w89_u32 *cp;
    w89_u32 ne;
    w89_u32 n1;
    w89_u32 bn;
    w89_ainstr **q;
    w89_ainstr av;
    q = &code->back;
    pp = (void **)q;
    cp = &code->bcap;
    ne = code->bn;
    n1 = ne + 1;
    e = grow(pp, cp, n1, sizeof(w89_ainstr));
    if (e != W89_ERR_NONE) {
        return e;
    }
    av = *a;
    code->back[ne] = av;
    bn = code->bn;
    bn = bn + 1;
    code->bn = bn;
    return W89_ERR_NONE;
}

static w89_ainstr *code_head(w89_code *code)
{
    w89_u32 fn;
    w89_u32 bh;
    w89_u32 bn;
    w89_u32 idx;
    w89_ainstr *fp;
    fn = code->fn;
    if (fn > 0) {
        idx = fn - 1;
        fp = &code->front[idx];
        return fp;
    }
    bh = code->bhead;
    bn = code->bn;
    if (bh < bn) {
        fp = &code->back[bh];
        return fp;
    }
    return NULL;
}

static void code_consume(w89_code *code)
{
    w89_u32 fn;
    w89_u32 bh;
    w89_u32 bn;
    w89_ainstr *fp;
    fn = code->fn;
    if (fn > 0) {
        fn = fn - 1;
        code->fn = fn;
        fp = &code->front[fn];
        memset(fp, 0, sizeof(w89_ainstr));
    } else {
        bh = code->bhead;
        bn = code->bn;
        if (bh < bn) {
            fp = &code->back[bh];
            memset(fp, 0, sizeof(w89_ainstr));
            bh = bh + 1;
            code->bhead = bh;
        }
    }
}

static w89_err vs_push(w89_code *code, const w89_value *v)
{
    w89_err e;
    void **pp;
    w89_u32 *cp;
    w89_u32 ne;
    w89_u32 n1;
    w89_u32 vsn;
    w89_value **q;
    w89_value vv;
    w89_value *vp;
    q = &code->vs;
    pp = (void **)q;
    cp = &code->vscap;
    ne = code->vsn;
    n1 = ne + 1;
    e = grow(pp, cp, n1, sizeof(w89_value));
    if (e != W89_ERR_NONE) {
        return e;
    }
    vv = *v;
    vp = &code->vs[ne];
    *vp = vv;
    vsn = code->vsn;
    vsn = vsn + 1;
    code->vsn = vsn;
    return W89_ERR_NONE;
}

static w89_err vs_append(w89_code *code, const w89_value *vals, w89_u32 n)
{
    w89_err e;
    void **pp;
    w89_u32 *cp;
    w89_u32 ne;
    w89_u32 n1;
    w89_u32 vsn;
    w89_value **q;
    w89_value *dst;
    const w89_value *src;
    size_t nb;
    if (n == 0) {
        return W89_ERR_NONE;
    }
    q = &code->vs;
    pp = (void **)q;
    cp = &code->vscap;
    ne = code->vsn;
    n1 = ne + n;
    e = grow(pp, cp, n1, sizeof(w89_value));
    if (e != W89_ERR_NONE) {
        return e;
    }
    dst = &code->vs[ne];
    src = &vals[0];
    nb = sizeof(w89_value) * n;
    memcpy(dst, src, nb);
    vsn = code->vsn;
    vsn = vsn + n;
    code->vsn = vsn;
    return W89_ERR_NONE;
}

void w89_code_init(w89_code *code)
{
    memset(code, 0, sizeof(w89_code));
}

static void w89_ainstr_free(w89_ainstr *a)
{
    w89_akind k;
    w89_frame *fp;
    w89_code *cp;
    w89_value *vp;
    k = a->kind;
    switch (k) {
    case W89_A_LABEL:
    case W89_A_HANDLER:
        cp = &a->code;
        w89_code_free(cp);
        break;
    case W89_A_FRAME:
        fp = a->frame;
        frame_free(fp);
        cp = &a->code;
        w89_code_free(cp);
        break;
    case W89_A_BREAKING:
    case W89_A_RETURNING:
    case W89_A_RETINV:
    case W89_A_THROWING:
        vp = a->vs0;
        free(vp);
        break;
    default:
        break;
    }
    memset(a, 0, sizeof(w89_ainstr));
}

void w89_code_free(w89_code *code)
{
    w89_u32 i;
    w89_ainstr *ip;
    w89_value *vp;
    w89_u32 fn;
    w89_u32 bn;
    void *p;
    fn = code->fn;
    for (i = 0; i < fn; i = i + 1) {
        ip = &code->front[i];
        w89_ainstr_free(ip);
    }
    bn = code->bn;
    for (i = 0; i < bn; i = i + 1) {
        ip = &code->back[i];
        w89_ainstr_free(ip);
    }
    p = code->front;
    free(p);
    p = code->back;
    free(p);
    vp = code->vs;
    free(vp);
    memset(code, 0, sizeof(w89_code));
}

w89_err w89_code_range(w89_code *code, const w89_instr *items, w89_u32 n,
                       w89_u32 start, w89_u32 count)
{
    w89_u32 i;
    w89_ainstr a;
    w89_err e;
    const w89_instr *ip;
    w89_u32 pos;
    code->src = items;
    code->nsrc = n;
    for (i = 0; i < count; i = i + 1) {
        memset(&a, 0, sizeof(w89_ainstr));
        a.kind = W89_A_PLAIN;
        pos = start + i;
        ip = &items[pos];
        a.in = ip;
        a.ipos = pos;
        e = code_back_push(code, &a);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }
    return W89_ERR_NONE;
}

void w89_config_init(w89_config *c, w89_frame *frame)
{
    memset(c, 0, sizeof(w89_config));
    c->frame = frame;
    c->budget = 5000;
}

void w89_config_free(w89_config *c)
{
    w89_code *cp;
    void *p;
    cp = &c->code;
    w89_code_free(cp);
    p = c->lvls;
    free(p);
    p = c->vs;
    free(p);
    memset(c, 0, sizeof(w89_config));
}

w89_err w89_lvl_push(w89_config *c, const w89_lvl *l)
{
    w89_err e;
    void **pp;
    w89_u32 *cp;
    w89_u32 ne;
    w89_u32 n1;
    w89_u32 ln;
    w89_lvl **q;
    w89_lvl *dst;
    q = &c->lvls;
    pp = (void **)q;
    cp = &c->lcap;
    ne = c->ln;
    n1 = ne + 1;
    e = grow(pp, cp, n1, sizeof(w89_lvl));
    if (e != W89_ERR_NONE) {
        return e;
    }
    dst = &c->lvls[ne];
    {
        w89_lvl lv;
        lv = *l;
        *dst = lv;
    }
    ln = c->ln;
    ln = ln + 1;
    c->ln = ln;
    return W89_ERR_NONE;
}

int w89_lvl_pop(w89_config *c, w89_lvl *out)
{
    w89_lvl *p;
    w89_u32 ln;
    ln = c->ln;
    if (ln == 0) {
        return 0;
    }
    ln = ln - 1;
    p = &c->lvls[ln];
    if (out != 0) {
        w89_lvl lv;
        lv = *p;
        *out = lv;
    }
    memset(p, 0, sizeof(w89_lvl));
    c->ln = ln;
    return 1;
}

w89_u32 w89_config_ln(const w89_config *c)
{
    return c->ln;
}

w89_err w89_vs_push(w89_config *c, const w89_value *v)
{
    w89_err e;
    void **pp;
    w89_u32 *cp;
    w89_u32 ne;
    w89_u32 n1;
    w89_u32 vsn;
    w89_value **q;
    w89_value vv;
    w89_value *vp;
    q = &c->vs;
    pp = (void **)q;
    cp = &c->vscap;
    ne = c->vsn;
    n1 = ne + 1;
    e = grow(pp, cp, n1, sizeof(w89_value));
    if (e != W89_ERR_NONE) {
        return e;
    }
    vv = *v;
    vp = &c->vs[ne];
    *vp = vv;
    vsn = c->vsn;
    vsn = vsn + 1;
    c->vsn = vsn;
    return W89_ERR_NONE;
}

void w89_vs_reset(w89_config *c, w89_u32 n)
{
    c->vsn = n;
}

/* w89_eval_iter (the iterative driver) is defined after the legacy
 * steppers it reuses for plain instructions. */

void w89_eval_out_free(w89_eval_out *out)
{
    w89_value *vp;
    vp = out->vs;
    free(vp);
    out->vs = 0;
    out->nvs = 0;
}

/* Crash helpers. */

static w89_step_status crash(w89_config *c, const char *msg)
{
    c->crash = msg;
    return W89_STEP_CRASH;
}

static const char *undef_msg(const char *cat, w89_u32 idx)
{
    static char buf[80];
    unsigned int u;
    u = (unsigned int)idx;
    sprintf(buf, "undefined %s %u", cat, u);
    return buf;
}

static w89_step_status push_trap(w89_config *c, const char *msg)
{
    w89_ainstr a;
    w89_err e;
    w89_code *code;
    memset(&a, 0, sizeof(w89_ainstr));
    a.kind = W89_A_TRAP;
    a.msg = msg;
    code = &c->code;
    e = code_front_push(code, &a);
    if (e != W89_ERR_NONE) {
        return crash(c, "out of memory");
    }
    return W89_STEP_OK;
}

/* Value stack accessors (top is the last element). */

static w89_u32 pop_u32(w89_config *c)
{
    w89_code *code;
    w89_u32 n;
    w89_u32 idx;
    w89_value v;
    w89_u64 num;
    w89_u32 r;
    code = &c->code;
    n = code->vsn;
    idx = n;
    idx = idx - 1;
    code->vsn = idx;
    v = code->vs[idx];
    num = v.u.num;
    r = (w89_u32)num;
    return r;
}

static w89_u64 pop_u64(w89_config *c)
{
    w89_code *code;
    w89_u32 n;
    w89_u32 idx;
    w89_value v;
    w89_u64 num;
    code = &c->code;
    n = code->vsn;
    idx = n;
    idx = idx - 1;
    code->vsn = idx;
    v = code->vs[idx];
    num = v.u.num;
    return num;
}

static w89_f32 pop_f32(w89_config *c)
{
    w89_u32 x;
    w89_f32 r;
    x = pop_u32(c);
    r = w89_bits_f32(x);
    return r;
}

static w89_f64 pop_f64(w89_config *c)
{
    w89_u64 x;
    w89_f64 r;
    x = pop_u64(c);
    r = w89_bits_f64(x);
    return r;
}

static void push_u32(w89_config *c, w89_u32 x)
{
    w89_value v;
    w89_code *code;
    memset(&v, 0, sizeof(w89_value));
    v.u.num = x;
    code = &c->code;
    vs_push(code, &v);
}

static void push_u64(w89_config *c, w89_u64 x)
{
    w89_value v;
    w89_code *code;
    memset(&v, 0, sizeof(w89_value));
    v.u.num = x;
    code = &c->code;
    vs_push(code, &v);
}

static void push_f32(w89_config *c, w89_f32 x)
{
    w89_value v;
    w89_code *code;
    w89_u64 bits;
    memset(&v, 0, sizeof(w89_value));
    bits = w89_f32_bits(x);
    v.u.num = bits;
    code = &c->code;
    vs_push(code, &v);
}

static void push_f64(w89_config *c, w89_f64 x)
{
    w89_value v;
    w89_code *code;
    w89_u64 bits;
    memset(&v, 0, sizeof(w89_value));
    bits = w89_f64_bits(x);
    v.u.num = bits;
    code = &c->code;
    vs_push(code, &v);
}

/* Step cases. */

static w89_step_status step_select(w89_config *c)
{
    w89_value cond;
    w89_value v2;
    w89_value v1;
    w89_value r;
    w89_code *code;
    w89_u32 n;
    w89_u32 idx;
    w89_u64 cn;
    code = &c->code;
    n = code->vsn;
    if (n < 3) {
        return crash(c, "stack underflow");
    }
    idx = n;
    idx = idx - 1;
    cond = code->vs[idx];
    idx = idx - 1;
    v2 = code->vs[idx];
    idx = idx - 1;
    v1 = code->vs[idx];
    n = n - 3;
    code->vsn = n;
    cn = cond.u.num;
    if (cn == 0) {
        r = v2;
    } else {
        r = v1;
    }
    vs_push(code, &r);
    code_consume(code);
    return W89_STEP_OK;
}

static w89_step_status step_br(w89_config *c, w89_u32 depth)
{
    w89_ainstr b;
    w89_value *cp;
    w89_code *code;
    w89_u32 n;
    w89_value *src;
    size_t nb;
    code = &c->code;
    n = code->vsn;
    memset(&b, 0, sizeof(w89_ainstr));
    if (n > 0) {
        nb = sizeof(w89_value) * n;
        cp = malloc(nb);
        if (cp == 0) {
            return crash(c, "out of memory");
        }
        src = code->vs;
        memcpy(cp, src, nb);
    } else {
        cp = 0;
    }
    b.kind = W89_A_BREAKING;
    b.k = depth;
    b.vs0 = cp;
    b.nvs0 = n;
    code_consume(code);
    code_front_push(code, &b);
    return W89_STEP_OK;
}

static w89_step_status step_return(w89_config *c)
{
    w89_ainstr b;
    w89_value *cp;
    w89_code *code;
    w89_u32 n;
    w89_value *src;
    size_t nb;
    code = &c->code;
    n = code->vsn;
    memset(&b, 0, sizeof(w89_ainstr));
    if (n > 0) {
        nb = sizeof(w89_value) * n;
        cp = malloc(nb);
        if (cp == 0) {
            return crash(c, "out of memory");
        }
        src = code->vs;
        memcpy(cp, src, nb);
    } else {
        cp = 0;
    }
    b.kind = W89_A_RETURNING;
    b.vs0 = cp;
    b.nvs0 = n;
    code_consume(code);
    code_front_push(code, &b);
    return W89_STEP_OK;
}

static w89_step_status step_block(w89_config *c, w89_ainstr *cur, w89_u32 op)
{
    w89_u32 n1;
    w89_u32 n2;
    w89_u32 bs;
    w89_u32 be;
    w89_u32 has_else;
    w89_u32 ep;
    w89_u32 arm_s;
    w89_u32 arm_e;
    w89_ainstr label;
    w89_err e;
    w89_frame *fr;
    w89_moduleinst *inst;
    const w89_typeenv *te;
    const w89_blocktype *bt;
    const w89_instr *src;
    w89_u32 nsrc;
    w89_u32 cond;
    w89_u32 fn;
    w89_code *code;
    w89_u32 n;
    w89_u32 idx;
    w89_value v;
    w89_u64 cn;
    w89_u32 a1;
    w89_u32 cnt;
    w89_u32 sk;
    w89_value *dest;
    w89_value *sour;
    w89_u32 base;
    w89_u32 vn;
    size_t nb;
    w89_u32 ipos;
    w89_code *lblcode;
    fr = c->frame;
    if (fr == 0) {
        return crash(c, "undefined frame");
    }
    inst = fr->inst;
    if (inst == 0) {
        return crash(c, "undefined frame");
    }
    te = inst->types;
    bt = &cur->in->bt;
    w89_blocktype_arity(te, bt, &n1, &n2);
    code = &c->code;
    ipos = cur->ipos;
    src = code->src;
    nsrc = code->nsrc;
    e = w89_block_extent(src, nsrc, ipos, &bs, &be, &has_else, &ep);
    if (e != W89_ERR_NONE) {
        return crash(c, "malformed block");
    }
    if (op == 0x04) {
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        idx = n;
        idx = idx - 1;
        code->vsn = idx;
        v = code->vs[idx];
        cn = v.u.num;
        cond = (w89_u32)cn;
        if (cond == 0) {
            if (has_else != 0) {
                a1 = ep + 1;
                arm_s = a1;
                arm_e = be;
            } else {
                arm_s = be;
                arm_e = be;
            }
        } else {
            arm_s = bs;
            if (has_else != 0) {
                arm_e = ep;
            } else {
                arm_e = be;
            }
        }
    } else {
        arm_s = bs;
        arm_e = be;
    }
    n = code->vsn;
    if (n < n1) {
        return crash(c, "stack underflow");
    }
    memset(&label, 0, sizeof(w89_ainstr));
    label.kind = W89_A_LABEL;
    if (op == 0x03) {
        label.n = n1;
    } else {
        label.n = n2;
    }
    label.src = src;
    label.nsrc = nsrc;
    if (op == 0x03) {
        label.contpos = ipos;
        label.contn = 1;
    }
    fn = code->fn;
    if (fn == 0) {
        sk = be - ipos;
        label.skip = sk;
    }
    lblcode = &label.code;
    w89_code_init(lblcode);
    cnt = arm_e - arm_s;
    e = w89_code_range(lblcode, src, nsrc, arm_s, cnt);
    if (e != W89_ERR_NONE) {
        w89_code_free(lblcode);
        return crash(c, "out of memory");
    }
    if (n1 > 0) {
        nb = sizeof(w89_value) * n1;
        dest = malloc(nb);
        if (dest == 0) {
            w89_code_free(lblcode);
            return crash(c, "out of memory");
        }
        label.code.vs = dest;
        vn = code->vsn;
        base = vn - n1;
        sour = &code->vs[base];
        memcpy(dest, sour, nb);
        label.code.vscap = n1;
        label.code.vsn = n1;
        code->vsn = base;
    }
    *cur = label;
    return W89_STEP_OK;
}

static int is_jumping(w89_akind k)
{
    if (k == W89_A_RETURNING) {
        return 1;
    }
    if (k == W89_A_RETINV) {
        return 1;
    }
    if (k == W89_A_BREAKING) {
        return 1;
    }
    if (k == W89_A_THROWING) {
        return 1;
    }
    if (k == W89_A_TRAP) {
        return 1;
    }
    return 0;
}

static w89_step_status step_try_table(w89_config *c, w89_ainstr *cur)
{
    w89_u32 n1;
    w89_u32 n2;
    w89_u32 bs;
    w89_u32 be;
    w89_u32 has_else;
    w89_u32 ep;
    w89_ainstr label;
    w89_ainstr handler;
    w89_err e;
    w89_frame *fr;
    w89_moduleinst *inst;
    const w89_typeenv *te;
    const w89_blocktype *bt;
    const w89_instr *src;
    w89_u32 nsrc;
    w89_u32 fn;
    w89_u32 cnt;
    w89_u32 sk;
    w89_code *code;
    w89_u32 n;
    w89_value *dest;
    w89_value *sour;
    w89_u32 base;
    w89_u32 vn;
    w89_catch *cct;
    w89_u32 cn_;
    w89_code *lblcode;
    w89_code *hcode;
    size_t nb;
    w89_u32 ipos;
    fr = c->frame;
    if (fr == 0) {
        return crash(c, "undefined frame");
    }
    inst = fr->inst;
    if (inst == 0) {
        return crash(c, "undefined frame");
    }
    te = inst->types;
    bt = &cur->in->bt;
    w89_blocktype_arity(te, bt, &n1, &n2);
    code = &c->code;
    ipos = cur->ipos;
    src = code->src;
    nsrc = code->nsrc;
    e = w89_block_extent(src, nsrc, ipos, &bs, &be, &has_else, &ep);
    if (e != W89_ERR_NONE) {
        return crash(c, "malformed block");
    }
    n = code->vsn;
    if (n < n1) {
        return crash(c, "stack underflow");
    }
    memset(&label, 0, sizeof(w89_ainstr));
    label.kind = W89_A_LABEL;
    label.n = n2;
    label.src = src;
    label.nsrc = nsrc;
    lblcode = &label.code;
    w89_code_init(lblcode);
    cnt = be - bs;
    e = w89_code_range(lblcode, src, nsrc, bs, cnt);
    if (e != W89_ERR_NONE) {
        w89_code_free(lblcode);
        return crash(c, "out of memory");
    }
    if (n1 > 0) {
        nb = sizeof(w89_value) * n1;
        dest = malloc(nb);
        if (dest == 0) {
            w89_code_free(lblcode);
            return crash(c, "out of memory");
        }
        label.code.vs = dest;
        vn = code->vsn;
        base = vn - n1;
        sour = &code->vs[base];
        memcpy(dest, sour, nb);
        label.code.vscap = n1;
        label.code.vsn = n1;
        code->vsn = base;
    }
    memset(&handler, 0, sizeof(w89_ainstr));
    handler.kind = W89_A_HANDLER;
    handler.n = n2;
    cct = cur->in->catches;
    handler.catches = cct;
    cn_ = cur->in->n;
    handler.ncatches = cn_;
    fn = code->fn;
    if (fn == 0) {
        sk = be - ipos;
        handler.skip = sk;
    }
    hcode = &handler.code;
    w89_code_init(hcode);
    e = code_back_push(hcode, &label);
    if (e != W89_ERR_NONE) {
        w89_code_free(lblcode);
        w89_code_free(hcode);
        return crash(c, "out of memory");
    }
    *cur = handler;
    return W89_STEP_OK;
}

static w89_step_status step_handler(w89_config *c, w89_ainstr *h)
{
    w89_ainstr *head;
    w89_code *hcode;
    w89_code tmp;
    w89_code *tp;
    w89_code *codel;
    w89_u32 skip;
    w89_u32 i;
    w89_u32 j;
    w89_taginst *a;
    const w89_catch *ct;
    w89_u32 ck;
    w89_u32 tidx;
    w89_u32 ntags;
    w89_taginst **tags;
    w89_taginst *tg;
    w89_exn *ex;
    w89_ref rr;
    w89_value rv;
    w89_frame *fr;
    w89_moduleinst *inst;
    w89_store *store;
    w89_value *pvs;
    w89_u32 nvs;
    w89_u32 l;
    w89_u32 hk;
    w89_u32 jm;
    w89_config sub;
    w89_step_status st;
    const char *scrash;
    w89_byte sexh;
    w89_i64 bdgt;
    w89_err e;
    w89_u32 ncc;
    w89_frame *sfr;
    hcode = &h->code;
    head = code_head(hcode);
    if (head == 0) {
        tmp = h->code;
        skip = h->skip;
        codel = &c->code;
        pvs = tmp.vs;
        nvs = tmp.vsn;
        e = vs_append(codel, pvs, nvs);
        if (e != W89_ERR_NONE) {
            tp = &tmp;
            w89_code_free(tp);
            return crash(c, "out of memory");
        }
        tp = &tmp;
        w89_code_free(tp);
        code_consume(codel);
        for (i = 0; i < skip; i = i + 1) {
            code_consume(codel);
        }
        return W89_STEP_OK;
    }
    hk = head->kind;
    if (hk == W89_A_THROWING) {
        a = head->tag;
        ncc = h->ncatches;
        for (j = 0; j < ncc; j = j + 1) {
            ct = &h->catches[j];
            ck = ct->kind;
            if (ck == 0x00) {
                fr = c->frame;
                if (fr != 0) {
                    inst = fr->inst;
                    if (inst != 0) {
                        tidx = ct->tagidx;
                        ntags = inst->ntags;
                        if (tidx < ntags) {
                            tags = inst->tags;
                            tg = tags[tidx];
                            if (a == tg) {
                                codel = &c->code;
                                pvs = head->vs0;
                                nvs = head->nvs0;
                                e = vs_append(codel, pvs, nvs);
                                if (e != W89_ERR_NONE) {
                                    return crash(c, "out of memory");
                                }
                                if (ck == 0x01) {
                                    store = inst->store;
                                    if (store == 0) {
                                        return crash(c, "no store for exception");
                                    }
                                    ex = w89_exn_alloc(store, a, pvs, nvs);
                                    if (ex == 0) {
                                        return crash(c, "out of memory");
                                    }
                                    head->vs0 = 0;
                                    head->nvs0 = 0;
                                    rr = w89_ref_exn(ex);
                                    rv = w89_value_ref(&rr);
                                    e = vs_push(codel, &rv);
                                    if (e != W89_ERR_NONE) {
                                        return crash(c, "out of memory");
                                    }
                                }
                                tp = hcode;
                                w89_code_free(tp);
                                l = ct->label;
                                return step_br(c, l);
                            }
                        }
                    }
                }
            } else if (ck == 0x01) {
                fr = c->frame;
                if (fr != 0) {
                    inst = fr->inst;
                    if (inst != 0) {
                        tidx = ct->tagidx;
                        ntags = inst->ntags;
                        if (tidx < ntags) {
                            tags = inst->tags;
                            tg = tags[tidx];
                            if (a == tg) {
                                codel = &c->code;
                                pvs = head->vs0;
                                nvs = head->nvs0;
                                e = vs_append(codel, pvs, nvs);
                                if (e != W89_ERR_NONE) {
                                    return crash(c, "out of memory");
                                }
                                if (ck == 0x01) {
                                    store = inst->store;
                                    if (store == 0) {
                                        return crash(c, "no store for exception");
                                    }
                                    ex = w89_exn_alloc(store, a, pvs, nvs);
                                    if (ex == 0) {
                                        return crash(c, "out of memory");
                                    }
                                    head->vs0 = 0;
                                    head->nvs0 = 0;
                                    rr = w89_ref_exn(ex);
                                    rv = w89_value_ref(&rr);
                                    e = vs_push(codel, &rv);
                                    if (e != W89_ERR_NONE) {
                                        return crash(c, "out of memory");
                                    }
                                }
                                tp = hcode;
                                w89_code_free(tp);
                                l = ct->label;
                                return step_br(c, l);
                            }
                        }
                    }
                }
            } else if (ck == 0x02) {
                tp = hcode;
                w89_code_free(tp);
                l = ct->label;
                return step_br(c, l);
            } else if (ck == 0x03) {
                fr = c->frame;
                if (fr == 0) {
                    return crash(c, "no store for exception");
                }
                inst = fr->inst;
                if (inst == 0) {
                    return crash(c, "no store for exception");
                }
                store = inst->store;
                if (store == 0) {
                    return crash(c, "no store for exception");
                }
                pvs = head->vs0;
                nvs = head->nvs0;
                ex = w89_exn_alloc(store, a, pvs, nvs);
                if (ex == 0) {
                    return crash(c, "out of memory");
                }
                head->vs0 = 0;
                head->nvs0 = 0;
                rr = w89_ref_exn(ex);
                rv = w89_value_ref(&rr);
                codel = &c->code;
                e = vs_push(codel, &rv);
                if (e != W89_ERR_NONE) {
                    return crash(c, "out of memory");
                }
                tp = hcode;
                w89_code_free(tp);
                l = ct->label;
                return step_br(c, l);
            }
        }
        tmp = h->code;
        memcpy(h, head, sizeof(w89_ainstr));
        memset(head, 0, sizeof(w89_ainstr));
        tp = &tmp;
        w89_code_free(tp);
        return W89_STEP_OK;
    }
    jm = is_jumping(hk);
    if (jm != 0) {
        tmp = h->code;
        memcpy(h, head, sizeof(w89_ainstr));
        memset(head, 0, sizeof(w89_ainstr));
        tp = &tmp;
        w89_code_free(tp);
        return W89_STEP_OK;
    }
    sfr = c->frame;
    sub.frame = sfr;
    bdgt = c->budget;
    sub.budget = bdgt;
    sub.crash = 0;
    sub.exhausted = 0;
    tmp = h->code;
    sub.code = tmp;
    st = w89_step(&sub);
    tmp = sub.code;
    h->code = tmp;
    scrash = sub.crash;
    c->crash = scrash;
    sexh = sub.exhausted;
    c->exhausted = sexh;
    return st;
}

void w89_func_arity(const w89_funcinst *f, w89_u32 *nparams, w89_u32 *nresults)
{
    w89_u32 is_host;
    const w89_ft *ft;
    w89_moduleinst *inst;
    const w89_typeenv *te;
    const w89_deftype *dt;
    w89_u32 typeidx;
    w89_u32 np;
    w89_u32 nr;
    is_host = f->is_host;
    if (is_host != 0) {
        ft = f->ft;
        np = ft->nparams;
        nr = ft->nresults;
        *nparams = np;
        *nresults = nr;
    } else {
        inst = f->inst;
        te = inst->types;
        typeidx = f->typeidx;
        dt = &te->types[typeidx];
        np = dt->sub->ft.nparams;
        nr = dt->sub->ft.nresults;
        *nparams = np;
        *nresults = nr;
    }
}

const w89_ft *w89_func_ft(const w89_funcinst *f)
{
    w89_u32 is_host;
    w89_moduleinst *inst;
    const w89_typeenv *te;
    w89_u32 typeidx;
    const w89_ft *ft;
    is_host = f->is_host;
    if (is_host != 0) {
        ft = f->ft;
        return ft;
    }
    inst = f->inst;
    te = inst->types;
    typeidx = f->typeidx;
    ft = &te->types[typeidx].sub->ft;
    return ft;
}

w89_value w89_default_value(const w89_vt *t)
{
    w89_value v;
    w89_u32 is_ref;
    memset(&v, 0, sizeof(w89_value));
    is_ref = t->is_ref;
    if (is_ref != 0) {
        v.is_ref = 1;
        v.u.ref.kind = W89_RK_NULL;
    }
    return v;
}

int w89_name_eq(const w89_name *n, const char *s, w89_u32 len)
{
    w89_u32 i;
    w89_u32 nl;
    nl = n->len;
    if (nl != len) {
        return 0;
    }
    for (i = 0; i < len; i = i + 1) {
        w89_byte b;
        char ch;
        w89_byte sb;
        b = n->bytes[i];
        ch = s[i];
        sb = (w89_byte)ch;
        if (b != sb) {
            return 0;
        }
    }
    return 1;
}

static w89_frame *frame_alloc(w89_moduleinst *inst, w89_u32 nlocals)
{
    w89_frame *f;
    w89_local *lp;
    size_t ts;
    size_t nb;
    f = malloc(sizeof(w89_frame));
    if (f == 0) {
        return NULL;
    }
    memset(f, 0, sizeof(w89_frame));
    f->inst = inst;
    f->nlocals = nlocals;
    if (nlocals > 0) {
        ts = (size_t)nlocals;
        nb = ts * sizeof(w89_local);
        lp = malloc(nb);
        if (lp == 0) {
            free(f);
            return NULL;
        }
        f->locals = lp;
        memset(lp, 0, nb);
    }
    return f;
}

static void frame_free(w89_frame *f)
{
    w89_local *lp;
    if (f == 0) {
        return;
    }
    lp = f->locals;
    free(lp);
    free(f);
}

static w89_u64 mem_bytes(const w89_meminst *m)
{
    w89_u64 np;
    w89_u64 nb;
    np = m->npages;
    nb = np * W89_PAGE_SIZE;
    return nb;
}

/* Overflow-aware bounds check (reference eval.ml `oob`): the full u64 sum
 * i + n must not wrap and must not exceed the bound j. */
static int oob(w89_u64 i, w89_u64 n, w89_u64 j)
{
    w89_u64 s;
    s = i + n;
    if (s < i) {
        return 1;
    }
    if (s > j) {
        return 1;
    }
    return 0;
}

static w89_u64 table_addr_max(int addr64)
{
    if (addr64 != 0) {
        return 0xFFFFFFFFFFFFFFFFUL;
    }
    return 0x100000000UL;
}

static const char *num_msg(const char *prefix, w89_u64 i)
{
    static char buf[80];
    unsigned long ul;
    ul = (unsigned long)i;
    sprintf(buf, "%s %lu", prefix, ul);
    return buf;
}

static w89_step_status step_invoke(w89_config *c, w89_ainstr *cur)
{
    w89_funcinst *f;
    w89_u32 n1;
    w89_u32 n2;
    w89_u32 i;
    w89_u32 is_host;
    w89_value res[16];
    w89_u32 nres;
    const char *trap;
    w89_host_status hs;
    w89_code *code;
    w89_u32 vsn;
    w89_u32 base;
    w89_value *hp;
    w89_u32 *nresp;
    const char **trapp;
    w89_hostfn hf;
    w89_value *vp;
    w89_u32 idx;
    w89_value v;
    w89_u32 nlocals;
    w89_frame *frame;
    const w89_func *fn;
    w89_u32 nlf;
    w89_vt *lt;
    w89_u32 lr;
    w89_u32 nl;
    w89_moduleinst *inst;
    w89_value dv;
    w89_ainstr fr;
    w89_ainstr lbl;
    w89_err e2;
    w89_code *frp;
    w89_code *lblp;
    w89_instr *items;
    w89_u32 ni;
    w89_i64 bdgt;
    w89_err e;
    f = cur->finst;
    bdgt = c->budget;
    if (bdgt == 0) {
        c->exhausted = 1;
        return W89_STEP_EXHAUSTED;
    }
    w89_func_arity(f, &n1, &n2);
    code = &c->code;
    vsn = code->vsn;
    if (vsn < n1) {
        return crash(c, "stack underflow");
    }
    base = vsn - n1;
    is_host = f->is_host;
    if (is_host != 0) {
        nres = 0;
        trap = 0;
        hp = &code->vs[base];
        nresp = &nres;
        trapp = &trap;
        hf = f->host;
        hs = hf(hp, n1, res, nresp, trapp);
        code->vsn = base;
        code_consume(code);
        if (hs == W89_HOST_TRAP) {
            if (trap != 0) {
                return push_trap(c, trap);
            }
            return push_trap(c, "host function trapped");
        }
        e = vs_append(code, res, nres);
        if (e != W89_ERR_NONE) {
            return crash(c, "out of memory");
        }
        return W89_STEP_OK;
    }
    fn = f->func;
    nlf = fn->nlocals;
    nlocals = n1 + nlf;
    inst = f->inst;
    frame = frame_alloc(inst, nlocals);
    if (frame == 0) {
        return crash(c, "out of memory");
    }
    for (i = 0; i < n1; i = i + 1) {
        idx = base + i;
        vp = &code->vs[idx];
        v = *vp;
        frame->locals[i].v = v;
        frame->locals[i].set = 1;
    }
    for (i = 0; i < nlf; i = i + 1) {
        lt = &fn->locals[i];
        lr = lt->is_ref;
        if (lr != 0) {
            nl = lt->rt.nullable;
            if (nl == 0) {
                idx = n1 + i;
                frame->locals[idx].set = 0;
            } else {
                dv = w89_default_value(lt);
                idx = n1 + i;
                frame->locals[idx].v = dv;
                frame->locals[idx].set = 1;
            }
        } else {
            dv = w89_default_value(lt);
            idx = n1 + i;
            frame->locals[idx].v = dv;
            frame->locals[idx].set = 1;
        }
    }
    code->vsn = base;
    memset(&fr, 0, sizeof(w89_ainstr));
    fr.kind = W89_A_FRAME;
    fr.n = n2;
    fr.frame = frame;
    frp = &fr.code;
    w89_code_init(frp);
    memset(&lbl, 0, sizeof(w89_ainstr));
    lbl.kind = W89_A_LABEL;
    lbl.n = n2;
    items = fn->code.items;
    ni = fn->code.n;
    lbl.src = items;
    lbl.nsrc = ni;
    lblp = &lbl.code;
    w89_code_init(lblp);
    e2 = w89_code_range(lblp, items, ni, 0, ni);
    if (e2 != W89_ERR_NONE) {
        w89_code_free(lblp);
        w89_code_free(frp);
        frame_free(frame);
        return crash(c, "out of memory");
    }
    e2 = code_back_push(frp, &lbl);
    if (e2 != W89_ERR_NONE) {
        w89_code_free(lblp);
        w89_code_free(frp);
        frame_free(frame);
        return crash(c, "out of memory");
    }
    *cur = fr;
    return W89_STEP_OK;
}

static w89_step_status step_frame(w89_config *c, w89_ainstr *fr)
{
    w89_ainstr *h;
    w89_code *frp;
    w89_code *codel;
    w89_code tmp;
    w89_code *tp;
    w89_frame *frame;
    w89_u32 n;
    w89_u32 take_n;
    w89_value *res;
    w89_funcinst *nf;
    w89_u32 n1;
    w89_u32 n2;
    w89_value *pvs;
    w89_u32 nvs0;
    w89_u32 nvs;
    w89_u32 base;
    w89_value *p0;
    w89_u32 hk;
    w89_u32 jm;
    w89_config sub;
    w89_step_status st;
    const char *scrash;
    w89_byte sexh;
    w89_frame *sfr;
    w89_i64 bdgt;
    w89_i64 b1;
    w89_err e;
    frp = &fr->code;
    h = code_head(frp);
    if (h == 0) {
        tmp = fr->code;
        frame = fr->frame;
        codel = &c->code;
        pvs = tmp.vs;
        nvs = tmp.vsn;
        e = vs_append(codel, pvs, nvs);
        if (e != W89_ERR_NONE) {
            tp = &tmp;
            w89_code_free(tp);
            frame_free(frame);
            return crash(c, "out of memory");
        }
        tp = &tmp;
        w89_code_free(tp);
        frame_free(frame);
        code_consume(codel);
        return W89_STEP_OK;
    }
    hk = h->kind;
    if (hk == W89_A_RETURNING) {
        n = fr->n;
        nvs0 = h->nvs0;
        if (nvs0 < n) {
            take_n = nvs0;
        } else {
            take_n = n;
        }
        base = nvs0 - take_n;
        p0 = h->vs0;
        res = p0 + base;
        tmp = fr->code;
        frame = fr->frame;
        codel = &c->code;
        e = vs_append(codel, res, take_n);
        if (e != W89_ERR_NONE) {
            tp = &tmp;
            w89_code_free(tp);
            frame_free(frame);
            return crash(c, "out of memory");
        }
        pvs = h->vs0;
        free(pvs);
        h->vs0 = 0;
        h->nvs0 = 0;
        tp = &tmp;
        w89_code_free(tp);
        frame_free(frame);
        code_consume(codel);
        return W89_STEP_OK;
    }
    if (hk == W89_A_RETINV) {
        nf = h->finst;
        w89_func_arity(nf, &n1, &n2);
        nvs0 = h->nvs0;
        if (nvs0 < n1) {
            take_n = nvs0;
        } else {
            take_n = n1;
        }
        base = nvs0 - take_n;
        p0 = h->vs0;
        res = p0 + base;
        tmp = fr->code;
        frame = fr->frame;
        codel = &c->code;
        e = vs_append(codel, res, take_n);
        if (e != W89_ERR_NONE) {
            tp = &tmp;
            w89_code_free(tp);
            frame_free(frame);
            return crash(c, "out of memory");
        }
        pvs = h->vs0;
        free(pvs);
        h->vs0 = 0;
        h->nvs0 = 0;
        tp = &tmp;
        w89_code_free(tp);
        frame_free(frame);
        memset(fr, 0, sizeof(w89_ainstr));
        fr->kind = W89_A_INVOKE;
        fr->finst = nf;
        return W89_STEP_OK;
    }
    jm = is_jumping(hk);
    if (jm != 0) {
        tmp = fr->code;
        frame = fr->frame;
        memcpy(fr, h, sizeof(w89_ainstr));
        memset(h, 0, sizeof(w89_ainstr));
        tp = &tmp;
        w89_code_free(tp);
        frame_free(frame);
        return W89_STEP_OK;
    }
    sfr = fr->frame;
    sub.frame = sfr;
    bdgt = c->budget;
    b1 = bdgt - 1;
    sub.budget = b1;
    sub.crash = 0;
    sub.exhausted = 0;
    tmp = fr->code;
    sub.code = tmp;
    st = w89_step(&sub);
    tmp = sub.code;
    fr->code = tmp;
    scrash = sub.crash;
    c->crash = scrash;
    sexh = sub.exhausted;
    c->exhausted = sexh;
    return st;
}

static w89_step_status step_call(w89_config *c, w89_ainstr *cur)
{
    w89_moduleinst *inst;
    w89_ainstr inv;
    w89_funcinst *f;
    w89_frame *fr;
    w89_u32 idx;
    w89_u32 nf;
    w89_code *code;
    const char *msg;
    fr = c->frame;
    if (fr == 0) {
        return crash(c, "undefined frame");
    }
    inst = fr->inst;
    if (inst == 0) {
        return crash(c, "undefined frame");
    }
    idx = cur->in->idx;
    nf = inst->nfuncs;
    if (idx >= nf) {
        msg = undef_msg("function", idx);
        return crash(c, msg);
    }
    f = inst->funcs[idx];
    memset(&inv, 0, sizeof(w89_ainstr));
    inv.kind = W89_A_INVOKE;
    inv.finst = f;
    code = &c->code;
    code_consume(code);
    code_front_push(code, &inv);
    return W89_STEP_OK;
}

static w89_step_status step_call_ref(w89_config *c, int tail)
{
    w89_value v;
    w89_ainstr inv;
    w89_code *code;
    w89_u32 vsn;
    w89_u32 idx;
    w89_u32 n;
    w89_value *dst;
    w89_value *src;
    size_t nb;
    w89_u32 is_ref;
    w89_u32 rk;
    w89_ref ref;
    w89_funcinst *fn;
    code = &c->code;
    vsn = code->vsn;
    if (vsn < 1) {
        return crash(c, "stack underflow");
    }
    idx = vsn;
    idx = idx - 1;
    code->vsn = idx;
    v = code->vs[idx];
    is_ref = v.is_ref;
    rk = v.u.ref.kind;
    if (is_ref == 0) {
        code_consume(code);
        return push_trap(c, "null function reference");
    }
    if (rk == W89_RK_NULL) {
        code_consume(code);
        return push_trap(c, "null function reference");
    }
    if (rk != W89_RK_FUNC) {
        return crash(c, "type mismatch at call_ref");
    }
    memset(&inv, 0, sizeof(w89_ainstr));
    if (tail != 0) {
        n = code->vsn;
        inv.kind = W89_A_RETINV;
        if (n > 0) {
            nb = sizeof(w89_value) * n;
            dst = malloc(nb);
            if (dst == 0) {
                return crash(c, "out of memory");
            }
            inv.vs0 = dst;
            src = &code->vs[0];
            memcpy(dst, src, nb);
            inv.nvs0 = n;
        }
        ref = v.u.ref;
        fn = ref.u.func;
        inv.finst = fn;
    } else {
        inv.kind = W89_A_INVOKE;
        ref = v.u.ref;
        fn = ref.u.func;
        inv.finst = fn;
    }
    code_consume(code);
    code_front_push(code, &inv);
    return W89_STEP_OK;
}

static w89_step_status step_call_indirect(w89_config *c, w89_ainstr *cur,
                                          int tail)
{
    w89_u32 typeidx;
    w89_u32 tableidx;
    w89_moduleinst *inst;
    w89_tableinst *tab;
    w89_u32 i;
    w89_ref r;
    w89_funcinst *f;
    w89_ainstr inv;
    w89_frame *fr;
    w89_u32 nt;
    w89_code *code;
    w89_u32 vsn;
    w89_u64 sz;
    w89_u32 rk;
    w89_u64 t64;
    w89_u32 is_host;
    const w89_ft *ef;
    const w89_ft *fft;
    const w89_typeenv *itypes;
    const w89_typeenv *ftypes;
    w89_u32 ftypeidx;
    w89_u32 mt;
    w89_u32 n;
    w89_value *dst;
    w89_value *src;
    size_t nb;
    const char *msg;
    typeidx = cur->in->idx2;
    tableidx = cur->in->idx;
    fr = c->frame;
    if (fr == 0) {
        return crash(c, "undefined frame");
    }
    inst = fr->inst;
    if (inst == 0) {
        return crash(c, "undefined frame");
    }
    nt = inst->ntables;
    if (tableidx >= nt) {
        msg = undef_msg("table", tableidx);
        return crash(c, msg);
    }
    tab = inst->tables[tableidx];
    code = &c->code;
    vsn = code->vsn;
    if (vsn < 1) {
        return crash(c, "stack underflow");
    }
    i = pop_u32(c);
    t64 = (w89_u64)i;
    sz = tab->size;
    if (t64 >= sz) {
        code_consume(code);
        msg = num_msg("undefined element", t64);
        return push_trap(c, msg);
    }
    r = tab->elems[i];
    rk = r.kind;
    if (rk == W89_RK_NULL) {
        code_consume(code);
        msg = num_msg("uninitialized element", t64);
        return push_trap(c, msg);
    }
    if (rk != W89_RK_FUNC) {
        return crash(c, "type mismatch at call_indirect");
    }
    f = r.u.func;
    is_host = f->is_host;
    if (is_host != 0) {
        ef = &inst->types->types[typeidx].sub->ft;
        fft = f->ft;
        itypes = inst->types;
        mt = match_ft_x(0, fft, itypes, ef);
        if (mt == 0) {
            code_consume(code);
            msg = num_msg("indirect call type mismatch, "
                      "expected type", t64);
            return push_trap(c, msg);
        }
    } else {
        ftypes = f->inst->types;
        ftypeidx = f->typeidx;
        itypes = inst->types;
        mt = w89_match_deftype_x(ftypes, ftypeidx, itypes, typeidx);
        if (mt == 0) {
            code_consume(code);
            msg = num_msg("indirect call type mismatch, expected "
                      "type", t64);
            return push_trap(c, msg);
        }
    }
    memset(&inv, 0, sizeof(w89_ainstr));
    if (tail != 0) {
        n = code->vsn;
        inv.kind = W89_A_RETINV;
        if (n > 0) {
            nb = sizeof(w89_value) * n;
            dst = malloc(nb);
            if (dst == 0) {
                return crash(c, "out of memory");
            }
            inv.vs0 = dst;
            src = &code->vs[0];
            memcpy(dst, src, nb);
            inv.nvs0 = n;
        }
        inv.finst = f;
    } else {
        inv.kind = W89_A_INVOKE;
        inv.finst = f;
    }
    code_consume(code);
    code_front_push(code, &inv);
    return W89_STEP_OK;
}

static w89_u64 read_le(const w89_byte *p, w89_u32 n)
{
    w89_u64 v;
    w89_u32 i;
    w89_byte b;
    w89_u64 bc;
    w89_u32 sh;
    w89_u64 shv;
    w89_u64 t;
    v = 0;
    for (i = 0; i < n; i = i + 1) {
        b = p[i];
        bc = (w89_u64)b;
        sh = 8 * i;
        shv = bc << sh;
        t = v | shv;
        v = t;
    }
    return v;
}

static void write_le(w89_byte *p, w89_u32 n, w89_u64 v)
{
    w89_u32 i;
    w89_byte b;
    w89_u64 s;
    for (i = 0; i < n; i = i + 1) {
        b = (w89_byte)v;
        p[i] = b;
        s = v >> 8;
        v = s;
    }
}

static int load_desc(w89_u32 op, w89_u32 *w, w89_u32 *is_float,
                     w89_u32 *is64, int *sext)
{
    switch (op) {
    case 0x28: *w = 4; *is_float = 0; *is64 = 0; *sext = 0; return 1;
    case 0x29: *w = 8; *is_float = 0; *is64 = 1; *sext = 0; return 1;
    case 0x2A: *w = 4; *is_float = 1; *is64 = 0; *sext = 0; return 1;
    case 0x2B: *w = 8; *is_float = 1; *is64 = 1; *sext = 0; return 1;
    case 0x2C: *w = 1; *is_float = 0; *is64 = 0; *sext = 1; return 1;
    case 0x2D: *w = 1; *is_float = 0; *is64 = 0; *sext = 0; return 1;
    case 0x2E: *w = 2; *is_float = 0; *is64 = 0; *sext = 1; return 1;
    case 0x2F: *w = 2; *is_float = 0; *is64 = 0; *sext = 0; return 1;
    case 0x30: *w = 1; *is_float = 0; *is64 = 1; *sext = 1; return 1;
    case 0x31: *w = 1; *is_float = 0; *is64 = 1; *sext = 0; return 1;
    case 0x32: *w = 2; *is_float = 0; *is64 = 1; *sext = 1; return 1;
    case 0x33: *w = 2; *is_float = 0; *is64 = 1; *sext = 0; return 1;
    case 0x34: *w = 4; *is_float = 0; *is64 = 1; *sext = 1; return 1;
    case 0x35: *w = 4; *is_float = 0; *is64 = 1; *sext = 0; return 1;
    default: return 0;
    }
}

static int store_desc(w89_u32 op, w89_u32 *w)
{
    switch (op) {
    case 0x36: *w = 4; return 1;
    case 0x37: *w = 8; return 1;
    case 0x38: *w = 4; return 1;
    case 0x39: *w = 8; return 1;
    case 0x3A: *w = 1; return 1;
    case 0x3B: *w = 2; return 1;
    case 0x3C: *w = 1; return 1;
    case 0x3D: *w = 2; return 1;
    case 0x3E: *w = 4; return 1;
    default: return 0;
    }
}

static w89_step_status step_load(w89_config *c, w89_ainstr *cur)
{
    w89_meminst *m;
    w89_u64 ea;
    w89_u32 w;
    w89_u32 is_float;
    w89_u32 is64;
    int sext;
    w89_frame *fr;
    w89_moduleinst *inst;
    w89_u32 memidx;
    w89_u32 nm;
    w89_code *code;
    w89_u32 vsn;
    w89_u32 op;
    w89_u32 ld;
    w89_u32 a64;
    w89_u32 x;
    w89_u64 off;
    w89_u64 mb;
    w89_u32 o;
    w89_byte *bp;
    w89_u64 rv;
    w89_u32 r32;
    w89_f32 f32;
    w89_f64 f64;
    w89_u64 v;
    w89_u64 t;
    w89_u32 vu;
    w89_u32 tu;
    const char *msg;
    fr = c->frame;
    if (fr == 0) {
        return crash(c, "undefined frame");
    }
    inst = fr->inst;
    if (inst == 0) {
        return crash(c, "undefined frame");
    }
    memidx = cur->in->memidx;
    nm = inst->nmemories;
    if (memidx >= nm) {
        msg = undef_msg("memory", memidx);
        return crash(c, msg);
    }
    code = &c->code;
    vsn = code->vsn;
    if (vsn < 1) {
        return crash(c, "stack underflow");
    }
    m = inst->memories[memidx];
    op = cur->in->op;
    ld = load_desc(op, &w, &is_float, &is64, &sext);
    if (ld == 0) {
        return crash(c, "internal error: bad load opcode");
    }
    a64 = m->limits.addr64;
    if (a64 != 0) {
        ea = pop_u64(c);
    } else {
        x = pop_u32(c);
        ea = (w89_u64)x;
    }
    off = cur->in->offset;
    ea = ea + off;
    mb = mem_bytes(m);
    o = oob(ea, w, mb);
    if (o != 0) {
        code_consume(code);
        return push_trap(c, "out of bounds memory access");
    }
    if (is_float != 0) {
        if (w == 4) {
            bp = &m->bytes[ea];
            rv = read_le(bp, 4);
            r32 = (w89_u32)rv;
            f32 = w89_bits_f32(r32);
            push_f32(c, f32);
        } else {
            bp = &m->bytes[ea];
            rv = read_le(bp, 8);
            f64 = w89_bits_f64(rv);
            push_f64(c, f64);
        }
    } else if (is64 != 0) {
        bp = &m->bytes[ea];
        v = read_le(bp, w);
        if (sext != 0) {
            if (w == 1) {
                if ((v & 0x80) != 0) {
                    t = v | 0xFFFFFFFFFFFFFF00UL;
                    v = t;
                }
            } else if (w == 2) {
                if ((v & 0x8000) != 0) {
                    t = v | 0xFFFFFFFFFFFF0000UL;
                    v = t;
                }
            } else if (w == 4) {
                if ((v & 0x80000000u) != 0) {
                    t = v | 0xFFFFFFFF00000000UL;
                    v = t;
                }
            }
        }
        push_u64(c, v);
    } else {
        bp = &m->bytes[ea];
        rv = read_le(bp, w);
        vu = (w89_u32)rv;
        if (sext != 0) {
            if (w == 1) {
                if ((vu & 0x80) != 0) {
                    tu = vu | 0xFFFFFF00u;
                    vu = tu;
                }
            } else if (w == 2) {
                if ((vu & 0x8000) != 0) {
                    tu = vu | 0xFFFF0000u;
                    vu = tu;
                }
            }
        }
        push_u32(c, vu);
    }
    code_consume(code);
    return W89_STEP_OK;
}

static w89_step_status step_store(w89_config *c, w89_ainstr *cur)
{
    w89_meminst *m;
    w89_u64 ea;
    w89_u32 w;
    w89_u64 v;
    w89_frame *fr;
    w89_moduleinst *inst;
    w89_u32 memidx;
    w89_u32 nm;
    w89_code *code;
    w89_u32 vsn;
    w89_u32 op;
    w89_u32 sd;
    w89_u32 a64;
    w89_u32 x;
    w89_u64 off;
    w89_u64 mb;
    w89_u32 o;
    w89_byte *bp;
    const char *msg;
    fr = c->frame;
    if (fr == 0) {
        return crash(c, "undefined frame");
    }
    inst = fr->inst;
    if (inst == 0) {
        return crash(c, "undefined frame");
    }
    memidx = cur->in->memidx;
    nm = inst->nmemories;
    if (memidx >= nm) {
        msg = undef_msg("memory", memidx);
        return crash(c, msg);
    }
    code = &c->code;
    vsn = code->vsn;
    if (vsn < 2) {
        return crash(c, "stack underflow");
    }
    m = inst->memories[memidx];
    op = cur->in->op;
    sd = store_desc(op, &w);
    if (sd == 0) {
        return crash(c, "internal error: bad store opcode");
    }
    v = pop_u64(c);
    a64 = m->limits.addr64;
    if (a64 != 0) {
        ea = pop_u64(c);
    } else {
        x = pop_u32(c);
        ea = (w89_u64)x;
    }
    off = cur->in->offset;
    ea = ea + off;
    mb = mem_bytes(m);
    o = oob(ea, w, mb);
    if (o != 0) {
        code_consume(code);
        return push_trap(c, "out of bounds memory access");
    }
    bp = &m->bytes[ea];
    write_le(bp, w, v);
    code_consume(code);
    return W89_STEP_OK;
}

static w89_step_status step_mem_size(w89_config *c, w89_ainstr *cur)
{
    w89_meminst *m;
    w89_frame *fr;
    w89_moduleinst *inst;
    w89_u32 idx;
    w89_u32 nm;
    w89_u32 a64;
    w89_u64 np;
    w89_u32 np32;
    w89_code *code;
    const char *msg;
    fr = c->frame;
    if (fr == 0) {
        return crash(c, "undefined frame");
    }
    inst = fr->inst;
    if (inst == 0) {
        return crash(c, "undefined frame");
    }
    idx = cur->in->idx;
    nm = inst->nmemories;
    if (idx >= nm) {
        msg = undef_msg("memory", idx);
        return crash(c, msg);
    }
    m = inst->memories[idx];
    a64 = m->limits.addr64;
    if (a64 != 0) {
        np = m->npages;
        push_u64(c, np);
    } else {
        np = m->npages;
        np32 = (w89_u32)np;
        push_u32(c, np32);
    }
    code = &c->code;
    code_consume(code);
    return W89_STEP_OK;
}

static w89_step_status step_mem_grow(w89_config *c, w89_ainstr *cur)
{
    w89_meminst *m;
    w89_u64 delta;
    w89_u64 npages;
    w89_u64 limit;
    w89_frame *fr;
    w89_moduleinst *inst;
    w89_u32 idx;
    w89_u32 nm;
    w89_code *code;
    w89_u32 vsn;
    w89_u32 a64;
    w89_u32 x;
    w89_u64 np0;
    w89_u64 nb;
    w89_u32 hm;
    w89_u64 mx;
    w89_u32 bad;
    w89_u64 oldbytes;
    w89_byte *nbuf;
    w89_byte *mb;
    size_t ts;
    w89_u64 cnt;
    w89_byte *nbp;
    w89_u32 np32;
    w89_u64 np;
    const char *msg;
    fr = c->frame;
    if (fr == 0) {
        return crash(c, "undefined frame");
    }
    inst = fr->inst;
    if (inst == 0) {
        return crash(c, "undefined frame");
    }
    idx = cur->in->idx;
    nm = inst->nmemories;
    if (idx >= nm) {
        msg = undef_msg("memory", idx);
        return crash(c, msg);
    }
    m = inst->memories[idx];
    code = &c->code;
    vsn = code->vsn;
    if (vsn < 1) {
        return crash(c, "stack underflow");
    }
    a64 = m->limits.addr64;
    if (a64 != 0) {
        delta = pop_u64(c);
    } else {
        x = pop_u32(c);
        delta = (w89_u64)x;
    }
    if (a64 != 0) {
        limit = 0x1000000000000UL;
    } else {
        limit = 0x10000UL;
    }
    np0 = m->npages;
    npages = np0 + delta;
    bad = 0;
    if (npages < np0) {
        bad = 1;
    }
    if (bad == 0) {
        if (npages > limit) {
            bad = 1;
        }
    }
    if (bad == 0) {
        hm = m->limits.has_max;
        if (hm != 0) {
            mx = m->limits.max;
            if (npages > mx) {
                bad = 1;
            }
        }
    }
    if (bad != 0) {
        if (a64 != 0) {
            push_u64(c, W89_I64_NEG1);
        } else {
            push_u32(c, 0xFFFFFFFFu);
        }
        code_consume(code);
        return W89_STEP_OK;
    }
    if (npages > np0) {
        oldbytes = mem_bytes(m);
        nb = npages * W89_PAGE_SIZE;
        mb = m->bytes;
        ts = (size_t)nb;
        nbuf = realloc(mb, ts);
        if (nbuf == 0) {
            if (a64 != 0) {
                push_u64(c, W89_I64_NEG1);
            } else {
                push_u32(c, 0xFFFFFFFFu);
            }
            code_consume(code);
            return W89_STEP_OK;
        }
        cnt = nb - oldbytes;
        nbp = nbuf + oldbytes;
        memset(nbp, 0, cnt);
        m->bytes = nbuf;
    }
    if (a64 != 0) {
        np = np0;
        push_u64(c, np);
    } else {
        np32 = (w89_u32)np0;
        push_u32(c, np32);
    }
    m->npages = npages;
    m->limits.min = npages;
    code_consume(code);
    return W89_STEP_OK;
}

static w89_step_status step_bulk(w89_config *c, w89_ainstr *cur)
{
    w89_u32 sub;
    w89_frame *fr;
    w89_moduleinst *inst;
    w89_code *code;
    w89_meminst *m;
    w89_datainst *d;
    w89_u64 n;
    w89_u64 s;
    w89_u64 dst;
    w89_u32 idx;
    w89_u32 idx2;
    w89_u32 nm;
    w89_u32 nd;
    w89_u32 vsn;
    w89_u64 mb;
    w89_u64 dl;
    w89_u64 dl64;
    w89_u32 o;
    w89_byte *dbp;
    const w89_byte *sbp;
    w89_datainst *dp;
    w89_meminst *md;
    w89_meminst *ms;
    w89_u32 nk;
    w89_u64 nb;
    w89_tableinst *tab;
    w89_eleminst *e;
    w89_u64 nt_;
    w89_u64 ne_;
    w89_ref *tep;
    w89_ref *sep;
    w89_ref rr;
    w89_value r;
    w89_u64 size;
    w89_u64 nsize;
    w89_u64 amax;
    w89_u64 j;
    w89_u32 a64;
    w89_u32 hm;
    w89_u64 mx;
    w89_u32 bad;
    w89_ref *nel;
    w89_ref *te;
    w89_u32 sz32;
    size_t ts;
    size_t csz;
    w89_tableinst *td;
    w89_tableinst *ts_;
    w89_u32 idx3;
    int ki;
    const char *msg;
    w89_u64 vx;
    w89_u64 tsz;
    w89_u32 en;
    sub = cur->in->sub;
    if (sub <= 0x07) {
        /* Saturating float-to-integer truncations (0xFC sub 0x00-0x07).
         * Never trap; primitives in numeric.c return the saturated value. */
        w89_f32 sf;
        w89_f64 sd;
        w89_u32 r32;
        w89_u64 r64;
        code = &c->code;
        switch (sub) {
        case 0x00:
            sf = pop_f32(c);
            r32 = w89_i32_trunc_sat_f32_s(sf);
            push_u32(c, r32);
            break;
        case 0x01:
            sf = pop_f32(c);
            r32 = w89_i32_trunc_sat_f32_u(sf);
            push_u32(c, r32);
            break;
        case 0x02:
            sd = pop_f64(c);
            r32 = w89_i32_trunc_sat_f64_s(sd);
            push_u32(c, r32);
            break;
        case 0x03:
            sd = pop_f64(c);
            r32 = w89_i32_trunc_sat_f64_u(sd);
            push_u32(c, r32);
            break;
        case 0x04:
            sf = pop_f32(c);
            r64 = w89_i64_trunc_sat_f32_s(sf);
            push_u64(c, r64);
            break;
        case 0x05:
            sf = pop_f32(c);
            r64 = w89_i64_trunc_sat_f32_u(sf);
            push_u64(c, r64);
            break;
        case 0x06:
            sd = pop_f64(c);
            r64 = w89_i64_trunc_sat_f64_s(sd);
            push_u64(c, r64);
            break;
        default:
            sd = pop_f64(c);
            r64 = w89_i64_trunc_sat_f64_u(sd);
            push_u64(c, r64);
            break;
        }
        code_consume(code);
        return W89_STEP_OK;
    }
    fr = c->frame;
    if (fr == 0) {
        return crash(c, "undefined frame");
    }
    inst = fr->inst;
    if (inst == 0) {
        return crash(c, "undefined frame");
    }
    code = &c->code;
    if (sub == 0x08) {
        idx = cur->in->idx;
        nm = inst->nmemories;
        if (idx >= nm) {
            msg = undef_msg("memory", idx);
            return crash(c, msg);
        }
        idx2 = cur->in->idx2;
        nd = inst->ndatas;
        if (idx2 >= nd) {
            msg = undef_msg("data segment", idx2);
            return crash(c, msg);
        }
        vsn = code->vsn;
        if (vsn < 3) {
            return crash(c, "stack underflow");
        }
        m = inst->memories[idx];
        d = inst->datas[idx2];
        n = pop_u64(c);
        s = pop_u64(c);
        dst = pop_u64(c);
        mb = mem_bytes(m);
        o = oob(dst, n, mb);
        if (o != 0) {
            code_consume(code);
            return push_trap(c, "out of bounds memory access");
        }
        dl = d->len;
        dl64 = (w89_u64)dl;
        o = oob(s, n, dl64);
        if (o != 0) {
            code_consume(code);
            return push_trap(c, "out of bounds memory access");
        }
        if (n > 0) {
            dbp = &m->bytes[dst];
            sbp = &d->bytes[s];
            csz = n;
            memcpy(dbp, sbp, csz);
        }
        code_consume(code);
        return W89_STEP_OK;
    }
    if (sub == 0x09) {
        idx = cur->in->idx;
        nd = inst->ndatas;
        if (idx >= nd) {
            msg = undef_msg("data segment", idx);
            return crash(c, msg);
        }
        dp = inst->datas[idx];
        dp->len = 0;
        code_consume(code);
        return W89_STEP_OK;
    }
    if (sub == 0x0A) {
        idx2 = cur->in->idx2;
        nm = inst->nmemories;
        if (idx2 >= nm) {
            msg = undef_msg("memory", idx2);
            return crash(c, msg);
        }
        idx = cur->in->idx;
        if (idx >= nm) {
            msg = undef_msg("memory", idx);
            return crash(c, msg);
        }
        vsn = code->vsn;
        if (vsn < 3) {
            return crash(c, "stack underflow");
        }
        md = inst->memories[idx2];
        ms = inst->memories[idx];
        n = pop_u64(c);
        s = pop_u64(c);
        dst = pop_u64(c);
        mb = mem_bytes(md);
        o = oob(dst, n, mb);
        if (o != 0) {
            code_consume(code);
            return push_trap(c, "out of bounds memory access");
        }
        mb = mem_bytes(ms);
        o = oob(s, n, mb);
        if (o != 0) {
            code_consume(code);
            return push_trap(c, "out of bounds memory access");
        }
        if (n > 0) {
            dbp = &md->bytes[dst];
            sbp = &ms->bytes[s];
            csz = n;
            memmove(dbp, sbp, csz);
        }
        code_consume(code);
        return W89_STEP_OK;
    }
    if (sub == 0x0B) {
        idx = cur->in->idx;
        nm = inst->nmemories;
        if (idx >= nm) {
            msg = undef_msg("memory", idx);
            return crash(c, msg);
        }
        vsn = code->vsn;
        if (vsn < 3) {
            return crash(c, "stack underflow");
        }
        m = inst->memories[idx];
        n = pop_u64(c);
        vx = pop_u64(c);
        nk = (w89_u32)vx;
        dst = pop_u64(c);
        mb = mem_bytes(m);
        o = oob(dst, n, mb);
        if (o != 0) {
            code_consume(code);
            return push_trap(c, "out of bounds memory access");
        }
        if (n > 0) {
            dbp = &m->bytes[dst];
            ki = (int)nk;
            csz = n;
            memset(dbp, ki, csz);
        }
        code_consume(code);
        return W89_STEP_OK;
    }
    if (sub == 0x0C) {
        idx = cur->in->idx;
        nt_ = inst->ntables;
        if (idx >= nt_) {
            msg = undef_msg("table", idx);
            return crash(c, msg);
        }
        idx2 = cur->in->idx2;
        ne_ = inst->nelems;
        if (idx2 >= ne_) {
            msg = undef_msg("element segment", idx2);
            return crash(c, msg);
        }
        vsn = code->vsn;
        if (vsn < 3) {
            return crash(c, "stack underflow");
        }
        tab = inst->tables[idx];
        e = inst->elems[idx2];
        n = pop_u64(c);
        s = pop_u64(c);
        dst = pop_u64(c);
        tsz = tab->size;
        o = oob(dst, n, tsz);
        if (o != 0) {
            code_consume(code);
            return push_trap(c, "out of bounds table access");
        }
        en = e->n;
        nb = (w89_u64)en;
        o = oob(s, n, nb);
        if (o != 0) {
            code_consume(code);
            return push_trap(c, "out of bounds table access");
        }
        if (n > 0) {
            tep = &tab->elems[dst];
            sep = &e->refs[s];
            csz = sizeof(w89_ref) * n;
            memcpy(tep, sep, csz);
        }
        code_consume(code);
        return W89_STEP_OK;
    }
    if (sub == 0x0D) {
        idx = cur->in->idx;
        ne_ = inst->nelems;
        if (idx >= ne_) {
            msg = undef_msg("element segment", idx);
            return crash(c, msg);
        }
        e = inst->elems[idx];
        e->n = 0;
        code_consume(code);
        return W89_STEP_OK;
    }
    if (sub == 0x0E) {
        idx2 = cur->in->idx2;
        nt_ = inst->ntables;
        if (idx2 >= nt_) {
            msg = undef_msg("table", idx2);
            return crash(c, msg);
        }
        idx = cur->in->idx;
        if (idx >= nt_) {
            msg = undef_msg("table", idx);
            return crash(c, msg);
        }
        vsn = code->vsn;
        if (vsn < 3) {
            return crash(c, "stack underflow");
        }
        td = inst->tables[idx2];
        ts_ = inst->tables[idx];
        n = pop_u64(c);
        s = pop_u64(c);
        dst = pop_u64(c);
        tsz = td->size;
        o = oob(dst, n, tsz);
        if (o != 0) {
            code_consume(code);
            return push_trap(c, "out of bounds table access");
        }
        tsz = ts_->size;
        o = oob(s, n, tsz);
        if (o != 0) {
            code_consume(code);
            return push_trap(c, "out of bounds table access");
        }
        if (n > 0) {
            tep = &td->elems[dst];
            sep = &ts_->elems[s];
            csz = sizeof(w89_ref) * n;
            memmove(tep, sep, csz);
        }
        code_consume(code);
        return W89_STEP_OK;
    }
    if (sub == 0x0F) {
        idx = cur->in->idx;
        nt_ = inst->ntables;
        if (idx >= nt_) {
            msg = undef_msg("table", idx);
            return crash(c, msg);
        }
        vsn = code->vsn;
        if (vsn < 2) {
            return crash(c, "stack underflow");
        }
        tab = inst->tables[idx];
        n = pop_u64(c);
        idx3 = code->vsn;
        idx3 = idx3 - 1;
        code->vsn = idx3;
        r = code->vs[idx3];
        size = tab->size;
        nsize = size + n;
        a64 = tab->type.limits.addr64;
        amax = table_addr_max(a64);
        bad = 0;
        if (nsize < size) {
            bad = 1;
        }
        if (bad == 0) {
            if (nsize > amax) {
                bad = 1;
            }
        }
        if (bad == 0) {
            hm = tab->type.limits.has_max;
            if (hm != 0) {
                mx = tab->type.limits.max;
                if (nsize > mx) {
                    bad = 1;
                }
            }
        }
        if (bad != 0) {
            if (a64 != 0) {
                push_u64(c, 0xFFFFFFFFFFFFFFFFUL);
            } else {
                push_u32(c, 0xFFFFFFFFu);
            }
            code_consume(code);
            return W89_STEP_OK;
        }
        if (nsize > size) {
            te = tab->elems;
            ts = (size_t)nsize;
            csz = ts * sizeof(w89_ref);
            nel = realloc(te, csz);
            if (nel == 0) {
                if (a64 != 0) {
                    push_u64(c, 0xFFFFFFFFFFFFFFFFUL);
                } else {
                    push_u32(c, 0xFFFFFFFFu);
                }
                code_consume(code);
                return W89_STEP_OK;
            }
            tab->elems = nel;
            rr = r.u.ref;
            for (j = size; j < nsize; j = j + 1) {
                tab->elems[j] = rr;
            }
        }
        if (a64 != 0) {
            push_u64(c, size);
        } else {
            sz32 = (w89_u32)size;
            push_u32(c, sz32);
        }
        tab->size = nsize;
        tab->type.limits.min = nsize;
        code_consume(code);
        return W89_STEP_OK;
    }
    if (sub == 0x10) {
        idx = cur->in->idx;
        nt_ = inst->ntables;
        if (idx >= nt_) {
            msg = undef_msg("table", idx);
            return crash(c, msg);
        }
        tab = inst->tables[idx];
        a64 = tab->type.limits.addr64;
        if (a64 != 0) {
            tsz = tab->size;
            push_u64(c, tsz);
        } else {
            tsz = tab->size;
            sz32 = (w89_u32)tsz;
            push_u32(c, sz32);
        }
        code_consume(code);
        return W89_STEP_OK;
    }
    if (sub == 0x11) {
        idx = cur->in->idx;
        nt_ = inst->ntables;
        if (idx >= nt_) {
            msg = undef_msg("table", idx);
            return crash(c, msg);
        }
        vsn = code->vsn;
        if (vsn < 3) {
            return crash(c, "stack underflow");
        }
        tab = inst->tables[idx];
        n = pop_u64(c);
        idx3 = code->vsn;
        idx3 = idx3 - 1;
        code->vsn = idx3;
        r = code->vs[idx3];
        dst = pop_u64(c);
        tsz = tab->size;
        o = oob(dst, n, tsz);
        if (o != 0) {
            code_consume(code);
            return push_trap(c, "out of bounds table access");
        }
        rr = r.u.ref;
        for (j = 0; j < n; j = j + 1) {
            idx3 = dst + j;
            tab->elems[idx3] = rr;
        }
        code_consume(code);
        return W89_STEP_OK;
    }
    return crash(c, "unsupported bulk instruction in evaluator");
}

static w89_step_status step_label(w89_config *c, w89_ainstr *lbl)
{
    w89_ainstr *h;
    w89_code *lblp;
    w89_code tmp;
    w89_code *tp;
    w89_code *codel;
    w89_u32 skip;
    w89_u32 i;
    w89_u32 n;
    w89_u32 take_n;
    w89_value *res;
    w89_u32 nvs0;
    w89_u32 base;
    w89_value *p0;
    w89_u32 hk;
    w89_u32 kk;
    w89_u32 contn;
    w89_u32 contpos;
    const w89_instr *csrc;
    w89_u32 cnsrc;
    w89_code cont;
    w89_err e;
    w89_u32 j;
    w89_u32 cb;
    w89_u32 cbi;
    w89_ainstr *cbp;
    w89_u32 jm;
    w89_config sub;
    w89_step_status st;
    const char *scrash;
    w89_byte sexh;
    w89_frame *sfr;
    w89_i64 bdgt;
    w89_value *pvs;
    w89_u32 nvs;
    lblp = &lbl->code;
    h = code_head(lblp);
    if (h == 0) {
        tmp = lbl->code;
        skip = lbl->skip;
        codel = &c->code;
        pvs = tmp.vs;
        nvs = tmp.vsn;
        e = vs_append(codel, pvs, nvs);
        if (e != W89_ERR_NONE) {
            tp = &tmp;
            w89_code_free(tp);
            return crash(c, "out of memory");
        }
        tp = &tmp;
        w89_code_free(tp);
        code_consume(codel);
        for (i = 0; i < skip; i = i + 1) {
            code_consume(codel);
        }
        return W89_STEP_OK;
    }
    hk = h->kind;
    kk = h->k;
    if (hk == W89_A_BREAKING) {
        if (kk == 0) {
            n = lbl->n;
            skip = lbl->skip;
            contn = lbl->contn;
            contpos = lbl->contpos;
            csrc = lbl->src;
            cnsrc = lbl->nsrc;
            nvs0 = h->nvs0;
            if (nvs0 < n) {
                take_n = nvs0;
            } else {
                take_n = n;
            }
            base = nvs0 - take_n;
            p0 = h->vs0;
            res = p0 + base;
            tmp = lbl->code;
            codel = &c->code;
            e = vs_append(codel, res, take_n);
            if (e != W89_ERR_NONE) {
                tp = &tmp;
                w89_code_free(tp);
                return crash(c, "out of memory");
            }
            p0 = h->vs0;
            free(p0);
            h->vs0 = 0;
            h->nvs0 = 0;
            tp = &tmp;
            w89_code_free(tp);
            code_consume(codel);
            for (i = 0; i < skip; i = i + 1) {
                code_consume(codel);
            }
            if (contn > 0) {
                w89_code_init(&cont);
                e = w89_code_range(&cont, csrc, cnsrc, contpos, contn);
                if (e != W89_ERR_NONE) {
                    tp = &cont;
                    w89_code_free(tp);
                    return crash(c, "out of memory");
                }
                cb = cont.bn;
                for (j = cb; j > 0; j = j - 1) {
                    cbi = j - 1;
                    cbp = &cont.back[cbi];
                    e = code_front_push(codel, cbp);
                    if (e != W89_ERR_NONE) {
                        tp = &cont;
                        w89_code_free(tp);
                        return crash(c, "out of memory");
                    }
                }
                tp = &cont;
                w89_code_free(tp);
            }
            return W89_STEP_OK;
        }
    }
    if (hk == W89_A_BREAKING) {
        tmp = lbl->code;
        kk = h->k;
        kk = kk - 1;
        h->k = kk;
        memcpy(lbl, h, sizeof(w89_ainstr));
        memset(h, 0, sizeof(w89_ainstr));
        tp = &tmp;
        w89_code_free(tp);
        return W89_STEP_OK;
    }
    jm = is_jumping(hk);
    if (jm != 0) {
        tmp = lbl->code;
        memcpy(lbl, h, sizeof(w89_ainstr));
        memset(h, 0, sizeof(w89_ainstr));
        tp = &tmp;
        w89_code_free(tp);
        return W89_STEP_OK;
    }
    sfr = c->frame;
    sub.frame = sfr;
    bdgt = c->budget;
    sub.budget = bdgt;
    sub.crash = 0;
    sub.exhausted = 0;
    tmp = lbl->code;
    sub.code = tmp;
    st = w89_step(&sub);
    tmp = sub.code;
    lbl->code = tmp;
    scrash = sub.crash;
    c->crash = scrash;
    sexh = sub.exhausted;
    c->exhausted = sexh;
    return st;
}

/* Numeric dispatch. */

static w89_step_status num_i32_cmp(w89_config *c, w89_u32 op)
{
    w89_u32 a;
    w89_u32 b;
    w89_u32 r;
    w89_code *code;
    w89_u32 n;
    code = &c->code;
    n = code->vsn;
    if (n < 2) {
        return crash(c, "stack underflow");
    }
    b = pop_u32(c);
    a = pop_u32(c);
    switch (op) {
    case 0x46: r = w89_i32_eq(a, b); break;
    case 0x47: r = w89_i32_ne(a, b); break;
    case 0x48: r = w89_i32_lt_s(a, b); break;
    case 0x49: r = w89_i32_lt_u(a, b); break;
    case 0x4A: r = w89_i32_gt_s(a, b); break;
    case 0x4B: r = w89_i32_gt_u(a, b); break;
    case 0x4C: r = w89_i32_le_s(a, b); break;
    case 0x4D: r = w89_i32_le_u(a, b); break;
    case 0x4E: r = w89_i32_ge_s(a, b); break;
    case 0x4F: r = w89_i32_ge_u(a, b); break;
    default: return crash(c, "internal error: bad opcode");
    }
    push_u32(c, r);
    code_consume(code);
    return W89_STEP_OK;
}

static w89_step_status num_i64_cmp(w89_config *c, w89_u32 op)
{
    w89_u64 a;
    w89_u64 b;
    w89_u32 r;
    w89_code *code;
    w89_u32 n;
    code = &c->code;
    n = code->vsn;
    if (n < 2) {
        return crash(c, "stack underflow");
    }
    b = pop_u64(c);
    a = pop_u64(c);
    switch (op) {
    case 0x51: r = w89_i64_eq(a, b); break;
    case 0x52: r = w89_i64_ne(a, b); break;
    case 0x53: r = w89_i64_lt_s(a, b); break;
    case 0x54: r = w89_i64_lt_u(a, b); break;
    case 0x55: r = w89_i64_gt_s(a, b); break;
    case 0x56: r = w89_i64_gt_u(a, b); break;
    case 0x57: r = w89_i64_le_s(a, b); break;
    case 0x58: r = w89_i64_le_u(a, b); break;
    case 0x59: r = w89_i64_ge_s(a, b); break;
    case 0x5A: r = w89_i64_ge_u(a, b); break;
    default: return crash(c, "internal error: bad opcode");
    }
    push_u32(c, r);
    code_consume(code);
    return W89_STEP_OK;
}

static w89_step_status num_f32_cmp(w89_config *c, w89_u32 op)
{
    w89_f32 a;
    w89_f32 b;
    w89_u32 r;
    w89_code *code;
    w89_u32 n;
    code = &c->code;
    n = code->vsn;
    if (n < 2) {
        return crash(c, "stack underflow");
    }
    b = pop_f32(c);
    a = pop_f32(c);
    switch (op) {
    case 0x5B: r = w89_f32_eq(a, b); break;
    case 0x5C: r = w89_f32_ne(a, b); break;
    case 0x5D: r = w89_f32_lt(a, b); break;
    case 0x5E: r = w89_f32_gt(a, b); break;
    case 0x5F: r = w89_f32_le(a, b); break;
    case 0x60: r = w89_f32_ge(a, b); break;
    default: return crash(c, "internal error: bad opcode");
    }
    push_u32(c, r);
    code_consume(code);
    return W89_STEP_OK;
}

static w89_step_status num_f64_cmp(w89_config *c, w89_u32 op)
{
    w89_f64 a;
    w89_f64 b;
    w89_u32 r;
    w89_code *code;
    w89_u32 n;
    code = &c->code;
    n = code->vsn;
    if (n < 2) {
        return crash(c, "stack underflow");
    }
    b = pop_f64(c);
    a = pop_f64(c);
    switch (op) {
    case 0x61: r = w89_f64_eq(a, b); break;
    case 0x62: r = w89_f64_ne(a, b); break;
    case 0x63: r = w89_f64_lt(a, b); break;
    case 0x64: r = w89_f64_gt(a, b); break;
    case 0x65: r = w89_f64_le(a, b); break;
    case 0x66: r = w89_f64_ge(a, b); break;
    default: return crash(c, "internal error: bad opcode");
    }
    push_u32(c, r);
    code_consume(code);
    return W89_STEP_OK;
}

static w89_step_status num_i32_un(w89_config *c, w89_u32 op)
{
    w89_u32 a;
    w89_u32 r;
    w89_code *code;
    w89_u32 n;
    code = &c->code;
    n = code->vsn;
    if (n < 1) {
        return crash(c, "stack underflow");
    }
    a = pop_u32(c);
    switch (op) {
    case 0x67: r = w89_i32_clz(a); break;
    case 0x68: r = w89_i32_ctz(a); break;
    case 0x69: r = w89_i32_popcnt(a); break;
    default: return crash(c, "internal error: bad opcode");
    }
    push_u32(c, r);
    code_consume(code);
    return W89_STEP_OK;
}

static w89_step_status num_i32_bin(w89_config *c, w89_u32 op)
{
    w89_u32 a;
    w89_u32 b;
    w89_u32 r;
    w89_code *code;
    w89_u32 n;
    code = &c->code;
    n = code->vsn;
    if (n < 2) {
        return crash(c, "stack underflow");
    }
    b = pop_u32(c);
    a = pop_u32(c);
    switch (op) {
    case 0x6A: r = w89_i32_add(a, b); break;
    case 0x6B: r = w89_i32_sub(a, b); break;
    case 0x6C: r = w89_i32_mul(a, b); break;
    case 0x6D:
        if (b == 0) {
            code_consume(code);
            return push_trap(c, "integer divide by zero");
        }
        if (a == W89_I32_MIN) {
            if (b == W89_I32_NEG1) {
                code_consume(code);
                return push_trap(c, "integer overflow");
            }
        }
        w89_i32_div_s(a, b, &r);
        break;
    case 0x6E:
        if (b == 0) {
            code_consume(code);
            return push_trap(c, "integer divide by zero");
        }
        w89_i32_div_u(a, b, &r);
        break;
    case 0x6F:
        if (b == 0) {
            code_consume(code);
            return push_trap(c, "integer divide by zero");
        }
        w89_i32_rem_s(a, b, &r);
        break;
    case 0x70:
        if (b == 0) {
            code_consume(code);
            return push_trap(c, "integer divide by zero");
        }
        w89_i32_rem_u(a, b, &r);
        break;
    case 0x71: r = w89_i32_and(a, b); break;
    case 0x72: r = w89_i32_or(a, b); break;
    case 0x73: r = w89_i32_xor(a, b); break;
    case 0x74: r = w89_i32_shl(a, b); break;
    case 0x75: r = w89_i32_shr_s(a, b); break;
    case 0x76: r = w89_i32_shr_u(a, b); break;
    case 0x77: r = w89_i32_rotl(a, b); break;
    case 0x78: r = w89_i32_rotr(a, b); break;
    default: return crash(c, "internal error: bad opcode");
    }
    push_u32(c, r);
    code_consume(code);
    return W89_STEP_OK;
}

static w89_step_status num_i64_un(w89_config *c, w89_u32 op)
{
    w89_u64 a;
    w89_u32 r;
    w89_code *code;
    w89_u32 n;
    code = &c->code;
    n = code->vsn;
    if (n < 1) {
        return crash(c, "stack underflow");
    }
    a = pop_u64(c);
    switch (op) {
    case 0x79: r = w89_i64_clz(a); break;
    case 0x7A: r = w89_i64_ctz(a); break;
    case 0x7B: r = w89_i64_popcnt(a); break;
    default: return crash(c, "internal error: bad opcode");
    }
    push_u32(c, r);
    code_consume(code);
    return W89_STEP_OK;
}

static w89_step_status num_i64_bin(w89_config *c, w89_u32 op)
{
    w89_u64 a;
    w89_u64 b;
    w89_u64 r;
    w89_code *code;
    w89_u32 n;
    code = &c->code;
    n = code->vsn;
    if (n < 2) {
        return crash(c, "stack underflow");
    }
    b = pop_u64(c);
    a = pop_u64(c);
    switch (op) {
    case 0x7C: r = w89_i64_add(a, b); break;
    case 0x7D: r = w89_i64_sub(a, b); break;
    case 0x7E: r = w89_i64_mul(a, b); break;
    case 0x7F:
        if (b == 0) {
            code_consume(code);
            return push_trap(c, "integer divide by zero");
        }
        if (a == W89_I64_MIN) {
            if (b == W89_I64_NEG1) {
                code_consume(code);
                return push_trap(c, "integer overflow");
            }
        }
        w89_i64_div_s(a, b, &r);
        break;
    case 0x80:
        if (b == 0) {
            code_consume(code);
            return push_trap(c, "integer divide by zero");
        }
        w89_i64_div_u(a, b, &r);
        break;
    case 0x81:
        if (b == 0) {
            code_consume(code);
            return push_trap(c, "integer divide by zero");
        }
        w89_i64_rem_s(a, b, &r);
        break;
    case 0x82:
        if (b == 0) {
            code_consume(code);
            return push_trap(c, "integer divide by zero");
        }
        w89_i64_rem_u(a, b, &r);
        break;
    case 0x83: r = w89_i64_and(a, b); break;
    case 0x84: r = w89_i64_or(a, b); break;
    case 0x85: r = w89_i64_xor(a, b); break;
    case 0x86: r = w89_i64_shl(a, b); break;
    case 0x87: r = w89_i64_shr_s(a, b); break;
    case 0x88: r = w89_i64_shr_u(a, b); break;
    case 0x89: r = w89_i64_rotl(a, b); break;
    case 0x8A: r = w89_i64_rotr(a, b); break;
    default: return crash(c, "internal error: bad opcode");
    }
    push_u64(c, r);
    code_consume(code);
    return W89_STEP_OK;
}

static w89_step_status num_f32_un(w89_config *c, w89_u32 op)
{
    w89_f32 a;
    w89_f32 r;
    w89_code *code;
    w89_u32 n;
    code = &c->code;
    n = code->vsn;
    if (n < 1) {
        return crash(c, "stack underflow");
    }
    a = pop_f32(c);
    switch (op) {
    case 0x8B: r = w89_f32_abs(a); break;
    case 0x8C: r = w89_f32_neg(a); break;
    case 0x8D: r = w89_f32_ceil(a); break;
    case 0x8E: r = w89_f32_floor(a); break;
    case 0x8F: r = w89_f32_trunc(a); break;
    case 0x90: r = w89_f32_nearest(a); break;
    case 0x91: r = w89_f32_sqrt(a); break;
    default: return crash(c, "internal error: bad opcode");
    }
    push_f32(c, r);
    code_consume(code);
    return W89_STEP_OK;
}

static w89_step_status num_f32_bin(w89_config *c, w89_u32 op)
{
    w89_f32 a;
    w89_f32 b;
    w89_f32 r;
    w89_code *code;
    w89_u32 n;
    code = &c->code;
    n = code->vsn;
    if (n < 2) {
        return crash(c, "stack underflow");
    }
    b = pop_f32(c);
    a = pop_f32(c);
    switch (op) {
    case 0x92: r = w89_f32_add(a, b); break;
    case 0x93: r = w89_f32_sub(a, b); break;
    case 0x94: r = w89_f32_mul(a, b); break;
    case 0x95: r = w89_f32_div(a, b); break;
    case 0x96: r = w89_f32_min(a, b); break;
    case 0x97: r = w89_f32_max(a, b); break;
    case 0x98: r = w89_f32_copysign(a, b); break;
    default: return crash(c, "internal error: bad opcode");
    }
    push_f32(c, r);
    code_consume(code);
    return W89_STEP_OK;
}

static w89_step_status num_f64_un(w89_config *c, w89_u32 op)
{
    w89_f64 a;
    w89_f64 r;
    w89_code *code;
    w89_u32 n;
    code = &c->code;
    n = code->vsn;
    if (n < 1) {
        return crash(c, "stack underflow");
    }
    a = pop_f64(c);
    switch (op) {
    case 0x99: r = w89_f64_abs(a); break;
    case 0x9A: r = w89_f64_neg(a); break;
    case 0x9B: r = w89_f64_ceil(a); break;
    case 0x9C: r = w89_f64_floor(a); break;
    case 0x9D: r = w89_f64_trunc(a); break;
    case 0x9E: r = w89_f64_nearest(a); break;
    case 0x9F: r = w89_f64_sqrt(a); break;
    default: return crash(c, "internal error: bad opcode");
    }
    push_f64(c, r);
    code_consume(code);
    return W89_STEP_OK;
}

static w89_step_status num_f64_bin(w89_config *c, w89_u32 op)
{
    w89_f64 a;
    w89_f64 b;
    w89_f64 r;
    w89_code *code;
    w89_u32 n;
    code = &c->code;
    n = code->vsn;
    if (n < 2) {
        return crash(c, "stack underflow");
    }
    b = pop_f64(c);
    a = pop_f64(c);
    switch (op) {
    case 0xA0: r = w89_f64_add(a, b); break;
    case 0xA1: r = w89_f64_sub(a, b); break;
    case 0xA2: r = w89_f64_mul(a, b); break;
    case 0xA3: r = w89_f64_div(a, b); break;
    case 0xA4: r = w89_f64_min(a, b); break;
    case 0xA5: r = w89_f64_max(a, b); break;
    case 0xA6: r = w89_f64_copysign(a, b); break;
    default: return crash(c, "internal error: bad opcode");
    }
    push_f64(c, r);
    code_consume(code);
    return W89_STEP_OK;
}

static w89_step_status num_cvt(w89_config *c, w89_u32 op)
{
    w89_u32 r32;
    w89_u64 r64;
    w89_f32 f;
    w89_f64 d;
    w89_code *code;
    w89_u32 n;
    w89_u32 x32;
    w89_u64 x64;
    w89_u32 ok;
    w89_f32 f32;
    w89_f64 f64;
    w89_u32 u32;
    w89_u64 u64;
    code = &c->code;
    switch (op) {
    case 0xA7:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        x64 = pop_u64(c);
        r32 = (w89_u32)x64;
        push_u32(c, r32);
        break;
    case 0xA8:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        f = pop_f32(c);
        ok = w89_i32_trunc_f32_s(f, &r32);
        if (ok == 0) {
            code_consume(code);
            return push_trap(c, "invalid conversion to integer");
        }
        push_u32(c, r32);
        break;
    case 0xA9:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        f = pop_f32(c);
        ok = w89_i32_trunc_f32_u(f, &r32);
        if (ok == 0) {
            code_consume(code);
            return push_trap(c, "invalid conversion to integer");
        }
        push_u32(c, r32);
        break;
    case 0xAA:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        d = pop_f64(c);
        ok = w89_i32_trunc_f64_s(d, &r32);
        if (ok == 0) {
            code_consume(code);
            return push_trap(c, "invalid conversion to integer");
        }
        push_u32(c, r32);
        break;
    case 0xAB:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        d = pop_f64(c);
        ok = w89_i32_trunc_f64_u(d, &r32);
        if (ok == 0) {
            code_consume(code);
            return push_trap(c, "invalid conversion to integer");
        }
        push_u32(c, r32);
        break;
    case 0xAC:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        x32 = pop_u32(c);
        r64 = w89_i64_extend_i32_s(x32);
        push_u64(c, r64);
        break;
    case 0xAD:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        x32 = pop_u32(c);
        r64 = w89_i64_extend_i32_u(x32);
        push_u64(c, r64);
        break;
    case 0xAE:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        f = pop_f32(c);
        ok = w89_i64_trunc_f32_s(f, &r64);
        if (ok == 0) {
            code_consume(code);
            return push_trap(c, "invalid conversion to integer");
        }
        push_u64(c, r64);
        break;
    case 0xAF:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        f = pop_f32(c);
        ok = w89_i64_trunc_f32_u(f, &r64);
        if (ok == 0) {
            code_consume(code);
            return push_trap(c, "invalid conversion to integer");
        }
        push_u64(c, r64);
        break;
    case 0xB0:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        d = pop_f64(c);
        ok = w89_i64_trunc_f64_s(d, &r64);
        if (ok == 0) {
            code_consume(code);
            return push_trap(c, "invalid conversion to integer");
        }
        push_u64(c, r64);
        break;
    case 0xB1:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        d = pop_f64(c);
        ok = w89_i64_trunc_f64_u(d, &r64);
        if (ok == 0) {
            code_consume(code);
            return push_trap(c, "invalid conversion to integer");
        }
        push_u64(c, r64);
        break;
    case 0xB2:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        x32 = pop_u32(c);
        f32 = w89_f32_convert_i32_s(x32);
        push_f32(c, f32);
        break;
    case 0xB3:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        x32 = pop_u32(c);
        f32 = w89_f32_convert_i32_u(x32);
        push_f32(c, f32);
        break;
    case 0xB4:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        x64 = pop_u64(c);
        f32 = w89_f32_convert_i64_s(x64);
        push_f32(c, f32);
        break;
    case 0xB5:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        x64 = pop_u64(c);
        f32 = w89_f32_convert_i64_u(x64);
        push_f32(c, f32);
        break;
    case 0xB6:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        d = pop_f64(c);
        f32 = w89_f32_demote_f64(d);
        push_f32(c, f32);
        break;
    case 0xB7:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        x32 = pop_u32(c);
        f64 = w89_f64_convert_i32_s(x32);
        push_f64(c, f64);
        break;
    case 0xB8:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        x32 = pop_u32(c);
        f64 = w89_f64_convert_i32_u(x32);
        push_f64(c, f64);
        break;
    case 0xB9:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        x64 = pop_u64(c);
        f64 = w89_f64_convert_i64_s(x64);
        push_f64(c, f64);
        break;
    case 0xBA:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        x64 = pop_u64(c);
        f64 = w89_f64_convert_i64_u(x64);
        push_f64(c, f64);
        break;
    case 0xBB:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        f = pop_f32(c);
        f64 = w89_f64_promote_f32(f);
        push_f64(c, f64);
        break;
    case 0xBC:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        f = pop_f32(c);
        u32 = w89_i32_reinterpret_f32(f);
        push_u32(c, u32);
        break;
    case 0xBD:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        d = pop_f64(c);
        u64 = w89_i64_reinterpret_f64(d);
        push_u64(c, u64);
        break;
    case 0xBE:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        x32 = pop_u32(c);
        f32 = w89_f32_reinterpret_i32(x32);
        push_f32(c, f32);
        break;
    case 0xBF:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        x64 = pop_u64(c);
        f64 = w89_f64_reinterpret_i64(x64);
        push_f64(c, f64);
        break;
    case 0xC0:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        x32 = pop_u32(c);
        u32 = w89_i32_extend8_s(x32);
        push_u32(c, u32);
        break;
    case 0xC1:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        x32 = pop_u32(c);
        u32 = w89_i32_extend16_s(x32);
        push_u32(c, u32);
        break;
    case 0xC2:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        x64 = pop_u64(c);
        u64 = w89_i64_extend8_s(x64);
        push_u64(c, u64);
        break;
    case 0xC3:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        x64 = pop_u64(c);
        u64 = w89_i64_extend16_s(x64);
        push_u64(c, u64);
        break;
    case 0xC4:
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        x64 = pop_u64(c);
        u64 = w89_i64_extend32_s(x64);
        push_u64(c, u64);
        break;
    default:
        return crash(c, "internal error: bad opcode");
    }
    code_consume(code);
    return W89_STEP_OK;
}

static w89_step_status step_numeric(w89_config *c,
                                    w89_u32 op)
{
    w89_u32 a;
    w89_u64 x64;
    w89_u32 z;
    w89_code *code;
    w89_u32 n;
    code = &c->code;
    switch (op) {
    case 0x45: {
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        a = pop_u32(c);
        z = w89_i32_eqz(a);
        push_u32(c, z);
        break;
    }
    case 0x50: {
        n = code->vsn;
        if (n < 1) {
            return crash(c, "stack underflow");
        }
        x64 = pop_u64(c);
        z = w89_i64_eqz(x64);
        push_u32(c, z);
        break;
    }
    case 0x46: case 0x47: case 0x48: case 0x49:
    case 0x4A: case 0x4B: case 0x4C: case 0x4D:
    case 0x4E: case 0x4F:
        return num_i32_cmp(c, op);
    case 0x51: case 0x52: case 0x53: case 0x54:
    case 0x55: case 0x56: case 0x57: case 0x58:
    case 0x59: case 0x5A:
        return num_i64_cmp(c, op);
    case 0x5B: case 0x5C: case 0x5D: case 0x5E:
    case 0x5F: case 0x60:
        return num_f32_cmp(c, op);
    case 0x61: case 0x62: case 0x63: case 0x64:
    case 0x65: case 0x66:
        return num_f64_cmp(c, op);
    case 0x67: case 0x68: case 0x69:
        return num_i32_un(c, op);
    case 0x6A: case 0x6B: case 0x6C: case 0x6D:
    case 0x6E: case 0x6F: case 0x70: case 0x71:
    case 0x72: case 0x73: case 0x74: case 0x75:
    case 0x76: case 0x77: case 0x78:
        return num_i32_bin(c, op);
    case 0x79: case 0x7A: case 0x7B:
        return num_i64_un(c, op);
    case 0x7C: case 0x7D: case 0x7E: case 0x7F:
    case 0x80: case 0x81: case 0x82: case 0x83:
    case 0x84: case 0x85: case 0x86: case 0x87:
    case 0x88: case 0x89: case 0x8A:
        return num_i64_bin(c, op);
    case 0x8B: case 0x8C: case 0x8D: case 0x8E:
    case 0x8F: case 0x90: case 0x91:
        return num_f32_un(c, op);
    case 0x92: case 0x93: case 0x94: case 0x95:
    case 0x96: case 0x97: case 0x98:
        return num_f32_bin(c, op);
    case 0x99: case 0x9A: case 0x9B: case 0x9C:
    case 0x9D: case 0x9E: case 0x9F:
        return num_f64_un(c, op);
    case 0xA0: case 0xA1: case 0xA2: case 0xA3:
    case 0xA4: case 0xA5: case 0xA6:
        return num_f64_bin(c, op);
    default:
        return num_cvt(c, op);
    }
    code_consume(code);
    return W89_STEP_OK;
}

static w89_step_status step_throw(w89_config *c, w89_ainstr *cur)
{
    w89_moduleinst *inst;
    w89_taginst *t;
    w89_ainstr th;
    w89_value *args;
    w89_u32 n;
    w89_frame *fr;
    w89_u32 idx;
    w89_u32 ntags;
    w89_code *code;
    w89_u32 vsn;
    w89_value *src;
    size_t nb;
    const char *msg;
    w89_u32 base;
    fr = c->frame;
    if (fr == 0) {
        return crash(c, "undefined frame");
    }
    inst = fr->inst;
    if (inst == 0) {
        return crash(c, "undefined frame");
    }
    idx = cur->in->idx;
    ntags = inst->ntags;
    if (idx >= ntags) {
        msg = undef_msg("tag", idx);
        return crash(c, msg);
    }
    t = inst->tags[idx];
    n = t->ft->nparams;
    code = &c->code;
    vsn = code->vsn;
    if (vsn < n) {
        return crash(c, "stack underflow");
    }
    args = 0;
    if (n > 0) {
        nb = sizeof(w89_value) * n;
        args = malloc(nb);
        if (args == 0) {
            return crash(c, "out of memory");
        }
        base = vsn - n;
        src = &code->vs[base];
        memcpy(args, src, nb);
    }
    code->vsn = base;
    memset(&th, 0, sizeof(w89_ainstr));
    th.kind = W89_A_THROWING;
    th.tag = t;
    th.vs0 = args;
    th.nvs0 = n;
    code_consume(code);
    code_front_push(code, &th);
    return W89_STEP_OK;
}

static w89_step_status step_throw_ref(w89_config *c, w89_ainstr *cur)
{
    w89_value v;
    w89_ainstr th;
    w89_value *args;
    w89_code *code;
    w89_u32 vsn;
    w89_u32 idx;
    w89_u32 is_ref;
    w89_u32 rk;
    w89_exn *ex;
    w89_u32 na;
    w89_value *src;
    w89_taginst *tg;
    size_t nb;
    (void)cur;
    code = &c->code;
    vsn = code->vsn;
    if (vsn < 1) {
        return crash(c, "stack underflow");
    }
    idx = vsn;
    idx = idx - 1;
    code->vsn = idx;
    v = code->vs[idx];
    is_ref = v.is_ref;
    rk = v.u.ref.kind;
    if (is_ref == 0) {
        code_consume(code);
        return push_trap(c, "null exception reference");
    }
    if (rk == W89_RK_NULL) {
        code_consume(code);
        return push_trap(c, "null exception reference");
    }
    if (rk != W89_RK_EXN) {
        return crash(c, "type mismatch at throw_ref");
    }
    ex = v.u.ref.u.exn;
    na = ex->nargs;
    args = 0;
    if (na > 0) {
        nb = sizeof(w89_value) * na;
        args = malloc(nb);
        if (args == 0) {
            return crash(c, "out of memory");
        }
        src = ex->args;
        memcpy(args, src, nb);
    }
    tg = ex->tag;
    memset(&th, 0, sizeof(w89_ainstr));
    th.kind = W89_A_THROWING;
    th.tag = tg;
    th.vs0 = args;
    th.nvs0 = na;
    code_consume(code);
    code_front_push(code, &th);
    return W89_STEP_OK;
}

/* ---------- S3.1 SIMD fixed-width v128 (design 0010, increment B1-b) ------
 * Carries the wide-value instruction set that needs no lane arithmetic:
 * v128.const (0x0C), v128.load (0x00), v128.store (0x0B). A v128 is a single
 * 16-byte value on the unified stack, distinct from scalars by validation;
 * here it is a raw byte carrier copied whole. */

static void push_v128(w89_config *c, w89_v128 w)
{
    w89_value v;
    w89_code *code;
    memset(&v, 0, sizeof(w89_value));
    v.u.vec = w;
    code = &c->code;
    vs_push(code, &v);
}

static w89_step_status simd_load(w89_config *c, w89_ainstr *cur)
{
    w89_meminst *m;
    w89_u64 ea;
    w89_frame *fr;
    w89_moduleinst *inst;
    w89_u32 memidx;
    w89_u32 nm;
    w89_code *code;
    w89_u32 vsn;
    w89_u32 a64;
    w89_u32 x;
    w89_u64 off;
    w89_u64 mb;
    w89_u32 o;
    w89_u32 k;
    w89_byte by;
    w89_byte *bp;
    w89_value v;
    const char *msg;
    fr = c->frame;
    if (fr == 0) {
        return crash(c, "undefined frame");
    }
    inst = fr->inst;
    if (inst == 0) {
        return crash(c, "undefined frame");
    }
    memidx = cur->in->memidx;
    nm = inst->nmemories;
    if (memidx >= nm) {
        msg = undef_msg("memory", memidx);
        return crash(c, msg);
    }
    code = &c->code;
    vsn = code->vsn;
    if (vsn < 1) {
        return crash(c, "stack underflow");
    }
    m = inst->memories[memidx];
    a64 = m->limits.addr64;
    if (a64 != 0) {
        ea = pop_u64(c);
    } else {
        x = pop_u32(c);
        ea = (w89_u64)x;
    }
    off = cur->in->offset;
    ea = ea + off;
    mb = mem_bytes(m);
    o = oob(ea, 16, mb);
    if (o != 0) {
        code_consume(code);
        return push_trap(c, "out of bounds memory access");
    }
    bp = &m->bytes[ea];
    memset(&v, 0, sizeof(w89_value));
    for (k = 0; k < 16; k = k + 1) {
        by = bp[k];
        v.u.vec.b[k] = by;
    }
    vs_push(code, &v);
    code_consume(code);
    return W89_STEP_OK;
}

static w89_step_status simd_store(w89_config *c, w89_ainstr *cur)
{
    w89_meminst *m;
    w89_u64 ea;
    w89_frame *fr;
    w89_moduleinst *inst;
    w89_u32 memidx;
    w89_u32 nm;
    w89_code *code;
    w89_u32 vsn;
    w89_u32 idx;
    w89_u32 a64;
    w89_u32 x;
    w89_u64 off;
    w89_u64 mb;
    w89_u32 o;
    w89_byte *bp;
    w89_value v;
    w89_v128 vec;
    w89_u32 k;
    w89_byte by;
    const char *msg;
    fr = c->frame;
    if (fr == 0) {
        return crash(c, "undefined frame");
    }
    inst = fr->inst;
    if (inst == 0) {
        return crash(c, "undefined frame");
    }
    memidx = cur->in->memidx;
    nm = inst->nmemories;
    if (memidx >= nm) {
        msg = undef_msg("memory", memidx);
        return crash(c, msg);
    }
    code = &c->code;
    vsn = code->vsn;
    if (vsn < 2) {
        return crash(c, "stack underflow");
    }
    m = inst->memories[memidx];
    idx = vsn;
    idx = idx - 1;
    code->vsn = idx;
    v = code->vs[idx];
    vec = v.u.vec;
    a64 = m->limits.addr64;
    if (a64 != 0) {
        ea = pop_u64(c);
    } else {
        x = pop_u32(c);
        ea = (w89_u64)x;
    }
    off = cur->in->offset;
    ea = ea + off;
    mb = mem_bytes(m);
    o = oob(ea, 16, mb);
    if (o != 0) {
        code_consume(code);
        return push_trap(c, "out of bounds memory access");
    }
    bp = &m->bytes[ea];
    for (k = 0; k < 16; k = k + 1) {
        by = vec.b[k];
        bp[k] = by;
    }
    code_consume(code);
    return W89_STEP_OK;
}

static w89_step_status step_simd(w89_config *c, w89_ainstr *cur)
{
    w89_code *code;
    w89_u32 sub;
    w89_v128 vec;
    code = &c->code;
    sub = cur->in->sub;
    switch (sub) {
    case 0x00:
        return simd_load(c, cur);
    case 0x0B:
        return simd_store(c, cur);
    case 0x0C:
        vec = cur->in->c128;
        push_v128(c, vec);
        code_consume(code);
        return W89_STEP_OK;
    default:
        return crash(c, "unsupported simd instruction in evaluator");
    }
}

static w89_step_status step_plain(w89_config *c, w89_ainstr *cur)
{
    w89_u32 op;
    w89_code *code;
    w89_u32 vsn;
    w89_u32 idx;
    w89_u32 n;
    w89_value v;
    w89_u64 num;
    w89_u32 d;
    w89_u32 sel;
    w89_u32 *labels;
    w89_u32 lv;
    w89_value r;
    w89_i32 c32;
    w89_u32 c32c;
    w89_i64 c64;
    w89_u64 c64c;
    w89_f32 f32v;
    w89_f64 f64v;
    w89_u64 bits;
    w89_local *l;
    w89_frame *fr;
    w89_u32 nlocals;
    w89_globalinst *g;
    w89_moduleinst *inst;
    w89_u32 mut;
    w89_funcinst *f;
    w89_ainstr inv;
    w89_value *dst;
    w89_value *src;
    size_t nb;
    w89_u32 is_ref;
    w89_tableinst *tab;
    w89_u32 i;
    w89_u64 x64;
    w89_u32 a64;
    w89_u64 tsz;
    w89_ref rr;
    w89_value va;
    w89_value vb;
    w89_u32 ar;
    w89_u32 br;
    w89_ref *p1;
    w89_ref *p2;
    w89_u32 eq;
    const char *msg;
    w89_byte st;
    op = cur->in->op;
    code = &c->code;
    switch (op) {
    case 0x00:
        code_consume(code);
        return push_trap(c, "unreachable executed");
    case 0x01:
        code_consume(code);
        return W89_STEP_OK;
    case 0x1A:
        vsn = code->vsn;
        if (vsn < 1) {
            return crash(c, "stack underflow");
        }
        idx = vsn;
        idx = idx - 1;
        code->vsn = idx;
        code_consume(code);
        return W89_STEP_OK;
    case 0x1B:
    case 0x1C:
        return step_select(c);
    case 0x0C:
        d = cur->in->idx;
        return step_br(c, d);
    case 0x0D: {
        vsn = code->vsn;
        if (vsn < 1) {
            return crash(c, "stack underflow");
        }
        idx = vsn;
        idx = idx - 1;
        code->vsn = idx;
        v = code->vs[idx];
        num = v.u.num;
        if (num != 0) {
            d = cur->in->idx;
            return step_br(c, d);
        }
        code_consume(code);
        return W89_STEP_OK;
    }
    case 0x0E: {
        vsn = code->vsn;
        if (vsn < 1) {
            return crash(c, "stack underflow");
        }
        sel = pop_u32(c);
        n = cur->in->n;
        if (sel >= n) {
            d = cur->in->idx;
            return step_br(c, d);
        }
        labels = cur->in->labels;
        lv = labels[sel];
        return step_br(c, lv);
    }
    case 0x0F:
        return step_return(c);
    case 0x08:
        return step_throw(c, cur);
    case 0x0A:
        return step_throw_ref(c, cur);
    case 0x02:
    case 0x03:
    case 0x04:
        return step_block(c, cur, op);
    case 0x1F:
        return step_try_table(c, cur);
    case 0x41:
    case 0x42:
    case 0x43:
    case 0x44: {
        memset(&v, 0, sizeof(w89_value));
        switch (op) {
        case 0x41:
            c32 = cur->in->c32;
            c32c = (w89_u32)c32;
            v.u.num = c32c;
            break;
        case 0x42:
            c64 = cur->in->c64;
            c64c = (w89_u64)c64;
            v.u.num = c64c;
            break;
        case 0x43:
            f32v = cur->in->f32;
            bits = w89_f32_bits(f32v);
            v.u.num = bits;
            break;
        case 0x44:
            f64v = cur->in->f64;
            bits = w89_f64_bits(f64v);
            v.u.num = bits;
            break;
        }
        vs_push(code, &v);
        code_consume(code);
        return W89_STEP_OK;
    }
    case 0x20: {
        fr = c->frame;
        if (fr == 0) {
            d = cur->in->idx;
            msg = undef_msg("local", d);
            return crash(c, msg);
        }
        idx = cur->in->idx;
        nlocals = fr->nlocals;
        if (idx >= nlocals) {
            msg = undef_msg("local", idx);
            return crash(c, msg);
        }
        l = &fr->locals[idx];
        st = l->set;
        if (st == 0) {
            return crash(c, "read of uninitialized local");
        }
        v = l->v;
        vs_push(code, &v);
        code_consume(code);
        return W89_STEP_OK;
    }
    case 0x21: {
        fr = c->frame;
        if (fr == 0) {
            d = cur->in->idx;
            msg = undef_msg("local", d);
            return crash(c, msg);
        }
        idx = cur->in->idx;
        nlocals = fr->nlocals;
        if (idx >= nlocals) {
            msg = undef_msg("local", idx);
            return crash(c, msg);
        }
        vsn = code->vsn;
        if (vsn < 1) {
            return crash(c, "stack underflow");
        }
        l = &fr->locals[idx];
        idx = vsn;
        idx = idx - 1;
        code->vsn = idx;
        v = code->vs[idx];
        l->v = v;
        l->set = 1;
        code_consume(code);
        return W89_STEP_OK;
    }
    case 0x22: {
        fr = c->frame;
        if (fr == 0) {
            d = cur->in->idx;
            msg = undef_msg("local", d);
            return crash(c, msg);
        }
        idx = cur->in->idx;
        nlocals = fr->nlocals;
        if (idx >= nlocals) {
            msg = undef_msg("local", idx);
            return crash(c, msg);
        }
        vsn = code->vsn;
        if (vsn < 1) {
            return crash(c, "stack underflow");
        }
        l = &fr->locals[idx];
        idx = vsn;
        idx = idx - 1;
        v = code->vs[idx];
        l->v = v;
        l->set = 1;
        code_consume(code);
        return W89_STEP_OK;
    }
    case 0x23: {
        fr = c->frame;
        if (fr == 0) {
            return crash(c, "undefined frame");
        }
        inst = fr->inst;
        if (inst == 0) {
            return crash(c, "undefined frame");
        }
        idx = cur->in->idx;
        n = inst->nglobals;
        if (idx >= n) {
            msg = undef_msg("global", idx);
            return crash(c, msg);
        }
        g = inst->globals[idx];
        v = g->value;
        vs_push(code, &v);
        code_consume(code);
        return W89_STEP_OK;
    }
    case 0x24: {
        fr = c->frame;
        if (fr == 0) {
            return crash(c, "undefined frame");
        }
        inst = fr->inst;
        if (inst == 0) {
            return crash(c, "undefined frame");
        }
        idx = cur->in->idx;
        n = inst->nglobals;
        if (idx >= n) {
            msg = undef_msg("global", idx);
            return crash(c, msg);
        }
        vsn = code->vsn;
        if (vsn < 1) {
            return crash(c, "stack underflow");
        }
        g = inst->globals[idx];
        mut = g->type.mut;
        if (mut == 0) {
            return crash(c, "write to immutable global");
        }
        idx = vsn;
        idx = idx - 1;
        code->vsn = idx;
        v = code->vs[idx];
        g->value = v;
        code_consume(code);
        return W89_STEP_OK;
    }
    case 0x10:
        return step_call(c, cur);
    case 0x11:
        return step_call_indirect(c, cur, 0);
    case 0x12: {
        fr = c->frame;
        if (fr == 0) {
            return crash(c, "undefined frame");
        }
        inst = fr->inst;
        if (inst == 0) {
            return crash(c, "undefined frame");
        }
        idx = cur->in->idx;
        n = inst->nfuncs;
        if (idx >= n) {
            msg = undef_msg("function", idx);
            return crash(c, msg);
        }
        f = inst->funcs[idx];
        memset(&inv, 0, sizeof(w89_ainstr));
        inv.kind = W89_A_RETINV;
        n = code->vsn;
        if (n > 0) {
            nb = sizeof(w89_value) * n;
            dst = malloc(nb);
            if (dst == 0) {
                return crash(c, "out of memory");
            }
            inv.vs0 = dst;
            src = &code->vs[0];
            memcpy(dst, src, nb);
            inv.nvs0 = n;
        }
        inv.finst = f;
        code_consume(code);
        code_front_push(code, &inv);
        return W89_STEP_OK;
    }
    case 0x13:
        return step_call_indirect(c, cur, 1);
    case 0x14:
        return step_call_ref(c, 0);
    case 0x15:
        return step_call_ref(c, 1);
    case 0x25: {
        fr = c->frame;
        if (fr == 0) {
            return crash(c, "undefined frame");
        }
        inst = fr->inst;
        if (inst == 0) {
            return crash(c, "undefined frame");
        }
        idx = cur->in->idx;
        n = inst->ntables;
        if (idx >= n) {
            msg = undef_msg("table", idx);
            return crash(c, msg);
        }
        vsn = code->vsn;
        if (vsn < 1) {
            return crash(c, "stack underflow");
        }
        tab = inst->tables[idx];
        a64 = tab->type.limits.addr64;
        if (a64 != 0) {
            x64 = pop_u64(c);
            i = (w89_u32)x64;
        } else {
            i = pop_u32(c);
        }
        tsz = tab->size;
        if (i >= tsz) {
            code_consume(code);
            msg = num_msg("out of bounds table access", 0);
            return push_trap(c, msg);
        }
        p1 = &tab->elems[i];
        v = w89_value_ref(p1);
        vs_push(code, &v);
        code_consume(code);
        return W89_STEP_OK;
    }
    case 0x26: {
        fr = c->frame;
        if (fr == 0) {
            return crash(c, "undefined frame");
        }
        inst = fr->inst;
        if (inst == 0) {
            return crash(c, "undefined frame");
        }
        idx = cur->in->idx;
        n = inst->ntables;
        if (idx >= n) {
            msg = undef_msg("table", idx);
            return crash(c, msg);
        }
        vsn = code->vsn;
        if (vsn < 2) {
            return crash(c, "stack underflow");
        }
        tab = inst->tables[idx];
        idx = vsn;
        idx = idx - 1;
        code->vsn = idx;
        v = code->vs[idx];
        a64 = tab->type.limits.addr64;
        if (a64 != 0) {
            x64 = pop_u64(c);
            i = (w89_u32)x64;
        } else {
            i = pop_u32(c);
        }
        tsz = tab->size;
        if (i >= tsz) {
            code_consume(code);
            msg = num_msg("out of bounds table access", 0);
            return push_trap(c, msg);
        }
        rr = v.u.ref;
        tab->elems[i] = rr;
        code_consume(code);
        return W89_STEP_OK;
    }
    case 0x28: case 0x29: case 0x2A: case 0x2B:
    case 0x2C: case 0x2D: case 0x2E: case 0x2F:
    case 0x30: case 0x31: case 0x32: case 0x33:
    case 0x34: case 0x35:
        return step_load(c, cur);
    case 0x36: case 0x37: case 0x38: case 0x39:
    case 0x3A: case 0x3B: case 0x3C: case 0x3D:
    case 0x3E:
        return step_store(c, cur);
    case 0x3F:
        return step_mem_size(c, cur);
    case 0x40:
        return step_mem_grow(c, cur);
    case 0xD0: {
        memset(&v, 0, sizeof(w89_value));
        v.is_ref = 1;
        v.u.ref.kind = W89_RK_NULL;
        vs_push(code, &v);
        code_consume(code);
        return W89_STEP_OK;
    }
    case 0xD1: {
        vsn = code->vsn;
        if (vsn < 1) {
            return crash(c, "stack underflow");
        }
        idx = vsn;
        idx = idx - 1;
        code->vsn = idx;
        v = code->vs[idx];
        memset(&r, 0, sizeof(w89_value));
        is_ref = v.is_ref;
        if (is_ref != 0) {
            p1 = &v.u.ref;
            eq = w89_ref_is_null(p1);
            if (eq != 0) {
                r.u.num = 1;
            } else {
                r.u.num = 0;
            }
        } else {
            r.u.num = 0;
        }
        vs_push(code, &r);
        code_consume(code);
        return W89_STEP_OK;
    }
    case 0xD2: {
        fr = c->frame;
        if (fr == 0) {
            return crash(c, "undefined frame");
        }
        inst = fr->inst;
        if (inst == 0) {
            return crash(c, "undefined frame");
        }
        idx = cur->in->idx;
        n = inst->nfuncs;
        if (idx >= n) {
            msg = undef_msg("function", idx);
            return crash(c, msg);
        }
        f = inst->funcs[idx];
        rr = w89_ref_func(f);
        v = w89_value_ref(&rr);
        vs_push(code, &v);
        code_consume(code);
        return W89_STEP_OK;
    }
    case 0xD3: {
        vsn = code->vsn;
        if (vsn < 2) {
            return crash(c, "stack underflow");
        }
        idx = vsn;
        idx = idx - 1;
        code->vsn = idx;
        vb = code->vs[idx];
        idx = idx - 1;
        code->vsn = idx;
        va = code->vs[idx];
        memset(&r, 0, sizeof(w89_value));
        ar = va.is_ref;
        br = vb.is_ref;
        if (ar != 0) {
            if (br != 0) {
                p1 = &va.u.ref;
                p2 = &vb.u.ref;
                eq = eq_ref(p1, p2);
                if (eq != 0) {
                    r.u.num = 1;
                } else {
                    r.u.num = 0;
                }
            } else {
                r.u.num = 0;
            }
        } else {
            r.u.num = 0;
        }
        vs_push(code, &r);
        code_consume(code);
        return W89_STEP_OK;
    }
    case 0xD4: {
        vsn = code->vsn;
        if (vsn < 1) {
            return crash(c, "stack underflow");
        }
        idx = vsn;
        idx = idx - 1;
        code->vsn = idx;
        v = code->vs[idx];
        is_ref = v.is_ref;
        if (is_ref == 0) {
            code_consume(code);
            return push_trap(c, "null reference");
        }
        p1 = &v.u.ref;
        eq = w89_ref_is_null(p1);
        if (eq != 0) {
            code_consume(code);
            return push_trap(c, "null reference");
        }
        vs_push(code, &v);
        code_consume(code);
        return W89_STEP_OK;
    }
    case 0xD5: {
        vsn = code->vsn;
        if (vsn < 1) {
            return crash(c, "stack underflow");
        }
        idx = vsn;
        idx = idx - 1;
        code->vsn = idx;
        v = code->vs[idx];
        is_ref = v.is_ref;
        if (is_ref != 0) {
            p1 = &v.u.ref;
            eq = w89_ref_is_null(p1);
            if (eq != 0) {
                d = cur->in->idx;
                return step_br(c, d);
            }
        }
        vs_push(code, &v);
        code_consume(code);
        return W89_STEP_OK;
    }
    case 0xD6: {
        vsn = code->vsn;
        if (vsn < 1) {
            return crash(c, "stack underflow");
        }
        idx = vsn;
        idx = idx - 1;
        code->vsn = idx;
        v = code->vs[idx];
        is_ref = v.is_ref;
        if (is_ref != 0) {
            p1 = &v.u.ref;
            eq = w89_ref_is_null(p1);
            if (eq == 0) {
                vs_push(code, &v);
                d = cur->in->idx;
                return step_br(c, d);
            }
        }
        code_consume(code);
        return W89_STEP_OK;
    }
    case 0x0B:
        code_consume(code);
        return W89_STEP_OK;
    case 0xFC:
        return step_bulk(c, cur);
    case 0xFD:
        return step_simd(c, cur);
    default:
        if (op >= 0x45) {
            if (op <= 0xC4) {
                return step_numeric(c, op);
            }
        }
        return crash(c, "unsupported instruction in evaluator");
    }
}

w89_step_status w89_step(w89_config *c)
{
    w89_ainstr *cur;
    w89_code *code;
    w89_value v;
    w89_ref *p;
    w89_akind k;
    code = &c->code;
    cur = code_head(code);
    if (cur == 0) {
        return W89_STEP_OK;
    }
    k = cur->kind;
    switch (k) {
    case W89_A_PLAIN:
        return step_plain(c, cur);
    case W89_A_REFER: {
        p = &cur->refer;
        v = w89_value_ref(p);
        vs_push(code, &v);
        code_consume(code);
        return W89_STEP_OK;
    }
    case W89_A_BREAKING:
        return crash(c, "undefined label");
    case W89_A_RETURNING:
    case W89_A_RETINV:
        return crash(c, "undefined frame");
    case W89_A_TRAP:
    case W89_A_THROWING:
        return crash(c, "internal error: terminal head in step");
    case W89_A_INVOKE:
        return step_invoke(c, cur);
    case W89_A_FRAME:
        return step_frame(c, cur);
    case W89_A_HANDLER:
        return step_handler(c, cur);
    case W89_A_LABEL:
        return step_label(c, cur);
    }
    return crash(c, "internal error: bad admin instr");
}

/* ---------- S2.2 iterative driver (design 0007) ----------
 * Walks a migratable function body on the explicit level stack (c->lvls)
 * over a single shared value stack (c->code.vs). Structured control
 * (block/loop/if/br/br_if/br_table) is orchestrated here; each plain
 * instruction is executed by handing a one-instruction code to the shared
 * w89_step, so numeric/const/local/select semantics and traps are
 * byte-identical by construction. Level 0 is a FUNC level that is not a
 * branch target (a br popping every block is a function return, S2.6). */

static w89_step_status iter_run_plain(w89_config *c, const w89_instr *in,
                                      w89_u32 pc)
{
    w89_ainstr a;
    w89_code *code;
    w89_err e;
    w89_step_status st;
    memset(&a, 0, sizeof(w89_ainstr));
    a.kind = W89_A_PLAIN;
    a.in = in;
    a.ipos = pc;
    code = &c->code;
    code->fn = 0;
    code->bn = 0;
    code->bhead = 0;
    e = code_front_push(code, &a);
    if (e != W89_ERR_NONE) {
        return crash(c, "out of memory");
    }
    st = w89_step(c);
    return st;
}

static w89_step_status iter_br(w89_config *c, w89_u32 depth)
{
    w89_code *code;
    w89_value *pdst;
    w89_value *psrc;
    w89_lvl *tg;
    w89_lvl *par;
    w89_u32 ln;
    w89_u32 tidx;
    w89_u32 base;
    w89_u32 vn;
    w89_u32 take;
    w89_u32 idx;
    w89_u32 nl;
    w89_u32 cpos;
    w89_u32 epos;
    w89_u32 pc2;
    w89_u32 li;
    w89_u32 cnum;
    w89_u32 kfb;
    w89_u32 nsr;
    w89_step_status st;
    size_t sz;
    code = &c->code;
    ln = c->ln;
    if (ln < 1) {
        st = W89_STEP_CRASH;
        c->crash = "undefined label";
        return st;
    }
    if (depth >= ln) {
        c->crash = "undefined label";
        return W89_STEP_CRASH;
    }
    tidx = ln - 1 - depth;
    tg = &c->lvls[tidx];
    base = tg->base;
    vn = code->vsn;
    take = tg->exit_arity;
    if (vn < take) {
        take = vn;
    }
    kfb = tg->kind;
    if (kfb == W89_LVL_FUNC) {
        /* A `br` whose target is the enclosing function body label acts as
         * that function's return (S2.6): present the br operands -- the
         * function results -- above the level's base, drop every level above
         * it, and mark it terminal so the driving loop's fn_splice returns to
         * the caller. Previously this was misreported as "undefined label". */
        if (take > 0) {
            idx = vn - take;
            pdst = &code->vs[base];
            psrc = &code->vs[idx];
            {
                size_t tu;
                tu = (size_t)take;
                sz = tu * sizeof(w89_value);
            }
            memmove(pdst, psrc, sz);
        }
        nl = base + take;
        code->vsn = nl;
        nsr = tg->nsrc;
        tg->pc = nsr;
        nl = tidx + 1;
        c->ln = nl;
        return W89_STEP_OK;
    }
    cnum = tg->contn;
    if (cnum != 0) {
        w89_u32 np;
        w89_u32 target;
        nl = tidx + 1;
        c->ln = nl;
        cpos = tg->contpos;
        tg->pc = cpos;
        /* Loop re-entry: the br operands (the loop's nparams values) become
         * the loop body's input and must sit at base. When they already do
         * (vn == base + nparams) leave the stack as is; otherwise move the
         * top nparams down to base, discarding anything pushed above them. */
        np = tg->nparams;
        vn = code->vsn;
        {
            w89_u32 tgt;
            tgt = base + np;
            target = tgt;
        }
        if (vn > target) {
            idx = vn - np;
            pdst = &code->vs[base];
            psrc = &code->vs[idx];
            if (np > 0) {
                size_t tu;
                tu = (size_t)np;
                sz = tu * sizeof(w89_value);
                memmove(pdst, psrc, sz);
            }
            code->vsn = target;
        }
        return W89_STEP_OK;
    }
    if (take > 0) {
        idx = vn - take;
        pdst = &code->vs[base];
        psrc = &code->vs[idx];
        {
            size_t tu;
            tu = (size_t)take;
            sz = tu * sizeof(w89_value);
        }
        memmove(pdst, psrc, sz);
    }
    {
        w89_u32 nb;
        nb = base + take;
        code->vsn = nb;
    }
    c->ln = tidx;
    ln = c->ln;
    if (ln > 0) {
        li = ln - 1;
        par = &c->lvls[li];
        epos = tg->end;
        pc2 = epos + 1;
        par->pc = pc2;
    }
    return W89_STEP_OK;
}

/* ---------------- S2.3: calls/frames in the iterative driver ----------
 * The iterative driver is the sole wasm evaluation path (S2.6): every
 * non-host function body, reachable through `call`/`call_indirect`/start or
 * top-level invocation, is driven here. Host functions are invoked as
 * leaves directly by the caller. */

static void iter_sync_frame(w89_config *c)
{
    w89_u32 i;
    w89_u32 li;
    w89_frame *frm;
    w89_lvlkind kd;
    li = c->ln;
    if (li == 0) {
        return;
    }
    i = li;
    while (i > 0) {
        i = i - 1;
        kd = c->lvls[i].kind;
        if (kd == W89_LVL_FUNC) {
            frm = c->lvls[i].frame;
            c->frame = frm;
            return;
        }
    }
    kd = c->lvls[0].kind;
    if (kd == W89_LVL_FUNC) {
        frm = c->lvls[0].frame;
        c->frame = frm;
    }
}

static void iter_free_frames(w89_config *c)
{
    w89_u32 i;
    w89_u32 ln2;
    w89_lvlkind kd;
    w89_frame *frm;
    ln2 = c->ln;
    for (i = 1; i < ln2; i = i + 1) {
        kd = c->lvls[i].kind;
        if (kd == W89_LVL_FUNC) {
            frm = c->lvls[i].frame;
            if (frm != 0) {
                frame_free(frm);
            }
            c->lvls[i].frame = 0;
        }
    }
}

static w89_u32 iter_func_of(w89_config *c, w89_u32 li)
{
    w89_lvlkind kd;
    while (li > 0) {
        kd = c->lvls[li].kind;
        if (kd == W89_LVL_FUNC) {
            return li;
        }
        li = li - 1;
    }
    return 0;
}

/* Copy and remove the top n values of the driver value stack. Returns the
 * owned copy (0 if n == 0 or out of memory). Used by a driver throw to take
 * the tag payload off the shared stack. */
static w89_value *thr_take_top(w89_code *code, w89_u32 n)
{
    w89_value *p;
    size_t nb;
    w89_u32 base;
    w89_u32 vn;
    w89_value *srcp;
    if (n == 0) {
        return 0;
    }
    nb = sizeof(w89_value) * n;
    p = malloc(nb);
    if (p == 0) {
        return 0;
    }
    vn = code->vsn;
    base = vn - n;
    srcp = &code->vs[base];
    memcpy(p, srcp, nb);
    code->vsn = base;
    return p;
}

/* Copy an owned array of n values. Returns 0 only if n == 0 or oom. */
static w89_value *thr_copy_val(const w89_value *src, w89_u32 n)
{
    w89_value *p;
    size_t nb;
    if (n == 0) {
        return 0;
    }
    nb = sizeof(w89_value) * n;
    p = malloc(nb);
    if (p == 0) {
        return 0;
    }
    memcpy(p, src, nb);
    return p;
}

w89_eval_out w89_eval_iter(w89_config *c)
{
    w89_eval_out out;
    w89_code *code;
    w89_lvl *tp;
    w89_lvl *par;
    w89_ainstr *h;
    const w89_instr *src;
    const w89_instr *in;
    const w89_typeenv *env;
    const w89_typeenv *tenv;
    const w89_typeenv *tenv2;
    const w89_ft *ef;
    const w89_ft *fft;
    w89_moduleinst *mi;
    w89_frame *fr;
    w89_frame *nframe;
    w89_frame *frm;
    w89_value *vp;
    w89_value *pvs;
    const w89_func *cfn;
    const w89_vt *lt;
    const w89_blocktype *btp;
    const char *msg;
    const char *trap;
    w89_funcinst *cf;
    w89_tableinst *tab;
    w89_moduleinst *cfinst;
    w89_ref rref;
    w89_ref tr;
    w89_ref *p1;
    w89_value res[16];
    w89_value cv;
    w89_step_status st;
    w89_host_status hs;
    w89_hostfn hf;
    w89_err e;
    w89_lvl lv;
    w89_lvl *tl;
    w89_lvlkind kd;
    w89_u64 sz;
    w89_u32 tn;
    w89_u32 nlocf;
    w89_u64 t64;
    w89_u64 cn;
    w89_u32 nsrc;
    w89_u32 pc;
    w89_u32 end;
    w89_u32 op;
    w89_u32 resn;
    w89_u32 n1;
    w89_u32 n2;
    w89_u32 bs;
    w89_u32 be;
    w89_u32 has_else;
    w89_u32 ep;
    w89_u32 vn;
    w89_u32 vn2;
    w89_u32 cond;
    w89_u32 arm_s;
    w89_u32 ln;
    w89_u32 nl;
    w89_u32 pc2;
    w89_u32 sel;
    w89_u32 nlab;
    w89_u32 dep;
    w89_u32 hn;
    w89_u32 n1c;
    w89_u32 n2c;
    w89_u32 cvs;
    w89_u32 cbase;
    w89_u32 idx;
    w89_u32 tableidx;
    w89_u32 typeidx;
    w89_u32 nt;
    w89_u32 nf;
    w89_u32 nlfc;
    w89_u32 nlocs;
    w89_u32 rk;
    w89_u32 fbase;
    w89_u32 far;
    w89_u32 f_avail;
    w89_u32 f_take;
    w89_u32 cfi;
    w89_u32 li;
    w89_u32 i;
    w89_u32 j;
    w89_u32 k;
    w89_u32 nres;
    w89_u32 pi;
    const w89_instr *its;
    w89_u32 nn;
    w89_u32 ftypeidx;
    w89_i64 bdgt;
    w89_i64 bnew;
    int is_host;
    int mt;
    int eq;
    int istail;
    w89_taginst *ttg;
    w89_exn *te;
    w89_store *ts;
    w89_value tv;
    w89_value *pp;
    w89_u32 pn;
    w89_u32 ttidx;
    w89_u32 ack;
    w89_u32 ldep;
    w89_u32 ti;
    w89_u32 uu;
    w89_u32 dbase;
    w89_u32 t_nc;
    w89_u32 t_nt;
    w89_lvl *tgl;
    w89_u32 tk;
    w89_u32 pix;
    w89_u32 pend;
    int ack_kind;
    w89_moduleinst *t_inst;
    w89_frame *t_fr;
    w89_taginst **t_tags;
    const w89_catch *ct;
    const w89_catch *cc;
    w89_u32 cn2;
    w89_u32 isr0;
    w89_u32 kd2;
    w89_frame *fr2;
    w89_u32 q;
    w89_taginst *tgx;
    w89_value *eargs;
    w89_lvl *tl2;
    w89_value *fdst;
    w89_value *fsrc;
    w89_u32 idx2;
    size_t fmv_n;
    size_t fmv_t;

    memset(&out, 0, sizeof(w89_eval_out));
    code = &c->code;
    fr = c->frame;
    src = code->src;
    nsrc = code->nsrc;

    /* Seed the outermost FUNC level over the whole body (not a branch
     * target). Its frame is caller-owned (never freed here); exit_arity
     * carries the top-level result count (0 == harness leftover). */
    code->fn = 0;
    code->bn = 0;
    code->bhead = 0;
    memset(&lv, 0, sizeof(w89_lvl));
    lv.kind = W89_LVL_FUNC;
    lv.frame = fr;
    lv.src = src;
    lv.nsrc = nsrc;
    lv.end = nsrc;
    resn = c->resn;
    lv.exit_arity = resn;
    e = w89_lvl_push(c, &lv);
    if (e != W89_ERR_NONE) {
        c->crash = "out of memory";
        goto out_crash;
    }

    for (;;) {
        ln = c->ln;
        if (ln == 0) {
            goto out_done;
        }
        li = ln - 1;
        tp = &c->lvls[li];
        pc = tp->pc;
        end = tp->end;
        kd = tp->kind;
        src = tp->src;
        nsrc = tp->nsrc;
        if (kd == W89_LVL_FUNC) {
            if (pc >= nsrc) {
                cfi = li;
                goto fn_splice;
            }
        }
        if (kd == W89_LVL_BLOCK) {
            if (pc >= end) {
                nl = ln - 1;
                c->ln = nl;
                ln = nl;
                if (ln > 0) {
                    li = ln - 1;
                    par = &c->lvls[li];
                    pc2 = end + 1;
                    par->pc = pc2;
                }
                continue;
            }
        }
        in = &src[pc];
        op = in->op;
        if (op == 0x05) {
            nl = ln - 1;
            c->ln = nl;
            ln = nl;
            if (ln > 0) {
                li = ln - 1;
                par = &c->lvls[li];
                pc2 = end + 1;
                par->pc = pc2;
            }
            continue;
        }
        if (op == 0x0B) {
            nl = ln - 1;
            c->ln = nl;
            ln = nl;
            if (ln > 0) {
                li = ln - 1;
                par = &c->lvls[li];
                pc2 = end + 1;
                par->pc = pc2;
            }
            continue;
        }
        if (op == 0x02) {
            goto enter_block;
        }
        if (op == 0x03) {
            goto enter_block;
        }
        if (op == 0x04) {
            goto enter_block;
        }
        if (op == 0x1F) {
            goto enter_block;
        }
        if (op == 0x08) {
            goto do_throw;
        }
        if (op == 0x0A) {
            goto do_throw_ref;
        }
        if (op == 0x0F) {
            cfi = iter_func_of(c, li);
            goto fn_splice;
        }
        if (op == 0x10) {
            istail = 0;
            goto resolve_direct;
        }
        if (op == 0x12) {
            istail = 1;
            goto resolve_direct;
        }
        if (op == 0x11) {
            istail = 0;
            goto resolve_ind;
        }
        if (op == 0x13) {
            istail = 1;
            goto resolve_ind;
        }
        if (op == 0x14) {
            istail = 0;
            goto resolve_ref;
        }
        if (op == 0x15) {
            istail = 1;
            goto resolve_ref;
        }
        if (op == 0xD5) {
            goto do_br_on_null;
        }
        if (op == 0xD6) {
            goto do_br_on_non_null;
        }
        goto block_done;
resolve_ref:
        vn = code->vsn;
        if (vn < 1) {
            c->crash = "stack underflow";
            goto out_crash;
        }
        vn2 = vn - 1;
        cv = code->vs[vn2];
        code->vsn = vn2;
        isr0 = cv.is_ref;
        if (isr0 == 0) {
            c->crash = "null function reference";
            goto out_trap;
        }
        rk = cv.u.ref.kind;
        if (rk == W89_RK_NULL) {
            c->crash = "null function reference";
            goto out_trap;
        }
        if (rk != W89_RK_FUNC) {
            c->crash = "type mismatch at call_ref";
            goto out_crash;
        }
        p1 = &cv.u.ref;
        cf = p1->u.func;
        if (istail != 0) {
            goto do_tail;
        }
        goto do_call;
resolve_direct:
        idx = in->idx;
        mi = c->frame->inst;
        nf = mi->nfuncs;
        if (idx >= nf) {
            msg = undef_msg("function", idx);
            c->crash = msg;
            goto out_crash;
        }
        cf = mi->funcs[idx];
        if (istail != 0) {
            goto do_tail;
        }
        goto do_call;
resolve_ind:
        typeidx = in->idx2;
        tableidx = in->idx;
        mi = c->frame->inst;
        nt = mi->ntables;
        if (tableidx >= nt) {
            msg = undef_msg("table", tableidx);
            c->crash = msg;
            goto out_crash;
        }
        tab = mi->tables[tableidx];
        cvs = code->vsn;
        if (cvs < 1) {
            c->crash = "stack underflow";
            goto out_crash;
        }
        i = pop_u32(c);
        t64 = (w89_u64)i;
        sz = tab->size;
        if (t64 >= sz) {
            msg = num_msg("undefined element", t64);
            c->crash = msg;
            goto out_trap;
        }
        rref = tab->elems[i];
        rk = rref.kind;
        if (rk == W89_RK_NULL) {
            msg = num_msg("uninitialized element", t64);
            c->crash = msg;
            goto out_trap;
        }
        if (rk != W89_RK_FUNC) {
            c->crash = "type mismatch at call_indirect";
            goto out_crash;
        }
        cf = rref.u.func;
        is_host = cf->is_host;
        tenv = mi->types;
        if (is_host != 0) {
            ef = &tenv->types[typeidx].sub->ft;
            fft = cf->ft;
            mt = match_ft_x(0, fft, tenv, ef);
            if (mt == 0) {
                msg = num_msg("indirect call type mismatch, expected "
                              "type", t64);
                c->crash = msg;
                goto out_trap;
            }
        } else {
            cfinst = cf->inst;
            tenv2 = cfinst->types;
            ftypeidx = cf->typeidx;
            mt = w89_match_deftype_x(tenv2, ftypeidx, tenv, typeidx);
            if (mt == 0) {
                msg = num_msg("indirect call type mismatch, expected "
                              "type", t64);
                c->crash = msg;
                goto out_trap;
            }
        }
        if (istail != 0) {
            goto do_tail;
        }
        goto do_call;

enter_block:
        e = w89_block_extent(src, nsrc, pc, &bs, &be, &has_else, &ep);
        if (e != W89_ERR_NONE) {
            c->crash = "malformed block";
            goto out_crash;
        }
        btp = &in->bt;
        mi = c->frame->inst;
        env = mi->types;
        w89_blocktype_arity(env, btp, &n1, &n2);
        arm_s = bs;
        if (op == 0x04) {
            vn = code->vsn;
            if (vn < 1) {
                c->crash = "stack underflow";
                goto out_crash;
            }
            vn = vn - 1;
            cv = code->vs[vn];
            cn = cv.u.num;
            cond = (w89_u32)cn;
            code->vsn = vn;
            if (cond == 0) {
                if (has_else != 0) {
                    arm_s = ep + 1;
                } else {
                    arm_s = be;
                }
            } else {
                arm_s = bs;
            }
        }
        memset(&lv, 0, sizeof(w89_lvl));
        lv.kind = W89_LVL_BLOCK;
        lv.src = src;
        lv.nsrc = nsrc;
        lv.pc = arm_s;
        lv.end = be;
        vn = code->vsn;
        /* A structured block's input params (n1) remain on the shared stack
         * as the body's starting operands; base points just below them so a
         * normal end or a br-out leaves the n2 results at base. */
        if (vn < n1) {
            c->crash = "stack underflow";
            goto out_crash;
        }
        {
            w89_u32 bb;
            bb = vn - n1;
            lv.base = bb;
        }
        lv.nparams = n1;
        lv.exit_arity = n2;
        if (op == 0x03) {
            lv.contpos = bs;
            lv.contn = 1;
        }
        if (op == 0x1F) {
            cc = in->catches;
            lv.catches = cc;
            cn2 = in->n;
            lv.ncatches = cn2;
        }
        e = w89_lvl_push(c, &lv);
        if (e != W89_ERR_NONE) {
            c->crash = "out of memory";
            goto out_crash;
        }
        continue;

        do_throw:
        mi = c->frame->inst;
        if (mi == 0) {
            c->crash = "undefined frame";
            goto out_crash;
        }
        nt = mi->ntags;
        idx = in->idx;
        if (idx >= nt) {
            msg = undef_msg("tag", idx);
            c->crash = msg;
            goto out_crash;
        }
        ttg = mi->tags[idx];
        pn = ttg->ft->nparams;
        vn = code->vsn;
        if (vn < pn) {
            c->crash = "stack underflow";
            goto out_crash;
        }
        pp = thr_take_top(code, pn);
        if (pp == 0) {
            if (pn > 0) {
                c->crash = "out of memory";
                goto out_crash;
            }
        }
        goto throw_scan;

        do_throw_ref:
        vn = code->vsn;
        if (vn < 1) {
            c->crash = "stack underflow";
            goto out_crash;
        }
        vn = vn - 1;
        cv = code->vs[vn];
        code->vsn = vn;
        isr0 = cv.is_ref;
        if (isr0 == 0) {
            c->crash = "null exception reference";
            goto out_trap;
        }
        rk = cv.u.ref.kind;
        if (rk == W89_RK_NULL) {
            c->crash = "null exception reference";
            goto out_trap;
        }
        if (rk != W89_RK_EXN) {
            c->crash = "type mismatch at throw_ref";
            goto out_crash;
        }
        te = cv.u.ref.u.exn;
        ttg = te->tag;
        pn = te->nargs;
        eargs = te->args;
        pp = thr_copy_val(eargs, pn);
        goto throw_scan;

        throw_scan:
        for (j = ln; j > 0; j = j - 1) {
            li = j - 1;
            tl = &c->lvls[li];
            kd2 = tl->kind;
            if (kd2 != W89_LVL_BLOCK) {
                continue;
            }
            t_nc = tl->ncatches;
            if (t_nc == 0) {
                continue;
            }
            uu = iter_func_of(c, li);
            tl2 = &c->lvls[uu];
            t_fr = tl2->frame;
            t_inst = 0;
            if (t_fr != 0) {
                t_inst = t_fr->inst;
            }
            for (k = 0; k < t_nc; k = k + 1) {
                ct = &tl->catches[k];
                ack_kind = ct->kind;
                if (ack_kind == 2) {
                    ttidx = li;
                    ack = k;
                    goto throw_hit;
                }
                if (ack_kind == 3) {
                    ttidx = li;
                    ack = k;
                    goto throw_hit;
                }
                if (t_inst == 0) {
                    continue;
                }
                idx = ct->tagidx;
                t_nt = t_inst->ntags;
                if (idx >= t_nt) {
                    continue;
                }
                t_tags = t_inst->tags;
                tgx = t_tags[idx];
                if (tgx != ttg) {
                    continue;
                }
                ttidx = li;
                ack = k;
                goto throw_hit;
            }
        }
        goto out_exception;

        throw_hit:
        ct = &c->lvls[ttidx].catches[ack];
        ack_kind = ct->kind;
        ldep = ct->label;
        /* The catch names the block enclosing the try_table: label 0 is the
         * try_table's immediate parent, label 1 its grandparent, etc. */
        if (ldep < ttidx) {
            ti = ttidx - 1 - ldep;
        } else {
            ti = 0;
        }
        for (i = ln; i > ti; i = i - 1) {
            q = i - 1;
            tl2 = &c->lvls[q];
            kd2 = tl2->kind;
            if (kd2 != W89_LVL_FUNC) {
                continue;
            }
            if (q == 0) {
                continue;
            }
            fr2 = tl2->frame;
            if (fr2 != 0) {
                frame_free(fr2);
            }
            tl2->frame = 0;
            bdgt = c->budget;
            bnew = bdgt + 1;
            c->budget = bnew;
        }
        uu = iter_func_of(c, ttidx);
        tl2 = &c->lvls[uu];
        t_fr = tl2->frame;
        t_inst = 0;
        if (t_fr != 0) {
            t_inst = t_fr->inst;
        }
        ts = 0;
        if (t_inst != 0) {
            ts = t_inst->store;
        }
        tgl = &c->lvls[ti];
        dbase = tgl->base;
        code->vsn = dbase;
        if (ack_kind == 0) {
            e = vs_append(code, pp, pn);
            free(pp);
            pp = 0;
            if (e != W89_ERR_NONE) {
                c->crash = "out of memory";
                goto out_crash;
            }
        } else if (ack_kind == 1) {
            if (ts == 0) {
                c->crash = "no store for exception";
                goto out_crash;
            }
            e = vs_append(code, pp, pn);
            if (e != W89_ERR_NONE) {
                c->crash = "out of memory";
                goto out_crash;
            }
            te = w89_exn_alloc(ts, ttg, pp, pn);
            if (te == 0) {
                c->crash = "out of memory";
                goto out_crash;
            }
            pp = 0;
            tr = w89_ref_exn(te);
            tv = w89_value_ref(&tr);
            e = vs_push(code, &tv);
            if (e != W89_ERR_NONE) {
                c->crash = "out of memory";
                goto out_crash;
            }
        } else if (ack_kind == 3) {
            if (ts == 0) {
                c->crash = "no store for exception";
                goto out_crash;
            }
            te = w89_exn_alloc(ts, ttg, pp, pn);
            if (te == 0) {
                c->crash = "out of memory";
                goto out_crash;
            }
            pp = 0;
            tr = w89_ref_exn(te);
            tv = w89_value_ref(&tr);
            e = vs_push(code, &tv);
            if (e != W89_ERR_NONE) {
                c->crash = "out of memory";
                goto out_crash;
            }
        } else {
            free(pp);
            pp = 0;
        }
        tgl = &c->lvls[ti];
        tk = tgl->kind;
        if (tk == W89_LVL_FUNC) {
            cfi = ti;
            goto fn_splice;
        }
        c->ln = ti;
        if (ti > 0) {
            pix = ti - 1;
            par = &c->lvls[pix];
            pend = tgl->end;
            pc2 = pend + 1;
            par->pc = pc2;
        }
        iter_sync_frame(c);
        continue;

block_done:
        if (op == 0x0C) {
            dep = in->idx;
            st = iter_br(c, dep);
            if (st == W89_STEP_CRASH) {
                goto out_crash;
            }
            continue;
        }
        if (op == 0x0D) {
            vn = code->vsn;
            if (vn < 1) {
                c->crash = "stack underflow";
                goto out_crash;
            }
            vn = vn - 1;
            cv = code->vs[vn];
            cn = cv.u.num;
            cond = (w89_u32)cn;
            code->vsn = vn;
            if (cond == 0) {
                pc2 = pc + 1;
                tp->pc = pc2;
                continue;
            }
            dep = in->idx;
            st = iter_br(c, dep);
            if (st == W89_STEP_CRASH) {
                goto out_crash;
            }
            continue;
        }
        if (op == 0x0E) {
            sel = pop_u32(c);
            nlab = in->n;
            if (sel < nlab) {
                dep = in->labels[sel];
            } else {
                dep = in->idx;
            }
            st = iter_br(c, dep);
            if (st == W89_STEP_CRASH) {
                goto out_crash;
            }
            continue;
        }
        if (op == 0x00) {
            c->crash = "unreachable executed";
            goto out_trap;
        }
        st = iter_run_plain(c, in, pc);
        if (st == W89_STEP_CRASH) {
            goto out_crash;
        }
        if (st == W89_STEP_EXHAUSTED) {
            goto out_exhaust;
        }
        h = code_head(code);
        if (h != 0) {
            hn = h->kind;
            if (hn == W89_A_TRAP) {
                msg = h->msg;
                c->crash = msg;
                goto out_trap;
            }
        }
        pc2 = pc + 1;
        tp->pc = pc2;
        continue;

        /* br_on_null 0xD5 / br_on_non_null 0xD6: peek the top reference and
         * conditionally branch to the label depth. A branching br_on_null
         * consumes the (null) reference; a branching br_on_non_null keeps
         * the non-null reference as the branch operand. */
        do_br_on_null:
        vn = code->vsn;
        if (vn < 1) {
            c->crash = "stack underflow";
            goto out_crash;
        }
        vn2 = vn - 1;
        cv = code->vs[vn2];
        isr0 = cv.is_ref;
        if (isr0 != 0) {
            p1 = &cv.u.ref;
            eq = w89_ref_is_null(p1);
            if (eq != 0) {
                code->vsn = vn2;
                dep = in->idx;
                st = iter_br(c, dep);
                if (st == W89_STEP_CRASH) {
                    goto out_crash;
                }
                continue;
            }
        }
        pc2 = pc + 1;
        tp->pc = pc2;
        continue;

        do_br_on_non_null:
        vn = code->vsn;
        if (vn < 1) {
            c->crash = "stack underflow";
            goto out_crash;
        }
        vn2 = vn - 1;
        cv = code->vs[vn2];
        isr0 = cv.is_ref;
        if (isr0 != 0) {
            p1 = &cv.u.ref;
            eq = w89_ref_is_null(p1);
            if (eq == 0) {
                /* Non-null: branch, keeping the reference on top as the last
                 * branch operand (below it sit any further operands). */
                dep = in->idx;
                st = iter_br(c, dep);
                if (st == W89_STEP_CRASH) {
                    goto out_crash;
                }
                continue;
            }
        }
        /* Null (or non-ref): br_on_non_null drops the reference and falls
         * through (matching legacy). */
        code->vsn = vn2;
        pc2 = pc + 1;
        tp->pc = pc2;
        continue;

do_tail:
        w89_func_arity(cf, &n1c, &n2c);
        cvs = code->vsn;
        if (cvs < n1c) {
            c->crash = "stack underflow";
            goto out_crash;
        }
        cbase = cvs - n1c;
        is_host = cf->is_host;
        /* The tail call returns from the enclosing function frame; any blocks
         * above that frame are discarded (never resumed). */
        cfi = iter_func_of(c, ln - 1);
        nl = cfi + 1;
        c->ln = nl;
        ln = nl;
        if (is_host != 0) {
            code->vsn = cbase;
            nres = 0;
            trap = 0;
            hf = cf->host;
            pvs = &code->vs[cbase];
            hs = hf(pvs, n1c, res, &nres, &trap);
            if (hs == W89_HOST_TRAP) {
                if (trap != 0) {
                    c->crash = trap;
                } else {
                    c->crash = "host function trapped";
                }
                goto out_trap;
            }
            e = vs_append(code, res, nres);
            if (e != W89_ERR_NONE) {
                c->crash = "out of memory";
                goto out_crash;
            }
            /* The enclosing function returns the host's results. */
            tl = &c->lvls[cfi];
            tn = tl->nsrc;
            tl->pc = tn;
            continue;
        }
        cfn = cf->func;
        if (cfn == 0) {
            c->crash = "undefined frame";
            goto out_crash;
        }
        cfinst = cf->inst;
        nlfc = cfn->nlocals;
        nlocs = n1c + nlfc;
        if (cfi == 0) {
            /* The seed frame is caller-owned and must not be freed. */
            frm = c->lvls[0].frame;
            if (frm == 0) {
                tl = &c->lvls[0];
                tn = tl->nsrc;
                tl->pc = tn;
                goto do_call;
            }
            nlocf = frm->nlocals;
            if (nlocf < nlocs) {
                /* Frame too small: charged push; the seed returns after the
                 * callee returns (bounded, never exhausts a tail loop). */
                tl = &c->lvls[0];
                tn = tl->nsrc;
                tl->pc = tn;
                goto do_call;
            }
            nframe = frm;
        } else {
            /* Driver-owned frame: free and reallocate in place. */
            frm = c->lvls[cfi].frame;
            if (frm != 0) {
                frame_free(frm);
            }
            nframe = frame_alloc(cfinst, nlocs);
            if (nframe == 0) {
                c->crash = "out of memory";
                goto out_crash;
            }
        }
        for (j = 0; j < n1c; j = j + 1) {
            idx = cbase + j;
            cv = code->vs[idx];
            nframe->locals[j].v = cv;
            nframe->locals[j].set = 1;
        }
        lt = cfn->locals;
        for (k = 0; k < nlfc; k = k + 1) {
            const w89_vt *ltv;
            w89_value dv;
            w89_u32 ni;
            w89_u32 isref;
            w89_u32 nullable;
            ltv = &lt[k];
            ni = n1c + k;
            isref = ltv->is_ref;
            if (isref != 0) {
                nullable = ltv->rt.nullable;
                if (nullable == 0) {
                    nframe->locals[ni].set = 0;
                } else {
                    dv = w89_default_value(ltv);
                    nframe->locals[ni].v = dv;
                    nframe->locals[ni].set = 1;
                }
            } else {
                dv = w89_default_value(ltv);
                nframe->locals[ni].v = dv;
                nframe->locals[ni].set = 1;
            }
        }
        code->vsn = cbase;
        its = cfn->code.items;
        nn = cfn->code.n;
        tl = &c->lvls[cfi];
        tl->kind = W89_LVL_FUNC;
        tl->finst = cf;
        tl->frame = nframe;
        tl->src = its;
        tl->nsrc = nn;
        tl->pc = 0;
        tl->end = nn;
        tl->exit_arity = n2c;
        tl->base = cbase;
        c->frame = nframe;
        continue;

do_call:
        w89_func_arity(cf, &n1c, &n2c);
        cvs = code->vsn;
        if (cvs < n1c) {
            c->crash = "stack underflow";
            goto out_crash;
        }
        cbase = cvs - n1c;
        is_host = cf->is_host;
        if (is_host != 0) {
            code->vsn = cbase;
            nres = 0;
            trap = 0;
            hf = cf->host;
            pvs = &code->vs[cbase];
            hs = hf(pvs, n1c, res, &nres, &trap);
            if (hs == W89_HOST_TRAP) {
                if (trap != 0) {
                    c->crash = trap;
                } else {
                    c->crash = "host function trapped";
                }
                goto out_trap;
            }
            e = vs_append(code, res, nres);
            if (e != W89_ERR_NONE) {
                c->crash = "out of memory";
                goto out_crash;
            }
            pc2 = pc + 1;
            tp->pc = pc2;
            continue;
        }
        bdgt = c->budget;
        if (bdgt == 0) {
            goto out_exhaust;
        }
        cfn = cf->func;
        if (cfn == 0) {
            c->crash = "undefined frame";
            goto out_crash;
        }
        cfinst = cf->inst;
        nlfc = cfn->nlocals;
        nlocs = n1c + nlfc;
        nframe = frame_alloc(cfinst, nlocs);
        if (nframe == 0) {
            c->crash = "out of memory";
            goto out_crash;
        }
        for (j = 0; j < n1c; j = j + 1) {
            idx = cbase + j;
            cv = code->vs[idx];
            frm = nframe;
            frm->locals[j].v = cv;
            frm->locals[j].set = 1;
        }
        lt = cfn->locals;
        for (k = 0; k < nlfc; k = k + 1) {
            const w89_vt *ltv;
            w89_value dv;
            w89_u32 ni;
            w89_u32 isref;
            w89_u32 nullable;
            ltv = &lt[k];
            ni = n1c + k;
            isref = ltv->is_ref;
            if (isref != 0) {
                nullable = ltv->rt.nullable;
                if (nullable == 0) {
                    nframe->locals[ni].set = 0;
                } else {
                    dv = w89_default_value(ltv);
                    nframe->locals[ni].v = dv;
                    nframe->locals[ni].set = 1;
                }
            } else {
                dv = w89_default_value(ltv);
                nframe->locals[ni].v = dv;
                nframe->locals[ni].set = 1;
            }
        }
        code->vsn = cbase;
        its = cfn->code.items;
        nn = cfn->code.n;
        memset(&lv, 0, sizeof(w89_lvl));
        lv.kind = W89_LVL_FUNC;
        lv.finst = cf;
        lv.frame = nframe;
        lv.src = its;
        lv.nsrc = nn;
        lv.pc = 0;
        lv.end = nn;
        lv.exit_arity = n2c;
        lv.base = cbase;
        e = w89_lvl_push(c, &lv);
        if (e != W89_ERR_NONE) {
            frame_free(nframe);
            c->crash = "out of memory";
            goto out_crash;
        }
        c->frame = nframe;
        bdgt = c->budget;
        bnew = bdgt - 1;
        c->budget = bnew;
        continue;

fn_splice:
        tp = &c->lvls[cfi];
        fbase = tp->base;
        far = tp->exit_arity;
        cvs = code->vsn;
        if (cvs < fbase) {
            f_avail = 0;
        } else {
            f_avail = cvs - fbase;
        }
        if (far == 0) {
            f_take = f_avail;
        } else {
            if (f_avail < far) {
                f_take = f_avail;
            } else {
                f_take = far;
            }
            /* A function return keeps the top results, discarding any stray
             * values left below them by an uncompleted enclosing value-typed
             * block (e.g. a non-taken br_if value operand). Move the top
             * f_take values down to the function base (S2.6). */
            if (f_take > 0) {
                idx2 = cvs - f_take;
                fdst = &code->vs[fbase];
                fsrc = &code->vs[idx2];
                fmv_t = (size_t)f_take;
                fmv_n = fmv_t * sizeof(w89_value);
                memmove(fdst, fsrc, fmv_n);
            }
        }
        cvs = fbase + f_take;
        code->vsn = cvs;
        if (cfi == 0) {
            c->ln = 0;
            goto out_done;
        }
        frm = tp->frame;
        if (frm != 0) {
            frame_free(frm);
        }
        c->ln = cfi;
        nl = cfi;
        pi = nl - 1;
        par = &c->lvls[pi];
        pc2 = par->pc;
        pc2 = pc2 + 1;
        par->pc = pc2;
        bdgt = c->budget;
        bnew = bdgt + 1;
        c->budget = bnew;
        iter_sync_frame(c);
        continue;
    }

out_done:
    out.status = W89_EVAL_OK;
    vp = code->vs;
    out.vs = vp;
    vn = code->vsn;
    out.nvs = vn;
    code->vs = 0;
    code->vsn = 0;
    return out;

out_trap:
    iter_free_frames(c);
    out.status = W89_EVAL_TRAP;
    msg = c->crash;
    out.msg = msg;
    vp = code->vs;
    free(vp);
    code->vs = 0;
    code->vsn = 0;
    return out;

out_exhaust:
    iter_free_frames(c);
    out.status = W89_EVAL_EXHAUSTED;
    out.msg = "call stack exhausted";
    vp = code->vs;
    free(vp);
    code->vs = 0;
    code->vsn = 0;
    return out;

out_exception:
    iter_free_frames(c);
    out.status = W89_EVAL_EXCEPTION;
    out.tag = ttg;
    out.vs = pp;
    out.nvs = pn;
    pp = 0;
    vp = code->vs;
    free(vp);
    code->vs = 0;
    code->vsn = 0;
    return out;

out_crash:
    iter_free_frames(c);
    out.status = W89_EVAL_CRASH;
    msg = c->crash;
    out.msg = msg;
    vp = code->vs;
    free(vp);
    code->vs = 0;
    code->vsn = 0;
    return out;
}

/* ---------- Cross-module type matching ---------- */

static int w89_deftype_canon_eq_x(const w89_typeenv *ae, w89_u32 a,
                                  const w89_typeenv *be, w89_u32 b);
static int typeuse_canon_eq_x(const w89_typeenv *ae, w89_u32 ga,
                              const w89_typeenv *be, w89_u32 gb,
                              w89_u32 x, w89_u32 y);

static int ht_canon_eq_x(const w89_typeenv *ae, const w89_reftype *a,
                         w89_u32 ga, const w89_typeenv *be,
                         const w89_reftype *b, w89_u32 gb)
{
    w89_u32 at;
    w89_u32 bt;
    w89_u32 ta;
    w89_u32 tb;
    w89_absheaptype aa;
    w89_absheaptype ba;
    at = a->is_typeidx;
    bt = b->is_typeidx;
    if (at != bt) {
        return 0;
    }
    if (at != 0) {
        ta = a->typeidx;
        tb = b->typeidx;
        return typeuse_canon_eq_x(ae, ga, be, gb, ta, tb);
    }
    aa = a->abs;
    ba = b->abs;
    return aa == ba;
}

static int vt_canon_eq_x(const w89_typeenv *ae, const w89_vt *a, w89_u32 ga,
                         const w89_typeenv *be, const w89_vt *b, w89_u32 gb)
{
    w89_u32 ar;
    w89_u32 br;
    w89_u32 an;
    w89_u32 bn;
    w89_u32 anull;
    w89_u32 bnull;
    const w89_reftype *p1;
    const w89_reftype *p2;
    ar = a->is_ref;
    br = b->is_ref;
    if (ar != br) {
        return 0;
    }
    if (ar == 0) {
        an = a->num;
        bn = b->num;
        return an == bn;
    }
    anull = a->rt.nullable;
    bnull = b->rt.nullable;
    if (anull != bnull) {
        return 0;
    }
    p1 = &a->rt;
    p2 = &b->rt;
    return ht_canon_eq_x(ae, p1, ga, be, p2, gb);
}

static int field_canon_eq_x(const w89_typeenv *ae, const w89_fieldtype *a,
                            w89_u32 ga, const w89_typeenv *be,
                            const w89_fieldtype *b, w89_u32 gb)
{
    w89_u32 ap;
    w89_u32 bp;
    w89_packed pk;
    w89_packed pkb;
    const w89_vt *p1;
    const w89_vt *p2;
    ap = a->is_packed;
    bp = b->is_packed;
    if (ap != bp) {
        return 0;
    }
    if (ap != 0) {
        pk = a->packed;
        pkb = b->packed;
        return pk == pkb;
    }
    p1 = &a->vt;
    p2 = &b->vt;
    return vt_canon_eq_x(ae, p1, ga, be, p2, gb);
}

static int comptype_canon_eq_x(const w89_typeenv *ae, const w89_subtype *a,
                               w89_u32 ga, const w89_typeenv *be,
                               const w89_subtype *b, w89_u32 gb)
{
    w89_u32 i;
    w89_u32 ka;
    w89_u32 kb;
    w89_u32 np;
    w89_u32 npb;
    w89_u32 nr;
    w89_u32 nrb;
    w89_u32 nf;
    w89_u32 nfb;
    const w89_vt *p1;
    const w89_vt *p2;
    const w89_fieldtype *fp1;
    const w89_fieldtype *fp2;
    w89_u32 eq;
    ka = a->kind;
    kb = b->kind;
    if (ka != kb) {
        return 0;
    }
    if (ka == W89_CK_FUNC) {
        np = a->ft.nparams;
        npb = b->ft.nparams;
        if (np != npb) {
            return 0;
        }
        nr = a->ft.nresults;
        nrb = b->ft.nresults;
        if (nr != nrb) {
            return 0;
        }
        for (i = 0; i < np; i = i + 1) {
            p1 = &a->ft.params[i];
            p2 = &b->ft.params[i];
            eq = vt_canon_eq_x(ae, p1, ga, be, p2, gb);
            if (eq == 0) {
                return 0;
            }
        }
        for (i = 0; i < nr; i = i + 1) {
            p1 = &a->ft.results[i];
            p2 = &b->ft.results[i];
            eq = vt_canon_eq_x(ae, p1, ga, be, p2, gb);
            if (eq == 0) {
                return 0;
            }
        }
        return 1;
    }
    nf = a->nfields;
    nfb = b->nfields;
    if (nf != nfb) {
        return 0;
    }
    for (i = 0; i < nf; i = i + 1) {
        fp1 = &a->fields[i];
        fp2 = &b->fields[i];
        eq = field_canon_eq_x(ae, fp1, ga, be, fp2, gb);
        if (eq == 0) {
            return 0;
        }
    }
    return 1;
}

static int typeuse_canon_eq_x(const w89_typeenv *ae, w89_u32 ga,
                              const w89_typeenv *be, w89_u32 gb,
                              w89_u32 x, w89_u32 y)
{
    int xin;
    int yin;
    w89_u32 nae;
    w89_u32 nbe;
    w89_u32 rx;
    w89_u32 ry;
    w89_u32 rpx;
    w89_u32 rpy;
    nae = ae->ntypes;
    if (x < nae) {
        rx = ae->types[x].recgroup;
        if (rx == ga) {
            xin = 1;
        } else {
            xin = 0;
        }
    } else {
        xin = 0;
    }
    nbe = be->ntypes;
    if (y < nbe) {
        ry = be->types[y].recgroup;
        if (ry == gb) {
            yin = 1;
        } else {
            yin = 0;
        }
    } else {
        yin = 0;
    }
    if (xin != 0) {
        if (yin != 0) {
            rpx = ae->types[x].recpos;
            rpy = be->types[y].recpos;
            return rpx == rpy;
        }
    }
    if (xin != 0) {
        return 0;
    }
    if (yin != 0) {
        return 0;
    }
    return w89_deftype_canon_eq_x(ae, x, be, y);
}

static int recgroup_canon_eq_x(const w89_typeenv *ae, w89_u32 ga,
                               const w89_typeenv *be, w89_u32 gb)
{
    w89_u32 k;
    w89_u32 j;
    w89_u32 na;
    w89_u32 nb;
    w89_u32 fa;
    w89_u32 fb;
    w89_u32 idx;
    const w89_subtype *sa;
    const w89_subtype *sb;
    w89_u32 fin;
    w89_u32 finb;
    w89_u32 nsup;
    w89_u32 nsupb;
    w89_u32 sp;
    w89_u32 sj;
    w89_u32 eq;
    if (ae == be) {
        if (ga == gb) {
            return 1;
        }
    }
    na = ae->recs[ga].nsubs;
    nb = be->recs[gb].nsubs;
    if (na != nb) {
        return 0;
    }
    fa = ae->recs[ga].first;
    fb = be->recs[gb].first;
    for (k = 0; k < na; k = k + 1) {
        idx = fa + k;
        sa = ae->types[idx].sub;
        idx = fb + k;
        sb = be->types[idx].sub;
        fin = sa->is_final;
        finb = sb->is_final;
        if (fin != finb) {
            return 0;
        }
        nsup = sa->nsupers;
        nsupb = sb->nsupers;
        if (nsup != nsupb) {
            return 0;
        }
        for (j = 0; j < nsup; j = j + 1) {
            sp = sa->supertypes[j];
            sj = sb->supertypes[j];
            eq = typeuse_canon_eq_x(ae, ga, be, gb, sp, sj);
            if (eq == 0) {
                return 0;
            }
        }
        eq = comptype_canon_eq_x(ae, sa, ga, be, sb, gb);
        if (eq == 0) {
            return 0;
        }
    }
    return 1;
}

static int w89_deftype_canon_eq_x(const w89_typeenv *ae, w89_u32 a,
                                  const w89_typeenv *be, w89_u32 b)
{
    w89_u32 nae;
    w89_u32 nbe;
    w89_u32 rpa;
    w89_u32 rpb;
    w89_u32 rga;
    w89_u32 rgb;
    nae = ae->ntypes;
    if (a >= nae) {
        return 0;
    }
    nbe = be->ntypes;
    if (b >= nbe) {
        return 0;
    }
    rpa = ae->types[a].recpos;
    rpb = be->types[b].recpos;
    if (rpa != rpb) {
        return 0;
    }
    rga = ae->types[a].recgroup;
    rgb = be->types[b].recgroup;
    return recgroup_canon_eq_x(ae, rga, be, rgb);
}

int w89_match_deftype_x(const w89_typeenv *ae, w89_u32 a,
                        const w89_typeenv *be, w89_u32 b)
{
    w89_u32 j;
    w89_u32 nae;
    w89_u32 nbe;
    w89_u32 eq;
    w89_u32 nsup;
    const w89_subtype *sub;
    w89_u32 sp;
    if (ae == be) {
        return w89_match_deftype(ae, a, b);
    }
    nae = ae->ntypes;
    if (a >= nae) {
        return 0;
    }
    nbe = be->ntypes;
    if (b >= nbe) {
        return 0;
    }
    eq = w89_deftype_canon_eq_x(ae, a, be, b);
    if (eq != 0) {
        return 1;
    }
    sub = ae->types[a].sub;
    nsup = sub->nsupers;
    for (j = 0; j < nsup; j = j + 1) {
        sp = sub->supertypes[j];
        eq = w89_match_deftype_x(ae, sp, be, b);
        if (eq != 0) {
            return 1;
        }
    }
    return 0;
}

static int abstract_lt_type_in(const w89_typeenv *env, w89_absheaptype ha,
                               w89_u32 x)
{
    w89_compkind kind;
    kind = env->types[x].sub->kind;
    switch (ha) {
    case W89_HT_NONE:
        if (kind == W89_CK_STRUCT) {
            return 1;
        }
        if (kind == W89_CK_ARRAY) {
            return 1;
        }
        return 0;
    case W89_HT_NOFUNC:
        return kind == W89_CK_FUNC;
    case W89_HT_NOEXN:
        return 0;
    case W89_HT_NOEXTERN:
        return 0;
    default:
        return 0;
    }
}

static int abstract_lt_abstract(w89_absheaptype ha, w89_absheaptype hb)
{
    if (ha == hb) {
        return 1;
    }
    switch (ha) {
    case W89_HT_EQ:
        return hb == W89_HT_ANY;
    case W89_HT_STRUCT:
        if (hb == W89_HT_ANY) {
            return 1;
        }
        return hb == W89_HT_EQ;
    case W89_HT_ARRAY:
        if (hb == W89_HT_ANY) {
            return 1;
        }
        return hb == W89_HT_EQ;
    case W89_HT_I31:
        if (hb == W89_HT_ANY) {
            return 1;
        }
        return hb == W89_HT_EQ;
    case W89_HT_NONE:
        if (hb == W89_HT_NONE) {
            return 1;
        }
        if (hb == W89_HT_ANY) {
            return 1;
        }
        if (hb == W89_HT_EQ) {
            return 1;
        }
        if (hb == W89_HT_STRUCT) {
            return 1;
        }
        if (hb == W89_HT_ARRAY) {
            return 1;
        }
        if (hb == W89_HT_I31) {
            return 1;
        }
        return 0;
    case W89_HT_NOFUNC:
        return abstract_lt_abstract(hb, W89_HT_FUNC);
    case W89_HT_NOEXN:
        return abstract_lt_abstract(hb, W89_HT_EXN);
    case W89_HT_NOEXTERN:
        return abstract_lt_abstract(hb, W89_HT_EXTERN);
    default:
        return 0;
    }
}

static int kind_matches_abstract(w89_compkind kind, w89_absheaptype hb)
{
    switch (kind) {
    case W89_CK_FUNC:
        return hb == W89_HT_FUNC;
    case W89_CK_STRUCT:
        if (hb == W89_HT_ANY) {
            return 1;
        }
        if (hb == W89_HT_EQ) {
            return 1;
        }
        return hb == W89_HT_STRUCT;
    case W89_CK_ARRAY:
        if (hb == W89_HT_ANY) {
            return 1;
        }
        if (hb == W89_HT_EQ) {
            return 1;
        }
        return hb == W89_HT_ARRAY;
    }
    return 0;
}

static int ht_match_x(const w89_typeenv *ae, const w89_reftype *a,
                      const w89_typeenv *be, const w89_reftype *b)
{
    w89_u32 at;
    w89_u32 bt;
    w89_absheaptype aa;
    w89_absheaptype ba;
    w89_u32 ta;
    w89_u32 tb;
    w89_compkind k;
    const w89_subtype *sub;
    at = a->is_typeidx;
    if (at == 0) {
        aa = a->abs;
        if (aa == 0) {
            return 1;
        }
    }
    if (at != 0) {
        bt = b->is_typeidx;
        if (bt != 0) {
            ta = a->typeidx;
            tb = b->typeidx;
            return w89_match_deftype_x(ae, ta, be, tb);
        }
        ta = a->typeidx;
        sub = ae->types[ta].sub;
        k = sub->kind;
        ba = b->abs;
        return kind_matches_abstract(k, ba);
    }
    bt = b->is_typeidx;
    if (bt != 0) {
        aa = a->abs;
        tb = b->typeidx;
        return abstract_lt_type_in(be, aa, tb);
    }
    aa = a->abs;
    ba = b->abs;
    return abstract_lt_abstract(aa, ba);
}

int w89_match_reftype_x(const w89_typeenv *ae, const w89_reftype *a,
                        const w89_typeenv *be, const w89_reftype *b)
{
    w89_u32 an;
    w89_u32 bn;
    an = a->nullable;
    bn = b->nullable;
    if (an == 1) {
        if (bn == 0) {
            return 0;
        }
    }
    return ht_match_x(ae, a, be, b);
}

static int vt_is_bot(const w89_vt *v)
{
    w89_u32 r;
    w89_u32 num;
    r = v->is_ref;
    if (r != 0) {
        return 0;
    }
    num = v->num;
    if (num != 0) {
        return 0;
    }
    return 1;
}

int w89_match_valtype_x(const w89_typeenv *ae, const w89_vt *a,
                        const w89_typeenv *be, const w89_vt *b)
{
    w89_u32 bot;
    w89_u32 ar;
    w89_u32 br;
    w89_u32 an;
    w89_u32 bn;
    const w89_reftype *p1;
    const w89_reftype *p2;
    bot = vt_is_bot(a);
    if (bot != 0) {
        return 1;
    }
    ar = a->is_ref;
    br = b->is_ref;
    if (ar != br) {
        return 0;
    }
    if (ar == 0) {
        an = a->num;
        bn = b->num;
        return an == bn;
    }
    p1 = &a->rt;
    p2 = &b->rt;
    return w89_match_reftype_x(ae, p1, be, p2);
}

static int match_ft_x(const w89_typeenv *ae, const w89_ft *a,
                      const w89_typeenv *be, const w89_ft *b)
{
    w89_u32 i;
    w89_u32 ap;
    w89_u32 bp;
    w89_u32 ar;
    w89_u32 br;
    const w89_vt *p1;
    const w89_vt *p2;
    w89_u32 eq;
    ap = a->nparams;
    bp = b->nparams;
    if (ap != bp) {
        return 0;
    }
    ar = a->nresults;
    br = b->nresults;
    if (ar != br) {
        return 0;
    }
    for (i = 0; i < ap; i = i + 1) {
        p1 = &b->params[i];
        p2 = &a->params[i];
        eq = w89_match_valtype_x(be, p1, ae, p2);
        if (eq == 0) {
            return 0;
        }
    }
    for (i = 0; i < ar; i = i + 1) {
        p1 = &a->results[i];
        p2 = &b->results[i];
        eq = w89_match_valtype_x(ae, p1, be, p2);
        if (eq == 0) {
            return 0;
        }
    }
    return 1;
}

int w89_match_limits(const w89_limits *a, const w89_limits *b)
{
    w89_u32 aa;
    w89_u32 ba;
    w89_u64 amin;
    w89_u64 bmin;
    w89_u32 bhm;
    w89_u32 ahm;
    w89_u64 amax;
    w89_u64 bmax;
    aa = a->addr64;
    ba = b->addr64;
    if (aa != ba) {
        return 0;
    }
    amin = a->min;
    bmin = b->min;
    if (amin < bmin) {
        return 0;
    }
    bhm = b->has_max;
    if (bhm != 0) {
        ahm = a->has_max;
        if (ahm == 0) {
            return 0;
        }
        amax = a->max;
        bmax = b->max;
        if (amax > bmax) {
            return 0;
        }
    }
    return 1;
}

int w89_match_externtype(const w89_externtype *a, const w89_externtype *b)
{
    w89_externkind ka;
    w89_externkind kb;
    const w89_typeenv *ae2;
    const w89_typeenv *be2;
    w89_u32 ta;
    w89_u32 tb;
    const w89_ft *fa_;
    const w89_ft *fb_;
    w89_u32 m1;
    w89_u32 m2;
    const w89_vt *p1;
    const w89_vt *p2;
    const w89_reftype *r1;
    const w89_reftype *r2;
    const w89_limits *l1;
    const w89_limits *l2;
    w89_u32 ma_;
    w89_u32 mb_;
    ka = a->kind;
    kb = b->kind;
    if (ka != kb) {
        return 0;
    }
    switch (ka) {
    case W89_EXT_FUNC:
        ae2 = a->env;
        be2 = b->env;
        if (ae2 != 0) {
            if (be2 != 0) {
                ta = a->typeidx;
                tb = b->typeidx;
                return w89_match_deftype_x(ae2, ta, be2, tb);
            }
        }
        fa_ = a->ft;
        fb_ = b->ft;
        return match_ft_x(ae2, fa_, be2, fb_);
    case W89_EXT_TAG:
        ae2 = a->env;
        be2 = b->env;
        if (ae2 != 0) {
            if (be2 != 0) {
                ta = a->typeidx;
                tb = b->typeidx;
                m1 = w89_match_deftype_x(ae2, ta, be2, tb);
                if (m1 == 0) {
                    return 0;
                }
                m2 = w89_match_deftype_x(be2, tb, ae2, ta);
                return m2;
            }
        }
        fa_ = a->ft;
        fb_ = b->ft;
        return match_ft_x(ae2, fa_, be2, fb_);
    case W89_EXT_GLOBAL:
        ma_ = a->gt.mut;
        mb_ = b->gt.mut;
        if (ma_ != mb_) {
            return 0;
        }
        ae2 = a->env;
        be2 = b->env;
        p1 = &a->gt.vt;
        p2 = &b->gt.vt;
        if (ma_ != 0) {
            m1 = w89_match_valtype_x(ae2, p1, be2, p2);
            if (m1 == 0) {
                return 0;
            }
            m2 = w89_match_valtype_x(be2, p2, ae2, p1);
            return m2;
        }
        return w89_match_valtype_x(ae2, p1, be2, p2);
    case W89_EXT_TABLE:
        ae2 = a->env;
        be2 = b->env;
        r1 = &a->tt.rt;
        r2 = &b->tt.rt;
        m1 = w89_match_reftype_x(ae2, r1, be2, r2);
        if (m1 == 0) {
            return 0;
        }
        m2 = w89_match_reftype_x(be2, r2, ae2, r1);
        if (m2 == 0) {
            return 0;
        }
        l1 = &a->tt.limits;
        l2 = &b->tt.limits;
        return w89_match_limits(l1, l2);
    case W89_EXT_MEMORY:
        l1 = &a->mem;
        l2 = &b->mem;
        return w89_match_limits(l1, l2);
    }
    return 0;
}

/* S2.6: iterative invocation of one wasm function; the sole path taken by
 * w89_invoke for non-host functions (design 0007). Params are copied into a
 * fresh heap frame and the body is driven by w89_eval_iter; the frame is
 * caller-owned (never freed by the driver). */
static w89_eval_out iter_invoke_driver(w89_funcinst *f, const w89_value *args,
                                       w89_u32 n1, w89_u32 n2)
{
    w89_eval_out out;
    w89_config cfg;
    const w89_func *fn;
    const w89_vt *lt;
    const w89_vt *ltv;
    const w89_instr *its;
    w89_frame *frame;
    w89_moduleinst *inst;
    w89_code *code;
    w89_value av;
    w89_value dv;
    w89_u32 nlfc;
    w89_u32 nlocs;
    w89_u32 i;
    w89_u32 ni;
    w89_u32 nn;
    w89_u32 isref;
    w89_u32 nullable;
    memset(&out, 0, sizeof(w89_eval_out));
    fn = f->func;
    inst = f->inst;
    nlfc = fn->nlocals;
    nlocs = n1 + nlfc;
    frame = frame_alloc(inst, nlocs);
    if (frame == 0) {
        out.status = W89_EVAL_CRASH;
        out.msg = "out of memory";
        return out;
    }
    for (i = 0; i < n1; i = i + 1) {
        av = args[i];
        frame->locals[i].v = av;
        frame->locals[i].set = 1;
    }
    lt = fn->locals;
    for (i = 0; i < nlfc; i = i + 1) {
        ltv = &lt[i];
        ni = n1 + i;
        isref = ltv->is_ref;
        if (isref != 0) {
            nullable = ltv->rt.nullable;
            if (nullable == 0) {
                frame->locals[ni].set = 0;
            } else {
                dv = w89_default_value(ltv);
                frame->locals[ni].v = dv;
                frame->locals[ni].set = 1;
            }
        } else {
            dv = w89_default_value(ltv);
            frame->locals[ni].v = dv;
            frame->locals[ni].set = 1;
        }
    }
    w89_config_init(&cfg, frame);
    cfg.resn = n2;
    code = &cfg.code;
    its = fn->code.items;
    nn = fn->code.n;
    code->src = its;
    code->nsrc = nn;
    out = w89_eval_iter(&cfg);
    frame_free(frame);
    w89_config_free(&cfg);
    return out;
}

/* S2.7: invoke a host function directly (single flat step, no wasm frames or
 * control flow, so no C recursion). Replaces the legacy stepper drive used
 * for host invocation. */
static w89_eval_out invoke_host_direct(w89_funcinst *f, const w89_value *args,
                                       w89_u32 n1, w89_u32 n2)
{
    w89_eval_out out;
    w89_host_status hs;
    w89_hostfn hf;
    w89_value res[16];
    w89_u32 nres;
    const char *trap;
    const char *msg;
    w89_u32 i;
    w89_u32 cap;
    w89_u32 arity;
    w89_value *dst;
    size_t nb;
    size_t sz;
    memset(&out, 0, sizeof(w89_eval_out));
    nres = 0;
    trap = 0;
    hf = f->host;
    hs = hf(args, n1, res, &nres, &trap);
    if (hs == W89_HOST_TRAP) {
        out.status = W89_EVAL_TRAP;
        msg = trap;
        if (msg == 0) {
            msg = "host function trapped";
        }
        out.msg = msg;
        return out;
    }
    cap = 16;
    if (nres > cap) {
        nres = cap;
    }
    arity = n2;
    if (arity > 0) {
        if (nres > arity) {
            nres = arity;
        }
    }
    out.status = W89_EVAL_OK;
    out.nvs = nres;
    if (nres > 0) {
        nb = sizeof(w89_value);
        sz = (size_t)nres;
        dst = malloc(sz * nb);
        out.vs = dst;
        if (dst == 0) {
            out.status = W89_EVAL_CRASH;
            out.msg = "out of memory";
            return out;
        }
        for (i = 0; i < nres; i = i + 1) {
            w89_value rv;
            rv = res[i];
            dst[i] = rv;
        }
    }
    return out;
}

w89_eval_out w89_invoke(w89_store *s, w89_funcinst *f, const w89_value *args,
                        w89_u32 nargs)
{
    w89_eval_out out;
    w89_u32 n1;
    w89_u32 n2;
    int is_host;
    (void)s;
    memset(&out, 0, sizeof(w89_eval_out));
    w89_func_arity(f, &n1, &n2);
    if (nargs != n1) {
        out.status = W89_EVAL_CRASH;
        out.msg = "wrong number of arguments";
        return out;
    }
    is_host = f->is_host;
    if (is_host == 0) {
        return iter_invoke_driver(f, args, n1, n2);
    }
    out = invoke_host_direct(f, args, n1, n2);
    return out;
}
