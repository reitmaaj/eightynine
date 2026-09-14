# 0001 — libfsm89 design

## Types

```c
typedef unsigned long fsm89_state;
typedef unsigned long fsm89_event;
typedef unsigned long fsm89_effect;

typedef struct fsm89_effects
{
    const fsm89_effect *v;
    size_t n;
} fsm89_effects;

typedef struct fsm89_state_def
{
    fsm89_state id;
    fsm89_effects leave;
    fsm89_effects enter;
    int accepting;
} fsm89_state_def;

typedef struct fsm89_edge
{
    fsm89_state from;
    fsm89_event event;
    fsm89_state to;
    fsm89_effects effects;
} fsm89_edge;

typedef struct fsm89_def
{
    const fsm89_state_def *states;
    size_t state_count;
    const fsm89_edge *edges;
    size_t edge_count;
    fsm89_state initial;
} fsm89_def;

typedef struct fsm89_step_result
{
    fsm89_state from;
    fsm89_event event;
    fsm89_state to;
    fsm89_effects leave;
    fsm89_effects edge;
    fsm89_effects enter;
} fsm89_step_result;
```

IDs are deliberately abstract integer names. `0` and `ULONG_MAX` are ordinary
values; no sentinel exists because counts accompany arrays.

## Status codes

```c
typedef enum fsm89_status
{
    FSM89_OK = 0,
    FSM89_NO_TRANSITION = 1,
    FSM89_ESTATE = -1,
    FSM89_EDEF = -2,
    FSM89_EEFFECTS = -3,
    FSM89_EDUP_STATE = -4,
    FSM89_EINITIAL = -5,
    FSM89_EEDGE_FROM = -6,
    FSM89_EEDGE_TO = -7,
    FSM89_EDUP_EDGE = -8
} fsm89_status;
```

`FSM89_NO_TRANSITION` is positive because it is an ordinary machine outcome,
not an API or data error. `FSM89_ESTATE` is the only runtime error.
`FSM89_EDEF` through `FSM89_EDUP_EDGE` are definition-validation errors.

## Module map

| File | Responsibility |
| --- | --- |
| `include/fsm89.h` | public types, status enum, function contracts |
| `src/fsm89_internal.h` | shared pure helpers: span validity, state lookup, edge lookup |
| `src/fsm89_lookup.c` | pure span and table lookups (`fsm89__*`) |
| `src/fsm89_validate.c` | the eight-stage definition validator |
| `src/fsm89_step.c` | one transition evaluation |
| `src/fsm89_state.c` | startup and acceptance queries |

The three `fsm89__*` helpers carry internal linkage by convention only (the
double underscore and the absence of declarations in the public header); they
are the only non-public symbols in the archive and the audit checks for
exactly that set.

## Ownership and lifetime

Everything is caller-owned and immutable:

```text
fsm89_def             caller-owned
state definitions     caller-owned
edge definitions      caller-owned
effect arrays         caller-owned

fsm89_step_result      caller-owned value
effect spans in result borrowed from the definition
```

`libfsm89` never allocates, never frees, never stores hidden state, and
returns no owned resources. A returned span remains valid only while the
definition and referenced effect arrays remain alive and unmodified. There is
no `destroy()` and no allocator parameter.

## Green constraints

The source is strict C89 and strict C23, warning-clean under GCC and Clang,
passes the `green` clang-tidy semantic checks, and uses the canonical Allman
format. In particular: no `&&`, `||`, `?:`, or comma operator in expression
position; assignments only as complete statements or `for` clauses; effectful
calls only as complete transitions; every controlled body braced; one object
per declaration; pure helpers listed in `green.yaml`.

## Build and test

`just build` produces `build/libfsm89.a`. `just test` runs the smoke, unit,
model, and generated suites in order. `just check` adds the green gate, lint,
symbol audit, API coverage, the 100% line/branch coverage gate, and the gate
self-checks. `just matrix`, `just sanitize`, and `just deep-test` provide the
compiler matrix, sanitizers, and the exhaustive N=3,E=3 sweep.

Gates must fail rather than pass vacuously. Scripts are invoked as
`sh scripts/<name>`, which ignores the shebang, so each script sets `set -eu`
in its body (`just shell-selftest`). The coverage gate requires a profile
report per source (`scripts/coverage-check.sh`), the sanitizer gate compiles
and runs known UB and address-error probes, and the build gate rebuilds after
a deleted source and checks that no stale symbol survives.
