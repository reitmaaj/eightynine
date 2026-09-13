#ifndef J89_ALG_H
#define J89_ALG_H

#include <stddef.h>

/* j89_alg.h - policy-neutral algebraic JSON core for libj89.
 *
 * This module is an additive sibling API to the practical JSON document
 * model in j89.h. It implements only the structures implied by the fixed
 * point
 *
 *     J = muX.( 1 + 2 + N + S + L(X) + B(S x X) )
 *
 * with the concrete realization
 *
 *     J = muX.( 1 + 2 + double + bytestring + L(X) + B(bytestring x X) )
 *
 * where
 *     N         = double                          (number carrier, copied
 * inline) S         = j89a_str                        (opaque refcounted
 * byte-string) Bool      = enum { J89A_FALSE, J89A_TRUE }  (the two-value
 * carrier) L(X)      = ordered finite list             (free monoid / list
 * functor) B(S x X)  = finite bag of member occurrences (free commutative
 * monoid)
 *
 * Objects are BAGS of member occurrences. Duplicate member names and
 * duplicate occurrences are preserved; no lookup, shadowing, uniqueness, or
 * order semantics are introduced. No scalar semantics are applied to N or S:
 * the core stores, copies, retains, releases, and transports the carriers but
 * never compares, orders, classifies, formats, decodes, or hashes them.
 *
 * The core does NOT provide parsing, rendering, equality, ordering, numeric
 * or Unicode processing, lookup, indexing, filtering, paths, or merging.
 *
 * ---- Const model ------------------------------------------------------------
 *
 * `const` denotes CONTAINER constness, not deep/transitive constness. Read-only
 * operations (project, fold, map, string readers) accept `const` containers.
 * Operations that mutate a child's logical ownership metadata (retain/release
 * and the sharing constructors) take non-const handles.
 *
 * A child pointer read out of a const container is BORROWED. Its non-const
 * pointer type exists only because retain/release and the sharing constructors
 * operate on logical ownership metadata; C's `const` protects the field, not
 * the pointee. A caller MUST NOT release, consume, or otherwise invalidate a
 * borrowed reference unless it first acquires an independent reference
 * (retain). Constructors retain the children they store; the caller still owns
 * any reference it held before the call.
 *
 * ---- Error model ------------------------------------------------------------
 *
 * Only operational failures exist. A pure constructor returns its owned value
 * or NULL (meaning allocation failure, recorded in the allocator). A
 * callback-bearing operation returns a j89a_status and writes its result
 * through an out parameter so that a CALLBACK failure propagates unchanged
 * and is never reinterpreted or converted into a JSON value.
 *
 * ---- Ownership --------------------------------------------------------------
 *
 * All structural nodes are immutable and refcounted. Every function returning
 * a value declared here returns an OWNED value that the caller must release
 * with the matching j89a_X_release. Constructors borrow their non-const
 * children and retain them. Ownership mechanics never affect observable
 * algebraic semantics. map builds fresh output and is not required to share
 * source subtrees.
 *
 * ---- Callback and output contract (transactional) --------------------------
 *
 * Every callback with an output parameter (list/object/json fold cases and
 * step/member/combine callbacks, list/object map callbacks, rtype.copy) obeys
 * this ABI invariant:
 *
 *   - On J89A_OK the callback initializes *out with exactly one owned result.
 *   - On any non-OK return *out remains uninitialized and owns nothing; the
 *     core must not read or drop it, and a failed recursive step must not be
 *     treated as if it had produced a value.
 *   - Output storage must not alias any borrowed input storage.
 *
 * Failing after writing partial/garbage state into *out violates the contract.
 *
 * Object member/combine callback invocation order is unspecified and carries
 * no algebraic meaning. Bag-semantic results require callbacks whose returned
 * contribution depends only on their explicit semantic inputs; side effects
 * through ctx must not affect the algebraic result.
 *
 * ---- Programmer preconditions ----------------------------------------------
 *
 * The core raises only operational failures (allocation, callback). It does
 * not validate arguments. The caller must ensure:
 *
 *   - every child/value passed to a constructor is a valid, non-NULL value;
 *   - `kind` holds one declared j89a_kind and `boolean` one declared j89a_bool;
 *   - every function pointer (callback, rtype, allocator) required by an
 *     operation is valid;
 *   - rt->size > 0 whenever an rtype is used;
 *   - NULL never denotes an algebraic list/object/json value (it means absence
 *     or failure); an empty list is an allocated NIL node.
 *
 * Violation of a precondition is undefined behaviour, outside the contract.
 *
 * ---- Result carrier R -------------------------------------------------------
 *
 * list.fold, object.fold, and json.fold eliminate into an arbitrary caller
 * result type R described by a j89a_rtype. The core never interprets the
 * bytes of R; it moves and drops R values according to the runtime. When a
 * combine/step must duplicate an R value it uses copy (or memcpy when NULL).
 */

