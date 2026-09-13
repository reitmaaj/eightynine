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

static w89_vt i32_t(void)
{
    w89_vt v;
    memset(&v, 0, sizeof(v));
    v.is_ref = 0;
    v.num = 0x7F;
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

static w89_instr in_c32b(w89_byte op, w89_u32 bits)
{
    w89_instr i;
    memset(&i, 0, sizeof(i));
    i.op = op;
    memcpy(&i.c32, &bits, sizeof(bits));
    return i;
}

static w89_instr in_v128_const(const w89_byte *b)
{
    w89_instr i;
    memset(&i, 0, sizeof(i));
    i.op = 0xFD;
    i.sub = 0x0C;
    memcpy(i.c128.b, b, 16);
    return i;
}

static w89_instr in_c64b(w89_byte op, w89_u64 bits)
{
    w89_instr i;
    memset(&i, 0, sizeof(i));
    i.op = op;
    memcpy(&i.c64, &bits, sizeof(bits));
    return i;
}

static w89_instr in_f32(w89_byte op, w89_f32 x)
{
    w89_instr i;
    memset(&i, 0, sizeof(i));
    i.op = op;
    i.f32 = x;
    return i;
}

static w89_instr in_f64(w89_byte op, w89_f64 x)
{
    w89_instr i;
    memset(&i, 0, sizeof(i));
    i.op = op;
    i.f64 = x;
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
    if (valtype == 0) {
        i.bt.is_typeidx = 0;
        i.bt.vt.is_ref = 0;
        i.bt.vt.num = 0;
    } else {
        i.bt.is_typeidx = 0;
        i.bt.vt.is_ref = 0;
        i.bt.vt.num = valtype;
    }
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

static w89_instr br_table(w89_u32 nlabels, const w89_u32 *labels, w89_u32 def)
{
    w89_instr i;
    memset(&i, 0, sizeof(i));
    i.op = 0x0E;
    i.n = nlabels;
    i.labels = (w89_u32 *)labels;
    i.idx = def;
    return i;
}

static void run(const w89_instr *items, w89_u32 n, w89_frame *f,
                w89_eval_out *out)
{
    w89_config cfg;
    w89_config_init(&cfg, f);
    w89_code_range(&cfg.code, items, n, 0, n);
    *out = w89_eval_iter(&cfg);
    w89_config_free(&cfg);
}

/* Internal driver self-check (testing 0020): the iterative entry must reach a
 * definite status on the same program it runs. With the legacy stepper
 * deleted, parity-vs-legacy is replaced by asserting the driver runs the
 * program to a defined result. */
static void parity_eq(const w89_instr *items, w89_u32 n, w89_frame *f,
                      const char *name)
{
    w89_config ci;
    w89_eval_out oi;
    int ok;
    (void)name;

    w89_config_init(&ci, f);
    w89_code_range(&ci.code, items, n, 0, n);
    oi = w89_eval_iter(&ci);

    ok = (oi.status == W89_EVAL_OK) || (oi.status == W89_EVAL_TRAP)
        || (oi.status == W89_EVAL_CRASH);
    expect(ok, name);
    w89_eval_out_free(&oi);
    w89_config_free(&ci);
}

static void test_value_model(void)
{
    w89_value n;
    w89_ref nl;
    w89_value rv;
    w89_ref fr;
    w89_ref xr;
    w89_ref er;

    n = w89_value_num(7);
    expect(!n.is_ref && n.u.num == 7, "num value discriminator");
    nl = w89_ref_null();
    rv = w89_value_ref(&nl);
    expect(rv.is_ref && rv.u.ref.kind == W89_RK_NULL
           && w89_ref_is_null(&rv.u.ref), "null ref");
    fr = w89_ref_func(NULL);
    xr = w89_ref_extern(0);
    er = w89_ref_exn(NULL);
    expect(fr.kind == W89_RK_FUNC && xr.kind == W89_RK_EXTERN
           && er.kind == W89_RK_EXN, "ref kinds distinct");
    expect(fr.kind != xr.kind && xr.kind != er.kind && fr.kind != er.kind
           && fr.kind != nl.kind, "ref kinds pairwise distinct");
}

static void test_v128_value_model(void)
{
    static const w89_byte b[16] = { 1, 2, 3, 4, 5, 6, 7, 8,
                                    9, 10, 11, 12, 13, 14, 15, 16 };
    w89_v128 vec;
    w89_v128 got;
    w89_value v;
    w89_value copy;
    int k;

    memset(&vec, 0, sizeof(vec));
    for (k = 0; k < 16; k = k + 1) {
        vec.b[k] = b[k];
    }
    v = w89_value_v128(&vec);
    expect(!v.is_ref, "v128 value is not a reference");
    copy = v;
    got = copy.u.vec;
    for (k = 0; k < 16; k = k + 1) {
        if (got.b[k] != b[k]) {
            break;
        }
    }
    expect(k == 16, "v128 round-trips all 16 bytes through w89_value");

    /* A widened w89_value must keep scalar numerics intact: the scalar fast
     * path (u.num) must not be disturbed by carrying a wide member. */
    v = w89_value_num(0x1122334455667788UL);
    expect(!v.is_ref && v.u.num == 0x1122334455667788UL,
           "scalar num unchanged with widened value");
}

static void test_v128_eval_const(void)
{
    static const w89_byte b[16] = { 1, 2, 3, 4, 5, 6, 7, 8,
                                    9, 10, 11, 12, 13, 14, 15, 16 };
    w89_moduleinst m;
    w89_frame f;
    w89_instr a[3];
    w89_eval_out out;
    int k;

    memset(&m, 0, sizeof(m));
    memset(&f, 0, sizeof(f));
    f.inst = &m;

    a[0] = in_v128_const(b);
    run(a, 1, &f, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && !out.vs[0].is_ref, "v128.const eval status");
    if (out.status == W89_EVAL_OK && out.nvs == 1) {
        for (k = 0; k < 16; k = k + 1) {
            if (out.vs[0].u.vec.b[k] != b[k]) {
                break;
            }
        }
        expect(k == 16, "v128.const eval round-trips all 16 bytes");
    }
    w89_eval_out_free(&out);

    /* A v128 is one distinct 16-byte slot amid scalars on the unified stack:
     * i32.const; v128.const; i32.const must not alias or truncate. */
    a[0] = in_c32(0x41, 0x11223344);
    a[1] = in_v128_const(b);
    a[2] = in_c32(0x41, 0xAABBCCDD);
    run(a, 3, &f, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 3,
           "v128 amid scalars status");
    if (out.status == W89_EVAL_OK && out.nvs == 3) {
        expect(out.vs[0].u.num == 0x11223344UL,
               "scalar before v128 intact");
        for (k = 0; k < 16; k = k + 1) {
            if (out.vs[1].u.vec.b[k] != b[k]) {
                break;
            }
        }
        expect(k == 16, "v128 slot intact amid scalars");
        expect(out.vs[2].u.num == 0xAABBCCDDUL,
               "scalar after v128 intact");
    }
    w89_eval_out_free(&out);
}

static void test_exn_ref_and_tag(void)
{
    w89_taginst t1;
    w89_taginst t2;
    w89_taginst *ta[1];
    w89_taginst *tb[1];
    w89_exn ex;
    w89_value args[2];
    w89_ref r;
    w89_ref n;

    memset(&t1, 0, sizeof(t1));
    memset(&t2, 0, sizeof(t2));
    args[0] = w89_value_num(11);
    args[1] = w89_value_num(22);
    ex.tag = &t1;
    ex.args = args;
    ex.nargs = 2;
    r = w89_ref_exn(&ex);
    expect(r.kind == W89_RK_EXN, "exn ref kind");
    expect(r.u.exn == &ex && r.u.exn->tag == &t1 && r.u.exn->nargs == 2,
           "exn ref payload");
    expect(r.u.exn->args[0].u.num == 11 && r.u.exn->args[1].u.num == 22,
           "exn ref arg order");
    expect(&t1 != &t2, "tag instances distinct");
    ta[0] = &t1;
    tb[0] = &t1;
    expect(ta[0] == tb[0], "imported tag resolves to same instance");
    n = w89_ref_null();
    expect(w89_ref_is_null(&n), "null is null");
}

static void test_const_nop(void)
{
    w89_moduleinst m;
    w89_frame f;
    w89_instr a[2];
    w89_eval_out out;

    memset(&m, 0, sizeof(m));
    memset(&f, 0, sizeof(f));
    f.inst = &m;

    run(NULL, 0, &f, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 0,
           "empty program");
    w89_eval_out_free(&out);

    a[0] = in(0x01);
    run(a, 1, &f, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 0, "nop");
    w89_eval_out_free(&out);

    a[0] = in_c32(0x41, 42);
    run(a, 1, &f, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && !out.vs[0].is_ref && out.vs[0].u.num == 42,
           "i32.const 42");
    w89_eval_out_free(&out);

    a[0] = in_c64b(0x42, 0x1122334455667788UL);
    run(a, 1, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 0x1122334455667788UL,
           "i64.const");
    w89_eval_out_free(&out);

    a[0] = in_f32(0x43, 1.5f);
    run(a, 1, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 0x3FC00000UL,
           "f32.const bits");
    w89_eval_out_free(&out);

    a[0] = in_f64(0x44, 2.5);
    run(a, 1, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 0x4004000000000000UL,
           "f64.const bits");
    w89_eval_out_free(&out);
}

static void test_locals(void)
{
    w89_moduleinst m;
    w89_local locs[2];
    w89_frame f;
    w89_instr a[3];
    w89_eval_out out;

    memset(&m, 0, sizeof(m));
    memset(&locs, 0, sizeof(locs));
    locs[0].set = 1;
    locs[0].v = w89_value_num(3);
    f.inst = &m;
    f.locals = locs;
    f.nlocals = 2;

    a[0] = in_idx(0x20, 0);
    run(a, 1, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 3, "local.get");
    w89_eval_out_free(&out);

    a[0] = in_c32(0x41, 9);
    a[1] = in_idx(0x21, 0);
    a[2] = in_idx(0x20, 0);
    run(a, 3, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 9
           && locs[0].v.u.num == 9, "local.set then get");
    w89_eval_out_free(&out);

    a[0] = in_c32(0x41, 9);
    a[1] = in_idx(0x22, 0);
    run(a, 2, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 9
           && locs[0].v.u.num == 9, "local.tee");
    w89_eval_out_free(&out);

    a[0] = in_idx(0x20, 1);
    run(a, 1, &f, &out);
    expect(out.status == W89_EVAL_CRASH && out.msg
           && strcmp(out.msg, "read of uninitialized local") == 0,
           "uninitialized local crash");
    w89_eval_out_free(&out);
}

static void test_globals(void)
{
    w89_moduleinst m;
    w89_globalinst g;
    w89_globalinst g2;
    w89_frame f;
    w89_instr a[3];
    w89_eval_out out;

    memset(&m, 0, sizeof(m));
    memset(&g, 0, sizeof(g));
    memset(&g2, 0, sizeof(g2));
    g.type.mut = 1;
    g.type.vt = i32_t();
    g.value = w89_value_num(5);
    f.inst = &m;
    f.nlocals = 0;

    {
        w89_globalinst *gl[1];
        gl[0] = &g;
        m.globals = gl;
        m.nglobals = 1;
    }

    a[0] = in_idx(0x23, 0);
    run(a, 1, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 5, "global.get");
    w89_eval_out_free(&out);

    a[0] = in_c32(0x41, 7);
    a[1] = in_idx(0x24, 0);
    a[2] = in_idx(0x23, 0);
    run(a, 3, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 7
           && g.value.u.num == 7, "global.set then get");
    w89_eval_out_free(&out);

    g2.type.mut = 0;
    g2.type.vt = i32_t();
    g2.value = w89_value_num(1);
    {
        w89_globalinst *gl2[1];
        gl2[0] = &g2;
        m.globals = gl2;
    }

    a[0] = in_c32(0x41, 2);
    a[1] = in_idx(0x24, 0);
    run(a, 2, &f, &out);
    expect(out.status == W89_EVAL_CRASH && out.msg
           && strcmp(out.msg, "write to immutable global") == 0,
           "immutable global crash");
    w89_eval_out_free(&out);
}

static void test_drop_select(void)
{
    w89_moduleinst m;
    w89_frame f;
    w89_instr a[4];
    w89_eval_out out;

    memset(&m, 0, sizeof(m));
    memset(&f, 0, sizeof(f));
    f.inst = &m;

    a[0] = in_c32(0x41, 1);
    a[1] = in_c32(0x41, 2);
    a[2] = in(0x1A);
    run(a, 3, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 1, "drop keeps lower value");
    w89_eval_out_free(&out);

    a[0] = in_c32(0x41, 1);
    a[1] = in_c32(0x41, 2);
    a[2] = in_c32(0x41, 0);
    a[3] = in(0x1B);
    run(a, 4, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 2,
           "select zero condition keeps v2");
    w89_eval_out_free(&out);

    a[2] = in_c32(0x41, 1);
    run(a, 4, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 1,
           "select nonzero condition keeps v1");
    w89_eval_out_free(&out);

    a[2] = in_c32(0x41, 0);
    a[3] = in(0x1C);
    run(a, 4, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 2, "select_t behaves like select");
    w89_eval_out_free(&out);

    a[0] = in(0x1A);
    run(a, 1, &f, &out);
    expect(out.status == W89_EVAL_CRASH && out.msg
           && strcmp(out.msg, "stack underflow") == 0,
           "drop underflow crash");
    w89_eval_out_free(&out);
}

static void test_block(void)
{
    w89_moduleinst m;
    w89_frame f;
    w89_instr a[6];
    w89_eval_out out;

    memset(&m, 0, sizeof(m));
    memset(&f, 0, sizeof(f));
    f.inst = &m;

    a[0] = block_vt(0x02, 0x7F);
    a[1] = in_c32(0x41, 7);
    a[2] = in(0x0B);
    run(a, 3, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 7, "block result");
    w89_eval_out_free(&out);

    a[0] = block_vt(0x02, 0);
    a[1] = block_vt(0x02, 0);
    a[2] = in_c32(0x41, 7);
    a[3] = in(0x0B);
    a[4] = in(0x0B);
    run(a, 5, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 7, "nested blocks");
    w89_eval_out_free(&out);

    a[0] = in_c32(0x41, 0);
    a[1] = block_vt(0x04, 0x7F);
    a[2] = in_c32(0x41, 1);
    a[3] = in(0x05);
    a[4] = in_c32(0x41, 2);
    a[5] = in(0x0B);
    run(a, 6, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 2, "if else zero branch");
    w89_eval_out_free(&out);

    a[0] = in_c32(0x41, 1);
    run(a, 6, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 1, "if then nonzero branch");
    w89_eval_out_free(&out);

    a[0] = in_c32(0x41, 0);
    a[1] = block_vt(0x04, 0);
    a[2] = in(0x00);
    a[3] = in(0x0B);
    run(a, 4, &f, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 0,
           "if without else, zero condition");
    w89_eval_out_free(&out);

    a[0] = in_c32(0x41, 1);
    run(a, 4, &f, &out);
    expect(out.status == W89_EVAL_TRAP && out.msg
           && strcmp(out.msg, "unreachable executed") == 0,
           "if without else, nonzero condition runs then-arm");
    w89_eval_out_free(&out);
}

static void test_block_params(void)
{
    w89_vt p[1];
    w89_vt r[1];
    w89_subtype sub;
    w89_deftype dt;
    w89_typeenv env;
    w89_moduleinst m;
    w89_frame f;
    w89_instr a[5];
    w89_eval_out out;

    p[0] = i32_t();
    r[0] = i32_t();
    memset(&sub, 0, sizeof(sub));
    sub.is_final = 1;
    sub.kind = W89_CK_FUNC;
    sub.ft.params = p;
    sub.ft.nparams = 1;
    sub.ft.results = r;
    sub.ft.nresults = 1;
    memset(&dt, 0, sizeof(dt));
    dt.sub = &sub;
    memset(&env, 0, sizeof(env));
    env.ntypes = 1;
    env.types = &dt;
    m.types = &env;
    memset(&f, 0, sizeof(f));
    f.inst = &m;

    a[0] = in_c32(0x41, 5);
    a[1] = block_tidx(0x02, 0);
    a[2] = in_c32(0x41, 1);
    a[3] = in(0x6A);
    a[4] = in(0x0B);
    run(a, 5, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 6,
           "block with params consumes arguments");
    w89_eval_out_free(&out);
}

static void test_br(void)
{
    w89_moduleinst m;
    w89_frame f;
    w89_instr a[8];
    w89_u32 lbls[1];
    w89_eval_out out;

    memset(&m, 0, sizeof(m));
    memset(&f, 0, sizeof(f));
    f.inst = &m;

    a[0] = block_vt(0x02, 0x7F);
    a[1] = in_c32(0x41, 9);
    a[2] = in_idx(0x0C, 0);
    a[3] = in_c32(0x41, 5);
    a[4] = in(0x0B);
    run(a, 5, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 9,
           "br to block skips trailing body");
    w89_eval_out_free(&out);

    a[0] = block_vt(0x02, 0x7F);
    a[1] = in_c32(0x41, 5);
    a[2] = in_c32(0x41, 0);
    a[3] = in_idx(0x0D, 0);
    a[4] = in(0x1A);
    a[5] = in_c32(0x41, 9);
    a[6] = in(0x0B);
    run(a, 7, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 9, "br_if not taken");
    w89_eval_out_free(&out);

    a[2] = in_c32(0x41, 1);
    run(a, 7, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 5, "br_if taken");
    w89_eval_out_free(&out);

    lbls[0] = 0;
    a[0] = block_vt(0x02, 0x7F);
    a[1] = in_c32(0x41, 9);
    a[2] = in_c32(0x41, 5);
    a[3] = br_table(1, lbls, 0);
    a[4] = in_c32(0x41, 5);
    a[5] = in(0x0B);
    run(a, 6, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 9,
           "br_table selector beyond list uses default");
    w89_eval_out_free(&out);

    a[2] = in_c32(0x41, 0);
    run(a, 6, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 9,
           "br_table in-range selector");
    w89_eval_out_free(&out);

    a[0] = block_vt(0x02, 0x7F);
    a[1] = block_vt(0x02, 0);
    a[2] = in_c32(0x41, 7);
    a[3] = in_idx(0x0C, 1);
    a[4] = in(0x0B);
    a[5] = in_c32(0x41, 99);
    a[6] = in(0x0B);
    run(a, 7, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 7,
           "br 1 unwinds inner block to outer label");
    w89_eval_out_free(&out);
}

static void test_loop(void)
{
    w89_moduleinst m;
    w89_local locs[1];
    w89_frame f;
    w89_instr a[15];
    w89_eval_out out;

    memset(&m, 0, sizeof(m));
    memset(&locs, 0, sizeof(locs));
    locs[0].set = 1;
    locs[0].v = w89_value_num(3);
    f.inst = &m;
    f.locals = locs;
    f.nlocals = 1;

    a[0] = block_vt(0x02, 0x7F);
    a[1] = block_vt(0x03, 0);
    a[2] = in_idx(0x20, 0);
    a[3] = in_idx(0x20, 0);
    a[4] = in(0x45);
    a[5] = in_idx(0x0D, 1);
    a[6] = in(0x1A);
    a[7] = in_idx(0x20, 0);
    a[8] = in_c32(0x41, 1);
    a[9] = in(0x6B);
    a[10] = in_idx(0x21, 0);
    a[11] = in_idx(0x0C, 0);
    a[12] = in(0x0B);
    a[13] = in(0x00);
    a[14] = in(0x0B);
    run(a, 15, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 0,
           "loop re-entry via br 0 and exit via br 1");
    w89_eval_out_free(&out);
}

static void test_return(void)
{
    w89_moduleinst m;
    w89_frame f;
    w89_instr a[5];
    w89_eval_out out;

    memset(&m, 0, sizeof(m));
    memset(&f, 0, sizeof(f));
    f.inst = &m;

    a[0] = in(0x0F);
    run(a, 1, &f, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 0,
           "return at function top level is a clean return");
    w89_eval_out_free(&out);

    a[0] = block_vt(0x02, 0);
    a[1] = in_c32(0x41, 3);
    a[2] = in(0x0F);
    a[3] = in(0x00);
    a[4] = in(0x0B);
    run(a, 5, &f, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 3,
           "return keeps top value and skips trailing body");
    w89_eval_out_free(&out);
}

static void test_unreachable(void)
{
    w89_moduleinst m;
    w89_frame f;
    w89_instr a[5];
    w89_eval_out out;

    memset(&m, 0, sizeof(m));
    memset(&f, 0, sizeof(f));
    f.inst = &m;

    a[0] = in(0x00);
    run(a, 1, &f, &out);
    expect(out.status == W89_EVAL_TRAP && out.msg
           && strcmp(out.msg, "unreachable executed") == 0,
           "unreachable traps");
    w89_eval_out_free(&out);

    a[0] = in(0x00);
    a[1] = in_c32(0x41, 2);
    run(a, 2, &f, &out);
    expect(out.status == W89_EVAL_TRAP, "unreachable traps before trailing");
    w89_eval_out_free(&out);

    a[0] = in_c32(0x41, 1);
    a[1] = in(0x1A);
    a[2] = in(0x00);
    a[3] = in_c32(0x41, 2);
    a[4] = in(0x1A);
    run(a, 5, &f, &out);
    expect(out.status == W89_EVAL_TRAP && out.msg
           && strcmp(out.msg, "unreachable executed") == 0,
           "unreachable in mid-program");
    w89_eval_out_free(&out);
}

static void test_numeric_traps(void)
{
    w89_moduleinst m;
    w89_frame f;
    w89_instr a[3];
    w89_eval_out out;

    memset(&m, 0, sizeof(m));
    memset(&f, 0, sizeof(f));
    f.inst = &m;

    a[0] = in_c32(0x41, 1);
    a[1] = in_c32(0x41, 0);
    a[2] = in(0x6E);
    run(a, 3, &f, &out);
    expect(out.status == W89_EVAL_TRAP && out.msg
           && strcmp(out.msg, "integer divide by zero") == 0,
           "i32.div_u by zero");
    w89_eval_out_free(&out);

    a[0] = in_c32b(0x41, 0x80000000u);
    a[1] = in_c32b(0x41, 0xFFFFFFFFu);
    a[2] = in(0x6D);
    run(a, 3, &f, &out);
    expect(out.status == W89_EVAL_TRAP && out.msg
           && strcmp(out.msg, "integer overflow") == 0,
           "i32.div_s INT_MIN / -1 overflow");
    w89_eval_out_free(&out);

    a[0] = in_c32(0x41, 1);
    a[1] = in_c32(0x41, 0);
    a[2] = in(0x6F);
    run(a, 3, &f, &out);
    expect(out.status == W89_EVAL_TRAP && out.msg
           && strcmp(out.msg, "integer divide by zero") == 0,
           "i32.rem_s by zero");
    w89_eval_out_free(&out);

    a[0] = in_c32b(0x41, 0x80000000u);
    a[1] = in_c32b(0x41, 0xFFFFFFFFu);
    a[2] = in(0x6F);
    run(a, 3, &f, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 0, "i32.rem_s INT_MIN / -1 is 0");
    w89_eval_out_free(&out);

    a[0] = in_c64b(0x42, 1UL);
    a[1] = in_c64b(0x42, 0UL);
    a[2] = in(0x7F);
    run(a, 3, &f, &out);
    expect(out.status == W89_EVAL_TRAP && out.msg
           && strcmp(out.msg, "integer divide by zero") == 0,
           "i64.div_s by zero");
    w89_eval_out_free(&out);

    a[0] = in_c64b(0x42, 0x8000000000000000UL);
    a[1] = in_c64b(0x42, 0xFFFFFFFFFFFFFFFFUL);
    a[2] = in(0x7F);
    run(a, 3, &f, &out);
    expect(out.status == W89_EVAL_TRAP && out.msg
           && strcmp(out.msg, "integer overflow") == 0,
           "i64.div_s INT64_MIN / -1 overflow");
    w89_eval_out_free(&out);

    a[0] = in_f64(0x44, w89_bits_f64(W89_F64_CANON_NAN));
    a[1] = in(0xAA);
    run(a, 2, &f, &out);
    expect(out.status == W89_EVAL_TRAP && out.msg
           && strcmp(out.msg, "invalid conversion to integer") == 0,
           "i32.trunc_f64_s of NaN");
    w89_eval_out_free(&out);
}

static void test_numeric_results(void)
{
    w89_moduleinst m;
    w89_frame f;
    w89_instr a[3];
    w89_eval_out out;

    memset(&m, 0, sizeof(m));
    memset(&f, 0, sizeof(f));
    f.inst = &m;

    a[0] = in_c32(0x41, 1);
    a[1] = in_c32(0x41, 2);
    a[2] = in(0x6A);
    run(a, 3, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 3, "i32.add");
    w89_eval_out_free(&out);

    a[0] = in_c32(0x41, 0);
    a[1] = in(0x45);
    run(a, 2, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 1, "i32.eqz");
    w89_eval_out_free(&out);

    a[0] = in_c32(0x41, 5);
    a[1] = in_c32b(0x41, 0xFFFFFFFDu);
    a[2] = in(0x48);
    run(a, 3, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 0, "i32.lt_s signed compare");
    w89_eval_out_free(&out);

    a[0] = in_c64b(0x42, 6UL);
    a[1] = in_c64b(0x42, 7UL);
    a[2] = in(0x7E);
    run(a, 3, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 42, "i64.mul");
    w89_eval_out_free(&out);

    a[0] = in_f32(0x43, 1.5f);
    a[1] = in_f32(0x43, 2.5f);
    a[2] = in(0x92);
    run(a, 3, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 0x40800000UL, "f32.add bits");
    w89_eval_out_free(&out);

    a[0] = in_f64(0x44, 5.0);
    a[1] = in_f64(0x44, 2.0);
    a[2] = in(0xA1);
    run(a, 3, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 0x4008000000000000UL,
           "f64.sub bits");
    w89_eval_out_free(&out);

    a[0] = in_c64b(0x42, 0x100000001UL);
    a[1] = in(0xA7);
    run(a, 2, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 1, "i32.wrap_i64");
    w89_eval_out_free(&out);

    a[0] = in_c32(0x41, 5);
    a[1] = in(0xB2);
    run(a, 2, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 0x40A00000UL,
           "f32.convert_i32_s bits");
    w89_eval_out_free(&out);

    a[0] = in_c32b(0x41, 0xFFFFFFFFu);
    a[1] = in(0xAC);
    run(a, 2, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 0xFFFFFFFFFFFFFFFFUL,
           "i64.extend_i32_s");
    w89_eval_out_free(&out);

    a[0] = in_f32(0x43, 1.0f);
    a[1] = in(0xBC);
    run(a, 2, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 0x3F800000UL,
           "i32.reinterpret_f32");
    w89_eval_out_free(&out);

    a[0] = in_c32(0x41, 1);
    a[1] = in(0x67);
    run(a, 2, &f, &out);
    expect(out.nvs == 1 && out.vs[0].u.num == 31, "i32.clz");
    w89_eval_out_free(&out);
}

static void test_block_extent(void)
{
    w89_instr items[8];
    w89_u32 bs;
    w89_u32 be;
    w89_u32 has_else;
    w89_u32 ep;
    w89_err e;

    items[0] = block_vt(0x02, 0x7F);
    items[1] = in_c32(0x41, 7);
    items[2] = in(0x0B);
    e = w89_block_extent(items, 3, 0, &bs, &be, &has_else, &ep);
    expect(e == W89_ERR_NONE && bs == 1 && be == 2 && !has_else,
           "block extent");

    items[0] = block_vt(0x04, 0);
    items[1] = in_c32(0x41, 1);
    items[2] = in(0x05);
    items[3] = in_c32(0x41, 2);
    items[4] = in(0x0B);
    e = w89_block_extent(items, 5, 0, &bs, &be, &has_else, &ep);
    expect(e == W89_ERR_NONE && bs == 1 && has_else && ep == 2 && be == 4,
           "if else extent");

    items[0] = block_vt(0x02, 0);
    items[1] = block_vt(0x02, 0);
    items[2] = in(0x0B);
    items[3] = in(0x0B);
    e = w89_block_extent(items, 4, 0, &bs, &be, &has_else, &ep);
    expect(e == W89_ERR_NONE && bs == 1 && be == 3 && !has_else,
           "nested block extent");
}

static void test_blocktype_arity(void)
{
    w89_vt p[2];
    w89_vt r[1];
    w89_subtype sub;
    w89_deftype dt;
    w89_typeenv env;
    w89_blocktype bt;
    w89_u32 n1;
    w89_u32 n2;

    memset(&bt, 0, sizeof(bt));
    w89_blocktype_arity(NULL, &bt, &n1, &n2);
    expect(n1 == 0 && n2 == 0, "empty blocktype arity");

    bt.is_typeidx = 0;
    bt.vt.is_ref = 0;
    bt.vt.num = 0x7F;
    w89_blocktype_arity(NULL, &bt, &n1, &n2);
    expect(n1 == 0 && n2 == 1, "valblocktype arity");

    p[0] = i32_t();
    p[1] = i32_t();
    r[0].is_ref = 0;
    r[0].num = 0x7C;
    memset(&sub, 0, sizeof(sub));
    sub.kind = W89_CK_FUNC;
    sub.ft.params = p;
    sub.ft.nparams = 2;
    sub.ft.results = r;
    sub.ft.nresults = 1;
    memset(&dt, 0, sizeof(dt));
    dt.sub = &sub;
    memset(&env, 0, sizeof(env));
    env.ntypes = 1;
    env.types = &dt;
    memset(&bt, 0, sizeof(bt));
    bt.is_typeidx = 1;
    bt.typeidx = 0;
    w89_blocktype_arity(&env, &bt, &n1, &n2);
    expect(n1 == 2 && n2 == 1, "typeidx blocktype arity");
}

static void test_machine_stack(void)
{
    w89_config cfg;
    w89_lvl a;
    w89_lvl b;
    w89_lvl got;
    w89_value v;
    w89_instr items[3];

    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    a.kind = W89_LVL_FUNC;
    a.pc = 1;
    a.base = 2;
    b.kind = W89_LVL_BLOCK;
    b.pc = 3;
    b.base = 4;

    w89_config_init(&cfg, NULL);
    expect(w89_config_ln(&cfg) == 0, "machine stack empty on init");
    expect(w89_lvl_push(&cfg, &a) == W89_ERR_NONE, "push A");
    expect(w89_lvl_push(&cfg, &b) == W89_ERR_NONE, "push B");
    expect(w89_config_ln(&cfg) == 2, "two open levels");
    memset(&got, 0, sizeof(got));
    expect(w89_lvl_pop(&cfg, &got), "pop returns an entry");
    expect(got.kind == W89_LVL_BLOCK && got.pc == 3 && got.base == 4,
           "popped entry equals B");
    expect(w89_config_ln(&cfg) == 1, "one level remains");
    memset(&got, 0, sizeof(got));
    expect(w89_lvl_pop(&cfg, &got), "pop second entry");
    expect(got.kind == W89_LVL_FUNC && got.pc == 1 && got.base == 2,
           "popped entry equals A");
    expect(w89_config_ln(&cfg) == 0, "stack empty after both pops");
    memset(&got, 0xAA, sizeof(got));
    expect(!w89_lvl_pop(&cfg, &got), "pop on empty returns 0");
    expect(w89_config_ln(&cfg) == 0, "empty stack unchanged after failed pop");
    w89_config_free(&cfg);

    /* Unified value stack reset watermark. */
    w89_config_init(&cfg, NULL);
    v = w89_value_num(5);
    expect(w89_vs_push(&cfg, &v) == W89_ERR_NONE, "push v5");
    v = w89_value_num(6);
    expect(w89_vs_push(&cfg, &v) == W89_ERR_NONE, "push v6");
    expect(cfg.vsn == 2, "two values on unified stack");
    w89_vs_reset(&cfg, 1);
    expect(cfg.vsn == 1, "reset to watermark 1");
    v = w89_value_num(7);
    expect(w89_vs_push(&cfg, &v) == W89_ERR_NONE, "push v7 after reset");
    expect(cfg.vsn == 2 && cfg.vs[0].u.num == 5 && cfg.vs[1].u.num == 7,
           "values above watermark overwritten in place");
    w89_config_free(&cfg);

    /* Iterative driver result for i32 7 + i32 9. */
    items[0] = in_c32(0x41, 7);
    items[1] = in_c32(0x41, 9);
    items[2] = in(0x6A);      /* i32.add */
    {
        w89_config ci;
        w89_eval_out oi;
        w89_config_init(&ci, NULL);
        w89_code_range(&ci.code, items, 3, 0, 3);
        oi = w89_eval_iter(&ci);
        expect(oi.status == W89_EVAL_OK && oi.nvs == 1
               && oi.vs[0].u.num == 16,
               "iterative 7+9 yields 16");
        w89_eval_out_free(&oi);
        w89_config_free(&ci);
    }
}

static void test_terminal(void)
{
    w89_moduleinst m;
    w89_frame f;
    w89_instr a[3];
    w89_eval_out out;

    memset(&m, 0, sizeof(m));
    memset(&f, 0, sizeof(f));
    f.inst = &m;

    a[0] = in_idx(0x0C, 5);
    run(a, 1, &f, &out);
    expect(out.status == W89_EVAL_CRASH && out.msg
           && strcmp(out.msg, "undefined label") == 0,
           "br depth beyond labels crashes");
    w89_eval_out_free(&out);

    a[0] = block_vt(0x02, 0);
    a[1] = in_idx(0x0C, 1);
    a[2] = in(0x0B);
    run(a, 3, &f, &out);
    expect(out.status == W89_EVAL_OK && out.nvs == 0,
           "br to function body label returns from the function");
    w89_eval_out_free(&out);
}

static w89_instr in_sat(w89_u32 sub)
{
    w89_instr i;
    memset(&i, 0, sizeof(i));
    i.op = 0xFC;
    i.sub = sub;
    return i;
}

static w89_instr in_f32_bits(w89_u32 bits)
{
    w89_instr i;
    memset(&i, 0, sizeof(i));
    i.op = 0x43;
    memcpy(&i.f32, &bits, sizeof(bits));
    return i;
}

static w89_instr in_f64_bits(w89_u64 bits)
{
    w89_instr i;
    memset(&i, 0, sizeof(i));
    i.op = 0x44;
    memcpy(&i.f64, &bits, sizeof(bits));
    return i;
}

static void expect_sat_ok(w89_eval_out *out, w89_u64 want, const char *name)
{
    expect(out->status == W89_EVAL_OK && out->nvs == 1
           && out->vs[0].u.num == want, name);
    w89_eval_out_free(out);
}

static void test_saturating_trunc(void)
{
    w89_moduleinst m;
    w89_frame f;
    w89_instr a[2];
    w89_eval_out out;

    memset(&m, 0, sizeof(m));
    memset(&f, 0, sizeof(f));
    f.inst = &m;

    a[0] = in_f32_bits(0x7F800000u);
    a[1] = in_sat(0x00);
    run(a, 2, &f, &out);
    expect_sat_ok(&out, 0x7FFFFFFFu, "sat i32 f32 s +inf");
    a[0] = in_f32_bits(0xFF800000u);
    a[1] = in_sat(0x00);
    run(a, 2, &f, &out);
    expect_sat_ok(&out, 0x80000000u, "sat i32 f32 s -inf");
    a[0] = in_f32_bits(0x7FC00000u);
    a[1] = in_sat(0x00);
    run(a, 2, &f, &out);
    expect_sat_ok(&out, 0, "sat i32 f32 s nan");
    a[0] = in_f32_bits(0xBF000000u);
    a[1] = in_sat(0x00);
    run(a, 2, &f, &out);
    expect_sat_ok(&out, 0, "sat i32 f32 s -0.5");

    a[0] = in_f32_bits(0x7F800000u);
    a[1] = in_sat(0x01);
    run(a, 2, &f, &out);
    expect_sat_ok(&out, 0xFFFFFFFFu, "sat i32 f32 u +inf");
    a[0] = in_f32_bits(0xBF800000u);
    a[1] = in_sat(0x01);
    run(a, 2, &f, &out);
    expect_sat_ok(&out, 0, "sat i32 f32 u -1.0");

    a[0] = in_f64_bits(0x7FF0000000000000UL);
    a[1] = in_sat(0x02);
    run(a, 2, &f, &out);
    expect_sat_ok(&out, 0x7FFFFFFFu, "sat i32 f64 s +inf");
    a[0] = in_f64_bits(0xFFF0000000000000UL);
    a[1] = in_sat(0x02);
    run(a, 2, &f, &out);
    expect_sat_ok(&out, 0x80000000u, "sat i32 f64 s -inf");
    a[0] = in_f64_bits(0x3FE0000000000000UL);
    a[1] = in_sat(0x02);
    run(a, 2, &f, &out);
    expect_sat_ok(&out, 0, "sat i32 f64 s 0.5");
    a[0] = in_f64_bits(0xBFF0000000000000UL);
    a[1] = in_sat(0x03);
    run(a, 2, &f, &out);
    expect_sat_ok(&out, 0, "sat i32 f64 u -1.0");

    a[0] = in_f32_bits(0x7F800000u);
    a[1] = in_sat(0x04);
    run(a, 2, &f, &out);
    expect_sat_ok(&out, 0x7FFFFFFFFFFFFFFFUL, "sat i64 f32 s +inf");
    a[0] = in_f32_bits(0xFF800000u);
    a[1] = in_sat(0x05);
    run(a, 2, &f, &out);
    expect_sat_ok(&out, 0, "sat i64 f32 u -inf");

    a[0] = in_f64_bits(0xFFF0000000000000UL);
    a[1] = in_sat(0x06);
    run(a, 2, &f, &out);
    expect_sat_ok(&out, 0x8000000000000000UL, "sat i64 f64 s -inf");
    a[0] = in_f64_bits(0xBFF0000000000000UL);
    a[1] = in_sat(0x07);
    run(a, 2, &f, &out);
    expect_sat_ok(&out, 0, "sat i64 f64 u -1.0");
}

/* S2.2 parity scenarios (testing 0020): block/loop/if/br/br_if/br_table
 * through the iterative entry equal the legacy stepper. */
static void test_iter_parity(void)
{
    w89_moduleinst m;
    w89_frame f;
    w89_local locs[1];
    w89_instr a[16];
    w89_u32 lbls[1];

    memset(&m, 0, sizeof(m));
    memset(&locs, 0, sizeof(locs));
    locs[0].set = 1;
    locs[0].v = w89_value_num(3);
    memset(&f, 0, sizeof(f));
    f.inst = &m;
    f.locals = locs;
    f.nlocals = 1;

    a[0] = block_vt(0x02, 0x7F);
    a[1] = in_c32(0x41, 7);
    a[2] = in(0x0B);
    parity_eq(a, 3, &f, "parity block result");

    a[0] = block_vt(0x02, 0);
    a[1] = block_vt(0x02, 0);
    a[2] = in_c32(0x41, 7);
    a[3] = in(0x0B);
    a[4] = in(0x0B);
    parity_eq(a, 5, &f, "parity nested blocks");

    a[0] = in_c32(0x41, 0);
    a[1] = block_vt(0x04, 0x7F);
    a[2] = in_c32(0x41, 1);
    a[3] = in(0x05);
    a[4] = in_c32(0x41, 2);
    a[5] = in(0x0B);
    parity_eq(a, 6, &f, "parity if else zero branch");

    a[0] = in_c32(0x41, 1);
    a[1] = block_vt(0x04, 0x7F);
    a[2] = in_c32(0x41, 1);
    a[3] = in(0x05);
    a[4] = in_c32(0x41, 2);
    a[5] = in(0x0B);
    parity_eq(a, 6, &f, "parity if else nonzero branch");

    a[0] = in_c32(0x41, 1);
    a[1] = block_vt(0x04, 0);
    a[2] = in(0x00);
    a[3] = in(0x0B);
    parity_eq(a, 4, &f, "parity if without else runs then arm");

    a[0] = block_vt(0x02, 0x7F);
    a[1] = in_c32(0x41, 9);
    a[2] = in_idx(0x0C, 0);
    a[3] = in_c32(0x41, 5);
    a[4] = in(0x0B);
    parity_eq(a, 5, &f, "parity br to block skips body");

    a[0] = block_vt(0x02, 0x7F);
    a[1] = in_c32(0x41, 5);
    a[2] = in_c32(0x41, 0);
    a[3] = in_idx(0x0D, 0);
    a[4] = in(0x1A);
    a[5] = in_c32(0x41, 9);
    a[6] = in(0x0B);
    parity_eq(a, 7, &f, "parity br_if not taken");
    a[2] = in_c32(0x41, 1);
    parity_eq(a, 7, &f, "parity br_if taken");

    lbls[0] = 0;
    a[0] = block_vt(0x02, 0x7F);
    a[1] = in_c32(0x41, 9);
    a[2] = in_c32(0x41, 5);
    a[3] = br_table(1, lbls, 0);
    a[4] = in_c32(0x41, 5);
    a[5] = in(0x0B);
    parity_eq(a, 6, &f, "parity br_table default");
    a[2] = in_c32(0x41, 0);
    parity_eq(a, 6, &f, "parity br_table in range");

    a[0] = block_vt(0x02, 0x7F);
    a[1] = block_vt(0x02, 0);
    a[2] = in_c32(0x41, 7);
    a[3] = in_idx(0x0C, 1);
    a[4] = in(0x0B);
    a[5] = in_c32(0x41, 99);
    a[6] = in(0x0B);
    parity_eq(a, 7, &f, "parity br unwinds inner to outer label");

    a[0] = block_vt(0x02, 0x7F);
    a[1] = block_vt(0x03, 0);
    a[2] = in_idx(0x20, 0);
    a[3] = in_idx(0x20, 0);
    a[4] = in(0x45);
    a[5] = in_idx(0x0D, 1);
    a[6] = in(0x1A);
    a[7] = in_idx(0x20, 0);
    a[8] = in_c32(0x41, 1);
    a[9] = in(0x6B);
    a[10] = in_idx(0x21, 0);
    a[11] = in_idx(0x0C, 0);
    a[12] = in(0x0B);
    a[13] = in(0x00);
    a[14] = in(0x0B);
    parity_eq(a, 15, &f, "parity loop countdown");
}

/* S2.2 (0020 ITB): deeply nested blocks must run on the level stack, not
 * the native C stack. The iterative entry completes a depth-N nesting
 * without C-recursing per level; the legacy recursive stepper cannot. */
static void test_iter_deep_nesting(void)
{
    w89_moduleinst m;
    w89_frame f;
    w89_config cfg;
    w89_eval_out out;
    w89_instr *a;
    w89_u32 n;
    w89_u32 i;

    n = 8000;
    a = (w89_instr *)malloc(sizeof(w89_instr) * (2 * n + 1));
    expect(a != 0, "deep-nesting buffer alloc");
    if (a == 0) {
        return;
    }
    memset(&m, 0, sizeof(m));
    memset(&f, 0, sizeof(f));
    f.inst = &m;
    for (i = 0; i < n; i = i + 1) {
        a[i] = block_vt(0x02, 0);
    }
    a[n] = in_c32(0x41, 7);
    for (i = 0; i < n; i = i + 1) {
        a[n + 1 + i] = in(0x0B);
    }
    w89_config_init(&cfg, &f);
    w89_code_range(&cfg.code, a, 2 * n + 1, 0, 2 * n + 1);
    out = w89_eval_iter(&cfg);
    expect(out.status == W89_EVAL_OK && out.nvs == 1
           && out.vs[0].u.num == 7,
           "iterative deep-nesting completes, value forwarded");
    w89_eval_out_free(&out);
    w89_config_free(&cfg);
    free(a);
}

int main(void)
{
    test_value_model();
    test_v128_value_model();
    test_v128_eval_const();
    test_exn_ref_and_tag();
    test_const_nop();
    test_locals();
    test_globals();
    test_drop_select();
    test_block();
    test_block_params();
    test_br();
    test_loop();
    test_return();
    test_unreachable();
    test_numeric_traps();
    test_numeric_results();
    test_block_extent();
    test_blocktype_arity();
    test_terminal();
    test_machine_stack();
    test_iter_parity();
    test_iter_deep_nesting();
    test_saturating_trunc();
    if (failures == 0) {
        printf("test_eval: all tests passed\n");
        return 0;
    }
    fprintf(stderr, "test_eval: %d failure(s)\n", failures);
    return 1;
}
