"""Randomized differential model (ST25-ST27).

A Python reference model tracks first/stable_end/end/revision, sealed part
boundaries, and every payload. Seeded random operation sequences are
compared against the library after every step. On a mismatch the runner
records a trace and shrinks the sequence to the minimal failing step count.
"""

import json
import os
import random

import ledger
from harness import ROOT, case, expect, expect_rc, scan_records
from ledger import (
    DONE,
    EGONE,
    ENOENT,
    ERANGE,
    ESTALE,
    EUNSTABLE,
    OK,
    appendv,
    appendv_at,
    close,
    get_state,
    iter_init,
    iter_next,
    open_ledger,
    prune_before,
    read,
    rotate,
    sync,
    truncate_from,
    verify,
)


class Model:
    def __init__(self):
        self.records = {}
        self.first = 1
        self.stable_end = 1
        self.end = 1
        self.revision = 0
        self.sealed = []
        self.active_first = 1

    def append(self, payloads):
        first = self.end
        for payload in payloads:
            self.records[self.end] = payload
            self.end += 1
        return first

    def sync(self):
        self.stable_end = self.end

    def truncate(self, position):
        if self.end != self.stable_end:
            return EUNSTABLE
        if position < self.first:
            return EGONE
        if position > self.end:
            return ERANGE
        if position == self.end:
            return OK
        for index in range(position, self.end):
            self.records.pop(index, None)
        self.end = position
        self.stable_end = position
        self.revision += 1
        kept = [(start, end) for start, end in self.sealed if end <= position]
        boundary = None
        for start, end in self.sealed:
            if start < position and end > position:
                boundary = start
                break
        if boundary is None and self.active_first < position:
            boundary = self.active_first
        if boundary is not None:
            kept.append((boundary, position))
        self.sealed = kept
        self.active_first = position
        return OK

    def prune(self, requested):
        if requested > self.stable_end:
            return EUNSTABLE, None
        actual = self.first
        kept = []
        for start, end in self.sealed:
            if end <= requested:
                actual = end
            else:
                kept.append((start, end))
        self.sealed = kept
        if actual > self.first:
            for index in range(self.first, actual):
                self.records.pop(index, None)
            self.first = actual
        return OK, actual

    def rotate(self):
        self.sync()
        if self.end > self.active_first:
            self.sealed.append((self.active_first, self.end))
        self.active_first = self.end

    def reopen(self):
        self.end = self.stable_end
        for index in list(self.records):
            if index >= self.end:
                del self.records[index]
        if self.active_first > self.end:
            self.active_first = self.end


def check_state(model, handle):
    rc, state = get_state(handle)
    expect_rc(rc, OK, "get_state")
    expect(state.first.value() == model.first,
           "first=%d model=%d" % (state.first.value(), model.first))
    expect(state.stable_end.value() == model.stable_end,
           "stable=%d model=%d" % (state.stable_end.value(), model.stable_end))
    expect(state.end.value() == model.end,
           "end=%d model=%d" % (state.end.value(), model.end))
    expect(state.revision.value() == model.revision,
           "revision=%d model=%d" % (state.revision.value(), model.revision))


def check_records(model, handle, indices):
    for index in indices:
        if index < model.first:
            rc, _size, _data = read(handle, index, 64)
            expect_rc(rc, EGONE, "read pruned %d" % index)
        elif index >= model.end:
            rc, _size, _data = read(handle, index, 64)
            expect_rc(rc, ENOENT, "read past end %d" % index)
        else:
            expected = model.records[index]
            rc, size, data = read(handle, index, max(len(expected), 1))
            expect_rc(rc, OK, "read %d" % index)
            expect(data == expected,
                   "record %d = %r model %r" % (index, data, expected))


def check_scan(model, handle):
    if model.first >= model.end:
        return
    records = scan_records(handle, model.first)
    expected = [(index, model.records[index])
                for index in range(model.first, model.end)]
    expect(records == expected,
           "scan mismatch: %d vs %d records" % (len(records), len(expected)))


