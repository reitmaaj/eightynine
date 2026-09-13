# libraft89 stakeholders

## AS a distributed-systems developer

I WANT a small consensus library that implements Raft and only Raft
SO THAT I can embed it in a service without inheriting a transport,
storage engine, or runtime.

## AS a host implementer

I WANT storage reads as callbacks and storage writes as acknowledged actions
SO THAT I can keep my existing log and database as the single source of
truth and control durability myself.

## AS a safety engineer

I WANT crash semantics fixed per action type, with hard state atomic and log
effects prefix-safe
SO THAT I can reason about what survives a crash without implementing
transactions the protocol does not need.

## AS an application author

I WANT committed entries delivered in index order with a stable identity
SO THAT I can build a replicated state machine with at-least-once apply and
index-keyed deduplication.

## AS a test author

I WANT deterministic event processing with injected time and randomness
SO THAT I can simulate partitions, crashes, restarts, and stale messages and
replay any failure exactly.

## AS an operator

I WANT diagnostic status without protocol access
SO THAT I can observe role, term, leader, and progress for monitoring.
