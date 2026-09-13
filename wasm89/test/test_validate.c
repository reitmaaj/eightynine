#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "validate.h"

static int failures;

static void expect(int cond, const char *name)
{
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    }
}

static w89_vt num_vt(w89_u32 num)
{
    w89_vt v;
    memset(&v, 0, sizeof(v));
    v.is_ref = 0;
    v.num = num;
    return v;
}

static w89_vt ref_vt(w89_u32 nullable, w89_u32 is_typeidx, w89_u32 typeidx,
                     w89_absheaptype abs)
{
    w89_vt v;
    memset(&v, 0, sizeof(v));
    v.is_ref = 1;
    v.num = nullable ? 0x63 : 0x64;
    v.rt.nullable = nullable;
    v.rt.is_typeidx = is_typeidx;
    v.rt.typeidx = typeidx;
    v.rt.abs = abs;
    return v;
}

static w89_typeenv make_env(const w89_subtype *subs, w89_u32 nsubs,
                            const w89_u32 *gsizes, w89_u32 ngroups)
{
    w89_typeenv env;
    w89_u32 idx, i, k;
    memset(&env, 0, sizeof(env));
    env.nrecs = ngroups;
    env.recs = malloc(ngroups * sizeof(w89_recgroup));
    env.ntypes = nsubs;
    env.types = malloc(nsubs * sizeof(w89_deftype));
    idx = 0;
    for (i = 0; i < ngroups; i++) {
        env.recs[i].first = idx;
        env.recs[i].nsubs = gsizes[i];
        for (k = 0; k < gsizes[i]; k++) {
            env.types[idx].sub = &subs[idx];
            env.types[idx].recgroup = i;
            env.types[idx].recpos = k;
            idx++;
        }
    }
    return env;
}

static void test_canon_identical(void)
{
    static const w89_vt p32[] = { { 0, 0x7F, { 0, 0, 0, 0 } } };
    static const w89_vt p64[] = { { 0, 0x7E, { 0, 0, 0, 0 } } };
    static const w89_vt r0[] = { { 0, 0x7F, { 0, 0, 0, 0 } } };
    static const w89_subtype a0 = { 1, NULL, 0, W89_CK_FUNC,
        { NULL, 0, (w89_vt *)r0, 1 }, NULL, 0 };
    static const w89_subtype a1 = { 1, NULL, 0, W89_CK_FUNC,
        { (w89_vt *)p32, 1, NULL, 0 }, NULL, 0 };
    static const w89_subtype b0 = { 1, NULL, 0, W89_CK_FUNC,
        { NULL, 0, (w89_vt *)r0, 1 }, NULL, 0 };
    static const w89_subtype b1 = { 1, NULL, 0, W89_CK_FUNC,
        { (w89_vt *)p32, 1, NULL, 0 }, NULL, 0 };
    static const w89_subtype c1 = { 1, NULL, 0, W89_CK_FUNC,
        { (w89_vt *)p64, 1, NULL, 0 }, NULL, 0 };
    static const w89_subtype subs[] = { a0, a1, b0, b1, c1 };
    static const w89_u32 gs[] = { 2, 2, 1 };
    w89_typeenv env = make_env(subs, 5, gs, 3);

    expect(env.ntypes == 5, "canon: ntypes");
    expect(env.recs[0].first == 0 && env.recs[0].nsubs == 2, "canon: group0");
    expect(env.recs[1].first == 2 && env.recs[1].nsubs == 2, "canon: group1");
    expect(env.recs[2].first == 4 && env.recs[2].nsubs == 1, "canon: group2");
    expect(w89_type_canon_eq(&env, 0, 2), "canon: a0=b0");
    expect(w89_type_canon_eq(&env, 1, 3), "canon: a1=b1");
    expect(!w89_type_canon_eq(&env, 0, 3), "canon: a0!=b1");
    expect(!w89_type_canon_eq(&env, 0, 4), "canon: a0!=c1");
    w89_typeenv_free(&env);
}

