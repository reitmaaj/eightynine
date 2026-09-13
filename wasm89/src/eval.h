#ifndef WASM89_EVAL_H
#define WASM89_EVAL_H

#include "module.h"
#include "validate.h"

/* Run-time value model (spec ch2.2 + reference interpreter Value module).
 * Numeric values are stored as raw bit patterns in a u64; the concrete
 * numeric type is known from validation, exactly like the reference's
 * untyped Num. References carry one of four kinds. */

typedef enum w89_refkind {
    W89_RK_NULL = 0,
    W89_RK_FUNC,
    W89_RK_EXTERN,
    W89_RK_EXN
} w89_refkind;

typedef struct w89_value w89_value;
typedef struct w89_ref w89_ref;
typedef struct w89_exn w89_exn;
typedef struct w89_taginst w89_taginst;
typedef struct w89_globalinst w89_globalinst;
typedef struct w89_meminst w89_meminst;
typedef struct w89_tableinst w89_tableinst;
typedef struct w89_funcinst w89_funcinst;
typedef struct w89_moduleinst w89_moduleinst;
typedef struct w89_frame w89_frame;
typedef struct w89_local w89_local;
typedef struct w89_datainst w89_datainst;
typedef struct w89_eleminst w89_eleminst;
typedef struct w89_store w89_store;
typedef struct w89_externinst w89_externinst;
typedef struct w89_registry w89_registry;
typedef struct w89_expinst w89_expinst;

typedef enum w89_host_status {
    W89_HOST_OK = 0,
    W89_HOST_TRAP
} w89_host_status;

typedef w89_host_status (*w89_hostfn)(const w89_value *args, w89_u32 nargs,
                                      w89_value *res, w89_u32 *nres,
                                      const char **trap);

struct w89_ref {
    w89_refkind kind;
    union {
        w89_funcinst *func;
        w89_u64 ext;
        w89_exn *exn;
    } u;
};

struct w89_exn {
    w89_taginst *tag;
    w89_value *args;
    w89_u32 nargs;
};

struct w89_value {
    w89_u32 is_ref;
    union {
        w89_u64 num;
        w89_ref ref;
        w89_v128 vec;
    } u;
};

/* Store-side instances. Tag identity is pointer identity: an imported
 * tag resolves to the exporting module's tag instance, which is what
 * makes catch matching work (Phase D). */
struct w89_taginst {
    const w89_ft *ft;
    const w89_typeenv *env;
    w89_u32 typeidx;
};

struct w89_globalinst {
    w89_globaltype type;
    w89_value value;
    const w89_typeenv *types;
};

#define W89_PAGE_SIZE 65536u

struct w89_meminst {
    w89_limits limits;
    w89_byte *bytes;
    w89_u64 npages;
};

struct w89_tableinst {
    w89_tabletype type;
    w89_ref *elems;
    w89_u64 size;
    const w89_typeenv *types;
};

struct w89_funcinst {
    w89_u32 is_host;
    w89_u32 typeidx;
    w89_moduleinst *inst;
    const w89_func *func;
    const w89_ft *ft;
    w89_hostfn host;
};

struct w89_datainst {
    const w89_byte *bytes;
    w89_u32 len;
};

struct w89_eleminst {
    w89_ref *refs;
    w89_u32 n;
};

typedef enum w89_externkind {
    W89_EXT_FUNC = 0,
    W89_EXT_GLOBAL,
    W89_EXT_MEMORY,
    W89_EXT_TABLE,
    W89_EXT_TAG
} w89_externkind;

struct w89_externinst {
    w89_externkind kind;
    union {
        w89_funcinst *func;
        w89_globalinst *global;
        w89_meminst *memory;
        w89_tableinst *table;
        w89_taginst *tag;
    } u;
};

struct w89_expinst {
    w89_name name;
    w89_externkind kind;
    union {
        w89_funcinst *func;
        w89_globalinst *global;
        w89_meminst *memory;
        w89_tableinst *table;
        w89_taginst *tag;
    } u;
};

struct w89_moduleinst {
    w89_store *store;
    const w89_typeenv *types;
    w89_funcinst **funcs;
    w89_u32 nfuncs;
    w89_u32 cfuncs;
    w89_tableinst **tables;
    w89_u32 ntables;
    w89_u32 ctables;
    w89_meminst **memories;
    w89_u32 nmemories;
    w89_u32 cmemories;
    w89_globalinst **globals;
    w89_u32 nglobals;
    w89_u32 cglobals;
    w89_taginst **tags;
    w89_u32 ntags;
    w89_u32 ctags;
    w89_datainst **datas;
    w89_u32 ndatas;
    w89_u32 cdatas;
    w89_eleminst **elems;
    w89_u32 nelems;
    w89_u32 celems;
    w89_expinst *exports;
    w89_u32 nexports;
};

