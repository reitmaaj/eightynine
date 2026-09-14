"""Latency and scaling (ST32-ST34).

Ceilings are deliberately generous so the suite is not flaky on a busy
host; the JSON report records the measured medians for trend tracking.
"""

import json
import os
import statistics
import time

import ledger
from harness import ROOT, case, expect, expect_rc
from ledger import OK

import tables_latency as T

REPORT = os.path.join(ROOT, "build", "strict", "latency.json")


def payload(size):
    if size <= 4096:
        return bytes((i * 13 + 5) & 0xFF for i in range(size))
    return bytes([size & 0xFF]) * size


def timed(fn, samples=5):
    values = []
    for _ in range(samples):
        started = time.perf_counter()
        fn()
        values.append((time.perf_counter() - started) * 1000.0)
    return values


def build_records(handle, size, count, batch=32):
    data = payload(size)
    remaining = count
    while remaining > 0:
        take = min(batch, remaining)
        rc, _first = ledger.appendv(handle, [data] * take)
        expect_rc(rc, OK, "build appendv")
        remaining -= take
    rc, _stable = ledger.sync(handle)
    expect_rc(rc, OK, "build sync")


def measure_op(ctx, op, size, count):
    data = payload(size)
    if op == "append":
        handle = ctx.fresh_handle()
        values = timed(lambda: ledger.append(handle, data))
        ledger.close(handle)
    elif op == "appendv":
        handle = ctx.fresh_handle()
        slices = [data] * count
        values = timed(lambda: ledger.appendv(handle, slices))
        ledger.close(handle)
    elif op == "sync_dirty":
        handle = ctx.fresh_handle()
        ledger.appendv(handle, [data] * count)
        values = timed(lambda: ledger.sync(handle))
        ledger.close(handle)
    elif op == "sync":
        handle = ctx.fresh_handle()
        values = timed(lambda: ledger.sync(handle))
        ledger.close(handle)
    elif op == "read":
        handle = ctx.fresh_handle()
        build_records(handle, size, count)
        values = timed(lambda: ledger.read(handle, 1, max(size, 1)))
        ledger.close(handle)
    elif op == "read_meta":
        handle = ctx.fresh_handle()
        build_records(handle, size, count)
        values = timed(lambda: ledger.read(handle, 1, 0, null_buffer=True))
        ledger.close(handle)
    elif op == "iterate":
        handle = ctx.fresh_handle()
        build_records(handle, size, count)

        def walk():
            rc, it = ledger.iter_init(handle, 1)
            expect_rc(rc, OK, "iter_init")
            seen = 0
            while True:
                rc, _index, _size, _data = ledger.iter_next(it, 0,
                                                            null_buffer=True)
                if rc == ledger.DONE:
                    break
                expect_rc(rc, OK, "iter_next")
                seen += 1
            expect(seen == count, "iterate saw %d of %d" % (seen, count))

        values = timed(walk)
        ledger.close(handle)
    elif op == "rotate":
        handle = ctx.fresh_handle()
        ledger.appendv(handle, [data] * count)
        ledger.sync(handle)
        values = timed(lambda: ledger.rotate(handle))
        ledger.close(handle)
    elif op == "prune":
        handle = ctx.fresh_handle()
        ledger.appendv(handle, [data] * count)
        ledger.sync(handle)
        ledger.rotate(handle)
        ledger.append(handle, data)
        ledger.sync(handle)
        values = timed(lambda: ledger.prune_before(handle, count))
        ledger.close(handle)
    elif op == "verify":
        handle = ctx.fresh_handle()
        build_records(handle, size, count)
        values = timed(lambda: ledger.verify(handle))
        ledger.close(handle)
    elif op == "open":
        handle = ctx.fresh_handle()
        build_records(handle, size, count)
        ledger.close(handle)

        def reopen():
            rc, fresh = ledger.open_ledger(ctx.ledger_path,
                                           ledger.OPEN_RDWR | ledger.OPEN_CREATE)
            expect_rc(rc, OK, "reopen")
            ledger.close(fresh)

        values = timed(reopen)
    else:
        raise AssertionError("unknown op %s" % op)
    return values