static void test_canon_recursive(void)
{
    static const w89_fieldtype fa0f[] = {
        { 0, W89_PK_I8, { 1, 0x63, { 1, 1, 1, W89_HT_FUNC } }, 0 } };
    static const w89_fieldtype fa1f[] = {
        { 0, W89_PK_I8, { 1, 0x63, { 1, 1, 0, W89_HT_FUNC } }, 0 } };
    static const w89_fieldtype fb0f[] = {
        { 0, W89_PK_I8, { 1, 0x63, { 1, 1, 3, W89_HT_FUNC } }, 0 } };
    static const w89_fieldtype fb1f[] = {
        { 0, W89_PK_I8, { 1, 0x63, { 1, 1, 2, W89_HT_FUNC } }, 0 } };
    static const w89_fieldtype fc0f[] = {
        { 0, W89_PK_I8, { 1, 0x63, { 1, 1, 4, W89_HT_FUNC } }, 0 } };
    static const w89_fieldtype fc1f[] = {
        { 0, W89_PK_I8, { 1, 0x63, { 1, 1, 5, W89_HT_FUNC } }, 0 } };
    static const w89_subtype a0 = { 1, NULL, 0, W89_CK_STRUCT, { NULL, 0, NULL, 0 },
        (w89_fieldtype *)fa0f, 1 };
    static const w89_subtype a1 = { 1, NULL, 0, W89_CK_STRUCT, { NULL, 0, NULL, 0 },
        (w89_fieldtype *)fa1f, 1 };
    static const w89_subtype b0 = { 1, NULL, 0, W89_CK_STRUCT, { NULL, 0, NULL, 0 },
        (w89_fieldtype *)fb0f, 1 };
    static const w89_subtype b1 = { 1, NULL, 0, W89_CK_STRUCT, { NULL, 0, NULL, 0 },
        (w89_fieldtype *)fb1f, 1 };
    static const w89_subtype c0 = { 1, NULL, 0, W89_CK_STRUCT, { NULL, 0, NULL, 0 },
        (w89_fieldtype *)fc0f, 1 };
    static const w89_subtype c1 = { 1, NULL, 0, W89_CK_STRUCT, { NULL, 0, NULL, 0 },
        (w89_fieldtype *)fc1f, 1 };
    static const w89_subtype subs[] = { a0, a1, b0, b1, c0, c1 };
    static const w89_u32 gs[] = { 2, 2, 2 };
    w89_typeenv env = make_env(subs, 6, gs, 3);

    expect(w89_type_canon_eq(&env, 0, 2), "rec: a0=b0 (mutual)");
    expect(w89_type_canon_eq(&env, 1, 3), "rec: a1=b1 (mutual)");
    expect(!w89_type_canon_eq(&env, 0, 4), "rec: a0!=c0 (self)");
    expect(!w89_type_canon_eq(&env, 0, 5), "rec: a0!=c1");
    w89_typeenv_free(&env);
}

static void test_canon_final_supers(void)
{
    static const w89_vt r0[] = { { 0, 0x7F, { 0, 0, 0, 0 } } };
    static const w89_subtype a0 = { 1, NULL, 0, W89_CK_FUNC,
        { NULL, 0, (w89_vt *)r0, 1 }, NULL, 0 };
    static const w89_subtype b0 = { 0, NULL, 0, W89_CK_FUNC,
        { NULL, 0, (w89_vt *)r0, 1 }, NULL, 0 };
    static const w89_u32 x0s[] = { 2 };
    static const w89_subtype x0 = { 0, NULL, 0, W89_CK_FUNC,
        { NULL, 0, NULL, 0 }, NULL, 0 };
    static const w89_subtype x1 = { 0, (w89_u32 *)x0s, 1, W89_CK_FUNC,
        { NULL, 0, NULL, 0 }, NULL, 0 };
    static const w89_u32 y0s[] = { 4 };
    static const w89_subtype y0 = { 0, NULL, 0, W89_CK_FUNC,
        { NULL, 0, NULL, 0 }, NULL, 0 };
    static const w89_subtype y1 = { 0, (w89_u32 *)y0s, 1, W89_CK_FUNC,
        { NULL, 0, NULL, 0 }, NULL, 0 };
    static const w89_subtype z1 = { 0, NULL, 0, W89_CK_FUNC,
        { NULL, 0, NULL, 0 }, NULL, 0 };
    static const w89_subtype subs[] = { a0, b0, x0, x1, y0, y1, z1 };
    static const w89_u32 gs[] = { 1, 1, 2, 2, 1 };
    w89_typeenv env = make_env(subs, 7, gs, 5);

    expect(!w89_type_canon_eq(&env, 0, 1), "final: final!=nonfinal");
    expect(w89_type_canon_eq(&env, 3, 5), "supers: x1=y1");
    expect(!w89_type_canon_eq(&env, 3, 6), "supers: x1!=z1 (arity)");
    w89_typeenv_free(&env);
}

