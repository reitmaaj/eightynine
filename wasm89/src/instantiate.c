#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eval.h"

#define W89_PAGE_SIZE_B 65536u

static char w89_inst_msg[512];

const char *w89_instantiate_message(void)
{
    return w89_inst_msg;
}

static w89_err grow_vec(void **pp, w89_u32 *cap, w89_u32 need, w89_u32 size)
{
    w89_u32 cur;
    w89_u32 ncap;
    void *p;
    size_t sz;

    cur = *cap;
    if (need <= cur) {
        return W89_ERR_NONE;
    }
    if (cur == 0) {
        ncap = 8;
    } else {
        ncap = cur * 2;
    }
    while (ncap < need) {
        ncap = ncap * 2;
    }
    p = *pp;
    sz = (size_t)ncap;
    sz = sz * size;
    p = realloc(p, sz);
    if (p == 0) {
        return W89_ERR_OUT_OF_MEMORY;
    }
    *pp = p;
    *cap = ncap;
    return W89_ERR_NONE;
}

static w89_err vec_add(void **pp, w89_u32 *n, w89_u32 *cap, const void *item,
                       w89_u32 size)
{
    w89_err e;
    w89_u32 nn;
    void *pv;
    w89_byte *dst;
    size_t off;

    nn = *n;
    e = grow_vec(pp, cap, nn + 1, size);
    if (e != W89_ERR_NONE) {
        return e;
    }
    nn = *n;
    off = (size_t)nn;
    off = off * size;
    pv = *pp;
    dst = (w89_byte *)pv;
    dst = dst + off;
    memcpy(dst, item, size);
    nn = *n;
    nn = nn + 1;
    *n = nn;
    return W89_ERR_NONE;
}

/* ---------- Store ---------- */

void w89_store_init(w89_store *s)
{
    size_t sz;

    sz = sizeof(w89_store);
    memset(s, 0, sz);
}

void w89_store_free(w89_store *s)
{
    w89_u32 i;
    w89_u32 n;
    w89_funcinst *f;
    w89_globalinst *g;
    w89_meminst *m;
    w89_tableinst *t;
    w89_taginst *tag;
    w89_exn *e;
    w89_datainst *d;
    w89_eleminst *el;
    w89_typeenv *te;
    w89_moduleinst *mi;
    w89_module *mod;
    void *b;
    size_t sz;

    sz = sizeof(w89_store);
    n = s->nfuncs;
    for (i = 0; i < n; i = i + 1) {
        f = s->funcs[i];
        free(f);
    }
    n = s->nglobals;
    for (i = 0; i < n; i = i + 1) {
        g = s->globals[i];
        free(g);
    }
    n = s->nmems;
    for (i = 0; i < n; i = i + 1) {
        m = s->mems[i];
        b = m->bytes;
        free(b);
        free(m);
    }
    n = s->ntables;
    for (i = 0; i < n; i = i + 1) {
        t = s->tables[i];
        b = t->elems;
        free(b);
        free(t);
    }
    n = s->ntags;
    for (i = 0; i < n; i = i + 1) {
        tag = s->tags[i];
        free(tag);
    }
    n = s->nexns;
    for (i = 0; i < n; i = i + 1) {
        e = s->exns[i];
        b = e->args;
        free(b);
        free(e);
    }
    n = s->ndatas;
    for (i = 0; i < n; i = i + 1) {
        d = s->datas[i];
        free(d);
    }
    n = s->nelems;
    for (i = 0; i < n; i = i + 1) {
        el = s->elems[i];
        b = el->refs;
        free(b);
        free(el);
    }
    n = s->ntenvs;
    for (i = 0; i < n; i = i + 1) {
        te = s->tenvs[i];
        w89_typeenv_free(te);
        free(te);
    }
    n = s->nmods;
    for (i = 0; i < n; i = i + 1) {
        mi = s->mods[i];
        b = mi->funcs;
        free(b);
        b = mi->globals;
        free(b);
        b = mi->tables;
        free(b);
        b = mi->memories;
        free(b);
        b = mi->tags;
        free(b);
        b = mi->datas;
        free(b);
        b = mi->elems;
        free(b);
        b = mi->exports;
        free(b);
        free(mi);
    }
    n = s->ndecoded;
    for (i = 0; i < n; i = i + 1) {
        mod = s->decoded[i];
        w89_module_free(mod);
        free(mod);
    }
    n = s->nbufs;
    for (i = 0; i < n; i = i + 1) {
        b = s->decbufs[i];
        free(b);
    }
    b = s->tenvs;
    free(b);
    b = s->funcs;
    free(b);
    b = s->globals;
    free(b);
    b = s->mems;
    free(b);
    b = s->tables;
    free(b);
    b = s->tags;
    free(b);
    b = s->exns;
    free(b);
    b = s->datas;
    free(b);
    b = s->elems;
    free(b);
    b = s->mods;
    free(b);
    b = s->decoded;
    free(b);
    b = s->decbufs;
    free(b);
    memset(s, 0, sz);
}

