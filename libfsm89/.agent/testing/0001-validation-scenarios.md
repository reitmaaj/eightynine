# 0001 — validation scenarios

## Definition-level structure

SCENARIO one state, no edges
  GIVEN a definition with one state and `state_count == 1`, `edge_count == 0`
  WHEN `fsm89_validate` is called
  THEN the result is `FSM89_OK`

SCENARIO zero states
  GIVEN `state_count == 0`
  WHEN `fsm89_validate` is called
  THEN the result is `FSM89_EDEF`

SCENARIO NULL definition
  GIVEN `def == NULL`
  WHEN `fsm89_validate` is called
  THEN the result is `FSM89_EDEF`

SCENARIO NULL state table with a nonzero count
  GIVEN `state_count > 0` and `states == NULL`
  WHEN `fsm89_validate` is called
  THEN the result is `FSM89_EDEF`

SCENARIO empty edge table
  GIVEN `edge_count == 0` with `edges == NULL` or a non-NULL pointer
  WHEN `fsm89_validate` is called
  THEN the result is `FSM89_OK` in both cases

SCENARIO NULL edge table with a nonzero count
  GIVEN `edge_count > 0` and `edges == NULL`
  WHEN `fsm89_validate` is called
  THEN the result is `FSM89_EDEF`

SCENARIO effect span with NULL storage
  GIVEN a state leave, state enter, or edge span with `n > 0` and `v == NULL`
  WHEN `fsm89_validate` is called
  THEN the result is `FSM89_EEFFECTS`

SCENARIO empty effect span
  GIVEN a span with `n == 0` and `v == NULL` or `v != NULL`
  WHEN `fsm89_validate` is called
  THEN the result is `FSM89_OK` in both cases

## States

SCENARIO duplicate state ID
  GIVEN two state definitions with equal `id`, identical or not
  WHEN `fsm89_validate` is called
  THEN the result is `FSM89_EDUP_STATE`

SCENARIO missing initial state
  GIVEN an initial ID that names no state
  WHEN `fsm89_validate` is called
  THEN the result is `FSM89_EINITIAL`

SCENARIO accepting initial state
  GIVEN the initial state carries `accepting != 0`
  WHEN `fsm89_validate` is called
  THEN the result is `FSM89_OK`

## Edges

SCENARIO missing edge source
  GIVEN an edge whose `from` names no state
  WHEN `fsm89_validate` is called
  THEN the result is `FSM89_EEDGE_FROM`

SCENARIO missing edge destination
  GIVEN an edge whose `to` names no state
  WHEN `fsm89_validate` is called
  THEN the result is `FSM89_EEDGE_TO`

SCENARIO duplicate edge key
  GIVEN two edges with equal `(from, event)` and equal or different targets
  WHEN `fsm89_validate` is called
  THEN the result is `FSM89_EDUP_EDGE`

SCENARIO shared event, different sources
  GIVEN edges `(A, X) -> B` and `(C, X) -> D`
  WHEN `fsm89_validate` is called
  THEN the result is `FSM89_OK`

SCENARIO shared source, different events
  GIVEN edges `(A, X) -> B` and `(A, Y) -> C`
  WHEN `fsm89_validate` is called
  THEN the result is `FSM89_OK`

SCENARIO self-transition
  GIVEN an edge `(A, X) -> A`
  WHEN `fsm89_validate` is called
  THEN the result is `FSM89_OK`

SCENARIO unreachable state
  GIVEN a valid initial component plus a disconnected state or component
  WHEN `fsm89_validate` is called
  THEN the result is `FSM89_OK`

SCENARIO accepting state with outgoing edges
  GIVEN an accepting state that has outgoing transitions
  WHEN `fsm89_validate` is called
  THEN the result is `FSM89_OK`

SCENARIO non-accepting terminal state
  GIVEN a non-accepting state with no outgoing transitions
  WHEN `fsm89_validate` is called
  THEN the result is `FSM89_OK`

## IDs

SCENARIO zero identifiers
  GIVEN state ID 0, event ID 0, and effect ID 0 in use
  WHEN `fsm89_validate` is called
  THEN the result is `FSM89_OK`

SCENARIO maximum identifiers
  GIVEN IDs `ULONG_MAX` and `ULONG_MAX - 1` in use
  WHEN `fsm89_validate` is called
  THEN the result is `FSM89_OK`

SCENARIO sparse identifiers
  GIVEN state IDs that are not table indices
  WHEN `fsm89_validate` is called
  THEN the result is `FSM89_OK`

## Error precedence

SCENARIO combined defects
  GIVEN machines with defects at adjacent stages of the documented order
  WHEN `fsm89_validate` is called
  THEN the first stage in the order is reported
