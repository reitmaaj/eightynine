#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "validate.h"

/* C89 does not declare snprintf/vsnprintf; they are provided by libc. */
int snprintf(char *s, size_t n, const char *fmt, ...);
int vsnprintf(char *s, size_t n, const char *fmt, va_list ap);

static char w89_vmsg[512];

static void vmsg(const char *fmt, ...)
{
    va_list ap;
    size_t nb;
    nb = sizeof(char[512]);
    va_start(ap, fmt);
    vsnprintf(w89_vmsg, nb, fmt, ap);
    va_end(ap);
}

/* ---------- Type environment ---------- */

w89_err w89_typeenv_build(const w89_module *m, w89_typeenv *env)
{
    w89_u32 i, k, idx = 0;
    w89_u32 n, nt, sz;
    w89_u32 nrec;
    w89_recgroup *recs;
    w89_deftype *types;
    w89_rectype *rt;
    w89_subtype *subtypes;
    memset(env, 0, sizeof(w89_typeenv));
    nrec = m->nrectypes;
    env->nrecs = nrec;
    if (nrec != 0) {
        sz = nrec * sizeof(w89_recgroup);
        recs = malloc(sz);
        env->recs = recs;
        if (recs == 0) {
            return W89_ERR_OUT_OF_MEMORY;
        }
    }
    env->ntypes = 0;
    for (i = 0; i < nrec; i = i + 1) {
        rt = &m->rectypes[i];
        nt = rt->n;
        n = env->ntypes;
        n = n + nt;
        env->ntypes = n;
    }
    n = env->ntypes;
    if (n != 0) {
        sz = n * sizeof(w89_deftype);
        types = malloc(sz);
        env->types = types;
        if (types == 0) {
            recs = env->recs;
            free(recs);
            env->recs = 0;
            return W89_ERR_OUT_OF_MEMORY;
        }
    }
    for (i = 0; i < nrec; i = i + 1) {
        rt = &m->rectypes[i];
        env->recs[i].first = idx;
        nt = rt->n;
        env->recs[i].nsubs = nt;
        for (k = 0; k < nt; k = k + 1) {
            subtypes = &rt->subtypes[k];
            env->types[idx].sub = subtypes;
            env->types[idx].recgroup = i;
            env->types[idx].recpos = k;
            idx = idx + 1;
        }
    }
    return W89_ERR_NONE;
}

void w89_typeenv_free(w89_typeenv *env)
{
    w89_deftype *types;
    w89_recgroup *recs;
    types = env->types;
    free(types);
    recs = env->recs;
    free(recs);
    memset(env, 0, sizeof(w89_typeenv));
}

/* ---------- Canonical equality ---------- */

static int typeuse_canon_eq(const w89_typeenv *env, w89_u32 x, w89_u32 y,
                            w89_u32 ga, w89_u32 gb)
{
    int xin, yin;
    w89_u32 nt, rg;
    w89_deftype *types;
    w89_deftype *dx, *dy;
    nt = env->ntypes;
    types = env->types;
    xin = 0;
    if (x < nt) {
        dx = &types[x];
        rg = dx->recgroup;
        if (rg == ga) {
            xin = 1;
        }
    }
    yin = 0;
    if (y < nt) {
        dy = &types[y];
        rg = dy->recgroup;
        if (rg == gb) {
            yin = 1;
        }
    }
    if (xin) {
        if (yin) {
            dx = &types[x];
            dy = &types[y];
            rg = dx->recpos;
            nt = dy->recpos;
            return rg == nt;
        }
    }
    if (xin) {
        return 0;
    }
    if (yin) {
        return 0;
    }
    return w89_type_canon_eq(env, x, y);
}

static int ht_canon_eq(const w89_typeenv *env, const w89_reftype *a,
                       const w89_reftype *b, w89_u32 ga, w89_u32 gb)
{
    w89_u32 ai, bi;
    ai = a->is_typeidx;
    bi = b->is_typeidx;
    if (ai != bi) {
        return 0;
    }
    if (ai) {
        ai = a->typeidx;
        bi = b->typeidx;
        return typeuse_canon_eq(env, ai, bi, ga, gb);
    }
    ai = a->abs;
    bi = b->abs;
    return ai == bi;
}

static int vt_canon_eq(const w89_typeenv *env, const w89_vt *a,
                       const w89_vt *b, w89_u32 ga, w89_u32 gb)
{
    w89_u32 ar, br;
    const w89_reftype *art;
    const w89_reftype *brt;
    ar = a->is_ref;
    br = b->is_ref;
    if (ar != br) {
        return 0;
    }
    if (!ar) {
        ar = a->num;
        br = b->num;
        return ar == br;
    }
    ar = a->rt.nullable;
    br = b->rt.nullable;
    if (ar != br) {
        return 0;
    }
    art = &a->rt;
    brt = &b->rt;
    return ht_canon_eq(env, art, brt, ga, gb);
}

static int field_canon_eq(const w89_typeenv *env, const w89_fieldtype *a,
                          const w89_fieldtype *b, w89_u32 ga, w89_u32 gb)
{
    w89_u32 ap, bp;
    const w89_vt *avt;
    const w89_vt *bvt;
    ap = a->is_packed;
    bp = b->is_packed;
    if (ap != bp) {
        return 0;
    }
    if (ap) {
        ap = a->packed;
        bp = b->packed;
        return ap == bp;
    }
    avt = &a->vt;
    bvt = &b->vt;
    return vt_canon_eq(env, avt, bvt, ga, gb);
}

static int comptype_canon_eq(const w89_typeenv *env, const w89_subtype *a,
                             const w89_subtype *b, w89_u32 ga, w89_u32 gb)
{
    w89_u32 i, n, m, ak;
    int eq;
    w89_vt *ap;
    w89_vt *bp;
    w89_fieldtype *af;
    w89_fieldtype *bf;
    ak = a->kind;
    m = b->kind;
    if (ak != m) {
        return 0;
    }
    if (ak == W89_CK_FUNC) {
        n = a->ft.nparams;
        m = b->ft.nparams;
        if (n != m) {
            return 0;
        }
        n = a->ft.nresults;
        m = b->ft.nresults;
        if (n != m) {
            return 0;
        }
        n = a->ft.nparams;
        for (i = 0; i < n; i = i + 1) {
            ap = &a->ft.params[i];
            bp = &b->ft.params[i];
            eq = vt_canon_eq(env, ap, bp, ga, gb);
            if (!eq) {
                return 0;
            }
        }
        n = a->ft.nresults;
        for (i = 0; i < n; i = i + 1) {
            ap = &a->ft.results[i];
            bp = &b->ft.results[i];
            eq = vt_canon_eq(env, ap, bp, ga, gb);
            if (!eq) {
                return 0;
            }
        }
        return 1;
    }
    if (ak == W89_CK_STRUCT) {
        n = a->nfields;
        m = b->nfields;
        if (n != m) {
            return 0;
        }
        for (i = 0; i < n; i = i + 1) {
            af = &a->fields[i];
            bf = &b->fields[i];
            eq = field_canon_eq(env, af, bf, ga, gb);
            if (!eq) {
                return 0;
            }
        }
        return 1;
    }
    af = &a->fields[0];
    bf = &b->fields[0];
    return field_canon_eq(env, af, bf, ga, gb);
}

static int recgroup_canon_eq(const w89_typeenv *env, w89_u32 ga, w89_u32 gb)
{
    w89_u32 k, j;
    int eq;
    w89_recgroup *ra, *rb;
    w89_u32 nsubs, first, first2, fi;
    const w89_subtype *sa, *sb;
    w89_u32 nsup;
    w89_u32 *spa, *spb;
    w89_u32 va, vb;
    if (ga == gb) {
        return 1;
    }
    ra = &env->recs[ga];
    rb = &env->recs[gb];
    nsubs = ra->nsubs;
    va = rb->nsubs;
    if (nsubs != va) {
        return 0;
    }
    first = ra->first;
    first2 = rb->first;
    for (k = 0; k < nsubs; k = k + 1) {
        fi = first + k;
        sa = env->types[fi].sub;
        fi = first2 + k;
        sb = env->types[fi].sub;
        va = sa->is_final;
        vb = sb->is_final;
        if (va != vb) {
            return 0;
        }
        nsup = sa->nsupers;
        va = sb->nsupers;
        if (nsup != va) {
            return 0;
        }
        for (j = 0; j < nsup; j = j + 1) {
            spa = &sa->supertypes[j];
            spb = &sb->supertypes[j];
            va = *spa;
            vb = *spb;
            eq = typeuse_canon_eq(env, va, vb, ga, gb);
            if (!eq) {
                return 0;
            }
        }
        eq = comptype_canon_eq(env, sa, sb, ga, gb);
        if (!eq) {
            return 0;
        }
    }
    return 1;
}

int w89_type_canon_eq(const w89_typeenv *env, w89_u32 a, w89_u32 b)
{
    w89_u32 nt, rp, rp2, rg, rg2;
    w89_deftype *da, *db;
    nt = env->ntypes;
    if (a == b) {
        return 1;
    }
    if (a >= nt) {
        return 0;
    }
    if (b >= nt) {
        return 0;
    }
    da = &env->types[a];
    db = &env->types[b];
    rp = da->recpos;
    rp2 = db->recpos;
    if (rp != rp2) {
        return 0;
    }
    rg = da->recgroup;
    rg2 = db->recgroup;
    return recgroup_canon_eq(env, rg, rg2);
}

/* ---------- Subtyping ---------- */

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