w89_err w89_store_own_module(w89_store *s, w89_module *m, w89_byte *buf)
{
    w89_err e;
    void *pa;
    w89_u32 *pn;
    w89_u32 *pc;
    w89_u32 size;

    size = sizeof(w89_module *);
    pa = &s->decoded;
    pn = &s->ndecoded;
    pc = &s->cdecoded;
    e = vec_add(pa, pn, pc, &m, size);
    if (e != W89_ERR_NONE) {
        return e;
    }
    pa = &s->decbufs;
    pn = &s->nbufs;
    pc = &s->cbufs;
    return vec_add(pa, pn, pc, &buf, size);
}

static w89_err store_add_tenv(w89_store *s, w89_typeenv **out)
{
    w89_err e;
    w89_typeenv *t;
    void *pa;
    w89_u32 *pc;
    w89_u32 n;
    w89_u32 size;
    w89_u32 ntenvs;
    w89_u32 tsize;

    pa = &s->tenvs;
    pc = &s->ctenvs;
    n = s->ntenvs;
    size = sizeof(w89_typeenv *);
    e = grow_vec(pa, pc, n + 1, size);
    if (e != W89_ERR_NONE) {
        return e;
    }
    tsize = sizeof(w89_typeenv);
    t = malloc(tsize);
    if (t == 0) {
        return W89_ERR_OUT_OF_MEMORY;
    }
    memset(t, 0, tsize);
    ntenvs = s->ntenvs;
    s->tenvs[ntenvs] = t;
    ntenvs = ntenvs + 1;
    s->ntenvs = ntenvs;
    *out = t;
    return W89_ERR_NONE;
}

/* ---------- Registry ---------- */

void w89_registry_init(w89_registry *r)
{
    size_t sz;

    sz = sizeof(w89_registry);
    memset(r, 0, sz);
}

void w89_registry_free(w89_registry *r)
{
    w89_u32 i;
    w89_u32 n;
    char *nm;
    void *p;
    size_t sz;

    sz = sizeof(w89_registry);
    n = r->n;
    for (i = 0; i < n; i = i + 1) {
        nm = r->items[i].name;
        free(nm);
    }
    p = r->items;
    free(p);
    memset(r, 0, sz);
}

w89_err w89_register(w89_registry *r, const char *name, w89_moduleinst *inst)
{
    w89_u32 i;
    w89_u32 n;
    w89_err e;
    char *dup;
    char *iname;
    int cmp;
    void *pa;
    w89_u32 *pc;
    w89_u32 itemsz;
    size_t sl;

    n = r->n;
    for (i = 0; i < n; i = i + 1) {
        iname = r->items[i].name;
        cmp = strcmp(iname, name);
        if (cmp == 0) {
            r->items[i].inst = inst;
            return W89_ERR_NONE;
        }
    }
    pa = &r->items;
    pc = &r->cap;
    n = r->n;
    itemsz = sizeof(w89_regitem);
    e = grow_vec(pa, pc, n + 1, itemsz);
    if (e != W89_ERR_NONE) {
        return e;
    }
    sl = strlen(name);
    dup = malloc(sl + 1);
    if (dup == 0) {
        return W89_ERR_OUT_OF_MEMORY;
    }
    strcpy(dup, name);
    n = r->n;
    r->items[n].name = dup;
    r->items[n].inst = inst;
    n = n + 1;
    r->n = n;
    return W89_ERR_NONE;
}

w89_moduleinst *w89_registry_find(const w89_registry *r, const char *name,
                                  w89_u32 len)
{
    w89_u32 i;
    w89_u32 n;
    char *iname;
    size_t sl;
    size_t l2;
    int cmp;
    w89_moduleinst *mi;

    n = r->n;
    for (i = 0; i < n; i = i + 1) {
        iname = r->items[i].name;
        sl = strlen(iname);
        l2 = (size_t)len;
        if (sl == l2) {
            cmp = strncmp(iname, name, len);
            if (cmp == 0) {
                mi = r->items[i].inst;
                return mi;
            }
        }
    }
    return 0;
}

/* ---------- Const-expression evaluation ---------- */

/* S2.7: evaluate one flat const/element expression (a contiguous slice of a
 * decoded instruction vector) on the iterative driver. The driver seeds its
 * outer FUNC level over code->src[0..nsrc), so the slice is handed over as
 * the exact source range [items+start, items+start+len) rather than the
 * whole enclosing vector. Legacy w89_eval consumed a per-slice admin queue
 * built by w89_code_range; the iterative driver does not, so giving it the
 * whole concatenated vector would re-run slot 0 for every slot (the bug this
 * fixes). Expressions are flat (no control flow/calls/frames), so a single
 * top value is the result. */
