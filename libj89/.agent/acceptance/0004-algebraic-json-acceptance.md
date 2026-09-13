# 0004-algebraic-json-acceptance

## Must exhibit

1. **List free-monoid algebra.** `nil` is the left/right identity of
   `concat`; `concat` is associative and preserves order and multiplicity.
2. **List fold.** `fold(nil,z,f) == z`; `fold(cons(x,xs),z,f) ==
   f(x, fold(xs,z,f))` (right catamorphism).
3. **List functor.** `map(x,id) == x`; `map(map(x,f),g) == map(x,g∘f)`;
   order and multiplicity preserved; callback fires exactly once per element.
4. **Object commutative-monoid algebra.** `empty` is the identity of `union`;
   `union` is associative and commutative.
5. **Object bag semantics.** `union(member(k,v), member(k,v))` preserves two
   occurrences; duplicate member names and duplicate occurrences survive every
   core transformation.
6. **Object order neutrality.** No object operation exposes member order as
   semantic information; folds through valid commutative monoids agree across
   union trees.
7. **Object functor.** `map(x,id,id) == x`; composition holds; mapped label
   collisions coexist without policy.
8. **JSON fixed point.** `project(embed(shape)) == shape` and
   `embed(project(value)) == value`; six constructors remain disjoint;
   `project` removes exactly one layer.
9. **JSON catamorphism.** `json.fold` obeys its constructor equations and the
   catamorphism law, terminates on finite values, and invokes exactly the
   matching branch with every child folded first.
10. **JSON functor.** `json.map` obeys identity and composition and maps the
    shared string carrier into both JSON strings and object member names.
11. **Scalar opacity.** Number and string carriers remain semantically opaque;
    the core runs against carriers exposing only construct/copy/destroy.
12. **Failure transparency.** Allocation and callback failures propagate and
    never become ordinary JSON values; no partial result escapes.

## Must reject

1. **Member multiplicity collapse.** Any operation that silently merges two
   equal object occurrences into one is a failure.
2. **Accidental scalar semantics.** Comparing, ordering, hashing, classifying,
   formatting, decoding, or arithmetic on the number/string carrier inside the
   core is a failure.
3. **Order leakage.** Exposing object member order as a promised contract is a
   failure.
4. **Array reordering.** Reordering or duplicating/dropping array elements in
   any list operation is a failure.
5. **Policy leakage.** The core depending on parse, render, equality, lookup,
   ordering, Unicode, numeric, path, or merge policy is a failure.
6. **Derived operations in the public core.** Exposing lookup, indexing,
   length, filter, search, paths, equality, merge, arithmetic, Unicode, parse,
   or render as mandatory core operations is a failure.
7. **Swallowed failures.** A callback/allocation failure reported as a
   successful result, or converted into a JSON value, is a failure.
8. **Effectful-call discipline (green).** Every value-returning call must form
   a complete statement/assignment; no effectful call as a direct operand of
   `return`; `NULL` spelled via `NULL`; no redundant casts.
9. **Green compliance.** Every `src/alg` translation unit must be strict-C89
   clean under the C89∩C23 × gcc/clang matrix, pass the clang-tidy semantic
   suite in both standards, and be canonically Allman-formatted.
