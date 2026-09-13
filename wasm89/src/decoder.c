#include <stdlib.h>
#include <string.h>
#include "numeric.h"
#include "module.h"

static w89_byte w89_last_illegal_op;

typedef struct w89_cur {
    const w89_byte *p;
    const w89_byte *end;
} w89_cur;

static w89_err read_reftype(w89_cur *c, w89_reftype *out);
static w89_err read_heaptype(w89_cur *c, w89_reftype *out);
static w89_err read_fieldtype(w89_cur *c, w89_fieldtype *out);
static w89_err decode_expr_to_vec(w89_cur *c, w89_instr_vec *v);

static w89_err cur_byte(w89_cur *c, w89_byte *out)
{
    const w89_byte *pp;
    const w89_byte *ep;
    const w89_byte *pn;
    w89_byte b;

    pp = c->p;
    ep = c->end;
    if (pp >= ep) {
        return W89_ERR_EOF;
    }
    b = *pp;
    *out = b;
    pn = pp + 1;
    c->p = pn;
    return W89_ERR_NONE;
}

static w89_err read_s7_byte(w89_cur *c, w89_byte *out)
{
    w89_err e;
    w89_byte b;

    e = cur_byte(c, out);
    if (e != W89_ERR_NONE) {
        return e;
    }
    b = *out;
    if (b & 0x80) {
        return W89_ERR_LEB_TOO_LONG;
    }
    return W89_ERR_NONE;
}

static w89_err cur_u32(w89_cur *c, w89_u32 *out)
{
    w89_u64 v;
    w89_err e;
    w89_u32 ov;
    const w89_byte **cpp;
    const w89_byte *ep;

    cpp = &c->p;
    ep = c->end;
    e = w89_leb_u_err(cpp, ep, 32, &v);
    if (e != W89_ERR_NONE) {
        return e;
    }
    ov = (w89_u32)v;
    *out = ov;
    return W89_ERR_NONE;
}

static w89_err cur_len(w89_cur *c, w89_u32 *out)
{
    w89_u64 remaining;
    w89_err e;
    w89_u64 ov;
    w89_u64 rem;
    w89_u32 lv;
    const w89_byte *ep;
    const w89_byte *pp;

    ep = c->end;
    pp = c->p;
    remaining = (w89_u64)(ep - pp);
    e = cur_u32(c, out);
    if (e != W89_ERR_NONE) {
        return e;
    }
    lv = *out;
    ov = (w89_u64)lv;
    rem = remaining;
    if (ov > rem) {
        return W89_ERR_LEN_OUT_OF_BOUNDS;
    }
    return W89_ERR_NONE;
}

static w89_err cur_u64(w89_cur *c, w89_u64 *out)
{
    const w89_byte **cpp;
    const w89_byte *ep;

    cpp = &c->p;
    ep = c->end;
    return w89_leb_u_err(cpp, ep, 64, out);
}

static w89_err cur_s33(w89_cur *c, w89_i64 *out)
{
    const w89_byte **cpp;
    const w89_byte *ep;

    cpp = &c->p;
    ep = c->end;
    return w89_leb_s_err(cpp, ep, 33, out);
}

static w89_err cur_s32(w89_cur *c, w89_i64 *out)
{
    const w89_byte **cpp;
    const w89_byte *ep;

    cpp = &c->p;
    ep = c->end;
    return w89_leb_s_err(cpp, ep, 32, out);
}

static w89_err cur_s64(w89_cur *c, w89_i64 *out)
{
    const w89_byte **cpp;
    const w89_byte *ep;

    cpp = &c->p;
    ep = c->end;
    return w89_leb_s_err(cpp, ep, 64, out);
}

static w89_err cur_bytes(w89_cur *c, w89_u32 n, const w89_byte **out)
{
    w89_u64 rem;
    w89_u64 nv;
    const w89_byte *pp;
    const w89_byte *ep;
    const w89_byte *pn;

    ep = c->end;
    pp = c->p;
    rem = (w89_u64)(ep - pp);
    nv = (w89_u64)n;
    if (rem < nv) {
        return W89_ERR_EOF;
    }
    pp = c->p;
    *out = pp;
    pn = pp + n;
    c->p = pn;
    return W89_ERR_NONE;
}

static int w89_valid_utf8(const w89_byte *p, w89_u32 len)
{
    w89_u32 i = 0;
    w89_byte b;
    w89_byte b1;
    w89_byte b2;
    w89_byte b3;
    w89_u32 i1;
    w89_u32 i2;
    w89_u32 i3;
    int r;

    while (i < len) {
        b = p[i];
        if (b < 0x80) {
            i = i + 1;
        } else {
            r = b >= 0xC2;
            if (r != 0) {
                r = b <= 0xDF;
            }
            if (r != 0) {
                i1 = i + 1;
                if (i1 >= len) {
                    return 0;
                }
                b1 = p[i1];
                if ((b1 & 0xC0) != 0x80) {
                    return 0;
                }
                i = i + 2;
            } else if (b == 0xE0) {
                i2 = i + 2;
                if (i2 >= len) {
                    return 0;
                }
                i1 = i + 1;
                b1 = p[i1];
                if (b1 < 0xA0) {
                    return 0;
                }
                if (b1 > 0xBF) {
                    return 0;
                }
                b2 = p[i2];
                if ((b2 & 0xC0) != 0x80) {
                    return 0;
                }
                i = i + 3;
            } else {
                r = b >= 0xE1;
                if (r != 0) {
                    r = b <= 0xEC;
                }
                if (r != 0) {
                    i2 = i + 2;
                    if (i2 >= len) {
                        return 0;
                    }
                    i1 = i + 1;
                    b1 = p[i1];
                    if ((b1 & 0xC0) != 0x80) {
                        return 0;
                    }
                    b2 = p[i2];
                    if ((b2 & 0xC0) != 0x80) {
                        return 0;
                    }
                    i = i + 3;
                } else if (b == 0xED) {
                    i2 = i + 2;
                    if (i2 >= len) {
                        return 0;
                    }
                    i1 = i + 1;
                    b1 = p[i1];
                    if (b1 < 0x80) {
                        return 0;
                    }
                    if (b1 > 0x9F) {
                        return 0;
                    }
                    b2 = p[i2];
                    if ((b2 & 0xC0) != 0x80) {
                        return 0;
                    }
                    i = i + 3;
                } else {
                    r = b >= 0xEE;
                    if (r != 0) {
                        r = b <= 0xEF;
                    }
                    if (r != 0) {
                        i2 = i + 2;
                        if (i2 >= len) {
                            return 0;
                        }
                        i1 = i + 1;
                        b1 = p[i1];
                        if ((b1 & 0xC0) != 0x80) {
                            return 0;
                        }
                        b2 = p[i2];
                        if ((b2 & 0xC0) != 0x80) {
                            return 0;
                        }
                        i = i + 3;
                    } else if (b == 0xF0) {
                        i3 = i + 3;
                        if (i3 >= len) {
                            return 0;
                        }
                        i1 = i + 1;
                        b1 = p[i1];
                        if (b1 < 0x90) {
                            return 0;
                        }
                        if (b1 > 0xBF) {
                            return 0;
                        }
                        i2 = i + 2;
                        b2 = p[i2];
                        if ((b2 & 0xC0) != 0x80) {
                            return 0;
                        }
                        b3 = p[i3];
                        if ((b3 & 0xC0) != 0x80) {
                            return 0;
                        }
                        i = i + 4;
                    } else {
                        r = b >= 0xF1;
                        if (r != 0) {
                            r = b <= 0xF3;
                        }
                        if (r != 0) {
                            i3 = i + 3;
                            if (i3 >= len) {
                                return 0;
                            }
                            i1 = i + 1;
                            b1 = p[i1];
                            if ((b1 & 0xC0) != 0x80) {
                                return 0;
                            }
                            i2 = i + 2;
                            b2 = p[i2];
                            if ((b2 & 0xC0) != 0x80) {
                                return 0;
                            }
                            b3 = p[i3];
                            if ((b3 & 0xC0) != 0x80) {
                                return 0;
                            }
                            i = i + 4;
                        } else if (b == 0xF4) {
                            i3 = i + 3;
                            if (i3 >= len) {
                                return 0;
                            }
                            i1 = i + 1;
                            b1 = p[i1];
                            if (b1 < 0x80) {
                                return 0;
                            }
                            if (b1 > 0x8F) {
                                return 0;
                            }
                            i2 = i + 2;
                            b2 = p[i2];
                            if ((b2 & 0xC0) != 0x80) {
                                return 0;
                            }
                            b3 = p[i3];
                            if ((b3 & 0xC0) != 0x80) {
                                return 0;
                            }
                            i = i + 4;
                        } else {
                            return 0;
                        }
                    }
                }
            }
        }
    }
    return 1;
}

static w89_err cur_name(w89_cur *c, w89_name *out)
{
    w89_u32 len;
    w89_err e;
    const w89_byte *ob;
    const w89_byte **obp;
    int ok;

    e = cur_len(c, &len);
    if (e != W89_ERR_NONE) {
        return e;
    }
    out->len = len;
    obp = &out->bytes;
    e = cur_bytes(c, len, obp);
    if (e != W89_ERR_NONE) {
        return e;
    }
    ob = out->bytes;
    ok = w89_valid_utf8(ob, len);
    if (ok == 0) {
        return W89_ERR_UTF8;
    }
    return W89_ERR_NONE;
}

static void *w89_xmalloc(w89_u32 n, w89_err *err)
{
    void *p;

    p = malloc(n);
    if (p == 0) {
        *err = W89_ERR_OUT_OF_MEMORY;
    }
    return p;
}

