# BDD scenarios

## Context and lifecycle

- SCENARIO Create: GIVEN an `hm_ctx` WHEN `adt_ctx_new` runs THEN a context
  is returned that holds no error.
- SCENARIO Origin: GIVEN an origin set on the context WHEN a failing
  operation runs THEN `adt_ctx_error` reports that origin.
- SCENARIO Last error: GIVEN a failing operation WHEN the context's error is
  read THEN kind and message are reported; the error remains until a later
  failure replaces it or the context is destroyed.
- SCENARIO Allocator: GIVEN a custom allocator WHEN `adt_ctx_new_with_allocator`
  runs THEN ADT allocations go through it and `adt_ctx_destroy` frees them.

## Declarations

- SCENARIO Nullary: GIVEN `adt_type_new(ctx, "void", 0)` WHEN parameter count
  is read THEN it is 0.
- SCENARIO Parameterized: GIVEN `adt_type_new(ctx, "option", 1)` THEN it owns
  one distinct fresh parameter variable.
- SCENARIO Nominal identity: GIVEN two declarations with the same display
  name WHEN each is applied to fresh arguments THEN the applications unify
  within one declaration and fail across declarations.
- SCENARIO Wrong apply arity: GIVEN `option` (arity 1) WHEN applied with 2
  arguments THEN NULL and `ADT_ERROR_WRONG_ARITY` are recorded.

## Constructors

- SCENARIO Add: GIVEN an OPEN owner WHEN a constructor with field types is
  added THEN it is recorded with those field types and its owner.
- SCENARIO Sealed rejection: GIVEN a sealed owner WHEN a constructor is added
  THEN it is rejected (`ADT_ERROR_SEALED`).
- SCENARIO Duplicate ctor: GIVEN two OPEN types WHEN the same constructor name
  is added to both THEN the second is rejected (`ADT_ERROR_DUPLICATE_CTOR`).
- SCENARIO Foreign variable: GIVEN a constructor field using a variable not in
  the owner's parameter set WHEN the owner is sealed THEN sealing fails
  (`ADT_ERROR_TYPE`).

## Sealing and schemes

- SCENARIO Seal: GIVEN a complete OPEN option WHEN sealed THEN constructors
  expose generated schemes quantifying all parameters.
- SCENARIO Nullary scheme: GIVEN `None` WHEN its scheme renders THEN it is
  `forall a. option(a)`.
- SCENARIO Unary scheme: GIVEN `Some : a` WHEN its scheme is decomposed THEN
  it is `forall a. a -> option(a)`.
- SCENARIO Quantify all: GIVEN `Left : a` in `('a,'b) either` WHEN its scheme
  quantifies THEN it quantifies both `a` and `b`.
- SCENARIO Recursive: GIVEN a recursive list (Nil, Cons) WHEN sealed THEN
  declaration succeeds and the HM occurs check is never triggered.
- SCENARIO Mutual recursion: GIVEN mutually recursive expr/stmt WHEN both are
  declared, populated, and sealed THEN it succeeds.

## Binding and instantiation

- SCENARIO Bind: GIVEN a sealed option bound into an env WHEN None and Some
  are looked up THEN both schemes are found.
- SCENARIO Instantiate independent: GIVEN `Some` instantiated twice WHEN the
  results are unified against `option(int)` and `option(string)` THEN both
  succeed and the template parameters stay unbound.

## Pattern typing

- SCENARIO Constrains fields: GIVEN `Some` and scrutinee `option(int)` WHEN
  pattern types run THEN the field type unifies with `int`.
- SCENARIO Wrong pattern arity: GIVEN a pattern with the wrong field count
  THEN `ADT_ERROR_PATTERN_ARITY`.

## Match analysis

- SCENARIO Exhaustive all: GIVEN clauses covering every constructor WHEN
  exhaustiveness is checked THEN exhaustive.
- SCENARIO Non-exhaustive: GIVEN clauses missing a constructor THEN
  nonexhaustive.
- SCENARIO Wildcard exhaustive: GIVEN a wildcard clause THEN exhaustive.
- SCENARIO Redundant: GIVEN `Some _` then `Some x` THEN the second is
  redundant.
- SCENARIO Redundant after wildcard: GIVEN `_` then `None` THEN `None` is
  redundant.
- SCENARIO Nested exhaustive: GIVEN `P(False,_)`,`P(True,_)` over pairbool
  THEN exhaustive.
- SCENARIO Nested non-exhaustive: GIVEN `P(False,_)`,`P(True,False)` THEN
  nonexhaustive.
