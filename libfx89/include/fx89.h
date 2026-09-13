#ifndef FX89_H
#define FX89_H

/* fx89.h - standalone generic effect-row and constraint library (ISO C89).
 *
 * libfx89 represents, constrains, and solves nominal finite/open effect rows
 * and effect-row variables, independent of any value type system, AST,
 * runtime, or effect-execution semantics. This header is the public API.
 *
 * Every object is owned by an fx_ctx and freed by fx_ctx_free. Host userdata
 * is never freed by the library. Objects do not outlive their context.
 */

/* Opaque public types. Their representation is private. */
typedef struct fx_ctx fx_ctx;
typedef struct fx_atom fx_atom;
typedef struct fx_row fx_row;
typedef struct fx_var fx_var;
typedef struct fx_constraint fx_constraint;
typedef struct fx_scheme fx_scheme;

/* Context-local identities. 0 is reserved invalid; ids are monotonic and
 * never reused within one context lifetime. Not persistent across runs. */
typedef unsigned long fx_kind_id;
typedef unsigned long fx_op_id;
typedef unsigned long fx_var_id;
typedef unsigned long fx_constraint_id;
typedef unsigned long fx_checkpoint;

/* Status values returned by fallible calls. No semantic error is reported
 * through errno. */
typedef enum fx_status
{
    FX_OK = 0,
    FX_ERR_NOMEM,
    FX_ERR_INVALID,
    FX_ERR_DUPLICATE,
    FX_ERR_UNKNOWN,
    FX_ERR_UNSAT,
    FX_ERR_OCCURS,
    FX_ERR_CALLBACK,
    FX_ERR_UNSUPPORTED,
    FX_ERR_RANGE,
    FX_ERR_INTERNAL
} fx_status;

/* Three-valued truth for read-only queries. FX_UNKNOWN is never false. */
typedef enum fx_truth
{
    FX_FALSE = 0,
    FX_TRUE = 1,
    FX_UNKNOWN = 2
} fx_truth;

/* Constraint kinds. */
typedef enum fx_constraint_kind
{
    FX_CONSTRAINT_EQUAL,
    FX_CONSTRAINT_SUBSET,
    FX_CONSTRAINT_MEMBER,
    FX_CONSTRAINT_LACKS,
    FX_CONSTRAINT_JOIN
} fx_constraint_kind;

/* Constraint completion state. PENDING does not imply failure: it denotes a
 * genuine residual symbolic relation. */
typedef enum fx_constraint_state
{
    FX_CONSTRAINT_PENDING,
    FX_CONSTRAINT_SATISFIED,
    FX_CONSTRAINT_FAILED
} fx_constraint_state;

/* Diagnostic conflict kinds. */
typedef enum fx_conflict_kind
{
    FX_CONFLICT_NONE = 0,
    FX_CONFLICT_EQUALITY,
    FX_CONFLICT_SUBSET,
    FX_CONFLICT_MEMBER_LACKS,
    FX_CONFLICT_OCCURS,
    FX_CONFLICT_JOIN,
    FX_CONFLICT_INTERNAL
} fx_conflict_kind;

/* Host-defined semantics for parameterized effect atoms. The library treats
 * parameter values as opaque and delegates equality/order/copy/destroy to
 * this interface. compare must define a deterministic total order per kind
 * and stay stable for the context lifetime. */
typedef struct fx_atom_domain
{
    int (*compare)(void *userdata, fx_kind_id kind, const void *a,
                   const void *b);
    unsigned long (*hash)(void *userdata, fx_kind_id kind, const void *value);
    fx_status (*copy)(void *userdata, fx_kind_id kind, const void *value,
                      void **out_copy);
    void (*destroy)(void *userdata, fx_kind_id kind, void *value);
} fx_atom_domain;

/* ------------------------------------------------------------------ */
/* Context                                                            */
/* ------------------------------------------------------------------ */

/* Create an empty, independent context. */
fx_status fx_ctx_new(fx_ctx **out);

/* Destroy a context and everything it owns. fx_ctx_free(NULL) does nothing.
 * Host userdata is not freed. */
