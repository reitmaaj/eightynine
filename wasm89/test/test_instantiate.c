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

/* Minimal wasm binary builder. */

typedef struct {
    w89_byte buf[2048];
    w89_u32 n;
} mbuf;

static void mb_byte(mbuf *b, w89_byte x)
{
    b->buf[b->n++] = x;
}

static void mb_u32(mbuf *b, w89_u32 v)
{
    do {
        w89_byte byte = (w89_byte)(v & 0x7F);
        v >>= 7;
        if (v) {
            byte |= 0x80;
        }
        mb_byte(b, byte);
    } while (v);
}

static void mb_bytes(mbuf *b, const w89_byte *p, w89_u32 n)
{
    w89_u32 i;
    for (i = 0; i < n; i++) {
        mb_byte(b, p[i]);
    }
}

static void mb_header(mbuf *b)
{
    static const w89_byte magic[8] = { 0, 0x61, 0x73, 0x6D, 1, 0, 0, 0 };
    mb_bytes(b, magic, 8);
}

static void mb_section(mbuf *b, w89_byte id, const mbuf *content)
{
    mb_byte(b, id);
    mb_u32(b, content->n);
    mb_bytes(b, content->buf, content->n);
}

static void mb_name(mbuf *b, const char *s)
{
    mb_u32(b, (w89_u32)strlen(s));
    mb_bytes(b, (const w89_byte *)s, (w89_u32)strlen(s));
}

static void mb_valtype(mbuf *b, w89_byte t)
{
    mb_byte(b, t);
}

static void mb_functype(mbuf *b, const w89_byte *params, w89_u32 np,
                        const w89_byte *results, w89_u32 nr)
{
    mb_byte(b, 0x60);
    mb_u32(b, np);
    {
        w89_u32 i;
        for (i = 0; i < np; i++) {
            mb_valtype(b, params[i]);
        }
    }
    mb_u32(b, nr);
    {
        w89_u32 i;
        for (i = 0; i < nr; i++) {
            mb_valtype(b, results[i]);
        }
    }
}

static void mb_typesec(mbuf *b, const mbuf *functypes, w89_u32 n)
{
    mbuf c;
    w89_u32 i;
    memset(&c, 0, sizeof(c));
    mb_u32(&c, n);
    for (i = 0; i < n; i++) {
        mb_bytes(&c, functypes[i].buf, functypes[i].n);
    }
    mb_section(b, 0x01, &c);
}

static void mb_funcsec(mbuf *b, const w89_u32 *typeidx, w89_u32 n)
{
    mbuf c;
    w89_u32 i;
    memset(&c, 0, sizeof(c));
    mb_u32(&c, n);
    for (i = 0; i < n; i++) {
        mb_u32(&c, typeidx[i]);
    }
    mb_section(b, 0x03, &c);
}

static void mb_export(mbuf *b, const char *name, w89_byte kind, w89_u32 idx)
{
    mb_name(b, name);
    mb_byte(b, kind);
    mb_u32(b, idx);
}

static void mb_exportsec(mbuf *b, const mbuf *exports, w89_u32 n)
{
    mbuf c;
    w89_u32 i;
    memset(&c, 0, sizeof(c));
    mb_u32(&c, n);
    for (i = 0; i < n; i++) {
        mb_bytes(&c, exports[i].buf, exports[i].n);
    }
    mb_section(b, 0x07, &c);
}

static void mb_code(mbuf *b, const w89_byte *instrs, w89_u32 n)
{
    mbuf body;
    memset(&body, 0, sizeof(body));
    mb_byte(&body, 0);
    mb_bytes(&body, instrs, n);
    mb_byte(&body, 0x0B);
    mb_u32(b, body.n);
    mb_bytes(b, body.buf, body.n);
}

static void mb_codesec(mbuf *b, const mbuf *bodies, w89_u32 n)
{
    mbuf c;
    w89_u32 i;
    memset(&c, 0, sizeof(c));
    mb_u32(&c, n);
    for (i = 0; i < n; i++) {
        mb_code(&c, bodies[i].buf, bodies[i].n);
    }
    mb_section(b, 0x0A, &c);
}

static void mb_globaltype(mbuf *b, w89_byte vt, w89_byte mut)
{
    mb_valtype(b, vt);
    mb_byte(b, mut);
}

static void mb_globalsec(mbuf *b, const mbuf *globals, w89_u32 n)
{
    mbuf c;
    w89_u32 i;
    memset(&c, 0, sizeof(c));
    mb_u32(&c, n);
    for (i = 0; i < n; i++) {
        mb_bytes(&c, globals[i].buf, globals[i].n);
    }
    mb_section(b, 0x06, &c);
}

static void mb_table(mbuf *b, w89_u32 min, w89_u32 max, int has_max)
{
    mb_byte(b, 0x70);
    mb_byte(b, 0x00);
    mb_u32(b, min);
    if (has_max) {
        mb_byte(b, 0x01);
        mb_u32(b, max);
    }
}

