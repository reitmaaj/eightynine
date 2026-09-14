# 0003 — startup and acceptance scenarios

## Startup

SCENARIO initial state and enter span
  GIVEN a validated machine with initial state `I`
  WHEN `fsm89_start(def, &state, &enter)` is called
  THEN `FSM89_OK` is returned, `*state == I`, and `*enter == I.enter`

SCENARIO empty enter span
  GIVEN an initial state with `enter.n == 0`
  WHEN `fsm89_start` is called
  THEN the returned span is empty and `FSM89_OK` is returned

SCENARIO startup emits no leave
  GIVEN an initial state with `leave = [1]` and `enter = [2]`
  WHEN `fsm89_start` is called
  THEN the returned span is exactly `[2]`, never `[1, 2]`

SCENARIO startup does not traverse
  GIVEN an initial state with outgoing and self-loop transitions
  WHEN `fsm89_start` is called
  THEN no transition is evaluated and no edge effect is emitted

SCENARIO startup with accepting flag
  GIVEN an initial state with either accepting value
  WHEN `fsm89_start` is called
  THEN the result is unaffected by the accepting flag

SCENARIO machine without edges
  GIVEN a one-state machine with no transitions
  WHEN `fsm89_start` is called
  THEN `FSM89_OK` is returned

SCENARIO borrowed enter span
  GIVEN a validated machine
  WHEN `fsm89_start` is called
  THEN `enter.v` equals the initial state's `enter.v`

## Acceptance

SCENARIO accepting state
  GIVEN a state with `accepting != 0`
  WHEN `fsm89_accepting` is called
  THEN the result is nonzero

SCENARIO non-accepting state
  GIVEN a state with `accepting == 0`
  WHEN `fsm89_accepting` is called
  THEN the result is 0

SCENARIO accepting is a state property
  GIVEN accepting states with outgoing edges and accepting self-loops, and
    transitions between accepting and non-accepting states
  WHEN `fsm89_accepting` is called
  THEN only the state's own `accepting` field decides the result

SCENARIO unknown state
  GIVEN a state ID absent from the definition
  WHEN `fsm89_accepting` is called
  THEN the result is 0

SCENARIO accepting is not terminal
  GIVEN an accepting state with outgoing edges
  WHEN the machine is validated and the state queried
  THEN validation is `FSM89_OK` and acceptance is nonzero