static void test_match_deftype(void)
{
    static const w89_fieldtype s0f[] = {
        { 0, W89_PK_I8, { 0, 0x7F, { 0, 0, 0, 0 } }, 0 },
        { 0, W89_PK_I8, { 0, 0x7E, { 0, 0, 0, 0 } }, 0 } };
    static const w89_fieldtype t0f[] = {
        { 0, W89_PK_I8, { 0, 0x7F, { 0, 0, 0, 0 } }, 0 } };
    static const w89_u32 t0sup[] = { 0 };
    static const w89_subtype s0 = { 0, NULL, 0, W89_CK_STRUCT, { NULL, 0, NULL, 0 },
        (w89_fieldtype *)s0f, 2 };
    static const w89_subtype t0 = { 0, (w89_u32 *)t0sup, 1, W89_CK_STRUCT,
        { NULL, 0, NULL, 0 }, (w89_fieldtype *)t0f, 1 };
    static const w89_subtype subs[] = { s0, t0 };
    static const w89_u32 gs[] = { 1, 1 };
    w89_typeenv env = make_env(subs, 2, gs, 2);

    expect(w89_match_deftype(&env, 1, 0), "match: t0 <: s0 via supertype");
    expect(!w89_match_deftype(&env, 0, 1), "match: s0 </: t0");
    expect(w89_match_deftype(&env, 0, 0), "match: s0 <: s0 identity");
    expect(w89_match_deftype(&env, 1, 1), "match: t0 <: t0 identity");
    w89_typeenv_free(&env);
}

static void test_match_valtype(void)
{
    static const w89_fieldtype s0f[] = {
        { 0, W89_PK_I8, { 0, 0x7F, { 0, 0, 0, 0 } }, 0 } };
    static const w89_subtype s0 = { 0, NULL, 0, W89_CK_STRUCT, { NULL, 0, NULL, 0 },
        (w89_fieldtype *)s0f, 1 };
    static const w89_subtype subs[] = { s0 };
    static const w89_u32 gs[] = { 1 };
    w89_typeenv env = make_env(subs, 1, gs, 1);
    w89_vt ref_null_func = ref_vt(1, 0, 0, W89_HT_FUNC);
    w89_vt ref_func = ref_vt(0, 0, 0, W89_HT_FUNC);
    w89_vt ref_null_s0 = ref_vt(1, 1, 0, W89_HT_FUNC);
    w89_vt ref_s0 = ref_vt(0, 1, 0, W89_HT_FUNC);
    w89_vt ref_null_struct = ref_vt(1, 0, 0, W89_HT_STRUCT);
    w89_vt ref_null_eq = ref_vt(1, 0, 0, W89_HT_EQ);
    w89_vt ref_null_func_t = ref_vt(1, 0, 0, W89_HT_FUNC);
    w89_vt i32 = num_vt(0x7F);
    w89_vt i64 = num_vt(0x7E);

    expect(!w89_match_valtype(&env, &ref_null_func, &ref_func),
           "val: null func </: func");
    expect(w89_match_valtype(&env, &ref_func, &ref_null_func),
           "val: func <: null func");
    expect(w89_match_valtype(&env, &i32, &i32), "val: i32 <: i32");
    expect(!w89_match_valtype(&env, &i32, &i64), "val: i32 </: i64");
    expect(w89_match_valtype(&env, &ref_null_s0, &ref_null_struct),
           "val: struct type <: struct");
    expect(w89_match_valtype(&env, &ref_null_s0, &ref_null_eq),
           "val: struct type <: eq");
    expect(!w89_match_valtype(&env, &ref_null_s0, &ref_null_func_t),
           "val: struct type </: func");
    expect(!w89_match_valtype(&env, &ref_null_s0, &ref_s0),
           "val: null ref </: nonnull ref");
    expect(w89_match_valtype(&env, &ref_s0, &ref_null_s0),
           "val: nonnull ref <: null ref");
    w89_typeenv_free(&env);
}

