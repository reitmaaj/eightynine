#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eval.h"

static int failures;

static void expect(int cond, const char *name)
{
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    }
}

static w89_vt vt_num(w89_u32 num)
{
    w89_vt v;
    memset(&v, 0, sizeof(v));
    v.is_ref = 0;
    v.num = num;
    return v;
}

static w89_instr in(w89_byte op)
{
    w89_instr i;
    memset(&i, 0, sizeof(i));
    i.op = op;
    return i;
}

static w89_instr in_c32(w89_byte op, w89_i32 c)
{
    w89_instr i;
    memset(&i, 0, sizeof(i));
    i.op = op;
    i.c32 = c;
    return i;
}

static w89_instr in_c64(w89_byte op, w89_i64 c)
{
    w89_instr i;
    memset(&i, 0, sizeof(i));
    i.op = op;
    i.c64 = c;
    return i;
}

static w89_instr in_idx(w89_byte op, w89_u32 idx)
{
    w89_instr i;
    memset(&i, 0, sizeof(i));
    i.op = op;
    i.idx = idx;
    return i;
}

static w89_instr in_idx2(w89_byte op, w89_u32 a, w89_u32 b)
{
    w89_instr i;
    memset(&i, 0, sizeof(i));
    i.op = op;
    i.idx2 = a;
    i.idx = b;
    return i;
}

static w89_instr block_vt(w89_byte op, w89_byte valtype)
{
    w89_instr i;
    memset(&i, 0, sizeof(i));
    i.op = op;
    i.bt.is_typeidx = 0;
    i.bt.vt.is_ref = 0;
    i.bt.vt.num = valtype;
    return i;
}

static w89_typeenv make_env(const w89_subtype *subs, w89_u32 nsubs,
                            const w89_u32 *gsizes, w89_u32 ngroups)
{
    w89_typeenv env;
    w89_u32 idx, i, k;
    w89_u32 ng = (ngroups == 0) ? 1 : ngroups;
    memset(&env, 0, sizeof(env));
    env.nrecs = ng;
    env.recs = malloc(ng * sizeof(w89_recgroup));
    env.ntypes = nsubs;
    env.types = malloc(nsubs * sizeof(w89_deftype));
    idx = 0;
    for (i = 0; i < ng; i++) {
        env.recs[i].first = idx;
        env.recs[i].nsubs = (ngroups == 0) ? nsubs : gsizes[i];
        for (k = 0; k < env.recs[i].nsubs; k++) {
            env.types[idx].sub = &subs[idx];
            env.types[idx].recgroup = i;
            env.types[idx].recpos = k;
            idx++;
        }
    }
    return env;
}

static w89_subtype sub_func(const w89_vt *params, w89_u32 np,
                            const w89_vt *results, w89_u32 nr)
{
    w89_subtype s;
    memset(&s, 0, sizeof(s));
    s.is_final = 1;
    s.kind = W89_CK_FUNC;
    s.ft.params = (w89_vt *)params;
    s.ft.nparams = np;
    s.ft.results = (w89_vt *)results;
    s.ft.nresults = nr;
    return s;
}

static w89_func mk_func(const w89_vt *locals, w89_u32 nlocals,
                        const w89_instr *ins, w89_u32 n)
{
    w89_func f;
    memset(&f, 0, sizeof(f));
    f.locals = (w89_vt *)locals;
    f.nlocals = nlocals;
    f.code.items = (w89_instr *)ins;
    f.code.n = n;
    return f;
}

static w89_funcinst *mk_finst(w89_moduleinst *inst, w89_u32 typeidx,
                              const w89_func *f)
{
    w89_funcinst *fi = malloc(sizeof(w89_funcinst));
    memset(fi, 0, sizeof(*fi));
    fi->is_host = 0;
    fi->typeidx = typeidx;
    fi->inst = inst;
    fi->func = f;
    return fi;
}

static void add_func(w89_moduleinst *inst, w89_funcinst *f)
{
    inst->funcs = realloc(inst->funcs,
                          (inst->nfuncs + 1) * sizeof(w89_funcinst *));
    inst->funcs[inst->nfuncs++] = f;
}

static void add_table(w89_moduleinst *inst, w89_tableinst *t)
{
    inst->tables = realloc(inst->tables,
                           (inst->ntables + 1) * sizeof(w89_tableinst *));
    inst->tables[inst->ntables++] = t;
}

static void run_invoke(w89_funcinst *f, const w89_value *args, w89_u32 n,
                       w89_eval_out *out)
{
    *out = w89_invoke(NULL, f, args, n);
}

static void add_mem(w89_moduleinst *inst, w89_meminst *m)
{
    inst->memories = realloc(inst->memories,
                             (inst->nmemories + 1) * sizeof(w89_meminst *));
    inst->memories[inst->nmemories++] = m;
}

static void add_data(w89_moduleinst *inst, w89_datainst *d)
{
    inst->datas = realloc(inst->datas,
                          (inst->ndatas + 1) * sizeof(w89_datainst *));
    inst->datas[inst->ndatas++] = d;
}

static void add_elem(w89_moduleinst *inst, w89_eleminst *e)
{
    inst->elems = realloc(inst->elems,
                          (inst->nelems + 1) * sizeof(w89_eleminst *));
    inst->elems[inst->nelems++] = e;
}

static void free_module_arrays(w89_moduleinst *m)
{
    w89_u32 i;
    for (i = 0; i < m->nfuncs; i++) {
        free(m->funcs[i]);
    }
    free(m->funcs);
    free(m->tables);
    free(m->memories);
    free(m->datas);
    free(m->elems);
}

