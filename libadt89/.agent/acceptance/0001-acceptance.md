# Acceptance criteria

Acceptance tests encode behavior the software MUST exhibit and behavior that
would be UNACCEPTABLE (must be rejected / fail safely / be reported as
errors). They trace to `.agent/testing/0001-scenarios.md` and `spec/adt-spec.md`.

## Must exhibit (exhibit)

- A nullary datatype MUST declare; its parameter count MUST be 0.
- A parameterized datatype MUST own one distinct fresh parameter per declared
  count.
- Two applications of the SAME declaration MUST unify; two declarations with
  the same display name MUST NOT unify (I1).
- `adt_type_apply` MUST require argument_count == parameter_count.
- Adding a constructor to an OPEN owner MUST record it with field types.
- Sealing MUST generate a scheme for every constructor quantifying ALL
  datatype parameters (I4), right-associated over its fields.
- `None : option(a)` and `Some : a -> option(a)` MUST arise for `option`.
- `Left : 'a` in `('a,'b) either` MUST quantify both `a` and `b`.
- Recursive and mutually recursive declarations MUST succeed without any HM
  occurs error.
- Binding a sealed type MUST make every constructor resolvable in `hm_env`.
- Two instantiations of one constructor scheme MUST receive independent fresh
  variables and never bind the declaration template (I7).
- Constructor-pattern typing MUST constrain pattern field types by unifying
  the instantiated result against the scrutinee.
- A match covering every constructor, or containing a wildcard, MUST be
  exhaustive; a clause matching nothing new after earlier clauses MUST be
  reported redundant.
- Exhaustiveness MUST hold for nested finite families and for recursive types
  by pattern depth.

## Must reject / fail safely (reject)

- Adding a constructor to a SEALED type MUST be rejected (`ADT_ERROR_SEALED`)
  and MUST NOT mutate the type (I3).
- A second constructor with an already-used name MUST be rejected
  (`ADT_ERROR_DUPLICATE_CTOR`, I6).
- `adt_type_apply` with the wrong argument count MUST return NULL and record
  `ADT_ERROR_WRONG_ARITY` (I2).
- A constructor field containing a free HM variable outside the owner's
  parameter set MUST be rejected at sealing (I5).
- Sealing twice MUST NOT corrupt state and MUST NOT add constructors.
- Unsealed types MUST NOT be bound into an env (`ADT_ERROR_UNSEALED`).
- A constructor-pattern query with the wrong output count MUST be rejected
  (`ADT_ERROR_PATTERN_ARITY`).
- A pattern over an unsealed type MUST be rejected.
- Reading an out-of-range constructor/field index MUST NOT crash; accessors
  return NULL/0.
- `adt_ctx_destroy` MUST NOT free HM objects; the supplied `hm_ctx` MUST
  remain valid afterwards (destroy order ADT then HM).
- Calling match analysis on a type whose constructors are not all closed
  (unsealed) MUST be rejected.

## Conformance boundary

An implementation conforms when it: declares nullary/parameterized/recursive/
mutually-recursive datatypes; generates correct quantified constructor
schemes; binds constructors into `hm_env`; types constructor patterns through
ordinary HM unification; and reports exhaustiveness and redundancy over
closed constructor families. Each requirement is covered by at least one
scenario and by the tests under `test/`.
