"""API boundary tables (ST04-ST09).

Every row of tables_api.py becomes one case. Rows are grouped by tier: fast
rows are cheap and always run; large-payload rows run in the long tier.
"""

import os

import ledger
from harness import case, expect, expect_payload, expect_rc, expect_state
from ledger import (
    DONE,
    EGONE,
    EINVAL,
    ENOENT,
    ERANGE,
    EROFS,
    ESTALE,
    ETOOSMALL,
    EUNSTABLE,
    OK,
)

import tables_api as T

EXPECT = {name: code for code, name in ledger.RESULT_NAMES.items()}

FIXTURES = {
    "abc": (b"", b"a", b"hello", b"0123456789"),
    "mixed": (b"x", b"", b"yz", b"", b"tail"),
}


def pattern(size):
    if size == 0:
        return b""
    if size <= 4096:
        return bytes((i * 31 + 7) & 0xFF for i in range(size))
    return bytes([size & 0xFF]) * size


def register(name, tier, params, fn):
    def run(ctx, params=params, fn=fn):
        fn(ctx, *params)

    run.__name__ = name
    case(name, tier=tier)(run)


def setup_fixture(ctx, fixture):
    handle = ctx.fresh_handle()
    rc, _first = ledger.appendv(handle, FIXTURES[fixture])
    expect_rc(rc, OK, "fixture appendv")
    rc, _stable = ledger.sync(handle)
    expect_rc(rc, OK, "fixture sync")
    return handle


def setup_truncate(ctx, setup):
    handle = ctx.fresh_handle()
    if setup == "clean3":
        ledger.appendv(handle, (b"a", b"b", b"c"))
        ledger.sync(handle)
    elif setup == "clean1":
        ledger.appendv(handle, (b"a",))
        ledger.sync(handle)
    else:
        ledger.appendv(handle, (b"a", b"b", b"c"))
        ledger.sync(handle)
        expect_rc(ledger.rotate(handle), OK, "rotate")
        ledger.appendv(handle, (b"d",))
        ledger.sync(handle)
        rc, actual = ledger.prune_before(handle, 4)
        expect_rc(rc, OK, "prune")
        expect(actual == 4, "pruned actual=%r" % actual)
    return handle


def setup_prune(ctx, setup):
    handle = ctx.fresh_handle()
    if setup == "rotated3":
        ledger.appendv(handle, (b"a", b"b", b"c"))
        ledger.sync(handle)
        expect_rc(ledger.rotate(handle), OK, "rotate")
        ledger.appendv(handle, (b"d", b"e", b"f"))
        ledger.sync(handle)
    elif setup == "dirty":
        ledger.appendv(handle, (b"a", b"b", b"c"))
        ledger.sync(handle)
        ledger.appendv(handle, (b"d",))
    else:
        ledger.appendv(handle, (b"a",))
        ledger.sync(handle)
    return handle


def check_open(ctx, flags, scenario, expected):
    path = ctx.ledger_path
    if scenario == "existing":
        handle = ctx.fresh_handle()
        ledger.close(handle)
    elif scenario == "missing":
        path = os.path.join(ctx.subdir("parent"), "ledger")
    rc, handle = ledger.open_ledger(path, flags)
    expect_rc(rc, EXPECT[expected], "open")
    if rc == OK:
        expect_state(handle, first=1, stable=1, end=1, rev=0)
        ledger.close(handle)
    else:
        expect(handle is None, "failure left a handle")


def check_append(ctx, sizes, count, expected, null_array, null_data, first,
                 end):
    handle = ctx.fresh_handle()
    slices = []
    for index, size in enumerate(sizes):
        if null_data == index or size > 32 * 1024 * 1024:
            slices.append(size)
        else:
            slices.append(pattern(size))
    rc, first_out = ledger.appendv_raw(handle, slices, count, null_array)
    expect_rc(rc, EXPECT[expected], "appendv")
    if expected == "OK":
        expect(first_out == first, "first_out=%r expected %r" % (first_out, first))
        expect_state(handle, first=1, stable=1, end=end, rev=0)
        for index, size in enumerate(sizes):
            if size <= 32 * 1024 * 1024:
                expect_payload(handle, 1 + index, pattern(size))
    else:
        expect_state(handle, first=1, stable=1, end=1, rev=0)


