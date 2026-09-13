# libledger89 stakeholders

## AS a libraft89 integrator

I WANT a local durable sequence with atomic append batches, an explicit sync
barrier, and suffix truncation
SO THAT I can map `LOG_APPEND`, `LOG_TRUNCATE`, and durable barriers onto
storage without teaching the consensus core about files.

## AS an application developer

I WANT opaque records identified by a contiguous index with a coarse tag
SO THAT I can define my own event format without inheriting a schema.

## AS a query-engine author

I WANT deterministic iteration and random reads by index
SO THAT I can rebuild projections from the ledger alone after any failure,
without trusting an advisory live-ingestion hook.

## AS a safety engineer

I WANT crash points fixed per persistence boundary, sealed segments immutable,
and torn tails distinguished from corruption
SO THAT I can reason about exactly which records survive a crash and never
silently skip damaged history.

## AS a retention owner

I WANT explicit prefix discard with no automatic policy
SO THAT a permanent audit ledger never loses history while a replicated log
can compact its prefix deliberately.

## AS a test author

I WANT an injectable I/O boundary and a deterministic model filesystem
SO THAT crashes, torn writes, ENOSPC, and allocation failure can be replayed
exactly against a reference model.

## AS an operator

I WANT structural errors (corruption, I/O, sequence) distinct from ordinary
failures
SO THAT monitoring can tell a damaged ledger from a transient fault.