struct w89_store {
    w89_typeenv **tenvs;
    w89_u32 ntenvs;
    w89_u32 ctenvs;
    w89_funcinst **funcs;
    w89_u32 nfuncs;
    w89_u32 cfuncs;
    w89_globalinst **globals;
    w89_u32 nglobals;
    w89_u32 cglobals;
    w89_meminst **mems;
    w89_u32 nmems;
    w89_u32 cmems;
    w89_tableinst **tables;
    w89_u32 ntables;
    w89_u32 ctables;
    w89_taginst **tags;
    w89_u32 ntags;
    w89_u32 ctags;
    w89_datainst **datas;
    w89_u32 ndatas;
    w89_u32 cdatas;
    w89_eleminst **elems;
    w89_u32 nelems;
    w89_u32 celems;
    w89_moduleinst **mods;
    w89_u32 nmods;
    w89_u32 cmods;
    w89_module **decoded;
    w89_u32 ndecoded;
    w89_u32 cdecoded;
    w89_byte **decbufs;
    w89_u32 nbufs;
    w89_u32 cbufs;
    w89_exn **exns;
    w89_u32 nexns;
    w89_u32 cexns;
};

struct w89_registry;

typedef struct w89_regitem {
    char *name;
    w89_moduleinst *inst;
} w89_regitem;

struct w89_registry {
    w89_regitem *items;
    w89_u32 n;
    w89_u32 cap;
};

struct w89_local {
    w89_value v;
    w89_byte set;
};

struct w89_frame {
    w89_moduleinst *inst;
    w89_local *locals;
    w89_u32 nlocals;
};

/* Value/ref constructors (pure helpers). */
w89_value w89_value_num(w89_u64 n);
w89_value w89_value_v128(const w89_v128 *v);
w89_value w89_value_ref(const w89_ref *r);
w89_ref w89_ref_null(void);
w89_ref w89_ref_func(w89_funcinst *f);
w89_ref w89_ref_extern(w89_u64 e);
w89_ref w89_ref_exn(w89_exn *e);
int w89_ref_is_null(const w89_ref *r);
w89_exn *w89_exn_alloc(w89_store *s, w89_taginst *tag, w89_value *args,
                       w89_u32 nargs);

/* Pure control-flow helpers. */
w89_err w89_block_extent(const w89_instr *items, w89_u32 n, w89_u32 i,
                         w89_u32 *body_start, w89_u32 *body_end,
                         w89_u32 *has_else, w89_u32 *else_pos);
void w89_blocktype_arity(const w89_typeenv *env, const w89_blocktype *bt,
                         w89_u32 *nparams, w89_u32 *nresults);

/* Admin instructions: a faithful port of the reference interpreter's
 * admininstr set. Plain instructions borrow the module's decoded flat
 * instruction vector; nested Label/Frame/Handler codes are owned. */
typedef enum w89_akind {
    W89_A_PLAIN = 0,
    W89_A_REFER,
    W89_A_INVOKE,
    W89_A_BREAKING,
    W89_A_RETURNING,
    W89_A_RETINV,
    W89_A_THROWING,
    W89_A_TRAP,
    W89_A_LABEL,
    W89_A_FRAME,
    W89_A_HANDLER
} w89_akind;

typedef struct w89_ainstr w89_ainstr;
typedef struct w89_code w89_code;

struct w89_code {
    w89_value *vs;
    w89_u32 vsn;
    w89_u32 vscap;
    w89_ainstr *front;
    w89_u32 fn;
    w89_u32 fcap;
    w89_ainstr *back;
    w89_u32 bn;
    w89_u32 bcap;
    w89_u32 bhead;
    const w89_instr *src;
    w89_u32 nsrc;
};

struct w89_ainstr {
    w89_akind kind;
    const w89_instr *in;
    w89_u32 ipos;
    w89_ref refer;
    w89_funcinst *finst;
    w89_value *vs0;
    w89_u32 nvs0;
    w89_u32 k;
    w89_taginst *tag;
    w89_u32 n;
    const w89_instr *src;
    w89_u32 nsrc;
    w89_u32 contpos;
    w89_u32 contn;
    w89_u32 skip;
    const w89_catch *catches;
    w89_u32 ncatches;
    w89_frame *frame;
    const char *msg;
    w89_code code;
};

typedef enum w89_lvlkind { W89_LVL_FUNC, W89_LVL_BLOCK } w89_lvlkind;

typedef struct w89_lvl {
    w89_lvlkind kind;
    /* function frame: */
    w89_funcinst *finst;
    w89_frame *frame;
    /* block/label & shared pc over enclosing instr range: */
    const w89_instr *src;
    w89_u32 nsrc;
    w89_u32 pc;
    w89_u32 end;
    w89_u32 contpos;
    w89_u32 contn;
    w89_u32 exit_arity;
    w89_u32 base;
    /* Structured-block input (param) arity n1 (0 for a param-less block). */
    w89_u32 nparams;
    const w89_catch *catches;
    w89_u32 ncatches;
} w89_lvl;