static w89_err read_valtype(w89_cur *c, w89_vt *out)
{
    w89_byte b;
    w89_err e;
    w89_byte bn;
    w89_absheaptype ab;
    w89_reftype *rtp;

    e = read_s7_byte(c, &b);
    if (e != W89_ERR_NONE) {
        return e;
    }
    if (b >= 0x7B) {
        if (b <= 0x7F) {
            out->is_ref = 0;
            out->num = b;
            return W89_ERR_NONE;
        }
    }
    if (b == 0x63) {
        out->is_ref = 1;
        out->num = b;
        out->rt.nullable = 1;
        rtp = &out->rt;
        return read_heaptype(c, rtp);
    }
    if (b == 0x64) {
        out->is_ref = 1;
        out->num = b;
        out->rt.nullable = 0;
        rtp = &out->rt;
        return read_heaptype(c, rtp);
    }
    if (b >= 0x69) {
        if (b <= 0x74) {
            out->is_ref = 1;
            out->num = b;
            out->rt.nullable = 1;
            out->rt.is_typeidx = 0;
            bn = b;
            ab = (w89_absheaptype)bn;
            out->rt.abs = ab;
            return W89_ERR_NONE;
        }
    }
    return W89_ERR_MALFORMED_TYPE;
}

static w89_err read_storage(w89_cur *c, w89_fieldtype *out)
{
    w89_byte b;
    w89_err e;
    w89_byte bn;
    w89_packed pk;
    w89_absheaptype ab;
    w89_reftype *rtp;

    e = read_s7_byte(c, &b);
    if (e != W89_ERR_NONE) {
        return e;
    }
    if (b == 0x78) {
        out->is_packed = 1;
        pk = (w89_packed)b;
        out->packed = pk;
        return W89_ERR_NONE;
    }
    if (b == 0x77) {
        out->is_packed = 1;
        pk = (w89_packed)b;
        out->packed = pk;
        return W89_ERR_NONE;
    }
    if (b >= 0x7B) {
        if (b <= 0x7F) {
            out->is_packed = 0;
            out->vt.is_ref = 0;
            out->vt.num = b;
            return W89_ERR_NONE;
        }
    }
    if (b == 0x63) {
        out->is_packed = 0;
        out->vt.is_ref = 1;
        out->vt.num = b;
        out->vt.rt.nullable = 1;
        rtp = &out->vt.rt;
        return read_heaptype(c, rtp);
    }
    if (b == 0x64) {
        out->is_packed = 0;
        out->vt.is_ref = 1;
        out->vt.num = b;
        out->vt.rt.nullable = 0;
        rtp = &out->vt.rt;
        return read_heaptype(c, rtp);
    }
    if (b >= 0x69) {
        if (b <= 0x74) {
            out->is_packed = 0;
            out->vt.is_ref = 1;
            out->vt.num = b;
            out->vt.rt.nullable = 1;
            out->vt.rt.is_typeidx = 0;
            bn = b;
            ab = (w89_absheaptype)bn;
            out->vt.rt.abs = ab;
            return W89_ERR_NONE;
        }
    }
    return W89_ERR_MALFORMED_TYPE;
}

static w89_err read_heaptype(w89_cur *c, w89_reftype *out)
{
    w89_i64 s;
    w89_err e;
    w89_u32 tv;
    w89_absheaptype ab;

    e = cur_s33(c, &s);
    if (e != W89_ERR_NONE) {
        return e;
    }
    if (s >= 0) {
        out->is_typeidx = 1;
        tv = (w89_u32)s;
        out->typeidx = tv;
        return W89_ERR_NONE;
    }
    switch (s) {
    case -12: ab = W89_HT_NOEXN; out->abs = ab; break;
    case -13: ab = W89_HT_NOFUNC; out->abs = ab; break;
    case -14: ab = W89_HT_NOEXTERN; out->abs = ab; break;
    case -15: ab = W89_HT_NONE; out->abs = ab; break;
    case -16: ab = W89_HT_FUNC; out->abs = ab; break;
    case -17: ab = W89_HT_EXTERN; out->abs = ab; break;
    case -18: ab = W89_HT_ANY; out->abs = ab; break;
    case -19: ab = W89_HT_EQ; out->abs = ab; break;
    case -20: ab = W89_HT_I31; out->abs = ab; break;
    case -21: ab = W89_HT_STRUCT; out->abs = ab; break;
    case -22: ab = W89_HT_ARRAY; out->abs = ab; break;
    case -23: ab = W89_HT_EXN; out->abs = ab; break;
    default: return W89_ERR_MALFORMED_REF;
    }
    out->is_typeidx = 0;
    return W89_ERR_NONE;
}

static w89_err read_reftype(w89_cur *c, w89_reftype *out)
{
    w89_byte b;
    w89_err e;
    w89_byte bn;
    w89_absheaptype ab;

    e = read_s7_byte(c, &b);
    if (e != W89_ERR_NONE) {
        return e;
    }
    if (b >= 0x69) {
        if (b <= 0x74) {
            out->nullable = 1;
            out->is_typeidx = 0;
            bn = b;
            ab = (w89_absheaptype)bn;
            out->abs = ab;
            return W89_ERR_NONE;
        }
    }
    if (b == 0x63) {
        out->nullable = 1;
        return read_heaptype(c, out);
    }
    if (b == 0x64) {
        out->nullable = 0;
        return read_heaptype(c, out);
    }
    return W89_ERR_MALFORMED_REF;
}

static w89_err read_limits(w89_cur *c, w89_limits *out)
{
    w89_byte flag;
    w89_err e;
    w89_u64 minv;
    w89_u64 maxv;
    w89_u64 *mvp;
    w89_u32 hm;

    e = cur_byte(c, &flag);
    if (e != W89_ERR_NONE) {
        return e;
    }
    if (flag == 0x00) {
        out->addr64 = 0;
        out->has_max = 0;
    } else if (flag == 0x01) {
        out->addr64 = 0;
        out->has_max = 1;
    } else if (flag == 0x04) {
        out->addr64 = 1;
        out->has_max = 0;
    } else if (flag == 0x05) {
        out->addr64 = 1;
        out->has_max = 1;
    } else {
        return W89_ERR_MALFORMED_LIMITS;
    }
    mvp = &out->min;
    e = cur_u64(c, mvp);
    if (e != W89_ERR_NONE) {
        return e;
    }
    hm = out->has_max;
    if (hm != 0) {
        mvp = &out->max;
        e = cur_u64(c, mvp);
        if (e != W89_ERR_NONE) {
            return e;
        }
        minv = out->min;
        maxv = out->max;
        if (maxv < minv) {
            return W89_ERR_MIN_GT_MAX;
        }
    }
    return W89_ERR_NONE;
}

static w89_err read_globaltype(w89_cur *c, w89_globaltype *out)
{
    w89_byte mut;
    w89_err e;
    w89_vt *vp;

    vp = &out->vt;
    e = read_valtype(c, vp);
    if (e != W89_ERR_NONE) {
        return e;
    }
    e = cur_byte(c, &mut);
    if (e != W89_ERR_NONE) {
        return e;
    }
    if (mut == 0x00) {
        out->mut = 0;
    } else if (mut == 0x01) {
        out->mut = 1;
    } else {
        return W89_ERR_MALFORMED_MUT;
    }
    return W89_ERR_NONE;
}

static w89_err read_tabletype(w89_cur *c, w89_tabletype *out)
{
    w89_err e;
    w89_reftype *rp;
    w89_limits *lp;

    rp = &out->rt;
    e = read_reftype(c, rp);
    if (e != W89_ERR_NONE) {
        return e;
    }
    lp = &out->limits;
    return read_limits(c, lp);
}

static w89_err read_ft(w89_cur *c, w89_ft *out, w89_err *err)
{
    w89_u32 i, n;
    w89_err e;
    w89_u32 sz;
    w89_vt *p;
    w89_vt *elp;
    w89_err er;

    out->params = 0;
    out->results = 0;
    out->nparams = 0;
    out->nresults = 0;
    e = cur_len(c, &n);
    if (e != W89_ERR_NONE) {
        return e;
    }
    if (n != 0) {
        sz = n * sizeof(w89_vt);
        p = w89_xmalloc(sz, err);
        out->params = p;
        er = *err;
        if (er != W89_ERR_NONE) {
            return er;
        }
        for (i = 0; i < n; i = i + 1) {
            elp = &out->params[i];
            e = read_valtype(c, elp);
            if (e != W89_ERR_NONE) {
                return e;
            }
        }
        out->nparams = n;
    }
    e = cur_len(c, &n);
    if (e != W89_ERR_NONE) {
        return e;
    }
    if (n != 0) {
        sz = n * sizeof(w89_vt);
        p = w89_xmalloc(sz, err);
        out->results = p;
        er = *err;
        if (er != W89_ERR_NONE) {
            return er;
        }
        for (i = 0; i < n; i = i + 1) {
            elp = &out->results[i];
            e = read_valtype(c, elp);
            if (e != W89_ERR_NONE) {
                return e;
            }
        }
        out->nresults = n;
    }
    return W89_ERR_NONE;
}

