#ifndef FX_INTERNAL_H
#define FX_INTERNAL_H

/* fx_internal.h - private libfx89 representation shared across translation
 * units. Never installed. Not part of the public ABI. */

#include <stddef.h>

#include <fx89.h>

typedef struct fx_kindrec fx_kindrec;
typedef struct fx_oprec fx_oprec;
typedef struct fx_block fx_block;
typedef struct fx_watch fx_watch;
typedef struct fx_var_tmpl fx_var_tmpl;
typedef struct fx_row_tmpl fx_row_tmpl;
typedef struct fx_cons_tmpl fx_cons_tmpl;

/* One variable-watcher entry: a constraint that depends on a variable's
 * unresolved tail. Context-monotonic; stale entries are harmless because
 * firing one only re-enqueues a constraint the solver renormalizes. */
struct fx_watch
{
    fx_watch *next;
    struct fx_constraint *constraint;
};

/* Context-owned allocation block. Everything the context owns lives in one
 * singly linked list of blocks; fx_ctx_free frees the whole list. Growth of a
 * dynamic array discards the old allocation (still freed at ctx_free); this
 * keeps every object context-owned without per-object frees. */

/* Frozen variable facts for one quantified slot. */
struct fx_var_tmpl
{
    const fx_atom **required;
    unsigned long nreq;
    const fx_atom **forbidden;
    unsigned long nforb;
};

/* Frozen row syntax: canonical head plus either a closed tail or a quantified
 * slot index. No live fx_var participates. */
struct fx_row_tmpl
{
    const fx_atom **head;
    unsigned long nhead;
    int open;
    unsigned long slot;
};

/* Frozen residual constraint (SUBSET or JOIN) over row templates. */
struct fx_cons_tmpl
{
    fx_constraint_kind kind;
    fx_row_tmpl a;
    fx_row_tmpl b;
    fx_row_tmpl extra;
};

struct fx_scheme
{
    fx_ctx *owner;
    fx_var **vars;
    unsigned long nvars;
    const fx_row *body;
    fx_constraint **cons;
    unsigned long ncons;

    /* Frozen templates. Instantiation replays these, never the live objects
     * above (which are retained only for inspection). This keeps a scheme's
     * meaning independent of later solving/mutation of its source variables
     * and constraints. */
    unsigned long *slot_ids;
    fx_var_tmpl *vtmpl;
    fx_row_tmpl body_tmpl;
    fx_cons_tmpl *ctmpl;
};

/* One undo-trail record. Only the fields relevant to its kind are used. */
struct fx_trail_rec
{
    int kind;            /* FX_TR_BIND / REQ / FORB / STATE / CONFLICT */
    fx_var *var;         /* BIND / REQ / FORB target */
    fx_row *bind_prev;   /* BIND prior binding (or NULL) */
    fx_constraint *cons; /* STATE target */
    fx_constraint_state state_prev; /* STATE prior state */
    fx_conflict_kind conflict;      /* CONFLICT prior kind */
    const fx_constraint *conflict_primary;
    const fx_constraint *conflict_secondary;
};

#define FX_TR_BIND 1
#define FX_TR_REQ 2
#define FX_TR_FORB 3
#define FX_TR_STATE 4
#define FX_TR_CONFLICT 5

typedef struct fx_cp
{
    fx_checkpoint token;
    unsigned long trail_mark;
    fx_constraint *tail;
    fx_var *vars;
    unsigned long fact;
    struct fx_cp *prev;
} fx_cp;

struct fx_block
{
    struct fx_block *next;
};

struct fx_ctx
{
    fx_block *blocks;

    struct fx_kindrec *kinds;
    struct fx_kindrec *kinds_tail;
    struct fx_oprec *ops;
    struct fx_oprec *ops_tail;
    struct fx_atom *atoms;
    struct fx_var *vars;
    struct fx_constraint *constraints;
    struct fx_constraint *constraints_tail;

    unsigned long next_kind;
    unsigned long next_op;
    unsigned long next_var;
    unsigned long next_cid;

    unsigned long fact_serial;

    fx_conflict_kind conflict;
    const fx_constraint *conflict_primary;
    const fx_constraint *conflict_secondary;

    fx_atom_domain domain;
    void *domain_userdata;
    int has_domain;
    int has_param;

    fx_checkpoint next_cp;
    fx_cp *cp_top;

    struct fx_trail_rec *trail;
    unsigned long trail_n;
    unsigned long trail_cap;

    struct fx_constraint *q_head;
    struct fx_constraint *q_tail;
    int solving;

    struct fx_atom **atombuckets;
    unsigned long atombucket_n;
};

struct fx_kindrec
{
    struct fx_kindrec *next;
    unsigned long id;
    char *name;
    void *userdata;
};

struct fx_oprec
{
    struct fx_oprec *next;
    unsigned long id;
    unsigned long kind;
    char *name;
    void *userdata;
};

struct fx_atom
{
    struct fx_atom *next;
    struct fx_atom *hnext;
    fx_ctx *owner;
    unsigned long kind;
    int parameterized;
    void *param;
    const fx_atom_domain *domain;
    void *domain_userdata;
};

