#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eval.h"

/* The pre-registered "spectest" host module (reference interpreter's
 * host/spectest.ml). The print functions write bare lines to stdout;
 * the REPL/driver distinguish them from @-prefixed command results. */

static w89_vt vt_i32;
static w89_vt vt_i64;
static w89_vt vt_f32;
static w89_vt vt_f64;

static w89_ft ft_print;
static w89_ft ft_print_i32;
static w89_ft ft_print_i64;
static w89_ft ft_print_f32;
static w89_ft ft_print_f64;
static w89_vt ft_p_i32_f32[2];
static w89_ft ft_print_i32_f32;
static w89_vt ft_p_f64_f64[2];
static w89_ft ft_print_f64_f64;

static w89_host_status host_print(const w89_value *args, w89_u32 nargs,
                                  w89_value *res, w89_u32 *nres,
                                  const char **trap)
{
    (void)args;
    (void)nargs;
    (void)res;
    (void)trap;
    printf("\n");
    *nres = 0;
    return W89_HOST_OK;
}

static w89_host_status host_print_i32(const w89_value *args, w89_u32 nargs,
                                      w89_value *res, w89_u32 *nres,
                                      const char **trap)
{
    w89_u64 n;
    w89_u32 n32;
    unsigned un;
    (void)nargs;
    (void)res;
    (void)trap;
    n = args[0].u.num;
    n32 = (w89_u32)n;
    un = (unsigned)n32;
    printf("i32: %u\n", un);
    *nres = 0;
    return W89_HOST_OK;
}

static w89_host_status host_print_i64(const w89_value *args, w89_u32 nargs,
                                      w89_value *res, w89_u32 *nres,
                                      const char **trap)
{
    w89_u64 n;
    unsigned long un;
    (void)nargs;
    (void)res;
    (void)trap;
    n = args[0].u.num;
    un = (unsigned long)n;
    printf("i64: %lu\n", un);
    *nres = 0;
    return W89_HOST_OK;
}

static w89_host_status host_print_f32(const w89_value *args, w89_u32 nargs,
                                      w89_value *res, w89_u32 *nres,
                                      const char **trap)
{
    w89_u64 n;
    w89_u32 n32;
    w89_f32 f;
    double d;
    (void)nargs;
    (void)res;
    (void)trap;
    n = args[0].u.num;
    n32 = (w89_u32)n;
    f = w89_bits_f32(n32);
    d = (double)f;
    printf("f32: %g\n", d);
    *nres = 0;
    return W89_HOST_OK;
}

static w89_host_status host_print_f64(const w89_value *args, w89_u32 nargs,
                                      w89_value *res, w89_u32 *nres,
                                      const char **trap)
{
    w89_u64 n;
    w89_f64 f;
    (void)nargs;
    (void)res;
    (void)trap;
    n = args[0].u.num;
    f = w89_bits_f64(n);
    printf("f64: %g\n", f);
    *nres = 0;
    return W89_HOST_OK;
}

static w89_host_status host_print_i32_f32(const w89_value *args, w89_u32 nargs,
                                          w89_value *res, w89_u32 *nres,
                                          const char **trap)
{
    w89_u64 n0;
    w89_u64 n1;
    w89_u32 n32;
    unsigned un;
    w89_f32 f;
    double d;
    (void)nargs;
    (void)res;
    (void)trap;
    n0 = args[0].u.num;
    n32 = (w89_u32)n0;
    un = (unsigned)n32;
    n1 = args[1].u.num;
    n32 = (w89_u32)n1;
    f = w89_bits_f32(n32);
    d = (double)f;
    printf("i32: %u f32: %g\n", un, d);
    *nres = 0;
    return W89_HOST_OK;
}

static w89_host_status host_print_f64_f64(const w89_value *args, w89_u32 nargs,
                                          w89_value *res, w89_u32 *nres,
                                          const char **trap)
{
    w89_u64 n0;
    w89_u64 n1;
    w89_f64 f0;
    w89_f64 f1;
    (void)nargs;
    (void)res;
    (void)trap;
    n0 = args[0].u.num;
    f0 = w89_bits_f64(n0);
    n1 = args[1].u.num;
    f1 = w89_bits_f64(n1);
    printf("f64: %g f64: %g\n", f0, f1);
    *nres = 0;
    return W89_HOST_OK;
}