typedef unsigned long j89a_refs;

/* ---- Concrete byte-string carrier S -------------------------------------- */

typedef struct j89a_str j89a_str;

/* ---- Two-value Boolean carrier ------------------------------------------- */

typedef enum j89a_bool
{
    J89A_FALSE = 0,
    J89A_TRUE = 1
} j89a_bool;

/* ---- JSON constructors ----------------------------------------------------
 */

typedef enum j89a_kind
{
    J89A_NULL = 0,
    J89A_BOOLEAN,
    J89A_NUMBER,
    J89A_STRING,
    J89A_ARRAY,
    J89A_OBJECT
} j89a_kind;

/* ---- Forward declarations of the algebraic structures -------------------- */

typedef struct j89a_list j89a_list; /* ordered list of Json */
typedef struct j89a_object
    j89a_object; /* bag of (S, Json) member occurrences */
typedef struct j89a_json j89a_json;
typedef struct j89a_rlist j89a_rlist;     /* fold-local list of R */
typedef struct j89a_robject j89a_robject; /* fold-local bag of (S, R) */

/* ---- Status / allocator ---------------------------------------------------
 */

typedef enum j89a_status
{
    J89A_OK = 0,
    J89A_NOMEM,
    J89A_CALLBACK,
    J89A_INTERNAL
} j89a_status;

typedef struct j89a_alloc
{
    void *(*alloc)(void *ctx, size_t size);
    void *(*realloc)(void *ctx, void *p, size_t size);
    void (*free)(void *ctx, void *p);
    void *ctx;
    int failed; /* set when a pure constructor hits NOMEM */
} j89a_alloc;

void j89a_alloc_init(
    j89a_alloc *al); /* default malloc/realloc/free, ctx NULL */

/* ---- Result-carrier runtime R ---------------------------------------------
 */

typedef struct j89a_rtype
{
    size_t size;
    /* Duplicate src into dst. NULL means memcpy(size). Failed copy leaves dst
     * uninitialized (owns nothing); the returned status propagates verbatim. */
    j89a_status (*copy)(void *ctx, void *dst, const void *src);
    void (*drop)(void *ctx, void *value); /* NULL => trivial */
    void *ctx;
} j89a_rtype;

/* ---- Byte-string carrier interface --------------------------------------- */

/* Construct an owned byte-string copying `len` bytes from `bytes`. NULL on
 * NOMEM. */
j89a_str *j89a_str_new(const void *bytes, size_t len, j89a_alloc *al);
void j89a_str_retain(j89a_str *s);
void j89a_str_release(j89a_str *s);
size_t j89a_str_len(const j89a_str *s);
const void *j89a_str_data(const j89a_str *s);

/* ---- List algebra (element type: Json) ------------------------------------
 */

