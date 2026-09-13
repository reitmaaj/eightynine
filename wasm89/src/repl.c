#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "module.h"
#include "validate.h"
#include "eval.h"

/* Persistent line-protocol REPL used by the conformance driver.
 *
 * Commands (one per line):
 *   module <path>                       -> @ok | @error <msg>
 *   register <name>                     -> @ok | @error <msg>
 *   invoke <name> <func> <nargs> <arg>* -> @return <val>* | @trap <msg>
 *                                         | @exhaustion | @error <msg>
 *   get <name> <global>                 -> @return <val> | @error <msg>
 *   assert_return <name> <func> <nargs> <arg>* <nexp> <exp>*
 *                                       -> @pass | @fail <val>*
 *   assert_return_get <name> <global> <exp>     -> @pass | @fail <val>
 *   assert_trap <name> <func> <nargs> <arg>* <msg>   -> @pass | @fail
 *   assert_exhaustion <name> <func> <nargs> <arg>*  -> @pass | @fail
 *   quit
 *
 * Values are <type>:<value> tokens: i32/i64/f32/f64 as hex bits, and
 * ref.null:<ht> / ref.func:<idx> / ref.extern:<hex> for references.
 * Host print output is written to stdout as bare lines; command results
 * are always @-prefixed. */

#define W89_REPL_LINE 4096
#define W89_REPL_BUF 64
#define W89_REPL_MAXARG 64

typedef struct {
    char *t[64];
    w89_u32 n;
} toks;

static void tok_split(toks *o, char *line)
{
    char *p;
    char c;
    w89_u32 n;

    p = line;
    o->n = 0;
    for (;;) {
        c = *p;
        while (c == ' ') {
            *p = '\0';
            p = p + 1;
            c = *p;
        }
        while (c == '\t') {
            *p = '\0';
            p = p + 1;
            c = *p;
        }
        c = *p;
        if (c == '\0') {
            break;
        }
        n = o->n;
        if (n < 64) {
            o->t[n] = p;
            n = n + 1;
            o->n = n;
        }
        c = *p;
        while (c != '\0') {
            if (c == ' ') {
                break;
            }
            if (c == '\t') {
                break;
            }
            p = p + 1;
            c = *p;
        }
    }
}

static w89_u64 parse_uint(const char *s, int *ok)
{
    char *end;
    unsigned long v;
    int base = 10;
    char c0;
    char c1;
    w89_u64 r;
    int is_ok;

    c0 = s[0];
    if (c0 == '0') {
        c1 = s[1];
        if (c1 == 'x') {
            base = 16;
        } else if (c1 == 'X') {
            base = 16;
        }
    }
    v = strtoul(s, &end, base);
    is_ok = end != s;
    if (is_ok != 0) {
        c0 = *end;
        is_ok = c0 == '\0';
    }
    *ok = is_ok;
    r = (w89_u64)v;
    return r;
}

static int parse_value(const char *tok, const w89_moduleinst *ctx,
                       w89_value *out)
{
    int ok = 0;
    w89_u64 v;
    const char *p;
    int c;
    w89_u32 nf;
    w89_funcinst *fi;
    w89_ref rf;
    w89_u32 idx;
    size_t sz;
    w89_u64 ext;

    sz = sizeof(w89_value);
    memset(out, 0, sz);
    c = strncmp(tok, "i32:", 4);
    if (c == 0) {
        p = tok + 4;
        v = parse_uint(p, &ok);
        if (ok == 0) {
            return 0;
        }
        idx = (w89_u32)v;
        out->u.num = idx;
        return 1;
    }
    c = strncmp(tok, "i64:", 4);
    if (c == 0) {
        p = tok + 4;
        v = parse_uint(p, &ok);
        if (ok == 0) {
            return 0;
        }
        out->u.num = v;
        return 1;
    }
    c = strncmp(tok, "f32:", 4);
    if (c == 0) {
        p = tok + 4;
        v = parse_uint(p, &ok);
        if (ok == 0) {
            return 0;
        }
        idx = (w89_u32)v;
        out->u.num = idx;
        return 1;
    }
    c = strncmp(tok, "f64:", 4);
    if (c == 0) {
        p = tok + 4;
        v = parse_uint(p, &ok);
        if (ok == 0) {
            return 0;
        }
        out->u.num = v;
        return 1;
    }
    c = strncmp(tok, "ref.null:", 9);
    if (c == 0) {
        out->is_ref = 1;
        out->u.ref.kind = W89_RK_NULL;
        return 1;
    }
    c = strncmp(tok, "ref.extern:", 11);
    if (c == 0) {
        p = tok + 11;
        v = parse_uint(p, &ok);
        if (ok == 0) {
            return 0;
        }
        out->is_ref = 1;
        out->u.ref.kind = W89_RK_EXTERN;
        ext = v;
        out->u.ref.u.ext = ext;
        return 1;
    }
    c = strncmp(tok, "ref.func:*", 10);
    if (c == 0) {
        out->is_ref = 1;
        out->u.ref.kind = W89_RK_FUNC;
        out->u.ref.u.func = 0;
        return 1;
    }
    c = strncmp(tok, "ref.func:", 9);
    if (c == 0) {
        p = tok + 9;
        v = parse_uint(p, &ok);
        if (ok == 0) {
            return 0;
        }
        if (ctx == 0) {
            return 0;
        }
        nf = ctx->nfuncs;
        if (v >= nf) {
            return 0;
        }
        idx = (w89_u32)v;
        out->is_ref = 1;
        fi = ctx->funcs[idx];
        rf = w89_ref_func(fi);
        out->u.ref = rf;
        return 1;
    }
    return 0;
}