static void test_match_reftype_result(void)
{
    static const w89_subtype s0 = { 0, NULL, 0, W89_CK_STRUCT,
        { NULL, 0, NULL, 0 }, NULL, 0 };
    static const w89_subtype subs[] = { s0 };
    static const w89_u32 gs[] = { 1 };
    w89_typeenv env = make_env(subs, 1, gs, 1);
    w89_reftype r_func = { 1, 0, 0, W89_HT_FUNC };
    w89_reftype r_nofunc = { 1, 0, 0, W89_HT_NOFUNC };
    w89_vt a[1];
    w89_vt b[1];
    w89_vt c[2];
    w89_vt d[2];
    w89_vt e[2];

    a[0] = ref_vt(1, 0, 0, W89_HT_FUNC);
    b[0] = ref_vt(1, 0, 0, W89_HT_NOFUNC);
    c[0] = num_vt(0x7F);
    c[1] = ref_vt(1, 0, 0, W89_HT_FUNC);
    d[0] = num_vt(0x7F);
    d[1] = ref_vt(1, 0, 0, W89_HT_FUNC);
    e[0] = num_vt(0x7E);
    e[1] = ref_vt(1, 0, 0, W89_HT_FUNC);

    expect(w89_match_reftype(&env, &r_nofunc, &r_func),
           "reftype: nofunc <: func");
    expect(!w89_match_reftype(&env, &r_func, &r_nofunc),
           "reftype: func </: nofunc");
    expect(w89_match_resulttype(&env, c, 2, d, 2), "result: equal lists");
    expect(!w89_match_resulttype(&env, c, 2, e, 2), "result: mismatched list");
    expect(!w89_match_resulttype(&env, a, 1, b, 1), "result: ref mismatch");
    w89_typeenv_free(&env);
}

static void test_match_comptype(void)
{
    static const w89_fieldtype sf[] = {
        { 0, W89_PK_I8, { 0, 0x7F, { 0, 0, 0, 0 } }, 0 },
        { 0, W89_PK_I8, { 0, 0x7E, { 0, 0, 0, 0 } }, 0 } };
    static const w89_fieldtype tf[] = {
        { 0, W89_PK_I8, { 0, 0x7F, { 0, 0, 0, 0 } }, 0 } };
    static const w89_fieldtype uf[] = {
        { 0, W89_PK_I8, { 0, 0x7D, { 0, 0, 0, 0 } }, 0 } };
    static const w89_subtype s = { 0, NULL, 0, W89_CK_STRUCT, { NULL, 0, NULL, 0 },
        (w89_fieldtype *)sf, 2 };
    static const w89_subtype t = { 0, NULL, 0, W89_CK_STRUCT, { NULL, 0, NULL, 0 },
        (w89_fieldtype *)tf, 1 };
    static const w89_subtype u = { 0, NULL, 0, W89_CK_STRUCT, { NULL, 0, NULL, 0 },
        (w89_fieldtype *)uf, 1 };
    w89_typeenv env;
    memset(&env, 0, sizeof(env));

    expect(w89_match_comptype(&env, &s, &t), "comptype: width subtyping");
    expect(!w89_match_comptype(&env, &t, &s), "comptype: no width down");
    expect(!w89_match_comptype(&env, &t, &u), "comptype: field mismatch");
}

static w89_err validate_module_with_types(const w89_rectype *rectypes,
                                          w89_u32 nrectypes,
                                          const char **msg)
{
    w89_module m;
    w89_err e;
    memset(&m, 0, sizeof(m));
    m.rectypes = (w89_rectype *)rectypes;
    m.nrectypes = nrectypes;
    e = w89_module_validate(&m);
    *msg = w89_validate_message();
    return e;
}

