# BDD scenarios: strict suite (v2)

Driven by `test/strict/` (Python + `strictrun` + `faultshim`) and the
table-driven additions under `test/unit/`, `test/crash/`, and `test/fault/`.
Acceptance criteria are in `.agent/acceptance/0001-strict-suite.md`; the
architecture is in `.agent/design/0007-strict-test-architecture.md`.

Every scenario below MUST have an executable case, and every case MUST live
in a named table so a reader can see the covered parameter space.

## Harness

SCENARIO ST01 smoke
  GIVEN a fresh temp directory
  WHEN the ctypes driver opens RDWR|CREATE|EXCL, appends a batch, syncs,
       reopens, reads every record, and verifies
  THEN every call returns OK and every payload round-trips byte for byte

SCENARIO ST02 runner protocol
  GIVEN a line-oriented op script
  WHEN `strictrun` executes it against the real filesystem
  THEN the transcript reports the same state and payloads as the ctypes driver

SCENARIO ST03 shim self-test
  GIVEN the fault shim loaded with a configured op and call index
  WHEN a mutation runs
  THEN exactly the configured call fails, the failure is observable, and no
       other call is affected

## API boundary tables

SCENARIO ST04 flag matrix
  GIVEN every combination of RDONLY, RDWR, CREATE, EXCL plus unknown bits
  WHEN open is called on a fresh, existing, or missing path
  THEN the result is the documented OK/EINVAL/EEXIST/ENOENT outcome and
       *ledger_out is NULL on every failure

SCENARIO ST05 NULL and empty arguments
  GIVEN NULL handles, NULL outputs, NULL slice arrays, zero counts, and
       zero-length records
  WHEN each public function is called
  THEN EINVAL or the documented result follows and the process never crashes

SCENARIO ST06 append boundaries
  GIVEN payload sizes 0, 1, 4096, 16 MiB, 16 MiB+1, SIZE_MAX and counts 0,
       1, 255, 256, UINT32_MAX+1
  WHEN appendv runs
  THEN records receive consecutive indices, limits return ERANGE, and no
       partial batch is ever visible

SCENARIO ST07 read and iterator matrix
  GIVEN every index around first/stable_end/end and every capacity around the
       record size
  WHEN read and iter_next run
  THEN EGONE/ENOENT/ETOOSMALL/DONE follow the spec, short buffers copy
       nothing, and the iterator does not advance on ETOOSMALL

SCENARIO ST08 structural tables
  GIVEN clean and dirty ledgers, truncated and pruned histories
  WHEN truncate_from, prune_before, rotate, and verify run
  THEN EUNSTABLE/EGONE/ERANGE follow the spec, revision changes only on
       truncation, and pruning never passes the requested first

SCENARIO ST09 scalar and string tables
  GIVEN boundary 64-bit values and every documented result code
  WHEN u64 helpers and strerror run
  THEN the ordering is total, arithmetic wraps nowhere, and every code maps
       to a distinct non-empty string

## State machine

SCENARIO ST10 invariants
  GIVEN any operation sequence
  WHEN each call returns
  THEN first <= stable_end <= end, revision never decreases, and the identity
       never changes

SCENARIO ST11 reopen idempotence
  GIVEN a recovered ledger
  WHEN it is closed and reopened repeatedly
  THEN the state is identical every time and the unstable tail is discarded

## On-disk format

SCENARIO ST12 independent codecs
  GIVEN a ledger built through the public API
  WHEN Python decodes CURRENT, manifests, part headers, batches, markers, and
       sealed footers field by field
  THEN every offset, magic, reserved byte, and checksum matches the v2 format

SCENARIO ST13 independent encoder
  GIVEN Python-built v2 files
  WHEN the library opens and reads them
  THEN the records decode correctly, proving the codecs agree in both
       directions

SCENARIO ST14 golden cross-check
  GIVEN the committed golden fixtures
  WHEN Python decodes them
  THEN the decoded state matches the fixture ledger

## Corruption

SCENARIO ST15 byte sweep
  GIVEN a valid multi-part ledger
  WHEN every byte of every file is replaced by 00/01/7F/80/FF and flipped bits
  THEN open returns OK with fallback, ECORRUPT, or EFORMAT, never a crash and
       never a silent repair of stable history