static void mb_tablesec(mbuf *b, const mbuf *tables, w89_u32 n)
{
    mbuf c;
    w89_u32 i;
    memset(&c, 0, sizeof(c));
    mb_u32(&c, n);
    for (i = 0; i < n; i++) {
        mb_bytes(&c, tables[i].buf, tables[i].n);
    }
    mb_section(b, 0x04, &c);
}

static void mb_memory(mbuf *b, w89_u32 min, w89_u32 max, int has_max)
{
    mb_byte(b, 0x00);
    mb_u32(b, min);
    if (has_max) {
        mb_byte(b, 0x01);
        mb_u32(b, max);
    }
}

static void mb_memsec(mbuf *b, const mbuf *mems, w89_u32 n)
{
    mbuf c;
    w89_u32 i;
    memset(&c, 0, sizeof(c));
    mb_u32(&c, n);
    for (i = 0; i < n; i++) {
        mb_bytes(&c, mems[i].buf, mems[i].n);
    }
    mb_section(b, 0x05, &c);
}

static void mb_startsec(mbuf *b, w89_u32 funcidx)
{
    mbuf c;
    memset(&c, 0, sizeof(c));
    mb_u32(&c, funcidx);
    mb_section(b, 0x08, &c);
}

static void mb_datassec(mbuf *b, const mbuf *datas, w89_u32 n)
{
    mbuf c;
    w89_u32 i;
    memset(&c, 0, sizeof(c));
    mb_u32(&c, n);
    for (i = 0; i < n; i++) {
        mb_bytes(&c, datas[i].buf, datas[i].n);
    }
    mb_section(b, 0x0B, &c);
}

static void mb_elemssec(mbuf *b, const mbuf *elems, w89_u32 n)
{
    mbuf c;
    w89_u32 i;
    memset(&c, 0, sizeof(c));
    mb_u32(&c, n);
    for (i = 0; i < n; i++) {
        mb_bytes(&c, elems[i].buf, elems[i].n);
    }
    mb_section(b, 0x09, &c);
}

static void mb_import(mbuf *b, const char *mod, const char *name,
                      const mbuf *desc)
{
    mb_name(b, mod);
    mb_name(b, name);
    mb_bytes(b, desc->buf, desc->n);
}

static void mb_importsec(mbuf *b, const mbuf *imports, w89_u32 n)
{
    mbuf c;
    w89_u32 i;
    memset(&c, 0, sizeof(c));
    mb_u32(&c, n);
    for (i = 0; i < n; i++) {
        mb_bytes(&c, imports[i].buf, imports[i].n);
    }
    mb_section(b, 0x02, &c);
}

/* Helper: decode + instantiate a module binary. The decoded module and
 * its buffer are transferred to the store, which frees them. */
static w89_err instantiate_bytes(w89_store *s, const w89_registry *reg,
                                 const w89_byte *bin, w89_u32 len,
                                 w89_moduleinst **out)
{
    w89_module *m;
    w89_byte *buf;
    w89_err e;
    *out = NULL;
    buf = malloc(len > 0 ? (size_t)len : 1);
    if (buf == NULL) {
        return W89_ERR_OUT_OF_MEMORY;
    }
    memcpy(buf, bin, (size_t)len);
    m = malloc(sizeof(w89_module));
    if (m == NULL) {
        free(buf);
        return W89_ERR_OUT_OF_MEMORY;
    }
    w89_module_init(m);
    e = w89_module_decode(buf, len, m);
    if (e == W89_ERR_NONE) {
        e = w89_module_validate(m);
    }
    if (e == W89_ERR_NONE) {
        e = w89_instantiate(s, m, reg, out);
    }
    w89_store_own_module(s, m, buf);
    return e;
}

static w89_funcinst *export_func(w89_moduleinst *inst, const char *name)
{
    w89_name n;
    w89_externinst ext;
    n.bytes = (const w89_byte *)name;
    n.len = (w89_u32)strlen(name);
    if (!w89_find_export(inst, &n, &ext)) {
        return NULL;
    }
    return ext.u.func;
}

static w89_globalinst *export_global(w89_moduleinst *inst, const char *name)
{
    w89_name n;
    w89_externinst ext;
    n.bytes = (const w89_byte *)name;
    n.len = (w89_u32)strlen(name);
    if (!w89_find_export(inst, &n, &ext)) {
        return NULL;
    }
    return ext.u.global;
}

static void test_empty_module(void)
{
    mbuf m;
    w89_store s;
    w89_registry reg;
    w89_moduleinst *inst;
    w89_err e;
    memset(&m, 0, sizeof(m));
    mb_header(&m);
    w89_store_init(&s);
    w89_registry_init(&reg);
    e = instantiate_bytes(&s, &reg, m.buf, m.n, &inst);
    expect(e == W89_ERR_NONE && inst != NULL && inst->nexports == 0,
           "empty module instantiates");
    w89_store_free(&s);
    w89_registry_free(&reg);
}