static w89_err read_comptype(w89_cur *c, w89_subtype *out, w89_err *err)
{
    w89_byte b;
    w89_u32 i, n;
    w89_err e;
    w89_u32 sz;
    w89_byte mut;
    w89_ft *fp;
    w89_compkind k;
    w89_fieldtype *fl;
    w89_fieldtype *p;
    w89_err er;

    e = read_s7_byte(c, &b);
    if (e != W89_ERR_NONE) {
        return e;
    }
    if (b == 0x60) {
        out->kind = W89_CK_FUNC;
        fp = &out->ft;
        return read_ft(c, fp, err);
    }
    if (b == 0x5F) {
        out->kind = W89_CK_STRUCT;
    } else if (b == 0x5E) {
        out->kind = W89_CK_ARRAY;
    } else {
        return W89_ERR_MALFORMED_TYPE;
    }
    k = out->kind;
    if (k == W89_CK_ARRAY) {
        out->fields = 0;
        out->nfields = 0;
        sz = 1 * sizeof(w89_fieldtype);
        p = w89_xmalloc(sz, err);
        out->fields = p;
        er = *err;
        if (er != W89_ERR_NONE) {
            return er;
        }
        out->nfields = 1;
        fl = &out->fields[0];
        return read_fieldtype(c, fl);
    }
    e = cur_len(c, &n);
    if (e != W89_ERR_NONE) {
        return e;
    }
    out->fields = 0;
    out->nfields = 0;
    if (n != 0) {
        sz = n * sizeof(w89_fieldtype);
        p = w89_xmalloc(sz, err);
        out->fields = p;
        er = *err;
        if (er != W89_ERR_NONE) {
            return er;
        }
        for (i = 0; i < n; i = i + 1) {
            fl = &out->fields[i];
            e = read_storage(c, fl);
            if (e != W89_ERR_NONE) {
                return e;
            }
            e = cur_byte(c, &mut);
            if (e != W89_ERR_NONE) {
                return e;
            }
            if (mut == 0x00) {
                out->fields[i].mut = 0;
            } else if (mut == 0x01) {
                out->fields[i].mut = 1;
            } else {
                return W89_ERR_MALFORMED_MUT;
            }
        }
        out->nfields = n;
    }
    return W89_ERR_NONE;
}

static w89_err read_fieldtype(w89_cur *c, w89_fieldtype *out)
{
    w89_byte mut;
    w89_err e;

    e = read_storage(c, out);
    if (e != W89_ERR_NONE) {
        return e;
    }
    e = cur_byte(c, &mut);
    if (e != W89_ERR_NONE) {
        return e;
    }
    if (mut == 0x00) {
        out->mut = 0;
    } else if (mut == 0x01) {
        out->mut = 1;
    } else {
        return W89_ERR_MALFORMED_MUT;
    }
    return W89_ERR_NONE;
}

static w89_err read_subtype(w89_cur *c, w89_subtype *out, w89_err *err)
{
    w89_byte b;
    w89_u32 i, n;
    w89_err e;
    w89_u32 sz;
    w89_u32 *p;
    w89_u32 *sp;
    w89_err er;
    const w89_byte *pp;

    e = read_s7_byte(c, &b);
    if (e != W89_ERR_NONE) {
        return e;
    }
    out->supertypes = 0;
    out->nsupers = 0;
    out->fields = 0;
    out->nfields = 0;
    if (b == 0x50) {
        out->is_final = 0;
        e = cur_len(c, &n);
        if (e != W89_ERR_NONE) {
            return e;
        }
        if (n != 0) {
            sz = n * sizeof(w89_u32);
            p = w89_xmalloc(sz, err);
            out->supertypes = p;
            er = *err;
            if (er != W89_ERR_NONE) {
                return er;
            }
            for (i = 0; i < n; i = i + 1) {
                sp = &out->supertypes[i];
                e = cur_u32(c, sp);
                if (e != W89_ERR_NONE) {
                    return e;
                }
            }
            out->nsupers = n;
        }
        return read_comptype(c, out, err);
    }
    if (b == 0x4F) {
        out->is_final = 1;
        e = cur_len(c, &n);
        if (e != W89_ERR_NONE) {
            return e;
        }
        if (n != 0) {
            sz = n * sizeof(w89_u32);
            p = w89_xmalloc(sz, err);
            out->supertypes = p;
            er = *err;
            if (er != W89_ERR_NONE) {
                return er;
            }
            for (i = 0; i < n; i = i + 1) {
                sp = &out->supertypes[i];
                e = cur_u32(c, sp);
                if (e != W89_ERR_NONE) {
                    return e;
                }
            }
            out->nsupers = n;
        }
        return read_comptype(c, out, err);
    }
    pp = c->p;
    pp = pp - 1;
    c->p = pp;
    out->is_final = 1;
    return read_comptype(c, out, err);
}

static w89_err read_rectype(w89_cur *c, w89_rectype *out, w89_err *err)
{
    w89_byte b;
    w89_u32 i, n;
    w89_err e;
    w89_u32 sz;
    size_t stsz;
    w89_subtype *sp;
    w89_subtype *p;
    w89_err er;
    const w89_byte *pp;
    w89_u32 ni;

    e = read_s7_byte(c, &b);
    if (e != W89_ERR_NONE) {
        return e;
    }
    if (b == 0x4E) {
        e = cur_len(c, &n);
        if (e != W89_ERR_NONE) {
            return e;
        }
        out->subtypes = 0;
        out->n = 0;
        if (n != 0) {
            sz = n * sizeof(w89_subtype);
            p = w89_xmalloc(sz, err);
            out->subtypes = p;
            er = *err;
            if (er != W89_ERR_NONE) {
                return er;
            }
            stsz = sizeof(w89_subtype);
            for (i = 0; i < n; i = i + 1) {
                sp = &out->subtypes[i];
                memset(sp, 0, stsz);
                ni = i + 1;
                out->n = ni;
                e = read_subtype(c, sp, err);
                if (e != W89_ERR_NONE) {
                    return e;
                }
            }
        }
        out->n = n;
        return W89_ERR_NONE;
    }
    pp = c->p;
    pp = pp - 1;
    c->p = pp;
    sz = 1 * sizeof(w89_subtype);
    p = w89_xmalloc(sz, err);
    out->subtypes = p;
    er = *err;
    if (er != W89_ERR_NONE) {
        return er;
    }
    stsz = sizeof(w89_subtype);
    sp = &out->subtypes[0];
    memset(sp, 0, stsz);
    out->n = 1;
    e = read_subtype(c, sp, err);
    if (e != W89_ERR_NONE) {
        return e;
    }
    out->n = 1;
    return W89_ERR_NONE;
}

static w89_err decode_imports(w89_cur *c, w89_module *m, w89_err *err)
{
    w89_u32 n, i;
    w89_err e;
    w89_u32 sz;
    w89_byte kind;
    w89_byte attr;
    w89_import *im;
    w89_import *p;
    w89_err er;
    w89_name *np;
    w89_u32 *tp;
    w89_tabletype *tt;
    w89_limits *lm;
    w89_globaltype *gp;

    e = cur_len(c, &n);
    if (e != W89_ERR_NONE) {
        return e;
    }
    m->imports = 0;
    if (n != 0) {
        sz = n * sizeof(w89_import);
        p = w89_xmalloc(sz, err);
        m->imports = p;
        er = *err;
        if (er != W89_ERR_NONE) {
            return er;
        }
    }
    for (i = 0; i < n; i = i + 1) {
        im = &m->imports[i];
        memset(im, 0, sizeof(w89_import));
        np = &im->module;
        e = cur_name(c, np);
        if (e != W89_ERR_NONE) {
            return e;
        }
        np = &im->name;
        e = cur_name(c, np);
        if (e != W89_ERR_NONE) {
            return e;
        }
        e = cur_byte(c, &kind);
        if (e != W89_ERR_NONE) {
            return e;
        }
        im->kind = kind;
        if (kind == 0x00) {
            tp = &im->typeidx;
            e = cur_u32(c, tp);
            if (e != W89_ERR_NONE) {
                return e;
            }
        } else if (kind == 0x04) {
            e = cur_byte(c, &attr);
            if (e != W89_ERR_NONE) {
                return e;
            }
            if (attr != 0x00) {
                return W89_ERR_UNSUPPORTED;
            }
            tp = &im->typeidx;
            e = cur_u32(c, tp);
            if (e != W89_ERR_NONE) {
                return e;
            }
        } else if (kind == 0x01) {
            tt = &im->table;
            e = read_tabletype(c, tt);
            if (e != W89_ERR_NONE) {
                return e;
            }
        } else if (kind == 0x02) {
            lm = &im->mem;
            e = read_limits(c, lm);
            if (e != W89_ERR_NONE) {
                return e;
            }
        } else if (kind == 0x03) {
            gp = &im->global;
            e = read_globaltype(c, gp);
            if (e != W89_ERR_NONE) {
                return e;
            }
        } else {
            return W89_ERR_MALFORMED_IMPORT_KIND;
        }
    }
    m->nimports = n;
    return W89_ERR_NONE;
}

static w89_err decode_funcsec(w89_cur *c, w89_module *m, w89_err *err)
{
    w89_u32 n, i;
    w89_err e;
    w89_u32 sz;
    w89_u32 *p;
    w89_u32 *tp;
    w89_err er;

    e = cur_len(c, &n);
    if (e != W89_ERR_NONE) {
        return e;
    }
    m->func_types = 0;
    if (n != 0) {
        sz = n * sizeof(w89_u32);
        p = w89_xmalloc(sz, err);
        m->func_types = p;
        er = *err;
        if (er != W89_ERR_NONE) {
            return er;
        }
    }
    for (i = 0; i < n; i = i + 1) {
        tp = &m->func_types[i];
        e = cur_u32(c, tp);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }
    m->nfuncs = n;
    return W89_ERR_NONE;
}

