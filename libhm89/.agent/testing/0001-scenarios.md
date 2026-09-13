# BDD scenarios

## Fresh variables

- SCENARIO Freshness: GIVEN a context WHEN two `hm_type_var` calls occur THEN
  the variables are distinct nodes.
- SCENARIO Fresh ids: GIVEN two fresh variables WHEN their ids are read THEN
  the ids differ.

## Unification

- SCENARIO Reflexive: GIVEN the same variable `a` twice WHEN `hm_unify(a, a)`
  occurs THEN it returns `HM_OK`.
- SCENARIO Variable binding: GIVEN `a` and `Int` WHEN `hm_unify(a, Int)`
  occurs THEN it returns `HM_OK` AND pruning `a` yields `Int`.
- SCENARIO Constructor equality: GIVEN `List(Int)` and `List(Int)` WHEN
  unified THEN `HM_OK`.
- SCENARIO Constructor mismatch: GIVEN `Int` and `Bool` WHEN unified THEN
  `HM_ERROR_MISMATCH`.
- SCENARIO Arity mismatch: GIVEN `T(Int)` and `T(Int, Bool)` WHEN unified THEN
  `HM_ERROR_MISMATCH`.
- SCENARIO Recursive unification: GIVEN `Pair(a, Int)` and `Pair(Bool, b)`
  WHEN unified THEN `HM_OK` AND pruning `a` yields `Bool` AND pruning `b`
  yields `Int`.
- SCENARIO Occurs check: GIVEN a fresh `a` and `List(a)` WHEN `hm_unify`
  occurs THEN `HM_ERROR_OCCURS`.
- SCENARIO Occurs in function: GIVEN fresh `a` and `->(Int, a)` WHEN unified
  THEN `HM_ERROR_OCCURS`.

## Schemes and instantiation

- SCENARIO Monomorphic scheme: GIVEN a monomorphic scheme `a -> a` WHEN it is
  instantiated twice THEN both results share the same `a` node.
- SCENARIO Quantified freshness: GIVEN `forall a. a -> a` WHEN instantiated
  twice THEN the two results carry distinct (unrelated) fresh variables.
- SCENARIO Quantified count: GIVEN a scheme built with two quantified
  variables WHEN its quantified count is read THEN it is 2.
- SCENARIO Scheme body: GIVEN a scheme over body `T` WHEN the body is read
  THEN it equals `T`.

## Generalization

- SCENARIO Empty environment: GIVEN type `a -> a` and an empty env WHEN
  generalized THEN the scheme has one quantified variable.
- SCENARIO Environment-sensitive: GIVEN env binding `x : a` and type `a -> b`
  WHEN generalized THEN exactly `b` is quantified (count 1) AND `a` is not.
- SCENARIO Scheme duplicate rejection: GIVEN a scheme requested with the same
  variable listed twice WHEN constructed THEN the operation fails or is
  rejected (see acceptance; invalid use).

## Environments

- SCENARIO Lookup present: GIVEN an env with `x` bound WHEN looked up THEN the
  bound scheme is returned.
- SCENARIO Lookup absent: GIVEN an env without `x` WHEN looked up THEN `NULL`
  is returned.
- SCENARIO Child shadowing: GIVEN a child env re-binding `x` WHEN looked up
  through the child THEN the child binding wins.
- SCENARIO Parent chain: GIVEN a binding in a grandparent WHEN looked up from
  a grandchild THEN it is found.

## Whole-expression inference (host-driven, using primitives)

- SCENARIO Identity: GIVEN `fun x -> x` inferred via primitives THEN the type
  is `a -> a` (unifiable to a fresh function type).
- SCENARIO Constant function: GIVEN `fun x -> fun y -> x` THEN `a -> b -> a`.
- SCENARIO Apply: GIVEN `fun f -> fun x -> f x` THEN `(a -> b) -> a -> b`
  modulo renaming.
- SCENARIO Let polymorphism: GIVEN `let id = fun x -> x in Pair(id 1, id
  true)` THEN the result unifies with `Pair(Int, Bool)`.
- SCENARIO Self application rejection: GIVEN `fun x -> x x` THEN inference
  fails via `HM_ERROR_OCCURS`.
- SCENARIO Lambda monomorphism: GIVEN `fun id -> Pair(id 1, id true)` with a
  monomorphic `id` THEN inference fails (`HM_ERROR_MISMATCH`), because
  lambda-bound variables are monomorphic.

## Constructor introspection

- SCENARIO Con name: GIVEN a constructor `List(a)` WHEN its name is read THEN
  it equals `"List"`.
- SCENARIO Con arity: GIVEN a constructor `List(a)` WHEN its arity is read THEN
  it is 1; for a nullary constructor it is 0.
- SCENARIO Con args: GIVEN `Pair(a, b)` WHEN args are read THEN they are `a`
  and `b` in order.
- SCENARIO Pruned con: GIVEN a variable `v` unified with `List(Int)` WHEN the
  introspection accessors are called on `v` THEN they report the constructor
  `List` with arity 1 and arg `Int`.
- SCENARIO Non-con query: GIVEN a plain variable WHEN queried as a constructor
  THEN name is `NULL`, arity is 0, and arg access returns `NULL`.
- SCENARIO Out-of-range arg: GIVEN `Pair(a, b)` WHEN an arg index of 2 is read
  THEN the result is `NULL`.

## Rendering

- SCENARIO Deterministic naming: GIVEN a type with free variables WHEN written
  THEN first-encountered variables render as `'a`, `'b`, ... regardless of
  internal ids.
- SCENARIO Constructor rendering: GIVEN `List('a)` WHEN written THEN the text
  is `List('a)`.

## Errors

- SCENARIO Mismatch sides: GIVEN a mismatch WHEN the error is inspected THEN
  kind is `HM_ERR_MISMATCH` AND left/right identify the incompatible types.
- SCENARIO Occurs sides: GIVEN an occurs failure WHEN inspected THEN kind is
  `HM_ERR_OCCURS` AND left is the variable AND right is the containing type.
- SCENARIO Unbound name: GIVEN an unbound-name failure WHEN inspected THEN
  kind is `HM_ERR_UNBOUND` AND name identifies the missing binding.
- SCENARIO Origin capture: GIVEN an origin set on the context before a failing
  operation WHEN the error is inspected THEN its origin equals the pointer.