static void test_func_export_call(void)
{
    mbuf m;
    mbuf t0;
    mbuf ex;
    mbuf body;
    w89_store s;
    w89_registry reg;
    w89_moduleinst *inst;
    w89_funcinst *f;
    w89_value arg;
    w89_eval_out out;
    w89_err e;
    static const w89_byte p[1] = { 0x7F };
    static const w89_byte r[1] = { 0x7F };
    static const w89_byte instrs[5] = { 0x20, 0x00, 0x41, 0x01, 0x6A };
    static const w89_u32 tyidx[1] = { 0 };

    memset(&m, 0, sizeof(m));
    mb_header(&m);
    memset(&t0, 0, sizeof(t0));
    mb_functype(&t0, p, 1, r, 1);
    mb_typesec(&m, &t0, 1);
    mb_funcsec(&m, tyidx, 1);
    memset(&ex, 0, sizeof(ex));
    mb_export(&ex, "add", 0x00, 0);
    mb_exportsec(&m, &ex, 1);
    memset(&body, 0, sizeof(body));
    mb_bytes(&body, instrs, 5);
    mb_codesec(&m, &body, 1);

    w89_store_init(&s);
    w89_registry_init(&reg);
    e = instantiate_bytes(&s, &reg, m.buf, m.n, &inst);
    expect(e == W89_ERR_NONE, "func module instantiates");
    if (e == W89_ERR_NONE) {
        f = export_func(inst, "add");
        expect(f != NULL, "func export resolves");
        arg = w89_value_num(5);
        out = w89_invoke(&s, f, &arg, 1);
        expect(out.status == W89_EVAL_OK && out.nvs == 1
               && out.vs[0].u.num == 6, "invoke exported add(5) == 6");
        w89_eval_out_free(&out);
    }
    w89_store_free(&s);
    w89_registry_free(&reg);
}

static void test_global_start(void)
{
    mbuf m;
    mbuf t0;
    mbuf ex;
    mbuf glob;
    mbuf body;
    w89_store s;
    w89_registry reg;
    w89_moduleinst *inst;
    w89_globalinst *g;
    w89_err e;
    static const w89_u32 tyidx[1] = { 0 };
    static const w89_byte instrs[7] = { 0x23, 0x00, 0x41, 0x01, 0x6A,
                                        0x24, 0x00 };
    static const w89_byte ginit[3] = { 0x41, 0x00, 0x0B };

    memset(&m, 0, sizeof(m));
    mb_header(&m);
    memset(&t0, 0, sizeof(t0));
    mb_functype(&t0, NULL, 0, NULL, 0);
    mb_typesec(&m, &t0, 1);
    mb_funcsec(&m, tyidx, 1);
    memset(&glob, 0, sizeof(glob));
    mb_globaltype(&glob, 0x7F, 0x01);
    mb_bytes(&glob, ginit, 3);
    mb_globalsec(&m, &glob, 1);
    memset(&ex, 0, sizeof(ex));
    mb_export(&ex, "g", 0x03, 0);
    mb_exportsec(&m, &ex, 1);
    mb_startsec(&m, 0);
    memset(&body, 0, sizeof(body));
    mb_bytes(&body, instrs, 7);
    mb_codesec(&m, &body, 1);

    w89_store_init(&s);
    w89_registry_init(&reg);
    e = instantiate_bytes(&s, &reg, m.buf, m.n, &inst);
    expect(e == W89_ERR_NONE, "start module instantiates");
    if (e == W89_ERR_NONE) {
        g = export_global(inst, "g");
        expect(g != NULL && g->value.u.num == 1,
               "start function ran (global == 1)");
    }
    w89_store_free(&s);
    w89_registry_free(&reg);
}