j89a_list *j89a_list_nil(j89a_alloc *al);
j89a_list *j89a_list_cons(j89a_json *head, j89a_list *tail, j89a_alloc *al);
j89a_list *j89a_list_concat(j89a_list *left, j89a_list *right, j89a_alloc *al);
void j89a_list_retain(j89a_list *xs);
void j89a_list_release(j89a_list *xs);

/* Right catamorphism. step(ctx, element, acc_r, out_r): reads the Json
 * element and the accumulated R (both borrowed), writes an owned R to
 * out_r. zero_r is copied when the list is empty. */
typedef j89a_status (*j89a_list_step_fn)(void *ctx, const j89a_json *element,
                                         const void *acc_r, void *out_r);
j89a_status j89a_list_fold(const j89a_list *xs, const j89a_rtype *rt,
                           const void *zero_r, j89a_list_step_fn step,
                           void *ctx, void *out_r, j89a_alloc *al);

/* map(f) once per element, preserving order and multiplicity. Writes an
 * owned result to *out (NULL on failure). */
typedef j89a_status (*j89a_list_map_fn)(void *ctx, const j89a_json *element,
                                        j89a_json **out);
j89a_status j89a_list_map(const j89a_list *xs, j89a_list_map_fn map, void *ctx,
                          j89a_list **out, j89a_alloc *al);

/* ---- Object algebra (bag of (S, Json)) ------------------------------------
 */

j89a_object *j89a_object_empty(j89a_alloc *al);
j89a_object *j89a_object_member(j89a_str *name, j89a_json *value,
                                j89a_alloc *al);
j89a_object *j89a_object_union(j89a_object *left, j89a_object *right,
                               j89a_alloc *al);
void j89a_object_retain(j89a_object *o);
void j89a_object_release(j89a_object *o);

/* Commutative fold. member_fn(ctx, name, value, out_r) contributes one member
 * occurrence; combine_fn(ctx, left_r, right_r, out_r) must form a commutative
 * monoid together with zero_r. Both read their inputs (borrowed) and write an
 * owned R to out_r. A non-commutative combine violates the caller contract and
 * yields an unspecified result. */
typedef j89a_status (*j89a_object_member_fn)(void *ctx, const j89a_str *name,
                                             const j89a_json *value,
                                             void *out_r);
typedef j89a_status (*j89a_object_combine_fn)(void *ctx, const void *left_r,
                                              const void *right_r, void *out_r);
j89a_status j89a_object_fold(const j89a_object *o, const j89a_rtype *rt,
                             const void *zero_r,
                             j89a_object_member_fn member_fn,
                             j89a_object_combine_fn combine_fn, void *ctx,
                             void *out_r, j89a_alloc *al);

/* map over names (S->S) and values (Json->Json). name_map reads a borrowed
 * name and returns an owned j89a_str (NULL => NOMEM). value_map reads a
 * borrowed Json and writes an owned Json to *out. Multiplicity is preserved;
 * label collisions coexist. */
typedef j89a_str *(*j89a_object_name_fn)(void *ctx, const j89a_str *name);
typedef j89a_status (*j89a_object_value_fn)(void *ctx, const j89a_json *value,
                                            j89a_json **out);
j89a_status j89a_object_map(const j89a_object *o, j89a_object_name_fn name_map,
                            j89a_object_value_fn value_map, void *ctx,
                            j89a_object **out, j89a_alloc *al);

/* ---- JSON fixed point ------------------------------------------------------
 */

/* One structural layer. The child fields are BORROWED pointers to
 * independently owned, refcounted values. They are non-const only so that a
 * construct/retain path may take ownership metadata from them without a cast.
 * The caller MUST NOT release/consume a borrowed child unless it first
 * acquires its own reference. Exactly one payload is meaningful per `kind`. */
typedef struct j89a_shape
{
    j89a_kind kind;
    j89a_bool boolean_value;
    double number_value;
    j89a_str *string_value;
    j89a_list *array_value;
    j89a_object *object_value;
} j89a_shape;