SCENARIO ST16 truncation sweep
  GIVEN a valid ledger
  WHEN every file is truncated to every prefix length
  THEN the outcome is classified as ECORRUPT/EFORMAT/ENOENT or a legal
       recovery, and stable history is never invented

SCENARIO ST17 crafted headers
  GIVEN manifests and batches with hostile counts, lengths, offsets, and
       checksums
  WHEN open/read/verify run
  THEN parsing rejects them without trusting unchecked values, allocating
       absurd memory, or reading out of bounds

SCENARIO ST18 splices and orphans
  GIVEN files mixed between two ledgers and orphaned generations
  WHEN open runs
  THEN uuid/generation mismatches are ECORRUPT and greater-generation orphans
       are ignored

## Faults

SCENARIO ST19 fault matrix
  GIVEN one injected I/O failure per syscall and call index in a mutation
  WHEN the operation returns and the handle is reopened
  THEN the error class is correct, poisoning is sticky, and recovery yields
       one of the permitted states

SCENARIO ST20 EINTR
  GIVEN EINTR injected into every I/O primitive
  WHEN the operation runs
  THEN the library retries transparently and reports success

SCENARIO ST21 resource exhaustion
  GIVEN ENOSPC/EFBIG/EDQUOT and low RLIMIT_NOFILE/RLIMIT_FSIZE
  WHEN mutations run
  THEN they fail with a storage error, the handle stays recoverable, and no
       partial batch is exposed

## Crashes

SCENARIO ST22 crash matrix
  GIVEN a kill at every syscall index of every structural operation
  WHEN the process dies and the ledger is reopened
  THEN the recovered state is exactly one of the permitted pre/post states

SCENARIO ST23 torn writes
  GIVEN a half-written pwrite followed by a crash
  WHEN recovery runs
  THEN the torn tail is discarded and the previous marker is the frontier

SCENARIO ST24 signals and concurrency
  GIVEN SIGKILL/SIGTERM during operations, concurrent readers, and a second
       writer
  WHEN the processes are joined and the ledger reopened
  THEN the reader only observes stable data, the second writer gets EBUSY,
       and recovery is idempotent

## Differential model

SCENARIO ST25 randomized equivalence
  GIVEN seeded random operation sequences
  WHEN the library is compared with the Python reference model
  THEN boundaries, revision, and every payload match exactly

SCENARIO ST26 crash equivalence
  GIVEN the same sequences with crash points
  WHEN recovery runs
  THEN the recovered state equals one of the model's permitted crash states

SCENARIO ST27 shrinking
  GIVEN a failing random sequence
  WHEN the runner shrinks it
  THEN it emits a minimal replayable script

## Adversarial and host safety

SCENARIO ST28 hostile paths
  GIVEN empty, dot, dotdot, trailing-slash, overlong, symlink, FIFO, and
       file-as-directory paths
  WHEN open runs
  THEN it fails with a documented error within the time budget

SCENARIO ST29 sparse and oversized files
  GIVEN a sparse part file with a huge apparent size
  WHEN open runs
  THEN it rejects the ledger within the time budget instead of scanning the
       whole file

SCENARIO ST30 lock tampering
  GIVEN a writer whose lock file is unlinked or replaced
  WHEN a second writer opens
  THEN the second writer is rejected (or the failure is reported as a defect)

SCENARIO ST31 bounded host impact
  GIVEN every case
  WHEN it runs
  THEN it stays inside its temp directory, respects RLIMIT_CPU/AS/FSIZE/
       NOFILE, terminates within its timeout, and leaves no process behind

## Latency

SCENARIO ST32 operation latency
  GIVEN appends, syncs, reads, iterations, rotations, and prunes
  WHEN each is timed over repeated samples
  THEN p50/p95/p99 stay under the documented ceilings

SCENARIO ST33 scaling
  GIVEN N and 2N records, markers, and parts
  WHEN append, read, open, and recovery are timed
  THEN the growth ratio stays below the documented bound (no quadratic path)

SCENARIO ST34 adversarial latency
  GIVEN maximum-size records, 4096-slice batches, and hostile files
  WHEN they are processed
  THEN they complete or are rejected within the documented budget

## Gates

SCENARIO ST35 ratio gate
  GIVEN the repository
  WHEN `just ratio` runs
  THEN it reports source and test NLOC and fails below 10:1

SCENARIO ST36 coverage gate
  GIVEN the instrumented build
  WHEN `just coverage` runs
  THEN it fails below 95% lines or 90% branches in src/