static void test_table_elem_call_indirect(void)
{
    mbuf m;
    mbuf t0;
    mbuf t1;
    mbuf t2;
    mbuf ex;
    mbuf tab;
    mbuf elem;
    mbuf body0;
    mbuf body1;
    mbuf body2;
    w89_store s;
    w89_registry reg;
    w89_moduleinst *inst;
    w89_funcinst *f;
    w89_value arg;
    w89_eval_out out;
    w89_err e;
    static const w89_byte r[1] = { 0x7F };
    static const w89_byte p[1] = { 0x7F };
    static const w89_u32 tyidx[3] = { 0, 0, 2 };
    static const w89_byte b0[2] = { 0x41, 0x01 };
    static const w89_byte b1[2] = { 0x41, 0x02 };
    static const w89_byte b2[5] = { 0x20, 0x00, 0x11, 0x00, 0x00 };

    memset(&m, 0, sizeof(m));
    mb_header(&m);
    memset(&t0, 0, sizeof(t0));
    mb_functype(&t0, NULL, 0, r, 1);
    memset(&t1, 0, sizeof(t1));
    mb_functype(&t1, NULL, 0, r, 1);
    memset(&t2, 0, sizeof(t2));
    mb_functype(&t2, p, 1, r, 1);
    {
        mbuf types[3];
        memcpy(&types[0], &t0, sizeof(t0));
        memcpy(&types[1], &t1, sizeof(t1));
        memcpy(&types[2], &t2, sizeof(t2));
        mb_typesec(&m, types, 3);
    }
    mb_funcsec(&m, tyidx, 3);
    memset(&tab, 0, sizeof(tab));
    mb_table(&tab, 2, 0, 0);
    mb_tablesec(&m, &tab, 1);
    memset(&ex, 0, sizeof(ex));
    mb_export(&ex, "call", 0x00, 2);
    mb_exportsec(&m, &ex, 1);
    memset(&elem, 0, sizeof(elem));
    mb_byte(&elem, 0x00);
    {
        static const w89_byte eo[2] = { 0x41, 0x00 };
        mb_bytes(&elem, eo, 2);
    }
    mb_byte(&elem, 0x0B);
    mb_u32(&elem, 2);
    mb_u32(&elem, 0);
    mb_u32(&elem, 1);
    mb_elemssec(&m, &elem, 1);
    memset(&body0, 0, sizeof(body0));
    mb_bytes(&body0, b0, 2);
    memset(&body1, 0, sizeof(body1));
    mb_bytes(&body1, b1, 2);
    memset(&body2, 0, sizeof(body2));
    mb_bytes(&body2, b2, 5);
    {
        mbuf bodies[3];
        memcpy(&bodies[0], &body0, sizeof(body0));
        memcpy(&bodies[1], &body1, sizeof(body1));
        memcpy(&bodies[2], &body2, sizeof(body2));
        mb_codesec(&m, bodies, 3);
    }

    w89_store_init(&s);
    w89_registry_init(&reg);
    e = instantiate_bytes(&s, &reg, m.buf, m.n, &inst);
    if (e == W89_ERR_NONE) {
        f = export_func(inst, "call");
        arg = w89_value_num(0);
        out = w89_invoke(&s, f, &arg, 1);
        expect(out.status == W89_EVAL_OK && out.nvs == 1
               && out.vs[0].u.num == 1, "call_indirect via elem slot 0");
        w89_eval_out_free(&out);
        arg = w89_value_num(1);
        out = w89_invoke(&s, f, &arg, 1);
        expect(out.status == W89_EVAL_OK && out.nvs == 1
               && out.vs[0].u.num == 2, "call_indirect via elem slot 1");
        w89_eval_out_free(&out);
    }
    w89_store_free(&s);
    w89_registry_free(&reg);
}

/* S2.7: an active element segment whose slots are element expressions
 * (ref.null + ref.func) must bind the ref.func slot to its function, even
 * when the ref.func is not slot 0 of a multi-slot segment. Regression for
 * the iterative-driver const/elem slice bug (elem.wast:175 slot 7). */
static void test_elem_expr_ref_func_later_slot(void)
{
    mbuf m;
    mbuf t0;
    mbuf ex;
    mbuf tab;
    mbuf elem;
    mbuf b0;
    mbuf b1;
    w89_store s;
    w89_registry reg;
    w89_moduleinst *inst;
    w89_funcinst *f;
    w89_eval_out out;
    w89_err e;
    static const w89_byte r[1] = { 0x7F };
    static const w89_u32 tyidx[2] = { 0, 0 };
    static const w89_byte b0b[3] = { 0x41, 0xC1, 0x00 };
    static const w89_byte b1b[5] = { 0x41, 0x07, 0x11, 0x00, 0x00 };

    memset(&m, 0, sizeof(m));
    mb_header(&m);
    memset(&t0, 0, sizeof(t0));
    mb_functype(&t0, NULL, 0, r, 1);
    mb_typesec(&m, &t0, 1);
    mb_funcsec(&m, tyidx, 2);
    memset(&tab, 0, sizeof(tab));
    mb_table(&tab, 11, 0, 0);
    mb_tablesec(&m, &tab, 1);
    memset(&ex, 0, sizeof(ex));
    mb_export(&ex, "call-7", 0x00, 1);
    mb_exportsec(&m, &ex, 1);
    /* elem expr form (flags 4): active table 0, offset 6, 2 exprs:
     * ref.null func at slot 6, ref.func 0 at slot 7. */
    memset(&elem, 0, sizeof(elem));
    mb_byte(&elem, 0x04);
    mb_byte(&elem, 0x41);
    mb_byte(&elem, 0x06);
    mb_byte(&elem, 0x0B);
    mb_u32(&elem, 2);
    mb_byte(&elem, 0xD0);
    mb_byte(&elem, 0x70);
    mb_byte(&elem, 0x0B);
    mb_byte(&elem, 0xD2);
    mb_byte(&elem, 0x00);
    mb_byte(&elem, 0x0B);
    mb_elemssec(&m, &elem, 1);
    memset(&b0, 0, sizeof(b0));
    mb_bytes(&b0, b0b, 3);
    memset(&b1, 0, sizeof(b1));
    mb_bytes(&b1, b1b, 5);
    {
        mbuf bodies[2];
        memcpy(&bodies[0], &b0, sizeof(bodies[0]));
        memcpy(&bodies[1], &b1, sizeof(bodies[1]));
        mb_codesec(&m, bodies, 2);
    }

    w89_store_init(&s);
    w89_registry_init(&reg);
    e = instantiate_bytes(&s, &reg, m.buf, m.n, &inst);
    expect(e == W89_ERR_NONE,
           "expr-form elem with ref.func in later slot instantiates");
    if (e == W89_ERR_NONE) {
        f = export_func(inst, "call-7");
        out = w89_invoke(&s, f, NULL, 0);
        expect(out.status == W89_EVAL_OK && out.nvs == 1
               && out.vs[0].u.num == 65,
               "call_indirect to ref.func slot 7 reaches the function");
        w89_eval_out_free(&out);
    }
    w89_store_free(&s);
    w89_registry_free(&reg);
}