static void test_basic_call(void)
{
    w89_vt p[1];
    w89_vt r[1];
    w89_subtype sub;
    w89_typeenv env;
    w89_moduleinst m;
    w89_instr body[1];
    w89_func f;
    w89_funcinst *fi;
    w89_value arg;
    w89_eval_out out;

    p[0] = vt_num(0x7F);
    r[0] = vt_num(0x7F);
    sub = sub_func(p, 1, r, 1);
    env = make_env(&sub, 1, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;

    body[0] = in_idx(0x20, 0);
    f = mk_func(NULL, 0, body, 1);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);

    arg = w89_value_num(42);
    run_invoke(fi, &arg, 1, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 42, "basic call returns param");
    w89_eval_out_free(&out);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

static void test_void_call(void)
{
    w89_subtype sub;
    w89_typeenv env;
    w89_moduleinst m;
    w89_instr body[1];
    w89_func f;
    w89_funcinst *fi;
    w89_eval_out out;

    sub = sub_func(NULL, 0, NULL, 0);
    env = make_env(&sub, 1, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;

    body[0] = in(0x01);
    f = mk_func(NULL, 0, body, 1);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);

    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 0,
           "void call produces no results");
    w89_eval_out_free(&out);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

static void test_local_default(void)
{
    w89_vt p[2];
    w89_vt r[1];
    w89_vt l[1];
    w89_subtype sub;
    w89_typeenv env;
    w89_moduleinst m;
    w89_instr body[3];
    w89_func f;
    w89_funcinst *fi;
    w89_value args[2];
    w89_eval_out out;

    p[0] = vt_num(0x7F);
    p[1] = vt_num(0x7F);
    r[0] = vt_num(0x7F);
    l[0] = vt_num(0x7E);
    sub = sub_func(p, 2, r, 1);
    env = make_env(&sub, 1, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;

    body[0] = in_idx(0x20, 0);
    body[1] = in_idx(0x20, 1);
    body[2] = in(0x6A);
    f = mk_func(l, 1, body, 3);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);

    args[0] = w89_value_num(5);
    args[1] = w89_value_num(7);
    run_invoke(fi, args, 2, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 12, "params map to locals in order");
    w89_eval_out_free(&out);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

static void test_recursion(void)
{
    w89_vt p[1];
    w89_vt r[1];
    w89_subtype sub;
    w89_typeenv env;
    w89_moduleinst m;
    w89_instr body[16];
    w89_func f;
    w89_funcinst *fi;
    w89_value arg;
    w89_eval_out out;

    p[0] = vt_num(0x7E);
    r[0] = vt_num(0x7E);
    sub = sub_func(p, 1, r, 1);
    env = make_env(&sub, 1, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;

    body[0] = block_vt(0x02, 0x7E);
    body[1] = in_idx(0x20, 0);
    body[2] = in_c64(0x42, 2);
    body[3] = in(0x53);
    body[4] = block_vt(0x04, 0x7E);
    body[5] = in_c64(0x42, 1);
    body[6] = in(0x05);
    body[7] = in_idx(0x20, 0);
    body[8] = in_idx(0x20, 0);
    body[9] = in_c64(0x42, 1);
    body[10] = in(0x7D);
    body[11] = in_idx(0x10, 0);
    body[12] = in(0x7E);
    body[13] = in(0x0B);
    body[14] = in(0x0B);
    f = mk_func(NULL, 0, body, 15);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);

    arg = w89_value_num(5);
    run_invoke(fi, &arg, 1, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 120, "recursive fac(5) == 120");
    w89_eval_out_free(&out);

    arg = w89_value_num(1);
    run_invoke(fi, &arg, 1, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 1, "fac(1) == 1");
    w89_eval_out_free(&out);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

static void test_call_indirect(void)
{
    w89_vt r[1];
    w89_vt p[2];
    w89_subtype subs[4];
    w89_u32 gsizes[4];
    w89_typeenv env;
    w89_moduleinst m;
    w89_instr body1[1];
    w89_instr body2[1];
    w89_instr body3[1];
    w89_instr bodyr[2];
    w89_func f1;
    w89_func f2;
    w89_func f3;
    w89_func fr;
    w89_funcinst *fi1;
    w89_funcinst *fi2;
    w89_funcinst *fi3;
    w89_funcinst *fir;
    w89_tableinst tab;
    w89_value arg;
    w89_eval_out out;

    r[0] = vt_num(0x7F);
    p[0] = vt_num(0x7F);
    p[1] = vt_num(0x7F);
    subs[0] = sub_func(NULL, 0, r, 1);
    subs[1] = sub_func(NULL, 0, r, 1);
    subs[2] = sub_func(p, 1, r, 1);
    subs[3] = sub_func(p, 2, r, 1);
    gsizes[0] = 1;
    gsizes[1] = 1;
    gsizes[2] = 1;
    gsizes[3] = 1;
    env = make_env(subs, 4, gsizes, 4);
    memset(&m, 0, sizeof(m));
    m.types = &env;

    body1[0] = in_c32(0x41, 1);
    body2[0] = in_c32(0x41, 2);
    body3[0] = in_idx(0x20, 0);
    f1 = mk_func(NULL, 0, body1, 1);
    f2 = mk_func(NULL, 0, body2, 1);
    f3 = mk_func(NULL, 0, body3, 1);
    fi1 = mk_finst(&m, 0, &f1);
    fi2 = mk_finst(&m, 1, &f2);
    fi3 = mk_finst(&m, 2, &f3);
    add_func(&m, fi1);
    add_func(&m, fi2);
    add_func(&m, fi3);

    memset(&tab, 0, sizeof(tab));
    tab.type.limits.min = 4;
    tab.type.rt.is_typeidx = 0;
    tab.type.rt.abs = W89_HT_FUNC;
    tab.type.rt.nullable = 1;
    tab.size = 4;
    tab.elems = malloc(4 * sizeof(w89_ref));
    tab.elems[0] = w89_ref_func(fi1);
    tab.elems[1] = w89_ref_func(fi2);
    tab.elems[2] = w89_ref_null();
    tab.elems[3] = w89_ref_func(fi3);
    add_table(&m, &tab);

    bodyr[0] = in_idx(0x20, 0);
    bodyr[1] = in_idx2(0x11, 0, 0);
    fr = mk_func(NULL, 0, bodyr, 2);
    fir = mk_finst(&m, 2, &fr);
    add_func(&m, fir);

    arg = w89_value_num(0);
    run_invoke(fir, &arg, 1, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 1, "call_indirect slot 0");
    w89_eval_out_free(&out);

    arg = w89_value_num(1);
    run_invoke(fir, &arg, 1, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 2, "call_indirect slot 1 (canonical type)");
    w89_eval_out_free(&out);

    arg = w89_value_num(2);
    run_invoke(fir, &arg, 1, &out);
    expect(out.status == W89_EVAL_TRAP && out.msg
           && strstr(out.msg, "uninitialized element") != NULL,
           "call_indirect null slot");
    w89_eval_out_free(&out);

    arg = w89_value_num(5);
    run_invoke(fir, &arg, 1, &out);
    expect(out.status == W89_EVAL_TRAP && out.msg
           && strstr(out.msg, "undefined element") != NULL,
           "call_indirect out of bounds");
    w89_eval_out_free(&out);

    arg = w89_value_num(3);
    run_invoke(fir, &arg, 1, &out);
    expect(out.status == W89_EVAL_TRAP && out.msg
           && strstr(out.msg, "indirect call type mismatch") != NULL,
           "call_indirect type mismatch");
    w89_eval_out_free(&out);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
    free(tab.elems);
}

static void test_call_ref(void)
{
    w89_vt p[1];
    w89_vt r[1];
    w89_subtype subs[2];
    w89_u32 gsizes[2];
    w89_typeenv env;
    w89_moduleinst m;
    w89_instr callee_body[1];
    w89_instr caller_body[3];
    w89_instr null_body[2];
    w89_func cf;
    w89_func cr;
    w89_func nf;
    w89_funcinst *cfi;
    w89_funcinst *cri;
    w89_funcinst *nfi;
    w89_eval_out out;

    p[0] = vt_num(0x7F);
    r[0] = vt_num(0x7F);
    subs[0] = sub_func(p, 1, r, 1);
    subs[1] = sub_func(NULL, 0, r, 1);
    gsizes[0] = 1;
    gsizes[1] = 1;
    env = make_env(subs, 2, gsizes, 2);
    memset(&m, 0, sizeof(m));
    m.types = &env;

    callee_body[0] = in_idx(0x20, 0);
    cf = mk_func(NULL, 0, callee_body, 1);
    cfi = mk_finst(&m, 0, &cf);
    add_func(&m, cfi);

    caller_body[0] = in_c32(0x41, 5);
    caller_body[1] = in_idx(0xD2, 0);
    caller_body[2] = in_idx(0x14, 0);
    cr = mk_func(NULL, 0, caller_body, 3);
    cri = mk_finst(&m, 1, &cr);
    add_func(&m, cri);

    run_invoke(cri, NULL, 0, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 5, "call_ref dispatches with args");
    w89_eval_out_free(&out);

    null_body[0] = in(0xD0);
    null_body[1] = in_idx(0x14, 0);
    nf = mk_func(NULL, 0, null_body, 2);
    nfi = mk_finst(&m, 1, &nf);
    add_func(&m, nfi);

    run_invoke(nfi, NULL, 0, &out);
    expect(out.status == W89_EVAL_TRAP && out.msg
           && strcmp(out.msg, "null function reference") == 0,
           "call_ref null reference");
    w89_eval_out_free(&out);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

static void test_tail_call(void)
{
    w89_vt p[1];
    w89_vt r[1];
    w89_subtype sub;
    w89_typeenv env;
    w89_moduleinst m;
    w89_instr abody[4];
    w89_instr bbody[3];
    w89_func af;
    w89_func bf;
    w89_funcinst *afi;
    w89_funcinst *bfi;
    w89_value arg;
    w89_eval_out out;

    p[0] = vt_num(0x7F);
    r[0] = vt_num(0x7F);
    sub = sub_func(p, 1, r, 1);
    env = make_env(&sub, 1, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;

    abody[0] = in_idx(0x20, 0);
    abody[1] = in_c32(0x41, 1);
    abody[2] = in(0x6A);
    abody[3] = in_idx(0x12, 1);
    af = mk_func(NULL, 0, abody, 4);
    afi = mk_finst(&m, 0, &af);
    add_func(&m, afi);

    bbody[0] = in_idx(0x20, 0);
    bbody[1] = in_c32(0x41, 2);
    bbody[2] = in(0x6C);
    bf = mk_func(NULL, 0, bbody, 3);
    bfi = mk_finst(&m, 0, &bf);
    add_func(&m, bfi);

    arg = w89_value_num(3);
    run_invoke(afi, &arg, 1, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 8, "return_call tail call (3+1)*2");
    w89_eval_out_free(&out);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

static void test_exhaustion(void)
{
    w89_subtype sub;
    w89_typeenv env;
    w89_moduleinst m;
    w89_instr body[1];
    w89_func f;
    w89_funcinst *fi;
    w89_eval_out out;

    sub = sub_func(NULL, 0, NULL, 0);
    env = make_env(&sub, 1, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;

    body[0] = in_idx(0x10, 0);
    f = mk_func(NULL, 0, body, 1);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);

    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_EXHAUSTED && out.msg
           && strcmp(out.msg, "call stack exhausted") == 0,
           "infinite recursion exhausts budget");
    w89_eval_out_free(&out);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

static w89_host_status host_id(const w89_value *args, w89_u32 nargs,
                               w89_value *res, w89_u32 *nres,
                               const char **trap)
{
    (void)nargs;
    (void)trap;
    res[0] = args[0];
    *nres = 1;
    return W89_HOST_OK;
}

static w89_host_status host_bad(const w89_value *args, w89_u32 nargs,
                                w89_value *res, w89_u32 *nres,
                                const char **trap)
{
    (void)args;
    (void)nargs;
    (void)res;
    *nres = 0;
    *trap = "host boom";
    return W89_HOST_TRAP;
}

static void test_host_func(void)
{
    w89_vt p[1];
    w89_vt r[1];
    w89_ft hft;
    w89_moduleinst m;
    w89_funcinst h1;
    w89_funcinst h2;
    w89_value arg;
    w89_eval_out out;

    memset(&m, 0, sizeof(m));
    p[0] = vt_num(0x7F);
    r[0] = vt_num(0x7F);
    hft.params = p;
    hft.nparams = 1;
    hft.results = r;
    hft.nresults = 1;

    memset(&h1, 0, sizeof(h1));
    h1.is_host = 1;
    h1.ft = &hft;
    h1.host = host_id;

    arg = w89_value_num(77);
    run_invoke(&h1, &arg, 1, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 77, "host function returns arg");
    w89_eval_out_free(&out);

    memset(&h2, 0, sizeof(h2));
    h2.is_host = 1;
    h2.ft = &hft;
    h2.host = host_bad;
    run_invoke(&h2, &arg, 1, &out);
    expect(out.status == W89_EVAL_TRAP && out.msg
           && strcmp(out.msg, "host boom") == 0, "host function trap");
    w89_eval_out_free(&out);
}

static void test_wrong_args(void)
{
    w89_vt p[1];
    w89_ft hft;
    w89_funcinst h;
    w89_eval_out out;

    p[0] = vt_num(0x7F);
    hft.params = p;
    hft.nparams = 1;
    hft.results = NULL;
    hft.nresults = 0;
    memset(&h, 0, sizeof(h));
    h.is_host = 1;
    h.ft = &hft;
    h.host = host_bad;

    run_invoke(&h, NULL, 0, &out);
    expect(out.status == W89_EVAL_CRASH && out.msg
           && strcmp(out.msg, "wrong number of arguments") == 0,
           "wrong argument count crashes");
    w89_eval_out_free(&out);
}

static w89_meminst *mk_mem(w89_u32 npages)
{
    w89_meminst *m = malloc(sizeof(w89_meminst));
    memset(m, 0, sizeof(*m));
    m->limits.min = npages;
    m->npages = npages;
    m->bytes = calloc((size_t)npages * W89_PAGE_SIZE, 1);
    return m;
}

static void test_mem_init_poporder(void)
{
    w89_vt r[1];
    w89_subtype sub;
    w89_typeenv env;
    w89_moduleinst m;
    static const w89_byte dbytes[8] = { 1, 0, 0, 0, 2, 0, 0, 0 };
    w89_datainst di;
    w89_meminst *mem;
    w89_instr body[6];
    w89_func f;
    w89_funcinst *fi;
    w89_eval_out out;

    r[0] = vt_num(0x7F);
    sub = sub_func(NULL, 0, r, 1);
    env = make_env(&sub, 1, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;

    mem = mk_mem(1);
    add_mem(&m, mem);
    memset(&di, 0, sizeof(di));
    di.bytes = dbytes;
    di.len = 8;
    add_data(&m, &di);

    body[0] = in_c32(0x41, 1);
    body[1] = in_c32(0x41, 0);
    body[2] = in_c32(0x41, 4);
    body[3] = in_idx2(0xFC, 0, 0);
    body[3].sub = 0x08;
    body[4] = in_c32(0x41, 1);
    body[5] = in(0x28);
    f = mk_func(NULL, 0, body, 6);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);

    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 1,
           "memory.init pops n,s,d (d=1,s=0,n=4 => load 1)");
    w89_eval_out_free(&out);

    free(mem->bytes);
    free(mem);
    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

static void test_mem_init_indexswap(void)
{
    w89_vt r[1];
    w89_subtype sub;
    w89_typeenv env;
    w89_moduleinst m;
    static const w89_byte b0[1] = { 0x01 };
    static const w89_byte b1[1] = { 0x02 };
    w89_datainst d0;
    w89_datainst d1;
    w89_meminst *mem0;
    w89_meminst *mem1;
    w89_instr body[6];
    w89_func f;
    w89_funcinst *fi;
    w89_eval_out out;

    r[0] = vt_num(0x7F);
    sub = sub_func(NULL, 0, r, 1);
    env = make_env(&sub, 1, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;

    mem0 = mk_mem(1);
    add_mem(&m, mem0);
    mem1 = mk_mem(1);
    add_mem(&m, mem1);
    memset(&d0, 0, sizeof(d0));
    d0.bytes = b0;
    d0.len = 1;
    add_data(&m, &d0);
    memset(&d1, 0, sizeof(d1));
    d1.bytes = b1;
    d1.len = 1;
    add_data(&m, &d1);

    body[0] = in_c32(0x41, 0);
    body[1] = in_c32(0x41, 0);
    body[2] = in_c32(0x41, 1);
    body[3] = in_idx2(0xFC, 0, 1);
    body[3].sub = 0x08;
    body[4] = in_c32(0x41, 0);
    body[5] = in(0x28);
    body[5].memidx = 1;
    f = mk_func(NULL, 0, body, 6);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);

    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 1,
           "memory.init targets memidx (idx) not dataidx (idx2)");
    w89_eval_out_free(&out);

    free(mem0->bytes);
    free(mem0);
    free(mem1->bytes);
    free(mem1);
    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

static void test_table_init_indexswap(void)
{
    w89_vt r[1];
    w89_subtype subs[2];
    w89_u32 gsizes[2];
    w89_typeenv env;
    w89_moduleinst m;
    w89_tableinst *t0;
    w89_tableinst *t1;
    w89_eleminst e0;
    w89_eleminst e1;
    w89_instr b0[1];
    w89_instr b1[1];
    w89_instr body[6];
    w89_func f0;
    w89_func f1;
    w89_func fr;
    w89_funcinst *fi0;
    w89_funcinst *fi1;
    w89_funcinst *fir;
    w89_eval_out out;

    r[0] = vt_num(0x7F);
    subs[0] = sub_func(NULL, 0, r, 1);
    subs[1] = sub_func(NULL, 0, r, 1);
    gsizes[0] = 1;
    gsizes[1] = 1;
    env = make_env(subs, 2, gsizes, 2);
    memset(&m, 0, sizeof(m));
    m.types = &env;

    b0[0] = in_c32(0x41, 11);
    b1[0] = in_c32(0x41, 22);
    f0 = mk_func(NULL, 0, b0, 1);
    f1 = mk_func(NULL, 0, b1, 1);
    fi0 = mk_finst(&m, 0, &f0);
    fi1 = mk_finst(&m, 1, &f1);
    add_func(&m, fi0);
    add_func(&m, fi1);

    t0 = malloc(sizeof(w89_tableinst));
    t1 = malloc(sizeof(w89_tableinst));
    memset(t0, 0, sizeof(*t0));
    memset(t1, 0, sizeof(*t1));
    t0->type.limits.min = 10;
    t0->size = 10;
    t0->elems = calloc(10, sizeof(w89_ref));
    t1->type.limits.min = 10;
    t1->size = 10;
    t1->elems = calloc(10, sizeof(w89_ref));
    add_table(&m, t0);
    add_table(&m, t1);

    memset(&e0, 0, sizeof(e0));
    e0.refs = malloc(sizeof(w89_ref));
    e0.refs[0] = w89_ref_func(fi0);
    e0.n = 1;
    add_elem(&m, &e0);
    memset(&e1, 0, sizeof(e1));
    e1.refs = malloc(sizeof(w89_ref));
    e1.refs[0] = w89_ref_func(fi1);
    e1.n = 1;
    add_elem(&m, &e1);

    body[0] = in_c32(0x41, 5);
    body[1] = in_c32(0x41, 0);
    body[2] = in_c32(0x41, 1);
    body[3] = in_idx2(0xFC, 0, 1);
    body[3].sub = 0x0C;
    body[4] = in_c32(0x41, 5);
    body[5] = in_idx(0x25, 1);
    fr = mk_func(NULL, 0, body, 6);
    fir = mk_finst(&m, 0, &fr);
    add_func(&m, fir);

    run_invoke(fir, NULL, 0, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].is_ref
           && out.vs[0].u.ref.kind == W89_RK_FUNC
           && out.vs[0].u.ref.u.func == fi0,
           "table.init targets tableidx (idx) not elemidx (idx2)");
    w89_eval_out_free(&out);

    free(t0->elems);
    free(t0);
    free(t1->elems);
    free(t1);
    free(e0.refs);
    free(e1.refs);
    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

static void test_mem_load_store(void)
{
    w89_vt r32[1];
    w89_vt r64[1];
    w89_subtype subs[2];
    w89_u32 gsizes[2];
    w89_typeenv env;
    w89_moduleinst m;
    w89_meminst *mem;
    w89_func f;
    w89_funcinst *fi;
    w89_instr body[8];
    w89_eval_out out;

    r32[0] = vt_num(0x7F);
    r64[0] = vt_num(0x7E);
    subs[0] = sub_func(NULL, 0, r32, 1);
    subs[1] = sub_func(NULL, 0, r64, 1);
    gsizes[0] = 1;
    gsizes[1] = 1;
    env = make_env(subs, 2, gsizes, 2);
    memset(&m, 0, sizeof(m));
    m.types = &env;

    mem = mk_mem(1);
    mem->bytes[0] = 0xFF;
    add_mem(&m, mem);

    body[0] = in_c32(0x41, 0);
    body[1] = in(0x2C);
    f = mk_func(NULL, 0, body, 2);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);
    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 0xFFFFFFFFu,
           "i32.load8_s sign-extends");
    w89_eval_out_free(&out);

    body[0] = in_c32(0x41, 0);
    body[1] = in(0x2D);
    f = mk_func(NULL, 0, body, 2);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);
    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 0xFFu, "i32.load8_u zero-extends");
    w89_eval_out_free(&out);

    body[0] = in_c32(0x41, 0);
    body[1] = in(0x29);
    f = mk_func(NULL, 0, body, 2);
    fi = mk_finst(&m, 1, &f);
    add_func(&m, fi);
    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 0xFFu, "i64.load zero-extends");
    w89_eval_out_free(&out);

    body[0] = in_c32(0x41, 0xFFFD);
    body[1] = in_c32(0x41, 0x12345678);
    body[2] = in(0x36);
    f = mk_func(NULL, 0, body, 3);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);
    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_TRAP && out.msg
           && strstr(out.msg, "out of bounds memory access") != NULL,
           "boundary store traps");
    w89_eval_out_free(&out);

    body[0] = in_c32(0x41, 0xFFFD);
    body[1] = in(0x2D);
    f = mk_func(NULL, 0, body, 2);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);
    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 0, "trapped store wrote no partial data");
    w89_eval_out_free(&out);

    body[0] = in_c32(0x41, 0xFFF8);
    body[1] = in_c64(0x42, 0x123456789ABCDEF0L);
    body[2] = in(0x37);
    body[3] = in_c32(0x41, 0xFFF8);
    body[4] = in(0x29);
    f = mk_func(NULL, 0, body, 5);
    fi = mk_finst(&m, 1, &f);
    add_func(&m, fi);
    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 0x123456789ABCDEF0UL,
           "i64.store/i64.load round-trip");
    w89_eval_out_free(&out);

    free(mem->bytes);
    free(mem);
    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