void fx_ctx_free(fx_ctx *ctx);

/* ------------------------------------------------------------------ */
/* Effect kinds                                                       */
/* ------------------------------------------------------------------ */

/* Define a nominal effect kind. Names are copied; duplicate names within a
 * context are rejected. */
fx_status fx_kind_define(fx_ctx *ctx, const char *name, void *userdata,
                         fx_kind_id *out_id);

/* Look up a kind by name. */
fx_status fx_kind_lookup(const fx_ctx *ctx, const char *name,
                         fx_kind_id *out_id);

/* Context-owned name of a kind, or NULL for an invalid id. */
const char *fx_kind_name(const fx_ctx *ctx, fx_kind_id id);

/* Userdata stored at declaration; no ownership transfer. */
void *fx_kind_userdata(const fx_ctx *ctx, fx_kind_id id);

/* ------------------------------------------------------------------ */
/* Operations                                                         */
/* ------------------------------------------------------------------ */

/* Define an operation under a kind. Names are unique within one kind. */
fx_status fx_op_define(fx_ctx *ctx, fx_kind_id kind, const char *name,
                       void *userdata, fx_op_id *out_id);

/* Owning kind of an operation, or 0 for an invalid id. */
fx_kind_id fx_op_kind(const fx_ctx *ctx, fx_op_id op);

/* Context-owned name of an operation, or NULL. */
const char *fx_op_name(const fx_ctx *ctx, fx_op_id op);

/* Userdata stored at declaration. */
void *fx_op_userdata(const fx_ctx *ctx, fx_op_id op);

/* ------------------------------------------------------------------ */
/* Atoms                                                              */
/* ------------------------------------------------------------------ */
/* Install the parameterized-atom domain. May only be called before the first
 * parameterized atom is created; nominal atoms need no domain. */
fx_status fx_ctx_set_atom_domain(fx_ctx *ctx, const fx_atom_domain *domain,
                                 void *domain_userdata);

/* Nominal atom for a kind. Repeated calls intern to an equivalent atom. */
fx_status fx_atom_nominal(fx_ctx *ctx, fx_kind_id kind, const fx_atom **out);

/* Parameterized atom. Requires an installed atom domain; the parameter is
 * copied through the domain's copy callback and interned by equivalence. */
fx_status fx_atom_parameterized(fx_ctx *ctx, fx_kind_id kind,
                                const void *parameter, const fx_atom **out);

/* Kind of an atom. */
fx_kind_id fx_atom_kind(const fx_atom *atom);

/* Whether the atom carries a host parameter (nominal atoms do not). */
int fx_atom_is_parameterized(const fx_atom *atom);

/* Read-only library-owned parameter copy, or NULL for nominal atoms. */
const void *fx_atom_parameter(const fx_atom *atom);

/* Structural equality (no constraint is added). */
int fx_atom_equal(const fx_ctx *ctx, const fx_atom *a, const fx_atom *b);

/* ------------------------------------------------------------------ */
/* Row variables                                                      */
/* ------------------------------------------------------------------ */

/* Allocate one fresh row variable. */
fx_status fx_var_new(fx_ctx *ctx, fx_var **out);

fx_var_id fx_var_id_of(const fx_var *var);

/* Optional debug name; copied by the context, carries no semantics. */
fx_status fx_var_set_name(fx_var *var, const char *name);

const char *fx_var_name(const fx_var *var);

/* Current structural binding, or NULL (with FX_OK) if unbound. */
fx_status fx_var_binding(fx_ctx *ctx, fx_var *var, const fx_row **out);

/* ------------------------------------------------------------------ */
/* Rows                                                               */
/* ------------------------------------------------------------------ */

/* {} */
fx_status fx_row_empty(fx_ctx *ctx, const fx_row **out);

/* {|var} */
fx_status fx_row_var(fx_ctx *ctx, fx_var *var, const fx_row **out);

/* Closed row over the given atoms. Order is irrelevant; duplicates collapse
 * and the head is canonically ordered. */