static void test_memory_data(void)
{
    mbuf m;
    mbuf t0;
    mbuf ex;
    mbuf mem;
    mbuf data;
    mbuf body;
    w89_store s;
    w89_registry reg;
    w89_moduleinst *inst;
    w89_funcinst *f;
    w89_value arg;
    w89_eval_out out;
    w89_err e;
    static const w89_byte p[1] = { 0x7F };
    static const w89_byte r[1] = { 0x7F };
    static const w89_u32 tyidx[1] = { 0 };
    static const w89_byte instrs[5] = { 0x20, 0x00, 0x2D, 0x00, 0x00 };

    memset(&m, 0, sizeof(m));
    mb_header(&m);
    memset(&t0, 0, sizeof(t0));
    mb_functype(&t0, p, 1, r, 1);
    mb_typesec(&m, &t0, 1);
    mb_funcsec(&m, tyidx, 1);
    memset(&mem, 0, sizeof(mem));
    mb_memory(&mem, 1, 0, 0);
    mb_memsec(&m, &mem, 1);
    memset(&ex, 0, sizeof(ex));
    mb_export(&ex, "load", 0x00, 0);
    mb_exportsec(&m, &ex, 1);
    memset(&body, 0, sizeof(body));
    mb_bytes(&body, instrs, 5);
    mb_codesec(&m, &body, 1);
    memset(&data, 0, sizeof(data));
    mb_byte(&data, 0x00);
    {
        static const w89_byte off[3] = { 0x41, 0x00, 0x0B };
        mb_bytes(&data, off, 3);
    }
    mb_u32(&data, 2);
    mb_byte(&data, 0x41);
    mb_byte(&data, 0x42);
    mb_datassec(&m, &data, 1);

    w89_store_init(&s);
    w89_registry_init(&reg);
    e = instantiate_bytes(&s, &reg, m.buf, m.n, &inst);
    expect(e == W89_ERR_NONE, "memory/data module instantiates");
    if (e == W89_ERR_NONE) {
        f = export_func(inst, "load");
        arg = w89_value_num(0);
        out = w89_invoke(&s, f, &arg, 1);
        expect(out.status == W89_EVAL_OK && out.nvs == 1
               && out.vs[0].u.num == 0x41, "active data at offset 0");
        w89_eval_out_free(&out);
        arg = w89_value_num(1);
        out = w89_invoke(&s, f, &arg, 1);
        expect(out.status == W89_EVAL_OK && out.nvs == 1
               && out.vs[0].u.num == 0x42, "active data at offset 1");
        w89_eval_out_free(&out);
    }
    w89_store_free(&s);
    w89_registry_free(&reg);
}

static void test_spectest_import(void)
{
    mbuf m;
    mbuf t0;
    mbuf imp;
    mbuf ex;
    mbuf body;
    w89_store s;
    w89_registry reg;
    w89_moduleinst *spectest;
    w89_moduleinst *inst;
    w89_funcinst *f;
    w89_eval_out out;
    w89_err e;
    static const w89_byte r[1] = { 0x7F };
    static const w89_u32 tyidx[1] = { 0 };

    memset(&m, 0, sizeof(m));
    mb_header(&m);
    memset(&t0, 0, sizeof(t0));
    mb_functype(&t0, NULL, 0, r, 1);
    mb_typesec(&m, &t0, 1);
    memset(&imp, 0, sizeof(imp));
    {
        mbuf desc;
        memset(&desc, 0, sizeof(desc));
        mb_byte(&desc, 0x03);
        mb_byte(&desc, 0x7F);
        mb_byte(&desc, 0x00);
        mb_import(&imp, "spectest", "global_i32", &desc);
    }
    mb_importsec(&m, &imp, 1);
    mb_funcsec(&m, tyidx, 1);
    memset(&ex, 0, sizeof(ex));
    mb_export(&ex, "get", 0x00, 0);
    mb_exportsec(&m, &ex, 1);
    memset(&body, 0, sizeof(body));
    mb_bytes(&body, (const w89_byte *)"\x23\x00", 2);
    mb_codesec(&m, &body, 1);

    w89_store_init(&s);
    w89_registry_init(&reg);
    spectest = w89_spectest(&s);
    expect(spectest != NULL, "spectest module builds");
    if (spectest != NULL) {
        w89_register(&reg, "spectest", spectest);
        e = instantiate_bytes(&s, &reg, m.buf, m.n, &inst);
        expect(e == W89_ERR_NONE, "spectest import instantiates");
        if (e == W89_ERR_NONE) {
            f = export_func(inst, "get");
            out = w89_invoke(&s, f, NULL, 0);
            expect(out.status == W89_EVAL_OK && out.nvs == 1
                   && out.vs[0].u.num == 666, "spectest global_i32 == 666");
            w89_eval_out_free(&out);
        }
    }
    w89_store_free(&s);
    w89_registry_free(&reg);
}

