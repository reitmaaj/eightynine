# libraft89 crash contract

This document is normative for the library's durability guarantees and for
the test oracle that validates them. The public contract is stated in
`spec/raft89-spec.md`; the crash oracle lives in
`test/support/crash_oracle.{c,h}`.

## 1. Domains

```text
H = durable hard state = (current_term, voted_for)
L = durable Raft log
X = durable application state
Q = network packets accepted by the transport

M = acknowledged in-memory libraft89 state
P = outstanding action + private continuation
```

For an action emitted from acknowledged state `M0`: `M = M0`, `P = action`.
The action may update `H`, `L`, `X`, or `Q`. Only successful acknowledgement
lets libraft89 advance `M`.

## 2. The barrier rule

> **Durable effects may lead acknowledged in-memory state; acknowledged
> in-memory state must never lead durable effects.**

An outstanding action is therefore a barrier. Until
`raft89_action_done(..., RAFT89_ACTION_OK)` returns, libraft89 keeps the
previous acknowledged consensus state and private pending-transition data.

A process crash destroys `M`, `P`, timers, role, `leader_id`, peer
`next_index`/`match_index`, and queued successor actions. It does not destroy
whatever the relevant durable subsystem already committed.

Restart from `(H, L)` always constructs:

```text
role             = FOLLOWER
current_term     = H.current_term
voted_for        = H.voted_for
leader_id        = NONE
last_log_index   = last_index(L)
last_log_term    = last_term(L)
commit_index     = 0
applied_index    = 0
outstanding      = none
faulted          = 0
election timer   = newly initialized
leader peer state = inactive
```

`commit_index` and `applied_index` are deliberately volatile in v1. That
decision drives the `APPLY` replay rule in section 7.

## 3. Action-state enum

The public API distinguishes only "no action outstanding" and "one action
outstanding"; the effect lifecycle is host-side. The oracle names the
phases so every crash point is explicit:

```c
/* test/support/crash_oracle.h - test-only, never installed. */
enum oracle_phase
{
    ORACLE_IDLE = 0,     /* no outstanding action                     */
    ORACLE_ISSUED,       /* library emitted action; effect not begun  (C0) */
    ORACLE_EFFECTING,    /* effect started; may be partial            (CX) */
    ORACLE_EFFECT_DONE,  /* durable/external effect complete          (C1) */
    ORACLE_ACKED         /* action_done returned; continuation ran    (C2) */
};

enum oracle_crash_point
{
    ORACLE_CRASH_BEFORE_EFFECT = 0,  /* C0 */
    ORACLE_CRASH_DURING_EFFECT,      /* CX */
    ORACLE_CRASH_AFTER_EFFECT,       /* C1 */
    ORACLE_CRASH_AFTER_ACK          /* C2 */
};
```

Transitions:

```text
IDLE --peek OK--> ISSUED
ISSUED --effect_begin--> EFFECTING
EFFECTING --effect_finish--> EFFECT_DONE
ISSUED --ack(LOST)--> IDLE              (SEND only; effect never ran)
EFFECT_DONE --ack(OK)--> IDLE           (continuation may re-issue)
EFFECT_DONE --ack(FATAL)--> faulted
any phase --crash--> volatile discarded; restart re-enters IDLE
```

A crash during `action_done()` is C1/C2 from the restart perspective because
`action_done()` changes only volatile state. For completed durable actions,
`restart(C1) == restart(C2)`.

## 4. Per-action durable effects

| Action | Effect | Allowed partial state during CX |
|---|---|---|
| `HARD_STATE` | atomic write of `(term, voted_for)` | old `H0` **or** complete `H1`; never torn |
| `LOG_APPEND` | append consecutive entries | `L0 ++ prefix(E,k)`, `0 <= k <= |E|`; contiguous, no gaps, no corruption |
| `LOG_TRUNCATE` | remove suffix from `first_index` | suffix removed down to `r`, `first-1 <= r <= old_last`; never past the boundary |
| `SEND` | transport accepts one message | packet absent, or exactly one complete packet accepted |
| `APPLY` | application effect for one entry | `X0`, or complete `apply(X0,e)`; never half-applied |

Emission preconditions the oracle asserts:

- `LOG_TRUNCATE.first_index > commit_index` and `> applied_index`;
- `APPLY.index == applied_index + 1` and `APPLY.index <= commit_index`;
- a granted-vote `SEND` is issued only after the matching `HARD_STATE` ack;
- a successful `AppendEntries` response is issued only after the required
  truncate/append acks.

## 5. Crash points

For every action, four crash points exist:

```text
C0  action emitted, effect not started
CX  crash during effect (partial durable state allowed as in section 4)
C1  effect completed, action_done not called
C2  action_done completed, before any successor action effect
```

Worked example, vote granting:

