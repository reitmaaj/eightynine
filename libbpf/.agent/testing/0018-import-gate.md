# 0018 - Testing: capability-gated import (helper) dispatch

*BDD scenarios for a machine that owns a capability table and only lets a
helper (import) call suspend after validating that the capability handle passed
in r1 is live and grants the resource type and permission the host requires.
A helper with no declared requirement suspends ungated; a requirement mismatch
traps without any dispatch.*

## 0018-001 Registered capabilities

SCENARIO: The host registers a capability with a type, rights, and host object
GIVEN a machine
WHEN the host registers a capability
THEN it receives a live handle it can pass to a helper.

## 0018-002 Valid capability grants a helper

SCENARIO: A helper call with a satisfying capability suspends
GIVEN a machine whose host required a type and rights
AND r1 holds a live handle granting that type and rights
WHEN a helper (import) call is executed
THEN the machine reports WAITING with the import id recorded.

## 0018-003 Wrong resource type is rejected (unacceptable behaviour)

SCENARIO: A capability of the wrong type does not grant a helper
GIVEN a host that requires a stream capability
AND r1 holds a live handle of a different type
WHEN the helper call is executed
THEN it traps without suspending.

## 0018-004 Insufficient permission is rejected (unacceptable behaviour)

SCENARIO: A capability without the required read permission is rejected
GIVEN a host that requires READ on a capability
AND r1 holds a live handle of the right type without READ
WHEN the helper call is executed
THEN it traps.

## 0018-005 Revoked or invalid handle is rejected (unacceptable behaviour)

SCENARIO: A revoked handle does not grant a helper
GIVEN a handle that was registered and then revoked
WHEN the helper call passes it
THEN it traps.

## 0018-006 Ungated helper

SCENARIO: A helper with no declared requirement suspends
GIVEN a machine with no required capability
WHEN a helper call is executed
THEN it reports WAITING regardless of r1.