static void test_mem_copy_fill(void)
{
    w89_vt r32[1];
    w89_subtype sub;
    w89_typeenv env;
    w89_moduleinst m;
    w89_meminst *mem;
    w89_datainst di;
    w89_func f;
    w89_funcinst *fi;
    w89_instr body[12];
    w89_eval_out out;

    r32[0] = vt_num(0x7F);
    sub = sub_func(NULL, 0, r32, 1);
    env = make_env(&sub, 1, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;
    mem = mk_mem(1);
    add_mem(&m, mem);
    memset(&di, 0, sizeof(di));
    di.bytes = (const w89_byte *)"\x01\x02\x03\x04";
    di.len = 4;
    add_data(&m, &di);

    body[0] = in_c32(0x41, 0);
    body[1] = in_c32(0x41, 0x5A);
    body[2] = in_c32(0x41, 8);
    body[3] = in_idx2(0xFC, 0, 0);
    body[3].sub = 0x0B;
    body[4] = in_c32(0x41, 0);
    body[5] = in_c32(0x41, 4);
    body[6] = in_c32(0x41, 4);
    body[7] = in_idx2(0xFC, 0, 0);
    body[7].sub = 0x0A;
    body[8] = in_c32(0x41, 0);
    body[9] = in(0x28);
    f = mk_func(NULL, 0, body, 10);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);
    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 0x5A5A5A5Au,
           "memory.fill then overlapping memory.copy");
    w89_eval_out_free(&out);

    body[0] = in_c32(0x41, 0);
    body[1] = in_c32(0x41, 0);
    body[2] = in_c32(0x41, 0);
    body[3] = in_idx2(0xFC, 0, 0);
    body[3].sub = 0x08;
    f = mk_func(NULL, 0, body, 4);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);
    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 0,
           "memory.init n=0 in bounds no-ops");
    w89_eval_out_free(&out);

    body[0] = in_c64(0x42, 0xFFFFFFFFFFFFFFFFUL);
    body[1] = in_c32(0x41, 0);
    body[2] = in_c32(0x41, 0);
    body[3] = in_idx2(0xFC, 0, 0);
    body[3].sub = 0x08;
    f = mk_func(NULL, 0, body, 4);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);
    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_TRAP && out.msg
           && strstr(out.msg, "out of bounds memory access") != NULL,
           "memory.init n=0 with out-of-bounds base traps");
    w89_eval_out_free(&out);

    free(mem->bytes);
    free(mem);
    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