```text
H0 = (6, 0), target H1 = (7, 3)

receive RequestVote(term=7, candidate=3)
        |
        v
HARD_STATE(7, 3)                     (C0 crash => H = H0)
        |
        v
durable atomic write                 (CX crash => H in {H0, H1})
        |
        v
action_done(OK)                      (C1 crash => H = H1, no response sent)
        |
        v
SEND(vote_granted=1, term=7)         (C2 crash => H = H1, response may be in Q)
```

No execution may send `vote_granted = 1` for term 7 until `H1` is durably
complete. Likewise, a follower never sends a successful `AppendEntries`
response before its truncate/append effects completed:

```text
receive AppendEntries
        |
        v
LOG_TRUNCATE(first=4) --> ack
        |
        v
LOG_APPEND(4:d, 5:d) --> ack
        |
        v
SEND(success)
```

## 6. Crash-oracle pseudocode

The oracle drives real libraft89 nodes. It owns the durable fake store, the
fake application, the packet queue, and the phase of each outstanding action.

```c
typedef struct oracle_node
{
    raft89 *raft;                 /* NULL while crashed */
    fake_store store;             /* durable H, L */
    fake_app app;                 /* durable X + dedup table */
    unsigned long incarnation;
} oracle_node;

typedef struct oracle_cluster
{
    oracle_node node[N];
    packet_queue q;               /* deep copies; delivery gated by edge[][] */
    int edge[N][N];
    enum oracle_phase phase[N];
    const raft89_action *action[N];
    trace tr;
} oracle_cluster;

/* --- driving ---------------------------------------------------------- */

void oracle_peek(oracle_cluster *c, int i)
{
    const raft89_action *a;
    int rc;
    rc = raft89_next_action(c->node[i].raft, &a);
    if (rc == RAFT89_OK)
    {
        assert(c->phase[i] == ORACLE_IDLE);
        c->phase[i] = ORACLE_ISSUED;
        c->action[i] = a;
        return;
    }
    assert(rc == RAFT89_EMPTY);
    c->phase[i] = ORACLE_IDLE;
}

void oracle_effect_begin(oracle_cluster *c, int i, partial_token *t)
{
    assert(c->phase[i] == ORACLE_ISSUED);
    c->phase[i] = ORACLE_EFFECTING;
    *t = choose_partial(c->action[i]);
}

void oracle_effect_finish(oracle_cluster *c, int i, partial_token t)
{
    assert(c->phase[i] == ORACLE_EFFECTING);
    durable_apply(&c->node[i], c->action[i], t);
    c->phase[i] = ORACLE_EFFECT_DONE;
}

void oracle_ack(oracle_cluster *c, int i, enum raft89_action_result r)
{
    int rc;
    assert(c->phase[i] == ORACLE_EFFECT_DONE);
    rc = raft89_action_done(c->node[i].raft, c->action[i]->id, r);
    assert(rc == RAFT89_OK);
    c->phase[i] = ORACLE_IDLE;
    c->action[i] = NULL;
    oracle_drain(c, i);
}

void oracle_drain(oracle_cluster *c, int i)
{
    for (;;)
    {
        partial_token t;
        oracle_peek(c, i);
        if (c->phase[i] == ORACLE_IDLE)
        {
            break;
        }
        oracle_effect_begin(c, i, &t);
        oracle_effect_finish(c, i, t);
        oracle_ack(c, i, RAFT89_ACTION_OK);
    }
}

/* --- crash and restart ------------------------------------------------ */

void oracle_crash(oracle_cluster *c, int i, enum oracle_crash_point p,
                  partial_token t)
{
    switch (p)
    {
    case ORACLE_CRASH_BEFORE_EFFECT:
        assert(c->phase[i] == ORACLE_ISSUED);
        break;
    case ORACLE_CRASH_DURING_EFFECT:
        assert(c->phase[i] == ORACLE_EFFECTING);
        durable_apply_partial(&c->node[i], c->action[i], t);
        break;
    case ORACLE_CRASH_AFTER_EFFECT:
        assert(c->phase[i] == ORACLE_EFFECT_DONE);
        break;
    case ORACLE_CRASH_AFTER_ACK:
        assert(c->phase[i] == ORACLE_IDLE);
        break;
    }
    trace_crash(&c->tr, i, p, t);
    raft89_destroy(c->node[i].raft);
    c->node[i].raft = NULL;
    c->phase[i] = ORACLE_IDLE;
    c->action[i] = NULL;
    c->node[i].incarnation++;
}

void oracle_restart(oracle_cluster *c, int i)
{
    raft89_config cfg;
    cfg = build_config(c, i, &c->node[i].store);
    assert(raft89_create(&cfg, &c->node[i].raft) == RAFT89_OK);
    oracle_expect_recovered(c, i);
    oracle_drain(c, i);
}

void oracle_expect_recovered(oracle_cluster *c, int i)
{
    raft89_status s;
    assert(raft89_status_get(c->node[i].raft, &s) == RAFT89_OK);
    assert(s.role == RAFT89_FOLLOWER);
    assert(s.faulted == 0);
    assert(s.current_term == c->node[i].store.hard.current_term);
    assert(s.voted_for == c->node[i].store.hard.voted_for);
    assert(s.leader_id == RAFT89_ID_NONE);
    assert(s.last_log_index == store_last_index(&c->node[i].store));
    assert(s.commit_index == 0);
    assert(s.applied_index == 0);
}

/* --- durable effect model --------------------------------------------- */

void durable_apply_partial(oracle_node *n, const raft89_action *a,
                           partial_token t)
{
    switch (a->type)
    {
    case RAFT89_ACT_HARD_STATE:
        n->store.hard = t.hard;                    /* H0 or H1 only */
        break;
    case RAFT89_ACT_LOG_APPEND:
        store_append_prefix(&n->store, a, t.k);
        break;
    case RAFT89_ACT_LOG_TRUNCATE:
        store_truncate_to(&n->store, t.r);
        break;
    case RAFT89_ACT_SEND:
        if (t.sent)
        {
            queue_copy(&n->q, &a->u.send.message);
        }
        break;
    case RAFT89_ACT_APPLY:
        if (t.applied)
        {
            app_apply_dedup(&n->app, &a->u.apply.entry);
        }
        break;
    }
}

/* --- every transition is followed by the invariant engine ------------- */

void oracle_step(oracle_cluster *c, const event *e)
{
    apply_event(c, e);
    check_invariants(c);            /* I01..I20 */
}
```