static w89_err decode_tablesec(w89_cur *c, w89_module *m, w89_err *err)
{
    w89_u32 n, i;
    w89_err e;
    w89_u32 sz;
    w89_table *t;
    w89_u32 prefix;
    const w89_byte *pp;
    const w89_byte *ep;
    const w89_byte *pn;
    w89_byte b;
    w89_table *p;
    w89_err er;
    w89_tabletype *tt;
    w89_instr_vec *iv;

    e = cur_len(c, &n);
    if (e != W89_ERR_NONE) {
        return e;
    }
    m->tables = 0;
    if (n != 0) {
        sz = n * sizeof(w89_table);
        p = w89_xmalloc(sz, err);
        m->tables = p;
        er = *err;
        if (er != W89_ERR_NONE) {
            return er;
        }
    }
    for (i = 0; i < n; i = i + 1) {
        t = &m->tables[i];
        prefix = 0;
        memset(t, 0, sizeof(w89_table));
        pp = c->p;
        ep = c->end;
        if (pp < ep) {
            b = c->p[0];
            if (b == 0x40) {
                prefix = 1;
                pn = pp + 1;
                c->p = pn;
                pp = c->p;
                ep = c->end;
                if (pp >= ep) {
                    return W89_ERR_EOF;
                }
                b = *pp;
                if (b != 0x00) {
                    return W89_ERR_UNSUPPORTED;
                }
                pn = pp + 1;
                c->p = pn;
            }
        }
        tt = &t->type;
        e = read_tabletype(c, tt);
        if (e != W89_ERR_NONE) {
            return e;
        }
        if (prefix != 0) {
            iv = &t->init;
            e = decode_expr_to_vec(c, iv);
            if (e != W89_ERR_NONE) {
                return e;
            }
        }
    }
    m->ntables = n;
    return W89_ERR_NONE;
}

static w89_err decode_memsec(w89_cur *c, w89_module *m, w89_err *err)
{
    w89_u32 n, i;
    w89_err e;
    w89_u32 sz;
    w89_memory *p;
    w89_err er;
    w89_limits *lm;

    e = cur_len(c, &n);
    if (e != W89_ERR_NONE) {
        return e;
    }
    m->memories = 0;
    if (n != 0) {
        sz = n * sizeof(w89_memory);
        p = w89_xmalloc(sz, err);
        m->memories = p;
        er = *err;
        if (er != W89_ERR_NONE) {
            return er;
        }
    }
    for (i = 0; i < n; i = i + 1) {
        lm = &m->memories[i].type;
        e = read_limits(c, lm);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }
    m->nmemories = n;
    return W89_ERR_NONE;
}

static w89_err decode_globalsec(w89_cur *c, w89_module *m, w89_err *err)
{
    w89_u32 n, i;
    w89_err e;
    w89_u32 sz;
    w89_global *g;
    w89_global *p;
    w89_err er;
    w89_globaltype *gp;
    w89_instr_vec *iv;

    e = cur_len(c, &n);
    if (e != W89_ERR_NONE) {
        return e;
    }
    m->globals = 0;
    if (n != 0) {
        sz = n * sizeof(w89_global);
        p = w89_xmalloc(sz, err);
        m->globals = p;
        er = *err;
        if (er != W89_ERR_NONE) {
            return er;
        }
    }
    for (i = 0; i < n; i = i + 1) {
        g = &m->globals[i];
        memset(g, 0, sizeof(w89_global));
        gp = &g->type;
        e = read_globaltype(c, gp);
        if (e != W89_ERR_NONE) {
            return e;
        }
        iv = &g->init;
        e = decode_expr_to_vec(c, iv);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }
    m->nglobals = n;
    return W89_ERR_NONE;
}

static w89_err decode_exportsec(w89_cur *c, w89_module *m, w89_err *err)
{
    w89_u32 n, i, j;
    w89_err e;
    w89_u32 sz;
    w89_byte kind;
    w89_export *ex;
    w89_u32 nl;
    w89_u32 el;
    const w89_byte *eb;
    const w89_byte *xb;
    int eq;
    w89_export *p;
    w89_err er;
    w89_name *np;
    w89_u32 *tp;

    e = cur_len(c, &n);
    if (e != W89_ERR_NONE) {
        return e;
    }
    m->exports = 0;
    if (n != 0) {
        sz = n * sizeof(w89_export);
        p = w89_xmalloc(sz, err);
        m->exports = p;
        er = *err;
        if (er != W89_ERR_NONE) {
            return er;
        }
    }
    for (i = 0; i < n; i = i + 1) {
        ex = &m->exports[i];
        np = &ex->name;
        e = cur_name(c, np);
        if (e != W89_ERR_NONE) {
            return e;
        }
        for (j = 0; j < i; j = j + 1) {
            nl = m->exports[j].name.len;
            el = ex->name.len;
            if (nl == el) {
                eb = m->exports[j].name.bytes;
                xb = ex->name.bytes;
                eq = memcmp(eb, xb, el);
                if (eq == 0) {
                    return W89_ERR_DUP_EXPORT;
                }
            }
        }
        e = cur_byte(c, &kind);
        if (e != W89_ERR_NONE) {
            return e;
        }
        if (kind > 0x04) {
            return W89_ERR_MALFORMED_EXPORT_KIND;
        }
        ex->kind = kind;
        tp = &ex->index;
        e = cur_u32(c, tp);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }
    m->nexports = n;
    return W89_ERR_NONE;
}

static w89_err decode_startsec(w89_cur *c, w89_module *m, w89_err *err)
{
    w89_err e;
    w89_u32 *tp;

    (void)err;
    tp = &m->start;
    e = cur_u32(c, tp);
    if (e != W89_ERR_NONE) {
        return e;
    }
    m->has_start = 1;
    return W89_ERR_NONE;
}

static w89_err read_catch(w89_cur *c, w89_catch *out);
static w89_err instr_push(w89_instr_vec *v, const w89_instr *in);
static w89_err decode_instr_block(w89_cur *c, w89_instr_vec *v);

static w89_err decode_elems(w89_cur *c, w89_module *m, w89_err *err)
{
    w89_u32 n, i;
    w89_err e;
    w89_u32 sz;
    w89_elem *el;
    w89_u32 flags;
    w89_byte ek;
    w89_u32 cnt, k;
    size_t ivsz;
    w89_elem *p;
    w89_err er;
    w89_u32 *tp;
    w89_instr_vec *iv;
    w89_reftype *rp;
    w89_u32 *ip;

    e = cur_len(c, &n);
    if (e != W89_ERR_NONE) {
        return e;
    }
    m->elems = 0;
    if (n != 0) {
        sz = n * sizeof(w89_elem);
        p = w89_xmalloc(sz, err);
        m->elems = p;
        er = *err;
        if (er != W89_ERR_NONE) {
            return er;
        }
    }
    ivsz = sizeof(w89_instr_vec);
    for (i = 0; i < n; i = i + 1) {
        el = &m->elems[i];
        memset(el, 0, sizeof(w89_elem));
        e = cur_u32(c, &flags);
        if (e != W89_ERR_NONE) {
            return e;
        }
        if (flags > 7) {
            return W89_ERR_UNSUPPORTED;
        }
        el->flags = flags;
        if (flags == 0) {
            el->tableidx = 0;
        } else if (flags == 4) {
            el->tableidx = 0;
        } else if (flags == 2) {
            tp = &el->tableidx;
            e = cur_u32(c, tp);
            if (e != W89_ERR_NONE) {
                return e;
            }
        } else if (flags == 6) {
            tp = &el->tableidx;
            e = cur_u32(c, tp);
            if (e != W89_ERR_NONE) {
                return e;
            }
        }
        if (flags == 0) {
            iv = &el->offset;
            e = decode_expr_to_vec(c, iv);
            if (e != W89_ERR_NONE) {
                return e;
            }
        } else if (flags == 2) {
            iv = &el->offset;
            e = decode_expr_to_vec(c, iv);
            if (e != W89_ERR_NONE) {
                return e;
            }
        } else if (flags == 4) {
            iv = &el->offset;
            e = decode_expr_to_vec(c, iv);
            if (e != W89_ERR_NONE) {
                return e;
            }
        } else if (flags == 6) {
            iv = &el->offset;
            e = decode_expr_to_vec(c, iv);
            if (e != W89_ERR_NONE) {
                return e;
            }
        }
        if (flags & 4) {
            if (flags == 4) {
                el->rt.is_typeidx = 0;
                el->rt.abs = W89_HT_FUNC;
                el->rt.nullable = 1;
            } else {
                rp = &el->rt;
                e = read_reftype(c, rp);
                if (e != W89_ERR_NONE) {
                    return e;
                }
            }
        } else if (flags == 0) {
            el->rt.is_typeidx = 0;
            el->rt.abs = W89_HT_FUNC;
            el->rt.nullable = 0;
        } else {
            e = cur_byte(c, &ek);
            if (e != W89_ERR_NONE) {
                return e;
            }
            if (ek != 0x00) {
                return W89_ERR_MALFORMED_REF;
            }
            el->rt.is_typeidx = 0;
            el->rt.abs = W89_HT_FUNC;
            el->rt.nullable = 0;
        }
        if (flags < 4) {
            e = cur_len(c, &cnt);
            if (e != W89_ERR_NONE) {
                return e;
            }
            el->n = cnt;
            if (cnt != 0) {
                sz = cnt * sizeof(w89_u32);
                ip = w89_xmalloc(sz, err);
                el->indices = ip;
                er = *err;
                if (er != W89_ERR_NONE) {
                    return er;
                }
            }
            for (k = 0; k < cnt; k = k + 1) {
                tp = &el->indices[k];
                e = cur_u32(c, tp);
                if (e != W89_ERR_NONE) {
                    return e;
                }
            }
        } else {
            e = cur_len(c, &cnt);
            if (e != W89_ERR_NONE) {
                return e;
            }
            el->n = cnt;
            iv = &el->exprs;
            memset(iv, 0, ivsz);
            for (k = 0; k < cnt; k = k + 1) {
                w89_instr in;
                e = decode_expr_to_vec(c, iv);
                if (e != W89_ERR_NONE) {
                    if (e == W89_ERR_EOF) {
                        return W89_ERR_EOF_SECTION;
                    }
                    return e;
                }
                memset(&in, 0, sizeof(w89_instr));
                in.op = 0x0B;
                e = instr_push(iv, &in);
                if (e != W89_ERR_NONE) {
                    return e;
                }
            }
            el->is_expr = 1;
        }
    }
    m->nelems = n;
    return W89_ERR_NONE;
}