static void test_table_ops(void)
{
    w89_vt r32[1];
    w89_subtype sub;
    w89_typeenv env;
    w89_moduleinst m;
    w89_tableinst *tab;
    w89_func f;
    w89_func f1;
    w89_funcinst *fi;
    w89_funcinst *fi1;
    w89_instr body[8];
    w89_eval_out out;

    r32[0] = vt_num(0x7F);
    sub = sub_func(NULL, 0, r32, 1);
    env = make_env(&sub, 1, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;

    tab = malloc(sizeof(w89_tableinst));
    memset(tab, 0, sizeof(*tab));
    tab->type.limits.min = 3;
    tab->size = 3;
    tab->elems = calloc(3, sizeof(w89_ref));
    add_table(&m, tab);

    {
        w89_instr b0[1];
        b0[0] = in_c32(0x41, 7);
        f = mk_func(NULL, 0, b0, 1);
        fi = mk_finst(&m, 0, &f);
        add_func(&m, fi);
    }
    {
        w89_instr b1[1];
        b1[0] = in_c32(0x41, 8);
        f1 = mk_func(NULL, 0, b1, 1);
        fi1 = mk_finst(&m, 0, &f1);
        add_func(&m, fi1);
    }

    body[0] = in_idx(0xFC, 0);
    body[0].sub = 0x10;
    f = mk_func(NULL, 0, body, 1);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);
    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 3, "table.size returns size");
    w89_eval_out_free(&out);

    body[0] = in(0xD0);
    body[1] = in_c32(0x41, 2);
    body[2] = in_idx(0xFC, 0);
    body[2].sub = 0x0F;
    f = mk_func(NULL, 0, body, 3);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);
    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 3, "table.grow returns old size");
    w89_eval_out_free(&out);

    body[0] = in_idx(0xFC, 0);
    body[0].sub = 0x10;
    f = mk_func(NULL, 0, body, 1);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);
    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 5, "table.size reflects grow");
    w89_eval_out_free(&out);

    body[0] = in_c32(0x41, 4);
    body[1] = in_idx(0xD2, 0);
    body[2] = in_c32(0x41, 1);
    body[3] = in_idx(0xFC, 0);
    body[3].sub = 0x11;
    body[4] = in_c32(0x41, 4);
    body[5] = in_idx(0x25, 0);
    f = mk_func(NULL, 0, body, 6);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);
    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].is_ref
           && out.vs[0].u.ref.kind == W89_RK_FUNC
           && out.vs[0].u.ref.u.func == m.funcs[0],
           "table.fill then table.get returns the ref");
    w89_eval_out_free(&out);

    body[0] = in_c32(0x41, 0);
    body[1] = in_idx(0xD2, 1);
    body[2] = in_idx(0x26, 0);
    body[3] = in_c32(0x41, 0);
    body[4] = in_idx(0x25, 0);
    f = mk_func(NULL, 0, body, 5);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);
    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].is_ref
           && out.vs[0].u.ref.kind == W89_RK_FUNC
           && out.vs[0].u.ref.u.func == m.funcs[1],
           "table.set then table.get returns the ref");
    w89_eval_out_free(&out);

    free(tab->elems);
    free(tab);
    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

