# 0017 - Acceptance: capability table

*Acceptance criteria for the machine-local capability table. Each item is a
behaviour the software MUST exhibit or MUST reject.*

## MUST

* A fresh table MUST allocate handles strictly monotonically from zero, and
  MUST never reuse a handle number that was previously allocated, even after
  it has been revoked.
* Each live handle MUST map to a recorded resource type, rights bitmask, and
  host object, and a query with a live handle MUST return all three.
* Revoking a handle MUST invalidate it so that subsequent queries reject it.
* Destroying the table MUST invalidate all remaining handles.
* Handle-space exhaustion MUST return an error rather than wrapping.

## MUST NOT

* The table MUST NOT hand out a previously used handle number.
* A query with a revoked, never-allocated, or out-of-range handle MUST NOT
  succeed.
* The table MUST NOT wrap its handle counter on exhaustion.