static void fmt_num(char *buf, size_t n, const w89_vt *vt, w89_u64 bits)
{
    w89_u32 is_ref;
    w89_u32 vnum;
    w89_u32 b32;
    unsigned un;
    unsigned long ul;

    (void)n;
    is_ref = vt->is_ref;
    if (is_ref != 0) {
        sprintf(buf, "ref");
        return;
    }
    vnum = vt->num;
    switch (vnum) {
    case 0x7F:
        b32 = (w89_u32)bits;
        un = (unsigned)b32;
        sprintf(buf, "i32:0x%x", un);
        break;
    case 0x7E:
        ul = (unsigned long)bits;
        sprintf(buf, "i64:0x%lx", ul);
        break;
    case 0x7D:
        b32 = (w89_u32)bits;
        un = (unsigned)b32;
        sprintf(buf, "f32:0x%x", un);
        break;
    case 0x7C:
        ul = (unsigned long)bits;
        sprintf(buf, "f64:0x%lx", ul);
        break;
    default:
        sprintf(buf, "?");
        break;
    }
}

static void fmt_value(char *buf, size_t n, const w89_vt *vt,
                      const w89_value *v)
{
    w89_u32 is_ref;
    w89_u32 rk;
    w89_u64 ext;
    w89_u64 n0;
    unsigned long ul;

    is_ref = v->is_ref;
    if (is_ref != 0) {
        rk = v->u.ref.kind;
        switch (rk) {
        case W89_RK_NULL:
            sprintf(buf, "ref.null");
            break;
        case W89_RK_FUNC:
            sprintf(buf, "ref.func");
            break;
        case W89_RK_EXTERN:
            ext = v->u.ref.u.ext;
            ul = (unsigned long)ext;
            sprintf(buf, "ref.extern:0x%lx", ul);
            break;
        case W89_RK_EXN:
            sprintf(buf, "ref.exn");
            break;
        default:
            sprintf(buf, "ref");
            break;
        }
        return;
    }
    n0 = v->u.num;
    fmt_num(buf, n, vt, n0);
}

static w89_moduleinst *resolve_module(w89_registry *reg,
                                      w89_moduleinst *last, const char *name)
{
    char c0;
    int cmp;
    w89_u32 len;
    size_t sl;
    w89_moduleinst *r;

    c0 = name[0];
    if (c0 == '\0') {
        return last;
    }
    cmp = strcmp(name, "last");
    if (cmp == 0) {
        return last;
    }
    sl = strlen(name);
    len = (w89_u32)sl;
    r = w89_registry_find(reg, name, len);
    return r;
}

static w89_funcinst *find_func(w89_moduleinst *inst, const char *name)
{
    w89_name n;
    w89_externinst ext;
    int found;
    const w89_byte *nb;
    size_t sl;
    w89_u32 len;
    w89_funcinst *f;
    w89_externkind ek;

    nb = (const w89_byte *)name;
    n.bytes = nb;
    sl = strlen(name);
    len = (w89_u32)sl;
    n.len = len;
    found = w89_find_export(inst, &n, &ext);
    if (found == 0) {
        return NULL;
    }
    ek = ext.kind;
    if (ek != W89_EXT_FUNC) {
        return NULL;
    }
    f = ext.u.func;
    return f;
}