static void test_ref_ops(void)
{
    w89_vt r32[1];
    w89_subtype sub;
    w89_typeenv env;
    w89_moduleinst m;
    w89_func f;
    w89_func f1;
    w89_funcinst *fi;
    w89_funcinst *fi1;
    w89_instr body[3];
    w89_eval_out out;

    r32[0] = vt_num(0x7F);
    sub = sub_func(NULL, 0, r32, 1);
    env = make_env(&sub, 1, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;
    {
        w89_instr b0[1];
        b0[0] = in_c32(0x41, 7);
        f = mk_func(NULL, 0, b0, 1);
        fi = mk_finst(&m, 0, &f);
        add_func(&m, fi);
    }
    {
        w89_instr b1[1];
        b1[0] = in_c32(0x41, 8);
        f1 = mk_func(NULL, 0, b1, 1);
        fi1 = mk_finst(&m, 0, &f1);
        add_func(&m, fi1);
    }

    body[0] = in(0xD0);
    body[1] = in(0xD1);
    f = mk_func(NULL, 0, body, 2);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);
    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 1, "ref.is_null on null is 1");
    w89_eval_out_free(&out);

    body[0] = in_idx(0xD2, 1);
    body[1] = in(0xD1);
    f = mk_func(NULL, 0, body, 2);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);
    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 0, "ref.is_null on func is 0");
    w89_eval_out_free(&out);

    body[0] = in(0xD0);
    body[1] = in(0xD0);
    body[2] = in(0xD3);
    f = mk_func(NULL, 0, body, 3);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);
    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 1, "ref.eq null/null is 1");
    w89_eval_out_free(&out);

    body[0] = in_idx(0xD2, 0);
    body[1] = in_idx(0xD2, 1);
    body[2] = in(0xD3);
    f = mk_func(NULL, 0, body, 3);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);
    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 0, "ref.eq distinct funcs is 0");
    w89_eval_out_free(&out);

    body[0] = in(0xD0);
    body[1] = in(0xD4);
    f = mk_func(NULL, 0, body, 2);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);
    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_TRAP && out.msg
           && strstr(out.msg, "null reference") != NULL,
           "ref.as_non_null traps on null");
    w89_eval_out_free(&out);

    body[0] = in_idx(0xD2, 0);
    body[1] = in(0xD4);
    body[2] = in(0xD1);
    f = mk_func(NULL, 0, body, 3);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);
    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 0, "ref.as_non_null keeps non-null ref");
    w89_eval_out_free(&out);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

static w89_eval_out invoke_env(w89_funcinst *f, const w89_value *args,
                               w89_u32 n, const char *iter)
{
    int r;
    r = setenv("W89_ITER", iter, 1);
    if (r != 0) {
        fprintf(stderr, "setenv failed\n");
        exit(1);
    }
    return w89_invoke(NULL, f, args, n);
}

/* S2.3: direct two-driver parity on a body that performs a real `call`
 * against a funcinst in the frame module (testing 0021 ITC-001/ITC-004).
 * Both drivers must give the same result for an identity callee. */