typedef struct modb {
    w89_subtype ft;
    w89_rectype rt;
    w89_vt results[8];
    w89_vt params[8];
    w89_func fn;
    w89_u32 typeidx_slot;
    w89_module m;
} modb;

static void modb_init(modb *b, const w89_vt *params, w89_u32 nparams,
                      const w89_vt *results, w89_u32 nresults)
{
    memset(b, 0, sizeof(*b));
    if (nparams != 0) {
        memcpy(b->params, params, (size_t)nparams * sizeof(w89_vt));
    }
    if (nresults != 0) {
        memcpy(b->results, results, (size_t)nresults * sizeof(w89_vt));
    }
    b->ft.kind = W89_CK_FUNC;
    b->ft.is_final = 1;
    b->ft.ft.params = b->params;
    b->ft.ft.nparams = nparams;
    b->ft.ft.results = b->results;
    b->ft.ft.nresults = nresults;
    b->rt.subtypes = &b->ft;
    b->rt.n = 1;
    b->typeidx_slot = 0;
    b->m.rectypes = &b->rt;
    b->m.nrectypes = 1;
    b->m.func_types = &b->typeidx_slot;
    b->m.nfuncs = 1;
    b->m.funcs = &b->fn;
    b->m.ncode = 1;
    b->fn.typeidx = 0;
}

static void modb_set_code(modb *b, w89_instr *instrs, w89_u32 n)
{
    b->fn.code.items = instrs;
    b->fn.code.n = n;
    b->fn.code.cap = n;
}

static w89_instr mk_i(w89_u32 op)
{
    w89_instr in;
    memset(&in, 0, sizeof(in));
    in.op = op;
    return in;
}

static w89_instr mk_iu(w89_u32 op, w89_u32 idx)
{
    w89_instr in;
    memset(&in, 0, sizeof(in));
    in.op = op;
    in.idx = idx;
    return in;
}

static void expect_module(const char *name, modb *b, int expect_ok,
                          const char *want)
{
    w89_err e = w89_module_validate(&b->m);
    const char *msg = w89_validate_message();
    if (expect_ok) {
        expect(e == W89_ERR_NONE, name);
    } else {
        expect(e == W89_ERR_INVALID, name);
        if (want != NULL) {
            expect(strstr(msg, want) != NULL, name);
        }
    }
}

static void test_func_bodies(void)
{
    static const w89_vt r0[] = { { 0, 0x7F, { 0, 0, 0, 0 } } };
    modb b;
    w89_instr code[4];

    code[0] = mk_i(0x41);
    modb_init(&b, NULL, 0, r0, 1);
    modb_set_code(&b, code, 1);
    expect_module("func: i32.const result ok", &b, 1, NULL);

    modb_init(&b, NULL, 0, NULL, 0);
    modb_set_code(&b, code, 1);
    expect_module("func: leftover value", &b, 0, "block requires");

    modb_init(&b, NULL, 0, r0, 1);
    modb_set_code(&b, code, 0);
    expect_module("func: missing result", &b, 0,
                  "instruction requires [i32] but stack has []");

    code[0] = mk_iu(0x0C, 1);
    modb_init(&b, NULL, 0, NULL, 0);
    modb_set_code(&b, code, 1);
    expect_module("func: unknown label 1", &b, 0, "unknown label 1");

    code[0] = mk_iu(0x10, 1);
    modb_init(&b, NULL, 0, NULL, 0);
    modb_set_code(&b, code, 1);
    expect_module("func: unknown function", &b, 0, "unknown function 1");

    code[0] = mk_i(0x1B);
    modb_init(&b, NULL, 0, NULL, 0);
    modb_set_code(&b, code, 1);
    expect_module("func: select needs operands", &b, 0,
                  "instruction requires [bot bot i32]");

    code[0] = mk_i(0x00);
    code[1] = mk_i(0x0B);
    modb_init(&b, NULL, 0, NULL, 0);
    modb_set_code(&b, code, 1);
    expect_module("func: unreachable is valid", &b, 1, NULL);

    code[0] = mk_i(0x00);
    code[1] = mk_iu(0x10, 1);
    modb_init(&b, NULL, 0, NULL, 0);
    modb_set_code(&b, code, 2);
    expect_module("func: unknown function after unreachable", &b, 0,
                  "unknown function 1");
}