fx_status fx_row_closed(fx_ctx *ctx, const fx_atom *const *atoms,
                        unsigned long natoms, const fx_row **out);

/* Open row {atoms | tail}, enforcing the disjoint-tail invariant: every head
 * atom is forbidden from the tail. */
fx_status fx_row_open(fx_ctx *ctx, const fx_atom *const *atoms,
                      unsigned long natoms, fx_var *tail, const fx_row **out);

/* Extend a row with one atom (extensional; duplicates have no effect). */
fx_status fx_row_extend(fx_ctx *ctx, const fx_row *base, const fx_atom *atom,
                        const fx_row **out);

/* Canonical current representation following substitutions. */
fx_status fx_row_normalize(fx_ctx *ctx, const fx_row *row, const fx_row **out);

/* Explicit-head atom count of the current normalized row. */
unsigned long fx_row_atom_count(fx_ctx *ctx, const fx_row *row);

/* Canonically ordered atom at index, or NULL out of range. */
const fx_atom *fx_row_atom_at(fx_ctx *ctx, const fx_row *row,
                              unsigned long index);

/* Whether the current normalized row has an unresolved tail. */
int fx_row_is_open(fx_ctx *ctx, const fx_row *row);

/* Current unresolved tail variable, or NULL. */
fx_var *fx_row_tail(fx_ctx *ctx, const fx_row *row);

/* Context-owned array of the row's free variables, ordered by variable id.
 * The returned array is valid until the next fx_row_free_vars call on this
 * context (or context destruction). */
fx_status fx_row_free_vars(fx_ctx *ctx, const fx_row *row, fx_var ***out_vars,
                           unsigned long *out_count);

/* Exact structural removal of an atom from a row. Returns FX_ERR_UNSUPPORTED
 * when the difference is not computable directly (unresolved symbolic tail). */
fx_status fx_row_remove(fx_ctx *ctx, const fx_row *row, const fx_atom *atom,
                        const fx_row **out);

/* Exact structural removal of several atoms. */
fx_status fx_row_remove_many(fx_ctx *ctx, const fx_row *row,
                             const fx_atom *const *atoms, unsigned long natoms,
                             const fx_row **out);

/* Structural equality of current normalized forms (no constraint added). */
int fx_row_equal(fx_ctx *ctx, const fx_row *a, const fx_row *b);
/* Proven membership (TRUE/FALSE/UNKNOWN); no constraint is added. */
fx_truth fx_row_membership(fx_ctx *ctx, const fx_row *row, const fx_atom *atom);

/* Proven purity (row == {}); no constraint is added. */
fx_truth fx_row_is_pure(fx_ctx *ctx, const fx_row *row);

/* Proven closedness (no unresolved tail). Never constrains the row. */
fx_truth fx_row_is_closed(fx_ctx *ctx, const fx_row *row);

/* ------------------------------------------------------------------ */
/* Constraints                                                        */
/* ------------------------------------------------------------------ */

/* atom in row. out may be NULL. */
fx_status fx_require_member(fx_ctx *ctx, const fx_atom *atom, const fx_row *row,
                            fx_constraint **out);

/* atom not-in row. out may be NULL. */
fx_status fx_require_lacks(fx_ctx *ctx, const fx_atom *atom, const fx_row *row,
                           fx_constraint **out);

/* subset <= superset. out may be NULL. */
fx_status fx_require_subset(fx_ctx *ctx, const fx_row *subset,
                            const fx_row *superset, fx_constraint **out);

/* a = b. out may be NULL. */
fx_status fx_require_equal(fx_ctx *ctx, const fx_row *a, const fx_row *b,
                           fx_constraint **out);
/* Pure constraint: row == {}. out may be NULL. */
fx_status fx_require_pure(fx_ctx *ctx, const fx_row *row, fx_constraint **out);

/* Exact set union: out_row = left union right. out_constraint may be NULL. */
fx_status fx_require_join(fx_ctx *ctx, const fx_row *out_row,
                          const fx_row *left, const fx_row *right,
                          fx_constraint **out_constraint);

