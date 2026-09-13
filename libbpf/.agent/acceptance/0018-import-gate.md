# 0018 - Acceptance: capability-gated import (helper) dispatch

*Acceptance criteria for capability-gated helper calls. Each item is a
behaviour the software MUST exhibit or MUST reject.*

## MUST

* A machine MUST own a capability table and let the host register live
  capabilities (type, rights, host object) yielding handles.
* Before a helper (import) call that the host has gated, the machine MUST
  validate that the capability handle in r1 is live, has the required resource
  type, and grants the required permission; on success it MUST report WAITING
  and record the import id.
* A helper with no declared requirement MUST suspend ungated.
* A rejected (wrong type, insufficient permission, revoked, or never-allocated
  handle) helper call MUST trap without suspending.

## MUST NOT

* A helper MUST NOT suspend when its r1 capability is not live or does not
  grant the required type and permission.
* The machine MUST NOT dispatch any helper whose capability check failed.