static w89_err decode_codesec(w89_cur *c, w89_module *m, w89_err *err)
{
    w89_u32 n, i;
    w89_err e;
    w89_u32 sz;
    w89_u32 body_len, decls, d;
    w89_u64 total;
    w89_u32 *lcounts;
    w89_vt *ltypes;
    w89_func *f;
    const w89_byte *body_end;
    w89_u32 consumed_1;
    w89_cur sc;
    w89_u32 k;
    w89_u64 nv;
    w89_u32 lc;
    w89_vt lt;
    w89_func *p;
    w89_err er;
    const w89_byte *pp;
    const w89_byte *ep;
    const w89_byte *sp;
    w89_u32 *lcp;
    w89_vt *ltp;
    w89_vt *lp;
    w89_u32 lcv;
    w89_u32 nl;
    w89_u64 nv2;
    w89_u32 tc;
    w89_u32 ix;
    const w89_byte *p0;
    const w89_byte *sp0;
    const w89_byte *ep0;
    long bd0;
    long bd1;
    w89_u32 win;
    w89_u32 bw;
    w89_instr_vec *code;

    e = cur_len(c, &n);
    if (e != W89_ERR_NONE) {
        return e;
    }
    m->funcs = 0;
    if (n != 0) {
        sz = n * sizeof(w89_func);
        p = w89_xmalloc(sz, err);
        m->funcs = p;
        er = *err;
        if (er != W89_ERR_NONE) {
            return er;
        }
    }
    for (i = 0; i < n; i = i + 1) {
        total = 0;
        lcounts = 0;
        ltypes = 0;
        f = &m->funcs[i];
        memset(f, 0, sizeof(w89_func));
        e = cur_len(c, &body_len);
        if (e != W89_ERR_NONE) {
            return e;
        }
        pp = c->p;
        body_end = pp + body_len;
        sc.p = pp;
        ep = c->end;
        sc.end = ep;
        e = cur_len(&sc, &decls);
        if (e != W89_ERR_NONE) {
            return e;
        }
        if (decls != 0) {
            sz = decls * sizeof(w89_u32);
            lcounts = w89_xmalloc(sz, err);
            er = *err;
            if (er != W89_ERR_NONE) {
                return er;
            }
            sz = decls * sizeof(w89_vt);
            ltypes = w89_xmalloc(sz, err);
            er = *err;
            if (er != W89_ERR_NONE) {
                return er;
            }
        }
        for (d = 0; d < decls; d = d + 1) {
            lcp = &lcounts[d];
            e = cur_u32(&sc, lcp);
            if (e != W89_ERR_NONE) {
                return e;
            }
            lcv = lcounts[d];
            nv = (w89_u64)lcv;
            total = total + nv;
            if (total >= 0x100000000UL) {
                return W89_ERR_TOO_MANY_LOCALS;
            }
            ltp = &ltypes[d];
            e = read_valtype(&sc, ltp);
            if (e != W89_ERR_NONE) {
                return e;
            }
        }
        sp = sc.p;
        if (sp > body_end) {
            return W89_ERR_SECTION_SIZE;
        }
        if (total != 0) {
            nv = (w89_u64)total;
            sz = (w89_u32)nv;
            sz = sz * sizeof(w89_vt);
            lp = w89_xmalloc(sz, err);
            f->locals = lp;
            er = *err;
            if (er != W89_ERR_NONE) {
                return er;
            }
            for (d = 0; d < decls; d = d + 1) {
                lc = lcounts[d];
                lt = ltypes[d];
                for (k = 0; k < lc; k = k + 1) {
                    nl = f->nlocals;
                    ix = nl + k;
                    f->locals[ix] = lt;
                }
                nv = (w89_u64)lc;
                nl = f->nlocals;
                nv2 = (w89_u64)nl;
                total = nv2;
                total = total + nv;
                tc = (w89_u32)total;
                f->nlocals = tc;
            }
        }
        p0 = sc.p;
        ep0 = c->end;
        sp0 = sc.p;
        bd0 = ep0 - sp0;
        win = (w89_u32)bd0;
        code = &f->code;
        e = w89_decode_expr(p0, win, &consumed_1, code);
        if (e != W89_ERR_NONE) {
            return e;
        }
        sp = sc.p;
        bd1 = body_end - sp;
        bw = (w89_u32)bd1;
        if (consumed_1 != bw) {
            return W89_ERR_SECTION_SIZE;
        }
        c->p = body_end;
        if (lcounts != 0) {
            free(lcounts);
        }
        if (ltypes != 0) {
            free(ltypes);
        }
    }
    m->ncode = n;
    return W89_ERR_NONE;
}

static w89_err decode_datas(w89_cur *c, w89_module *m, w89_err *err)
{
    w89_u32 n, i;
    w89_err e;
    w89_u32 sz;
    w89_data *d;
    w89_u32 flags;
    w89_data *p;
    w89_err er;
    w89_u32 *tp;
    w89_instr_vec *iv;
    w89_u32 *lp;
    w89_u32 dl;
    const w89_byte **bp;

    e = cur_len(c, &n);
    if (e != W89_ERR_NONE) {
        return e;
    }
    m->datas = 0;
    if (n != 0) {
        sz = n * sizeof(w89_data);
        p = w89_xmalloc(sz, err);
        m->datas = p;
        er = *err;
        if (er != W89_ERR_NONE) {
            return er;
        }
    }
    for (i = 0; i < n; i = i + 1) {
        d = &m->datas[i];
        memset(d, 0, sizeof(w89_data));
        e = cur_u32(c, &flags);
        if (e != W89_ERR_NONE) {
            return e;
        }
        if (flags > 2) {
            return W89_ERR_UNSUPPORTED;
        }
        d->flags = flags;
        if (flags == 0) {
            d->memidx = 0;
        } else if (flags == 2) {
            tp = &d->memidx;
            e = cur_u32(c, tp);
            if (e != W89_ERR_NONE) {
                return e;
            }
        }
        if (flags == 0) {
            iv = &d->offset;
            e = decode_expr_to_vec(c, iv);
            if (e != W89_ERR_NONE) {
                return e;
            }
        } else if (flags == 2) {
            iv = &d->offset;
            e = decode_expr_to_vec(c, iv);
            if (e != W89_ERR_NONE) {
                return e;
            }
        }
        lp = &d->len;
        e = cur_len(c, lp);
        if (e != W89_ERR_NONE) {
            return e;
        }
        dl = d->len;
        bp = &d->bytes;
        e = cur_bytes(c, dl, bp);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }
    m->ndatas = n;
    return W89_ERR_NONE;
}

static w89_err decode_tagsec(w89_cur *c, w89_module *m, w89_err *err)
{
    w89_u32 n, i;
    w89_err e;
    w89_u32 sz;
    w89_byte attr;
    w89_tag *p;
    w89_err er;
    w89_u32 *tp;

    e = cur_len(c, &n);
    if (e != W89_ERR_NONE) {
        return e;
    }
    m->tags = 0;
    if (n != 0) {
        sz = n * sizeof(w89_tag);
        p = w89_xmalloc(sz, err);
        m->tags = p;
        er = *err;
        if (er != W89_ERR_NONE) {
            return er;
        }
    }
    for (i = 0; i < n; i = i + 1) {
        e = cur_byte(c, &attr);
        if (e != W89_ERR_NONE) {
            return e;
        }
        if (attr != 0x00) {
            return W89_ERR_UNSUPPORTED;
        }
        tp = &m->tags[i].typeidx;
        e = cur_u32(c, tp);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }
    m->ntags = n;
    return W89_ERR_NONE;
}

void w89_module_init(w89_module *m)
{
    size_t sz;

    sz = sizeof(w89_module);
    memset(m, 0, sz);
}

static void free_rectype(w89_rectype *rt)
{
    w89_u32 i;
    w89_u32 n;
    w89_subtype *st;
    void *p;
    w89_compkind k;

    n = rt->n;
    for (i = 0; i < n; i = i + 1) {
        st = &rt->subtypes[i];
        p = st->supertypes;
        if (p != 0) {
            free(p);
        }
        k = st->kind;
        if (k == W89_CK_FUNC) {
            p = st->ft.params;
            if (p != 0) {
                free(p);
            }
            p = st->ft.results;
            if (p != 0) {
                free(p);
            }
        } else {
            p = st->fields;
            if (p != 0) {
                free(p);
            }
        }
    }
    p = rt->subtypes;
    free(p);
}

void w89_module_free(w89_module *m)
{
    w89_u32 i;
    w89_u32 n;
    void *p;
    w89_rectype *rp;
    w89_instr_vec *iv;

    n = m->nrectypes;
    for (i = 0; i < n; i = i + 1) {
        rp = &m->rectypes[i];
        free_rectype(rp);
    }
    n = m->nelems;
    for (i = 0; i < n; i = i + 1) {
        p = m->elems[i].indices;
        if (p != 0) {
            free(p);
        }
        iv = &m->elems[i].offset;
        w89_instr_vec_free(iv);
        iv = &m->elems[i].exprs;
        w89_instr_vec_free(iv);
    }
    n = m->ncode;
    for (i = 0; i < n; i = i + 1) {
        p = m->funcs[i].locals;
        if (p != 0) {
            free(p);
        }
        iv = &m->funcs[i].code;
        w89_instr_vec_free(iv);
    }
    n = m->nglobals;
    for (i = 0; i < n; i = i + 1) {
        iv = &m->globals[i].init;
        w89_instr_vec_free(iv);
    }
    n = m->ntables;
    for (i = 0; i < n; i = i + 1) {
        iv = &m->tables[i].init;
        w89_instr_vec_free(iv);
    }
    n = m->ndatas;
    for (i = 0; i < n; i = i + 1) {
        iv = &m->datas[i].offset;
        w89_instr_vec_free(iv);
    }
    p = m->rectypes;
    free(p);
    p = m->imports;
    free(p);
    p = m->func_types;
    free(p);
    p = m->tables;
    free(p);
    p = m->memories;
    free(p);
    p = m->globals;
    free(p);
    p = m->exports;
    free(p);
    p = m->elems;
    free(p);
    p = m->funcs;
    free(p);
    p = m->datas;
    free(p);
    p = m->tags;
    free(p);
    memset(m, 0, sizeof(w89_module));
}