static void test_globals_const(void)
{
    static const w89_vt r0[] = { { 0, 0x7F, { 0, 0, 0, 0 } } };
    modb b;
    w89_instr code[2];
    w89_globaltype gt = { 0, { 0, 0x7F, { 0, 0, 0, 0 } } };
    w89_instr ginit[2];
    w89_global glob;
    w89_module saved;

    modb_init(&b, NULL, 0, r0, 1);
    b.m.nglobals = 1;
    b.m.globals = &glob;
    glob.type = gt;
    ginit[0] = mk_i(0x41);
    glob.init.items = ginit;
    glob.init.n = 1;
    glob.init.cap = 1;
    saved = b.m;
    code[0] = mk_iu(0x24, 0);
    modb_set_code(&b, code, 1);
    expect_module("global: set immutable", &b, 0, "immutable global");
    b.m = saved;
    code[0] = mk_iu(0x23, 0);
    modb_set_code(&b, code, 1);
    expect_module("global: get ok", &b, 1, NULL);

    ginit[0] = mk_i(0x45);
    expect_module("global: const expr required", &b, 0,
                  "constant expression required");
}

static void test_memory_align(void)
{
    static const w89_vt r0[] = { { 0, 0x7F, { 0, 0, 0, 0 } } };
    modb b;
    w89_instr code[2];
    w89_memory mem;
    w89_instr bad;
    memset(&mem, 0, sizeof(mem));
    mem.type.min = 1;
    mem.type.has_max = 0;
    mem.type.addr64 = 0;

    modb_init(&b, NULL, 0, r0, 1);
    b.m.nmemories = 1;
    b.m.memories = &mem;

    code[0] = mk_i(0x41);
    code[1] = mk_i(0x28);
    code[1].align = 3;
    modb_set_code(&b, code, 2);
    expect_module("memory: bad alignment", &b, 0,
                  "alignment must not be larger than natural");

    code[1].align = 0;
    modb_set_code(&b, code, 2);
    expect_module("memory: load ok", &b, 1, NULL);

    mem.type.min = 65537;
    modb_set_code(&b, code, 2);
    expect_module("memory: min too large", &b, 0, "memory size must be at most");

    memset(&bad, 0, sizeof(bad));
    bad.op = 0x28;
    bad.align = 0;
    bad.offset = 0x100000000UL;
    code[1] = bad;
    mem.type.min = 1;
    mem.type.has_max = 0;
    modb_set_code(&b, code, 2);
    expect_module("memory: offset out of range", &b, 0, "offset out of range");
}

static void test_declared_func(void)
{
    static const w89_vt refnullfunc[] = { { 1, 0x63, { 1, 0, 0, W89_HT_FUNC } } };
    modb b;
    w89_instr code[2];
    w89_export ex;
    w89_module saved;

    modb_init(&b, NULL, 0, refnullfunc, 1);
    code[0] = mk_iu(0xD2, 0);
    modb_set_code(&b, code, 1);
    expect_module("ref.func: undeclared", &b, 0,
                  "undeclared function reference 0");

    memset(&ex, 0, sizeof(ex));
    ex.kind = 0x00;
    ex.index = 0;
    b.m.nexports = 1;
    b.m.exports = &ex;
    expect_module("ref.func: exported is declared", &b, 1, NULL);

    saved = b.m;
    b.m.nexports = 0;
    b.m.exports = NULL;
    modb_set_code(&b, code, 1);
    expect_module("ref.func: still undeclared", &b, 0,
                  "undeclared function reference 0");
    b.m = saved;
}

static void test_start(void)
{
    static const w89_vt p0[] = { { 0, 0x7F, { 0, 0, 0, 0 } } };
    modb b;
    w89_instr code[2];

    modb_init(&b, p0, 1, NULL, 0);
    b.m.has_start = 1;
    b.m.start = 0;
    code[0] = mk_i(0x0B);
    modb_set_code(&b, code, 1);
    expect_module("start: with params", &b, 0,
                  "start function must not have parameters or results");
}