def register_latency_rows():
    for fields in T.LATENCY_ROWS:
        name, op, size, count, ceiling = fields

        def run(ctx, name=name, op=op, size=size, count=count, ceiling=ceiling):
            values = measure_op(ctx, op, size, count)
            median = statistics.median(values)
            record(name, values)
            expect(median <= ceiling,
                   "%s median=%.3fms ceiling=%.1fms samples=%s"
                   % (name, median, ceiling, values))

        run.__name__ = name
        case(name, tier="fast")(run)


def measure_scale(ctx, op, count):
    import shutil

    if os.path.exists(ctx.ledger_path):
        shutil.rmtree(ctx.ledger_path)
    data = payload(64)
    if op == "append":
        handle = ctx.fresh_handle()
        started = time.perf_counter()
        for _ in range(count):
            ledger.append(handle, data)
        elapsed = time.perf_counter() - started
        ledger.close(handle)
        return elapsed
    if op == "read":
        handle = ctx.fresh_handle()
        build_records(handle, 64, count)
        started = time.perf_counter()
        for index in range(1, count + 1):
            ledger.read(handle, index, 64)
        elapsed = time.perf_counter() - started
        ledger.close(handle)
        return elapsed
    if op == "open":
        handle = ctx.fresh_handle()
        build_records(handle, 64, count)
        ledger.close(handle)
        started = time.perf_counter()
        rc, fresh = ledger.open_ledger(ctx.ledger_path,
                                       ledger.OPEN_RDWR | ledger.OPEN_CREATE)
        elapsed = time.perf_counter() - started
        expect_rc(rc, OK, "scale reopen")
        ledger.close(fresh)
        return elapsed
    if op == "iterate":
        handle = ctx.fresh_handle()
        build_records(handle, 64, count)
        started = time.perf_counter()
        rc, it = ledger.iter_init(handle, 1)
        expect_rc(rc, OK, "scale iter_init")
        while True:
            rc, _index, _size, _data = ledger.iter_next(it, 0,
                                                        null_buffer=True)
            if rc == ledger.DONE:
                break
            expect_rc(rc, OK, "scale iter_next")
        elapsed = time.perf_counter() - started
        ledger.close(handle)
        return elapsed
    if op == "sync_many":
        handle = ctx.fresh_handle()
        started = time.perf_counter()
        for _ in range(count):
            ledger.append(handle, data)
            ledger.sync(handle)
        elapsed = time.perf_counter() - started
        ledger.close(handle)
        return elapsed
    raise AssertionError("unknown scaling op %s" % op)


def register_scaling_rows():
    for fields in T.SCALING_ROWS:
        name, op, small, large, factor = fields

        def run(ctx, name=name, op=op, small=small, large=large, factor=factor):
            first = measure_scale(ctx, op, small)
            second = measure_scale(ctx, op, large)
            record(name + ".small", [first * 1000.0])
            record(name + ".large", [second * 1000.0])
            ratio = second / first if first > 0 else 0.0
            expect(ratio <= factor,
                   "%s scaling ratio=%.2f factor=%.1f (%.3fms vs %.3fms)"
                   % (name, ratio, factor, first * 1000.0, second * 1000.0))

        run.__name__ = name
        case(name, tier="full")(run)


_RECORDS = {}


def record(name, values):
    _RECORDS[name] = {
        "samples_ms": [round(value, 4) for value in values],
        "median_ms": round(statistics.median(values), 4),
        "max_ms": round(max(values), 4),
    }


def flush_report():
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    with open(REPORT, "w") as handle:
        json.dump(_RECORDS, handle, indent=2, sort_keys=True)


@case("latency.report", tags=("latency",))
def report(ctx):
    flush_report()


register_latency_rows()
register_scaling_rows()
