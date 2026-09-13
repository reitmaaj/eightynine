# 0017 - Testing: capability table

*BDD scenarios for the machine-local capability table that turns integer
guest handles into authority. Handles are allocated monotonically and never
reused during a table's lifetime; exhaustion returns an error instead of
wrapping. Revoking a handle invalidates it; destroying the table invalidates
every entry.*

## 0017-001 Monotonic allocation

SCENARIO: Handles are allocated in increasing order
GIVEN a fresh capability table
WHEN capabilities are allocated
THEN each new handle is strictly greater than the previous one.

## 0017-002 No reuse after revocation

SCENARIO: A revoked handle number is never handed out again (unacceptable to
          reuse)
GIVEN a capability that is allocated then revoked
WHEN the table is asked for a new capability
THEN it receives a fresh, larger handle rather than the revoked one.

## 0017-003 Exhaustion

SCENARIO: Handle space exhaustion returns an error rather than wrapping
       (unacceptable behaviour)
GIVEN a table whose handle space is exhausted
WHEN a capability is allocated
THEN it is rejected with an exhaustion error.

## 0017-004 Lookup and revocation

SCENARIO: A live handle resolves to its type, rights, and host object
GIVEN a capability allocated with a type, rights, and host object
WHEN the table is queried with that handle
THEN it returns the recorded type, rights, and host object.

SCENARIO: A revoked handle no longer resolves (unacceptable behaviour)
GIVEN a capability that has been revoked
WHEN the table is queried with that handle
THEN it is rejected as not a live handle.

SCENARIO: A handle never allocated is rejected (unacceptable behaviour)
GIVEN an out-of-range or never-allocated handle
WHEN the table is queried
THEN it is rejected.

## 0017-005 Destruction

SCENARIO: Destroying the table invalidates all entries
GIVEN a table with live capabilities
WHEN the table is destroyed
THEN every handle is invalid (the table no longer exists).