static w89_globalinst *find_global(w89_moduleinst *inst, const char *name)
{
    w89_name n;
    w89_externinst ext;
    int found;
    const w89_byte *nb;
    size_t sl;
    w89_u32 len;
    w89_globalinst *g;
    w89_externkind ek;

    nb = (const w89_byte *)name;
    n.bytes = nb;
    sl = strlen(name);
    len = (w89_u32)sl;
    n.len = len;
    found = w89_find_export(inst, &n, &ext);
    if (found == 0) {
        return NULL;
    }
    ek = ext.kind;
    if (ek != W89_EXT_GLOBAL) {
        return NULL;
    }
    g = ext.u.global;
    return g;
}

static void print_results(const w89_ft *ft, const w89_value *vs, w89_u32 nvs)
{
    w89_u32 i;
    char buf[W89_REPL_BUF];
    const w89_vt *vt;
    w89_u32 nresults;
    size_t bsz;
    const w89_value *vp;

    bsz = sizeof(char[W89_REPL_BUF]);
    nresults = ft->nresults;
    for (i = 0; i < nvs; i = i + 1) {
        if (i < nresults) {
            vt = &ft->results[i];
        } else {
            vt = 0;
        }
        vp = &vs[i];
        fmt_value(buf, bsz, vt, vp);
        printf(" %s", buf);
    }
}

static const char *fail_msg(const w89_eval_out *out)
{
    const char *msg;
    w89_eval_status st;

    msg = out->msg;
    if (msg != 0) {
        return msg;
    }
    st = out->status;
    switch (st) {
    case W89_EVAL_EXCEPTION:
        return "exception";
    case W89_EVAL_EXHAUSTED:
        return "exhaustion";
    case W89_EVAL_TRAP:
        return "trap";
    default:
        return "crash";
    }
}

static int values_equal(const w89_value *a, const w89_value *b)
{
    w89_u32 ar;
    w89_u32 br;
    w89_u64 an;
    w89_u64 bn;
    w89_u32 ak;
    w89_u32 bk;
    w89_funcinst *af;
    w89_funcinst *bf;
    w89_u64 ae;
    w89_u64 be;
    w89_exn *ax;
    w89_exn *bx;

    ar = a->is_ref;
    br = b->is_ref;
    if (ar != br) {
        return 0;
    }
    if (ar == 0) {
        an = a->u.num;
        bn = b->u.num;
        return an == bn;
    }
    ak = a->u.ref.kind;
    bk = b->u.ref.kind;
    if (ak != bk) {
        return 0;
    }
    switch (ak) {
    case W89_RK_NULL:
        return 1;
    case W89_RK_FUNC:
        af = a->u.ref.u.func;
        bf = b->u.ref.u.func;
        return af == bf;
    case W89_RK_EXTERN:
        ae = a->u.ref.u.ext;
        be = b->u.ref.u.ext;
        return ae == be;
    case W89_RK_EXN:
        ax = a->u.ref.u.exn;
        bx = b->u.ref.u.exn;
        return ax == bx;
    }
    return 0;
}

static int cmd_module(w89_store *s, w89_registry *reg, w89_moduleinst **last,
                      const char *path)
{
    FILE *fp;
    long size;
    w89_byte *buf;
    w89_module *m;
    w89_err e;
    w89_moduleinst *inst;
    w89_u32 bsize;
    size_t want;
    size_t got;
    size_t wantr;
    long alloc;
    unsigned int op;
    int opcode;
    int rc;
    const char *msg;
    w89_u32 msize;
    char mc;

    inst = 0;
    fp = fopen(path, "rb");
    if (fp == 0) {
        printf("@error cannot open %s\n", path);
        return 1;
    }
    rc = fseek(fp, 0, SEEK_END);
    if (rc != 0) {
        fclose(fp);
        return 1;
    }
    size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return 1;
    }
    rc = fseek(fp, 0, SEEK_SET);
    if (rc != 0) {
        fclose(fp);
        return 1;
    }
    if (size > 0) {
        alloc = size;
    } else {
        alloc = 1;
    }
    want = (size_t)alloc;
    buf = malloc(want);
    if (buf == 0) {
        fclose(fp);
        return 1;
    }
    wantr = (size_t)size;
    got = fread(buf, 1, wantr, fp);
    if (got != wantr) {
        fclose(fp);
        free(buf);
        printf("@error cannot read %s\n", path);
        return 1;
    }
    fclose(fp);

    msize = sizeof(w89_module);
    m = malloc(msize);
    if (m == 0) {
        free(buf);
        return 1;
    }
    w89_module_init(m);
    bsize = (w89_u32)size;
    e = w89_module_decode(buf, bsize, m);
    if (e == W89_ERR_NONE) {
        e = w89_module_validate(m);
    }
    if (e == W89_ERR_NONE) {
        e = w89_instantiate(s, m, reg, &inst);
    }
    w89_store_own_module(s, m, buf);
    if (e == W89_ERR_NONE) {
        *last = inst;
        printf("@ok\n");
        return 0;
    }
    if (e == W89_ERR_UNKNOWN_OPCODE) {
        opcode = w89_last_illegal();
        op = (unsigned int)opcode;
        printf("@error illegal opcode %02x\n", op);
    } else if (e != W89_ERR_INVALID) {
        msg = w89_err_message(e);
        printf("@error %s\n", msg);
    } else {
        msg = w89_validate_message();
        mc = msg[0];
        if (mc == '\0') {
            msg = w89_instantiate_message();
        }
        mc = msg[0];
        if (mc == '\0') {
            msg = w89_err_message(e);
        }
        printf("@error %s\n", msg);
    }
    return 1;
}