static w89_err decode_typesec(w89_cur *c, w89_module *m, w89_err *err)
{
    w89_u32 n, i;
    w89_err e;
    w89_u32 sz;
    size_t rtsz;
    w89_rectype *p;
    w89_err er;
    w89_rectype *rp;
    w89_u32 ni;

    e = cur_len(c, &n);
    if (e != W89_ERR_NONE) {
        return e;
    }
    m->rectypes = 0;
    if (n != 0) {
        sz = n * sizeof(w89_rectype);
        p = w89_xmalloc(sz, err);
        m->rectypes = p;
        er = *err;
        if (er != W89_ERR_NONE) {
            return er;
        }
    }
    rtsz = sizeof(w89_rectype);
    for (i = 0; i < n; i = i + 1) {
        rp = &m->rectypes[i];
        memset(rp, 0, rtsz);
        e = read_rectype(c, rp, err);
        if (e != W89_ERR_NONE) {
            ni = i + 1;
            m->nrectypes = ni;
            return e;
        }
    }
    m->nrectypes = n;
    return W89_ERR_NONE;
}

typedef w89_err (*w89_secfn)(w89_cur *, w89_module *, w89_err *);

static w89_f32 w89_f32_from_bytes(const w89_byte *b)
{
    w89_u32 u;
    w89_u32 v;
    w89_f32 f;
    w89_byte b0;

    b0 = b[0];
    v = (w89_u32)b0;
    b0 = b[1];
    u = (w89_u32)b0;
    u = u << 8;
    v = v | u;
    b0 = b[2];
    u = (w89_u32)b0;
    u = u << 16;
    v = v | u;
    b0 = b[3];
    u = (w89_u32)b0;
    u = u << 24;
    v = v | u;
    f = w89_bits_f32(v);
    return f;
}

static w89_f64 w89_f64_from_bytes(const w89_byte *b)
{
    w89_u64 u;
    w89_u64 v;
    w89_f64 f;
    int i;
    w89_byte b0;

    b0 = b[0];
    v = (w89_u64)b0;
    for (i = 1; i < 8; i = i + 1) {
        b0 = b[i];
        u = (w89_u64)b0;
        u = u << (i * 8);
        v = v | u;
    }
    f = w89_bits_f64(v);
    return f;
}

w89_err w89_module_decode(const w89_byte *buf, w89_u32 len, w89_module *m)
{
    static const w89_byte magic[4] = { 0x00, 0x61, 0x73, 0x6D };
    static const w89_byte version[4] = { 0x01, 0x00, 0x00, 0x00 };
    w89_cur c;
    w89_err err;
    w89_u32 last_id;
    int saw_data_count;
    w89_byte id;
    w89_u32 size;
    w89_cur sc;
    w89_err e;
    w89_secfn fn;
    const w89_byte *content;
    w89_u64 consumed;
    w89_cur cc;
    w89_u32 name_len;
    w89_name nm;
    w89_u32 k;
    w89_u32 kk;
    w89_u32 ordlen;
    w89_byte ob;
    w89_u32 cnt;
    w89_u32 dc;
    w89_u32 nd;
    w89_byte sub;
    w89_byte op;
    int ok;
    const w89_byte *en;
    const w89_byte *mp;
    int r;
    const w89_byte *pn;
    const w89_byte *pp;
    const w89_byte *ep;
    const w89_byte *pz;
    const w89_byte *ce;
    const w89_byte **bp;
    const w89_byte *nb;
    long df;
    const w89_byte *pc;
    w89_u32 nf;
    w89_u32 nc;
    w89_u32 jj;

    err = W89_ERR_NONE;
    last_id = 0;
    saw_data_count = 0;

    c.p = buf;
    en = buf + len;
    c.end = en;
    if (len < 4) {
        return W89_ERR_EOF;
    }
    mp = c.p;
    r = memcmp(mp, magic, 4);
    if (r != 0) {
        return W89_ERR_MAGIC;
    }
    pn = c.p;
    pn = pn + 4;
    c.p = pn;
    if (len < 8) {
        return W89_ERR_EOF;
    }
    mp = c.p;
    r = memcmp(mp, version, 4);
    if (r != 0) {
        return W89_ERR_VERSION;
    }
    pn = c.p;
    pn = pn + 4;
    c.p = pn;

    for (;;) {
        pp = c.p;
        ep = c.end;
        if (pp >= ep) {
            break;
        }
        fn = 0;
        e = cur_byte(&c, &id);
        if (e != W89_ERR_NONE) {
            return e;
        }
        e = cur_len(&c, &size);
        if (e != W89_ERR_NONE) {
            return e;
        }

        if (id == 0) {
            pz = c.p;
            cc.p = pz;
            ce = pz + size;
            cc.end = ce;
            e = cur_len(&cc, &name_len);
            if (e != W89_ERR_NONE) {
                return e;
            }
            nm.len = name_len;
            bp = &nm.bytes;
            e = cur_bytes(&cc, name_len, bp);
            if (e != W89_ERR_NONE) {
                return e;
            }
            ob = nm.bytes[0];
            nb = nm.bytes;
            ok = w89_valid_utf8(nb, name_len);
            if (ok == 0) {
                return W89_ERR_UTF8;
            }
            pn = c.p;
            pn = pn + size;
            c.p = pn;
            continue;
        }
        if (id > 13) {
            return W89_ERR_SECTION_ID;
        }
        {
            static const w89_byte w89_sec_order[] = {
                1, 2, 3, 4, 5, 13, 6, 7, 8, 9, 12, 10, 11
            };
            static const w89_u32 w89_sec_order_len = 13;
            ordlen = w89_sec_order_len;
            for (k = last_id; k < ordlen; k = k + 1) {
                ob = w89_sec_order[k];
                if (ob == id) {
                    last_id = k + 1;
                    break;
                }
            }
            if (k == ordlen) {
                return W89_ERR_TRAILING;
            }
        }

        content = c.p;
        pp = c.p;
        sc.p = pp;
        ep = c.end;
        sc.end = ep;

        switch (id) {
        case 1: fn = decode_typesec; break;
        case 2: fn = decode_imports; break;
        case 3: fn = decode_funcsec; break;
        case 4: fn = decode_tablesec; break;
        case 5: fn = decode_memsec; break;
        case 6: fn = decode_globalsec; break;
        case 7: fn = decode_exportsec; break;
        case 8: fn = decode_startsec; break;
        case 9: fn = decode_elems; break;
        case 10: fn = decode_codesec; break;
        case 11: fn = decode_datas; break;
        case 12: fn = NULL; break;
        case 13: fn = decode_tagsec; break;
        default: return W89_ERR_SECTION_ID;
        }
        if (id == 12) {
            w89_cur dc2;
            dc2 = sc;
            e = cur_u32(&dc2, &cnt);
            if (e != W89_ERR_NONE) {
                if (e == W89_ERR_EOF) {
                    return W89_ERR_EOF_SECTION;
                }
                return e;
            }
            m->has_data_count = 1;
            m->data_count = cnt;
            saw_data_count = 1;
            pp = dc2.p;
            df = pp - content;
            consumed = (w89_u64)df;
        } else {
            e = fn(&sc, m, &err);
            if (e != W89_ERR_NONE) {
                if (e == W89_ERR_EOF) {
                    return W89_ERR_EOF_SECTION;
                }
                return e;
            }
            pp = sc.p;
            df = pp - content;
            consumed = (w89_u64)df;
        }
        if (consumed != size) {
            return W89_ERR_SECTION_SIZE;
        }
        pc = content + consumed;
        c.p = pc;
    }

    if (saw_data_count) {
        nd = m->ndatas;
        dc = m->data_count;
        if (nd != dc) {
            return W89_ERR_DATA_COUNT;
        }
    } else {
        kk = m->ncode;
        for (k = 0; k < kk; k = k + 1) {
            cnt = m->funcs[k].code.n;
            for (jj = 0; jj < cnt; jj = jj + 1) {
                op = m->funcs[k].code.items[jj].op;
                if (op == 0xFC) {
                    sub = m->funcs[k].code.items[jj].sub;
                    if (sub == 0x08) {
                        return W89_ERR_DATA_COUNT_REQUIRED;
                    }
                    if (sub == 0x09) {
                        return W89_ERR_DATA_COUNT_REQUIRED;
                    }
                }
            }
        }
    }
    nf = m->nfuncs;
    nc = m->ncode;
    if (nf != nc) {
        return W89_ERR_FUNC_CODE_LEN;
    }
    (void)err;
    return W89_ERR_NONE;
}

static void instr_vec_grow(w89_instr_vec *v)
{
    w89_instr *ni;
    w89_u32 newcap;
    w89_u32 cap;
    w89_u32 sz;
    w89_instr *it;

    cap = v->cap;
    if (cap != 0) {
        newcap = cap * 2;
    } else {
        newcap = 16;
    }
    if (newcap < 16) {
        newcap = 16;
    }
    sz = newcap * sizeof(w89_instr);
    it = v->items;
    ni = realloc(it, sz);
    if (ni != 0) {
        v->items = ni;
        v->cap = newcap;
    }
}

