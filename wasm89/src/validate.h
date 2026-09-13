#ifndef WASM89_VALIDATE_H
#define WASM89_VALIDATE_H

#include "module.h"

/* Flattened type environment derived from a decoded module's type
 * section. Each decoded subtype becomes one deftype with a consecutive
 * global index; each rec group records its first index and size. */
typedef struct w89_deftype {
    const w89_subtype *sub;
    w89_u32 recgroup;
    w89_u32 recpos;
} w89_deftype;

typedef struct w89_recgroup {
    w89_u32 first;
    w89_u32 nsubs;
} w89_recgroup;

typedef struct w89_typeenv {
    w89_deftype *types;
    w89_u32 ntypes;
    w89_recgroup *recs;
    w89_u32 nrecs;
} w89_typeenv;

w89_err w89_typeenv_build(const w89_module *m, w89_typeenv *env);
void w89_typeenv_free(w89_typeenv *env);

/* Canonical type equality: mirrors subst_deftype (subst_of) on the
 * flattened type list. */
int w89_type_canon_eq(const w89_typeenv *env, w89_u32 a, w89_u32 b);

/* Subtyping relations (a <: b). */
int w89_match_deftype(const w89_typeenv *env, w89_u32 a, w89_u32 b);
int w89_match_valtype(const w89_typeenv *env, const w89_vt *a,
                      const w89_vt *b);
int w89_match_reftype(const w89_typeenv *env, const w89_reftype *a,
                      const w89_reftype *b);
int w89_match_resulttype(const w89_typeenv *env, const w89_vt *a,
                         w89_u32 na, const w89_vt *b, w89_u32 nb);
int w89_match_comptype(const w89_typeenv *env, const w89_subtype *a,
                       const w89_subtype *b);

w89_err w89_module_validate(const w89_module *m);
const char *w89_validate_message(void);

#endif