static int cmd_module_named(w89_store *s, w89_registry *reg,
                            w89_moduleinst **last, const char *path,
                            const char *name)
{
    int r;
    w89_moduleinst *lst;
    char c0;
    int cmp;
    w89_err e;

    r = cmd_module(s, reg, last, path);
    if (r != 0) {
        return 0;
    }
    lst = *last;
    if (lst == 0) {
        return 0;
    }
    c0 = name[0];
    if (c0 == '\0') {
        return 0;
    }
    cmp = strcmp(name, "last");
    if (cmp == 0) {
        return 0;
    }
    e = w89_register(reg, name, lst);
    if (e != W89_ERR_NONE) {
        printf("@error out of memory\n");
    }
    return 0;
}

static int cmd_register(w89_registry *reg, w89_moduleinst *last, toks *t)
{
    w89_err e;
    char buf[W89_REPL_BUF];
    char *dst;
    w89_u32 i;
    w89_u32 p;
    w89_u32 tn;
    const char *tt;
    size_t rl;
    size_t need;
    size_t rem;
    size_t capr;

    if (last == 0) {
        printf("@error no module to register\n");
        return 0;
    }
    /* The registered name is every token after "register" joined with single
     * spaces, so names containing spaces (e.g. "not wasm") round-trip. */
    capr = (size_t)W89_REPL_BUF;
    rem = capr - 1;
    p = 0;
    tn = t->n;
    for (i = 1; i < tn; i = i + 1) {
        tt = t->t[i];
        rl = strlen(tt);
        need = rl;
        if (i > 1) {
            need = need + 1;
        }
        if (need > rem) {
            printf("@error name too long\n");
            return 0;
        }
        if (i > 1) {
            buf[p] = ' ';
            p = p + 1;
        }
        dst = &buf[0];
        dst = dst + p;
        memcpy(dst, tt, rl);
        p = p + rl;
        rem = rem - need;
    }
    buf[p] = '\0';
    e = w89_register(reg, buf, last);
    if (e == W89_ERR_NONE) {
        printf("@ok\n");
    } else {
        printf("@error out of memory\n");
    }
    return 0;
}