def check_read(ctx, fixture, index, capacity, null_buffer, expected, size):
    handle = setup_fixture(ctx, fixture)
    rc, got_size, data = ledger.read(handle, index, capacity, null_buffer)
    expect_rc(rc, EXPECT[expected], "read")
    if size is not None:
        expect(got_size == size, "size=%r expected %r" % (got_size, size))
    if rc == OK and not null_buffer:
        expect(data == FIXTURES[fixture][index - 1], "payload mismatch")
    if rc == ETOOSMALL:
        rc2, size2, raw = ledger.read_into(handle, index, capacity)
        expect(rc2 == ETOOSMALL, "read_into rc=%r" % rc2)
        expect(all(byte == 0x5A for byte in raw),
               "short read modified the buffer")


def check_iter(ctx, fixture, start, capacity, null_buffer, expected):
    handle = setup_fixture(ctx, fixture)
    rc, it = ledger.iter_init(handle, start)
    expect_rc(rc, EXPECT[expected], "iter_init")
    if rc != OK:
        return
    end = 1 + len(FIXTURES[fixture])
    if start == end:
        rc, index, size, data = ledger.iter_next(it, capacity, null_buffer)
        expect_rc(rc, DONE, "iter_next at end")
        return
    payload = FIXTURES[fixture][start - 1]
    rc, index, size, data = ledger.iter_next(it, capacity, null_buffer)
    if null_buffer:
        expect_rc(rc, OK, "iter_next size-only")
        expect(index == start and size == len(payload), "size-only metadata")
        return
    if capacity < len(payload):
        expect_rc(rc, ETOOSMALL, "iter_next short buffer")
        expect(size == len(payload), "required size")
        rc, index, size, data = ledger.iter_next(it, len(payload), False)
        expect_rc(rc, OK, "iter_next retry")
        expect(index == start, "iterator advanced on ETOOSMALL")
    else:
        expect_rc(rc, OK, "iter_next")
    expect(data == payload, "iterator payload")


def check_truncate(ctx, setup, position, expected):
    handle = setup_truncate(ctx, setup)
    if setup == "clean3":
        first, end, rev = 1, 4, 0
    elif setup == "clean1":
        first, end, rev = 1, 2, 0
    else:
        first, end, rev = 4, 5, 0
    rc = ledger.truncate_from(handle, position)
    expect_rc(rc, EXPECT[expected], "truncate")
    if expected == "OK":
        if position == end:
            expect_state(handle, first=first, stable=end, end=end, rev=rev)
        else:
            expect_state(handle, first=first, stable=position, end=position,
                         rev=rev + 1)
            rc2, size2, data2 = ledger.read(handle, position, 8)
            expect_rc(rc2, ENOENT, "removed index still readable")
    else:
        expect_state(handle, first=first, stable=end, end=end, rev=rev)


def check_prune(ctx, setup, request, expected):
    handle = setup_prune(ctx, setup)
    if setup == "rotated3":
        stable, end, low, high = 7, 7, 1, 4
    elif setup == "dirty":
        stable, end, low, high = 4, 5, 1, 1
    else:
        stable, end, low, high = 2, 2, 1, 1
    rc, actual = ledger.prune_before(handle, request)
    expect_rc(rc, EXPECT[expected], "prune")
    if expected == "OK":
        expect(low <= actual <= high,
               "actual_first=%r outside [%d,%d]" % (actual, low, high))
        expect_state(handle, first=actual, stable=stable, end=end, rev=0)
        if actual > 1:
            rc2, size2, data2 = ledger.read(handle, 1, 8)
            expect_rc(rc2, EGONE, "pruned index readable")
    else:
        expect(actual is None, "EUNSTABLE returned an actual")


def check_strerror(ctx, value):
    text = ledger.strerror(value)
    expect(text is not None, "strerror(%d) is NULL" % value)
    expect(len(text) > 0, "strerror(%d) empty" % value)