static w89_err instr_push(w89_instr_vec *v, const w89_instr *in)
{
    w89_u32 n;
    w89_u32 cap;
    w89_instr iv;

    n = v->n;
    cap = v->cap;
    if (n == cap) {
        instr_vec_grow(v);
        n = v->n;
        cap = v->cap;
        if (n == cap) {
            return W89_ERR_OUT_OF_MEMORY;
        }
    }
    iv = *in;
    v->items[n] = iv;
    n = v->n;
    n = n + 1;
    v->n = n;
    return W89_ERR_NONE;
}

void w89_instr_vec_free(w89_instr_vec *v)
{
    w89_u32 i;
    w89_u32 n;
    void *p;

    n = v->n;
    for (i = 0; i < n; i = i + 1) {
        p = v->items[i].labels;
        if (p != 0) {
            free(p);
        }
        p = v->items[i].seltypes;
        if (p != 0) {
            free(p);
        }
        p = v->items[i].catches;
        if (p != 0) {
            free(p);
        }
    }
    p = v->items;
    free(p);
    memset(v, 0, sizeof(w89_instr_vec));
}

static w89_err read_blocktype(w89_cur *c, w89_blocktype *out)
{
    w89_i64 v;
    w89_err e;
    w89_u32 tv;
    w89_i64 nv;
    w89_u32 nu;
    int eq;
    w89_absheaptype ab;
    w89_reftype *rtp;

    e = cur_s33(c, &v);
    if (e != W89_ERR_NONE) {
        return e;
    }
    if (v >= 0) {
        out->is_typeidx = 1;
        tv = (w89_u32)v;
        out->typeidx = tv;
        return W89_ERR_NONE;
    }
    out->is_typeidx = 0;
    if (v == -64) {
        out->vt.is_ref = 0;
        out->vt.num = 0;
        return W89_ERR_NONE;
    }
    if (v >= -5) {
        if (v <= -1) {
            out->vt.is_ref = 0;
            nv = -v;
            nu = (w89_u32)nv;
            out->vt.num = nu;
            return W89_ERR_NONE;
        }
    }
    if (v == -28) {
        out->vt.is_ref = 1;
        nv = -v - 1;
        nu = (w89_u32)nv;
        out->vt.num = nu;
        eq = v == -29;
        out->vt.rt.nullable = eq;
        rtp = &out->vt.rt;
        return read_heaptype(c, rtp);
    }
    if (v == -29) {
        out->vt.is_ref = 1;
        nv = -v - 1;
        nu = (w89_u32)nv;
        out->vt.num = nu;
        eq = v == -29;
        out->vt.rt.nullable = eq;
        rtp = &out->vt.rt;
        return read_heaptype(c, rtp);
    }
    if (v >= -23) {
        if (v <= -12) {
            out->vt.is_ref = 1;
            nv = -v;
            nu = (w89_u32)nv;
            out->vt.num = nu;
            out->vt.rt.nullable = 1;
            out->vt.rt.is_typeidx = 0;
            nv = 0x74 + v + 12;
            nu = (w89_u32)nv;
            ab = (w89_absheaptype)nu;
            out->vt.rt.abs = ab;
            return W89_ERR_NONE;
        }
    }
    return W89_ERR_MALFORMED_TYPE;
}

static w89_err read_memarg(w89_cur *c, w89_instr *out)
{
    w89_u32 flags;
    w89_err e;
    int hm;
    w89_u32 al;
    w89_u32 *mp;
    w89_u64 *op;

    e = cur_u32(c, &flags);
    if (e != W89_ERR_NONE) {
        return e;
    }
    if (flags >= 0x80) {
        return W89_ERR_MEMOP_FLAGS;
    }
    al = flags & 0x3F;
    hm = (flags & 0x40) != 0;
    out->has_memidx = hm;
    out->align = al;
    if (hm != 0) {
        mp = &out->memidx;
        e = cur_u32(c, mp);
        if (e != W89_ERR_NONE) {
            return e;
        }
    } else {
        out->memidx = 0;
    }
    op = &out->offset;
    return cur_u64(c, op);
}

static w89_err read_catch(w89_cur *c, w89_catch *out)
{
    w89_byte b;
    w89_err e;
    w89_u32 *tg;
    w89_u32 *lb;

    e = cur_byte(c, &b);
    if (e != W89_ERR_NONE) {
        return e;
    }
    out->kind = b;
    if (b == 0x00) {
        tg = &out->tagidx;
        e = cur_u32(c, tg);
        if (e != W89_ERR_NONE) {
            return e;
        }
        lb = &out->label;
        return cur_u32(c, lb);
    }
    if (b == 0x01) {
        tg = &out->tagidx;
        e = cur_u32(c, tg);
        if (e != W89_ERR_NONE) {
            return e;
        }
        lb = &out->label;
        return cur_u32(c, lb);
    }
    if (b == 0x02) {
        lb = &out->label;
        return cur_u32(c, lb);
    }
    if (b == 0x03) {
        lb = &out->label;
        return cur_u32(c, lb);
    }
    return W89_ERR_MALFORMED_TYPE;
}

static w89_err decode_instr_block(w89_cur *c, w89_instr_vec *v);