static void test_import_mismatch(void)
{
    mbuf m;
    w89_store s;
    w89_registry reg;
    w89_moduleinst *inst;
    w89_moduleinst *spectest;
    w89_err e;
    memset(&m, 0, sizeof(m));
    mb_header(&m);
    {
        mbuf imp;
        mbuf desc;
        memset(&imp, 0, sizeof(imp));
        memset(&desc, 0, sizeof(desc));
        mb_byte(&desc, 0x02);
        mb_byte(&desc, 0x00);
        mb_u32(&desc, 1);
        mb_import(&imp, "spectest", "global_i32", &desc);
        mb_importsec(&m, &imp, 1);
    }
    w89_store_init(&s);
    w89_registry_init(&reg);
    spectest = w89_spectest(&s);
    if (spectest != NULL) {
        w89_register(&reg, "spectest", spectest);
        e = instantiate_bytes(&s, &reg, m.buf, m.n, &inst);
        expect(e == W89_ERR_INVALID
               && strstr(w89_instantiate_message(),
                         "incompatible import type") != NULL,
               "import kind mismatch rejected");
    }
    w89_store_free(&s);
    w89_registry_free(&reg);
}

static void test_unknown_import(void)
{
    mbuf m;
    mbuf t0;
    mbuf imp;
    w89_store s;
    w89_registry reg;
    w89_moduleinst *inst;
    w89_err e;
    static const w89_byte r[1] = { 0x7F };

    memset(&m, 0, sizeof(m));
    mb_header(&m);
    memset(&t0, 0, sizeof(t0));
    mb_functype(&t0, NULL, 0, r, 1);
    mb_typesec(&m, &t0, 1);
    memset(&imp, 0, sizeof(imp));
    {
        mbuf desc;
        memset(&desc, 0, sizeof(desc));
        mb_byte(&desc, 0x00);
        mb_u32(&desc, 0);
        mb_import(&imp, "nope", "x", &desc);
    }
    mb_importsec(&m, &imp, 1);

    w89_store_init(&s);
    w89_registry_init(&reg);
    e = instantiate_bytes(&s, &reg, m.buf, m.n, &inst);
    expect(e == W89_ERR_INVALID
           && strstr(w89_instantiate_message(), "unknown import") != NULL,
           "unknown import rejected");
    w89_store_free(&s);
    w89_registry_free(&reg);
}

static void test_register_import(void)
{
    mbuf ma;
    mbuf mb;
    mbuf t0;
    mbuf ex;
    mbuf glob;
    mbuf imp;
    mbuf body;
    w89_store s;
    w89_registry reg;
    w89_moduleinst *a;
    w89_moduleinst *b;
    w89_funcinst *f;
    w89_eval_out out;
    w89_err e;
    static const w89_byte r[1] = { 0x7F };
    static const w89_u32 tyidx[1] = { 0 };
    static const w89_byte ginit[3] = { 0x41, 0x05, 0x0B };

    memset(&ma, 0, sizeof(ma));
    mb_header(&ma);
    memset(&glob, 0, sizeof(glob));
    mb_globaltype(&glob, 0x7F, 0x00);
    mb_bytes(&glob, ginit, 3);
    mb_globalsec(&ma, &glob, 1);
    memset(&ex, 0, sizeof(ex));
    mb_export(&ex, "g", 0x03, 0);
    mb_exportsec(&ma, &ex, 1);

    memset(&mb, 0, sizeof(mb));
    mb_header(&mb);
    memset(&t0, 0, sizeof(t0));
    mb_functype(&t0, NULL, 0, r, 1);
    mb_typesec(&mb, &t0, 1);
    memset(&imp, 0, sizeof(imp));
    {
        mbuf desc;
        memset(&desc, 0, sizeof(desc));
        mb_byte(&desc, 0x03);
        mb_byte(&desc, 0x7F);
        mb_byte(&desc, 0x00);
        mb_import(&imp, "A", "g", &desc);
    }
    mb_importsec(&mb, &imp, 1);
    mb_funcsec(&mb, tyidx, 1);
    memset(&ex, 0, sizeof(ex));
    mb_export(&ex, "getg", 0x00, 0);
    mb_exportsec(&mb, &ex, 1);
    memset(&body, 0, sizeof(body));
    mb_bytes(&body, (const w89_byte *)"\x23\x00", 2);
    mb_codesec(&mb, &body, 1);

    w89_store_init(&s);
    w89_registry_init(&reg);
    e = instantiate_bytes(&s, &reg, ma.buf, ma.n, &a);
    expect(e == W89_ERR_NONE, "module A instantiates");
    if (e == W89_ERR_NONE) {
        w89_register(&reg, "A", a);
        e = instantiate_bytes(&s, &reg, mb.buf, mb.n, &b);
        expect(e == W89_ERR_NONE, "module B imports from registered A");
        if (e == W89_ERR_NONE) {
            f = export_func(b, "getg");
            out = w89_invoke(&s, f, NULL, 0);
            expect(out.status == W89_EVAL_OK && out.nvs == 1
                   && out.vs[0].u.num == 5, "imported global value 5");
            w89_eval_out_free(&out);
        }
    }
    w89_store_free(&s);
    w89_registry_free(&reg);
}