def check_u64(ctx, a, b, sign):
    got = ledger.u64_cmp(a, b)
    if sign < 0:
        expect(got < 0, "cmp(%d,%d)=%d" % (a, b, got))
    elif sign > 0:
        expect(got > 0, "cmp(%d,%d)=%d" % (a, b, got))
    else:
        expect(got == 0, "cmp(%d,%d)=%d" % (a, b, got))
    equal = ledger.u64_equal(a, b)
    expect(equal == (1 if sign == 0 else 0), "equal(%d,%d)=%d" % (a, b, equal))


def register_api_rows():
    for fields in T.OPEN_ROWS:
        name, flags, scenario, expected = fields
        register(name, "fast", (flags, scenario, expected), check_open)
    for fields in T.APPEND_ROWS:
        name, sizes, count, expected, null_array, null_data, first, end = fields
        tier = "long" if any(size > 1024 * 1024 for size in sizes) else "fast"
        register(name, tier,
                 (sizes, count, expected, null_array, null_data, first, end),
                 check_append)
    for fields in T.READ_ROWS:
        name, fixture, index, capacity, null_buffer, expected, size = fields
        if index <= 5 and capacity in (0, 1, 10, 11, 64):
            tier = "fast"
        elif index <= 5 and capacity <= 16:
            tier = "full"
        else:
            tier = "long"
        register(name, tier,
                 (fixture, index, capacity, null_buffer, expected, size),
                 check_read)
    for fields in T.ITER_ROWS:
        name, fixture, start, capacity, null_buffer, expected = fields
        if start <= 6 and capacity in (0, 1, 4, 32):
            tier = "fast"
        elif start <= 6 and capacity <= 8:
            tier = "full"
        else:
            tier = "long"
        register(name, tier,
                 (fixture, start, capacity, null_buffer, expected), check_iter)
    for fields in T.TRUNCATE_ROWS:
        name, setup, position, expected = fields
        register(name, "fast", (setup, position, expected), check_truncate)
    for fields in T.PRUNE_ROWS:
        name, setup, request, expected = fields
        register(name, "fast", (setup, request, expected), check_prune)
    for fields in T.STRERROR_ROWS:
        name, value = fields
        register(name, "fast", (value,), check_strerror)
    for fields in T.U64_ROWS:
        name, a, b, sign = fields
        register(name, "fast", (a, b, sign), check_u64)


@case("api.strerror_distinct", tags=("api",))
def strerror_distinct(ctx):
    seen = {}
    for code, name in ledger.RESULT_NAMES.items():
        text = ledger.strerror(code)
        expect(text not in seen, "%s collides with %s" % (name, seen.get(text)))
        seen[text] = name


@case("api.u64_zero", tags=("api",))
def u64_zero(ctx):
    expect(ledger.u64_zero() == 0, "zero")
    for value in (0, 1, 0xFFFFFFFF):
        expect(ledger.u64_from_u32(value) == value, "from_u32(%d)" % value)


