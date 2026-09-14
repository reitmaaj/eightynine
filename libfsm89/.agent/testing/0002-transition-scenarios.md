# 0002 — transition scenarios

## Successful transitions

SCENARIO ordinary transition
  GIVEN a validated machine with `A --X--> B`, `A != B`
  WHEN `fsm89_step(def, A, X, &r)` is called
  THEN `FSM89_OK` is returned and `r.from == A`, `r.event == X`,
    `r.to == B`, `r.leave == A.leave`, `r.edge == edge.effects`, and
    `r.enter == B.enter`

SCENARIO self-transition
  GIVEN a validated machine with `A --X--> A`
  WHEN `fsm89_step(def, A, X, &r)` is called
  THEN `FSM89_OK` is returned and the result contains `A.leave`,
    `edge.effects`, and `A.enter`, in that order

SCENARIO silent transition
  GIVEN a transition whose leave, edge, and enter spans are all empty
  WHEN `fsm89_step` is called
  THEN `FSM89_OK` is returned, the state changes, and the flattened effect
    sequence is empty

SCENARIO repeated evaluation
  GIVEN the same machine, state, and event
  WHEN `fsm89_step` is called twice
  THEN both results are field-for-field equal

## Effect ordering

SCENARIO all emission sites
  GIVEN `A.leave = [10, 11]`, edge `= [20, 21]`, `B.enter = [30, 31]`
  WHEN `fsm89_step` is called
  THEN the flattened sequence is `[10, 11, 20, 21, 30, 31]`

SCENARIO partial emission sites
  GIVEN any single-site or two-site combination of leave, edge, and enter
  WHEN `fsm89_step` is called
  THEN exactly the declared effects appear in leave-then-edge-then-enter
    order

SCENARIO duplicated effects
  GIVEN `leave = [7, 7]`, `edge = [7]`, `enter = [7, 7]`
  WHEN `fsm89_step` is called
  THEN the flattened sequence is `[7, 7, 7, 7, 7]` with no deduplication

SCENARIO self-transition emission matrix
  GIVEN `A --X--> A` with every presence/absence combination of leave, edge,
    and enter effects
  WHEN `fsm89_step` is called
  THEN the flattened sequence is exactly the declared combination and enter
    and leave are never suppressed

## Missing transitions

SCENARIO state without outgoing edges
  GIVEN a valid state with no outgoing transitions
  WHEN `fsm89_step` is called
  THEN `FSM89_NO_TRANSITION` is returned and `*result` is unchanged

SCENARIO unmatched event
  GIVEN a state whose edges do not carry the event
  WHEN `fsm89_step` is called
  THEN `FSM89_NO_TRANSITION` is returned and `*result` is unchanged

SCENARIO boundary event IDs
  GIVEN event 0 or `ULONG_MAX` absent from the table
  WHEN `fsm89_step` is called
  THEN `FSM89_NO_TRANSITION` is returned and `*result` is unchanged

## Invalid current state

SCENARIO unknown state
  GIVEN a state ID absent from the definition, including a gap between
    defined IDs and `ULONG_MAX`
  WHEN `fsm89_step` is called
  THEN `FSM89_ESTATE` is returned and `*result` is unchanged

SCENARIO distinction
  GIVEN the same machine
  WHEN an unknown state and an unmatched event are evaluated
  THEN `FSM89_ESTATE` and `FSM89_NO_TRANSITION` are distinguished

## Borrowing and purity

SCENARIO borrowed spans
  GIVEN a successful transition
  WHEN the result spans are inspected
  THEN each span points into the definition storage, not a copy

SCENARIO definition immutability
  GIVEN a snapshot of the whole definition
  WHEN validate, start, step success, step failure, and accepting run
  THEN every definition field and effect array is unchanged

SCENARIO interleaved machines
  GIVEN two independent machines
  WHEN operations interleave
  THEN each result equals its isolated-run result

SCENARIO table-order independence
  GIVEN semantically identical machines with reordered state and edge tables
  WHEN every `(state, event)` pair is evaluated
  THEN the results are equivalent

SCENARIO renaming invariance
  GIVEN bijective renumbering of state, event, and effect IDs
  WHEN the renamed machine is evaluated
  THEN `f(T(s,e)) == T'(f(s),g(e))` wherever the transition exists
