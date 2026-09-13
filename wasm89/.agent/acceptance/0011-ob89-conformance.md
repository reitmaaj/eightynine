# Acceptance: ob89 (Obligation-C89) conformance

*Specification reference: ob89/ob89.md (Obligation-C89).*

## MUST

* Every file under `src/*.c` MUST pass the `ob89` linter with zero
  findings (exit 0) when linted as its own translation unit.
* The `ob89` lint recipe in the root `Justfile` MUST lint all `src/*.c`
  files and MUST fail the build flow if any file reports a finding.
* Every accepted statement MUST map to exactly one obligation class
  (`BIND`, `COMPUTE`, `CONVERT`, `LAYOUT`, `ADDR`, `PTR`, `LOAD`,
  `STORE`, `CALL`, `BRANCH`).
* Witness variables introduced by normalization MUST be declared at the
  top of the smallest enclosing block that contains their producer and
  consumer, MUST be used (prefer once), and MUST be declared in C89
  declaration position (before statements in the block) so the sources
  still build under `-std=c89 -pedantic-errors -Werror`.
* Splitting an expression into witness statements MUST preserve the
  original evaluation order; splitting a short-circuit (`&&`/`||`) or
  conditional (`?:`) operator MUST preserve its control flow via nested
  `if` statements.
* Converting increments/decrements and compound assignments MUST use the
  equivalent `x = x + 1` / `x = x op y` compute statements.
* `sizeof` MUST use type syntax (`sizeof(type)`) only.
* Casts MUST stand alone as convert statements (`x = (T)y;`).
* Function-pointer call targets MUST be hoisted into a local variable so
  the call target is a simple identifier.
* The wasm89 test suite (`just test`) MUST remain green after the
  conversion.

## MUST NOT

* The sources MUST NOT contain any of the rejected expression-level
  effects or control forms: assignments or increments inside expressions,
  compound assignments, comma operator, conditional operator, `&&`/`||`,
  `sizeof expr`, non-constant initializers, loads/calls/casts/address-of/
  pointer arithmetic mixed into pure computation, computed place indexes,
  calls inside places, calls or loads inside branch conditions, or store
  RHS that is not a simple value.
* The conversion MUST NOT change runtime behavior; no test may regress.