static void test_segment_oob(void)
{
    mbuf m;
    mbuf mem;
    mbuf data;
    w89_store s;
    w89_registry reg;
    w89_moduleinst *inst;
    w89_err e;

    memset(&m, 0, sizeof(m));
    mb_header(&m);
    memset(&mem, 0, sizeof(mem));
    mb_memory(&mem, 1, 0, 0);
    mb_memsec(&m, &mem, 1);
    memset(&data, 0, sizeof(data));
    mb_byte(&data, 0x00);
    mb_byte(&data, 0x41);
    mb_byte(&data, 0xFF);
    mb_byte(&data, 0xFF);
    mb_byte(&data, 0x03);
    mb_byte(&data, 0x0B);
    mb_u32(&data, 2);
    mb_byte(&data, 0x41);
    mb_byte(&data, 0x42);
    mb_datassec(&m, &data, 1);

    w89_store_init(&s);
    w89_registry_init(&reg);
    e = instantiate_bytes(&s, &reg, m.buf, m.n, &inst);
    expect(e == W89_ERR_INVALID
           && strstr(w89_instantiate_message(),
                     "out of bounds memory access") != NULL,
           "active data segment past memory rejected");
    w89_store_free(&s);
    w89_registry_free(&reg);
}

static void test_ref_import_match(void)
{
    mbuf ma;
    mbuf mb;
    mbuf mc;
    mbuf t0;
    mbuf exg;
    mbuf ext;
    mbuf glob;
    mbuf tab;
    mbuf body;
    mbuf imp;
    mbuf desc;
    mbuf impbad;
    mbuf descbad;
    w89_store s;
    w89_registry reg;
    w89_moduleinst *a;
    w89_moduleinst *b;
    w89_moduleinst *c;
    w89_err e;
    static const w89_u32 tyidx[1] = { 0 };
    static const w89_byte ginit[6] = { 0x64, 0x00, 0x00, 0xD2, 0x00, 0x0B };
    static const w89_byte tinit[6] = { 0x40, 0x00, 0x64, 0x00, 0x00,
                                       0x01 };
    static const w89_byte texpr[3] = { 0xD2, 0x00, 0x0B };

    /* Module A: type (func); global (ref 0) const (ref.func 0); table
     * (ref 0) 1 init (ref.func 0); exports "g" and "tab". */
    memset(&ma, 0, sizeof(ma));
    mb_header(&ma);
    memset(&t0, 0, sizeof(t0));
    mb_functype(&t0, NULL, 0, NULL, 0);
    mb_typesec(&ma, &t0, 1);
    mb_funcsec(&ma, tyidx, 1);
    memset(&tab, 0, sizeof(tab));
    mb_bytes(&tab, tinit, 6);
    mb_bytes(&tab, texpr, 3);
    mb_tablesec(&ma, &tab, 1);
    memset(&glob, 0, sizeof(glob));
    mb_bytes(&glob, ginit, 6);
    mb_globalsec(&ma, &glob, 1);
    memset(&exg, 0, sizeof(exg));
    mb_export(&exg, "g", 0x03, 0);
    memset(&ext, 0, sizeof(ext));
    mb_export(&ext, "tab", 0x01, 0);
    {
        mbuf exs[2];
        memcpy(&exs[0], &exg, sizeof(exs[0]));
        memcpy(&exs[1], &ext, sizeof(exs[1]));
        mb_exportsec(&ma, exs, 2);
    }
    memset(&body, 0, sizeof(body));
    mb_codesec(&ma, &body, 1);

    /* Module B: imports A.g and A.tab with structurally-equal types. */
    memset(&mb, 0, sizeof(mb));
    mb_header(&mb);
    mb_typesec(&mb, &t0, 1);
    memset(&imp, 0, sizeof(imp));
    memset(&desc, 0, sizeof(desc));
    mb_byte(&desc, 0x03);
    mb_byte(&desc, 0x64);
    mb_byte(&desc, 0x00);
    mb_byte(&desc, 0x00);
    mb_import(&imp, "A", "g", &desc);
    memset(&desc, 0, sizeof(desc));
    mb_byte(&desc, 0x01);
    mb_byte(&desc, 0x64);
    mb_byte(&desc, 0x00);
    mb_byte(&desc, 0x00);
    mb_byte(&desc, 0x01);
    mb_import(&imp, "A", "tab", &desc);
    mb_importsec(&mb, &imp, 2);

    /* Module C: imports A.g as i32 -> incompatible. */
    memset(&mc, 0, sizeof(mc));
    mb_header(&mc);
    mb_typesec(&mc, &t0, 1);
    memset(&impbad, 0, sizeof(impbad));
    memset(&descbad, 0, sizeof(descbad));
    mb_byte(&descbad, 0x03);
    mb_byte(&descbad, 0x7F);
    mb_byte(&descbad, 0x00);
    mb_import(&impbad, "A", "g", &descbad);
    mb_importsec(&mc, &impbad, 1);

    w89_store_init(&s);
    w89_registry_init(&reg);
    e = instantiate_bytes(&s, &reg, ma.buf, ma.n, &a);
    expect(e == W89_ERR_NONE, "ref-typed global/table module instantiates");
    if (e == W89_ERR_NONE) {
        w89_register(&reg, "A", a);
        e = instantiate_bytes(&s, &reg, mb.buf, mb.n, &b);
        expect(e == W89_ERR_NONE,
               "ref-typed global/table import matches across modules");
        e = instantiate_bytes(&s, &reg, mc.buf, mc.n, &c);
        expect(e == W89_ERR_INVALID
               && strstr(w89_instantiate_message(),
                         "incompatible import type") != NULL,
               "mismatched ref-typed import rejected");
    }
    w89_store_free(&s);
    w89_registry_free(&reg);
}

