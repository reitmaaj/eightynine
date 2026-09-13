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

static w89_vt exnref_t(void)
{
    w89_vt v;
    memset(&v, 0, sizeof(v));
    v.is_ref = 1;
    v.rt.nullable = 0;
    v.rt.is_typeidx = 0;
    v.rt.abs = W89_HT_EXN;
    return v;
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

static w89_instr in_idx(w89_byte op, w89_u32 idx)
{
    w89_instr i;
    memset(&i, 0, sizeof(i));
    i.op = op;
    i.idx = idx;
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

static w89_instr block_tidx(w89_byte op, w89_u32 typeidx)
{
    w89_instr i;
    memset(&i, 0, sizeof(i));
    i.op = op;
    i.bt.is_typeidx = 1;
    i.bt.typeidx = typeidx;
    return i;
}

static w89_catch cc(w89_byte kind, w89_u32 tagidx, w89_u32 label)
{
    w89_catch c;
    memset(&c, 0, sizeof(c));
    c.kind = kind;
    c.tagidx = tagidx;
    c.label = label;
    return c;
}

static w89_instr try_catch(const w89_vt *res, w89_u32 n,
                           const w89_catch *catches)
{
    w89_instr i;
    memset(&i, 0, sizeof(i));
    i.op = 0x1F;
    i.bt.is_typeidx = 0;
    i.bt.vt = *res;
    i.n = n;
    i.catches = (w89_catch *)catches;
    return i;
}

static w89_instr try_i32(w89_u32 n, const w89_catch *catches)
{
    w89_vt r = vt_num(0x7F);
    return try_catch(&r, n, catches);
}

static w89_instr try_empty(w89_u32 n, const w89_catch *catches)
{
    w89_vt r = vt_num(0);
    return try_catch(&r, n, catches);
}

static w89_instr try_exnref(w89_u32 n, const w89_catch *catches)
{
    w89_vt r = exnref_t();
    return try_catch(&r, n, catches);
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

static w89_taginst *mk_taginst(const w89_ft *ft)
{
    w89_taginst *t = malloc(sizeof(w89_taginst));
    memset(t, 0, sizeof(*t));
    t->ft = ft;
    return t;
}

static void add_func(w89_moduleinst *inst, w89_funcinst *f)
{
    inst->funcs = realloc(inst->funcs,
                          (inst->nfuncs + 1) * sizeof(w89_funcinst *));
    inst->funcs[inst->nfuncs++] = f;
}

static void add_tag(w89_moduleinst *inst, w89_taginst *t)
{
    inst->tags = realloc(inst->tags,
                         (inst->ntags + 1) * sizeof(w89_taginst *));
    inst->tags[inst->ntags++] = t;
}

static void run_invoke(w89_funcinst *f, const w89_value *args, w89_u32 n,
                       w89_eval_out *out)
{
    *out = w89_invoke(NULL, f, args, n);
}

static void free_module_arrays(w89_moduleinst *m)
{
    w89_u32 i;
    for (i = 0; i < m->nfuncs; i++) {
        free(m->funcs[i]);
    }
    for (i = 0; i < m->ntags; i++) {
        free(m->tags[i]);
    }
    free(m->funcs);
    free(m->tables);
    free(m->memories);
    free(m->datas);
    free(m->elems);
    free(m->tags);
}

static void test_throw_uncaught(void)
{
    w89_subtype subs[2];
    w89_typeenv env;
    w89_moduleinst m;
    w89_instr body[1];
    w89_func f;
    w89_funcinst *fi;
    w89_taginst *t0;
    w89_eval_out out;

    subs[0] = sub_func(NULL, 0, NULL, 0);
    subs[1] = sub_func(NULL, 0, NULL, 0);
    env = make_env(subs, 2, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;
    t0 = mk_taginst(&env.types[1].sub->ft);
    add_tag(&m, t0);

    body[0] = in_idx(0x08, 0);
    f = mk_func(NULL, 0, body, 1);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);

    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_EXCEPTION && out.tag == t0
           && out.nvs == 0,
           "throw uncaught: exception status with tag");
    w89_eval_out_free(&out);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

static void test_throw_payload(void)
{
    w89_vt p[2];
    w89_subtype subs[2];
    w89_typeenv env;
    w89_moduleinst m;
    w89_instr body[3];
    w89_func f;
    w89_funcinst *fi;
    w89_taginst *t0;
    w89_eval_out out;

    p[0] = vt_num(0x7F);
    p[1] = vt_num(0x7F);
    subs[0] = sub_func(NULL, 0, NULL, 0);
    subs[1] = sub_func(p, 2, NULL, 0);
    env = make_env(subs, 2, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;
    t0 = mk_taginst(&env.types[1].sub->ft);
    add_tag(&m, t0);

    body[0] = in_c32(0x41, 1);
    body[1] = in_c32(0x41, 2);
    body[2] = in_idx(0x08, 0);
    f = mk_func(NULL, 0, body, 3);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);

    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_EXCEPTION && out.tag == t0
           && out.nvs == 2 && out.vs[0].u.num == 1 && out.vs[1].u.num == 2,
           "throw payload carried in order");
    w89_eval_out_free(&out);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

static void test_try_catch(void)
{
    w89_vt p[1];
    w89_vt r[1];
    w89_subtype subs[2];
    w89_typeenv env;
    w89_moduleinst m;
    w89_instr body[13];
    w89_catch cs[1];
    w89_func f;
    w89_funcinst *fi;
    w89_taginst *t0;
    w89_value arg;
    w89_eval_out out;

    p[0] = vt_num(0x7F);
    r[0] = vt_num(0x7F);
    subs[0] = sub_func(p, 1, r, 1);
    subs[1] = sub_func(NULL, 0, NULL, 0);
    env = make_env(subs, 2, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;
    t0 = mk_taginst(&env.types[1].sub->ft);
    add_tag(&m, t0);

    cs[0] = cc(0x00, 0, 0);
    body[0] = block_vt(0x02, 0);
    body[1] = try_i32(1, cs);
    body[2] = in_idx(0x20, 0);
    body[3] = in(0x45);
    body[4] = block_vt(0x04, 0);
    body[5] = in_idx(0x08, 0);
    body[6] = in(0x05);
    body[7] = in(0x0B);
    body[8] = in_c32(0x41, 42);
    body[9] = in(0x0B);
    body[10] = in(0x0F);
    body[11] = in(0x0B);
    body[12] = in_c32(0x41, 23);
    f = mk_func(NULL, 0, body, 13);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);

    arg = w89_value_num(0);
    run_invoke(fi, &arg, 1, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 23, "catch routes past return to const 23");
    w89_eval_out_free(&out);

    arg = w89_value_num(1);
    run_invoke(fi, &arg, 1, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 42, "no throw: try_table result 42");
    w89_eval_out_free(&out);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

static void test_try_catch_payload(void)
{
    w89_vt p[1];
    w89_vt r[1];
    w89_subtype subs[2];
    w89_typeenv env;
    w89_moduleinst m;
    w89_instr body[9];
    w89_catch cs[1];
    w89_func f;
    w89_funcinst *fi;
    w89_taginst *t0;
    w89_value arg;
    w89_eval_out out;

    p[0] = vt_num(0x7F);
    r[0] = vt_num(0x7F);
    subs[0] = sub_func(p, 1, r, 1);
    subs[1] = sub_func(p, 1, NULL, 0);
    env = make_env(subs, 2, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;
    t0 = mk_taginst(&env.types[1].sub->ft);
    add_tag(&m, t0);

    cs[0] = cc(0x00, 0, 0);
    body[0] = block_vt(0x02, 0x7F);
    body[1] = try_i32(1, cs);
    body[2] = in_idx(0x20, 0);
    body[3] = in_idx(0x08, 0);
    body[4] = in_c32(0x41, 2);
    body[5] = in(0x0B);
    body[6] = in(0x0F);
    body[7] = in(0x0B);
    body[8] = in(0x0F);
    f = mk_func(NULL, 0, body, 9);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);

    arg = w89_value_num(5);
    run_invoke(fi, &arg, 1, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 5, "catch payload feeds block result");
    w89_eval_out_free(&out);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

static void test_try_catch_mismatch(void)
{
    w89_subtype subs[2];
    w89_typeenv env;
    w89_moduleinst m;
    w89_instr body[3];
    w89_catch cs[1];
    w89_func f;
    w89_funcinst *fi;
    w89_taginst *t0;
    w89_taginst *t1;
    w89_eval_out out;

    subs[0] = sub_func(NULL, 0, NULL, 0);
    subs[1] = sub_func(NULL, 0, NULL, 0);
    env = make_env(subs, 2, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;
    t0 = mk_taginst(&env.types[1].sub->ft);
    t1 = mk_taginst(&env.types[1].sub->ft);
    add_tag(&m, t0);
    add_tag(&m, t1);

    cs[0] = cc(0x00, 0, 0);
    body[0] = try_empty(1, cs);
    body[1] = in_idx(0x08, 1);
    body[2] = in(0x0B);
    f = mk_func(NULL, 0, body, 3);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);

    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_EXCEPTION && out.tag == t1,
           "mismatched catch does not fire; exception propagates");
    w89_eval_out_free(&out);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

static void test_try_catch_all(void)
{
    w89_subtype subs[2];
    w89_typeenv env;
    w89_moduleinst m;
    w89_instr body[8];
    w89_catch cs[1];
    w89_func f;
    w89_funcinst *fi;
    w89_taginst *t0;
    w89_eval_out out;

    subs[0] = sub_func(NULL, 0, NULL, 0);
    subs[1] = sub_func(NULL, 0, NULL, 0);
    env = make_env(subs, 2, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;
    t0 = mk_taginst(&env.types[1].sub->ft);
    add_tag(&m, t0);

    cs[0] = cc(0x02, 0, 0);
    body[0] = block_vt(0x02, 0);
    body[1] = try_i32(1, cs);
    body[2] = in_idx(0x08, 0);
    body[3] = in_c32(0x41, 42);
    body[4] = in(0x0B);
    body[5] = in(0x0F);
    body[6] = in(0x0B);
    body[7] = in_c32(0x41, 23);
    f = mk_func(NULL, 0, body, 8);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);

    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 23, "catch_all branches with no payload");
    w89_eval_out_free(&out);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

static void test_try_catch_ref(void)
{
    w89_vt p[1];
    w89_vt r[1];
    w89_vt bre[2];
    w89_subtype subs[3];
    w89_typeenv env;
    w89_moduleinst m;
    w89_store store;
    w89_instr body[10];
    w89_catch cs[1];
    w89_func f;
    w89_funcinst *fi;
    w89_taginst *t0;
    w89_value arg;
    w89_eval_out out;

    p[0] = vt_num(0x7F);
    r[0] = vt_num(0x7F);
    bre[0] = vt_num(0x7F);
    bre[1] = exnref_t();
    subs[0] = sub_func(p, 1, r, 1);
    subs[1] = sub_func(p, 1, NULL, 0);
    subs[2] = sub_func(NULL, 0, bre, 2);
    env = make_env(subs, 3, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;
    w89_store_init(&store);
    m.store = &store;
    t0 = mk_taginst(&env.types[1].sub->ft);
    add_tag(&m, t0);

    cs[0] = cc(0x01, 0, 0);
    body[0] = block_tidx(0x02, 2);
    body[1] = try_i32(1, cs);
    body[2] = in_idx(0x20, 0);
    body[3] = in_idx(0x08, 0);
    body[4] = in_c32(0x41, 2);
    body[5] = in(0x0B);
    body[6] = in(0x0F);
    body[7] = in(0x0B);
    body[8] = in(0x1A);
    body[9] = in(0x0F);
    f = mk_func(NULL, 0, body, 10);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);

    arg = w89_value_num(5);
    run_invoke(fi, &arg, 1, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 5 && store.nexns == 1,
           "catch_ref pushes exnref above payload; store owns one exn");
    w89_eval_out_free(&out);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
    w89_store_free(&store);
}

static void test_try_catch_all_ref(void)
{
    w89_vt r[1];
    w89_subtype subs[3];
    w89_typeenv env;
    w89_moduleinst m;
    w89_store store;
    w89_instr body[6];
    w89_catch cs[1];
    w89_func f;
    w89_funcinst *fi;
    w89_taginst *t0;
    w89_eval_out out;

    r[0] = exnref_t();
    subs[0] = sub_func(NULL, 0, NULL, 0);
    subs[1] = sub_func(NULL, 0, NULL, 0);
    subs[2] = sub_func(NULL, 0, r, 1);
    env = make_env(subs, 3, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;
    w89_store_init(&store);
    m.store = &store;
    t0 = mk_taginst(&env.types[1].sub->ft);
    add_tag(&m, t0);

    cs[0] = cc(0x03, 0, 0);
    body[0] = block_tidx(0x02, 2);
    body[1] = try_exnref(1, cs);
    body[2] = in_idx(0x08, 0);
    body[3] = in(0x0B);
    body[4] = in(0x0B);
    body[5] = in(0x0A);
    f = mk_func(NULL, 0, body, 6);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);

    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_EXCEPTION && out.tag == t0 && out.nvs == 0,
           "catch_all_ref exnref rethrown via throw_ref");
    w89_eval_out_free(&out);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
    w89_store_free(&store);
}

static void test_throw_ref_null(void)
{
    w89_subtype subs[1];
    w89_typeenv env;
    w89_moduleinst m;
    w89_instr body[2];
    w89_func f;
    w89_funcinst *fi;
    w89_eval_out out;

    subs[0] = sub_func(NULL, 0, NULL, 0);
    env = make_env(subs, 1, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;

    body[0] = in(0xD0);
    body[1] = in(0x0A);
    f = mk_func(NULL, 0, body, 2);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);

    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_TRAP && out.msg
           && strcmp(out.msg, "null exception reference") == 0,
           "throw_ref on null traps");
    w89_eval_out_free(&out);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

static void test_try_trap_propagation(void)
{
    w89_subtype subs[2];
    w89_typeenv env;
    w89_moduleinst m;
    w89_instr body[3];
    w89_catch cs[1];
    w89_func f;
    w89_funcinst *fi;
    w89_taginst *t0;
    w89_eval_out out;

    subs[0] = sub_func(NULL, 0, NULL, 0);
    subs[1] = sub_func(NULL, 0, NULL, 0);
    env = make_env(subs, 2, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;
    t0 = mk_taginst(&env.types[1].sub->ft);
    add_tag(&m, t0);

    cs[0] = cc(0x00, 0, 0);
    body[0] = try_empty(1, cs);
    body[1] = in(0x00);
    body[2] = in(0x0B);
    f = mk_func(NULL, 0, body, 3);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);

    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_TRAP && out.msg
           && strcmp(out.msg, "unreachable executed") == 0,
           "trap propagates through handler");
    w89_eval_out_free(&out);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

static void test_throw_in_callee(void)
{
    w89_vt r[1];
    w89_subtype subs[3];
    w89_typeenv env;
    w89_moduleinst m;
    w89_instr caller_body[8];
    w89_instr callee_body[1];
    w89_catch cs[1];
    w89_func fc;
    w89_func fn;
    w89_funcinst *fci;
    w89_funcinst *fni;
    w89_taginst *t0;
    w89_eval_out out;

    r[0] = vt_num(0x7F);
    subs[0] = sub_func(NULL, 0, r, 1);
    subs[1] = sub_func(NULL, 0, NULL, 0);
    subs[2] = sub_func(NULL, 0, NULL, 0);
    env = make_env(subs, 3, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;
    t0 = mk_taginst(&env.types[2].sub->ft);
    add_tag(&m, t0);

    cs[0] = cc(0x02, 0, 0);
    caller_body[0] = block_vt(0x02, 0);
    caller_body[1] = try_i32(1, cs);
    caller_body[2] = in_idx(0x10, 1);
    caller_body[3] = in_c32(0x41, 0);
    caller_body[4] = in(0x0B);
    caller_body[5] = in(0x0F);
    caller_body[6] = in(0x0B);
    caller_body[7] = in_c32(0x41, 23);
    fc = mk_func(NULL, 0, caller_body, 8);
    fci = mk_finst(&m, 0, &fc);
    add_func(&m, fci);

    callee_body[0] = in_idx(0x08, 0);
    fn = mk_func(NULL, 0, callee_body, 1);
    fni = mk_finst(&m, 1, &fn);
    add_func(&m, fni);

    run_invoke(fci, NULL, 0, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 23,
           "exception propagates out of callee into caller's catch_all");
    w89_eval_out_free(&out);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

static void test_try_br_target(void)
{
    w89_vt r[1];
    w89_subtype subs[1];
    w89_typeenv env;
    w89_moduleinst m;
    w89_instr body[8];
    w89_func f;
    w89_funcinst *fi;
    w89_eval_out out;

    r[0] = vt_num(0x7F);
    subs[0] = sub_func(NULL, 0, r, 1);
    env = make_env(subs, 1, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;

    body[0] = block_vt(0x02, 0x7F);
    body[1] = try_i32(0, NULL);
    body[2] = in_c32(0x41, 0x15);
    body[3] = in_idx(0x0C, 0);
    body[4] = in(0x0B);
    body[5] = in(0x0F);
    body[6] = in(0x0B);
    body[7] = in_c32(0x41, 0x0DE);
    f = mk_func(NULL, 0, body, 8);
    fi = mk_finst(&m, 0, &f);
    add_func(&m, fi);

    run_invoke(fi, NULL, 0, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 0x15, "br 0 exits try_table body");
    w89_eval_out_free(&out);

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

/* Run the same cross-function try/catch program once on the legacy default and
 * once forcing the iterative driver (W89_ITER=1); both must produce the same
 * caught result. A callee throws inside the caller's catch_all, so the driver
 * path must unwind the callee frame to the caller's try_table. */
static void test_iter_catch_parity(void)
{
    w89_vt r[1];
    w89_subtype subs[3];
    w89_typeenv env;
    w89_moduleinst m;
    w89_instr caller_body[8];
    w89_instr callee_body[1];
    w89_catch cs[1];
    w89_func fc;
    w89_func fn;
    w89_funcinst *fci;
    w89_funcinst *fni;
    w89_taginst *t0;
    w89_eval_out o0;
    w89_eval_out o1;

    r[0] = vt_num(0x7F);
    subs[0] = sub_func(NULL, 0, r, 1);
    subs[1] = sub_func(NULL, 0, NULL, 0);
    subs[2] = sub_func(NULL, 0, NULL, 0);
    env = make_env(subs, 3, 0, 0);
    memset(&m, 0, sizeof(m));
    m.types = &env;
    t0 = mk_taginst(&env.types[2].sub->ft);
    add_tag(&m, t0);

    cs[0] = cc(0x02, 0, 0);
    caller_body[0] = block_vt(0x02, 0);
    caller_body[1] = try_i32(1, cs);
    caller_body[2] = in_idx(0x10, 1);
    caller_body[3] = in_c32(0x41, 0);
    caller_body[4] = in(0x0B);
    caller_body[5] = in(0x0F);
    caller_body[6] = in(0x0B);
    caller_body[7] = in_c32(0x41, 23);
    fc = mk_func(NULL, 0, caller_body, 8);
    fci = mk_finst(&m, 0, &fc);
    add_func(&m, fci);

    callee_body[0] = in_idx(0x08, 0);
    fn = mk_func(NULL, 0, callee_body, 1);
    fni = mk_finst(&m, 1, &fn);
    add_func(&m, fni);

    unsetenv("W89_ITER");
    run_invoke(fci, NULL, 0, &o0);
    setenv("W89_ITER", "1", 1);
    run_invoke(fci, NULL, 0, &o1);
    expect(o0.status == W89_EVAL_OK && o0.nvs == 1
           && o0.vs[0].u.num == 23,
           "legacy cross-function catch result 23");
    expect(o1.status == W89_EVAL_OK && o1.nvs == 1
           && o1.vs[0].u.num == 23,
           "driver cross-function catch result 23");
    w89_eval_out_free(&o0);
    w89_eval_out_free(&o1);
    unsetenv("W89_ITER");

    free_module_arrays(&m);
    free(env.types);
    free(env.recs);
}

int main(void)
{
    test_throw_uncaught();
    test_throw_payload();
    test_try_catch();
    test_try_catch_payload();
    test_try_catch_mismatch();
    test_try_catch_all();
    test_try_catch_ref();
    test_try_catch_all_ref();
    test_throw_ref_null();
    test_try_trap_propagation();
    test_throw_in_callee();
    test_try_br_target();
    test_iter_catch_parity();
    if (failures != 0) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("all exception tests passed\n");
    return 0;
}