static void test_elem_type_mismatch(void)
{
    static const w89_vt r0[] = { { 0, 0x7F, { 0, 0, 0, 0 } } };
    modb b;
    w89_instr code[2];
    w89_tabletype tt;
    w89_table tab;
    w89_elem el;
    w89_instr eoff;
    w89_u32 idxs[1];

    memset(&tt, 0, sizeof(tt));
    tt.limits.min = 1;
    tt.limits.has_max = 0;
    tt.rt.nullable = 1;
    tt.rt.is_typeidx = 0;
    tt.rt.abs = W89_HT_EXTERN;

    memset(&tab, 0, sizeof(tab));
    tab.type = tt;

    memset(&el, 0, sizeof(el));
    el.flags = 0;
    el.rt.nullable = 0;
    el.rt.is_typeidx = 0;
    el.rt.abs = W89_HT_FUNC;
    el.n = 1;
    el.indices = idxs;
    idxs[0] = 0;
    eoff = mk_i(0x41);
    el.offset.items = &eoff;
    el.offset.n = 1;
    el.offset.cap = 1;

    modb_init(&b, NULL, 0, r0, 1);
    b.m.ntables = 1;
    b.m.tables = &tab;
    b.m.nelems = 1;
    b.m.elems = &el;
    code[0] = mk_i(0x0B);
    modb_set_code(&b, code, 1);
    expect_module("elem: type mismatch with table", &b, 0,
                  "element segment's type");
}

static void test_type_section_rules(void)
{
    static const w89_vt r0[] = { { 0, 0x7F, { 0, 0, 0, 0 } } };
    static const w89_subtype fu = { 0, NULL, 0, W89_CK_FUNC,
        { NULL, 0, NULL, 0 }, NULL, 0 };
    static const w89_u32 fwd_sup[] = { 1 };
    static const w89_subtype fwd = { 0, (w89_u32 *)fwd_sup, 1, W89_CK_FUNC,
        { NULL, 0, NULL, 0 }, NULL, 0 };
    static const w89_subtype rec_fwd[] = { fwd, fu };
    static const w89_rectype rt_fwd = { (w89_subtype *)rec_fwd, 2 };

    static const w89_subtype fin = { 1, NULL, 0, W89_CK_FUNC,
        { NULL, 0, NULL, 0 }, NULL, 0 };
    static const w89_u32 fin_sup[] = { 0 };
    static const w89_subtype fin_ext = { 0, (w89_u32 *)fin_sup, 1,
        W89_CK_FUNC, { NULL, 0, NULL, 0 }, NULL, 0 };
    static const w89_subtype rec_fin[] = { fin, fin_ext };
    static const w89_rectype rt_fin = { (w89_subtype *)rec_fin, 2 };

    static const w89_fieldtype s0f[] = {
        { 0, W89_PK_I8, { 0, 0x7F, { 0, 0, 0, 0 } }, 0 } };
    static const w89_subtype s0 = { 0, NULL, 0, W89_CK_STRUCT,
        { NULL, 0, NULL, 0 }, (w89_fieldtype *)s0f, 1 };
    static const w89_u32 bad_sup[] = { 0 };
    static const w89_fieldtype b0f[] = {
        { 0, W89_PK_I8, { 0, 0x7E, { 0, 0, 0, 0 } }, 0 } };
    static const w89_subtype b0 = { 0, (w89_u32 *)bad_sup, 1, W89_CK_STRUCT,
        { NULL, 0, NULL, 0 }, (w89_fieldtype *)b0f, 1 };
    static const w89_subtype rec_bad[] = { s0, b0 };
    static const w89_rectype rt_bad = { (w89_subtype *)rec_bad, 2 };

    static const w89_u32 ok_sup[] = { 0 };
    static const w89_subtype ok0 = { 0, NULL, 0, W89_CK_FUNC,
        { NULL, 0, NULL, 0 }, NULL, 0 };
    static const w89_subtype ok1 = { 0, (w89_u32 *)ok_sup, 1, W89_CK_FUNC,
        { NULL, 0, NULL, 0 }, NULL, 0 };
    static const w89_subtype rec_ok[] = { ok0, ok1 };
    static const w89_rectype rt_ok = { (w89_subtype *)rec_ok, 2 };

    const char *msg = NULL;
    w89_err e;

    e = validate_module_with_types(&rt_fwd, 1, &msg);
    expect(e == W89_ERR_INVALID, "type: forward use rejected");
    expect(strstr(msg, "forward use of type 1") != NULL,
           "type: forward use message");

    e = validate_module_with_types(&rt_fin, 1, &msg);
    expect(e == W89_ERR_INVALID, "type: final supertype rejected");
    expect(strstr(msg, "sub type 1 has final super type 0") != NULL,
           "type: final supertype message");

    e = validate_module_with_types(&rt_bad, 1, &msg);
    expect(e == W89_ERR_INVALID, "type: mismatch rejected");
    expect(strstr(msg, "sub type 1 does not match super type 0") != NULL,
           "type: mismatch message");

    e = validate_module_with_types(&rt_ok, 1, &msg);
    expect(e == W89_ERR_NONE, "type: valid rec type accepted");

    (void)r0;
}