static int cmd_invoke(w89_store *s, w89_registry *reg, w89_moduleinst *last,
                      toks *t)
{
    w89_moduleinst *inst;
    w89_funcinst *f;
    w89_value args[64];
    w89_eval_out out;
    w89_u32 i;
    w89_u32 nargs;
    w89_u32 tn;
    w89_u32 idx;
    const char *tt;
    w89_value *av;
    w89_eval_status st;
    const char *msg;
    const w89_ft *ff;
    w89_value *ovs;
    w89_u32 onv;
    int ok;
    int pv;

    tn = t->n;
    if (tn < 3) {
        printf("@error malformed invoke\n");
        return 0;
    }
    tt = t->t[1];
    inst = resolve_module(reg, last, tt);
    if (inst == 0) {
        printf("@error unknown module %s\n", tt);
        return 0;
    }
    tt = t->t[2];
    f = find_func(inst, tt);
    if (f == 0) {
        printf("@error unknown function %s\n", tt);
        return 0;
    }
    {
        ok = 0;
        tt = t->t[3];
        parse_uint(tt, &ok);
        tn = t->n;
        if (tn < 4) {
            printf("@error malformed invoke\n");
            return 0;
        }
        if (ok == 0) {
            printf("@error malformed invoke\n");
            return 0;
        }
    }
    {
        nargs = 0;
        tn = t->n;
        for (i = 0; i < tn - 4; i = i + 1) {
            if (nargs >= 64) {
                break;
            }
            idx = 4 + i;
            tt = t->t[idx];
            av = &args[nargs];
            pv = parse_value(tt, inst, av);
            if (pv == 0) {
                printf("@error bad argument %s\n", tt);
                return 0;
            }
            nargs = nargs + 1;
        }
        out = w89_invoke(s, f, args, nargs);
    }
    st = out.status;
    if (st == W89_EVAL_OK) {
        printf("@return");
        ff = w89_func_ft(f);
        ovs = out.vs;
        onv = out.nvs;
        print_results(ff, ovs, onv);
        printf("\n");
    } else if (st == W89_EVAL_TRAP) {
        msg = out.msg;
        if (msg == 0) {
            msg = "trap";
        }
        printf("@trap %s\n", msg);
    } else if (st == W89_EVAL_EXHAUSTED) {
        printf("@exhaustion\n");
    } else if (st == W89_EVAL_EXCEPTION) {
        printf("@exception\n");
    } else {
        msg = out.msg;
        if (msg == 0) {
            msg = "crash";
        }
        printf("@error %s\n", msg);
    }
    w89_eval_out_free(&out);
    return 0;
}

static int cmd_get(w89_registry *reg, w89_moduleinst *last, toks *t)
{
    w89_moduleinst *inst;
    w89_globalinst *g;
    char buf[W89_REPL_BUF];
    const char *tt;
    size_t bsz;
    w89_vt *gt;
    w89_value *gv;
    w89_u32 tn;

    tn = t->n;
    if (tn != 3) {
        printf("@error malformed get\n");
        return 0;
    }
    tt = t->t[1];
    inst = resolve_module(reg, last, tt);
    if (inst == 0) {
        printf("@error unknown module %s\n", tt);
        return 0;
    }
    tt = t->t[2];
    g = find_global(inst, tt);
    if (g == 0) {
        printf("@error unknown global %s\n", tt);
        return 0;
    }
    bsz = sizeof(char[W89_REPL_BUF]);
    gt = &g->type.vt;
    gv = &g->value;
    fmt_value(buf, bsz, gt, gv);
    printf("@return %s\n", buf);
    return 0;
}