static w89_err decode_instr(w89_cur *c, w89_instr_vec *v)
{
    w89_byte op;
    w89_instr in;
    w89_err e;
    w89_u32 n, i;
    w89_u32 sub;
    w89_u32 sz;
    const w89_byte *pp;
    const w89_byte *ep;
    const w89_byte *pn;
    w89_f32 fb;
    w89_f64 fd;
    w89_byte ob;
    w89_blocktype *btp;
    w89_catch *cap;
    w89_catch *cp;
    w89_u32 *lp;
    w89_u32 *ip;
    w89_u32 *ip2;
    w89_vt *vp;
    w89_vt *sp;
    w89_i64 *c64p;
    w89_i64 c64v;
    w89_i32 c32v;

    memset(&in, 0, sizeof(w89_instr));
    e = cur_byte(c, &op);
    if (e != W89_ERR_NONE) {
        return e;
    }
    in.op = op;

    if (op != 0x02) {
        if (op != 0x03) {
            if (op != 0x04) {
                goto not_block;
            }
        }
    }
    btp = &in.bt;
    e = read_blocktype(c, btp);
    if (e != W89_ERR_NONE) {
        return e;
    }
    if (op == 0x04) {
        e = instr_push(v, &in);
        if (e != W89_ERR_NONE) {
            return e;
        }
        e = decode_instr_block(c, v);
        if (e != W89_ERR_NONE) {
            return e;
        }
        pp = c->p;
        ep = c->end;
        if (pp < ep) {
            ob = *pp;
            if (ob == 0x05) {
                memset(&in, 0, sizeof(w89_instr));
                in.op = 0x05;
                pn = pp + 1;
                c->p = pn;
                e = instr_push(v, &in);
                if (e != W89_ERR_NONE) {
                    return e;
                }
                e = decode_instr_block(c, v);
                if (e != W89_ERR_NONE) {
                    return e;
                }
            }
        }
        pp = c->p;
        ep = c->end;
        if (pp >= ep) {
            return W89_ERR_EOF;
        }
        ob = *pp;
        if (ob != 0x0B) {
            return W89_ERR_END_EXPECTED;
        }
        pn = pp + 1;
        c->p = pn;
        memset(&in, 0, sizeof(w89_instr));
        in.op = 0x0B;
        return instr_push(v, &in);
    }
    e = instr_push(v, &in);
    if (e != W89_ERR_NONE) {
        return e;
    }
    e = decode_instr_block(c, v);
    if (e != W89_ERR_NONE) {
        return e;
    }
    pp = c->p;
    ep = c->end;
    if (pp >= ep) {
        return W89_ERR_EOF;
    }
    ob = *pp;
    if (ob != 0x0B) {
        return W89_ERR_END_EXPECTED;
    }
    pn = pp + 1;
    c->p = pn;
    memset(&in, 0, sizeof(w89_instr));
    in.op = 0x0B;
    return instr_push(v, &in);
not_block:
    if (op == 0x1F) {
        btp = &in.bt;
        e = read_blocktype(c, btp);
        if (e != W89_ERR_NONE) {
            return e;
        }
        e = cur_len(c, &n);
        if (e != W89_ERR_NONE) {
            return e;
        }
        in.n = n;
        if (n != 0) {
            sz = n * sizeof(w89_catch);
            cp = malloc(sz);
            in.catches = cp;
            if (cp == 0) {
                return W89_ERR_OUT_OF_MEMORY;
            }
        }
        for (i = 0; i < n; i = i + 1) {
            cap = &in.catches[i];
            e = read_catch(c, cap);
            if (e != W89_ERR_NONE) {
                return e;
            }
        }
        e = instr_push(v, &in);
        if (e != W89_ERR_NONE) {
            return e;
        }
        e = decode_instr_block(c, v);
        if (e != W89_ERR_NONE) {
            return e;
        }
        pp = c->p;
        ep = c->end;
        if (pp >= ep) {
            return W89_ERR_EOF;
        }
        ob = *pp;
        if (ob != 0x0B) {
            return W89_ERR_END_EXPECTED;
        }
        pn = pp + 1;
        c->p = pn;
        memset(&in, 0, sizeof(w89_instr));
        in.op = 0x0B;
        return instr_push(v, &in);
    }

    if (op == 0x0E) {
        e = cur_len(c, &n);
        if (e != W89_ERR_NONE) {
            return e;
        }
        in.n = n;
        if (n != 0) {
            sz = n * sizeof(w89_u32);
            lp = malloc(sz);
            in.labels = lp;
            if (lp == 0) {
                return W89_ERR_OUT_OF_MEMORY;
            }
        }
        for (i = 0; i < n; i = i + 1) {
            lp = &in.labels[i];
            e = cur_u32(c, lp);
            if (e != W89_ERR_NONE) {
                return e;
            }
        }
        ip = &in.idx;
        e = cur_u32(c, ip);
        if (e != W89_ERR_NONE) {
            return e;
        }
        return instr_push(v, &in);
    }

    if (op == 0x1C) {
        e = cur_len(c, &n);
        if (e != W89_ERR_NONE) {
            return e;
        }
        in.n = n;
        if (n != 0) {
            sz = n * sizeof(w89_vt);
            sp = malloc(sz);
            in.seltypes = sp;
            if (sp == 0) {
                return W89_ERR_OUT_OF_MEMORY;
            }
        }
        for (i = 0; i < n; i = i + 1) {
            vp = &in.seltypes[i];
            e = read_valtype(c, vp);
            if (e != W89_ERR_NONE) {
                return e;
            }
        }
        return instr_push(v, &in);
    }

    if (op >= 0x28) {
        if (op <= 0x3E) {
            e = read_memarg(c, &in);
            if (e != W89_ERR_NONE) {
                return e;
            }
            return instr_push(v, &in);
        }
    }

    if (op == 0x41) {
        c64p = &in.c64;
        e = cur_s32(c, c64p);
        if (e != W89_ERR_NONE) {
            return e;
        }
        c64v = in.c64;
        c32v = (w89_i32)c64v;
        in.c32 = c32v;
        return instr_push(v, &in);
    }

    if (op == 0x42) {
        c64p = &in.c64;
        e = cur_s64(c, c64p);
        if (e != W89_ERR_NONE) {
            return e;
        }
        return instr_push(v, &in);
    }

    if (op == 0xD0) {
        w89_reftype rt;
        e = read_heaptype(c, &rt);
        if (e != W89_ERR_NONE) {
            return e;
        }
        in.rt = rt;
        return instr_push(v, &in);
    }

    if (op == 0xFB) {
        e = cur_u32(c, &sub);
        if (e != W89_ERR_NONE) {
            return e;
        }
        return W89_ERR_UNSUPPORTED;
    }
    if (op == 0xFD) {
        e = cur_u32(c, &sub);
        if (e != W89_ERR_NONE) {
            return e;
        }
        in.sub = sub;
        if (sub == 0x00) {
            e = read_memarg(c, &in);
            if (e != W89_ERR_NONE) {
                return e;
            }
            return instr_push(v, &in);
        }
        if (sub == 0x0B) {
            e = read_memarg(c, &in);
            if (e != W89_ERR_NONE) {
                return e;
            }
            return instr_push(v, &in);
        }
        if (sub == 0x0C) {
            pp = c->p;
            ep = c->end;
            if (ep - pp < 16) {
                return W89_ERR_EOF;
            }
            for (i = 0; i < 16; i = i + 1) {
                ob = pp[i];
                in.c128.b[i] = ob;
            }
            pn = pp + 16;
            c->p = pn;
            return instr_push(v, &in);
        }
        return W89_ERR_UNKNOWN_OPCODE;
    }
    if (op == 0xFC) {
        e = cur_u32(c, &sub);
        if (e != W89_ERR_NONE) {
            return e;
        }
        in.sub = sub;
        if (sub <= 0x07) {
            return instr_push(v, &in);
        }
        if (sub == 0x08) {
            ip2 = &in.idx2;
            e = cur_u32(c, ip2);
            if (e != W89_ERR_NONE) {
                return e;
            }
            ip = &in.idx;
            e = cur_u32(c, ip);
            if (e != W89_ERR_NONE) {
                return e;
            }
            return instr_push(v, &in);
        }
        if (sub == 0x0A) {
            ip2 = &in.idx2;
            e = cur_u32(c, ip2);
            if (e != W89_ERR_NONE) {
                return e;
            }
            ip = &in.idx;
            e = cur_u32(c, ip);
            if (e != W89_ERR_NONE) {
                return e;
            }
            return instr_push(v, &in);
        }
        if (sub == 0x0C) {
            ip2 = &in.idx2;
            e = cur_u32(c, ip2);
            if (e != W89_ERR_NONE) {
                return e;
            }
            ip = &in.idx;
            e = cur_u32(c, ip);
            if (e != W89_ERR_NONE) {
                return e;
            }
            return instr_push(v, &in);
        }
        if (sub == 0x0E) {
            ip2 = &in.idx2;
            e = cur_u32(c, ip2);
            if (e != W89_ERR_NONE) {
                return e;
            }
            ip = &in.idx;
            e = cur_u32(c, ip);
            if (e != W89_ERR_NONE) {
                return e;
            }
            return instr_push(v, &in);
        }
        if (sub == 0x09) {
            ip = &in.idx;
            e = cur_u32(c, ip);
            if (e != W89_ERR_NONE) {
                return e;
            }
            return instr_push(v, &in);
        }
        if (sub == 0x0B) {
            ip = &in.idx;
            e = cur_u32(c, ip);
            if (e != W89_ERR_NONE) {
                return e;
            }
            return instr_push(v, &in);
        }
        if (sub == 0x0D) {
            ip = &in.idx;
            e = cur_u32(c, ip);
            if (e != W89_ERR_NONE) {
                return e;
            }
            return instr_push(v, &in);
        }
        if (sub == 0x0F) {
            ip = &in.idx;
            e = cur_u32(c, ip);
            if (e != W89_ERR_NONE) {
                return e;
            }
            return instr_push(v, &in);
        }
        if (sub == 0x10) {
            ip = &in.idx;
            e = cur_u32(c, ip);
            if (e != W89_ERR_NONE) {
                return e;
            }
            return instr_push(v, &in);
        }
        if (sub == 0x11) {
            ip = &in.idx;
            e = cur_u32(c, ip);
            if (e != W89_ERR_NONE) {
                return e;
            }
            return instr_push(v, &in);
        }
        return W89_ERR_UNKNOWN_OPCODE;
    }

    switch (op) {
    case 0x08: case 0x0C: case 0x0D: case 0x10: case 0x12: case 0x14:
    case 0x15: case 0x20: case 0x21: case 0x22: case 0x23: case 0x24:
    case 0x25: case 0x26: case 0x3F: case 0x40: case 0xD2: case 0xD5:
    case 0xD6:
        ip = &in.idx;
        e = cur_u32(c, ip);
        if (e != W89_ERR_NONE) {
            return e;
        }
        return instr_push(v, &in);
    case 0x11: case 0x13:
        ip2 = &in.idx2;
        e = cur_u32(c, ip2);
        if (e != W89_ERR_NONE) {
            return e;
        }
        ip = &in.idx;
        e = cur_u32(c, ip);
        if (e != W89_ERR_NONE) {
            return e;
        }
        return instr_push(v, &in);
    case 0x43:
        pp = c->p;
        ep = c->end;
        if (ep - pp < 4) {
            return W89_ERR_EOF;
        }
        fb = w89_f32_from_bytes(pp);
        in.f32 = fb;
        pn = pp + 4;
        c->p = pn;
        return instr_push(v, &in);
    case 0x44:
        pp = c->p;
        ep = c->end;
        if (ep - pp < 8) {
            return W89_ERR_EOF;
        }
        fd = w89_f64_from_bytes(pp);
        in.f64 = fd;
        pn = pp + 8;
        c->p = pn;
        return instr_push(v, &in);
    case 0x00: case 0x01: case 0x0A: case 0x0F: case 0x1A: case 0x1B:
    case 0xD1: case 0xD3: case 0xD4:
        return instr_push(v, &in);
    default:
        if (op >= 0x45) {
            if (op <= 0xC4) {
                return instr_push(v, &in);
            }
        }
        w89_last_illegal_op = op;
        return W89_ERR_UNKNOWN_OPCODE;
    }
}

static w89_err decode_instr_block(w89_cur *c, w89_instr_vec *v)
{
    w89_err e;
    const w89_byte *pp;
    const w89_byte *ep;
    w89_byte ob;

    for (;;) {
        pp = c->p;
        ep = c->end;
        if (pp >= ep) {
            return W89_ERR_NONE;
        }
        ob = *pp;
        if (ob == 0x0B) {
            return W89_ERR_NONE;
        }
        if (ob == 0x05) {
            return W89_ERR_NONE;
        }
        e = decode_instr(c, v);
        if (e != W89_ERR_NONE) {
            return e;
        }
    }
}

static w89_err decode_expr_to_vec(w89_cur *c, w89_instr_vec *v)
{
    w89_err e;
    const w89_byte *pp;
    const w89_byte *ep;
    const w89_byte *pn;
    w89_byte ob;

    e = decode_instr_block(c, v);
    if (e != W89_ERR_NONE) {
        return e;
    }
    pp = c->p;
    ep = c->end;
    if (pp >= ep) {
        return W89_ERR_EOF;
    }
    ob = *pp;
    if (ob != 0x0B) {
        return W89_ERR_END_EXPECTED;
    }
    pn = pp + 1;
    c->p = pn;
    return W89_ERR_NONE;
}

w89_err w89_decode_expr(const w89_byte *p, w89_u32 window,
                          w89_u32 *consumed, w89_instr_vec *out)
{
    w89_cur c;
    w89_err e;
    w89_u32 cv;
    size_t osz;
    const w89_byte *en;
    const w89_byte *pp;
    long df;

    osz = sizeof(w89_instr_vec);
    memset(out, 0, osz);
    c.p = p;
    en = p + window;
    c.end = en;
    e = decode_expr_to_vec(&c, out);
    if (e != W89_ERR_NONE) {
        if (e == W89_ERR_EOF) {
            return W89_ERR_EOF_SECTION;
        }
        return e;
    }
    pp = c.p;
    df = pp - p;
    cv = (w89_u32)df;
    *consumed = cv;
    return W89_ERR_NONE;
}

w89_byte w89_last_illegal(void)
{
    return w89_last_illegal_op;
}
