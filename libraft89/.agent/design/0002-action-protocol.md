# Action protocol

The action protocol is the entire integration surface for effects. It is
intentionally small: one action at a time, acknowledged synchronously.

## Host loop

```c
r = raft89_recv(node, &msg);
if (r != RAFT89_OK)
{
    return r;
}

for (;;)
{
    const raft89_action *a;
    r = raft89_next_action(node, &a);
    if (r == RAFT89_EMPTY)
    {
        break;
    }
    if (r != RAFT89_OK)
    {
        return r;
    }
    ar = execute_action(a);
    r = raft89_action_done(node, a->id, ar);
    if (r != RAFT89_OK)
    {
        return r;
    }
}
```

## Action meanings

| Action | Host effect | Result |
|---|---|---|
| `RAFT89_ACT_SEND` | transport accepts the message | `OK` accepted, `LOST` treated as packet loss |
| `RAFT89_ACT_HARD_STATE` | atomically persist `(current_term, voted_for)` | `OK` only after durability |
| `RAFT89_ACT_LOG_APPEND` | durably append consecutive entries | `OK` after durability |
| `RAFT89_ACT_LOG_TRUNCATE` | durably remove entries `>= first_index` | `OK` after durability |
| `RAFT89_ACT_APPLY` | apply one committed entry to the FSM | `OK` after atomic application |

`LOST` on any action except `SEND` is rejected with `RAFT89_ERR_STATE`.
`FATAL` on any action faults the node.

## Ordering guarantees

The library emits actions in an order that preserves every Raft durability
barrier:

- hard state before a vote response or a higher-term step-down reply;
- log truncate before the replacement append;
- log append before a successful AppendEntries response;
- local append before replicating a proposal;
- committed-log durability before `APPLY`.

A host that executes each action to completion before acknowledging cannot
reorder those barriers.

## Lifetimes

- Entry payloads passed to `raft89_recv`/`raft89_propose` need remain valid
  only for the duration of the call; the library copies what it needs.
- Payloads reachable from an outstanding action remain valid until
  `raft89_action_done` or `raft89_destroy`.
- After acknowledgement, all action pointers are invalid.

## Status versus actions

`raft89_status_get` reports diagnostics only. `commit_index` there does not
mean the host may apply entries; only `RAFT89_ACT_APPLY` does. A host must
never infer apply order from status.