static int cmd_assert_return(w89_store *s, w89_registry *reg,
                             w89_moduleinst *last, toks *t)
{
    w89_moduleinst *inst;
    w89_funcinst *f;
    w89_value args[64];
    w89_value exp[64];
    w89_eval_out out;
    w89_u32 nargs;
    w89_u32 nexp;
    w89_u32 i;
    w89_u32 tn;
    w89_u32 idx;
    w89_u32 nv;
    w89_u32 pos;
    const char *tt;
    w89_value *av;
    w89_eval_status st;
    const w89_ft *ff;
    int ok;
    int pv;
    int eq;
    w89_value *ev;
    w89_value *ov;
    int eis_ref;
    int ois_ref;
    w89_u32 ek;
    w89_u32 ok_;
    w89_funcinst *ef;
    w89_funcinst *of;
    w89_value *ovs;
    w89_u32 onv;
    const char *fm;

    tn = t->n;
    if (tn < 4) {
        printf("@error malformed assert_return\n");
        return 0;
    }
    tt = t->t[1];
    inst = resolve_module(reg, last, tt);
    if (inst == 0) {
        printf("@fail unknown module\n");
        return 0;
    }
    tt = t->t[2];
    f = find_func(inst, tt);
    if (f == 0) {
        printf("@fail unknown function\n");
        return 0;
    }
    tn = t->n;
    if (tn < 5) {
        printf("@error malformed assert_return\n");
        return 0;
    }
    {
        ok = 0;
        tt = t->t[3];
        nv = parse_uint(tt, &ok);
        if (ok != 0) {
            nargs = (w89_u32)nv;
        }
        if (ok == 0) {
            printf("@error malformed assert_return\n");
            return 0;
        }
        tn = t->n;
        if (nargs > tn - 5) {
            printf("@error malformed assert_return\n");
            return 0;
        }
    }
    tn = t->n;
    for (i = 0; i < nargs; i = i + 1) {
        idx = 4 + i;
        tt = t->t[idx];
        av = &args[i];
        pv = parse_value(tt, inst, av);
        if (pv == 0) {
            printf("@fail bad argument %s\n", tt);
            return 0;
        }
    }
    {
        ok = 0;
        pos = 4 + nargs;
        tt = t->t[pos];
        nv = parse_uint(tt, &ok);
        if (ok != 0) {
            nexp = (w89_u32)nv;
        }
        if (ok == 0) {
            printf("@error malformed assert_return\n");
            return 0;
        }
        if (nargs + 1 + nexp > tn - 4) {
            printf("@error malformed assert_return\n");
            return 0;
        }
    }
    if (nexp > 64) {
        nexp = 64;
    }
    for (i = 0; i < nexp; i = i + 1) {
        pos = 5 + nargs + i;
        tt = t->t[pos];
        av = &exp[i];
        pv = parse_value(tt, inst, av);
        if (pv == 0) {
            printf("@fail bad expected %s\n", tt);
            return 0;
        }
    }
    out = w89_invoke(s, f, args, nargs);
    st = out.status;
    if (st != W89_EVAL_OK) {
        printf("@fail");
        fm = fail_msg(&out);
        printf(" %s", fm);
        printf("\n");
        w89_eval_out_free(&out);
        return 0;
    }
    onv = out.nvs;
    if (onv != nexp) {
        printf("@fail");
        ff = w89_func_ft(f);
        ovs = out.vs;
        print_results(ff, ovs, onv);
        printf("\n");
        w89_eval_out_free(&out);
        return 0;
    }
    for (i = 0; i < nexp; i = i + 1) {
        ev = &exp[i];
        ov = &out.vs[i];
        eis_ref = ev->is_ref;
        if (eis_ref != 0) {
            ek = ev->u.ref.kind;
            if (ek == W89_RK_FUNC) {
                ef = ev->u.ref.u.func;
                if (ef == 0) {
                    ois_ref = ov->is_ref;
                    if (ois_ref != 0) {
                        ok_ = ov->u.ref.kind;
                        if (ok_ == W89_RK_FUNC) {
                            of = ov->u.ref.u.func;
                            eq = of != 0;
                        } else {
                            eq = 0;
                        }
                    } else {
                        eq = 0;
                    }
                } else {
                    eq = values_equal(ov, ev);
                }
            } else {
                eq = values_equal(ov, ev);
            }
        } else {
            eq = values_equal(ov, ev);
        }
        if (eq == 0) {
            printf("@fail");
            ff = w89_func_ft(f);
            ovs = out.vs;
            onv = out.nvs;
            print_results(ff, ovs, onv);
            printf("\n");
            w89_eval_out_free(&out);
            return 0;
        }
    }
    printf("@pass\n");
    w89_eval_out_free(&out);
    return 0;
}

static int cmd_assert_return_get(w89_registry *reg, w89_moduleinst *last,
                                 toks *t)
{
    w89_moduleinst *inst;
    w89_globalinst *g;
    w89_value exp;
    const char *tt;
    w89_value *gv;
    int pv;
    int eq;
    w89_u32 tn;

    tn = t->n;
    if (tn != 4) {
        printf("@error malformed assert_return_get\n");
        return 0;
    }
    tt = t->t[1];
    inst = resolve_module(reg, last, tt);
    if (inst == 0) {
        printf("@fail unknown module\n");
        return 0;
    }
    tt = t->t[2];
    g = find_global(inst, tt);
    if (g == 0) {
        printf("@fail unknown global\n");
        return 0;
    }
    tt = t->t[3];
    pv = parse_value(tt, inst, &exp);
    if (pv == 0) {
        printf("@fail bad expected\n");
        return 0;
    }
    gv = &g->value;
    eq = values_equal(gv, &exp);
    if (eq != 0) {
        printf("@pass\n");
    } else {
        printf("@fail\n");
    }
    return 0;
}