static w89_err eval_expr_driver(const w89_moduleinst *inst,
                                const w89_instr *items, w89_u32 start,
                                w89_u32 len, w89_value *out)
{
    w89_config cfg;
    w89_frame frame;
    w89_eval_out eo;
    w89_eval_status st;
    w89_u32 nvs;
    w89_value v0;
    w89_moduleinst *finst;
    w89_code *cp;
    size_t fsz;
    const w89_instr *slice;
    w89_u32 slen;

    fsz = sizeof(w89_frame);
    memset(&frame, 0, fsz);
    finst = (w89_moduleinst *)inst;
    frame.inst = finst;
    w89_config_init(&cfg, &frame);
    cp = &cfg.code;
    slice = items + start;
    slen = len;
    cp->src = slice;
    cp->nsrc = slen;
    eo = w89_eval_iter(&cfg);
    w89_config_free(&cfg);
    st = eo.status;
    if (st != W89_EVAL_OK) {
        w89_eval_out_free(&eo);
        return W89_ERR_INVALID;
    }
    nvs = eo.nvs;
    if (nvs != 1) {
        w89_eval_out_free(&eo);
        return W89_ERR_INVALID;
    }
    v0 = eo.vs[0];
    *out = v0;
    w89_eval_out_free(&eo);
    return W89_ERR_NONE;
}

static w89_err eval_const(const w89_moduleinst *inst, const w89_instr_vec *v,
                          w89_value *out)
{
    const w89_instr *items;
    w89_u32 n;
    items = v->items;
    n = v->n;
    return eval_expr_driver(inst, items, 0, n, out);
}

/* ---------- Export lookup ---------- */

int w89_find_export(w89_moduleinst *inst, const w89_name *name,
                    w89_externinst *out)
{
    w89_u32 i;
    w89_u32 n;
    w89_expinst *ex;
    w89_name *exn;
    const char *nb;
    const w89_byte *mbp;
    w89_u32 nl;
    int eq;
    size_t osz;
    w89_externkind k;
    w89_funcinst *ef;
    w89_globalinst *eg;
    w89_meminst *em;
    w89_tableinst *et;
    w89_taginst *etag;

    osz = sizeof(w89_externinst);
    n = inst->nexports;
    for (i = 0; i < n; i = i + 1) {
        ex = &inst->exports[i];
        exn = &ex->name;
        mbp = name->bytes;
        nb = (const char *)mbp;
        nl = name->len;
        eq = w89_name_eq(exn, nb, nl);
        if (eq != 0) {
            memset(out, 0, osz);
            k = ex->kind;
            out->kind = k;
            ef = ex->u.func;
            out->u.func = ef;
            eg = ex->u.global;
            out->u.global = eg;
            em = ex->u.memory;
            out->u.memory = em;
            et = ex->u.table;
            out->u.table = et;
            etag = ex->u.tag;
            out->u.tag = etag;
            return 1;
        }
    }
    return 0;
}

/* ---------- Extern types ---------- */

static w89_externkind wasm_kind(w89_u32 k)
{
    switch (k) {
    case 0: return W89_EXT_FUNC;
    case 1: return W89_EXT_TABLE;
    case 2: return W89_EXT_MEMORY;
    case 3: return W89_EXT_GLOBAL;
    case 4: return W89_EXT_TAG;
    }
    return W89_EXT_FUNC;
}

static const w89_ft *func_ft(const w89_funcinst *f, const w89_typeenv **env)
{
    w89_u32 is_host;
    const w89_ft *ft;
    const w89_typeenv *e;
    w89_u32 typeidx;

    is_host = f->is_host;
    if (is_host != 0) {
        *env = 0;
        ft = f->ft;
        return ft;
    }
    e = f->inst->types;
    *env = e;
    typeidx = f->typeidx;
    ft = &e->types[typeidx].sub->ft;
    return ft;
}

static void externtype_of_inst(const w89_externinst *ext, w89_externtype *xt)
{
    w89_externkind k;
    w89_u32 typeidx;
    const w89_typeenv *env;
    const w89_typeenv **envp;
    const w89_ft *ft;
    w89_globaltype gt;
    w89_limits lm;
    w89_tabletype tt;
    w89_funcinst *f;
    w89_globalinst *g;
    w89_meminst *m;
    w89_tableinst *t;
    w89_taginst *tag;
    size_t xtsz;

    xtsz = sizeof(w89_externtype);
    memset(xt, 0, xtsz);
    k = ext->kind;
    xt->kind = k;
    switch (k) {
    case W89_EXT_FUNC:
        f = ext->u.func;
        envp = &xt->env;
        ft = func_ft(f, envp);
        xt->ft = ft;
        env = xt->env;
        if (env != 0) {
            typeidx = f->typeidx;
            xt->typeidx = typeidx;
        }
        break;
    case W89_EXT_GLOBAL:
        g = ext->u.global;
        gt = g->type;
        xt->gt = gt;
        env = g->types;
        xt->env = env;
        break;
    case W89_EXT_MEMORY:
        m = ext->u.memory;
        lm = m->limits;
        xt->mem = lm;
        break;
    case W89_EXT_TABLE:
        t = ext->u.table;
        tt = t->type;
        xt->tt = tt;
        env = t->types;
        xt->env = env;
        break;
    case W89_EXT_TAG:
        tag = ext->u.tag;
        ft = tag->ft;
        xt->ft = ft;
        env = tag->env;
        xt->env = env;
        typeidx = tag->typeidx;
        xt->typeidx = typeidx;
        break;
    }
}