static void test_iter_direct_call_parity(void)
{
    w89_vt p[1];
    w89_vt r[1];
    w89_subtype sub;
    w89_typeenv env;
    w89_moduleinst m;
    w89_instr callee_body[1];
    w89_instr caller_body[2];
    w89_func cf;
    w89_func cr;
    w89_funcinst *cfi;
    w89_funcinst *cri;
    w89_frame fr;
    w89_config ci;
    w89_eval_out oi;
    int ok;
    int ok2;

    p[0] = vt_num(0x7F);
    r[0] = vt_num(0x7F);
    sub = sub_func(p, 1, r, 1);
    env = make_env(&sub, 1, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;

    callee_body[0] = in_idx(0x20, 0);
    cf = mk_func(NULL, 0, callee_body, 1);
    cfi = mk_finst(&m, 0, &cf);
    add_func(&m, cfi);

    caller_body[0] = in_c32(0x41, 9);
    caller_body[1] = in_idx(0x10, 0);
    cr = mk_func(NULL, 0, caller_body, 2);
    cri = mk_finst(&m, 1, &cr);
    add_func(&m, cri);

    memset(&fr, 0, sizeof(fr));
    fr.inst = &m;

    w89_config_init(&ci, &fr);
    w89_code_range(&ci.code, caller_body, 2, 0, 2);
    oi = w89_eval_iter(&ci);

    ok = (oi.status == W89_EVAL_OK) && (oi.nvs == 1);
    if (ok) {
        ok2 = (oi.vs[0].u.num == 9);
        ok = ok && ok2;
    }
    expect(ok, "iter direct call forwards the value");
    w89_eval_out_free(&oi);
    w89_config_free(&ci);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

/* S2.3: a non-tail `down` recursion through w89_invoke gives the same
 * outcome under the legacy default and under W89_ITER=1 (ITC-004), for a
 * depth both drivers can run quickly. */
static void test_iter_down_parity(void)
{
    w89_vt p[1];
    w89_subtype sub;
    w89_typeenv env;
    w89_moduleinst m;
    w89_instr body[9];
    w89_func f;
    w89_funcinst *fi;
    w89_eval_out ol;
    w89_eval_out oi;
    w89_value arg;

    p[0] = vt_num(0x7F);
    sub = sub_func(p, 1, NULL, 0);
    env = make_env(&sub, 1, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;

    body[0] = in_idx(0x20, 0);
    body[1] = in_c32(0x41, 0);
    body[2] = in(0x47);
    body[3] = in(0x04);
    body[4] = in_idx(0x20, 0);
    body[5] = in_c32(0x41, 1);
    body[6] = in(0x6B);
    body[7] = in_idx(0x10, 0);
    body[8] = in(0x0B);
    f = mk_func(NULL, 0, body, 9);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);

    arg = w89_value_num(900);
    ol = invoke_env(fi, &arg, 1, "0");
    oi = invoke_env(fi, &arg, 1, "1");
    expect(ol.status == W89_EVAL_OK && ol.nvs == 0,
           "down(900) legacy returns normally");
    expect(oi.status == W89_EVAL_OK && oi.nvs == 0,
           "down(900) iterative returns normally (no false exhaustion)");
    w89_eval_out_free(&ol);
    w89_eval_out_free(&oi);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

/* S2.3: genuinely unbounded recursion exhausts the budget under W89_ITER=1
 * (ITC-002), with the same message as legacy. */
static void test_iter_exhaustion(void)
{
    w89_subtype sub;
    w89_typeenv env;
    w89_moduleinst m;
    w89_instr body[1];
    w89_func f;
    w89_funcinst *fi;
    w89_eval_out oi;

    sub = sub_func(NULL, 0, NULL, 0);
    env = make_env(&sub, 1, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;

    body[0] = in_idx(0x10, 0);
    f = mk_func(NULL, 0, body, 1);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);

    oi = invoke_env(fi, NULL, 0, "1");
    expect(oi.status == W89_EVAL_EXHAUSTED && oi.msg
           && strcmp(oi.msg, "call stack exhausted") == 0,
           "iterative unbounded recursion exhausts at budget");
    w89_eval_out_free(&oi);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

/* S2.3: a wasm function that calls an imported host function gives the same
 * result under legacy and W89_ITER=1 (ITC-003 host inline). */
static void test_iter_host_call(void)
{
    w89_vt p[1];
    w89_vt r[1];
    w89_subtype subs[2];
    w89_u32 gsizes[2];
    w89_typeenv env;
    w89_moduleinst m;
    w89_ft hft;
    w89_funcinst *hw;
    w89_instr wb[2];
    w89_func wf;
    w89_funcinst *wi;
    w89_eval_out ol;
    w89_eval_out oi;

    p[0] = vt_num(0x7F);
    r[0] = vt_num(0x7F);
    subs[0] = sub_func(p, 1, r, 1);
    subs[1] = sub_func(NULL, 0, r, 1);
    gsizes[0] = 1;
    gsizes[1] = 1;
    env = make_env(subs, 2, gsizes, 2);
    memset(&m, 0, sizeof(m));
    m.types = &env;

    memset(&hft, 0, sizeof(hft));
    hft.params = p;
    hft.nparams = 1;
    hft.results = r;
    hft.nresults = 1;
    hw = mk_finst(&m, 0, NULL);
    hw->is_host = 1;
    hw->ft = &hft;
    hw->host = host_id;
    hw->inst = &m;
    add_func(&m, hw);

    wb[0] = in_c32(0x41, 5);
    wb[1] = in_idx(0x10, 0);
    wf = mk_func(NULL, 0, wb, 2);
    wi = mk_finst(&m, 1, &wf);
    add_func(&m, wi);

    ol = invoke_env(wi, NULL, 0, "0");
    oi = invoke_env(wi, NULL, 0, "1");
    expect(ol.status == W89_EVAL_OK && ol.nvs == 1
           && ol.vs[0].u.num == 5, "legacy wasm->host call returns 5");
    expect(oi.status == W89_EVAL_OK && oi.nvs == 1
           && oi.vs[0].u.num == 5, "iterative wasm->host call returns 5");
    w89_eval_out_free(&ol);
    w89_eval_out_free(&oi);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

/* S2.4: a self tail-recursive countdown (return_call to itself) driven under
 * W89_ITER=1 must complete without exhausting the budget even when the depth
 * exceeds the 5000-frame non-tail budget ceiling, because each tail step
 * reuses the frame in place (ITT-001). */
static void test_iter_tail_self_loop(void)
{
    w89_vt p[1];
    w89_vt r[1];
    w89_subtype sub;
    w89_typeenv env;
    w89_moduleinst m;
    w89_instr body[10];
    w89_func f;
    w89_funcinst *fi;
    w89_eval_out oi;
    w89_value arg;

    p[0] = vt_num(0x7F);
    r[0] = vt_num(0x7F);
    sub = sub_func(p, 1, r, 1);
    env = make_env(&sub, 1, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;

    body[0] = in_idx(0x20, 0);
    body[1] = in(0x45);
    body[2] = block_vt(0x04, 0x7F);
    body[3] = in_c32(0x41, 0);
    body[4] = in(0x05);
    body[5] = in_idx(0x20, 0);
    body[6] = in_c32(0x41, 1);
    body[7] = in(0x6B);
    body[8] = in_idx(0x12, 0);
    body[9] = in(0x0B);
    f = mk_func(NULL, 0, body, 10);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);

    arg = w89_value_num(1000000);
    oi = invoke_env(fi, &arg, 1, "1");
    expect(oi.status == W89_EVAL_OK && oi.nvs == 1
           && oi.vs[0].u.num == 0,
           "iterative 1M self tail countdown returns without exhaustion");
    w89_eval_out_free(&oi);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

/* S2.4: a small self tail-recursive countdown gives the same result under
 * the legacy default and under W89_ITER=1 (ITT-001). */
static void test_iter_tail_self_parity(void)
{
    w89_vt p[1];
    w89_vt r[1];
    w89_subtype sub;
    w89_typeenv env;
    w89_moduleinst m;
    w89_instr body[10];
    w89_func f;
    w89_funcinst *fi;
    w89_eval_out ol;
    w89_eval_out oi;
    w89_value arg;

    p[0] = vt_num(0x7F);
    r[0] = vt_num(0x7F);
    sub = sub_func(p, 1, r, 1);
    env = make_env(&sub, 1, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;

    body[0] = in_idx(0x20, 0);
    body[1] = in(0x45);
    body[2] = block_vt(0x04, 0x7F);
    body[3] = in_c32(0x41, 0);
    body[4] = in(0x05);
    body[5] = in_idx(0x20, 0);
    body[6] = in_c32(0x41, 1);
    body[7] = in(0x6B);
    body[8] = in_idx(0x12, 0);
    body[9] = in(0x0B);
    f = mk_func(NULL, 0, body, 10);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);

    arg = w89_value_num(300);
    ol = invoke_env(fi, &arg, 1, "0");
    oi = invoke_env(fi, &arg, 1, "1");
    expect(ol.status == W89_EVAL_OK && ol.nvs == 1
           && ol.vs[0].u.num == 0, "legacy self tail returns 0");
    expect(oi.status == ol.status && oi.nvs == ol.nvs
           && oi.vs[0].u.num == 0,
           "iterative self tail matches legacy (returns 0)");
    w89_eval_out_free(&ol);
    w89_eval_out_free(&oi);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

/* S2.4: a seed function whose frame is too small to hold a larger-locals
 * tail callee must still return the callee's result and must not exhaust a
 * deep loop (the seed frame is caller-owned; a bigger callee uses the charged
 * fallback once, then reuses a driver-owned level in place) (ITT-002). */
static void test_iter_tail_seed_oversized(void)
{
    w89_vt p[1];
    w89_vt r[1];
    w89_subtype sub;
    w89_typeenv env;
    w89_moduleinst m;
    w89_vt flocals[2];
    w89_instr abody[2];
    w89_instr bbody[10];
    w89_func af;
    w89_func bf;
    w89_funcinst *afi;
    w89_funcinst *bfi;
    w89_eval_out oi;
    w89_value arg;

    p[0] = vt_num(0x7F);
    r[0] = vt_num(0x7F);
    sub = sub_func(p, 1, r, 1);
    env = make_env(&sub, 1, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;

    flocals[0] = vt_num(0x7F);
    flocals[1] = vt_num(0x7F);

    abody[0] = in_idx(0x20, 0);
    abody[1] = in_idx(0x12, 1);
    af = mk_func(NULL, 0, abody, 2);
    afi = mk_finst(&m, 0, &af);
    add_func(&m, afi);

    bbody[0] = in_idx(0x20, 0);
    bbody[1] = in(0x45);
    bbody[2] = block_vt(0x04, 0x7F);
    bbody[3] = in_c32(0x41, 7);
    bbody[4] = in(0x05);
    bbody[5] = in_idx(0x20, 0);
    bbody[6] = in_c32(0x41, 1);
    bbody[7] = in(0x6B);
    bbody[8] = in_idx(0x12, 1);
    bbody[9] = in(0x0B);
    bf = mk_func(flocals, 2, bbody, 10);
    bfi = mk_finst(&m, 0, &bf);
    add_func(&m, bfi);

    arg = w89_value_num(1000000);
    oi = invoke_env(afi, &arg, 1, "1");
    expect(oi.status == W89_EVAL_OK && oi.nvs == 1
           && oi.vs[0].u.num == 7,
           "iterative oversized-seed tail returns callee result");
    w89_eval_out_free(&oi);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

/* S2.4: a wasm function that tail-calls an imported host function gives the
 * same result under the legacy default and under W89_ITER=1 (host inline). */
static void test_iter_tail_host(void)
{
    w89_vt p[1];
    w89_vt r[1];
    w89_subtype subs[2];
    w89_u32 gsizes[2];
    w89_typeenv env;
    w89_moduleinst m;
    w89_ft hft;
    w89_funcinst *hw;
    w89_instr wb[2];
    w89_func wf;
    w89_funcinst *wi;
    w89_eval_out ol;
    w89_eval_out oi;
    w89_value arg;

    p[0] = vt_num(0x7F);
    r[0] = vt_num(0x7F);
    subs[0] = sub_func(p, 1, r, 1);
    subs[1] = sub_func(p, 1, r, 1);
    gsizes[0] = 1;
    gsizes[1] = 1;
    env = make_env(subs, 2, gsizes, 2);
    memset(&m, 0, sizeof(m));
    m.types = &env;

    memset(&hft, 0, sizeof(hft));
    hft.params = p;
    hft.nparams = 1;
    hft.results = r;
    hft.nresults = 1;
    hw = mk_finst(&m, 0, NULL);
    hw->is_host = 1;
    hw->ft = &hft;
    hw->host = host_id;
    hw->inst = &m;
    add_func(&m, hw);

    wb[0] = in_idx(0x20, 0);
    wb[1] = in_idx(0x12, 0);
    wf = mk_func(NULL, 0, wb, 2);
    wi = mk_finst(&m, 1, &wf);
    add_func(&m, wi);

    arg = w89_value_num(5);
    ol = invoke_env(wi, &arg, 1, "0");
    oi = invoke_env(wi, &arg, 1, "1");
    expect(ol.status == W89_EVAL_OK && ol.nvs == 1
           && ol.vs[0].u.num == 5, "legacy wasm->host tail returns 5");
    expect(oi.status == ol.status && oi.nvs == ol.nvs
           && oi.vs[0].u.num == 5, "iterative wasm->host tail returns 5");
    w89_eval_out_free(&ol);
    w89_eval_out_free(&oi);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

/* S2.4: direct-driver return_call tail reuse on a self-recursive countdown.
 * Driving w89_eval_iter directly forces the driver's own 0x12 tail path (the
 * seed body has no callee that would fall back to legacy). */
static void test_iter_tail_direct(void)
{
    w89_vt p[1];
    w89_vt r[1];
    w89_subtype sub;
    w89_typeenv env;
    w89_moduleinst m;
    w89_instr body[10];
    w89_func f;
    w89_funcinst *fi;
    w89_frame fr;
    w89_config cfg;
    w89_eval_out out;
    w89_local l0;

    p[0] = vt_num(0x7F);
    r[0] = vt_num(0x7F);
    sub = sub_func(p, 1, r, 1);
    env = make_env(&sub, 1, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;

    body[0] = in_idx(0x20, 0);
    body[1] = in(0x45);
    body[2] = block_vt(0x04, 0x7F);
    body[3] = in_c32(0x41, 0);
    body[4] = in(0x05);
    body[5] = in_idx(0x20, 0);
    body[6] = in_c32(0x41, 1);
    body[7] = in(0x6B);
    body[8] = in_idx(0x12, 0);
    body[9] = in(0x0B);
    f = mk_func(NULL, 0, body, 10);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);

    memset(&l0, 0, sizeof(l0));
    l0.v = w89_value_num(2000);
    l0.set = 1;
    memset(&fr, 0, sizeof(fr));
    fr.inst = &m;
    fr.nlocals = 1;
    fr.locals = malloc(1 * sizeof(w89_local));
    fr.locals[0] = l0;

    w89_config_init(&cfg, &fr);
    w89_code_range(&cfg.code, body, 10, 0, 10);
    out = w89_eval_iter(&cfg);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 0,
           "direct-driver self tail countdown returns without exhaustion");
    w89_eval_out_free(&out);
    w89_config_free(&cfg);

    free(fr.locals);
    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

/* S2.4: direct-driver return_call_indirect tail reuse (the driver's indirect
 * path, exercised directly since conformance routes indirect bodies to the
 * legacy path, mirroring handover note #3). Self indirect tail countdown. */
static void test_iter_tail_indirect(void)
{
    w89_vt p[1];
    w89_vt r[1];
    w89_subtype sub;
    w89_typeenv env;
    w89_moduleinst m;
    w89_instr body[11];
    w89_func f;
    w89_funcinst *fi;
    w89_tableinst tab;
    w89_frame fr;
    w89_config cfg;
    w89_eval_out out;
    w89_local l0;

    p[0] = vt_num(0x7F);
    r[0] = vt_num(0x7F);
    sub = sub_func(p, 1, r, 1);
    env = make_env(&sub, 1, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;

    body[0] = in_idx(0x20, 0);
    body[1] = in(0x45);
    body[2] = block_vt(0x04, 0x7F);
    body[3] = in_c32(0x41, 0);
    body[4] = in(0x05);
    body[5] = in_idx(0x20, 0);
    body[6] = in_c32(0x41, 1);
    body[7] = in(0x6B);
    body[8] = in_c32(0x41, 0);
    body[9] = in_idx2(0x13, 0, 0);
    body[10] = in(0x0B);
    f = mk_func(NULL, 0, body, 11);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);

    memset(&tab, 0, sizeof(tab));
    tab.type.limits.min = 1;
    tab.type.rt.is_typeidx = 0;
    tab.type.rt.abs = W89_HT_FUNC;
    tab.type.rt.nullable = 1;
    tab.size = 1;
    tab.elems = malloc(1 * sizeof(w89_ref));
    tab.elems[0] = w89_ref_func(fi);
    add_table(&m, &tab);

    memset(&l0, 0, sizeof(l0));
    l0.v = w89_value_num(2000);
    l0.set = 1;
    memset(&fr, 0, sizeof(fr));
    fr.inst = &m;
    fr.nlocals = 1;
    fr.locals = malloc(1 * sizeof(w89_local));
    fr.locals[0] = l0;

    w89_config_init(&cfg, &fr);
    w89_code_range(&cfg.code, body, 11, 0, 11);
    out = w89_eval_iter(&cfg);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 0,
           "direct-driver indirect tail countdown returns without exhaustion");
    w89_eval_out_free(&out);
    w89_config_free(&cfg);

    free(fr.locals);
    free(tab.elems);
    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

/* S2.6: a `br` whose resolved target is the enclosing function body label
 * (a bare top-level `br 0`, or a `br N` reaching the function through
 * enclosing blocks) must act as that function's return, giving the same
 * result under the legacy default and under W89_ITER=1. The iterative
 * driver previously reported these as "undefined label" (ITD-002). */
static void test_iter_br_to_func_return(void)
{
    w89_vt r[1];
    w89_subtype sub;
    w89_typeenv env;
    w89_moduleinst m;
    w89_instr b1[2];
    w89_instr b2[9];
    w89_func f1;
    w89_func f2;
    w89_funcinst *fi1;
    w89_funcinst *fi2;
    w89_eval_out ol;
    w89_eval_out oi;

    r[0] = vt_num(0x7F);
    sub = sub_func(NULL, 0, r, 1);
    env = make_env(&sub, 1, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;

    /* (func (result i32) (br 0 (i32.const 79))) */
    b1[0] = in_c32(0x41, 79);
    b1[1] = in_idx(0x0C, 0);
    f1 = mk_func(NULL, 0, b1, 2);
    fi1 = mk_finst(&m, 0, &f1);
    add_func(&m, fi1);

    /* (func (result i32) (block (br 1 (i32.const 5))) (i32.const 9)) */
    b2[0] = block_vt(0x02, 0x40);
    b2[1] = in_c32(0x41, 5);
    b2[2] = in_idx(0x0C, 1);
    b2[3] = in(0x0B);
    b2[4] = in_c32(0x41, 9);
    f2 = mk_func(NULL, 0, b2, 5);
    fi2 = mk_finst(&m, 0, &f2);
    add_func(&m, fi2);

    ol = invoke_env(fi1, NULL, 0, "0");
    oi = invoke_env(fi1, NULL, 0, "1");
    expect(ol.status == W89_EVAL_OK && ol.nvs == 1
           && ol.vs[0].u.num == 79,
           "legacy top-level br 0 returns its operand");
    expect(oi.status == W89_EVAL_OK && oi.nvs == 1
           && oi.vs[0].u.num == 79,
           "iterative top-level br 0 returns its operand");
    w89_eval_out_free(&ol);
    w89_eval_out_free(&oi);

    ol = invoke_env(fi2, NULL, 0, "0");
    oi = invoke_env(fi2, NULL, 0, "1");
    expect(ol.status == W89_EVAL_OK && ol.nvs == 1
           && ol.vs[0].u.num == 5,
           "legacy br-to-function across a block returns 5");
    expect(oi.status == W89_EVAL_OK && oi.nvs == 1
           && oi.vs[0].u.num == 5,
           "iterative br-to-function across a block returns 5");
    w89_eval_out_free(&ol);
    w89_eval_out_free(&oi);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

/* S2.6: a `return` executed from inside an uncompleted value-typed block must
 * discard any stray value left below the results (e.g. the value operand of a
 * non-taken br_if) and return only the declared result (as-br_if-last shape).
 * Legacy and W89_ITER=1 must agree (ITD-002). */
static void test_iter_return_discards_stray(void)
{
    w89_vt p[1];
    w89_vt r[1];
    w89_subtype sub;
    w89_typeenv env;
    w89_moduleinst m;
    w89_instr body[9];
    w89_func f;
    w89_funcinst *fi;
    w89_eval_out ol;
    w89_eval_out oi;
    w89_value arg;

    p[0] = vt_num(0x7F);
    r[0] = vt_num(0x7F);
    sub = sub_func(p, 1, r, 1);
    env = make_env(&sub, 1, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;

    /* (func (param i32) (result i32)
     *   (block (result i32)
     *     (i32.const 2) (local.get 0) (br_if 0)
     *     (return (i32.const 3)))) */
    body[0] = block_vt(0x02, 0x7F);
    body[1] = in_c32(0x41, 2);
    body[2] = in_idx(0x20, 0);
    body[3] = in_idx(0x0D, 0);
    body[4] = in_c32(0x41, 3);
    body[5] = in(0x0F);
    body[6] = in(0x0B);
    f = mk_func(NULL, 0, body, 7);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);

    arg = w89_value_num(0);
    ol = invoke_env(fi, &arg, 1, "0");
    oi = invoke_env(fi, &arg, 1, "1");
    expect(ol.status == W89_EVAL_OK && ol.nvs == 1
           && ol.vs[0].u.num == 3,
           "legacy return drops stray value and returns 3");
    expect(oi.status == W89_EVAL_OK && oi.nvs == 1
           && oi.vs[0].u.num == 3,
           "iterative return drops stray value and returns 3");
    w89_eval_out_free(&ol);
    w89_eval_out_free(&oi);

    arg = w89_value_num(1);
    ol = invoke_env(fi, &arg, 1, "0");
    oi = invoke_env(fi, &arg, 1, "1");
    expect(ol.status == W89_EVAL_OK && ol.nvs == 1
           && ol.vs[0].u.num == 2,
           "legacy taken br_if yields block value 2");
    expect(oi.status == W89_EVAL_OK && oi.nvs == 1
           && oi.vs[0].u.num == 2,
           "iterative taken br_if yields block value 2");
    w89_eval_out_free(&ol);
    w89_eval_out_free(&oi);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

/* S2.6: a genuinely unbounded recursion reached through `call_indirect` must
 * exhaust the budget under W89_ITER=1 with a flat C stack (formerly the body
 * routed to legacy and the runaway STALLed). Bodies containing call_indirect
 * are now driver-eligible (ITD-002). */
static void test_iter_call_indirect_exhaustion(void)
{
    w89_vt p[1];
    w89_vt r[1];
    w89_subtype sub;
    w89_typeenv env;
    w89_moduleinst m;
    w89_instr body[3];
    w89_func f;
    w89_funcinst *fi;
    w89_tableinst tab;
    w89_eval_out oi;
    w89_value arg;

    p[0] = vt_num(0x7F);
    r[0] = vt_num(0x7F);
    sub = sub_func(p, 1, r, 1);
    env = make_env(&sub, 1, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;

    /* (func (param i32) (result i32) (local.get 0) (i32.const 0)
     *   (call_indirect (type 0) (table 0))) -- runaway self recursion */
    body[0] = in_idx(0x20, 0);
    body[1] = in_c32(0x41, 0);
    body[2] = in_idx2(0x11, 0, 0);
    f = mk_func(NULL, 0, body, 3);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);

    memset(&tab, 0, sizeof(tab));
    tab.type.limits.min = 1;
    tab.type.rt.is_typeidx = 0;
    tab.type.rt.abs = W89_HT_FUNC;
    tab.type.rt.nullable = 1;
    tab.size = 1;
    tab.elems = malloc(1 * sizeof(w89_ref));
    tab.elems[0] = w89_ref_func(fi);
    add_table(&m, &tab);

    arg = w89_value_num(0);
    oi = invoke_env(fi, &arg, 1, "1");
    expect(oi.status == W89_EVAL_EXHAUSTED && oi.msg
           && strcmp(oi.msg, "call stack exhausted") == 0,
           "iterative call_indirect runaway exhausts at budget");
    w89_eval_out_free(&oi);

    free(tab.elems);
    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

int main(void)
{
    test_basic_call();
    test_void_call();
    test_local_default();
    test_recursion();
    test_iter_br_to_func_return();
    test_iter_return_discards_stray();
    test_iter_call_indirect_exhaustion();
    test_iter_direct_call_parity();
    test_iter_down_parity();
    test_iter_exhaustion();
    test_iter_host_call();
    test_iter_tail_self_loop();
    test_iter_tail_self_parity();
    test_iter_tail_seed_oversized();
    test_iter_tail_host();
    test_iter_tail_direct();
    test_iter_tail_indirect();
    test_call_indirect();
    test_call_ref();
    test_tail_call();
    test_exhaustion();
    test_host_func();
    test_wrong_args();
    test_mem_init_poporder();
    test_mem_init_indexswap();
    test_table_init_indexswap();
    test_mem_load_store();
    test_mem_copy_fill();
    test_table_ops();
    test_ref_ops();
    if (failures == 0) {
        printf("test_calls: all tests passed\n");
        return 0;
    }
    fprintf(stderr, "test_calls: %d failure(s)\n", failures);
    return 1;
}
