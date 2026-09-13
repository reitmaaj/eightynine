#ifndef WASM89_MODULE_H
#define WASM89_MODULE_H

#include "leb.h"
#include "numeric.h"

typedef enum w89_valtype {
    W89_T_V128 = 0x7B,
    W89_T_F64 = 0x7C,
    W89_T_F32 = 0x7D,
    W89_T_I64 = 0x7E,
    W89_T_I32 = 0x7F
} w89_valtype;

typedef enum w89_absheaptype {
    W89_HT_EXN = 0x69,
    W89_HT_ARRAY = 0x6A,
    W89_HT_STRUCT = 0x6B,
    W89_HT_I31 = 0x6C,
    W89_HT_EQ = 0x6D,
    W89_HT_ANY = 0x6E,
    W89_HT_EXTERN = 0x6F,
    W89_HT_FUNC = 0x70,
    W89_HT_NONE = 0x71,
    W89_HT_NOEXTERN = 0x72,
    W89_HT_NOFUNC = 0x73,
    W89_HT_NOEXN = 0x74
} w89_absheaptype;

/* 128-bit SIMD value (16 lanes of one byte, lane i at byte i, least
 * significant first). Carried as raw bytes so v128.const, v128.load and
 * v128.store are plain 16-byte copies; wasm memory is little-endian and a
 * vector's lane i occupies memory byte i at the vector's base address. */
typedef struct w89_v128 {
    w89_byte b[16];
} w89_v128;

typedef struct w89_reftype {
    w89_u32 nullable;
    w89_u32 is_typeidx;
    w89_u32 typeidx;
    w89_absheaptype abs;
} w89_reftype;

typedef struct w89_vt {
    w89_u32 is_ref;
    w89_u32 num;
    w89_reftype rt;
} w89_vt;

typedef enum w89_packed {
    W89_PK_I8 = 0x78,
    W89_PK_I16 = 0x77
} w89_packed;

typedef struct w89_fieldtype {
    w89_u32 is_packed;
    w89_packed packed;
    w89_vt vt;
    w89_u32 mut;
} w89_fieldtype;

typedef struct w89_ft {
    w89_vt *params;
    w89_u32 nparams;
    w89_vt *results;
    w89_u32 nresults;
} w89_ft;

typedef enum w89_compkind {
    W89_CK_FUNC = 0,
    W89_CK_STRUCT = 1,
    W89_CK_ARRAY = 2
} w89_compkind;

typedef struct w89_subtype {
    w89_u32 is_final;
    w89_u32 *supertypes;
    w89_u32 nsupers;
    w89_compkind kind;
    w89_ft ft;
    w89_fieldtype *fields;
    w89_u32 nfields;
} w89_subtype;

typedef struct w89_rectype {
    w89_subtype *subtypes;
    w89_u32 n;
} w89_rectype;

typedef struct w89_limits {
    w89_u32 addr64;
    w89_u64 min;
    w89_u64 max;
    w89_u32 has_max;
} w89_limits;

typedef struct w89_tabletype {
    w89_limits limits;
    w89_reftype rt;
} w89_tabletype;

typedef struct w89_globaltype {
    w89_u32 mut;
    w89_vt vt;
} w89_globaltype;

typedef struct w89_name {
    const w89_byte *bytes;
    w89_u32 len;
} w89_name;

typedef struct w89_import {
    w89_name module;
    w89_name name;
    w89_u32 kind;
    w89_u32 typeidx;
    w89_tabletype table;
    w89_limits mem;
    w89_globaltype global;
} w89_import;

typedef struct w89_export {
    w89_name name;
    w89_u32 kind;
    w89_u32 index;
} w89_export;

typedef struct w89_blocktype {
    w89_u32 is_typeidx;
    w89_u32 typeidx;
    w89_vt vt;
} w89_blocktype;

typedef struct w89_catch {
    w89_u32 kind;
    w89_u32 tagidx;
    w89_u32 label;
} w89_catch;

typedef struct w89_instr {
    w89_u32 op;
    w89_u32 sub;
    w89_u32 idx;
    w89_u32 idx2;
    w89_u32 align;
    w89_u64 offset;
    w89_u32 memidx;
    w89_u32 has_memidx;
    w89_blocktype bt;
    w89_reftype rt;
    w89_u32 n;
    w89_u32 *labels;
    w89_vt *seltypes;
    w89_catch *catches;
    w89_i32 c32;
    w89_i64 c64;
    w89_f32 f32;
    w89_f64 f64;
    w89_v128 c128;
} w89_instr;

typedef struct w89_instr_vec {
    w89_instr *items;
    w89_u32 n;
    w89_u32 cap;
} w89_instr_vec;

w89_err w89_decode_expr(const w89_byte *p, w89_u32 window,
                        w89_u32 *consumed, w89_instr_vec *out);
void w89_instr_vec_free(w89_instr_vec *v);

typedef struct w89_func {
    w89_u32 typeidx;
    w89_vt *locals;
    w89_u32 nlocals;
    w89_instr_vec code;
} w89_func;

typedef struct w89_global {
    w89_globaltype type;
    w89_instr_vec init;
} w89_global;

typedef struct w89_table {
    w89_tabletype type;
    w89_instr_vec init;
} w89_table;

typedef struct w89_memory {
    w89_limits type;
} w89_memory;

typedef struct w89_tag {
    w89_u32 typeidx;
} w89_tag;

typedef struct w89_elem {
    w89_u32 flags;
    w89_u32 tableidx;
    w89_reftype rt;
    w89_u32 is_expr;
    w89_instr_vec offset;
    w89_u32 *indices;
    w89_instr_vec exprs;
    w89_u32 n;
} w89_elem;

typedef struct w89_data {
    w89_u32 flags;
    w89_u32 memidx;
    w89_instr_vec offset;
    const w89_byte *bytes;
    w89_u32 len;
} w89_data;

typedef struct w89_module {
    const w89_byte *base;
    w89_u32 size;
    w89_rectype *rectypes;
    w89_u32 nrectypes;
    w89_import *imports;
    w89_u32 nimports;
    w89_u32 *func_types;
    w89_u32 nfuncs;
    w89_table *tables;
    w89_u32 ntables;
    w89_memory *memories;
    w89_u32 nmemories;
    w89_global *globals;
    w89_u32 nglobals;
    w89_export *exports;
    w89_u32 nexports;
    w89_u32 start;
    w89_u32 has_start;
    w89_elem *elems;
    w89_u32 nelems;
    w89_func *funcs;
    w89_u32 ncode;
    w89_data *datas;
    w89_u32 ndatas;
    w89_u32 has_data_count;
    w89_u32 data_count;
    w89_tag *tags;
    w89_u32 ntags;
} w89_module;

void w89_module_init(w89_module *m);
void w89_module_free(w89_module *m);
w89_err w89_module_decode(const w89_byte *buf, w89_u32 len, w89_module *m);
w89_byte w89_last_illegal(void);

#endif