static int cmd_assert_trap(w89_store *s, w89_registry *reg,
                           w89_moduleinst *last, toks *t)
{
    w89_moduleinst *inst;
    w89_funcinst *f;
    w89_value args[64];
    w89_eval_out out;
    w89_u32 nargs;
    w89_u32 i;
    w89_u32 tn;
    w89_u32 idx;
    w89_u32 nv;
    const char *tt;
    w89_value *av;
    w89_eval_status st;
    const char *msg;
    const char *hit;
    const char *fm;
    int ok;
    int pv;
    int found;

    tn = t->n;
    if (tn < 4) {
        printf("@error malformed assert_trap\n");
        return 0;
    }
    tt = t->t[1];
    inst = resolve_module(reg, last, tt);
    if (inst == 0) {
        printf("@fail unknown module\n");
        return 0;
    }
    tt = t->t[2];
    f = find_func(inst, tt);
    if (f == 0) {
        printf("@fail unknown function\n");
        return 0;
    }
    {
        ok = 0;
        tt = t->t[3];
        nv = parse_uint(tt, &ok);
        if (ok != 0) {
            nargs = (w89_u32)nv;
        }
        if (ok == 0) {
            printf("@error malformed assert_trap\n");
            return 0;
        }
        tn = t->n;
        if (nargs > tn - 4) {
            printf("@error malformed assert_trap\n");
            return 0;
        }
    }
    tn = t->n;
    for (i = 0; i < nargs; i = i + 1) {
        idx = 4 + i;
        tt = t->t[idx];
        av = &args[i];
        pv = parse_value(tt, inst, av);
        if (pv == 0) {
            printf("@fail bad argument\n");
            return 0;
        }
    }
    out = w89_invoke(s, f, args, nargs);
    st = out.status;
    if (st == W89_EVAL_TRAP) {
        tn = t->n;
        if (tn > 4 + nargs) {
            msg = out.msg;
            if (msg != 0) {
                idx = 4 + nargs;
                tt = t->t[idx];
                hit = strstr(msg, tt);
                found = hit != 0;
                if (found != 0) {
                    printf("@pass\n");
                } else {
                    fm = fail_msg(&out);
                    printf("@fail %s\n", fm);
                }
            } else {
                fm = fail_msg(&out);
                printf("@fail %s\n", fm);
            }
        } else {
            fm = fail_msg(&out);
            printf("@fail %s\n", fm);
        }
    } else if (st != W89_EVAL_OK) {
        fm = fail_msg(&out);
        printf("@fail %s\n", fm);
    } else {
        printf("@fail no trap\n");
    }
    w89_eval_out_free(&out);
    return 0;
}

static int cmd_assert_exhaustion(w89_store *s, w89_registry *reg,
                                 w89_moduleinst *last, toks *t)
{
    w89_moduleinst *inst;
    w89_funcinst *f;
    w89_value args[64];
    w89_eval_out out;
    w89_u32 nargs;
    w89_u32 i;
    w89_u32 tn;
    w89_u32 idx;
    w89_u32 nv;
    const char *tt;
    w89_value *av;
    w89_eval_status st;
    const char *fm;
    int ok;
    int pv;

    tn = t->n;
    if (tn < 4) {
        printf("@error malformed assert_exhaustion\n");
        return 0;
    }
    tt = t->t[1];
    inst = resolve_module(reg, last, tt);
    if (inst == 0) {
        printf("@fail unknown module\n");
        return 0;
    }
    tt = t->t[2];
    f = find_func(inst, tt);
    if (f == 0) {
        printf("@fail unknown function\n");
        return 0;
    }
    {
        ok = 0;
        tt = t->t[3];
        nv = parse_uint(tt, &ok);
        if (ok != 0) {
            nargs = (w89_u32)nv;
        }
        if (ok == 0) {
            printf("@error malformed assert_exhaustion\n");
            return 0;
        }
        tn = t->n;
        if (nargs > tn - 4) {
            printf("@error malformed assert_exhaustion\n");
            return 0;
        }
    }
    tn = t->n;
    for (i = 0; i < nargs; i = i + 1) {
        idx = 4 + i;
        tt = t->t[idx];
        av = &args[i];
        pv = parse_value(tt, inst, av);
        if (pv == 0) {
            printf("@fail bad argument\n");
            return 0;
        }
    }
    out = w89_invoke(s, f, args, nargs);
    st = out.status;
    if (st == W89_EVAL_EXHAUSTED) {
        printf("@pass\n");
    } else if (st != W89_EVAL_OK) {
        fm = fail_msg(&out);
        printf("@fail %s\n", fm);
    } else {
        printf("@fail no exhaustion\n");
    }
    w89_eval_out_free(&out);
    return 0;
}