static void externtype_of_import(const w89_moduleinst *inst,
                                 const w89_import *im, w89_externtype *xt)
{
    w89_u32 k;
    w89_u32 typeidx;
    const w89_typeenv *env;
    const w89_ft *ft;
    w89_globaltype gt;
    w89_limits lm;
    w89_tabletype tt;
    w89_externkind kind;
    size_t xtsz;

    xtsz = sizeof(w89_externtype);
    memset(xt, 0, xtsz);
    env = inst->types;
    xt->env = env;
    k = im->kind;
    kind = wasm_kind(k);
    xt->kind = kind;
    switch (kind) {
    case W89_EXT_FUNC:
    case W89_EXT_TAG:
        typeidx = im->typeidx;
        ft = &env->types[typeidx].sub->ft;
        xt->ft = ft;
        xt->typeidx = typeidx;
        break;
    case W89_EXT_GLOBAL:
        gt = im->global;
        xt->gt = gt;
        break;
    case W89_EXT_MEMORY:
        lm = im->mem;
        xt->mem = lm;
        break;
    case W89_EXT_TABLE:
        tt = im->table;
        xt->tt = tt;
        break;
    }
}

/* ---------- Instantiation ---------- */

static w89_err inst_append(w89_moduleinst *inst, const w89_externinst *ext)
{
    w89_err e;
    w89_externkind k;
    void *pa;
    w89_u32 *pn;
    w89_u32 *pc;
    w89_u32 size;
    void *item;

    k = ext->kind;
    switch (k) {
    case W89_EXT_FUNC:
        pa = &inst->funcs;
        pn = &inst->nfuncs;
        pc = &inst->cfuncs;
        size = sizeof(w89_funcinst *);
        item = ext->u.func;
        e = vec_add(pa, pn, pc, &item, size);
        break;
    case W89_EXT_GLOBAL:
        pa = &inst->globals;
        pn = &inst->nglobals;
        pc = &inst->cglobals;
        size = sizeof(w89_globalinst *);
        item = ext->u.global;
        e = vec_add(pa, pn, pc, &item, size);
        break;
    case W89_EXT_MEMORY:
        pa = &inst->memories;
        pn = &inst->nmemories;
        pc = &inst->cmemories;
        size = sizeof(w89_meminst *);
        item = ext->u.memory;
        e = vec_add(pa, pn, pc, &item, size);
        break;
    case W89_EXT_TABLE:
        pa = &inst->tables;
        pn = &inst->ntables;
        pc = &inst->ctables;
        size = sizeof(w89_tableinst *);
        item = ext->u.table;
        e = vec_add(pa, pn, pc, &item, size);
        break;
    case W89_EXT_TAG:
        pa = &inst->tags;
        pn = &inst->ntags;
        pc = &inst->ctags;
        size = sizeof(w89_taginst *);
        item = ext->u.tag;
        e = vec_add(pa, pn, pc, &item, size);
        break;
    default:
        e = W89_ERR_INVALID;
        break;
    }
    return e;
}

static w89_u64 mem_bytes(const w89_meminst *m)
{
    w89_u64 np;
    w89_u64 nb;

    np = m->npages;
    nb = np * W89_PAGE_SIZE_B;
    return nb;
}

static w89_err run_data_segment(w89_store *s, w89_moduleinst *inst,
                                const w89_data *d, w89_u32 i)
{
    w89_value off;
    w89_meminst *m;
    w89_datainst *di;
    w89_err e;
    w89_u32 flags;
    w89_u32 memidx;
    w89_u32 nmems;
    w89_u64 onum;
    w89_u64 di_len;
    w89_u64 mbytes;
    w89_u64 total;
    w89_byte *mb;
    const w89_byte *db;
    size_t dl;
    unsigned u;
    const w89_instr_vec *offp;
    w89_u32 di_idx;

    (void)s;
    flags = d->flags;
    if (flags == 1) {
        return W89_ERR_NONE;
    }
    offp = &d->offset;
    e = eval_const(inst, offp, &off);
    if (e != W89_ERR_NONE) {
        return e;
    }
    memidx = d->memidx;
    nmems = inst->nmemories;
    if (memidx >= nmems) {
        u = (unsigned)memidx;
        sprintf(w89_inst_msg, "unknown memory %u", u);
        return W89_ERR_INVALID;
    }
    m = inst->memories[memidx];
    di_idx = i;
    di = inst->datas[di_idx];
    onum = off.u.num;
    di_len = di->len;
    mbytes = mem_bytes(m);
    total = onum + di_len;
    if (total > mbytes) {
        sprintf(w89_inst_msg, "out of bounds memory access");
        return W89_ERR_INVALID;
    }
    if (di_len > 0) {
        mb = m->bytes;
        onum = off.u.num;
        mb = mb + onum;
        db = di->bytes;
        dl = (size_t)di_len;
        memcpy(mb, db, dl);
    }
    di->len = 0;
    return W89_ERR_NONE;
}