static void test_declarative_elem_drop(void)
{
    mbuf m;
    mbuf t0;
    mbuf tab;
    mbuf elem;
    mbuf body;
    mbuf ex;
    w89_store s;
    w89_registry reg;
    w89_moduleinst *inst;
    w89_funcinst *f;
    w89_eval_out out;
    w89_err e;
    static const w89_u32 tyidx[1] = { 0 };
    static const w89_byte tbytes[3] = { 0x70, 0x00, 0x0A };
    static const w89_byte ebytes[4] = { 0x03, 0x00, 0x01, 0x00 };
    static const w89_byte ibytes[10] = { 0x41, 0x00, 0x41, 0x00, 0x41,
                                         0x01, 0xFC, 0x0C, 0x00, 0x00 };

    memset(&m, 0, sizeof(m));
    mb_header(&m);
    memset(&t0, 0, sizeof(t0));
    mb_functype(&t0, NULL, 0, NULL, 0);
    mb_typesec(&m, &t0, 1);
    mb_funcsec(&m, tyidx, 1);
    memset(&tab, 0, sizeof(tab));
    mb_bytes(&tab, tbytes, 3);
    mb_tablesec(&m, &tab, 1);
    memset(&ex, 0, sizeof(ex));
    mb_export(&ex, "init", 0x00, 0);
    mb_exportsec(&m, &ex, 1);
    memset(&elem, 0, sizeof(elem));
    mb_bytes(&elem, ebytes, 4);
    mb_elemssec(&m, &elem, 1);
    memset(&body, 0, sizeof(body));
    mb_bytes(&body, ibytes, 10);
    mb_codesec(&m, &body, 1);

    w89_store_init(&s);
    w89_registry_init(&reg);
    e = instantiate_bytes(&s, &reg, m.buf, m.n, &inst);
    expect(e == W89_ERR_NONE, "declarative-elem module instantiates");
    if (e == W89_ERR_NONE) {
        f = export_func(inst, "init");
        out = w89_invoke(&s, f, NULL, 0);
        expect(out.status == W89_EVAL_TRAP && out.msg
               && strstr(out.msg, "out of bounds table access") != NULL,
               "declarative elem dropped at instantiation (init traps)");
        w89_eval_out_free(&out);
    }
    w89_store_free(&s);
    w89_registry_free(&reg);
}

int main(void)
{
    test_empty_module();
    test_func_export_call();
    test_global_start();
    test_table_elem_call_indirect();
    test_elem_expr_ref_func_later_slot();
    test_memory_data();
    test_spectest_import();
    test_import_mismatch();
    test_unknown_import();
    test_register_import();
    test_segment_oob();
    test_ref_import_match();
    test_declarative_elem_drop();
    if (failures == 0) {
        printf("test_instantiate: all tests passed\n");
        return 0;
    }
    fprintf(stderr, "test_instantiate: %d failure(s)\n", failures);
    return 1;
}
