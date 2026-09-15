# BDD scenarios: transport faults and POSIX isolation

## Syscall seam

SCENARIO: retry a read interrupted by EINTR
GIVEN a read that returns EINTR before delivering a byte
WHEN jrpc89_fd_read_frame runs
THEN it retries and returns the complete frame.

SCENARIO: retry a write interrupted by EINTR
GIVEN a write that returns EINTR before accepting any byte
WHEN jrpc89_fd_write_frame runs
THEN it retries and writes the whole frame.

SCENARIO: retry EINTR during an overflow drain
GIVEN a frame that overflows the buffer and a read that returns EINTR while
draining
WHEN jrpc89_fd_read_frame runs
THEN it retries, drains through the newline, and returns JRPC89_ETOOLONG.

SCENARIO: complete a short write
GIVEN a write that accepts only part of the bytes per call
WHEN jrpc89_fd_write_frame runs
THEN it loops until every byte plus the newline is written.

SCENARIO: report a hard read failure
GIVEN a read that returns a hard error
WHEN jrpc89_fd_read_frame runs
THEN it returns JRPC89_EIO and clears the output state.

SCENARIO: report a hard write failure
GIVEN a write that returns a hard error
WHEN jrpc89_fd_write_frame runs
THEN it returns JRPC89_EIO.

## POSIX profile

SCENARIO: framing logic stays free of POSIX headers
GIVEN the framing translation unit
WHEN it is compiled under the strict profile
THEN it includes no POSIX header; read and write live behind the syscall
seam in the POSIX adapter.

SCENARIO: SIGPIPE does not terminate the CLI
GIVEN a write to a pipe whose read end is closed
WHEN the CLI writes its request
THEN the CLI reports a write failure and exits nonzero instead of dying by
SIGPIPE.

SCENARIO: explicit fd ownership
GIVEN the CLI receives an already-open fd
WHEN it finishes
THEN it closes that fd explicitly and the library never closes it.
