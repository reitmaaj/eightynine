# 0001 — libfsm89 stories

## S1 — protocol and controller author

AS a developer of protocol machines, parsers, and lifecycle controllers
I WANT a deterministic transition table that maps `(state, event)` to a next
state plus declarative effects
SO THAT control flow is data, testable without mocks, and reusable across
recognizers, transducers, and effect-driving controllers.

## S2 — effect interpreter

AS the host that owns I/O, queues, timers, and application objects
I WANT transitions evaluated without executing or interpreting effects
SO THAT I decide when to commit the new state and how to execute, batch,
replay, or fail each emission.

## S3 — test and verification author

AS a test author
I WANT the machine definition to be immutable, caller-owned data with no
allocation and no hidden global state
SO THAT I can compare transitions field-for-field, exhaustively enumerate
small machines, and verify replay determinism.

## S4 — library integrator

AS an author of sibling `lib*89` libraries
I WANT a dependency-free core with borrowed effect spans
SO THAT recognizers (for example lexical recognition) and controllers (for
example lifecycle machines) can embed `libfsm89` without adopting I/O,
threads, or a runtime.

## S5 — effect-typing integrator

AS a user of `libfx89`
I WANT a later optional adapter that maps `fsm89_effect` identifiers to
`fx89` atoms and checks that every emission of a machine lies inside a
declared effect row
SO THAT effect typing and effect execution stay separate concerns.