static w89_err eval_elem_exprs(const w89_moduleinst *inst, const w89_elem *el,
                               w89_ref *refs)
{
    w89_u32 k;
    w89_u32 pos;
    w89_u32 en;
    w89_u32 en_items;
    const w89_instr *items;
    w89_value v;
    w89_err e;
    w89_u32 start;
    w89_u32 op;
    w89_u32 len;
    w89_ref rr;
    w89_u32 is_ref;

    pos = 0;
    en = el->n;
    en_items = el->exprs.n;
    items = el->exprs.items;
    for (k = 0; k < en; k = k + 1) {
        start = pos;
        while (pos < en_items) {
            op = items[pos].op;
            if (op == 0x0B) {
                break;
            }
            pos = pos + 1;
        }
        if (pos >= en_items) {
            return W89_ERR_INVALID;
        }
        len = pos - start;
        e = eval_expr_driver(inst, items, start, len, &v);
        if (e != W89_ERR_NONE) {
            return e;
        }
        is_ref = v.is_ref;
        if (is_ref == 0) {
            return W89_ERR_INVALID;
        }
        rr = v.u.ref;
        refs[k] = rr;
        pos = pos + 1;
    }
    return W89_ERR_NONE;
}

static w89_err run_elem_segment(w89_store *s, w89_moduleinst *inst,
                                const w89_elem *el, w89_u32 i)
{
    w89_value off;
    w89_tableinst *tab;
    w89_eleminst *ei;
    w89_err e;
    w89_u32 flags;
    w89_u32 tableidx;
    w89_u32 ntables;
    w89_u64 onum;
    w89_u64 ei_n;
    w89_u64 tsize;
    w89_u64 total;
    w89_ref *tr;
    const w89_ref *rr;
    size_t n;
    unsigned u;
    const w89_instr_vec *offp;
    w89_u32 ei_idx;

    (void)s;
    flags = el->flags;
    if (flags == 1) {
        return W89_ERR_NONE;
    }
    if (flags == 3) {
        ei = inst->elems[i];
        ei->n = 0;
        return W89_ERR_NONE;
    }
    if (flags == 5) {
        return W89_ERR_NONE;
    }
    if (flags == 7) {
        ei = inst->elems[i];
        ei->n = 0;
        return W89_ERR_NONE;
    }
    offp = &el->offset;
    e = eval_const(inst, offp, &off);
    if (e != W89_ERR_NONE) {
        return e;
    }
    tableidx = el->tableidx;
    ntables = inst->ntables;
    if (tableidx >= ntables) {
        u = (unsigned)tableidx;
        sprintf(w89_inst_msg, "unknown table %u", u);
        return W89_ERR_INVALID;
    }
    tab = inst->tables[tableidx];
    ei_idx = i;
    ei = inst->elems[ei_idx];
    onum = off.u.num;
    ei_n = ei->n;
    tsize = tab->size;
    total = onum + ei_n;
    if (total > tsize) {
        sprintf(w89_inst_msg, "out of bounds table access");
        return W89_ERR_INVALID;
    }
    if (ei_n > 0) {
        tr = tab->elems;
        onum = off.u.num;
        tr = tr + onum;
        rr = ei->refs;
        n = (size_t)ei_n;
        n = n * sizeof(w89_ref);
        memcpy(tr, rr, n);
    }
    ei->n = 0;
    return W89_ERR_NONE;
}