static w89_host_status host_abort(const w89_value *args, w89_u32 nargs,
                                  w89_value *res, w89_u32 *nres,
                                  const char **trap)
{
    (void)args;
    (void)nargs;
    (void)res;
    *nres = 0;
    *trap = "abort";
    return W89_HOST_TRAP;
}

typedef struct host_func_def {
    const char *name;
    const w89_ft *ft;
    w89_hostfn fn;
} host_func_def;

static host_func_def host_funcs[8];
static w89_u32 nhost_funcs;

static void host_static_init(void)
{
    vt_i32.num = 0x7F;
    vt_i64.num = 0x7E;
    vt_f32.num = 0x7D;
    vt_f64.num = 0x7C;

    ft_p_i32_f32[0].num = 0x7F;
    ft_p_i32_f32[1].num = 0x7D;
    ft_print_i32_f32.params = ft_p_i32_f32;
    ft_print_i32_f32.nparams = 2;
    ft_p_f64_f64[0].num = 0x7C;
    ft_p_f64_f64[1].num = 0x7C;
    ft_print_f64_f64.params = ft_p_f64_f64;
    ft_print_f64_f64.nparams = 2;

    ft_print_i32.params = &vt_i32;
    ft_print_i32.nparams = 1;
    ft_print_i64.params = &vt_i64;
    ft_print_i64.nparams = 1;
    ft_print_f32.params = &vt_f32;
    ft_print_f32.nparams = 1;
    ft_print_f64.params = &vt_f64;
    ft_print_f64.nparams = 1;

    host_funcs[0].name = "print";
    host_funcs[0].ft = &ft_print;
    host_funcs[0].fn = host_print;
    host_funcs[1].name = "print_i32";
    host_funcs[1].ft = &ft_print_i32;
    host_funcs[1].fn = host_print_i32;
    host_funcs[2].name = "print_i64";
    host_funcs[2].ft = &ft_print_i64;
    host_funcs[2].fn = host_print_i64;
    host_funcs[3].name = "print_f32";
    host_funcs[3].ft = &ft_print_f32;
    host_funcs[3].fn = host_print_f32;
    host_funcs[4].name = "print_f64";
    host_funcs[4].ft = &ft_print_f64;
    host_funcs[4].fn = host_print_f64;
    host_funcs[5].name = "print_i32_f32";
    host_funcs[5].ft = &ft_print_i32_f32;
    host_funcs[5].fn = host_print_i32_f32;
    host_funcs[6].name = "print_f64_f64";
    host_funcs[6].ft = &ft_print_f64_f64;
    host_funcs[6].fn = host_print_f64_f64;
    host_funcs[7].name = "abort";
    host_funcs[7].ft = &ft_print;
    host_funcs[7].fn = host_abort;
    nhost_funcs = 8;
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

static w89_err store_add_func(w89_store *s, w89_moduleinst *inst,
                              const host_func_def *def)
{
    w89_funcinst *f;
    w89_err e;
    w89_u32 fsize;
    w89_u32 psize;
    void *pa;
    w89_u32 *pn;
    w89_u32 *pc;
    const w89_ft *dft;
    w89_hostfn dfn;

    fsize = sizeof(w89_funcinst);
    f = malloc(fsize);
    if (f == 0) {
        return W89_ERR_OUT_OF_MEMORY;
    }
    memset(f, 0, fsize);
    f->is_host = 1;
    f->inst = inst;
    dft = def->ft;
    f->ft = dft;
    dfn = def->fn;
    f->host = dfn;
    psize = sizeof(w89_funcinst *);
    pa = &s->funcs;
    pn = &s->nfuncs;
    pc = &s->cfuncs;
    e = vec_add(pa, pn, pc, &f, psize);
    if (e != W89_ERR_NONE) {
        return e;
    }
    pa = &inst->funcs;
    pn = &inst->nfuncs;
    pc = &inst->cfuncs;
    e = vec_add(pa, pn, pc, &f, psize);
    return e;
}

w89_moduleinst *w89_spectest(w89_store *s)
{
    w89_moduleinst *inst;
    w89_globalinst *gi;
    w89_meminst *m;
    w89_tableinst *t;
    w89_tableinst *t64;
    w89_err e;
    w89_u32 i;
    w89_u32 instsize;
    w89_u32 gsize;
    w89_u32 msize;
    w89_u32 tsize;
    w89_u32 rsize;
    w89_u32 rsize10;
    w89_u32 exsize;
    w89_u32 exn;
    w89_u32 psize;
    w89_u32 vsize;
    w89_u32 e8;
    w89_u32 nl32;
    w89_u64 gv;
    w89_u32 f32v;
    w89_u64 f64v;
    w89_vt gt;
    w89_value vv;
    w89_ref r;
    w89_byte *mb;
    w89_ref *tr;
    w89_expinst *ex;
    w89_funcinst *fi;
    w89_globalinst *gi2;
    const char *nm;
    const w89_byte *nb;
    size_t nl;
    void *pa;
    w89_u32 *pn;
    w89_u32 *pc;
    const host_func_def *hfd;
    static const char *names[15] = {
        "print", "print_i32", "print_i64", "print_f32", "print_f64",
        "print_i32_f32", "print_f64_f64", "abort",
        "global_i32", "global_i64", "global_f32", "global_f64",
        "memory", "table", "table64"
    };

    host_static_init();
    instsize = sizeof(w89_moduleinst);
    inst = malloc(instsize);
    if (inst == 0) {
        return NULL;
    }
    memset(inst, 0, instsize);
    inst->store = s;

    for (i = 0; i < nhost_funcs; i = i + 1) {
        hfd = &host_funcs[i];
        e = store_add_func(s, inst, hfd);
        if (e != W89_ERR_NONE) {
            return NULL;
        }
    }

    {
        static const w89_vt gtypes[4] = {
            { 0, 0x7F, { 0, 0, 0, 0 } }, { 0, 0x7E, { 0, 0, 0, 0 } },
            { 0, 0x7D, { 0, 0, 0, 0 } }, { 0, 0x7C, { 0, 0, 0, 0 } }
        };
        w89_u64 gvals[4];
        gvals[0] = 666;
        gvals[1] = 666;
        f32v = w89_f32_bits(666.6f);
        gvals[2] = f32v;
        f64v = w89_f64_bits(666.6);
        gvals[3] = f64v;
        gsize = sizeof(w89_globalinst);
        psize = sizeof(w89_globalinst *);
        for (i = 0; i < 4; i = i + 1) {
            gi = malloc(gsize);
            if (gi == 0) {
                return NULL;
            }
            memset(gi, 0, gsize);
            gi->type.mut = 0;
            gt = gtypes[i];
            gi->type.vt = gt;
            gv = gvals[i];
            vv = w89_value_num(gv);
            gi->value = vv;
            pa = &s->globals;
            pn = &s->nglobals;
            pc = &s->cglobals;
            e = vec_add(pa, pn, pc, &gi, psize);
            if (e != W89_ERR_NONE) {
                return NULL;
            }
            pa = &inst->globals;
            pn = &inst->nglobals;
            pc = &inst->cglobals;
            e = vec_add(pa, pn, pc, &gi, psize);
            if (e != W89_ERR_NONE) {
                return NULL;
            }
        }
    }

    msize = sizeof(w89_meminst);
    m = malloc(msize);
    if (m == 0) {
        return NULL;
    }
    memset(m, 0, msize);
    m->limits.min = 1;
    m->limits.max = 2;
    m->limits.has_max = 1;
    mb = malloc(W89_PAGE_SIZE);
    if (mb == 0) {
        return NULL;
    }
    m->bytes = mb;
    memset(mb, 0, W89_PAGE_SIZE);
    m->npages = 1;
    psize = sizeof(w89_meminst *);
    pa = &s->mems;
    pn = &s->nmems;
    pc = &s->cmems;
    e = vec_add(pa, pn, pc, &m, psize);
    if (e != W89_ERR_NONE) {
        return NULL;
    }
    pa = &inst->memories;
    pn = &inst->nmemories;
    pc = &inst->cmemories;
    e = vec_add(pa, pn, pc, &m, psize);
    if (e != W89_ERR_NONE) {
        return NULL;
    }

    tsize = sizeof(w89_tableinst);
    t = malloc(tsize);
    if (t == 0) {
        return NULL;
    }
    memset(t, 0, tsize);
    t->type.limits.min = 10;
    t->type.limits.max = 20;
    t->type.limits.has_max = 1;
    t->type.rt.is_typeidx = 0;
    t->type.rt.abs = W89_HT_FUNC;
    t->type.rt.nullable = 1;
    t->size = 10;
    rsize = sizeof(w89_ref);
    rsize10 = 10 * rsize;
    tr = malloc(rsize10);
    if (tr == 0) {
        return NULL;
    }
    t->elems = tr;
    for (i = 0; i < 10; i = i + 1) {
        r = w89_ref_null();
        t->elems[i] = r;
    }
    psize = sizeof(w89_tableinst *);
    pa = &s->tables;
    pn = &s->ntables;
    pc = &s->ctables;
    e = vec_add(pa, pn, pc, &t, psize);
    if (e != W89_ERR_NONE) {
        return NULL;
    }
    pa = &inst->tables;
    pn = &inst->ntables;
    pc = &inst->ctables;
    e = vec_add(pa, pn, pc, &t, psize);
    if (e != W89_ERR_NONE) {
        return NULL;
    }

    t64 = malloc(tsize);
    if (t64 == 0) {
        return NULL;
    }
    memset(t64, 0, tsize);
    t64->type.limits.addr64 = 1;
    t64->type.limits.min = 10;
    t64->type.limits.max = 20;
    t64->type.limits.has_max = 1;
    t64->type.rt.is_typeidx = 0;
    t64->type.rt.abs = W89_HT_FUNC;
    t64->type.rt.nullable = 1;
    t64->size = 10;
    tr = malloc(rsize10);
    if (tr == 0) {
        return NULL;
    }
    t64->elems = tr;
    for (i = 0; i < 10; i = i + 1) {
        r = w89_ref_null();
        t64->elems[i] = r;
    }
    pa = &s->tables;
    pn = &s->ntables;
    pc = &s->ctables;
    e = vec_add(pa, pn, pc, &t64, psize);
    if (e != W89_ERR_NONE) {
        return NULL;
    }
    pa = &inst->tables;
    pn = &inst->ntables;
    pc = &inst->ctables;
    e = vec_add(pa, pn, pc, &t64, psize);
    if (e != W89_ERR_NONE) {
        return NULL;
    }

    exsize = sizeof(w89_expinst);
    exn = 15 * exsize;
    ex = malloc(exn);
    if (ex == 0) {
        return NULL;
    }
    inst->exports = ex;
    for (i = 0; i < 15; i = i + 1) {
        nm = names[i];
        nb = (const w89_byte *)nm;
        inst->exports[i].name.bytes = nb;
        nl = strlen(nm);
        nl32 = (w89_u32)nl;
        inst->exports[i].name.len = nl32;
    }
    for (i = 0; i < 8; i = i + 1) {
        inst->exports[i].kind = W89_EXT_FUNC;
        fi = inst->funcs[i];
        inst->exports[i].u.func = fi;
    }
    for (i = 0; i < 4; i = i + 1) {
        e8 = 8 + i;
        inst->exports[e8].kind = W89_EXT_GLOBAL;
        gi2 = inst->globals[i];
        inst->exports[e8].u.global = gi2;
    }
    inst->exports[12].kind = W89_EXT_MEMORY;
    inst->exports[12].u.memory = m;
    inst->exports[13].kind = W89_EXT_TABLE;
    inst->exports[13].u.table = t;
    inst->exports[14].kind = W89_EXT_TABLE;
    inst->exports[14].u.table = t64;
    inst->nexports = 15;

    vsize = sizeof(w89_moduleinst *);
    pa = &s->mods;
    pn = &s->nmods;
    pc = &s->cmods;
    e = vec_add(pa, pn, pc, &inst, vsize);
    if (e != W89_ERR_NONE) {
        return NULL;
    }
    return inst;
}
