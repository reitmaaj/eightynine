# libledger89 stakeholders (v2)

## AS a WAL author
I WANT a durable ordered sequence with ledger-assigned LSNs and an exact
stable frontier SO THAT my BEGIN/REDO/COMMIT envelopes survive crashes and I
can replay exactly the durable prefix.

## AS a message-queue author
I WANT durable sends with a precise acknowledgement boundary SO THAT consumer
state (claim/ack/nack) lives in my own ledger or K/V store and never leaks
into the storage substrate.

## AS an IPC-ledger author
I WANT process multiplexing and follow semantics over a durable record
sequence SO THAT lost wakeups are harmless because `stable_end` supplies
truth.

## AS a query-engine author
I WANT `{ledger uuid, revision, next}` checkpoints plus `read`/iterate SO THAT
I can rebuild projections exactly and detect index reuse after truncation.

## AS a libraft89 integrator
I WANT compare-and-append (`appendv_at`), suffix truncation, and an exact
local durability point SO THAT I can implement `LOG_APPEND`, `LOG_TRUNCATE`,
and `ACTION_OK` without merging `stable_end` with `commit_index`.

## AS a safety engineer
I WANT torn tails discarded, stable-history corruption reported, and every
crash resolved to one published topology SO THAT no partial batch, gap, or
resurrected record is ever visible.

## AS a retention owner
I WANT a pruning mechanism that removes only complete sealed parts SO THAT I
choose the policy while the ledger executes it crash-safely without changing
surviving index meanings.

## AS a test author
I WANT a deterministic model filesystem, crash injection points, and a
reference model SO THAT every crash point and byte-level corruption is
checked without relying on wall-clock timing.

## AS an operator
I WANT exclusive writer ownership, shared read-only opens, and a verification
command SO THAT concurrent processes cannot corrupt a ledger and media damage
is detected rather than repaired silently.