@case("api.raw_edge_arguments", tags=("api",))
def raw_edge_arguments(ctx):
    import ctypes

    lib = ledger._lib
    out = ctypes.c_void_p(0)
    state = ledger.State()
    size = ctypes.c_size_t(0)
    it = ledger.Iter()
    actual = ledger.U64()
    expect_rc(lib.ledger89_open(None, b"x", 14), ledger.EINVAL,
              "open NULL out")
    expect_rc(lib.ledger89_open(ctypes.byref(out), None, 14), ledger.EINVAL,
              "open NULL path")
    handle = ctx.fresh_handle()
    expect_rc(lib.ledger89_get_state(handle, None), ledger.EINVAL,
              "get_state NULL out")
    expect_rc(lib.ledger89_read(handle, ledger.u64(1), None, 8, None),
              ledger.EINVAL, "read NULL size_out")
    expect_rc(lib.ledger89_iter_init(None, handle, ledger.u64(1)),
              ledger.EINVAL, "iter_init NULL iter")
    expect_rc(lib.ledger89_iter_init(ctypes.byref(it), None, ledger.u64(1)),
              ledger.EINVAL, "iter_init NULL ledger")
    expect_rc(lib.ledger89_iter_next(None, None, None, 0, ctypes.byref(size)),
              ledger.EINVAL, "iter_next NULL iter")
    expect_rc(lib.ledger89_iter_next(ctypes.byref(it), None, None, 0,
                                     None), ledger.EINVAL,
              "iter_next NULL size_out")
    expect_rc(lib.ledger89_prune_before(handle, ledger.u64(1), None),
              ledger.EINVAL, "prune NULL actual")
    expect_rc(lib.ledger89_appendv(handle, None, 1, ctypes.byref(ledger.U64())),
              ledger.EINVAL, "appendv NULL records")
    rc, first = ledger.appendv(handle, [b"x"])
    expect_rc(rc, OK, "append")
    expect(first == 1, "first")
    expect_rc(lib.ledger89_append(handle, None, 0, ctypes.byref(ledger.U64())),
              OK, "append NULL data size zero")
    ledger.close(handle)


@case("api.iterator_edge_states", tags=("api",))
def iterator_edge_states(ctx):
    handle = ctx.fresh_handle()
    ledger.appendv(handle, [b"a", b"b", b"c"])
    ledger.sync(handle)
    rc, it = ledger.iter_init(handle, 1)
    expect_rc(rc, OK, "iter_init")
    expect_rc(ledger.truncate_from(handle, 2), OK, "truncate")
    rc, index, size, data = ledger.iter_next(it, 8)
    expect_rc(rc, ESTALE, "iter after truncate")
    rc, it = ledger.iter_init(handle, 2)
    expect_rc(rc, OK, "iter_init at end")
    rc, index, size, data = ledger.iter_next(it, 8)
    expect_rc(rc, DONE, "iter at end")
    ledger.appendv(handle, [b"d"])
    rc, index, size, data = ledger.iter_next(it, 8)
    expect_rc(rc, OK, "DONE not permanent")
    expect(data == b"d", "payload after DONE")
    rc, it = ledger.iter_init(handle, 99)
    expect_rc(rc, ERANGE, "iter past end")
    rc, it = ledger.iter_init(handle, 0)
    expect_rc(rc, EGONE, "iter below first")
    ledger.close(handle)


@case("api.append_limit_fast", tags=("api",))
def append_limit_fast(ctx):
    handle = ctx.fresh_handle()
    rc, first = ledger.appendv_raw(handle, [b"\x00" * 16777216], 1)
    expect_rc(rc, OK, "max record accepted")
    rc, first = ledger.appendv_raw(handle, [b"\x00" * 16777217], 1)
    expect_rc(rc, ERANGE, "over max record")
    rc, first = ledger.appendv_raw(handle, [1], 0)
    expect_rc(rc, EINVAL, "zero count")
    expect_state(handle, first=1, stable=1, end=2, rev=0)
    ledger.close(handle)


@case("api.readonly_mutations", tags=("api",))
def readonly_mutations(ctx):
    handle = ctx.fresh_handle()
    ledger.appendv(handle, [b"a"])
    ledger.sync(handle)
    ledger.close(handle)
    rc, ro = ledger.open_ledger(ctx.ledger_path, ledger.OPEN_RDONLY)
    expect_rc(rc, OK, "readonly open")
    expect_rc(ledger.appendv(ro, [b"b"])[0], ledger.EROFS, "appendv")
    expect_rc(ledger.sync(ro)[0], ledger.EROFS, "sync")
    expect_rc(ledger.truncate_from(ro, 1), ledger.EROFS, "truncate")
    expect_rc(ledger.prune_before(ro, 1)[0], ledger.EROFS, "prune")
    expect_rc(ledger.rotate(ro), ledger.EROFS, "rotate")
    expect_rc(ledger.verify(ro), OK, "verify readonly")
    expect_rc(ledger.read(ro, 1, 8)[0], OK, "read readonly")
    ledger.close(ro)


register_api_rows()
