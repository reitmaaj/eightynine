# libadt89 — specification

Companion to `libhm89`'s `spec/hm-spec.md`. This document records the public
contract and required invariants of `libadt89`. Sections map to tests under
`test/` and scenarios under `.agent/testing/`.

## Purpose

Declare and analyze ordinary ML-style algebraic datatypes on top of
`libhm89`, lowering every datatype type and value-constructor signature to
ordinary `hm_type` and `hm_scheme` values. No second type system, no HM
modification, no runtime value representation, no GADTs.

The central lowering for

```text
type T(a1 ... an) = C1(F11 ... F1m) | ... | Ck(Fk1 ... Fkp)
```

is the nominal application `T(a1 ... an)` plus ordinary HM bindings

```text
Ci : forall a1 ... an. Fi1 -> ... -> Fim -> T(a1 ... an)
```

## Scope

Supported: nullary constructors, n-ary constructors, parameterized ADTs,
recursive ADTs, mutually recursive ADTs, polymorphic constructor schemes,
constructor-pattern typing, exhaustiveness and redundancy analysis.
Excluded: GADTs, existential fields, higher-kinded parameters, type aliases,
records, extensible/polymorphic variants, subtyping, runtime values.

## Nominal identity

Each `adt_type` declaration receives a private, process-unique canonical HM
type-constructor name, assigned at `adt_type_new` and stable for the lifetime
of the declaration. The textual format is NOT part of the contract and MUST
NOT be read by callers. The observable contract is:

> Distinct `adt_type` declarations receive distinct nominal identities as
> observed through HM equality/unification.

## Invariants

- I1 Nominality: distinct declarations never unify merely because display
  names match.
- I2 Fixed arity: every `adt_type_apply` supplies exactly the declared count.
- I3 Closed constructors: after seal the constructor set cannot change.
- I4 Result ownership: every generated scheme returns an application of its
  owning datatype.
- I5 Parameter closure: every free HM variable in a constructor declaration
  belongs to the owner's parameter set.
- I6 Constructor-name uniqueness within an `adt_ctx`.
- I7 Template isolation: instantiation never mutates declaration templates.

## Immutability / ownership rules

After sealing, an `adt_type`, its constructors, and their schemes are
immutable. Declaration-template parameters MUST NOT be unified during
ordinary expression inference. `adt_ctx_destroy` frees ADT-owned storage only
and MUST precede `hm_ctx_destroy`.

## Sealing validation

`adt_type_seal` validates: type open; each field type well-formed; every free
field variable belongs to the parameter set (I5); constructor names valid;
scheme generation succeeds. A datatype MAY contain zero constructors.