static int abstract_lt_abstract(w89_absheaptype ha, w89_absheaptype hb)
{
    if (ha == hb) {
        return 1;
    }
    switch (ha) {
    case W89_HT_EQ:
        return hb == W89_HT_ANY;
    case W89_HT_STRUCT:
    case W89_HT_ARRAY:
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
        return hb == W89_HT_I31;
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

static int abstract_lt_type(const w89_typeenv *env, w89_absheaptype ha,
                            w89_u32 x)
{
    w89_compkind kind;
    const w89_deftype *dt;
    const w89_subtype *st;
    dt = &env->types[x];
    st = dt->sub;
    kind = st->kind;
    switch (ha) {
    case W89_HT_NONE:
        if (kind == W89_CK_STRUCT) {
            return 1;
        }
        return kind == W89_CK_ARRAY;
    case W89_HT_NOFUNC:
        return kind_matches_abstract(kind, W89_HT_FUNC);
    case W89_HT_NOEXN:
        return kind_matches_abstract(kind, W89_HT_EXN);
    case W89_HT_NOEXTERN:
        return kind_matches_abstract(kind, W89_HT_EXTERN);
    default:
        return 0;
    }
}

static int ht_match(const w89_typeenv *env, const w89_reftype *a,
                    const w89_reftype *b)
{
    w89_u32 ai, bi;
    w89_u32 ti;
    w89_compkind kind;
    const w89_deftype *dt;
    const w89_subtype *st;
    ai = a->is_typeidx;
    if (!ai) {
        ai = a->abs;
        if (ai == 0) {
            return 1;
        }
    }
    ai = a->is_typeidx;
    if (ai) {
        bi = b->is_typeidx;
        if (bi) {
            ai = a->typeidx;
            bi = b->typeidx;
            return w89_match_deftype(env, ai, bi);
        }
        ti = a->typeidx;
        dt = &env->types[ti];
        st = dt->sub;
        kind = st->kind;
        bi = b->abs;
        return kind_matches_abstract(kind, bi);
    }
    bi = b->is_typeidx;
    if (bi) {
        ai = a->abs;
        bi = b->typeidx;
        return abstract_lt_type(env, ai, bi);
    }
    ai = a->abs;
    bi = b->abs;
    return abstract_lt_abstract(ai, bi);
}

int w89_match_reftype(const w89_typeenv *env, const w89_reftype *a,
                      const w89_reftype *b)
{
    w89_u32 an, bn;
    an = a->nullable;
    bn = b->nullable;
    if (an == 1) {
        if (bn == 0) {
            return 0;
        }
    }
    return ht_match(env, a, b);
}

static int vt_is_bot(const w89_vt *v);

int w89_match_valtype(const w89_typeenv *env, const w89_vt *a,
                      const w89_vt *b)
{
    w89_u32 ar, br, an, bn;
    int bot;
    const w89_reftype *rt1;
    const w89_reftype *rt2;
    bot = vt_is_bot(a);
    if (bot) {
        return 1;
    }
    ar = a->is_ref;
    br = b->is_ref;
    if (ar != br) {
        return 0;
    }
    if (!ar) {
        an = a->num;
        bn = b->num;
        return an == bn;
    }
    rt1 = &a->rt;
    rt2 = &b->rt;
    return w89_match_reftype(env, rt1, rt2);
}

int w89_match_resulttype(const w89_typeenv *env, const w89_vt *a,
                         w89_u32 na, const w89_vt *b, w89_u32 nb)
{
    w89_u32 i;
    const w89_vt *av;
    const w89_vt *bv;
    int m;
    if (na != nb) {
        return 0;
    }
    for (i = 0; i < na; i = i + 1) {
        av = &a[i];
        bv = &b[i];
        m = w89_match_valtype(env, av, bv);
        if (!m) {
            return 0;
        }
    }
    return 1;
}

static int match_storagetype(const w89_typeenv *env, const w89_fieldtype *a,
                             const w89_fieldtype *b)
{
    w89_u32 ap, bp;
    const w89_vt *vt1;
    const w89_vt *vt2;
    ap = a->is_packed;
    bp = b->is_packed;
    if (ap != bp) {
        return 0;
    }
    if (ap) {
        ap = a->packed;
        bp = b->packed;
        return ap == bp;
    }
    vt1 = &a->vt;
    vt2 = &b->vt;
    return w89_match_valtype(env, vt1, vt2);
}

static int match_fieldtype(const w89_typeenv *env, const w89_fieldtype *a,
                           const w89_fieldtype *b)
{
    w89_u32 am, bm;
    int ms;
    am = a->mut;
    bm = b->mut;
    if (am != bm) {
        return 0;
    }
    ms = match_storagetype(env, a, b);
    if (!ms) {
        return 0;
    }
    if (am == 0) {
        return 1;
    }
    return match_storagetype(env, b, a);
}

int w89_match_comptype(const w89_typeenv *env, const w89_subtype *a,
                       const w89_subtype *b)
{
    w89_u32 i, n, m;
    w89_u32 ak;
    int ok;
    w89_vt *ap;
    w89_vt *bp;
    w89_fieldtype *af;
    w89_fieldtype *bf;
    ak = a->kind;
    m = b->kind;
    if (ak != m) {
        return 0;
    }
    if (ak == W89_CK_FUNC) {
        ap = b->ft.params;
        n = b->ft.nparams;
        bp = a->ft.params;
        m = a->ft.nparams;
        ok = w89_match_resulttype(env, ap, n, bp, m);
        if (!ok) {
            return 0;
        }
        ap = a->ft.results;
        n = a->ft.nresults;
        bp = b->ft.results;
        m = b->ft.nresults;
        return w89_match_resulttype(env, ap, n, bp, m);
    }
    if (ak == W89_CK_STRUCT) {
        n = a->nfields;
        m = b->nfields;
        if (n < m) {
            return 0;
        }
        n = b->nfields;
        for (i = 0; i < n; i = i + 1) {
            af = &a->fields[i];
            bf = &b->fields[i];
            ok = match_fieldtype(env, af, bf);
            if (!ok) {
                return 0;
            }
        }
        return 1;
    }
    af = &a->fields[0];
    bf = &b->fields[0];
    return match_fieldtype(env, af, bf);
}

int w89_match_deftype(const w89_typeenv *env, w89_u32 a, w89_u32 b)
{
    w89_u32 j, nsup, sp;
    int ok;
    const w89_deftype *dt;
    const w89_subtype *st;
    if (a == b) {
        return 1;
    }
    ok = w89_type_canon_eq(env, a, b);
    if (ok) {
        return 1;
    }
    dt = &env->types[a];
    st = dt->sub;
    nsup = st->nsupers;
    for (j = 0; j < nsup; j = j + 1) {
        sp = st->supertypes[j];
        ok = w89_match_deftype(env, sp, b);
        if (ok) {
            return 1;
        }
    }
    return 0;
}

/* ---------- Validation context ---------- */

#define W89_ELL_NO 0
#define W89_ELL_YES 1

typedef struct w89_local {
    w89_u32 init;
    w89_vt vt;
} w89_local;

typedef struct w89_label {
    const w89_vt *vts;
    w89_u32 len;
} w89_label;

typedef struct w89_valctx {
    w89_typeenv types;
    w89_u32 *tags;
    w89_u32 ntags, tags_cap;
    w89_globaltype *globals;
    w89_u32 nglobals, globals_cap;
    w89_limits *memories;
    w89_u32 nmems, mems_cap;
    w89_tabletype *tables;
    w89_u32 ntables, tables_cap;
    w89_u32 *funcs;
    w89_u32 nfuncs, funcs_cap;
    w89_u32 ndatas;
    w89_reftype *elems;
    w89_u32 nelems, elems_cap;
    w89_u32 *declared;
    w89_u32 ndeclared, declared_cap;
    w89_local *locals;
    w89_u32 nlocals, locals_cap;
    w89_label *labels;
    w89_u32 nlabels, labels_cap;
    const w89_vt *results;
    w89_u32 nresults;
    w89_vt *ostk;
    w89_u32 on, ocap;
    w89_u32 sbase;
    int ell;
} w89_valctx;

static const w89_vt w89_bot = { 0, 0, { 0, 0, 0, 0 } };

static int vt_is_bot(const w89_vt *v)
{
    int isr;
    isr = v->is_ref;
    if (isr != 0) {
        return 0;
    }
    isr = v->num;
    return isr == 0;
}

static int grow(void **pp, w89_u32 *cap, w89_u32 needed, size_t sz)
{
    w89_u32 nc;
    w89_u32 c;
    void *np;
    void *pv;
    size_t nn;
    c = *cap;
    if (c >= needed) {
        return 1;
    }
    if (c != 0) {
        nc = c * 2;
    } else {
        nc = 8;
    }
    while (nc < needed) {
        nc = nc * 2;
    }
    pv = *pp;
    nn = (size_t)nc;
    sz = nn * sz;
    np = realloc(pv, sz);
    if (np == 0) {
        return 0;
    }
    *pp = np;
    *cap = nc;
    return 1;
}

static int ctx_push(void **pp, w89_u32 *n, w89_u32 *cap, size_t sz,
                    const void *item, w89_err *err)
{
    int ok;
    size_t nn;
    w89_byte *dst;
    w89_byte *pv;
    w89_u32 nn32;
    nn = *n;
    nn = nn + 1;
    nn32 = (w89_u32)nn;
    ok = grow(pp, cap, nn32, sz);
    if (!ok) {
        *err = W89_ERR_OUT_OF_MEMORY;
        return 0;
    }
    nn = *n;
    nn = nn * sz;
    pv = *pp;
    dst = pv + nn;
    memcpy(dst, item, sz);
    nn = *n;
    nn = nn + 1;
    nn32 = (w89_u32)nn;
    *n = nn32;
    return 1;
}

static void ctx_init(w89_valctx *ctx)
{
    size_t nb;
    nb = sizeof(w89_valctx);
    memset(ctx, 0, nb);
}

static void ctx_free(w89_valctx *ctx)
{
    w89_typeenv *te;
    w89_u32 *p;
    w89_globaltype *gl;
    w89_limits *me;
    w89_tabletype *tb;
    w89_reftype *el;
    w89_local *lo;
    w89_label *la;
    w89_vt *os;
    size_t nb;
    te = &ctx->types;
    w89_typeenv_free(te);
    p = ctx->tags;
    free(p);
    gl = ctx->globals;
    free(gl);
    me = ctx->memories;
    free(me);
    tb = ctx->tables;
    free(tb);
    p = ctx->funcs;
    free(p);
    el = ctx->elems;
    free(el);
    p = ctx->declared;
    free(p);
    lo = ctx->locals;
    free(lo);
    la = ctx->labels;
    free(la);
    os = ctx->ostk;
    free(os);
    nb = sizeof(w89_valctx);
    memset(ctx, 0, nb);
}

/* ---------- Formatting ---------- */

static void app_str(char *buf, size_t nbuf, size_t *pos, const char *s)
{
    size_t i = 0;
    size_t p;
    size_t j;
    char c;
    p = *pos;
    while (1) {
        p = *pos;
        p = p + 1;
        if (p >= nbuf) {
            break;
        }
        c = s[i];
        if (c == '\0') {
            break;
        }
        p = *pos;
        j = s[i];
        buf[p] = j;
        p = *pos;
        p = p + 1;
        *pos = p;
        i = i + 1;
    }
    p = *pos;
    buf[p] = '\0';
}

static void app_num(char *buf, size_t nbuf, size_t *pos, w89_u32 v)
{
    char tmp[16];
    unsigned long ul;
    ul = (unsigned long)v;
    snprintf(tmp, sizeof(char[16]), "%lu", ul);
    app_str(buf, nbuf, pos, tmp);
}

static void fmt_heaptype(const w89_reftype *ht, char *buf, size_t nbuf)
{
    size_t pos = 0;
    w89_u32 ti;
    w89_u32 ab;
    buf[0] = '\0';
    ti = ht->is_typeidx;
    if (ti) {
        ti = ht->typeidx;
        app_num(buf, nbuf, &pos, ti);
        return;
    }
    ab = ht->abs;
    switch (ab) {
    case W89_HT_ANY: app_str(buf, nbuf, &pos, "any"); break;
    case W89_HT_NONE: app_str(buf, nbuf, &pos, "none"); break;
    case W89_HT_EQ: app_str(buf, nbuf, &pos, "eq"); break;
    case W89_HT_I31: app_str(buf, nbuf, &pos, "i31"); break;
    case W89_HT_STRUCT: app_str(buf, nbuf, &pos, "struct"); break;
    case W89_HT_ARRAY: app_str(buf, nbuf, &pos, "array"); break;
    case W89_HT_FUNC: app_str(buf, nbuf, &pos, "func"); break;
    case W89_HT_NOFUNC: app_str(buf, nbuf, &pos, "nofunc"); break;
    case W89_HT_EXTERN: app_str(buf, nbuf, &pos, "extern"); break;
    case W89_HT_NOEXTERN: app_str(buf, nbuf, &pos, "noextern"); break;
    case W89_HT_EXN: app_str(buf, nbuf, &pos, "exn"); break;
    case W89_HT_NOEXN: app_str(buf, nbuf, &pos, "noexn"); break;
    default: app_str(buf, nbuf, &pos, "something"); break;
    }
}

static void fmt_reftype(const w89_reftype *rt, char *buf, size_t nbuf)
{
    size_t pos = 0;
    char ht[48];
    w89_u32 nl;
    buf[0] = '\0';
    app_str(buf, nbuf, &pos, "(ref ");
    nl = rt->nullable;
    if (nl) {
        app_str(buf, nbuf, &pos, "null ");
    }
    fmt_heaptype(rt, ht, sizeof(char[48]));
    app_str(buf, nbuf, &pos, ht);
    app_str(buf, nbuf, &pos, ")");
}

static void fmt_valtype(const w89_vt *v, char *buf, size_t nbuf)
{
    size_t pos = 0;
    char rt[64];
    int bot;
    w89_u32 isr;
    w89_u32 nm;
    const w89_reftype *rp;
    buf[0] = '\0';
    bot = vt_is_bot(v);
    if (bot) {
        app_str(buf, nbuf, &pos, "bot");
        return;
    }
    isr = v->is_ref;
    if (isr) {
        rp = &v->rt;
        fmt_reftype(rp, rt, sizeof(char[64]));
        app_str(buf, nbuf, &pos, rt);
        return;
    }
    nm = v->num;
    switch (nm) {
    case 0x7F: app_str(buf, nbuf, &pos, "i32"); break;
    case 0x7E: app_str(buf, nbuf, &pos, "i64"); break;
    case 0x7D: app_str(buf, nbuf, &pos, "f32"); break;
    case 0x7C: app_str(buf, nbuf, &pos, "f64"); break;
    case 0x7B: app_str(buf, nbuf, &pos, "v128"); break;
    default: app_str(buf, nbuf, &pos, "?"); break;
    }
}

static void fmt_adj(const w89_vt *base, w89_u32 n, w89_u32 nbot,
                    char *buf, size_t nbuf)
{
    size_t pos = 0;
    w89_u32 i;
    char tmp[64];
    const w89_vt *p;
    buf[0] = '\0';
    app_str(buf, nbuf, &pos, "[");
    for (i = 0; i < nbot; i = i + 1) {
        if (i > 0) {
            app_str(buf, nbuf, &pos, " ");
        }
        app_str(buf, nbuf, &pos, "bot");
    }
    for (i = 0; i < n; i = i + 1) {
        if (i > 0) {
            app_str(buf, nbuf, &pos, " ");
        } else if (nbot > 0) {
            app_str(buf, nbuf, &pos, " ");
        }
        p = &base[i];
        fmt_valtype(p, tmp, sizeof(char[64]));
        app_str(buf, nbuf, &pos, tmp);
    }
    app_str(buf, nbuf, &pos, "]");
}

static void fmt_resulttype(const w89_vt *vs, w89_u32 n, char *buf, size_t nbuf)
{
    fmt_adj(vs, n, 0, buf, nbuf);
}

/* ---------- Lookups ---------- */

static const w89_subtype *type_of(w89_valctx *ctx, w89_u32 x, w89_err *err)
{
    w89_u32 nt;
    const w89_deftype *dt;
    const w89_subtype *st;
    unsigned long ul;
    nt = ctx->types.ntypes;
    if (x >= nt) {
        ul = (unsigned long)x;
        vmsg("unknown type %lu", ul);
        *err = W89_ERR_INVALID;
        return NULL;
    }
    dt = &ctx->types.types[x];
    st = dt->sub;
    return st;
}

static w89_err func_type_by_typeidx(w89_valctx *ctx, w89_u32 ti,
                                    const w89_vt **params, w89_u32 *nparams,
                                    const w89_vt **results, w89_u32 *nresults)
{
    const w89_subtype *st;
    w89_err e = W89_ERR_NONE;
    w89_u32 k;
    const w89_vt *p;
    w89_u32 n;
    unsigned long ul;
    st = type_of(ctx, ti, &e);
    if (st == 0) {
        return W89_ERR_INVALID;
    }
    k = st->kind;
    if (k != W89_CK_FUNC) {
        ul = (unsigned long)ti;
        vmsg("non-function type %lu", ul);
        return W89_ERR_INVALID;
    }
    p = st->ft.params;
    *params = p;
    n = st->ft.nparams;
    *nparams = n;
    p = st->ft.results;
    *results = p;
    n = st->ft.nresults;
    *nresults = n;
    return W89_ERR_NONE;
}

static w89_err func_type(w89_valctx *ctx, w89_u32 x,
                         const w89_vt **params, w89_u32 *nparams,
                         const w89_vt **results, w89_u32 *nresults)
{
    w89_u32 nf;
    w89_u32 fx;
    unsigned long ul;
    nf = ctx->nfuncs;
    if (x >= nf) {
        ul = (unsigned long)x;
        vmsg("unknown function %lu", ul);
        return W89_ERR_INVALID;
    }
    fx = ctx->funcs[x];
    return func_type_by_typeidx(ctx, fx, params, nparams,
                                results, nresults);
}

static w89_err table_at(w89_valctx *ctx, w89_u32 x, w89_tabletype **tt)
{
    w89_u32 n;
    w89_tabletype *t;
    unsigned long ul;
    n = ctx->ntables;
    if (x >= n) {
        ul = (unsigned long)x;
        vmsg("unknown table %lu", ul);
        return W89_ERR_INVALID;
    }
    t = &ctx->tables[x];
    *tt = t;
    return W89_ERR_NONE;
}

static w89_err memory_at(w89_valctx *ctx, w89_u32 x, w89_limits **lim)
{
    w89_u32 n;
    w89_limits *l;
    unsigned long ul;
    n = ctx->nmems;
    if (x >= n) {
        ul = (unsigned long)x;
        vmsg("unknown memory %lu", ul);
        return W89_ERR_INVALID;
    }
    l = &ctx->memories[x];
    *lim = l;
    return W89_ERR_NONE;
}

static w89_err global_at(w89_valctx *ctx, w89_u32 x, w89_globaltype **gt)
{
    w89_u32 n;
    w89_globaltype *g;
    unsigned long ul;
    n = ctx->nglobals;
    if (x >= n) {
        ul = (unsigned long)x;
        vmsg("unknown global %lu", ul);
        return W89_ERR_INVALID;
    }
    g = &ctx->globals[x];
    *gt = g;
    return W89_ERR_NONE;
}

static w89_err tag_type(w89_valctx *ctx, w89_u32 x, w89_u32 *ti)
{
    w89_u32 n;
    w89_u32 t;
    unsigned long ul;
    n = ctx->ntags;
    if (x >= n) {
        ul = (unsigned long)x;
        vmsg("unknown tag %lu", ul);
        return W89_ERR_INVALID;
    }
    t = ctx->tags[x];
    *ti = t;
    return W89_ERR_NONE;
}

static w89_err elem_at(w89_valctx *ctx, w89_u32 x, w89_reftype **rt)
{
    w89_u32 n;
    w89_reftype *r;
    unsigned long ul;
    n = ctx->nelems;
    if (x >= n) {
        ul = (unsigned long)x;
        vmsg("unknown elem segment %lu", ul);
        return W89_ERR_INVALID;
    }
    r = &ctx->elems[x];
    *rt = r;
    return W89_ERR_NONE;
}

static w89_err local_at(w89_valctx *ctx, w89_u32 x, w89_local **l)
{
    w89_u32 n;
    w89_local *lo;
    unsigned long ul;
    n = ctx->nlocals;
    if (x >= n) {
        ul = (unsigned long)x;
        vmsg("unknown local %lu", ul);
        return W89_ERR_INVALID;
    }
    lo = &ctx->locals[x];
    *l = lo;
    return W89_ERR_NONE;
}

static w89_err label_at(w89_valctx *ctx, w89_u32 x, w89_label **l)
{
    w89_u32 n;
    w89_label *la;
    unsigned long ul;
    n = ctx->nlabels;
    if (x >= n) {
        ul = (unsigned long)x;
        vmsg("unknown label %lu", ul);
        return W89_ERR_INVALID;
    }
    la = &ctx->labels[x];
    *l = la;
    return W89_ERR_NONE;
}

static int declared_contains(w89_valctx *ctx, w89_u32 x)
{
    w89_u32 i;
    w89_u32 nd;
    w89_u32 v;
    nd = ctx->ndeclared;
    for (i = 0; i < nd; i = i + 1) {
        v = ctx->declared[i];
        if (v == x) {
            return 1;
        }
    }
    return 0;
}

static void declared_add(w89_valctx *ctx, w89_u32 x)
{
    w89_err e = W89_ERR_NONE;
    int ok;
    void **pp;
    w89_u32 **pdeclared;
    w89_u32 *pndeclared;
    w89_u32 *pcap;
    ok = declared_contains(ctx, x);
    if (ok) {
        return;
    }
    pdeclared = &ctx->declared;
    pp = (void **)pdeclared;
    pndeclared = &ctx->ndeclared;
    pcap = &ctx->declared_cap;
    ctx_push(pp, pndeclared, pcap, sizeof(w89_u32), &x, &e);
}

/* ---------- Operand stack operations ---------- */

static int stk_append(w89_valctx *ctx, const w89_vt *vs, w89_u32 n,
                      w89_err *err)
{
    w89_vt *os;
    w89_u32 on;
    w89_u32 nn;
    size_t sz;
    int ok;
    void **pp;
    w89_vt **pos;
    w89_u32 *pocap;
    size_t nsz;
    w89_vt *pv;
    os = ctx->ostk;
    on = ctx->on;
    nn = on + n;
    sz = sizeof(w89_vt);
    pos = &ctx->ostk;
    pp = (void **)pos;
    pocap = &ctx->ocap;
    ok = grow(pp, pocap, nn, sz);
    if (!ok) {
        *err = W89_ERR_OUT_OF_MEMORY;
        return 0;
    }
    if (n != 0) {
        os = ctx->ostk;
        on = ctx->on;
        nn = on;
        nsz = (size_t)n;
        sz = nsz * sizeof(w89_vt);
        pv = &os[nn];
        memcpy(pv, vs, sz);
    }
    on = ctx->on;
    on = on + n;
    ctx->on = on;
    return 1;
}

static w89_vt *peek(w89_valctx *ctx, w89_u32 i)
{
    w89_u32 on, sb, idx;
    const w89_vt *cp;
    w89_vt *vp;
    on = ctx->on;
    sb = ctx->sbase;
    if (i >= on - sb) {
        cp = &w89_bot;
        vp = (w89_vt *)cp;
        return vp;
    }
    on = ctx->on;
    sb = ctx->sbase;
    idx = on - 1 - i;
    return &ctx->ostk[idx];
}

static w89_err peek_ref(w89_valctx *ctx, w89_u32 i, w89_reftype *out)
{
    w89_vt *v;
    w89_u32 isr;
    int bot;
    w89_reftype *rp;
    w89_reftype rv;
    size_t nb;
    char tmp[64];
    v = peek(ctx, i);
    isr = v->is_ref;
    if (isr) {
        rp = &v->rt;
        rv = *rp;
        *out = rv;
        return W89_ERR_NONE;
    }
    bot = vt_is_bot(v);
    if (bot) {
        nb = sizeof(w89_reftype);
        memset(out, 0, nb);
        out->abs = 0;
        return W89_ERR_NONE;
    }
    {
        fmt_valtype(v, tmp, sizeof(char[64]));
        vmsg("type mismatch: instruction requires reference type but stack "
             "has %s", tmp);
        return W89_ERR_INVALID;
    }
}

static w89_err pop_inputs(w89_valctx *ctx, int ell_ins, const w89_vt *ins,
                          w89_u32 nins)
{
    int ell2;
    w89_u32 n2, n1, n, n3;
    w89_u32 on, sb;
    w89_u32 j;
    const w89_vt *a;
    const w89_vt *b;
    w89_vt *os;
    w89_u32 idx;
    w89_typeenv *te;
    int m;
    char rb[160], sbuf[160];
    ell2 = ctx->ell;
    on = ctx->on;
    sb = ctx->sbase;
    n2 = on - sb;
    n1 = nins;
    if (n1 < n2) {
        n = n1;
    } else {
        n = n2;
    }
    if (ell2 != 0) {
        n3 = n1 - n;
    } else {
        n3 = 0;
    }
    if (n3 + n != n1) {
        fmt_resulttype(ins, n1, rb, sizeof(char[160]));
        os = ctx->ostk;
        sb = ctx->sbase;
        a = &os[sb];
        fmt_resulttype(a, n2, sbuf, sizeof(char[160]));
        vmsg("type mismatch: instruction requires %s but stack has %s",
             rb, sbuf);
        return W89_ERR_INVALID;
    }
    for (j = 0; j < n; j = j + 1) {
        os = ctx->ostk;
        on = ctx->on;
        sb = ctx->sbase;
        idx = sb + n2 - n + j;
        a = &os[idx];
        idx = j + n3;
        b = &ins[idx];
        te = &ctx->types;
        m = w89_match_valtype(te, a, b);
        if (!m) {
            fmt_resulttype(ins, n1, rb, sizeof(char[160]));
            os = ctx->ostk;
            sb = ctx->sbase;
            idx = sb + n2 - n;
            a = &os[idx];
            fmt_adj(a, n, n3, sbuf, sizeof(char[160]));
            vmsg("type mismatch: instruction requires %s but stack has %s",
                 rb, sbuf);
            return W89_ERR_INVALID;
        }
    }
    ctx->ell = ell2;
    if (ell_ins) {
        sb = ctx->sbase;
        ctx->on = sb;
    } else {
        on = ctx->sbase;
        on = on + (n2 - n);
        ctx->on = on;
    }
    return W89_ERR_NONE;
}

static w89_err push_outs(w89_valctx *ctx, int outs_ell, const w89_vt *outs,
                         w89_u32 nouts)
{
    w89_err e = W89_ERR_NONE;
    int ok;
    int ell;
    ell = ctx->ell;
    if (ell) {
        ctx->ell = W89_ELL_YES;
    } else if (outs_ell) {
        ctx->ell = W89_ELL_YES;
    }
    ok = stk_append(ctx, outs, nouts, &e);
    if (!ok) {
        return e;
    }
    return W89_ERR_NONE;
}

typedef struct w89_it {
    int ins_ell;
    const w89_vt *ins;
    w89_u32 nins;
    int outs_ell;
    const w89_vt *outs;
    w89_u32 nouts;
} w89_it;

static w89_err apply_it(w89_valctx *ctx, const w89_it *it)
{
    w89_err e;
    int ie;
    int oe;
    const w89_vt *ins;
    const w89_vt *outs;
    w89_u32 nin, nout;
    ie = it->ins_ell;
    ins = it->ins;
    nin = it->nins;
    e = pop_inputs(ctx, ie, ins, nin);
    if (e != W89_ERR_NONE) {
        return e;
    }
    oe = it->outs_ell;
    outs = it->outs;
    nout = it->nouts;
    return push_outs(ctx, oe, outs, nout);
}

static w89_err push_label(w89_valctx *ctx, const w89_vt *vts, w89_u32 len,
                          w89_err *err)
{
    int ok;
    w89_u32 nl;
    size_t sz;
    size_t sz2;
    w89_label *labels;
    w89_label **plabels;
    w89_u32 *pcap;
    w89_u32 nn;
    void **pp;
    w89_label *src;
    w89_label *dst;
    nl = ctx->nlabels;
    nn = nl + 1;
    plabels = &ctx->labels;
    pp = (void **)plabels;
    pcap = &ctx->labels_cap;
    sz = sizeof(w89_label);
    ok = grow(pp, pcap, nn, sz);
    if (!ok) {
        *err = W89_ERR_OUT_OF_MEMORY;
        return *err;
    }
    nl = ctx->nlabels;
    if (nl > 0) {
        labels = ctx->labels;
        nl = ctx->nlabels;
        sz2 = (size_t)nl;
        sz = sz2 * sizeof(w89_label);
        dst = &labels[1];
        src = &labels[0];
        memmove(dst, src, sz);
    }
    ctx->labels[0].vts = vts;
    ctx->labels[0].len = len;
    nl = ctx->nlabels;
    nl = nl + 1;
    ctx->nlabels = nl;
    return W89_ERR_NONE;
}

static void pop_label(w89_valctx *ctx)
{
    w89_u32 nl;
    size_t sz;
    size_t sz2;
    w89_label *labels;
    w89_label *src;
    w89_label *dst;
    nl = ctx->nlabels;
    if (nl > 0) {
        nl = ctx->nlabels;
        if (nl > 1) {
            labels = ctx->labels;
            nl = ctx->nlabels;
            nl = nl - 1;
            sz2 = (size_t)nl;
            sz = sz2 * sizeof(w89_label);
            src = &labels[1];
            dst = &labels[0];
            memmove(dst, src, sz);
        }
        nl = ctx->nlabels;
        nl = nl - 1;
        ctx->nlabels = nl;
    }
}

/* ---------- Instruction typing ---------- */

#define W89_SCR 512

typedef struct w89_it_scr {
    w89_vt items[W89_SCR];
    w89_u32 n;
} w89_it_scr;

static void scr_push(w89_it_scr *sc, const w89_vt *v)
{
    w89_u32 n;
    w89_vt tv;
    n = sc->n;
    if (n < W89_SCR) {
        tv = *v;
        sc->items[n] = tv;
        n = sc->n;
        n = n + 1;
        sc->n = n;
    }
}

static w89_vt num_vt_from_byte(w89_u32 num)
{
    w89_vt v;
    size_t nb;
    nb = sizeof(w89_vt);
    memset(&v, 0, nb);
    v.is_ref = 0;
    v.num = num;
    return v;
}

static int vt_defaultable(const w89_vt *v)
{
    int isr;
    isr = v->is_ref;
    if (isr) {
        isr = v->rt.nullable;
        return isr;
    }
    return 1;
}

static w89_err check_memop(w89_valctx *ctx, const w89_instr *in, int size)
{
    w89_limits *lim;
    w89_u32 natural = 0;
    w89_u32 limval;
    w89_u32 lv;
    w89_u32 sv;
    w89_err e;
    w89_u32 mx;
    w89_u32 align;
    w89_u64 off;
    while (1) {
        limval = natural + 1;
        lv = 1u << limval;
        lv = (w89_u32)lv;
        sv = (w89_u32)size;
        if (lv > sv) {
            break;
        }
        natural = natural + 1;
    }
    align = in->align;
    if (align > natural) {
        vmsg("alignment must not be larger than natural");
        return W89_ERR_INVALID;
    }
    mx = in->memidx;
    e = memory_at(ctx, mx, &lim);
    if (e != W89_ERR_NONE) {
        return e;
    }
    limval = lim->addr64;
    if (!limval) {
        off = in->offset;
        if (off >= 0x100000000UL) {
            vmsg("offset out of range");
            return W89_ERR_INVALID;
        }
    }
    return W89_ERR_NONE;
}

static w89_u32 addr_num(const w89_limits *lim)
{
    w89_u32 a64;
    a64 = lim->addr64;
    if (a64) {
        return 0x7E;
    }
    return 0x7F;
}

static w89_err check_blocktype(w89_valctx *ctx, w89_instr *in,
                               const w89_vt **ts1, w89_u32 *nts1,
                               const w89_vt **ts2, w89_u32 *nts2)
{
    w89_blocktype *bt;
    w89_u32 is_ti;
    w89_u32 isr, num;
    w89_u32 ti;
    w89_vt *vtp;
    unsigned long ul;
    bt = &in->bt;
    is_ti = bt->is_typeidx;
    if (is_ti) {
        ti = bt->typeidx;
        return func_type_by_typeidx(ctx, ti, ts1, nts1, ts2, nts2);
    }
    *ts1 = 0;
    *nts1 = 0;
    *ts2 = 0;
    *nts2 = 0;
    isr = bt->vt.is_ref;
    num = bt->vt.num;
    if (isr == 0) {
        if (num == 0) {
            return W89_ERR_NONE;
        }
    }
    isr = bt->vt.is_ref;
    if (isr) {
        is_ti = bt->vt.rt.is_typeidx;
        if (is_ti) {
            is_ti = bt->vt.rt.typeidx;
            num = ctx->types.ntypes;
            if (is_ti >= num) {
                is_ti = bt->vt.rt.typeidx;
                ul = (unsigned long)is_ti;
                vmsg("unknown type %lu", ul);
                return W89_ERR_INVALID;
            }
        }
    }
    isr = bt->vt.is_ref;
    num = bt->vt.num;
    if (!isr) {
        if (num >= 1) {
            if (num <= 5) {
                num = 0x7F - (num - 1);
                bt->vt.num = num;
            }
        }
    }
    vtp = &bt->vt;
    *ts2 = vtp;
    *nts2 = 1;
    return W89_ERR_NONE;
}

static w89_u32 check_instrs(w89_valctx *ctx, const w89_instr_vec *v,
                            w89_u32 i, w89_err *err);

/* Verify a block body's result stack (items in [base, full)) against the
 * block's expected result type ts2, mirroring check_block's pop + leftover
 * checks on the block-relative stack. */
static w89_err check_block_result(w89_valctx *ctx, w89_u32 base, w89_u32 full,
                                  const w89_vt *ts2, w89_u32 nts2)
{
    w89_u32 rel;
    w89_u32 j;
    int ell;
    w89_vt *os;
    const w89_vt *a;
    const w89_vt *b;
    w89_u32 idx;
    w89_typeenv *te;
    int m;
    char rb[160], sb[160];
    if (full >= base) {
        rel = full - base;
    } else {
        rel = 0;
    }
    if (rel <= nts2) {
        ell = ctx->ell;
        if (ell == W89_ELL_NO) {
            if (rel != nts2) {
                fmt_resulttype(ts2, nts2, rb, sizeof(char[160]));
                os = ctx->ostk;
                a = &os[base];
                fmt_resulttype(a, rel, sb, sizeof(char[160]));
                vmsg("type mismatch: instruction requires %s but stack has %s",
                     rb, sb);
                return W89_ERR_INVALID;
            }
        }
        for (j = 0; j < rel; j = j + 1) {
            os = ctx->ostk;
            idx = base + j;
            a = &os[idx];
            idx = nts2 - rel + j;
            b = &ts2[idx];
            te = &ctx->types;
            m = w89_match_valtype(te, a, b);
            if (!m) {
                fmt_resulttype(ts2, nts2, rb, sizeof(char[160]));
                os = ctx->ostk;
                idx = base;
                a = &os[idx];
                fmt_adj(a, rel, nts2 - rel, sb, sizeof(char[160]));
                vmsg("type mismatch: instruction requires %s but stack has %s",
                     rb, sb);
                return W89_ERR_INVALID;
            }
        }
        return W89_ERR_NONE;
    }
    for (j = 0; j < nts2; j = j + 1) {
        os = ctx->ostk;
        idx = full - nts2 + j;
        a = &os[idx];
        b = &ts2[j];
        te = &ctx->types;
        m = w89_match_valtype(te, a, b);
        if (!m) {
            fmt_resulttype(ts2, nts2, rb, sizeof(char[160]));
            os = ctx->ostk;
            idx = full - nts2;
            a = &os[idx];
            fmt_resulttype(a, nts2, sb, sizeof(char[160]));
            vmsg("type mismatch: instruction requires %s but stack has %s",
                 rb, sb);
            return W89_ERR_INVALID;
        }
    }
    {
        fmt_resulttype(ts2, nts2, rb, sizeof(char[160]));
        os = ctx->ostk;
        a = &os[base];
        fmt_resulttype(a, rel, sb, sizeof(char[160]));
        vmsg("type mismatch: block requires %s but stack has %s", rb, sb);
        return W89_ERR_INVALID;
    }
}

/* Check one block body. Pushes the block's label, runs the body, pops the
 * label, and verifies the result against ts2. The terminator (0x05 or
 * 0x0B) is not consumed; *is_else reports an 0x05. The outer stack is
 * restored on exit. */
static w89_err check_block_body(w89_valctx *ctx, const w89_instr_vec *v,
                                w89_u32 start, const w89_vt *ts1,
                                w89_u32 nts1, const w89_vt *ts2,
                                w89_u32 nts2, int loop_label,
                                w89_u32 *term, int *is_else)
{
    w89_u32 base;
    int saved_ell;
    w89_u32 saved_sbase;
    w89_u32 full;
    w89_err e = W89_ERR_NONE;
    w89_u32 k;
    w89_u32 *saved_init = 0;
    w89_u32 j;
    const w89_vt *lbl;
    w89_u32 nlbl;
    w89_u32 nl;
    w89_u32 nn;
    size_t sz;
    size_t sz2;
    w89_u32 *init;
    w89_local *lo;
    w89_vt *os;
    const w89_instr *itm;
    w89_instr *arritems;
    int ok;
    void **pp;
    w89_vt **pos;
    w89_u32 *pocap;
    w89_vt *pv;
    base = ctx->on;
    saved_ell = ctx->ell;
    saved_sbase = ctx->sbase;
    if (loop_label) {
        lbl = ts1;
        nlbl = nts1;
    } else {
        lbl = ts2;
        nlbl = nts2;
    }
    nl = ctx->nlocals;
    if (nl != 0) {
        sz2 = (size_t)nl;
        sz = sz2 * sizeof(w89_u32);
        saved_init = malloc(sz);
        if (saved_init == 0) {
            return W89_ERR_OUT_OF_MEMORY;
        }
        for (j = 0; j < nl; j = j + 1) {
            lo = &ctx->locals[j];
            init = &saved_init[j];
            nn = lo->init;
            *init = nn;
        }
    }
    nn = base + nts1;
    sz = sizeof(w89_vt);
    pos = &ctx->ostk;
    pp = (void **)pos;
    pocap = &ctx->ocap;
    ok = grow(pp, pocap, nn, sz);
    if (!ok) {
        free(saved_init);
        return W89_ERR_OUT_OF_MEMORY;
    }
    if (nts1 != 0) {
        os = ctx->ostk;
        sz2 = (size_t)nts1;
        sz = sz2 * sizeof(w89_vt);
        pv = &os[base];
        memcpy(pv, ts1, sz);
    }
    nn = base + nts1;
    ctx->on = nn;
    ctx->ell = W89_ELL_NO;
    ctx->sbase = base;
    e = push_label(ctx, lbl, nlbl, &e);
    if (e != W89_ERR_NONE) {
        ctx->sbase = saved_sbase;
        free(saved_init);
        return e;
    }
    k = check_instrs(ctx, v, start, &e);
    if (e != W89_ERR_NONE) {
        ctx->sbase = saved_sbase;
        free(saved_init);
        return e;
    }
    nn = v->n;
    if (k < nn) {
        arritems = v->items;
        itm = &arritems[k];
        nl = itm->op;
        if (nl != 0x05) {
            if (nl != 0x0B) {
                vmsg("END opcode expected");
                ctx->sbase = saved_sbase;
                free(saved_init);
                return W89_ERR_EOF_SECTION;
            }
        }
    }
    pop_label(ctx);
    *term = k;
    nn = v->n;
    if (k < nn) {
        arritems = v->items;
        itm = &arritems[k];
        nl = itm->op;
        if (nl == 0x05) {
            *is_else = 1;
        } else {
            *is_else = 0;
        }
    } else {
        *is_else = 0;
    }
    full = ctx->on;
    e = check_block_result(ctx, base, full, ts2, nts2);
    if (e != W89_ERR_NONE) {
        ctx->sbase = saved_sbase;
        free(saved_init);
        return e;
    }
    ctx->on = base;
    ctx->ell = saved_ell;
    ctx->sbase = saved_sbase;
    if (saved_init != 0) {
        nl = ctx->nlocals;
        for (j = 0; j < nl; j = j + 1) {
            lo = &ctx->locals[j];
            init = &saved_init[j];
            nn = *init;
            lo->init = nn;
        }
        free(saved_init);
    }
    return W89_ERR_NONE;
}

static w89_err catch_body(w89_valctx *ctx, const w89_catch *cc,
                          w89_it_scr *sc)
{
    w89_err e;
    w89_u32 ti;
    const w89_vt *params;
    w89_u32 nparams;
    const w89_vt *dummy;
    w89_u32 ndummy;
    w89_u32 p;
    w89_u32 tg;
    const w89_vt *v;
    tg = cc->tagidx;
    e = tag_type(ctx, tg, &ti);
    if (e != W89_ERR_NONE) {
        return e;
    }
    e = func_type_by_typeidx(ctx, ti, &params, &nparams, &dummy, &ndummy);
    if (e != W89_ERR_NONE) {
        return e;
    }
    for (p = 0; p < nparams; p = p + 1) {
        v = &params[p];
        scr_push(sc, v);
    }
    return W89_ERR_NONE;
}

static w89_err check_catch(w89_valctx *ctx, const w89_catch *cc)
{
    w89_err e;
    w89_label *lb;
    w89_it_scr sc;
    w89_u32 ck;
    w89_u32 lbidx;
    w89_u32 n;
    w89_u32 len;
    w89_vt exn;
    size_t nb;
    int m;
    w89_typeenv *te;
    const w89_vt *cvp;
    const w89_vt *pitms;
    char rb[160], sb[160];
    sc.n = 0;
    lbidx = cc->label;
    e = label_at(ctx, lbidx, &lb);
    if (e != W89_ERR_NONE) {
        return e;
    }
    ck = cc->kind;
    if (ck == 0x00) {
        e = catch_body(ctx, cc, &sc);
        if (e != W89_ERR_NONE) {
            return e;
        }
    } else if (ck == 0x01) {
        e = catch_body(ctx, cc, &sc);
        if (e != W89_ERR_NONE) {
            return e;
        }
        nb = sizeof(w89_vt);
        memset(&exn, 0, nb);
        exn.is_ref = 1;
        exn.rt.nullable = 0;
        exn.rt.abs = W89_HT_EXN;
        scr_push(&sc, &exn);
    } else if (ck == 0x02) {
        sc.n = 0;
    } else {
        nb = sizeof(w89_vt);
        memset(&exn, 0, nb);
        exn.is_ref = 1;
        exn.rt.nullable = 0;
        exn.rt.abs = W89_HT_EXN;
        sc.n = 0;
        scr_push(&sc, &exn);
    }
    te = &ctx->types;
    n = sc.n;
    cvp = lb->vts;
    len = lb->len;
    pitms = sc.items;
    m = w89_match_resulttype(te, pitms, n, cvp, len);
    if (!m) {
        fmt_resulttype(cvp, len, rb, sizeof(char[160]));
        fmt_resulttype(pitms, n, sb, sizeof(char[160]));
        vmsg("type mismatch: catch handler requires %s but label has %s",
             rb, sb);
        return W89_ERR_INVALID;
    }
    return W89_ERR_NONE;
}

typedef struct w89_cvt {
    w89_u32 op;
    w89_u32 in;
    w89_u32 out;
} w89_cvt;

static const w89_cvt w89_conv_tab[] = {
    { 0xA7, 0x7E, 0x7F }, { 0xA8, 0x7D, 0x7F }, { 0xA9, 0x7D, 0x7F },
    { 0xAA, 0x7C, 0x7F }, { 0xAB, 0x7C, 0x7F }, { 0xAC, 0x7F, 0x7E },
    { 0xAD, 0x7F, 0x7E }, { 0xAE, 0x7D, 0x7E }, { 0xAF, 0x7D, 0x7E },
    { 0xB0, 0x7C, 0x7E }, { 0xB1, 0x7C, 0x7E }, { 0xB2, 0x7F, 0x7D },
    { 0xB3, 0x7F, 0x7D }, { 0xB4, 0x7E, 0x7D }, { 0xB5, 0x7E, 0x7D },
    { 0xB6, 0x7C, 0x7D }, { 0xB7, 0x7F, 0x7C }, { 0xB8, 0x7F, 0x7C },
    { 0xB9, 0x7E, 0x7C }, { 0xBA, 0x7E, 0x7C }, { 0xBB, 0x7D, 0x7C },
    { 0xBC, 0x7D, 0x7F }, { 0xBD, 0x7C, 0x7E }, { 0xBE, 0x7F, 0x7D },
    { 0xBF, 0x7E, 0x7C }
};

static const w89_cvt w89_tsat_tab[] = {
    { 0x00, 0x7D, 0x7F }, { 0x01, 0x7D, 0x7F }, { 0x02, 0x7C, 0x7F },
    { 0x03, 0x7C, 0x7F }, { 0x04, 0x7D, 0x7E }, { 0x05, 0x7D, 0x7E },
    { 0x06, 0x7C, 0x7E }, { 0x07, 0x7C, 0x7E }
};

static const w89_cvt w89_extend_tab[] = {
    { 0xC0, 0x7F, 0x7F }, { 0xC1, 0x7F, 0x7F }, { 0xC2, 0x7E, 0x7E },
    { 0xC3, 0x7E, 0x7E }, { 0xC4, 0x7E, 0x7E }
};

static int range_match(w89_byte op, w89_u32 lo, w89_u32 hi)
{
    if (op < lo) {
        return 0;
    }
    if (op > hi) {
        return 0;
    }
    return 1;
}

static int numeric_optype(w89_byte op, w89_u32 *num, int *kind,
                          const w89_cvt **cvt)
{
    w89_u32 idx;
    const w89_cvt *c;
    int rm;
    if (op == 0x45) {
        *num = 0x7F;
        *kind = 0;
        return 1;
    }
    if (op == 0x50) {
        *num = 0x7E;
        *kind = 0;
        return 1;
    }
    rm = range_match(op, 0x46, 0x4F);
    if (rm) {
        *num = 0x7F;
        *kind = 1;
        return 1;
    }
    rm = range_match(op, 0x51, 0x5A);
    if (rm) {
        *num = 0x7E;
        *kind = 1;
        return 1;
    }
    rm = range_match(op, 0x5B, 0x60);
    if (rm) {
        *num = 0x7D;
        *kind = 1;
        return 1;
    }
    rm = range_match(op, 0x61, 0x66);
    if (rm) {
        *num = 0x7C;
        *kind = 1;
        return 1;
    }
    rm = range_match(op, 0x67, 0x69);
    if (rm) {
        *num = 0x7F;
        *kind = 2;
        return 1;
    }
    rm = range_match(op, 0x6A, 0x78);
    if (rm) {
        *num = 0x7F;
        *kind = 3;
        return 1;
    }
    rm = range_match(op, 0x79, 0x7B);
    if (rm) {
        *num = 0x7E;
        *kind = 2;
        return 1;
    }
    rm = range_match(op, 0x7C, 0x8A);
    if (rm) {
        *num = 0x7E;
        *kind = 3;
        return 1;
    }
    rm = range_match(op, 0x8B, 0x91);
    if (rm) {
        *num = 0x7D;
        *kind = 2;
        return 1;
    }
    rm = range_match(op, 0x92, 0x98);
    if (rm) {
        *num = 0x7D;
        *kind = 3;
        return 1;
    }
    rm = range_match(op, 0x99, 0x9F);
    if (rm) {
        *num = 0x7C;
        *kind = 2;
        return 1;
    }
    rm = range_match(op, 0xA0, 0xA6);
    if (rm) {
        *num = 0x7C;
        *kind = 3;
        return 1;
    }
    rm = range_match(op, 0xA7, 0xBF);
    if (rm) {
        idx = op - 0xA7;
        c = &w89_conv_tab[idx];
        *cvt = c;
        *kind = 4;
        return 1;
    }
    rm = range_match(op, 0xC0, 0xC4);
    if (rm) {
        idx = op - 0xC0;
        c = &w89_extend_tab[idx];
        *cvt = c;
        *kind = 4;
        return 1;
    }
    return 0;
}

static w89_u32 check_instr(w89_valctx *ctx, const w89_instr_vec *v,
                           w89_u32 i, w89_err *err)
{
    w89_instr *in;
    w89_u32 op, num;
    int kind;
    const w89_cvt *cvt;
    w89_it_scr sc;
    w89_err e = W89_ERR_NONE;
    w89_vt t, t2;
    w89_vt i32v;
    w89_vt addr;
    w89_u32 j;
    w89_it it;
    w89_instr *arr;
    w89_vt *vp;
    const w89_vt *cvp;
    w89_vt tv;
    w89_vt *pv;
    int bot;
    w89_u32 isr;
    w89_u32 ti;
    w89_u32 n;
    w89_u32 nn;
    unsigned long ul;

    arr = v->items;
    in = &arr[i];
    op = in->op;
    i32v = num_vt_from_byte(0x7F);

    sc.n = 0;
    memset(&it, 0, sizeof(w89_it));
    *err = W89_ERR_NONE;

    switch (op) {
    case 0x00:
        it.ins_ell = W89_ELL_YES;
        it.outs_ell = W89_ELL_YES;
        break;
    case 0x01:
        break;
    case 0x1A:
        vp = peek(ctx, 0);
        it.ins = vp;
        it.nins = 1;
        break;
    case 0x1B:
        vp = peek(ctx, 1);
        tv = *vp;
        bot = vt_is_bot(&tv);
        if (!bot) {
            isr = tv.is_ref;
            if (isr) {
                char tmp[64];
                fmt_valtype(&tv, tmp, sizeof(char[64]));
                vmsg("type mismatch: instruction requires numeric or vector "
                     "type but stack has %s", tmp);
                *err = W89_ERR_INVALID; return i;
            }
        }
        scr_push(&sc, &tv);
        scr_push(&sc, &tv);
        scr_push(&sc, &i32v);
        cvp = sc.items;
        it.ins = cvp;
        n = sc.n;
        it.nins = n;
        cvp = sc.items;
        it.outs = cvp;
        it.nouts = 1;
        break;
    case 0x1C:
        n = in->n;
        if (n != 1) {
            vmsg("invalid result arity other than 1 is not (yet) allowed");
            *err = W89_ERR_INVALID; return i;
        }
        pv = &in->seltypes[0];
        isr = pv->is_ref;
        if (isr) {
            ti = pv->rt.is_typeidx;
            if (ti) {
                ti = pv->rt.typeidx;
                n = ctx->types.ntypes;
                if (ti >= n) {
                    ti = pv->rt.typeidx;
                    ul = (unsigned long)ti;
                    vmsg("unknown type %lu", ul);
                    *err = W89_ERR_INVALID; return i;
                }
            }
        }
        t = *pv;
        scr_push(&sc, &t);
        scr_push(&sc, &t);
        scr_push(&sc, &i32v);
        cvp = sc.items;
        it.ins = cvp;
        it.nins = 3;
        cvp = sc.items;
        it.outs = cvp;
        it.nouts = 1;
        break;
    case 0x0C: {
        w89_label *lb;
        w89_u32 ix;
        ix = in->idx;
        e = label_at(ctx, ix, &lb);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        it.ins_ell = W89_ELL_YES;
        it.outs_ell = W89_ELL_YES;
        cvp = lb->vts;
        it.ins = cvp;
        n = lb->len;
        it.nins = n;
        break;
    }
    case 0x0D: {
        w89_label *lb;
        w89_u32 ix;
        w89_u32 len;
        const w89_vt *itm;
        ix = in->idx;
        e = label_at(ctx, ix, &lb);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        len = lb->len;
        for (j = 0; j < len; j = j + 1) {
            itm = &lb->vts[j];
            scr_push(&sc, itm);
        }
        scr_push(&sc, &i32v);
        cvp = sc.items;
        it.ins = cvp;
        n = sc.n;
        it.nins = n;
        cvp = lb->vts;
        it.outs = cvp;
        n = lb->len;
        it.nouts = n;
        break;
    }
    case 0x0E: {
        w89_label *lb;
        w89_u32 n;
        w89_u32 k;
        w89_u32 ix;
        w89_u32 fail;
        w89_u32 li;
        w89_u32 len2;
        w89_vt *pv2;
        const w89_vt *l2vt;
        w89_label *l2;
        w89_typeenv *te;
        int m;
        char rb[160], sb[160];
        ix = in->idx;
        e = label_at(ctx, ix, &lb);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        n = lb->len;
        for (j = 0; j < n; j = j + 1) {
            vp = peek(ctx, n - j);
            scr_push(&sc, vp);
        }
        {
            fail = 0;
            nn = in->n;
            for (k = 0; k <= nn; k = k + 1) {
                if (k == nn) {
                    li = in->idx;
                } else {
                    li = in->labels[k];
                }
                e = label_at(ctx, li, &l2);
                if (e != W89_ERR_NONE) {
                    *err = W89_ERR_INVALID; return i;
                }
                len2 = l2->len;
                if (len2 != n) {
                    fail = 1;
                } else {
                    te = &ctx->types;
                    for (j = 0; j < n; j = j + 1) {
                        pv2 = &sc.items[j];
                        l2vt = &l2->vts[j];
                        m = w89_match_valtype(te, pv2, l2vt);
                        if (!m) {
                            fail = 1;
                            break;
                        }
                    }
                }
                if (fail) {
                    l2vt = l2->vts;
                    len2 = l2->len;
                    cvp = sc.items;
                    fmt_resulttype(l2vt, len2, rb, sizeof(char[160]));
                    fmt_resulttype(cvp, n, sb, sizeof(char[160]));
                    vmsg("type mismatch: instruction requires %s but stack "
                         "has %s", rb, sb);
                    *err = W89_ERR_INVALID; return i;
                }
                fail = 0;
            }
        }
        scr_push(&sc, &i32v);
        it.ins_ell = W89_ELL_YES;
        it.outs_ell = W89_ELL_YES;
        cvp = sc.items;
        it.ins = cvp;
        nn = n + 1;
        it.nins = nn;
        break;
    }
    case 0x0F:
        it.ins_ell = W89_ELL_YES;
        it.outs_ell = W89_ELL_YES;
        cvp = ctx->results;
        it.ins = cvp;
        n = ctx->nresults;
        it.nins = n;
        break;
    case 0x08: {
        w89_u32 ti;
        w89_u32 ix;
        const w89_vt *params;
        w89_u32 nparams;
        const w89_vt *dummy;
        w89_u32 ndummy;
        ix = in->idx;
        e = tag_type(ctx, ix, &ti);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        e = func_type_by_typeidx(ctx, ti, &params, &nparams, &dummy, &ndummy);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        it.ins_ell = W89_ELL_YES;
        it.outs_ell = W89_ELL_YES;
        cvp = params;
        it.ins = cvp;
        it.nins = nparams;
        break;
    }
    case 0x0A:
        t.is_ref = 1;
        t.rt.nullable = 1;
        t.rt.is_typeidx = 0;
        t.rt.abs = W89_HT_EXN;
        it.ins_ell = W89_ELL_YES;
        it.outs_ell = W89_ELL_YES;
        it.ins = &t;
        it.nins = 1;
        break;
    case 0x10: {
        const w89_vt *params;
        w89_u32 nparams;
        const w89_vt *results;
        w89_u32 nresults;
        w89_u32 ix;
        ix = in->idx;
        e = func_type(ctx, ix, &params, &nparams, &results, &nresults);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        cvp = params;
        it.ins = cvp;
        it.nins = nparams;
        cvp = results;
        it.outs = cvp;
        it.nouts = nresults;
        break;
    }
    case 0x11: {
        w89_tabletype *tt;
        const w89_vt *params;
        w89_u32 nparams;
        const w89_vt *results;
        w89_u32 nresults;
        w89_reftype funcref;
        w89_reftype *trt;
        w89_typeenv *te;
        w89_limits *tlim;
        char rb[64];
        w89_u32 ix;
        w89_u32 an;
        int m;
        size_t nb;
        ix = in->idx2;
        e = func_type_by_typeidx(ctx, ix, &params, &nparams,
                                 &results, &nresults);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        ix = in->idx;
        e = table_at(ctx, ix, &tt);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        nb = sizeof(w89_reftype);
        memset(&funcref, 0, nb);
        funcref.nullable = 1;
        funcref.abs = W89_HT_FUNC;
        te = &ctx->types;
        trt = &tt->rt;
        m = w89_match_reftype(te, trt, &funcref);
        if (!m) {
            fmt_reftype(trt, rb, sizeof(char[64]));
            vmsg("type mismatch: instruction requires table of function type "
                 "but table has element type %s", rb);
            *err = W89_ERR_INVALID; return i;
        }
        tlim = &tt->limits;
        an = addr_num(tlim);
        addr = num_vt_from_byte(an);
        for (j = 0; j < nparams; j = j + 1) {
            cvp = &params[j];
            scr_push(&sc, cvp);
        }
        scr_push(&sc, &addr);
        cvp = sc.items;
        it.ins = cvp;
        n = sc.n;
        it.nins = n;
        it.outs = results;
        it.nouts = nresults;
        break;
    }
    case 0x12: {
        const w89_vt *params;
        w89_u32 nparams;
        const w89_vt *results;
        w89_u32 nresults;
        w89_u32 ix;
        w89_typeenv *te;
        const w89_vt *cr;
        w89_u32 cnr;
        int m;
        char rb[160], sb[160];
        ix = in->idx;
        e = func_type(ctx, ix, &params, &nparams, &results, &nresults);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        te = &ctx->types;
        cr = ctx->results;
        cnr = ctx->nresults;
        m = w89_match_resulttype(te, results, nresults, cr, cnr);
        if (!m) {
            fmt_resulttype(cr, cnr, rb, sizeof(char[160]));
            fmt_resulttype(results, nresults, sb, sizeof(char[160]));
            vmsg("type mismatch: current function requires result type %s "
                 "but callee returns %s", rb, sb);
            *err = W89_ERR_INVALID; return i;
        }
        it.ins_ell = W89_ELL_YES;
        it.outs_ell = W89_ELL_YES;
        cvp = params;
        it.ins = cvp;
        it.nins = nparams;
        break;
    }
    case 0x13: {
        w89_tabletype *tt;
        const w89_vt *params;
        w89_u32 nparams;
        const w89_vt *results;
        w89_u32 nresults;
        w89_reftype funcref;
        w89_reftype *trt;
        w89_typeenv *te;
        const w89_vt *cr;
        w89_u32 cnr;
        w89_limits *tlim;
        char rb[64];
        char rb2[160], sb2[160];
        w89_u32 ix;
        w89_u32 an;
        int m;
        size_t nb;
        ix = in->idx2;
        e = func_type_by_typeidx(ctx, ix, &params, &nparams,
                                 &results, &nresults);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        ix = in->idx;
        e = table_at(ctx, ix, &tt);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        nb = sizeof(w89_reftype);
        memset(&funcref, 0, nb);
        funcref.nullable = 1;
        funcref.abs = W89_HT_FUNC;
        te = &ctx->types;
        trt = &tt->rt;
        m = w89_match_reftype(te, trt, &funcref);
        if (!m) {
            fmt_reftype(trt, rb, sizeof(char[64]));
            vmsg("type mismatch: instruction requires table of function type "
                 "but table has element type %s", rb);
            *err = W89_ERR_INVALID; return i;
        }
        cr = ctx->results;
        cnr = ctx->nresults;
        m = w89_match_resulttype(te, results, nresults, cr, cnr);
        if (!m) {
            fmt_resulttype(cr, cnr, rb2, sizeof(char[160]));
            fmt_resulttype(results, nresults, sb2, sizeof(char[160]));
            vmsg("type mismatch: current function requires result type %s "
                 "but callee returns %s", rb2, sb2);
            *err = W89_ERR_INVALID; return i;
        }
        tlim = &tt->limits;
        an = addr_num(tlim);
        addr = num_vt_from_byte(an);
        for (j = 0; j < nparams; j = j + 1) {
            cvp = &params[j];
            scr_push(&sc, cvp);
        }
        scr_push(&sc, &addr);
        it.ins_ell = W89_ELL_YES;
        it.outs_ell = W89_ELL_YES;
        cvp = sc.items;
        it.ins = cvp;
        n = sc.n;
        it.nins = n;
        break;
    }
    case 0x14: {
        const w89_vt *params;
        w89_u32 nparams;
        const w89_vt *results;
        w89_u32 nresults;
        w89_vt ref;
        w89_u32 ix;
        size_t nb;
        ix = in->idx;
        e = func_type_by_typeidx(ctx, ix, &params, &nparams,
                                 &results, &nresults);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        nb = sizeof(w89_vt);
        memset(&ref, 0, nb);
        ref.is_ref = 1;
        ref.rt.nullable = 1;
        ref.rt.is_typeidx = 1;
        ref.rt.typeidx = ix;
        for (j = 0; j < nparams; j = j + 1) {
            cvp = &params[j];
            scr_push(&sc, cvp);
        }
        scr_push(&sc, &ref);
        cvp = sc.items;
        it.ins = cvp;
        n = sc.n;
        it.nins = n;
        cvp = results;
        it.outs = cvp;
        it.nouts = nresults;
        break;
    }
    case 0x15: {
        const w89_vt *params;
        w89_u32 nparams;
        const w89_vt *results;
        w89_u32 nresults;
        w89_vt ref;
        w89_u32 ix;
        w89_typeenv *te;
        const w89_vt *cr;
        w89_u32 cnr;
        int m;
        size_t nb;
        char rb[160], sb[160];
        ix = in->idx;
        e = func_type_by_typeidx(ctx, ix, &params, &nparams,
                                 &results, &nresults);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        te = &ctx->types;
        cr = ctx->results;
        cnr = ctx->nresults;
        m = w89_match_resulttype(te, results, nresults, cr, cnr);
        if (!m) {
            fmt_resulttype(cr, cnr, rb, sizeof(char[160]));
            fmt_resulttype(results, nresults, sb, sizeof(char[160]));
            vmsg("type mismatch: current function requires result type %s "
                 "but callee returns %s", rb, sb);
            *err = W89_ERR_INVALID; return i;
        }
        nb = sizeof(w89_vt);
        memset(&ref, 0, nb);
        ref.is_ref = 1;
        ref.rt.nullable = 1;
        ref.rt.is_typeidx = 1;
        ref.rt.typeidx = ix;
        for (j = 0; j < nparams; j = j + 1) {
            cvp = &params[j];
            scr_push(&sc, cvp);
        }
        scr_push(&sc, &ref);
        it.ins_ell = W89_ELL_YES;
        it.outs_ell = W89_ELL_YES;
        cvp = sc.items;
        it.ins = cvp;
        n = sc.n;
        it.nins = n;
        break;
    }
    case 0x02:
    case 0x03:
    case 0x04: {
        const w89_vt *ts1;
        w89_u32 nts1;
        const w89_vt *ts2;
        w89_u32 nts2;
        w89_u32 term;
        int is_else = 0;
        int is_loop;
        int is_else2;
        w89_u32 ip1;
        if (op == 0x03) {
            is_loop = 1;
        } else {
            is_loop = 0;
        }
        e = check_blocktype(ctx, in, &ts1, &nts1, &ts2, &nts2);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        if (op == 0x04) {
            ip1 = i + 1;
            e = check_block_body(ctx, v, ip1, ts1, nts1, ts2, nts2,
                                 is_loop, &term, &is_else);
            if (e != W89_ERR_NONE) {
                *err = W89_ERR_INVALID; return i;
            }
            if (is_else) {
                is_else2 = 0;
                ip1 = term + 1;
                e = check_block_body(ctx, v, ip1, ts1, nts1, ts2, nts2,
                                     is_loop, &term, &is_else2);
                if (e != W89_ERR_NONE) {
                    *err = W89_ERR_INVALID; return i;
                }
                if (is_else2) {
                    vmsg("END opcode expected");
                    *err = W89_ERR_INVALID; return i;
                }
            } else {
                is_else2 = 0;
                e = check_block_body(ctx, v, term, ts1, nts1, ts2, nts2,
                                     is_loop, &term, &is_else2);
                if (e != W89_ERR_NONE) {
                    *err = W89_ERR_INVALID; return i;
                }
            }
            i = term;
        } else {
            ip1 = i + 1;
            e = check_block_body(ctx, v, ip1, ts1, nts1, ts2, nts2,
                                 is_loop, &term, &is_else);
            if (e != W89_ERR_NONE) {
                *err = W89_ERR_INVALID; return i;
            }
            if (is_else) {
                vmsg("END opcode expected");
                *err = W89_ERR_INVALID; return i;
            }
            i = term;
        }
        cvp = ts1;
        it.ins = cvp;
        it.nins = nts1;
        cvp = ts2;
        it.outs = cvp;
        it.nouts = nts2;
        if (op == 0x04) {
            for (j = 0; j < nts1; j = j + 1) {
                cvp = &ts1[j];
                scr_push(&sc, cvp);
            }
            scr_push(&sc, &i32v);
            cvp = sc.items;
            it.ins = cvp;
            nn = nts1 + 1;
            it.nins = nn;
        }
        break;
    }
    case 0x1F: {
        const w89_vt *ts1;
        w89_u32 nts1;
        const w89_vt *ts2;
        w89_u32 nts2;
        w89_u32 term;
        int is_else = 0;
        w89_u32 inn;
        w89_u32 ip1;
        w89_catch *cc;
        inn = in->n;
        e = check_blocktype(ctx, in, &ts1, &nts1, &ts2, &nts2);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        for (j = 0; j < inn; j = j + 1) {
            cc = &in->catches[j];
            e = check_catch(ctx, cc);
            if (e != W89_ERR_NONE) {
                *err = W89_ERR_INVALID; return i;
            }
        }
        ip1 = i + 1;
        e = check_block_body(ctx, v, ip1, ts1, nts1, ts2, nts2, 0,
                             &term, &is_else);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        if (is_else) {
            vmsg("END opcode expected");
            *err = W89_ERR_INVALID; return i;
        }
        i = term;
        cvp = ts1;
        it.ins = cvp;
        it.nins = nts1;
        cvp = ts2;
        it.outs = cvp;
        it.nouts = nts2;
        break;
    }
    case 0x05:
    case 0x0B:
        vmsg("END opcode expected");
        *err = W89_ERR_INVALID; return i;
    case 0x20: {
        w89_local *l;
        w89_u32 ix;
        w89_vt *vtp;
        ix = in->idx;
        e = local_at(ctx, ix, &l);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        n = l->init;
        if (!n) {
            vmsg("uninitialized local");
            *err = W89_ERR_INVALID; return i;
        }
        vtp = &l->vt;
        it.outs = vtp;
        it.nouts = 1;
        break;
    }
    case 0x21:
    case 0x22: {
        w89_local *l;
        w89_u32 ix;
        w89_vt *vtp;
        ix = in->idx;
        e = local_at(ctx, ix, &l);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        l->init = 1;
        vtp = &l->vt;
        if (op == 0x22) {
            it.ins = vtp;
            it.nins = 1;
            it.outs = vtp;
            it.nouts = 1;
        } else {
            it.ins = vtp;
            it.nins = 1;
        }
        break;
    }
    case 0x23: {
        w89_globaltype *gt;
        w89_u32 ix;
        w89_vt *vtp;
        ix = in->idx;
        e = global_at(ctx, ix, &gt);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        vtp = &gt->vt;
        it.outs = vtp;
        it.nouts = 1;
        break;
    }
    case 0x24: {
        w89_globaltype *gt;
        w89_u32 ix;
        w89_vt *vtp;
        ix = in->idx;
        e = global_at(ctx, ix, &gt);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        n = gt->mut;
        if (!n) {
            vmsg("immutable global");
            *err = W89_ERR_INVALID; return i;
        }
        vtp = &gt->vt;
        it.ins = vtp;
        it.nins = 1;
        break;
    }
    case 0x25: {
        w89_tabletype *tt;
        w89_vt ref;
        w89_u32 ix;
        w89_u32 an;
        w89_reftype *trt;
        w89_limits *tlim;
        w89_reftype rv;
        size_t nb;
        ix = in->idx;
        e = table_at(ctx, ix, &tt);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        tlim = &tt->limits;
        an = addr_num(tlim);
        addr = num_vt_from_byte(an);
        nb = sizeof(w89_vt);
        memset(&ref, 0, nb);
        ref.is_ref = 1;
        trt = &tt->rt;
        rv = *trt;
        ref.rt = rv;
        it.ins = &addr;
        it.nins = 1;
        it.outs = &ref;
        it.nouts = 1;
        break;
    }
    case 0x26: {
        w89_tabletype *tt;
        w89_vt ref;
        w89_u32 ix;
        w89_u32 an;
        w89_reftype *trt;
        w89_limits *tlim;
        w89_reftype rv;
        size_t nb;
        ix = in->idx;
        e = table_at(ctx, ix, &tt);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        tlim = &tt->limits;
        an = addr_num(tlim);
        addr = num_vt_from_byte(an);
        nb = sizeof(w89_vt);
        memset(&ref, 0, nb);
        ref.is_ref = 1;
        trt = &tt->rt;
        rv = *trt;
        ref.rt = rv;
        scr_push(&sc, &addr);
        scr_push(&sc, &ref);
        cvp = sc.items;
        it.ins = cvp;
        it.nins = 2;
        break;
    }
    case 0x28: case 0x29: case 0x2A: case 0x2B:
    case 0x2C: case 0x2D: case 0x2E: case 0x2F:
    case 0x30: case 0x31: case 0x32: case 0x33:
    case 0x34: case 0x35: {
        static const w89_byte lsize[14] = { 4, 8, 4, 8, 1, 1, 2, 2,
                                            1, 1, 2, 2, 4, 4 };
        static const w89_byte lnum[14] = { 0x7F, 0x7E, 0x7D, 0x7C,
                                           0x7F, 0x7F, 0x7F, 0x7F,
                                           0x7E, 0x7E, 0x7E, 0x7E,
                                           0x7E, 0x7E };
        w89_limits *lim;
        w89_u32 ix;
        w89_u32 an;
        int sz;
        int sz2;
        ix = op - 0x28;
        sz = lsize[ix];
        sz2 = (int)sz;
        e = check_memop(ctx, in, sz2);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        ix = in->memidx;
        e = memory_at(ctx, ix, &lim);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        an = addr_num(lim);
        t = num_vt_from_byte(an);
        ix = op - 0x28;
        an = lnum[ix];
        t2 = num_vt_from_byte(an);
        it.ins = &t;
        it.nins = 1;
        it.outs = &t2;
        it.nouts = 1;
        break;
    }
    case 0x36: case 0x37: case 0x38: case 0x39:
    case 0x3A: case 0x3B: case 0x3C: case 0x3D:
    case 0x3E: {
        static const w89_byte ssize[9] = { 4, 8, 4, 8, 1, 2, 1, 2, 4 };
        static const w89_byte snum[9] = { 0x7F, 0x7E, 0x7D, 0x7C,
                                          0x7F, 0x7F, 0x7E, 0x7E, 0x7E };
        w89_limits *lim;
        w89_u32 ix;
        w89_u32 an;
        int sz;
        int sz2;
        ix = op - 0x36;
        sz = ssize[ix];
        sz2 = (int)sz;
        e = check_memop(ctx, in, sz2);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        ix = in->memidx;
        e = memory_at(ctx, ix, &lim);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        an = addr_num(lim);
        t = num_vt_from_byte(an);
        ix = op - 0x36;
        an = snum[ix];
        t2 = num_vt_from_byte(an);
        scr_push(&sc, &t);
        scr_push(&sc, &t2);
        cvp = sc.items;
        it.ins = cvp;
        it.nins = 2;
        break;
    }
    case 0x3F:
    case 0x40: {
        w89_limits *lim;
        w89_u32 ix;
        w89_u32 an;
        ix = in->idx;
        e = memory_at(ctx, ix, &lim);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        an = addr_num(lim);
        addr = num_vt_from_byte(an);
        it.outs = &addr;
        it.nouts = 1;
        if (op == 0x40) {
            it.ins = &addr;
            it.nins = 1;
        }
        break;
    }
    case 0x41:
        t = num_vt_from_byte(0x7F);
        it.outs = &t;
        it.nouts = 1;
        break;
    case 0x42:
        t = num_vt_from_byte(0x7E);
        it.outs = &t;
        it.nouts = 1;
        break;
    case 0x43:
        t = num_vt_from_byte(0x7D);
        it.outs = &t;
        it.nouts = 1;
        break;
    case 0x44:
        t = num_vt_from_byte(0x7C);
        it.outs = &t;
        it.nouts = 1;
        break;
    case 0xD0: {
        w89_vt ref;
        w89_reftype *inrt;
        w89_reftype rv;
        size_t nb;
        w89_u32 is_ti;
        w89_u32 ti;
        nb = sizeof(w89_vt);
        memset(&ref, 0, nb);
        ref.is_ref = 1;
        inrt = &in->rt;
        rv = *inrt;
        ref.rt = rv;
        ref.rt.nullable = 1;
        is_ti = ref.rt.is_typeidx;
        if (is_ti) {
            ti = ref.rt.typeidx;
            n = ctx->types.ntypes;
            if (ti >= n) {
                ti = ref.rt.typeidx;
                ul = (unsigned long)ti;
                vmsg("unknown type %lu", ul);
                *err = W89_ERR_INVALID; return i;
            }
        }
        it.outs = &ref;
        it.nouts = 1;
        break;
    }
    case 0xD1: {
        w89_reftype rt;
        w89_vt ref;
        size_t nb;
        e = peek_ref(ctx, 0, &rt);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        nb = sizeof(w89_vt);
        memset(&ref, 0, nb);
        ref.is_ref = 1;
        ref.rt = rt;
        ref.rt.nullable = 1;
        it.ins = &ref;
        it.nins = 1;
        t = num_vt_from_byte(0x7F);
        it.outs = &t;
        it.nouts = 1;
        break;
    }
    case 0xD2: {
        const w89_vt *params;
        w89_u32 nparams;
        const w89_vt *results;
        w89_u32 nresults;
        w89_vt ref;
        w89_u32 ix;
        w89_u32 nf;
        w89_u32 fi;
        int ok;
        size_t nb;
        unsigned long ul;
        ix = in->idx;
        nf = ctx->nfuncs;
        if (ix >= nf) {
            ul = (unsigned long)ix;
            vmsg("unknown function %lu", ul);
            *err = W89_ERR_INVALID; return i;
        }
        e = func_type(ctx, ix, &params, &nparams, &results, &nresults);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        ok = declared_contains(ctx, ix);
        if (!ok) {
            ul = (unsigned long)ix;
            vmsg("undeclared function reference %lu", ul);
            *err = W89_ERR_INVALID; return i;
        }
        nb = sizeof(w89_vt);
        memset(&ref, 0, nb);
        ref.is_ref = 1;
        ref.rt.is_typeidx = 1;
        fi = ctx->funcs[ix];
        ref.rt.typeidx = fi;
        it.outs = &ref;
        it.nouts = 1;
        break;
    }
    case 0xD3: {
        w89_vt ref;
        size_t nb;
        nb = sizeof(w89_vt);
        memset(&ref, 0, nb);
        ref.is_ref = 1;
        ref.rt.nullable = 1;
        ref.rt.abs = W89_HT_EQ;
        scr_push(&sc, &ref);
        scr_push(&sc, &ref);
        cvp = sc.items;
        it.ins = cvp;
        it.nins = 2;
        t = num_vt_from_byte(0x7F);
        it.outs = &t;
        it.nouts = 1;
        break;
    }
    case 0xD4: {
        w89_reftype rt;
        w89_vt ref_in;
        w89_vt ref_out;
        size_t nb;
        e = peek_ref(ctx, 0, &rt);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        nb = sizeof(w89_vt);
        memset(&ref_in, 0, nb);
        ref_in.is_ref = 1;
        ref_in.rt = rt;
        ref_in.rt.nullable = 1;
        memset(&ref_out, 0, nb);
        ref_out.is_ref = 1;
        ref_out.rt = rt;
        ref_out.rt.nullable = 0;
        it.ins = &ref_in;
        it.nins = 1;
        it.outs = &ref_out;
        it.nouts = 1;
        break;
    }
    case 0xD5: {
        w89_label *lb;
        w89_reftype rt;
        w89_u32 j;
        w89_vt ref_in;
        w89_vt ref_out;
        w89_it_scr sc2;
        w89_u32 ix;
        w89_u32 len;
        const w89_vt *itm;
        const w89_vt *sc2items;
        w89_u32 sc2n;
        size_t nb;
        sc2.n = 0;
        ix = in->idx;
        e = label_at(ctx, ix, &lb);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        e = peek_ref(ctx, 0, &rt);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        nb = sizeof(w89_vt);
        memset(&ref_in, 0, nb);
        ref_in.is_ref = 1;
        ref_in.rt = rt;
        ref_in.rt.nullable = 1;
        memset(&ref_out, 0, nb);
        ref_out.is_ref = 1;
        ref_out.rt = rt;
        ref_out.rt.nullable = 0;
        len = lb->len;
        for (j = 0; j < len; j = j + 1) {
            itm = &lb->vts[j];
            scr_push(&sc, itm);
        }
        scr_push(&sc, &ref_in);
        cvp = sc.items;
        it.ins = cvp;
        n = sc.n;
        it.nins = n;
        for (j = 0; j < len; j = j + 1) {
            itm = &lb->vts[j];
            scr_push(&sc2, itm);
        }
        scr_push(&sc2, &ref_out);
        sc2items = sc2.items;
        it.outs = sc2items;
        sc2n = sc2.n;
        it.nouts = sc2n;
        break;
    }
    case 0xD6: {
        w89_label *lb;
        w89_reftype rt;
        w89_vt ref;
        w89_u32 ix;
        w89_u32 len;
        w89_u32 idx;
        const w89_vt *itm;
        w89_u32 isr;
        size_t nb;
        ix = in->idx;
        e = label_at(ctx, ix, &lb);
        if (e != W89_ERR_NONE) {
            *err = W89_ERR_INVALID; return i;
        }
        len = lb->len;
        if (len == 0) {
            vmsg("type mismatch: instruction requires reference type but "
                 "label has []");
            *err = W89_ERR_INVALID; return i;
        }
        idx = len - 1;
        itm = &lb->vts[idx];
        isr = itm->is_ref;
        if (!isr) {
            char tmp[64];
            fmt_valtype(itm, tmp, sizeof(char[64]));
            vmsg("type mismatch: instruction requires reference type but "
                 "label has %s", tmp);
            *err = W89_ERR_INVALID; return i;
        }
        idx = len - 1;
        itm = &lb->vts[idx];
        rt = itm->rt;
        n = len - 1;
        for (j = 0; j < n; j = j + 1) {
            itm = &lb->vts[j];
            scr_push(&sc, itm);
        }
        nb = sizeof(w89_vt);
        memset(&ref, 0, nb);
        ref.is_ref = 1;
        ref.rt = rt;
        ref.rt.nullable = 1;
        scr_push(&sc, &ref);
        cvp = sc.items;
        it.ins = cvp;
        nn = sc.n;
        it.nins = nn;
        cvp = sc.items;
        it.outs = cvp;
        it.nouts = n;
        break;
    }
    case 0xFC: {
        w89_u32 sub;
        w89_u32 ix;
        w89_u32 nd;
        w89_u32 an;
        w89_u32 an1, an2;
        w89_limits *lim;
        w89_limits *lim1, *lim2;
        w89_tabletype *tt;
        w89_tabletype *ta, *tb;
        w89_reftype *rt;
        w89_reftype *trt;
        w89_reftype *trt2;
        w89_vt a1, a2, am;
        w89_vt ref;
        w89_reftype rv;
        w89_typeenv *te;
        w89_limits *tlim;
        int m;
        size_t nb;
        char rb[64], sb[64];
        sub = in->sub;
        if (sub <= 0x07) {
            cvt = &w89_tsat_tab[sub];
            n = cvt->in;
            t = num_vt_from_byte(n);
            n = cvt->out;
            t2 = num_vt_from_byte(n);
            it.ins = &t;
            it.nins = 1;
            it.outs = &t2;
            it.nouts = 1;
            break;
        }
        if (sub == 0x08) {
            ix = in->idx;
            e = memory_at(ctx, ix, &lim);
            if (e != W89_ERR_NONE) {
                *err = W89_ERR_INVALID; return i;
            }
            nd = ctx->ndatas;
            ix = in->idx2;
            if (ix >= nd) {
                ul = (unsigned long)ix;
                vmsg("unknown data segment %lu", ul);
                *err = W89_ERR_INVALID; return i;
            }
            an = addr_num(lim);
            addr = num_vt_from_byte(an);
            scr_push(&sc, &addr);
            scr_push(&sc, &i32v);
            scr_push(&sc, &i32v);
            cvp = sc.items;
            it.ins = cvp;
            it.nins = 3;
            break;
        }
        if (sub == 0x09) {
            nd = ctx->ndatas;
            ix = in->idx;
            if (ix >= nd) {
                ul = (unsigned long)ix;
                vmsg("unknown data segment %lu", ul);
                *err = W89_ERR_INVALID; return i;
            }
            break;
        }
        if (sub == 0x0A) {
            ix = in->idx2;
            e = memory_at(ctx, ix, &lim1);
            if (e != W89_ERR_NONE) {
                *err = W89_ERR_INVALID; return i;
            }
            ix = in->idx;
            e = memory_at(ctx, ix, &lim2);
            if (e != W89_ERR_NONE) {
                *err = W89_ERR_INVALID; return i;
            }
            an1 = addr_num(lim1);
            a1 = num_vt_from_byte(an1);
            an2 = addr_num(lim2);
            a2 = num_vt_from_byte(an2);
            an1 = lim1->addr64;
            an2 = lim2->addr64;
            if (an1) {
                if (an2) {
                    am = num_vt_from_byte(0x7E);
                } else {
                    am = num_vt_from_byte(0x7F);
                }
            } else {
                am = num_vt_from_byte(0x7F);
            }
            scr_push(&sc, &a1);
            scr_push(&sc, &a2);
            scr_push(&sc, &am);
            cvp = sc.items;
            it.ins = cvp;
            it.nins = 3;
            break;
        }
        if (sub == 0x0B) {
            ix = in->idx;
            e = memory_at(ctx, ix, &lim);
            if (e != W89_ERR_NONE) {
                *err = W89_ERR_INVALID; return i;
            }
            an = addr_num(lim);
            addr = num_vt_from_byte(an);
            scr_push(&sc, &addr);
            scr_push(&sc, &i32v);
            scr_push(&sc, &addr);
            cvp = sc.items;
            it.ins = cvp;
            it.nins = 3;
            break;
        }
        if (sub == 0x0C) {
            ix = in->idx;
            e = table_at(ctx, ix, &tt);
            if (e != W89_ERR_NONE) {
                *err = W89_ERR_INVALID; return i;
            }
            ix = in->idx2;
            e = elem_at(ctx, ix, &rt);
            if (e != W89_ERR_NONE) {
                *err = W89_ERR_INVALID; return i;
            }
            te = &ctx->types;
            trt = &tt->rt;
            m = w89_match_reftype(te, rt, trt);
            if (!m) {
                fmt_reftype(rt, rb, sizeof(char[64]));
                fmt_reftype(trt, sb, sizeof(char[64]));
                vmsg("type mismatch: element segment's type %s does not match "
                     "table's element type %s", rb, sb);
                *err = W89_ERR_INVALID; return i;
            }
            tlim = &tt->limits;
            an = addr_num(tlim);
            addr = num_vt_from_byte(an);
            scr_push(&sc, &addr);
            scr_push(&sc, &i32v);
            scr_push(&sc, &i32v);
            cvp = sc.items;
            it.ins = cvp;
            it.nins = 3;
            break;
        }
        if (sub == 0x0D) {
            ix = in->idx;
            e = elem_at(ctx, ix, &rt);
            if (e != W89_ERR_NONE) {
                *err = W89_ERR_INVALID; return i;
            }
            break;
        }
        if (sub == 0x0E) {
            ix = in->idx2;
            e = table_at(ctx, ix, &ta);
            if (e != W89_ERR_NONE) {
                *err = W89_ERR_INVALID; return i;
            }
            ix = in->idx;
            e = table_at(ctx, ix, &tb);
            if (e != W89_ERR_NONE) {
                *err = W89_ERR_INVALID; return i;
            }
            te = &ctx->types;
            trt = &tb->rt;
            trt2 = &ta->rt;
            m = w89_match_reftype(te, trt, trt2);
            if (!m) {
                fmt_reftype(trt2, rb, sizeof(char[64]));
                fmt_reftype(trt, sb, sizeof(char[64]));
                vmsg("type mismatch: source element type %s does not match "
                     "destination element type %s", rb, sb);
                *err = W89_ERR_INVALID; return i;
            }
            tlim = &ta->limits;
            an = addr_num(tlim);
            a1 = num_vt_from_byte(an);
            tlim = &tb->limits;
            an = addr_num(tlim);
            a2 = num_vt_from_byte(an);
            an1 = ta->limits.addr64;
            an2 = tb->limits.addr64;
            if (an1) {
                if (an2) {
                    am = num_vt_from_byte(0x7E);
                } else {
                    am = num_vt_from_byte(0x7F);
                }
            } else {
                am = num_vt_from_byte(0x7F);
            }
            scr_push(&sc, &a1);
            scr_push(&sc, &a2);
            scr_push(&sc, &am);
            cvp = sc.items;
            it.ins = cvp;
            it.nins = 3;
            break;
        }
        if (sub == 0x0F) {
            w89_limits *tlim2;
            ix = in->idx;
            e = table_at(ctx, ix, &tt);
            if (e != W89_ERR_NONE) {
                *err = W89_ERR_INVALID; return i;
            }
            tlim2 = &tt->limits;
            an = addr_num(tlim2);
            addr = num_vt_from_byte(an);
            nb = sizeof(w89_vt);
            memset(&ref, 0, nb);
            ref.is_ref = 1;
            trt = &tt->rt;
            rv = *trt;
            ref.rt = rv;
            scr_push(&sc, &ref);
            scr_push(&sc, &addr);
            cvp = sc.items;
            it.ins = cvp;
            it.nins = 2;
            it.outs = &addr;
            it.nouts = 1;
            break;
        }
        if (sub == 0x10) {
            w89_limits *tlim2;
            ix = in->idx;
            e = table_at(ctx, ix, &tt);
            if (e != W89_ERR_NONE) {
                *err = W89_ERR_INVALID; return i;
            }
            tlim2 = &tt->limits;
            an = addr_num(tlim2);
            addr = num_vt_from_byte(an);
            it.outs = &addr;
            it.nouts = 1;
            break;
        }
        if (sub == 0x11) {
            w89_limits *tlim2;
            ix = in->idx;
            e = table_at(ctx, ix, &tt);
            if (e != W89_ERR_NONE) {
                *err = W89_ERR_INVALID; return i;
            }
            tlim2 = &tt->limits;
            an = addr_num(tlim2);
            addr = num_vt_from_byte(an);
            nb = sizeof(w89_vt);
            memset(&ref, 0, nb);
            ref.is_ref = 1;
            trt = &tt->rt;
            rv = *trt;
            ref.rt = rv;
            scr_push(&sc, &addr);
            scr_push(&sc, &ref);
            scr_push(&sc, &addr);
            cvp = sc.items;
            it.ins = cvp;
            it.nins = 3;
            break;
        }
        vmsg("illegal opcode");
        *err = W89_ERR_INVALID; return i;
    }
    case 0xFD: {
        w89_u32 sub;
        w89_u32 ix;
        w89_u32 an;
        w89_limits *lim;
        w89_vt vvec;
        sub = in->sub;
        vvec = num_vt_from_byte(0x7B);
        if (sub == 0x00) {
            e = check_memop(ctx, in, 16);
            if (e != W89_ERR_NONE) {
                *err = W89_ERR_INVALID; return i;
            }
            ix = in->memidx;
            e = memory_at(ctx, ix, &lim);
            if (e != W89_ERR_NONE) {
                *err = W89_ERR_INVALID; return i;
            }
            an = addr_num(lim);
            t = num_vt_from_byte(an);
            t2 = vvec;
            it.ins = &t;
            it.nins = 1;
            it.outs = &t2;
            it.nouts = 1;
            break;
        }
        if (sub == 0x0B) {
            e = check_memop(ctx, in, 16);
            if (e != W89_ERR_NONE) {
                *err = W89_ERR_INVALID; return i;
            }
            ix = in->memidx;
            e = memory_at(ctx, ix, &lim);
            if (e != W89_ERR_NONE) {
                *err = W89_ERR_INVALID; return i;
            }
            an = addr_num(lim);
            addr = num_vt_from_byte(an);
            scr_push(&sc, &addr);
            scr_push(&sc, &vvec);
            cvp = sc.items;
            it.ins = cvp;
            it.nins = 2;
            break;
        }
        if (sub == 0x0C) {
            t2 = vvec;
            it.outs = &t2;
            it.nouts = 1;
            break;
        }
        vmsg("illegal opcode");
        *err = W89_ERR_INVALID; return i;
    }
    default:
        {
            int ok;
            w89_byte bop;
            bop = (w89_byte)op;
            ok = numeric_optype(bop, &num, &kind, &cvt);
            if (ok) {
                if (kind == 4) {
                    n = cvt->in;
                    t = num_vt_from_byte(n);
                    n = cvt->out;
                    t2 = num_vt_from_byte(n);
                    it.ins = &t;
                    it.nins = 1;
                    it.outs = &t2;
                    it.nouts = 1;
                } else if (kind == 0) {
                    t = num_vt_from_byte(num);
                    t2 = num_vt_from_byte(0x7F);
                    it.ins = &t;
                    it.nins = 1;
                    it.outs = &t2;
                    it.nouts = 1;
                } else if (kind == 1) {
                    t = num_vt_from_byte(num);
                    t2 = num_vt_from_byte(0x7F);
                    scr_push(&sc, &t);
                    scr_push(&sc, &t);
                    cvp = sc.items;
                    it.ins = cvp;
                    it.nins = 2;
                    it.outs = &t2;
                    it.nouts = 1;
                } else if (kind == 2) {
                    t = num_vt_from_byte(num);
                    it.ins = &t;
                    it.nins = 1;
                    it.outs = &t;
                    it.nouts = 1;
                } else {
                    t = num_vt_from_byte(num);
                    scr_push(&sc, &t);
                    scr_push(&sc, &t);
                    cvp = sc.items;
                    it.ins = cvp;
                    it.nins = 2;
                    it.outs = &t;
                    it.nouts = 1;
                }
                break;
            }
            vmsg("illegal opcode");
            *err = W89_ERR_INVALID; return i;
        }
    }

    e = apply_it(ctx, &it);
    if (e != W89_ERR_NONE) {
        *err = W89_ERR_INVALID; return i;
    }
    return i + 1;
}

static w89_u32 check_instrs(w89_valctx *ctx, const w89_instr_vec *v,
                            w89_u32 i, w89_err *err)
{
    w89_u32 vn;
    w89_instr *arr;
    w89_instr *itm;
    w89_u32 op;
    w89_u32 ni;
    vn = v->n;
    while (i < vn) {
        arr = v->items;
        itm = &arr[i];
        op = itm->op;
        if (op == 0x05) {
            break;
        }
        if (op == 0x0B) {
            break;
        }
        i = check_instr(ctx, v, i, err);
        ni = *err;
        if (ni != W89_ERR_NONE) {
            return i;
        }
    }
    return i;
}

/* ---------- Type section validation ---------- */

static int vt_valid(const w89_vt *vt, w89_u32 bound)
{
    w89_u32 isr;
    w89_u32 is_ti;
    w89_u32 ti;
    isr = vt->is_ref;
    if (!isr) {
        return 1;
    }
    is_ti = vt->rt.is_typeidx;
    if (!is_ti) {
        return 1;
    }
    ti = vt->rt.typeidx;
    return ti < bound;
}

static w89_err validate_type_section(const w89_typeenv *env)
{
    w89_u32 i, j;
    w89_u32 nt;
    w89_u32 rg;
    w89_u32 first, nsubs, bound;
    w89_deftype *dt;
    const w89_subtype *st;
    w89_u32 nsup;
    w89_u32 sp;
    w89_vt *ap;
    w89_u32 n;
    int ok;
    w89_fieldtype *ft;
    w89_vt *vtp;
    w89_u32 s;
    const w89_subtype *su;
    w89_u32 isf;
    unsigned long ul;
    unsigned long ul2;
    w89_u32 kk;
    w89_u32 fpk;
    nt = env->ntypes;
    for (i = 0; i < nt; i = i + 1) {
        dt = &env->types[i];
        rg = dt->recgroup;
        first = env->recs[rg].first;
        nsubs = env->recs[rg].nsubs;
        bound = first + nsubs;
        st = dt->sub;
        nsup = st->nsupers;
        for (j = 0; j < nsup; j = j + 1) {
            sp = st->supertypes[j];
            if (sp >= bound) {
                ul = (unsigned long)sp;
                vmsg("unknown type %lu", ul);
                return W89_ERR_INVALID;
            }
        }
        kk = st->kind;
        if (kk == W89_CK_FUNC) {
            n = st->ft.nparams;
            for (j = 0; j < n; j = j + 1) {
                ap = &st->ft.params[j];
                ok = vt_valid(ap, bound);
                if (!ok) {
                    sp = ap->rt.typeidx;
                    ul = (unsigned long)sp;
                    vmsg("unknown type %lu", ul);
                    return W89_ERR_INVALID;
                }
            }
            n = st->ft.nresults;
            for (j = 0; j < n; j = j + 1) {
                ap = &st->ft.results[j];
                ok = vt_valid(ap, bound);
                if (!ok) {
                    sp = ap->rt.typeidx;
                    ul = (unsigned long)sp;
                    vmsg("unknown type %lu", ul);
                    return W89_ERR_INVALID;
                }
            }
        } else {
            n = st->nfields;
            for (j = 0; j < n; j = j + 1) {
                ft = &st->fields[j];
                fpk = ft->is_packed;
                if (fpk == 0) {
                    vtp = &ft->vt;
                    ok = vt_valid(vtp, bound);
                    if (!ok) {
                        sp = vtp->rt.typeidx;
                        ul = (unsigned long)sp;
                        vmsg("unknown type %lu", ul);
                        return W89_ERR_INVALID;
                    }
                }
            }
        }
    }
    nt = env->ntypes;
    for (i = 0; i < nt; i = i + 1) {
        dt = &env->types[i];
        st = dt->sub;
        nsup = st->nsupers;
        for (j = 0; j < nsup; j = j + 1) {
            s = st->supertypes[j];
            if (s >= i) {
                ul = (unsigned long)s;
                vmsg("forward use of type %lu in sub type definition", ul);
                return W89_ERR_INVALID;
            }
            su = env->types[s].sub;
            isf = su->is_final;
            if (isf) {
                ul = (unsigned long)i;
                ul2 = (unsigned long)s;
                vmsg("sub type %lu has final super type %lu", ul, ul2);
                return W89_ERR_INVALID;
            }
            ok = w89_match_comptype(env, st, su);
            if (!ok) {
                ul = (unsigned long)i;
                ul2 = (unsigned long)s;
                vmsg("sub type %lu does not match super type %lu", ul, ul2);
                return W89_ERR_INVALID;
            }
        }
    }
    return W89_ERR_NONE;
}

/* ---------- Module-level checks ---------- */

static w89_err check_limits(const w89_limits *lim, w89_u64 range,
                            const char *msg)
{
    w89_u64 mn;
    w89_u64 mx;
    w89_u32 has;
    mn = lim->min;
    if (mn > range) {
        vmsg("%s", msg);
        return W89_ERR_INVALID;
    }
    has = lim->has_max;
    if (has) {
        mx = lim->max;
        if (mx > range) {
            vmsg("%s", msg);
            return W89_ERR_INVALID;
        }
        mn = lim->min;
        mx = lim->max;
        if (mn > mx) {
            vmsg("size minimum must not be greater than maximum");
            return W89_ERR_INVALID;
        }
    }
    return W89_ERR_NONE;
}

static w89_err check_valtype_ref(w89_valctx *ctx, const w89_vt *vt)
{
    w89_u32 isr;
    w89_u32 is_ti;
    w89_u32 ti;
    w89_u32 nt;
    unsigned long ul;
    isr = vt->is_ref;
    if (isr) {
        is_ti = vt->rt.is_typeidx;
        if (is_ti) {
            ti = vt->rt.typeidx;
            nt = ctx->types.ntypes;
            if (ti >= nt) {
                ul = (unsigned long)ti;
                vmsg("unknown type %lu", ul);
                return W89_ERR_INVALID;
            }
        }
    }
    return W89_ERR_NONE;
}

static w89_err check_reftype_ref(w89_valctx *ctx, const w89_reftype *rt)
{
    w89_u32 is_ti;
    w89_u32 ti;
    w89_u32 nt;
    unsigned long ul;
    is_ti = rt->is_typeidx;
    if (is_ti) {
        ti = rt->typeidx;
        nt = ctx->types.ntypes;
        if (ti >= nt) {
            ul = (unsigned long)ti;
            vmsg("unknown type %lu", ul);
            return W89_ERR_INVALID;
        }
    }
    return W89_ERR_NONE;
}

static w89_err check_memorytype(w89_valctx *ctx, const w89_limits *lim)
{
    w89_u32 a64;
    (void)ctx;
    a64 = lim->addr64;
    if (a64) {
        return check_limits(lim, 0x1000000000000UL,
                            "memory size must be at most 2^48 pages "
                            "(256 TiB) for i64");
    }
    return check_limits(lim, 0x10000UL,
                        "memory size must be at most 2^16 pages (4 GiB) "
                        "for i32");
}

static w89_err check_tabletype(w89_valctx *ctx, const w89_tabletype *tt)
{
    w89_err e;
    const w89_reftype *trt;
    const w89_limits *tlim;
    w89_u32 a64;
    trt = &tt->rt;
    e = check_reftype_ref(ctx, trt);
    if (e != W89_ERR_NONE) {
        return e;
    }
    tlim = &tt->limits;
    a64 = tlim->addr64;
    if (a64) {
        return check_limits(tlim, 0xFFFFFFFFFFFFFFFFUL,
                            "table size must be at most 2^64-1 for i64");
    }
    return check_limits(tlim, 0xFFFFFFFFUL,
                        "table size must be at most 2^32-1 for i32");
}

static w89_err is_const_instr(w89_valctx *ctx, const w89_instr *in,
                              int *is_const)
{
    w89_globaltype *gt;
    w89_err e;
    w89_u32 op;
    w89_u32 ix;
    w89_u32 mut;
    w89_u32 sub;
    *is_const = 0;
    op = in->op;
    switch (op) {
    case 0x41: case 0x42: case 0x43: case 0x44:
    case 0x6A: case 0x6B: case 0x6C:
    case 0x7C: case 0x7D: case 0x7E:
    case 0xD0:
    case 0xD2:
        *is_const = 1;
        return W89_ERR_NONE;
    case 0xFD:
        sub = in->sub;
        if (sub == 0x0C) {
            *is_const = 1;
        }
        return W89_ERR_NONE;
    case 0x23: {
        ix = in->idx;
        e = global_at(ctx, ix, &gt);
        if (e != W89_ERR_NONE) {
            return e;
        }
        mut = gt->mut;
        if (mut) {
            *is_const = 0;
        } else {
            *is_const = 1;
        }
        return W89_ERR_NONE;
    }
    default:
        return W89_ERR_NONE;
    }
}

/* Check a constant expression v[start..end) against the expected single
 * result type t. */
static w89_err check_const(w89_valctx *ctx, const w89_instr_vec *v,
                           w89_u32 start, w89_u32 end, const w89_vt *t)
{
    w89_u32 j;
    w89_err e = W89_ERR_NONE;
    w89_u32 base;
    int saved_ell;
    w89_u32 saved_sbase;
    w89_u32 full;
    int isc;
    w89_instr *arr;
    w89_instr *itm;
    base = ctx->on;
    saved_ell = ctx->ell;
    saved_sbase = ctx->sbase;
    arr = v->items;
    for (j = start; j < end; j = j + 1) {
        itm = &arr[j];
        e = is_const_instr(ctx, itm, &isc);
        if (e != W89_ERR_NONE) {
            return e;
        }
        if (!isc) {
            vmsg("constant expression required");
            return W89_ERR_INVALID;
        }
    }
    ctx->on = base;
    ctx->ell = W89_ELL_NO;
    ctx->sbase = base;
    while (start < end) {
        start = check_instr(ctx, v, start, &e);
        if (e != W89_ERR_NONE) {
            ctx->on = base;
            ctx->ell = saved_ell;
            ctx->sbase = saved_sbase;
            return e;
        }
    }
    full = ctx->on;
    e = check_block_result(ctx, base, full, t, 1);
    ctx->on = base;
    ctx->ell = saved_ell;
    ctx->sbase = saved_sbase;
    return e;
}

static w89_err push_u32(w89_u32 **pp, w89_u32 *n, w89_u32 *cap, w89_u32 val,
                        w89_err *e)
{
    void **pv;
    pv = (void **)pp;
    ctx_push(pv, n, cap, sizeof(w89_u32), &val, e);
    return *e;
}

static w89_err push_table(w89_tabletype **pp, w89_u32 *n, w89_u32 *cap,
                          const w89_tabletype *val, w89_err *e)
{
    void **pv;
    pv = (void **)pp;
    ctx_push(pv, n, cap, sizeof(w89_tabletype), val, e);
    return *e;
}

static w89_err push_mem(w89_limits **pp, w89_u32 *n, w89_u32 *cap,
                        const w89_limits *val, w89_err *e)
{
    void **pv;
    pv = (void **)pp;
    ctx_push(pv, n, cap, sizeof(w89_limits), val, e);
    return *e;
}

static w89_err push_global(w89_globaltype **pp, w89_u32 *n, w89_u32 *cap,
                           const w89_globaltype *val, w89_err *e)
{
    void **pv;
    pv = (void **)pp;
    ctx_push(pv, n, cap, sizeof(w89_globaltype), val, e);
    return *e;
}

static w89_err check_import(w89_valctx *ctx, const w89_import *im)
{
    w89_err e;
    w89_u32 kd;
    w89_u32 **pfuncs;
    w89_u32 *pnfuncs;
    w89_u32 *pfuncs_cap;
    w89_tabletype **ptables;
    w89_u32 *pntables;
    w89_u32 *ptables_cap;
    w89_limits **pmemories;
    w89_u32 *pnmems;
    w89_u32 *pmems_cap;
    w89_globaltype **pglobals;
    w89_u32 *pnglobals;
    w89_u32 *pglobals_cap;
    const w89_vt *gvt;
    w89_u32 ti;
    const w89_vt *params;
    w89_u32 nparams;
    const w89_vt *results;
    w89_u32 nresults;
    const w89_tabletype *tt;
    const w89_limits *lim;
    const w89_globaltype *gt;
    kd = im->kind;
    switch (kd) {
    case 0x00: {
        ti = im->typeidx;
        e = func_type_by_typeidx(ctx, ti, &params, &nparams,
                                 &results, &nresults);
        if (e != W89_ERR_NONE) {
            return e;
        }
        pfuncs = &ctx->funcs;
        pnfuncs = &ctx->nfuncs;
        pfuncs_cap = &ctx->funcs_cap;
        e = push_u32(pfuncs, pnfuncs, pfuncs_cap, ti, &e);
        return e;
    }
    case 0x01:
        tt = &im->table;
        e = check_tabletype(ctx, tt);
        if (e != W89_ERR_NONE) {
            return e;
        }
        ptables = &ctx->tables;
        pntables = &ctx->ntables;
        ptables_cap = &ctx->tables_cap;
        e = push_table(ptables, pntables, ptables_cap, tt, &e);
        return e;
    case 0x02:
        lim = &im->mem;
        e = check_memorytype(ctx, lim);
        if (e != W89_ERR_NONE) {
            return e;
        }
        pmemories = &ctx->memories;
        pnmems = &ctx->nmems;
        pmems_cap = &ctx->mems_cap;
        e = push_mem(pmemories, pnmems, pmems_cap, lim, &e);
        return e;
    case 0x03:
        gt = &im->global;
        gvt = &gt->vt;
        e = check_valtype_ref(ctx, gvt);
        if (e != W89_ERR_NONE) {
            return e;
        }
        pglobals = &ctx->globals;
        pnglobals = &ctx->nglobals;
        pglobals_cap = &ctx->globals_cap;
        e = push_global(pglobals, pnglobals, pglobals_cap, gt, &e);
        return e;
    case 0x04: {
        ti = im->typeidx;
        e = func_type_by_typeidx(ctx, ti, &params, &nparams,
                                 &results, &nresults);
        if (e != W89_ERR_NONE) {
            return e;
        }
        if (nresults != 0) {
            vmsg("non-empty tag result type");
            return W89_ERR_INVALID;
        }
        pfuncs = &ctx->tags;
        pnfuncs = &ctx->ntags;
        pfuncs_cap = &ctx->tags_cap;
        e = push_u32(pfuncs, pnfuncs, pfuncs_cap, ti, &e);
        return e;
    }
    default:
        return W89_ERR_NONE;
    }
}

static w89_err check_tag(w89_valctx *ctx, const w89_tag *tag)
{
    w89_err e;
    w89_u32 **pfuncs;
    w89_u32 *pnfuncs;
    w89_u32 *pfuncs_cap;
    const w89_vt *params;
    w89_u32 nparams;
    const w89_vt *results;
    w89_u32 nresults;
    w89_u32 ti;
    ti = tag->typeidx;
    e = func_type_by_typeidx(ctx, ti, &params, &nparams,
                             &results, &nresults);
    if (e != W89_ERR_NONE) {
        return e;
    }
    if (nresults != 0) {
        vmsg("non-empty tag result type");
        return W89_ERR_INVALID;
    }
    pfuncs = &ctx->tags;
    pnfuncs = &ctx->ntags;
    pfuncs_cap = &ctx->tags_cap;
    e = push_u32(pfuncs, pnfuncs, pfuncs_cap, ti, &e);
    return e;
}

static w89_err check_func(w89_valctx *ctx, w89_u32 typeidx)
{
    w89_err e;
    w89_u32 **pfuncs;
    w89_u32 *pnfuncs;
    w89_u32 *pfuncs_cap;
    const w89_vt *params;
    w89_u32 nparams;
    const w89_vt *results;
    w89_u32 nresults;
    e = func_type_by_typeidx(ctx, typeidx, &params, &nparams,
                             &results, &nresults);
    if (e != W89_ERR_NONE) {
        return e;
    }
    pfuncs = &ctx->funcs;
    pnfuncs = &ctx->nfuncs;
    pfuncs_cap = &ctx->funcs_cap;
    e = push_u32(pfuncs, pnfuncs, pfuncs_cap, typeidx, &e);
    return e;
}

static w89_err check_memory(w89_valctx *ctx, const w89_limits *lim)
{
    w89_err e;
    w89_limits **pmemories;
    w89_u32 *pnmems;
    w89_u32 *pmems_cap;
    e = check_memorytype(ctx, lim);
    if (e != W89_ERR_NONE) {
        return e;
    }
    pmemories = &ctx->memories;
    pnmems = &ctx->nmems;
    pmems_cap = &ctx->mems_cap;
    e = push_mem(pmemories, pnmems, pmems_cap, lim, &e);
    return e;
}

static w89_err check_table(w89_valctx *ctx, const w89_table *tab)
{
    w89_err e;
    w89_tabletype **ptables;
    w89_u32 *pntables;
    w89_u32 *ptables_cap;
    const w89_tabletype *tp;
    const w89_instr_vec *init;
    w89_u32 inn;
    w89_vt ref;
    w89_reftype rv;
    const w89_reftype *trt;
    const w89_vt *rp;
    const w89_instr_vec *ivp;
    size_t nb;
    w89_instr in;
    w89_instr_vec iv;
    w89_instr *pitem;
    tp = &tab->type;
    e = check_tabletype(ctx, tp);
    if (e != W89_ERR_NONE) {
        return e;
    }
    init = &tab->init;
    inn = init->n;
    if (inn != 0) {
        nb = sizeof(w89_vt);
        memset(&ref, 0, nb);
        ref.is_ref = 1;
        trt = &tp->rt;
        rv = *trt;
        ref.rt = rv;
        rp = &ref;
        e = check_const(ctx, init, 0, inn, rp);
        if (e != W89_ERR_NONE) {
            return e;
        }
    } else {
        nb = sizeof(w89_instr);
        memset(&in, 0, nb);
        in.op = 0xD0;
        trt = &tp->rt;
        rv = *trt;
        in.rt = rv;
        nb = sizeof(w89_instr_vec);
        memset(&iv, 0, nb);
        pitem = &in;
        iv.items = pitem;
        iv.n = 1;
        nb = sizeof(w89_vt);
        memset(&ref, 0, nb);
        ref.is_ref = 1;
        trt = &tp->rt;
        rv = *trt;
        ref.rt = rv;
        ivp = &iv;
        rp = &ref;
        e = check_const(ctx, ivp, 0, 1, rp);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }
    ptables = &ctx->tables;
    pntables = &ctx->ntables;
    ptables_cap = &ctx->tables_cap;
    e = push_table(ptables, pntables, ptables_cap, tp, &e);
    return e;
}

static w89_err check_global(w89_valctx *ctx, const w89_global *g)
{
    w89_err e;
    w89_globaltype **pglobals;
    w89_u32 *pnglobals;
    w89_u32 *pglobals_cap;
    const w89_globaltype *tp;
    const w89_vt *vtp;
    const w89_instr_vec *init;
    w89_u32 inn;
    tp = &g->type;
    vtp = &tp->vt;
    e = check_valtype_ref(ctx, vtp);
    if (e != W89_ERR_NONE) {
        return e;
    }
    init = &g->init;
    inn = init->n;
    e = check_const(ctx, init, 0, inn, vtp);
    if (e != W89_ERR_NONE) {
        return e;
    }
    pglobals = &ctx->globals;
    pnglobals = &ctx->nglobals;
    pglobals_cap = &ctx->globals_cap;
    e = push_global(pglobals, pnglobals, pglobals_cap, tp, &e);
    return e;
}

static w89_err check_data(w89_valctx *ctx, const w89_data *d)
{
    w89_limits *lim;
    w89_err e;
    w89_u32 fl;
    w89_u32 nm;
    w89_u32 mi;
    w89_u32 an;
    w89_vt addr;
    const w89_instr_vec *off;
    w89_u32 on;
    fl = d->flags;
    if (fl == 0) {
        nm = ctx->nmems;
        if (nm == 0) {
            vmsg("unknown memory 0");
            return W89_ERR_INVALID;
        }
        lim = &ctx->memories[0];
        an = addr_num(lim);
        addr = num_vt_from_byte(an);
        off = &d->offset;
        on = off->n;
        e = check_const(ctx, off, 0, on, &addr);
        if (e != W89_ERR_NONE) {
            return e;
        }
    } else if (fl == 2) {
        mi = d->memidx;
        e = memory_at(ctx, mi, &lim);
        if (e != W89_ERR_NONE) {
            return e;
        }
        an = addr_num(lim);
        addr = num_vt_from_byte(an);
        off = &d->offset;
        on = off->n;
        e = check_const(ctx, off, 0, on, &addr);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }
    nm = ctx->ndatas;
    nm = nm + 1;
    ctx->ndatas = nm;
    return W89_ERR_NONE;
}

static w89_err check_elem(w89_valctx *ctx, const w89_elem *el)
{
    w89_err e;
    w89_u32 k;
    w89_u32 fl;
    w89_u32 n;
    w89_u32 ti;
    w89_u32 nt;
    w89_u32 is_ti;
    w89_u32 idx;
    w89_u32 p, q;
    w89_u32 en;
    w89_instr *arr;
    w89_instr *itm;
    w89_u32 opv;
    w89_tabletype *tt;
    w89_vt addr;
    w89_vt ref;
    const w89_reftype *rtp;
    w89_reftype rv;
    w89_reftype *trt;
    w89_typeenv *te;
    w89_u32 ntb;
    w89_limits *tlim;
    w89_u32 an;
    w89_instr in;
    w89_instr_vec iv;
    w89_instr *pitem;
    const w89_instr_vec *ivp;
    const w89_vt *rp;
    const w89_instr_vec *offp;
    w89_u32 on;
    size_t nb;
    int m;
    void **pp;
    w89_reftype **pel;
    w89_u32 *pn;
    w89_u32 *pc;
    char rb[64], sb[64];
    unsigned long ul;
    is_ti = el->rt.is_typeidx;
    if (is_ti) {
        ti = el->rt.typeidx;
        nt = ctx->types.ntypes;
        if (ti >= nt) {
            ul = (unsigned long)ti;
            vmsg("unknown type %lu", ul);
            return W89_ERR_INVALID;
        }
    }
    nb = sizeof(w89_vt);
    memset(&ref, 0, nb);
    ref.is_ref = 1;
    rtp = &el->rt;
    rv = *rtp;
    ref.rt = rv;
    fl = el->flags;
    if (fl < 4) {
        n = el->n;
        for (k = 0; k < n; k = k + 1) {
            nb = sizeof(w89_instr);
            memset(&in, 0, nb);
            in.op = 0xD2;
            idx = el->indices[k];
            in.idx = idx;
            nb = sizeof(w89_instr_vec);
            memset(&iv, 0, nb);
            pitem = &in;
            iv.items = pitem;
            iv.n = 1;
            e = check_const(ctx, &iv, 0, 1, &ref);
            if (e != W89_ERR_NONE) {
                return e;
            }
        }
    } else {
        p = 0;
        en = el->exprs.n;
        while (p < en) {
            q = p;
            while (1) {
                if (q >= en) {
                    break;
                }
                arr = el->exprs.items;
                itm = &arr[q];
                opv = itm->op;
                if (opv == 0x0B) {
                    break;
                }
                q = q + 1;
            }
            ivp = &el->exprs;
            rp = &ref;
            e = check_const(ctx, ivp, p, q, rp);
            if (e != W89_ERR_NONE) {
                return e;
            }
            p = q + 1;
        }
    }
    fl = el->flags;
    if (fl == 0) {
        ntb = ctx->ntables;
        if (ntb == 0) {
            vmsg("unknown table 0");
            return W89_ERR_INVALID;
        }
        tt = &ctx->tables[0];
        te = &ctx->types;
        trt = &tt->rt;
        m = w89_match_reftype(te, rtp, trt);
        if (!m) {
            fmt_reftype(rtp, rb, sizeof(char[64]));
            fmt_reftype(trt, sb, sizeof(char[64]));
            vmsg("type mismatch: element segment's type %s does not match "
                 "table's element type %s", rb, sb);
            return W89_ERR_INVALID;
        }
        tlim = &tt->limits;
        an = addr_num(tlim);
        addr = num_vt_from_byte(an);
        offp = &el->offset;
        on = offp->n;
        rp = &addr;
        e = check_const(ctx, offp, 0, on, rp);
        if (e != W89_ERR_NONE) {
            return e;
        }
    } else if (fl == 2) {
        idx = el->tableidx;
        e = table_at(ctx, idx, &tt);
        if (e != W89_ERR_NONE) {
            return e;
        }
        te = &ctx->types;
        trt = &tt->rt;
        m = w89_match_reftype(te, rtp, trt);
        if (!m) {
            fmt_reftype(rtp, rb, sizeof(char[64]));
            fmt_reftype(trt, sb, sizeof(char[64]));
            vmsg("type mismatch: element segment's type %s does not match "
                 "table's element type %s", rb, sb);
            return W89_ERR_INVALID;
        }
        tlim = &tt->limits;
        an = addr_num(tlim);
        addr = num_vt_from_byte(an);
        offp = &el->offset;
        on = offp->n;
        rp = &addr;
        e = check_const(ctx, offp, 0, on, rp);
        if (e != W89_ERR_NONE) {
            return e;
        }
    } else if (fl == 4) {
        ntb = ctx->ntables;
        if (ntb == 0) {
            vmsg("unknown table 0");
            return W89_ERR_INVALID;
        }
        tt = &ctx->tables[0];
        te = &ctx->types;
        trt = &tt->rt;
        m = w89_match_reftype(te, rtp, trt);
        if (!m) {
            fmt_reftype(rtp, rb, sizeof(char[64]));
            fmt_reftype(trt, sb, sizeof(char[64]));
            vmsg("type mismatch: element segment's type %s does not match "
                 "table's element type %s", rb, sb);
            return W89_ERR_INVALID;
        }
        tlim = &tt->limits;
        an = addr_num(tlim);
        addr = num_vt_from_byte(an);
        offp = &el->offset;
        on = offp->n;
        rp = &addr;
        e = check_const(ctx, offp, 0, on, rp);
        if (e != W89_ERR_NONE) {
            return e;
        }
    } else if (fl == 6) {
        idx = el->tableidx;
        e = table_at(ctx, idx, &tt);
        if (e != W89_ERR_NONE) {
            return e;
        }
        te = &ctx->types;
        trt = &tt->rt;
        m = w89_match_reftype(te, rtp, trt);
        if (!m) {
            fmt_reftype(rtp, rb, sizeof(char[64]));
            fmt_reftype(trt, sb, sizeof(char[64]));
            vmsg("type mismatch: element segment's type %s does not match "
                 "table's element type %s", rb, sb);
            return W89_ERR_INVALID;
        }
        tlim = &tt->limits;
        an = addr_num(tlim);
        addr = num_vt_from_byte(an);
        offp = &el->offset;
        on = offp->n;
        rp = &addr;
        e = check_const(ctx, offp, 0, on, rp);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }
    pel = &ctx->elems;
    pp = (void **)pel;
    pn = &ctx->nelems;
    pc = &ctx->elems_cap;
    ctx_push(pp, pn, pc, sizeof(w89_reftype), rtp, &e);
    return e;
}

static w89_err check_func_body(w89_valctx *ctx, const w89_func *f,
                               w89_u32 typeidx)
{
    w89_err e;
    const w89_vt *params;
    w89_u32 nparams;
    const w89_vt *results;
    w89_u32 nresults;
    w89_u32 j;
    w89_u32 base;
    int saved_ell;
    w89_u32 term;
    int is_else;
    w89_local l;
    int ok;
    int def;
    w89_u32 nl;
    const w89_vt *cvp;
    w89_vt tv;
    const w89_instr_vec *code;
    void **pp;
    w89_local **pl;
    w89_u32 *pn;
    w89_u32 *pc;
    e = func_type_by_typeidx(ctx, typeidx, &params, &nparams, &results,
                             &nresults);
    if (e != W89_ERR_NONE) {
        return e;
    }
    base = ctx->on;
    saved_ell = ctx->ell;
    ctx->nlocals = 0;
    ctx->nlabels = 0;
    cvp = results;
    ctx->results = cvp;
    ctx->nresults = nresults;
    for (j = 0; j < nparams; j = j + 1) {
        l.init = 1;
        cvp = &params[j];
        tv = *cvp;
        l.vt = tv;
        pl = &ctx->locals;
        pp = (void **)pl;
        pn = &ctx->nlocals;
        pc = &ctx->locals_cap;
        ok = ctx_push(pp, pn, pc, sizeof(w89_local), &l, &e);
        if (!ok) {
            return e;
        }
    }
    nl = f->nlocals;
    for (j = 0; j < nl; j = j + 1) {
        cvp = &f->locals[j];
        e = check_valtype_ref(ctx, cvp);
        if (e != W89_ERR_NONE) {
            return e;
        }
        def = vt_defaultable(cvp);
        if (def) {
            l.init = 1;
        } else {
            l.init = 0;
        }
        tv = *cvp;
        l.vt = tv;
        pl = &ctx->locals;
        pp = (void **)pl;
        pn = &ctx->nlocals;
        pc = &ctx->locals_cap;
        ok = ctx_push(pp, pn, pc, sizeof(w89_local), &l, &e);
        if (!ok) {
            return e;
        }
    }
    ctx->on = base;
    ctx->ell = W89_ELL_NO;
    code = &f->code;
    e = check_block_body(ctx, code, 0, 0, 0, results, nresults, 0,
                         &term, &is_else);
    ctx->on = base;
    ctx->ell = saved_ell;
    ctx->results = 0;
    ctx->nresults = 0;
    return e;
}

static void scan_ref_func(const w89_instr_vec *v, w89_valctx *ctx)
{
    w89_u32 i;
    w89_u32 n;
    w89_instr *arr;
    w89_instr *itm;
    w89_u32 op;
    w89_u32 idx;
    n = v->n;
    arr = v->items;
    for (i = 0; i < n; i = i + 1) {
        itm = &arr[i];
        op = itm->op;
        if (op == 0xD2) {
            idx = itm->idx;
            declared_add(ctx, idx);
        }
    }
}

static w89_err check_module_fields(w89_valctx *ctx, const w89_module *m)
{
    w89_u32 i;
    w89_err e;
    w89_u32 n;
    w89_u32 j;
    w89_u32 idx;
    w89_export *ex;
    w89_export *arr;
    const w89_import *im;
    const w89_tag *tag;
    const w89_table *tab;
    const w89_global *gl;
    const w89_data *data;
    const w89_elem *elem;
    const w89_func *fn;
    const w89_vt *params;
    w89_u32 nparams;
    const w89_vt *results;
    w89_u32 nresults;
    w89_tabletype *tt;
    w89_limits *lim;
    w89_globaltype *gt;
    w89_u32 ti;
    w89_u32 st;
    w89_u32 l1, l2;
    w89_name n1, n2;
    const w89_byte *b1;
    const w89_byte *b2;
    int eq;
    n = m->nimports;
    for (i = 0; i < n; i = i + 1) {
        im = &m->imports[i];
        e = check_import(ctx, im);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }
    n = m->ntags;
    for (i = 0; i < n; i = i + 1) {
        tag = &m->tags[i];
        e = check_tag(ctx, tag);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }
    n = m->nfuncs;
    for (i = 0; i < n; i = i + 1) {
        idx = m->func_types[i];
        e = check_func(ctx, idx);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }
    n = m->nmemories;
    for (i = 0; i < n; i = i + 1) {
        lim = &m->memories[i].type;
        e = check_memory(ctx, lim);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }
    n = m->ntables;
    for (i = 0; i < n; i = i + 1) {
        tab = &m->tables[i];
        e = check_table(ctx, tab);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }
    n = m->nglobals;
    for (i = 0; i < n; i = i + 1) {
        gl = &m->globals[i];
        e = check_global(ctx, gl);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }
    n = m->ndatas;
    for (i = 0; i < n; i = i + 1) {
        data = &m->datas[i];
        e = check_data(ctx, data);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }
    n = m->nelems;
    for (i = 0; i < n; i = i + 1) {
        elem = &m->elems[i];
        e = check_elem(ctx, elem);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }
    n = m->ncode;
    for (i = 0; i < n; i = i + 1) {
        idx = m->func_types[i];
        fn = &m->funcs[i];
        e = check_func_body(ctx, fn, idx);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }
    st = m->has_start;
    if (st) {
        idx = m->start;
        e = func_type(ctx, idx, &params, &nparams, &results, &nresults);
        if (e != W89_ERR_NONE) {
            return e;
        }
        if (nparams != 0) {
            vmsg("start function must not have parameters or results");
            return W89_ERR_INVALID;
        }
        if (nresults != 0) {
            vmsg("start function must not have parameters or results");
            return W89_ERR_INVALID;
        }
    }
    n = m->nexports;
    arr = m->exports;
    for (i = 0; i < n; i = i + 1) {
        ex = &arr[i];
        idx = ex->kind;
        switch (idx) {
        case 0x00: {
            idx = ex->index;
            e = func_type(ctx, idx, &params, &nparams, &results, &nresults);
            if (e != W89_ERR_NONE) {
                return e;
            }
            break;
        }
        case 0x01: {
            idx = ex->index;
            e = table_at(ctx, idx, &tt);
            if (e != W89_ERR_NONE) {
                return e;
            }
            break;
        }
        case 0x02: {
            idx = ex->index;
            e = memory_at(ctx, idx, &lim);
            if (e != W89_ERR_NONE) {
                return e;
            }
            break;
        }
        case 0x03: {
            idx = ex->index;
            e = global_at(ctx, idx, &gt);
            if (e != W89_ERR_NONE) {
                return e;
            }
            break;
        }
        default: {
            idx = ex->index;
            e = tag_type(ctx, idx, &ti);
            if (e != W89_ERR_NONE) {
                return e;
            }
            break;
        }
        }
    }
    n = m->nexports;
    for (i = 0; i < n; i = i + 1) {
        for (j = 0; j < i; j = j + 1) {
            n1 = m->exports[j].name;
            n2 = m->exports[i].name;
            l1 = n1.len;
            l2 = n2.len;
            if (l1 == l2) {
                b1 = n1.bytes;
                b2 = n2.bytes;
                eq = memcmp(b1, b2, l2);
                if (eq == 0) {
                    vmsg("duplicate export name");
                    return W89_ERR_INVALID;
                }
            }
        }
    }
    return W89_ERR_NONE;
}

w89_err w89_module_validate(const w89_module *m)
{
    w89_valctx ctx;
    w89_err e;
    w89_u32 i;
    w89_u32 n;
    const w89_elem *el;
    const w89_global *gl;
    const w89_table *tb;
    const w89_export *ex;
    w89_u32 fl;
    w89_u32 k;
    w89_u32 idx;
    w89_u32 vn;
    const w89_instr_vec *ivp;
    w89_typeenv *te;
    w89_vmsg[0] = '\0';
    ctx_init(&ctx);
    te = &ctx.types;
    e = w89_typeenv_build(m, te);
    if (e != W89_ERR_NONE) {
        ctx_free(&ctx);
        return e;
    }
    e = validate_type_section(te);
    if (e != W89_ERR_NONE) {
        ctx_free(&ctx);
        return e;
    }
    n = m->nglobals;
    for (i = 0; i < n; i = i + 1) {
        gl = &m->globals[i];
        ivp = &gl->init;
        scan_ref_func(ivp, &ctx);
    }
    n = m->ntables;
    for (i = 0; i < n; i = i + 1) {
        tb = &m->tables[i];
        ivp = &tb->init;
        scan_ref_func(ivp, &ctx);
    }
    n = m->nelems;
    for (i = 0; i < n; i = i + 1) {
        el = &m->elems[i];
        ivp = &el->offset;
        scan_ref_func(ivp, &ctx);
        fl = el->flags;
        if (fl < 4) {
            vn = el->n;
            for (k = 0; k < vn; k = k + 1) {
                idx = el->indices[k];
                declared_add(&ctx, idx);
            }
        } else {
            ivp = &el->exprs;
            scan_ref_func(ivp, &ctx);
        }
    }
    n = m->nexports;
    for (i = 0; i < n; i = i + 1) {
        ex = &m->exports[i];
        fl = ex->kind;
        if (fl == 0x00) {
            idx = ex->index;
            declared_add(&ctx, idx);
        }
    }
    e = check_module_fields(&ctx, m);
    ctx_free(&ctx);
    return e;
}

const char *w89_validate_message(void)
{
    return w89_vmsg;
}
