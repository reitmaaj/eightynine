# 0002 — libfsm89 semantics

## Definition invariants

`fsm89_validate()` proves structural consistency and determinism. It does not
impose higher-level modeling policy such as reachability or terminal
accepting states.

Required:

```text
state_count > 0
states != NULL
edge_count > 0  =>  edges != NULL
effects.n > 0   =>  effects.v != NULL
```

Permitted: `edge_count == 0` with either `edges == NULL` or a non-NULL
pointer; empty effect spans with either `NULL` or non-NULL `v`; a one-state
machine with no transitions.

For every pair of state definitions `i != j`, `states[i].id != states[j].id`.
Even identical duplicate definitions fail. The state ID is the unique key.

Exactly one state definition satisfies `id == def.initial`. Uniqueness
follows from the duplicate-state rule. A missing initial state is
`FSM89_EINITIAL`.

For every edge, `edge.from` and `edge.to` must name defined states. No
implicit state creation occurs. A missing source is `FSM89_EEDGE_FROM`; a
missing destination is `FSM89_EEDGE_TO`.

The unique edge key is `(from, event)`; destination and effects are not part
of it. Duplicate keys are `FSM89_EDUP_EDGE` even when the rows are identical.
Multiple events may lead to the same destination; the same event from
different sources is fine.

Self-transitions are valid. Unreachable states are valid. Accepting states
may have outgoing edges; accepting means `accepting(s) = true`, not
`outdegree(s) = 0`. Non-accepting states may lack outgoing edges. Accepting
initial states are valid. Effect lists may contain duplicates and keep
declaration order. Effect IDs need no registry. Event IDs gain meaning only
by appearing in an edge.

## Validation order

The first detected error is returned:

```text
1. def == NULL, state_count == 0, states == NULL, or
   (edge_count > 0 and edges == NULL)      -> FSM89_EDEF
2. state leave/enter span with n > 0 and v == NULL -> FSM89_EEFFECTS
3. duplicate state ID                      -> FSM89_EDUP_STATE
4. initial state absent                    -> FSM89_EINITIAL
5. edge effects span with n > 0 and v == NULL -> FSM89_EEFFECTS
6. edge source state absent                -> FSM89_EEDGE_FROM
7. edge destination state absent           -> FSM89_EEDGE_TO
8. duplicate (from, event) edge            -> FSM89_EDUP_EDGE
9. otherwise                               -> FSM89_OK
```

Table ordering determines which error is reported when several defects
coexist; any valid definition receives `FSM89_OK`.

## Transition semantics

For `A --x--> B` with `A != B`, `fsm89_step(def, A, x, &r)` returns
`FSM89_OK` and populates:

```text
r.from  = A
r.event = x
r.to    = B
r.leave = A.leave
r.edge  = edge(A, x).effects
r.enter = B.enter
```

The logical emission order is `r.leave || r.edge || r.enter`. For a
self-transition `A --x--> A` the same rule applies: `A.leave`,
`edge(A,x).effects`, `A.enter`. Leave and enter are never suppressed because
source equals destination.

For a missing transition, `fsm89_step()` returns `FSM89_NO_TRANSITION` and
emits nothing: no leave, no edge, no enter. For an unknown current state it
returns `FSM89_ESTATE` and emits nothing. `*result` is modified if and only
if the function returns `FSM89_OK`; otherwise every field keeps its previous
value (field values, not padding bytes).

`fsm89_step()` executes no effect, never mutates caller state, and is safe to
call repeatedly with identical inputs.

## Startup semantics

For initial state `I`, `fsm89_start(def, &state, &enter)` returns
`FSM89_OK`, sets `*state = I`, and sets `*enter = I.enter`. No leave or edge
emission occurs. `*state` and `*enter` are modified only on `FSM89_OK`.

## Acceptance

`fsm89_accepting(def, state)` returns nonzero exactly when `state` names a
state whose `accepting` field is nonzero, and `0` when the state is not
defined. Accepting carries no finality or termination semantics.

## Purity and borrowing

Every span in a successful result and in `fsm89_start()` borrows storage from
the definition. No operation allocates, performs I/O, calls a callback,
executes an effect, or reads or writes hidden global state.
