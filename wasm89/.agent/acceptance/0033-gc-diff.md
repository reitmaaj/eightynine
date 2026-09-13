# Acceptance: GC — differential (S3.2.C4)

*Relates to `.agent/testing/0033-gc-diff.md`. Design: `.agent/design/0008-
differential.md`, `.agent/design/0011-gc.md`. Cross-reference: acceptance
`0015-differential.md`.*

## MUST

* GC differential is enabled only after the interpreter runs GC (decode→
  validate→eval green), so a mismatch is a real finding.
* Generated GC programs MUST expose only address-independent observables
  (ref identity via `ref.eq`/`ref.test`, structural equality of i31/fields,
  booleans) and MUST compare equal across wasm89 and the engines with the
  GC flag on.
* Raw object addresses MUST NOT be compared across engines.
* A runtime failure (trap) that both wasm89 and an engine report MUST be
  acceptable (rt≈rt); a pass-vs-fail or an observable-result mismatch MUST
  be a finding.

## MUST NOT

* GC MUST NOT be emitted while the engines would parse it as unsupported or
  while an identity/equality rule mismatch could make a false finding.
* A generated GC case MUST NOT compare against an engine using a different
  GC subtyping/`ref.test` reading of the same module.
* A scalar/SIMD/ref differential mismatch MUST NOT be introduced or hidden
  by the GC work.

## Gate

* Differential GC self-test green; the seeded differential sweep including
  GC templates runs with zero mismatch; `diff-sweep` green.