static void test_bot_and_poly(void)
{
    static const w89_vt r0[] = { { 0, 0x7F, { 0, 0, 0, 0 } } };
    modb b;
    w89_instr code[8];

    code[0] = mk_i(0x00);
    code[1] = mk_i(0x6A);
    modb_init(&b, NULL, 0, r0, 1);
    modb_set_code(&b, code, 2);
    expect_module("poly: i32.add after unreachable", &b, 1, NULL);

    code[0] = mk_i(0x00);
    code[1] = mk_i(0x41);
    code[2] = mk_i(0x0E);
    modb_init(&b, NULL, 0, NULL, 0);
    modb_set_code(&b, code, 3);
    expect_module("poly: br_table on polymorphic stack", &b, 1, NULL);
}

static void test_if_no_else(void)
{
    static const w89_vt r0[] = { { 0, 0x7F, { 0, 0, 0, 0 } } };
    modb b;
    w89_instr code[4];

    code[0] = mk_i(0x41);
    code[1] = mk_i(0x04);
    code[2] = mk_i(0x41);
    code[3] = mk_i(0x0B);
    modb_init(&b, NULL, 0, r0, 1);
    modb_set_code(&b, code, 4);
    expect_module("if: missing else branch", &b, 0, "type mismatch");

    code[0] = mk_i(0x41);
    code[1] = mk_i(0x04);
    code[2] = mk_i(0x41);
    code[3] = mk_i(0x05);
    modb_init(&b, NULL, 0, r0, 1);
    modb_set_code(&b, code, 4);
    expect_module("if: truncated else", &b, 0, "type mismatch");
}

static void test_local_init_blocks(void)
{
    static const w89_vt refextern[] = { { 1, 0x64, { 0, 0, 0, W89_HT_EXTERN } } };
    modb b;
    w89_instr code[8];

    code[0] = mk_i(0x02);
    code[1] = mk_iu(0x20, 0);
    code[2] = mk_iu(0x21, 1);
    code[3] = mk_i(0x0B);
    code[4] = mk_iu(0x20, 1);
    modb_init(&b, (w89_vt *)refextern, 1, NULL, 0);
    b.fn.locals = (w89_vt *)refextern;
    b.fn.nlocals = 1;
    modb_set_code(&b, code, 5);
    expect_module("local_init: set inside block does not escape", &b, 0,
                  "uninitialized local");
}

int main(void)
{
    test_canon_identical();
    test_canon_recursive();
    test_canon_final_supers();
    test_match_deftype();
    test_match_valtype();
    test_match_reftype_result();
    test_match_comptype();
    test_type_section_rules();
    test_func_bodies();
    test_globals_const();
    test_memory_align();
    test_declared_func();
    test_start();
    test_elem_type_mismatch();
    test_bot_and_poly();
    test_if_no_else();
    test_local_init_blocks();
    if (failures == 0) {
        printf("PASS: test_validate\n");
    } else {
        printf("FAIL: %d failures in test_validate\n", failures);
    }
    return failures == 0 ? 0 : 1;
}