static int cmd_assert_exception(w89_store *s, w89_registry *reg,
                                w89_moduleinst *last, toks *t)
{
    w89_moduleinst *inst;
    w89_funcinst *f;
    w89_value args[64];
    w89_eval_out out;
    w89_u32 nargs;
    w89_u32 i;
    w89_u32 tn;
    w89_u32 idx;
    w89_u32 nv;
    const char *tt;
    w89_value *av;
    w89_eval_status st;
    const char *fm;
    int ok;
    int pv;

    tn = t->n;
    if (tn < 4) {
        printf("@error malformed assert_exception\n");
        return 0;
    }
    tt = t->t[1];
    inst = resolve_module(reg, last, tt);
    if (inst == 0) {
        printf("@fail unknown module\n");
        return 0;
    }
    tt = t->t[2];
    f = find_func(inst, tt);
    if (f == 0) {
        printf("@fail unknown function\n");
        return 0;
    }
    {
        ok = 0;
        tt = t->t[3];
        nv = parse_uint(tt, &ok);
        if (ok != 0) {
            nargs = (w89_u32)nv;
        }
        if (ok == 0) {
            printf("@error malformed assert_exception\n");
            return 0;
        }
        tn = t->n;
        if (nargs > tn - 4) {
            printf("@error malformed assert_exception\n");
            return 0;
        }
    }
    tn = t->n;
    for (i = 0; i < nargs; i = i + 1) {
        idx = 4 + i;
        tt = t->t[idx];
        av = &args[i];
        pv = parse_value(tt, inst, av);
        if (pv == 0) {
            printf("@fail bad argument\n");
            return 0;
        }
    }
    out = w89_invoke(s, f, args, nargs);
    st = out.status;
    if (st == W89_EVAL_EXCEPTION) {
        printf("@pass\n");
    } else if (st != W89_EVAL_OK) {
        fm = fail_msg(&out);
        printf("@fail %s\n", fm);
    } else {
        printf("@fail no exception\n");
    }
    w89_eval_out_free(&out);
    return 0;
}

int w89_repl_main(void)
{
    w89_store store;
    w89_registry reg;
    w89_moduleinst *spectest;
    w89_moduleinst *last;
    char line[W89_REPL_LINE];
    char *nl;
    char *okr;
    int cmp;
    size_t lsize;
    toks t;
    w89_u32 tn;
    const char *cmd0;
    const char *t1;
    const char *t2;

    last = 0;
    lsize = sizeof(char[W89_REPL_LINE]);
    w89_store_init(&store);
    w89_registry_init(&reg);
    spectest = w89_spectest(&store);
    if (spectest != 0) {
        w89_register(&reg, "spectest", spectest);
    }

    okr = fgets(line, lsize, stdin);
    while (okr != 0) {
        nl = strchr(line, '\n');
        if (nl != 0) {
            *nl = '\0';
        }
        tok_split(&t, line);
        tn = t.n;
        if (tn == 0) {
            /* continue */
        } else {
            cmd0 = t.t[0];
            cmp = strcmp(cmd0, "quit");
            if (cmp == 0) {
                break;
            }
            cmp = strcmp(cmd0, "module");
            if (cmp == 0) {
                if (tn == 2) {
                    t1 = t.t[1];
                    cmd_module(&store, &reg, &last, t1);
                } else if (tn == 3) {
                    t1 = t.t[1];
                    t2 = t.t[2];
                    cmd_module_named(&store, &reg, &last, t1, t2);
                }
            } else {
                cmp = strcmp(cmd0, "register");
                if (cmp == 0) {
                    if (tn >= 2) {
                        cmd_register(&reg, last, &t);
                    }
                } else {
                    cmp = strcmp(cmd0, "invoke");
                    if (cmp == 0) {
                        cmd_invoke(&store, &reg, last, &t);
                    } else {
                        cmp = strcmp(cmd0, "get");
                        if (cmp == 0) {
                            cmd_get(&reg, last, &t);
                        } else {
                            cmp = strcmp(cmd0, "assert_return");
                            if (cmp == 0) {
                                cmd_assert_return(&store, &reg, last, &t);
                            } else {
                                cmp = strcmp(cmd0, "assert_return_get");
                                if (cmp == 0) {
                                    cmd_assert_return_get(&reg, last, &t);
                                } else {
                                    cmp = strcmp(cmd0, "assert_trap");
                                    if (cmp == 0) {
                                        cmd_assert_trap(&store, &reg, last, &t);
                                    } else {
                                        cmp = strcmp(cmd0, "assert_exhaustion");
                                        if (cmp == 0) {
                                            cmd_assert_exhaustion(&store, &reg,
                                                                  last, &t);
                                        } else {
                                            cmp = strcmp(cmd0,
                                                         "assert_exception");
                                            if (cmp == 0) {
                                                cmd_assert_exception(&store,
                                                                     &reg,
                                                                     last,
                                                                     &t);
                                            } else {
                                                printf("@error unknown command\n");
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        fflush(stdout);
        okr = fgets(line, lsize, stdin);
    }
    w89_store_free(&store);
    w89_registry_free(&reg);
    return 0;
}