`choose_partial` is deterministic in unit tests (explicit tokens) and
enumerates both choices in exhaustive mode. Non-partial `durable_apply` uses
the maximal token.

## 7. APPLY deduplication

libraft89 provides **at-least-once application keyed by log index**, never
exactly-once application.

Library guarantees:

- `APPLY` is emitted only for `index == applied_index + 1` and
  `index <= commit_index`, and only after the entry is durable in the log;
- `applied_index` advances only on `action_done(APPLY, OK)`;
- no two `APPLY` actions ever carry different `(term, payload)` for the same
  index; the payload always equals the durable log entry at that index;
- on restart `applied_index = 0`, so any already-applied index may be
  re-emitted;
- `APPLY + RAFT89_ACTION_LOST` is rejected with `RAFT89_ERR_STATE`;
  `APPLY + RAFT89_ACTION_FATAL` faults the node.

Host requirements:

- the FSM mutation and the dedup record update are one atomic durable
  transaction;
- a replay with matching `(index, term, payload hash)` is a no-op success; a
  mismatch is fatal corruption;
- the host need not retain payload bytes for old entries; a hash suffices.

Reference oracle implementation:

```c
void app_apply_dedup(fake_app *app, const raft89_entry *e)
{
    unsigned long h;
    h = hash(e->data, e->size);
    if (e->index > app->last_applied)
    {
        assert(e->index == app->last_applied + 1);
        app->state = fsm_step(app->state, e);
        app->rec[e->index].term = e->term;
        app->rec[e->index].hash = h;
        app->last_applied = e->index;
        return;
    }
    assert(e->index <= app->last_applied);
    assert(app->rec[e->index].term == e->term);
    assert(app->rec[e->index].hash == h);
}
```

Hosts may skip re-executing a known index (verify, then acknowledge `OK`);
the library still advances its volatile `applied_index`.

## 8. Normative crash matrix

| Action | before effect | during effect | after effect / pre-ACK | after ACK |
|---|---|---|---|---|
| `HARD_STATE` | old `H` | old or complete new `H` | new `H` | new `H` |
| `LOG_APPEND` | old log | old log + any complete prefix | full appended log | full appended log |
| `LOG_TRUNCATE` | old log | prefix ending in `[k-1, old_last]` | exact prefix through `k-1` | exact prefix through `k-1` |
| `SEND` | packet absent | packet absent or one complete packet | packet accepted | packet accepted / later network outcome |
| `APPLY` | old app state | old or complete new app state | new app state | new app state |

For all five actions, restart also means: pending action lost, continuation
lost, `role = FOLLOWER`, `leader_id = NONE`, `commit_index = 0`,
`applied_index = 0`; hard state and log come from recovered durable storage.

## 9. Named crash properties

```text
CR01 hard state never tears term from vote
CR02 no granted vote escapes before hard-state durability
CR03 append crash recovery always leaves a valid log prefix
CR04 successful AppendEntries response cannot escape before
     required truncate/append durability
CR05 truncation never crosses the action's requested boundary
CR06 truncation never targets a currently known committed entry
CR07 sender crash cannot imply a packet was definitely absent
CR08 duplicate semantic Raft messages remain harmless
CR09 APPLY crash recovery exposes pre-command or complete-command state
CR10 APPLY may replay after every process restart
CR11 repeated APPLY of the same index carries identical term and payload
CR12 crash after effect/pre-ACK and crash after ACK recover identically for
     HARD_STATE, completed LOG_APPEND, completed LOG_TRUNCATE, and APPLY
CR13 no acknowledgement needs persistence of its own
```

Each property maps to tests under `test/crash/` and to invariants in
`test/support/invariants.c`.