/* Remove exactly one recursive layer (no recursion). Children written into
 * the shape are borrowed. */
void j89a_json_project(const j89a_json *value, j89a_shape *shape);
/* Construct exactly one recursive layer from a shape. Retains the borrowed
 * children. NULL on NOMEM. */
j89a_json *j89a_json_embed(const j89a_shape *shape, j89a_alloc *al);

void j89a_json_retain(j89a_json *value);
void j89a_json_release(j89a_json *value);

/* Six constructors, exact specializations of embed. NULL on NOMEM. Each
 * retains its child. */
j89a_json *j89a_json_null(j89a_alloc *al);
j89a_json *j89a_json_boolean(j89a_bool b, j89a_alloc *al);
j89a_json *j89a_json_number(double n, j89a_alloc *al);
j89a_json *j89a_json_string(j89a_str *s, j89a_alloc *al);
j89a_json *j89a_json_array(j89a_list *xs, j89a_alloc *al);
j89a_json *j89a_json_object(j89a_object *ms, j89a_alloc *al);

/* ---- Recursive morphisms --------------------------------------------------
 */

/* JSON catamorphism. algebra.rt must describe R. Each case callback reads
 * its inputs (borrowed; string values, array/object child bags are fold-local
 * and valid only for the duration of the call) and writes an owned R to out_r.
 * Matches ICD: array_case receives L(R) and object_case receives B(S x R). */
typedef struct j89a_algebra
{
    const j89a_rtype *rt;
    void *ctx;
    j89a_status (*null_case)(void *ctx, void *out_r);
    j89a_status (*boolean_case)(void *ctx, j89a_bool b, void *out_r);
    j89a_status (*number_case)(void *ctx, double n, void *out_r);
    j89a_status (*string_case)(void *ctx, const j89a_str *s, void *out_r);
    j89a_status (*array_case)(void *ctx, const j89a_rlist *children,
                              void *out_r);
    j89a_status (*object_case)(void *ctx, const j89a_robject *members,
                               void *out_r);
} j89a_algebra;

j89a_status j89a_json_fold(const j89a_json *value, const j89a_algebra *alg,
                           void *out_r, j89a_alloc *al);

/* Fold views (fold-local child bags) are private, borrowed inputs delivered
 * to array_case/object_case. The following are the minimal inspection ABIs
 * for a catamorphism callback to consume L(R) / B(S x R). They remain valid
 * only for the duration of the enclosing container callback; no derived
 * operations are added around them. */
typedef j89a_status (*j89a_rlist_step_fn)(void *ctx, const void *element_r,
                                          const void *acc_r, void *out_r);
j89a_status j89a_rlist_fold(const j89a_rlist *xs, const j89a_rtype *rt,
                            const void *zero_r, j89a_rlist_step_fn step,
                            void *ctx, void *out_r, j89a_alloc *al);

typedef j89a_status (*j89a_robject_member_fn)(void *ctx, const j89a_str *name,
                                              const void *value_r, void *out_r);
j89a_status j89a_robject_fold(const j89a_robject *o, const j89a_rtype *rt,
                              const void *zero_r,
                              j89a_robject_member_fn member_fn,
                              j89a_object_combine_fn combine_fn, void *ctx,
                              void *out_r, j89a_alloc *al);

/* Recursive functorial mapping over the number and string carriers. The same
 * string_map transforms both JSON string scalars and object member names.
 * Returns an owned result, or NULL on NOMEM (map_number/map_string are pure
 * transforms; string_map returns NULL on NOMEM). */
typedef double (*j89a_number_map_fn)(void *ctx, double n);
typedef j89a_str *(*j89a_string_map_fn)(void *ctx, const j89a_str *s);
j89a_json *j89a_json_map(const j89a_json *value, j89a_number_map_fn number_map,
                         j89a_string_map_fn string_map, void *ctx,
                         j89a_alloc *al);

#endif