struct fx_var
{
    struct fx_var *next;
    fx_ctx *owner;
    unsigned long id;
    char *name;
    struct fx_var *parent;
    unsigned long rank;
    struct fx_row *binding;
    struct fx_watch *watchers;

    const fx_atom **required;
    unsigned long nreq;
    unsigned long creq;
    const fx_atom **forbidden;
    unsigned long nforb;
    unsigned long cforb;
};

struct fx_row
{
    fx_ctx *owner;
    const fx_atom **head;
    unsigned long nhead;
    struct fx_var *tail;
};

struct fx_constraint
{
    struct fx_constraint *next;
    struct fx_constraint *qnext;
    int enqueued;
    int live;
    fx_ctx *owner;
    unsigned long id;
    fx_constraint_kind kind;
    fx_constraint_state state;
    void *userdata;

    const fx_row *a;
    const fx_row *b;
    const fx_row *extra;
    const fx_atom *atom;
};

/* context / allocation */
void *fx__alloc(fx_ctx *ctx, unsigned long size);
void *fx__alloc_atoms(fx_ctx *ctx, unsigned long count);
void *fx__alloc_vars(fx_ctx *ctx, unsigned long count);
char *fx__strdup(fx_ctx *ctx, const char *s);

/* Representational-overflow guard for count * sizeof-style products. */
int fx__mul_overflow(unsigned long a, unsigned long b);

/* growable pointer-array append; returns the (possibly new) pointer to store
 * back. Caller supplies the current capacity pointer. */
const fx_atom **fx__arr_push(fx_ctx *ctx, const fx_atom **arr, unsigned long *n,
                             unsigned long *cap, const fx_atom *atom);

/* variable representative (union-find root) */
struct fx_var *fx__var_rep(fx_ctx *ctx, struct fx_var *var);

/* fact helpers on a representative variable */
fx_status fx__var_require(fx_ctx *ctx, struct fx_var *var, const fx_atom *atom);
fx_status fx__var_forbid(fx_ctx *ctx, struct fx_var *var, const fx_atom *atom);
int fx__var_has_required(const struct fx_var *var, const fx_atom *atom);
int fx__var_has_forbidden(const struct fx_var *var, const fx_atom *atom);

/* row helpers */
struct fx_row *fx__row_make(fx_ctx *ctx, const fx_atom *const *atoms,
                            unsigned long natoms, struct fx_var *tail);
struct fx_row *fx__row_normalize(fx_ctx *ctx, const fx_row *row);

/* atom comparison by kind id */
int fx__atom_less(const fx_atom *a, const fx_atom *b);
int fx__atom_eq(const fx_atom *a, const fx_atom *b);

/* solver (read-only decision helpers; registered pure in green.yaml) */
int fx__occurs(fx_ctx *ctx, struct fx_var *root, const struct fx_row *row);
int fx__occurs_walk(fx_ctx *ctx, struct fx_var *root, struct fx_var *var);
int fx__equal_class(fx_ctx *ctx, const struct fx_row *na,
                    const struct fx_row *nb);

void fx__record_conflict(fx_ctx *ctx, fx_conflict_kind kind,
                         const fx_constraint *primary,
                         const fx_constraint *secondary);

/* Ownership validation helpers: does the object (and, for rows, every atom it
 * references) belong to ctx? Used by public operations to reject mixed-context
 * objects deterministically. */
int fx__atom_local(fx_ctx *ctx, const fx_atom *atom);
int fx__var_local(fx_ctx *ctx, const fx_var *var);
int fx__row_local(fx_ctx *ctx, const fx_row *row);
int fx__constraint_local(fx_ctx *ctx, const fx_constraint *c);

/* Mutation-trail recording. Each returns FX_ERR_NOMEM on allocation failure;
 * callers must then abort the mutation so state and trail stay consistent.
 * All are no-ops (FX_OK) when no checkpoint is open. */
fx_status fx__trail_bind(fx_ctx *ctx, fx_var *var, fx_row *prev);
fx_status fx__trail_req(fx_ctx *ctx, fx_var *var);
fx_status fx__trail_forb(fx_ctx *ctx, fx_var *var);
fx_status fx__trail_state(fx_ctx *ctx, fx_constraint *c,
                          fx_constraint_state prev);
fx_status fx__trail_conflict(fx_ctx *ctx, fx_conflict_kind kind,
                             const fx_constraint *primary,
                             const fx_constraint *secondary);

/* Worklist notification: called after a semantic mutation on a variable
 * (required/forbidden fact or binding) while fx_solve is running. No-op
 * outside a solve. Precise (D4): fires the mutated variable's watchers. */
void fx__solve_notify(fx_ctx *ctx, fx_var *var);

/* (Re)register a constraint as a watcher on the unresolved tail variable(s)
 * of its operand rows. Called when the constraint is created and after it is
 * processed, so a binding that exposes a new tail updates dependencies. */
void fx__watch_register(fx_ctx *ctx, fx_constraint *c);

#endif /* FX_INTERNAL_H */