typedef struct w89_config {
    w89_frame *frame;
    w89_code code;
    w89_i64 budget;
    const char *crash;
    w89_byte exhausted;
    /* Explicit, heap-backed machine stack for the iterative driver
     * (design 0007 S2.1). An entry distinguishes a function frame from a
     * structured block; the config owns one level array and one unified
     * value stack shared by all levels. Legacy recursion (c->code) is
     * unaffected and remains the default until features migrate. */
    w89_lvl *lvls;
    w89_u32 ln;
    w89_u32 lcap;
    w89_value *vs;
    w89_u32 vsn;
    w89_u32 vscap;
    /* Result arity of the top-level (level-0) function when driven as a
     * real invocation (S2.3): >0 truncates the level-0 FUNC results to this
     * many values on exit; 0 keeps the harness "leftover" semantics. */
    w89_u32 resn;
} w89_config;

typedef enum w89_step_status {
    W89_STEP_OK = 0,
    W89_STEP_CRASH,
    W89_STEP_EXHAUSTED
} w89_step_status;

typedef enum w89_eval_status {
    W89_EVAL_OK = 0,
    W89_EVAL_TRAP,
    W89_EVAL_EXHAUSTED,
    W89_EVAL_EXCEPTION,
    W89_EVAL_CRASH
} w89_eval_status;

typedef struct w89_eval_out {
    w89_eval_status status;
    const char *msg;
    w89_value *vs;
    w89_u32 nvs;
    w89_taginst *tag;
} w89_eval_out;

void w89_config_init(w89_config *c, w89_frame *frame);
void w89_config_free(w89_config *c);
void w89_code_init(w89_code *code);
void w89_code_free(w89_code *code);
w89_err w89_code_range(w89_code *code, const w89_instr *items, w89_u32 n,
                       w89_u32 start, w89_u32 count);
w89_step_status w89_step(w89_config *c);
void w89_eval_out_free(w89_eval_out *out);

/* Explicit machine-stack container (design 0007 S2.1). lvls[0] is the
 * outermost (oldest) level; ln is the count of open levels. */
w89_err w89_lvl_push(w89_config *c, const w89_lvl *l);
int w89_lvl_pop(w89_config *c, w89_lvl *out);
w89_u32 w89_config_ln(const w89_config *c);
/* Unified value-stack accessors over the config-owned array. */
w89_err w89_vs_push(w89_config *c, const w89_value *v);
void w89_vs_reset(w89_config *c, w89_u32 n);
/* Iterative top-of-stack driver; the sole evaluation path (design 0007
 * S2.6). */
w89_eval_out w89_eval_iter(w89_config *c);

typedef struct w89_externtype {
    w89_externkind kind;
    const w89_typeenv *env;
    const w89_ft *ft;
    w89_u32 typeidx;
    w89_globaltype gt;
    w89_tabletype tt;
    w89_limits mem;
} w89_externtype;

/* Cross-module type matching (generalizes the within-env matching in
 * validate.h to types that live in different module instances). */
int w89_match_deftype_x(const w89_typeenv *ae, w89_u32 a,
                        const w89_typeenv *be, w89_u32 b);
int w89_match_valtype_x(const w89_typeenv *ae, const w89_vt *a,
                        const w89_typeenv *be, const w89_vt *b);
int w89_match_reftype_x(const w89_typeenv *ae, const w89_reftype *a,
                        const w89_typeenv *be, const w89_reftype *b);
int w89_match_limits(const w89_limits *actual, const w89_limits *expected);
int w89_match_externtype(const w89_externtype *a, const w89_externtype *b);

/* Instantiation, imports, invocation (src/instantiate.c). */
void w89_store_init(w89_store *s);
void w89_store_free(w89_store *s);
w89_err w89_store_own_module(w89_store *s, w89_module *m, w89_byte *buf);
void w89_registry_init(w89_registry *r);
void w89_registry_free(w89_registry *r);
w89_err w89_register(w89_registry *r, const char *name, w89_moduleinst *inst);
w89_moduleinst *w89_registry_find(const w89_registry *r, const char *name,
                                  w89_u32 len);
w89_err w89_instantiate(w89_store *s, const w89_module *m,
                        const w89_registry *reg, w89_moduleinst **out);
const char *w89_instantiate_message(void);
int w89_find_export(w89_moduleinst *inst, const w89_name *name,
                    w89_externinst *out);
w89_eval_out w89_invoke(w89_store *s, w89_funcinst *f,
                        const w89_value *args, w89_u32 nargs);
w89_moduleinst *w89_spectest(w89_store *s);

/* Host functions (src/host.c). */
int w89_name_eq(const w89_name *n, const char *s, w89_u32 len);
w89_value w89_default_value(const w89_vt *t);
void w89_func_arity(const w89_funcinst *f, w89_u32 *nparams, w89_u32 *nresults);
const w89_ft *w89_func_ft(const w89_funcinst *f);

#endif
