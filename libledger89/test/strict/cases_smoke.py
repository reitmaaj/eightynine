"""ST01/ST09 smoke: the end-to-end path through ctypes.

This module must pass before any other strict module is trusted; it proves
the shared object loads, the ABI matches, and open/append/sync/reopen/read/
verify round-trips payloads byte for byte.
"""

import ledger
from harness import (
    case,
    expect,
    expect_payload,
    expect_rc,
    expect_state,
)
from ledger import (
    EINVAL,
    OK,
    append,
    close,
    open_ledger,
    read,
    sync,
    u64,
    u64_cmp,
    u64_equal,
    u64_from_u32,
    u64_zero,
    verify,
)


@case("smoke.end_to_end", tags=("smoke",))
def smoke_end_to_end(ctx):
    handle = ctx.fresh_handle()
    expect_state(handle, first=1, stable=1, end=1, rev=0)

    payloads = [b"alpha", b"", b"gamma-payload"]
    rc, first = ledger.appendv(handle, payloads)
    expect_rc(rc, OK, "appendv")
    expect(first == 1, "first_out=%r" % first)
    expect_state(handle, first=1, stable=1, end=4, rev=0)

    rc, stable = sync(handle)
    expect_rc(rc, OK, "sync")
    expect(stable == 4, "stable_end_out=%r" % stable)
    expect_state(handle, first=1, stable=4, end=4, rev=0)

    rc, state = ledger.get_state(handle)
    expect_rc(rc, OK, "get_state")
    identity = bytes(state.id.bytes)

    close(handle)
    handle = ctx.reopen()

    rc, state = ledger.get_state(handle)
    expect_rc(rc, OK, "reopen get_state")
    expect(bytes(state.id.bytes) == identity, "identity changed across reopen")
    expect_state(handle, first=1, stable=4, end=4, rev=0)
    for index, payload in enumerate(payloads, start=1):
        expect_payload(handle, index, payload)
    expect_rc(verify(handle), OK, "verify")
    close(handle)


@case("smoke.abi_scalars", tags=("smoke",))
def smoke_abi_scalars(ctx):
    expect(u64_zero() == 0, "u64_zero")
    for value in (0, 1, 0x7FFFFFFF, 0xFFFFFFFF):
        expect(u64_from_u32(value) == value, "u64_from_u32(%d)" % value)
    expect(u64_cmp(1, 2) < 0, "cmp 1,2")
    expect(u64_cmp(2, 1) > 0, "cmp 2,1")
    expect(u64_cmp(7, 7) == 0, "cmp 7,7")
    expect(u64_cmp(0x100000000, 0xFFFFFFFF) > 0, "cmp high word")
    expect(u64_equal(0x100000000, 0x100000000) == 1, "equal high word")
    expect(u64_equal(0x100000000, 0xFFFFFFFF) == 0, "not equal high word")


@case("smoke.convenience_append", tags=("smoke",))
def smoke_convenience_append(ctx):
    handle = ctx.fresh_handle()
    rc, index = append(handle, b"one")
    expect_rc(rc, OK, "append")
    expect(index == 1, "append index=%r" % index)
    rc, index = append(handle, b"two")
    expect_rc(rc, OK, "append")
    expect(index == 2, "append index=%r" % index)
    expect_payload(handle, 1, b"one")
    expect_payload(handle, 2, b"two")
    rc, size, data = read(handle, u64(1), 0)
    expect_rc(rc, ledger.ETOOSMALL, "short read")
    expect(size == 3, "required size=%r" % size)
    expect(data is None, "short read copied bytes")
    rc, size, data = read(handle, u64(9), 8)
    expect_rc(rc, ledger.ENOENT, "read past end")
    close(handle)