w89_err w89_instantiate(w89_store *s, const w89_module *m,
                        const w89_registry *reg, w89_moduleinst **out)
{
    w89_moduleinst *inst;
    w89_typeenv env;
    w89_err e;
    w89_u32 i;
    w89_u32 k;
    w89_typeenv *tenv;
    w89_u32 n;
    w89_u32 typeidx;
    w89_u32 instsize;
    w89_u32 msize;
    w89_u32 fsize;
    w89_u32 gsize;
    w89_u32 tsize;
    w89_u32 dsize;
    w89_u32 elsize;
    w89_u32 psize;
    w89_u32 rsize;
    w89_taginst *t;
    w89_funcinst *f;
    w89_globalinst *g;
    w89_meminst *me;
    w89_tableinst *ta;
    w89_datainst *d;
    w89_eleminst *el;
    w89_expinst *o;
    w89_value v;
    const w89_ft *ft2;
    w89_globaltype gt;
    w89_limits lm;
    w89_tabletype tt;
    w89_name nm;
    w89_u32 k2;
    w89_externkind kind2;
    w89_u64 size;
    w89_u64 j;
    w89_u64 lim;
    w89_u64 np;
    w89_u64 di_len;
    w89_ref rr;
    w89_ref nl2;
    w89_funcinst *fi2;
    const w89_func *funcp;
    const w89_byte *mbp;
    const char *nb2;
    w89_u32 mlen;
    w89_u32 nlen;
    int ilen;
    int nn2;
    w89_u32 has_start;
    w89_u32 start;
    w89_funcinst *sf;
    w89_eval_out so;
    w89_eval_status st;
    const char *msg;
    w89_moduleinst *src;
    w89_externinst ext;
    w89_externtype xta;
    w89_externtype xte;
    w89_u32 nfuncs;
    w89_u32 nglobals;
    w89_u32 nmems;
    w89_u32 ntables;
    w89_u32 ntags;
    w89_u32 nexports;
    w89_u32 eln;
    w89_u32 ninit;
    w89_u32 isexpr;
    w89_u32 idx;
    w89_u32 is_ok;
    void *b;
    w89_ref *refsp;
    const w89_byte *db2;
    void *pa;
    w89_u32 *pn;
    w89_u32 *pc;
    void *item;
    const w89_import *im;
    const w89_name *inp;
    const w89_instr_vec *offp;
    const w89_instr_vec *tinit;
    const w89_elem *elmp;
    const w89_data *dp;
    const w89_export *ex;
    size_t esz;
    size_t sz2;

    *out = 0;
    w89_inst_msg[0] = 0;

    e = w89_typeenv_build(m, &env);
    if (e != W89_ERR_NONE) {
        return e;
    }
    e = store_add_tenv(s, &tenv);
    if (e != W89_ERR_NONE) {
        w89_typeenv_free(&env);
        return e;
    }
    *tenv = env;

    instsize = sizeof(w89_moduleinst);
    inst = malloc(instsize);
    if (inst == 0) {
        return W89_ERR_OUT_OF_MEMORY;
    }
    memset(inst, 0, instsize);
    inst->store = s;
    inst->types = tenv;

    psize = sizeof(w89_moduleinst *);
    pa = &s->mods;
    pn = &s->nmods;
    pc = &s->cmods;
    item = inst;
    e = vec_add(pa, pn, pc, &item, psize);
    if (e != W89_ERR_NONE) {
        return e;
    }

    /* Imports */
    n = m->nimports;
    for (i = 0; i < n; i = i + 1) {
        im = &m->imports[i];
        mbp = im->module.bytes;
        nb2 = (const char *)mbp;
        mlen = im->module.len;
        src = w89_registry_find(reg, nb2, mlen);
        if (src == 0) {
            mlen = im->module.len;
            ilen = (int)mlen;
            mbp = im->module.bytes;
            nb2 = (const char *)mbp;
            nlen = im->name.len;
            nn2 = (int)nlen;
            mbp = im->name.bytes;
            db2 = mbp;
            sprintf(w89_inst_msg, "unknown import \"%.*s\" \"%.*s\"",
                    ilen, nb2, nn2, db2);
            return W89_ERR_INVALID;
        }
        inp = &im->name;
        is_ok = w89_find_export(src, inp, &ext);
        if (is_ok == 0) {
            mlen = im->module.len;
            ilen = (int)mlen;
            mbp = im->module.bytes;
            nb2 = (const char *)mbp;
            nlen = im->name.len;
            nn2 = (int)nlen;
            mbp = im->name.bytes;
            db2 = mbp;
            sprintf(w89_inst_msg, "unknown import \"%.*s\" \"%.*s\"",
                    ilen, nb2, nn2, db2);
            return W89_ERR_INVALID;
        }
        externtype_of_inst(&ext, &xta);
        externtype_of_import(inst, im, &xte);
        is_ok = w89_match_externtype(&xta, &xte);
        if (is_ok == 0) {
            mlen = im->module.len;
            ilen = (int)mlen;
            mbp = im->module.bytes;
            nb2 = (const char *)mbp;
            nlen = im->name.len;
            nn2 = (int)nlen;
            mbp = im->name.bytes;
            db2 = mbp;
            sprintf(w89_inst_msg,
                    "incompatible import type for \"%.*s\" \"%.*s\"",
                    ilen, nb2, nn2, db2);
            return W89_ERR_INVALID;
        }
        e = inst_append(inst, &ext);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }

    /* Tags */
    tsize = sizeof(w89_taginst);
    psize = sizeof(w89_taginst *);
    n = m->ntags;
    for (i = 0; i < n; i = i + 1) {
        t = malloc(tsize);
        if (t == 0) {
            return W89_ERR_OUT_OF_MEMORY;
        }
        typeidx = m->tags[i].typeidx;
        ft2 = &tenv->types[typeidx].sub->ft;
        t->ft = ft2;
        t->env = tenv;
        t->typeidx = typeidx;
        pa = &s->tags;
        pn = &s->ntags;
        pc = &s->ctags;
        item = t;
        e = vec_add(pa, pn, pc, &item, psize);
        if (e != W89_ERR_NONE) {
            return e;
        }
        pa = &inst->tags;
        pn = &inst->ntags;
        pc = &inst->ctags;
        item = t;
        e = vec_add(pa, pn, pc, &item, psize);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }

    /* Functions */
    fsize = sizeof(w89_funcinst);
    psize = sizeof(w89_funcinst *);
    n = m->nfuncs;
    for (i = 0; i < n; i = i + 1) {
        f = malloc(fsize);
        if (f == 0) {
            return W89_ERR_OUT_OF_MEMORY;
        }
        memset(f, 0, fsize);
        f->is_host = 0;
        typeidx = m->func_types[i];
        f->typeidx = typeidx;
        f->inst = inst;
        funcp = &m->funcs[i];
        f->func = funcp;
        pa = &s->funcs;
        pn = &s->nfuncs;
        pc = &s->cfuncs;
        item = f;
        e = vec_add(pa, pn, pc, &item, psize);
        if (e != W89_ERR_NONE) {
            return e;
        }
        pa = &inst->funcs;
        pn = &inst->nfuncs;
        pc = &inst->cfuncs;
        item = f;
        e = vec_add(pa, pn, pc, &item, psize);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }

    /* Globals */
    gsize = sizeof(w89_globalinst);
    psize = sizeof(w89_globalinst *);
    n = m->nglobals;
    for (i = 0; i < n; i = i + 1) {
        g = malloc(gsize);
        if (g == 0) {
            return W89_ERR_OUT_OF_MEMORY;
        }
        offp = &m->globals[i].init;
        e = eval_const(inst, offp, &v);
        if (e != W89_ERR_NONE) {
            sprintf(w89_inst_msg, "constant expression required");
            return e;
        }
        gt = m->globals[i].type;
        g->type = gt;
        g->value = v;
        g->types = tenv;
        pa = &s->globals;
        pn = &s->nglobals;
        pc = &s->cglobals;
        item = g;
        e = vec_add(pa, pn, pc, &item, psize);
        if (e != W89_ERR_NONE) {
            return e;
        }
        pa = &inst->globals;
        pn = &inst->nglobals;
        pc = &inst->cglobals;
        item = g;
        e = vec_add(pa, pn, pc, &item, psize);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }

    /* Tables */
    tsize = sizeof(w89_tableinst);
    psize = sizeof(w89_tableinst *);
    rsize = sizeof(w89_ref);
    n = m->ntables;
    for (i = 0; i < n; i = i + 1) {
        ta = malloc(tsize);
        if (ta == 0) {
            return W89_ERR_OUT_OF_MEMORY;
        }
        memset(ta, 0, tsize);
        ninit = m->tables[i].init.n;
        if (ninit == 0) {
            nl2 = w89_ref_null();
            v = w89_value_ref(&nl2);
            e = W89_ERR_NONE;
        } else {
            tinit = &m->tables[i].init;
            e = eval_const(inst, tinit, &v);
        }
        if (e != W89_ERR_NONE) {
            sprintf(w89_inst_msg, "constant expression required");
            return e;
        }
        tt = m->tables[i].type;
        ta->type = tt;
        ta->types = tenv;
        lim = ta->type.limits.min;
        ta->size = lim;
        size = lim;
        if (size > 0) {
            sz2 = (size_t)size;
            sz2 = sz2 * rsize;
            b = malloc(sz2);
            if (b == 0) {
                return W89_ERR_OUT_OF_MEMORY;
            }
            ta->elems = b;
            for (j = 0; j < size; j = j + 1) {
                rr = v.u.ref;
                ta->elems[j] = rr;
            }
        }
        pa = &s->tables;
        pn = &s->ntables;
        pc = &s->ctables;
        item = ta;
        e = vec_add(pa, pn, pc, &item, psize);
        if (e != W89_ERR_NONE) {
            return e;
        }
        pa = &inst->tables;
        pn = &inst->ntables;
        pc = &inst->ctables;
        item = ta;
        e = vec_add(pa, pn, pc, &item, psize);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }

    /* Memories */
    msize = sizeof(w89_meminst);
    psize = sizeof(w89_meminst *);
    n = m->nmemories;
    for (i = 0; i < n; i = i + 1) {
        me = malloc(msize);
        if (me == 0) {
            return W89_ERR_OUT_OF_MEMORY;
        }
        memset(me, 0, msize);
        lm = m->memories[i].type;
        me->limits = lm;
        lim = me->limits.min;
        size = lim * W89_PAGE_SIZE_B;
        if (size > 0) {
            sz2 = (size_t)size;
            b = malloc(sz2);
            if (b == 0) {
                return W89_ERR_OUT_OF_MEMORY;
            }
            me->bytes = b;
            memset(b, 0, sz2);
        }
        np = me->limits.min;
        me->npages = np;
        pa = &s->mems;
        pn = &s->nmems;
        pc = &s->cmems;
        item = me;
        e = vec_add(pa, pn, pc, &item, psize);
        if (e != W89_ERR_NONE) {
            return e;
        }
        pa = &inst->memories;
        pn = &inst->nmemories;
        pc = &inst->cmemories;
        item = me;
        e = vec_add(pa, pn, pc, &item, psize);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }

    /* Data segments */
    dsize = sizeof(w89_datainst);
    psize = sizeof(w89_datainst *);
    n = m->ndatas;
    for (i = 0; i < n; i = i + 1) {
        d = malloc(dsize);
        if (d == 0) {
            return W89_ERR_OUT_OF_MEMORY;
        }
        db2 = m->datas[i].bytes;
        d->bytes = db2;
        di_len = m->datas[i].len;
        d->len = di_len;
        pa = &s->datas;
        pn = &s->ndatas;
        pc = &s->cdatas;
        item = d;
        e = vec_add(pa, pn, pc, &item, psize);
        if (e != W89_ERR_NONE) {
            return e;
        }
        pa = &inst->datas;
        pn = &inst->ndatas;
        pc = &inst->cdatas;
        item = d;
        e = vec_add(pa, pn, pc, &item, psize);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }

    /* Element segments */
    elsize = sizeof(w89_eleminst);
    psize = sizeof(w89_eleminst *);
    n = m->nelems;
    for (i = 0; i < n; i = i + 1) {
        el = malloc(elsize);
        if (el == 0) {
            return W89_ERR_OUT_OF_MEMORY;
        }
        memset(el, 0, elsize);
        eln = m->elems[i].n;
        el->n = eln;
        if (eln > 0) {
            sz2 = (size_t)eln;
            sz2 = sz2 * rsize;
            b = malloc(sz2);
            if (b == 0) {
                return W89_ERR_OUT_OF_MEMORY;
            }
            el->refs = b;
            isexpr = m->elems[i].is_expr;
            if (isexpr != 0) {
                elmp = &m->elems[i];
                refsp = el->refs;
                e = eval_elem_exprs(inst, elmp, refsp);
            } else {
                for (k = 0; k < eln; k = k + 1) {
                    idx = m->elems[i].indices[k];
                    fi2 = inst->funcs[idx];
                    rr = w89_ref_func(fi2);
                    el->refs[k] = rr;
                }
                e = W89_ERR_NONE;
            }
            if (e != W89_ERR_NONE) {
                return e;
            }
        }
        pa = &s->elems;
        pn = &s->nelems;
        pc = &s->celems;
        item = el;
        e = vec_add(pa, pn, pc, &item, psize);
        if (e != W89_ERR_NONE) {
            return e;
        }
        pa = &inst->elems;
        pn = &inst->nelems;
        pc = &inst->celems;
        item = el;
        e = vec_add(pa, pn, pc, &item, psize);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }

    /* Exports */
    nexports = m->nexports;
    if (nexports > 0) {
        esz = (size_t)nexports;
        esz = esz * sizeof(w89_expinst);
        b = malloc(esz);
        if (b == 0) {
            return W89_ERR_OUT_OF_MEMORY;
        }
        inst->exports = b;
    }
    n = m->nexports;
    for (i = 0; i < n; i = i + 1) {
        ex = &m->exports[i];
        o = &inst->exports[i];
        memset(o, 0, sizeof(w89_expinst));
        nm = ex->name;
        o->name = nm;
        k2 = ex->kind;
        kind2 = wasm_kind(k2);
        o->kind = kind2;
        switch (kind2) {
        case W89_EXT_FUNC:
            idx = ex->index;
            nfuncs = inst->nfuncs;
            if (idx >= nfuncs) {
                return W89_ERR_INVALID;
            }
            fi2 = inst->funcs[idx];
            o->u.func = fi2;
            break;
        case W89_EXT_GLOBAL:
            idx = ex->index;
            nglobals = inst->nglobals;
            if (idx >= nglobals) {
                return W89_ERR_INVALID;
            }
            g = inst->globals[idx];
            o->u.global = g;
            break;
        case W89_EXT_MEMORY:
            idx = ex->index;
            nmems = inst->nmemories;
            if (idx >= nmems) {
                return W89_ERR_INVALID;
            }
            me = inst->memories[idx];
            o->u.memory = me;
            break;
        case W89_EXT_TABLE:
            idx = ex->index;
            ntables = inst->ntables;
            if (idx >= ntables) {
                return W89_ERR_INVALID;
            }
            ta = inst->tables[idx];
            o->u.table = ta;
            break;
        case W89_EXT_TAG:
            idx = ex->index;
            ntags = inst->ntags;
            if (idx >= ntags) {
                return W89_ERR_INVALID;
            }
            t = inst->tags[idx];
            o->u.tag = t;
            break;
        }
    }
    nexports = m->nexports;
    inst->nexports = nexports;

    /* Element segment injection (active), then data, then start */
    n = m->nelems;
    for (i = 0; i < n; i = i + 1) {
        elmp = &m->elems[i];
        e = run_elem_segment(s, inst, elmp, i);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }
    n = m->ndatas;
    for (i = 0; i < n; i = i + 1) {
        dp = &m->datas[i];
        e = run_data_segment(s, inst, dp, i);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }
    has_start = m->has_start;
    if (has_start != 0) {
        start = m->start;
        sf = inst->funcs[start];
        so = w89_invoke(s, sf, 0, 0);
        st = so.status;
        if (st != W89_EVAL_OK) {
            msg = so.msg;
            if (msg != 0) {
                sprintf(w89_inst_msg, "%s", msg);
            }
            w89_eval_out_free(&so);
            return W89_ERR_INVALID;
        }
        w89_eval_out_free(&so);
    }

    *out = inst;
    return W89_ERR_NONE;
}