def run_step(ctx, rng, handle, model, op, step):
    if op < 30:
        count = rng.randrange(1, 5)
        payloads = [bytes(rng.randrange(256)
                          for _ in range(rng.randrange(0, 64)))
                    for _ in range(count)]
        rc, first = appendv(handle, payloads)
        expect_rc(rc, OK, "appendv step %d" % step)
        expect(first == model.append(payloads), "appendv first step %d" % step)
    elif op < 34:
        count = rng.randrange(1, 3)
        payloads = [b"x" * rng.randrange(0, 8) for _ in range(count)]
        use_revision = rng.choice([model.revision, model.revision + 1])
        use_end = rng.choice([model.end, model.end + 1])
        rc, first = appendv_at(handle, use_revision, use_end, payloads)
        if use_revision == model.revision and use_end == model.end:
            expect_rc(rc, OK, "appendv_at match step %d" % step)
            expect(first == model.append(payloads), "appendv_at first")
        else:
            expect_rc(rc, ESTALE, "appendv_at stale step %d" % step)
    elif op < 45:
        rc, stable = sync(handle)
        expect_rc(rc, OK, "sync step %d" % step)
        model.sync()
        expect(stable == model.stable_end, "sync stable")
    elif op < 55:
        position = rng.randrange(max(model.first - 1, 0), model.end + 2)
        rc = truncate_from(handle, position)
        expected = model.truncate(position)
        expect_rc(rc, expected, "truncate step %d" % step)
    elif op < 63:
        requested = rng.randrange(0, model.stable_end + 2)
        rc, actual = prune_before(handle, requested)
        expected, expected_actual = model.prune(requested)
        expect_rc(rc, expected, "prune step %d" % step)
        if expected == OK:
            expect(actual == expected_actual,
                   "prune requested=%d actual=%r model=%r"
                   % (requested, actual, expected_actual))
    elif op < 70:
        rc = rotate(handle)
        expect_rc(rc, OK, "rotate step %d" % step)
        model.rotate()
    elif op < 80:
        close(handle)
        model.reopen()
        rc, handle = open_ledger(ctx.ledger_path,
                                 ledger.OPEN_RDWR | ledger.OPEN_CREATE)
        expect_rc(rc, OK, "reopen step %d" % step)
    elif op < 90:
        if model.first < model.end:
            index = rng.randrange(model.first, model.end + 1)
            check_records(model, handle, [index])
    elif op < 95:
        rc = verify(handle)
        expect_rc(rc, OK, "verify step %d" % step)
    else:
        start = rng.randrange(max(model.first, 1), model.end + 1)
        rc, it = iter_init(handle, start)
        expect_rc(rc, OK, "iter_init step %d" % step)
        while True:
            rc, index, size, data = iter_next(it, 64)
            if rc == DONE:
                break
            expect_rc(rc, OK, "iter_next step %d" % step)
            expect(data == model.records[index],
                   "iter record %d step %d" % (index, step))
    return handle


def replay(ctx, seed, steps):
    """Run a sequence; return (error_or_None, trace)."""
    import shutil

    rng = random.Random(seed)
    model = Model()
    handle = None
    if os.path.exists(ctx.ledger_path):
        shutil.rmtree(ctx.ledger_path)
    rc, handle = open_ledger(ctx.ledger_path,
                             ledger.OPEN_RDWR | ledger.OPEN_CREATE |
                             ledger.OPEN_EXCL)
    if rc != OK:
        return "open rc=%d" % rc, []
    trace = []
    try:
        for step in range(steps):
            op = rng.randrange(0, 100)
            trace.append(
                "step %d op %d model first=%d stable=%d end=%d rev=%d"
                % (step, op, model.first, model.stable_end, model.end,
                   model.revision))
            handle = run_step(ctx, rng, handle, model, op, step)
            check_state(model, handle)
            if step % 25 == 0:
                check_scan(model, handle)
                check_records(model, handle,
                              [model.first, model.stable_end, model.end,
                               model.first - 1, model.end])
        check_scan(model, handle)
    except AssertionError as error:
        return str(error), trace
    finally:
        close(handle)
    return None, trace


def shrink(ctx, seed, steps):
    """Find the minimal failing step count by binary search."""
    error, _trace = replay(ctx, seed, steps)
    if error is None:
        return steps, None
    low = 1
    high = steps
    while low < high:
        middle = (low + high) // 2
        probe, _trace = replay(ctx, seed, middle)
        if probe is None:
            low = middle + 1
        else:
            high = middle
    error, trace = replay(ctx, seed, low)
    return low, (error, trace)


def register_model_cases():
    configs = [("fast", 3, 200), ("full", 12, 2000), ("long", 48, 20000)]
    for tier, seeds, steps in configs:
        for seed in range(seeds):
            name = "model.diff.%s.%d" % (tier, seed)

            def run(ctx, seed=seed, steps=steps):
                error, trace = replay(ctx, seed + 1, steps)
                if error is not None:
                    minimal, shrunk = shrink(ctx, seed + 1, steps)
                    report = os.path.join(ROOT, "build", "strict",
                                          "model-failure-%d.json" % (seed + 1))
                    os.makedirs(os.path.dirname(report), exist_ok=True)
                    with open(report, "w") as handle:
                        json.dump({"seed": seed + 1, "steps": steps,
                                   "minimal_steps": minimal,
                                   "error": shrunk[0],
                                   "trace": shrunk[1]}, handle, indent=2)
                    raise AssertionError(
                        "%s\nminimal steps=%d\ntrace:\n%s\nreport: %s"
                        % (error, minimal, "\n".join(trace[-30:]), report))

            run.__name__ = name
            case(name, tier=tier)(run)


def synthetic_runner(fail_from):
    def run(seed, steps):
        if steps >= fail_from:
            return "synthetic failure at %d" % steps, []
        return None, []

    return run


def shrink_generic(runner, seed, steps):
    error, _trace = runner(seed, steps)
    if error is None:
        return steps
    low = 1
    high = steps
    while low < high:
        middle = (low + high) // 2
        probe, _trace = runner(seed, middle)
        if probe is None:
            low = middle + 1
        else:
            high = middle
    return low


@case("model.shrinker", tags=("model",))
def shrinker(ctx):
    runner = synthetic_runner(7)
    expect(shrink_generic(runner, 1, 1000) == 7, "shrinker did not minimize")
    expect(shrink_generic(synthetic_runner(1), 1, 1000) == 1, "minimum 1")
    expect(shrink_generic(synthetic_runner(1000), 1, 1000) == 1000,
           "maximum bound")


register_model_cases()
