# 0001 — libfsm89 concept

`libfsm89` models a small deterministic labelled transition system with
optional emissions attached to states and edges. One abstraction covers
recognizers, Moore/Mealy transducers, protocol machines, parsers, and
effect-driving controllers without becoming a workflow engine.

The key separation:

```text
(state, event) -> (next state, emissions)
```

`libfsm89` determines that mapping. It does not execute I/O, maintain queues,
schedule timers, allocate application objects, or interpret emissions.

## Semantic core

A machine is `M = (S, E, T, s0)` with a deterministic partial transition
relation

```text
T : S x E -> S
```

and three optional emission sites:

```text
X : S -> A*          state-exit emissions (leave)
Y : T -> A*          edge emissions
Z : S -> A*          state-entry emissions (enter)
```

A successful transition yields

```text
(s, e) -> (s', X(s) || Y(s, e, s') || Z(s'))
```

where `||` is ordered concatenation, in exactly that order.

| Variety          | Leave | Edge | Enter | Interpretation                    |
| ---------------- | ----: | ---: | ----: | --------------------------------- |
| DFA / recognizer |     - |    - |     - | state transition only             |
| Moore transducer |     - |    - |     x | destination state produces output |
| Mealy transducer |     - |    x |     - | transition produces output        |
| mixed transducer |     - |    x |     x | edge + destination output         |
| lifecycle FSM    |     x |    x |     x | leave -> transition -> enter      |
| protocol FSM     |     ? |    ? |     ? | effects interpreted by the host   |

The idiomatic style is "edge maps, state does effects": the edge answers
*where next?*; entering the new state tells the host *what should happen
there*.

## Effects are data

Transition tables carry effect identifiers, never callbacks:

```c
{
    STATE_IDLE,
    EVENT_CONNECT,
    STATE_CONNECTING,
    effects_connecting
}
```

The application interprets `SEND_SYN`. This keeps transition evaluation pure
and gives deterministic tests without mocks, replay, logging, serialization,
effect inspection, effect batching, execution by another subsystem, and easy
failure injection.

## libfx89 is not a dependency

`libfx89` concerns effect inference and effect descriptions, not execution. A
future, separate adapter may map effect identifiers to `fx89` atoms and check
`effects(M) subset R`, but `libfsm89` itself depends on no other library.

## No mutable FSM object

The library never owns runtime state:

```c
rc = fsm89_step(&def, state, event, &r);
if (rc == FSM89_OK) {
    state = r.to;
}
```

`libfsm89` calculates a transition; the caller decides whether and when to
commit it. An emitted effect failing is an application policy question
(roll back, retry, error state, stay committed); no fake transactional
semantics enter the library.

## Non-goals

Hierarchical states, orthogonal states, nondeterminism, guards, callbacks,
transition priorities, event queues, timers, threads, asynchronous
execution, effect handlers, arbitrary payloads, dynamic graph mutation,
persistence, serialization, allocation, rollback, and nested FSM execution
all live outside the primitive. Timers reduce to an external subsystem
submitting `EVENT_TIMEOUT`. Composition happens through event/effect
adapters between machines.