/* Union of two rows as an exact set union. Closed operands compute directly;
 * otherwise a fresh row variable and an internal exact-union (JOIN)
 * constraint are introduced. */
fx_status fx_row_union(fx_ctx *ctx, const fx_row *a, const fx_row *b,
                       const fx_row **out);

fx_constraint_id fx_constraint_id_of(const fx_constraint *constraint);

fx_constraint_kind fx_constraint_kind_of(const fx_constraint *constraint);

fx_constraint_state fx_constraint_state_of(const fx_constraint *constraint);

void fx_constraint_set_userdata(fx_constraint *constraint, void *userdata);

void *fx_constraint_userdata(const fx_constraint *constraint);

/* ------------------------------------------------------------------ */
/* Solver                                                             */
/* ------------------------------------------------------------------ */

/* Saturate the worklist. Returns FX_OK on success even when genuine residual
 * symbolic constraints remain PENDING. */
fx_status fx_solve(fx_ctx *ctx);

/* Report the last solver conflict, if any. */
fx_status fx_last_conflict(const fx_ctx *ctx, fx_conflict_kind *out_kind,
                           const fx_constraint **out_primary,
                           const fx_constraint **out_secondary);

/* Read-only queries. Never add constraints or mutate semantic state. */
fx_truth fx_check_subset(fx_ctx *ctx, const fx_row *subset,
                         const fx_row *superset);
fx_truth fx_check_lacks(fx_ctx *ctx, const fx_atom *atom, const fx_row *row);

/* ------------------------------------------------------------------ */
/* Checkpoints                                                        */
/* ------------------------------------------------------------------ */

/* Create a checkpoint over the solver state. Checkpoints must be rolled back
 * or committed in strict LIFO order. Declarations (kinds, operations, atom
 * domain, atom interning) survive rollback. */
fx_checkpoint fx_ctx_checkpoint(fx_ctx *ctx);
fx_status fx_ctx_rollback(fx_ctx *ctx, fx_checkpoint checkpoint);
fx_status fx_ctx_commit(fx_ctx *ctx, fx_checkpoint checkpoint);

/* ------------------------------------------------------------------ */
/* Version                                                            */
/* ------------------------------------------------------------------ */

#define FX89_VERSION_MAJOR 0
#define FX89_VERSION_MINOR 1
#define FX89_VERSION_PATCH 0

unsigned long fx_version_major(void);
unsigned long fx_version_minor(void);
unsigned long fx_version_patch(void);

/* ------------------------------------------------------------------ */
/* Schemes (effect-row polymorphism)                                  */
/* ------------------------------------------------------------------ */

/* Construct an effect-only scheme quantifying the given row variables over
 * the body row, carrying the given residual constraints. Every residual
 * constraint must reference only the quantified variables (or closed rows);
 * references to mutable outer variables are rejected. */
fx_status fx_scheme_new(fx_ctx *ctx, fx_var *const *vars, unsigned long nvars,
                        const fx_row *body, fx_constraint *const *constraints,
                        unsigned long nconstraints, fx_scheme **out);

/* Generalize body: quantify free(body) minus nongeneralizable, capturing the
 * residual subset/join constraints that reference only the generalized
 * component. */
fx_status fx_generalize(fx_ctx *ctx, const fx_row *body,
                        fx_var *const *nongeneralizable,
                        unsigned long nnongeneralizable, fx_scheme **out);

/* Instantiate a scheme with fresh variables; recreates its residual
 * constraints as live constraints in the current context. */
fx_status fx_instantiate(fx_ctx *ctx, const fx_scheme *scheme,
                         const fx_row **out);

unsigned long fx_scheme_var_count(const fx_scheme *scheme);
fx_var_id fx_scheme_var_id_at(const fx_scheme *scheme, unsigned long index);
const fx_row *fx_scheme_body(const fx_scheme *scheme);
unsigned long fx_scheme_constraint_count(const fx_scheme *scheme);
const fx_constraint *fx_scheme_constraint_at(const fx_scheme *scheme,
                                             unsigned long index);

#endif /* FX89_H */
