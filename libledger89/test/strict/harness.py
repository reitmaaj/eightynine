"""Strict-suite harness: case registry, temp dirs, limits, and assertions.

Every executable case is registered with @case and receives a CaseContext.
The runner selects cases by tier and name filter, enforces a per-case
timeout, and reports a summary with a machine-readable JSON report.

Tiers are cumulative: a "fast" case runs in fast, full, and long; a "full"
case runs in full and long; a "long" case only in long.
"""

import argparse
import ctypes
import json
import os
import shutil
import signal
import subprocess
import sys
import time
import traceback

import ledger
from crclib import crc32c
from ledger import (
    DONE,
    EINVAL,
    EGONE,
    ENOENT,
    ETOOSMALL,
    OK,
    RESULT_NAMES,
    State,
    U64,
    append,
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
    u64,
    verify,
)

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
RUNNER = os.environ.get("LED89_RUNNER", os.path.join(ROOT, "build", "strictrun"))
SHIM = os.environ.get("LED89_SHIM",
                      os.path.join(ROOT, "build", "libledger89_fault.so"))
TMP_ROOT = os.environ.get(
    "LED89_STRICT_TMP", os.path.join(ROOT, "build", "strict-tmp")
)

TIER_ORDER = {"fast": 0, "full": 1, "long": 2}

DEFAULT_LIMITS = {
    "fast": {
        "timeout": 20,
        "as_bytes": 512 * 1024 * 1024,
        "fsize": 64 * 1024 * 1024,
        "cpu": 20,
        "nofile": 256,
        "disk": 16 * 1024 * 1024,
    },
    "full": {
        "timeout": 60,
        "as_bytes": 1024 * 1024 * 1024,
        "fsize": 256 * 1024 * 1024,
        "cpu": 60,
        "nofile": 512,
        "disk": 64 * 1024 * 1024,
    },
    "long": {
        "timeout": 300,
        "as_bytes": 2 * 1024 * 1024 * 1024,
        "fsize": 1024 * 1024 * 1024,
        "cpu": 300,
        "nofile": 1024,
        "disk": 256 * 1024 * 1024,
    },
}


class Case:
    def __init__(self, name, min_tier, tags, fn):
        self.name = name
        self.min_tier = min_tier
        self.tags = tuple(tags)
        self.fn = fn


_REGISTRY = []


def case(name, tier="fast", tags=()):
    if tier not in TIER_ORDER:
        raise ValueError("bad tier: %s" % tier)

    def decorate(fn):
        _REGISTRY.append(Case(name, tier, tags, fn))
        return fn

    return decorate


def registry():
    return list(_REGISTRY)


class CaseTimeout(Exception):
    pass


def _alarm(signum, frame):
    raise CaseTimeout("case exceeded its time budget")


def _limit_fn(limits):
    import resource

    def apply():
        signal.signal(signal.SIGXFSZ, signal.SIG_IGN)
        resource.setrlimit(resource.RLIMIT_AS, (limits["as_bytes"], limits["as_bytes"]))
        resource.setrlimit(resource.RLIMIT_FSIZE, (limits["fsize"], limits["fsize"]))
        resource.setrlimit(resource.RLIMIT_CPU, (limits["cpu"], limits["cpu"]))
        resource.setrlimit(resource.RLIMIT_NOFILE, (limits["nofile"], limits["nofile"]))

    return apply


def run_script(root, script, env=None, timeout=20, preload=False, check=True,
               limits=None):
    """Run strictrun against an explicit root; return (proc, transcript)."""
    if not os.path.exists(RUNNER):
        raise AssertionError("strictrun missing; run `just strictrun`")
    merged = dict(os.environ)
    merged["LED89_STRICT_ROOT"] = root
    if env:
        merged.update(env)
    if preload:
        if not os.path.exists(SHIM):
            raise AssertionError("fault shim missing; run `just strictrun`")
        preloads = [SHIM]
        if merged.get("LD_PRELOAD"):
            preloads.append(merged["LD_PRELOAD"])
        merged["LD_PRELOAD"] = ":".join(preloads)
    effective = dict(DEFAULT_LIMITS["fast"])
    if limits:
        effective.update(limits)
    proc = subprocess.run(
        [RUNNER],
        input=script.encode(),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=merged,
        timeout=timeout,
        preexec_fn=_limit_fn(effective) if limits else None,
    )
    transcript = proc.stdout.decode("utf-8", "replace")
    if check and proc.returncode != 0:
        raise AssertionError(
            "strictrun exit %d\nscript:\n%s\nstdout:\n%s\nstderr:\n%s"
            % (proc.returncode, script, transcript,
               proc.stderr.decode("utf-8", "replace"))
        )
    return proc, transcript


class CaseContext:
    def __init__(self, name, tier, root):
        self.name = name
        self.tier = tier
        self.limits = DEFAULT_LIMITS[tier]
        self.dir = os.path.join(root, name)
        if os.path.exists(self.dir):
            shutil.rmtree(self.dir)
        os.makedirs(self.dir)
        self._n = 0
        self.subprocesses = []
        self.warnings = []

    @property
    def ledger_path(self):
        return os.path.join(self.dir, "ledger")

    def subdir(self, name):
        path = os.path.join(self.dir, name)
        os.makedirs(path, exist_ok=True)
        return path

    def files(self, path=None):
        target = path or self.ledger_path
        if not os.path.isdir(target):
            return []
        return sorted(os.listdir(target))

    def read_file(self, name, path=None):
        target = path or self.ledger_path
        with open(os.path.join(target, name), "rb") as handle:
            return handle.read()

    def write_file(self, name, data, path=None):
        target = path or self.ledger_path
        with open(os.path.join(target, name), "wb") as handle:
            handle.write(data)

    def total_bytes(self):
        total = 0
        for base, _dirs, files in os.walk(self.dir):
            for name in files:
                total += os.path.getsize(os.path.join(base, name))
        return total

    def fresh_handle(self, flags=None, path=None):
        if flags is None:
            flags = ledger.OPEN_RDWR | ledger.OPEN_CREATE | ledger.OPEN_EXCL
        rc, handle = open_ledger(path or self.ledger_path, flags)
        expect_rc(rc, OK, "open")
        return handle

    def reopen(self, flags=None, path=None):
        if flags is None:
            flags = ledger.OPEN_RDWR | ledger.OPEN_CREATE
        rc, handle = open_ledger(path or self.ledger_path, flags)
        expect_rc(rc, OK, "reopen")
        return handle

    def run_runner(self, script, env=None, timeout=None, preload=False,
                   check=True, limits=True):
        """Run build/strictrun with a script; return (proc, transcript)."""
        if timeout is None:
            timeout = self.limits["timeout"]
        return run_script(
            self.dir, script, env=env, timeout=timeout, preload=preload,
            check=check, limits=self.limits if limits else None,
        )

    def spawn_runner(self, script, env=None, preload=False):
        """Start strictrun without waiting (for signal tests)."""
        merged = dict(os.environ)
        merged["LED89_STRICT_ROOT"] = self.dir
        if env:
            merged.update(env)
        if preload:
            preloads = [SHIM]
            if merged.get("LD_PRELOAD"):
                preloads.append(merged["LD_PRELOAD"])
            merged["LD_PRELOAD"] = ":".join(preloads)
        proc = subprocess.Popen(
            [RUNNER],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=merged,
            preexec_fn=_limit_fn(self.limits),
        )
        self.subprocesses.append(proc)
        if script:
            proc.stdin.write(script.encode())
            proc.stdin.flush()
        return proc

    def cleanup(self):
        for proc in self.subprocesses:
            if proc.poll() is None:
                proc.kill()
                proc.wait(timeout=10)
        if os.environ.get("LED89_STRICT_KEEP"):
            return
        shutil.rmtree(self.dir, ignore_errors=True)


def expect(condition, message):
    if not condition:
        raise AssertionError(message)


def expect_rc(rc, expected, what=""):
    if isinstance(expected, (set, frozenset, tuple, list)):
        if rc not in expected:
            raise AssertionError(
                "%s: rc=%s not in %s"
                % (what, RESULT_NAMES.get(rc, rc),
                   [RESULT_NAMES.get(e, e) for e in expected])
            )
        return
    if rc != expected:
        raise AssertionError(
            "%s: rc=%s, expected %s"
            % (what, RESULT_NAMES.get(rc, rc), RESULT_NAMES.get(expected, expected))
        )


def expect_state(handle, first=None, stable=None, end=None, rev=None):
    rc, state = get_state(handle)
    expect_rc(rc, OK, "get_state")
    if first is not None:
        expect(state.first.value() == first,
               "first=%d, expected %d" % (state.first.value(), first))
    if stable is not None:
        expect(state.stable_end.value() == stable,
               "stable_end=%d, expected %d" % (state.stable_end.value(), stable))
    if end is not None:
        expect(state.end.value() == end,
               "end=%d, expected %d" % (state.end.value(), end))
    if rev is not None:
        expect(state.revision.value() == rev,
               "revision=%d, expected %d" % (state.revision.value(), rev))
    return state


def expect_payload(handle, index, payload):
    rc, size, data = read(handle, index, max(len(payload), 1))
    expect_rc(rc, OK, "read(%d)" % index)
    expect(size == len(payload),
           "read(%d): size=%d, expected %d" % (index, size, len(payload)))
    expect(data == bytes(payload),
           "read(%d): payload mismatch %r != %r" % (index, data, bytes(payload)))


def scan_records(handle, start=1):
    """Iterate from start and return [(index, bytes)]."""
    rc, it = iter_init(handle, start)
    expect_rc(rc, OK, "iter_init(%d)" % start)
    records = []
    while True:
        rc, index, size, data = iter_next(it, 0, null_buffer=False)
        if rc == DONE:
            break
        if rc == ETOOSMALL:
            rc, index, size, data = iter_next(it, size, null_buffer=False)
        expect_rc(rc, OK, "iter_next")
        records.append((index, data))
    return records


def expand(table):
    """Yield (name, params) rows from a table of dicts."""
    for row in table:
        params = dict(row)
        name = params.pop("name")
        yield name, params


def parse_transcript(text):
    """Parse strictrun output into a list of key=value dicts."""
    rows = []
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        row = {}
        for token in line.split():
            key, sep, value = token.partition("=")
            if sep:
                row[key] = value
        rows.append(row)
    return rows


def now():
    return time.perf_counter()


def percentile(samples, fraction):
    ordered = sorted(samples)
    if not ordered:
        return 0.0
    position = int(round(fraction * (len(ordered) - 1)))
    return ordered[position]


def run_case(case_obj, tier, root, keep=False):
    context = CaseContext(case_obj.name, tier, root)
    started = now()
    old_handler = signal.signal(signal.SIGALRM, _alarm)
    signal.setitimer(signal.ITIMER_REAL, context.limits["timeout"])
    error = None
    try:
        case_obj.fn(context)
    except BaseException:
        error = traceback.format_exc()
    finally:
        signal.setitimer(signal.ITIMER_REAL, 0)
        signal.signal(signal.SIGALRM, old_handler)
        try:
            context.cleanup()
        except BaseException:
            if error is None:
                error = traceback.format_exc()
    elapsed = now() - started
    return error, elapsed


def discover_modules():
    directory = os.path.dirname(os.path.abspath(__file__))
    names = sorted(
        name[:-3]
        for name in os.listdir(directory)
        if name.startswith("cases_") and name.endswith(".py")
    )
    for name in names:
        __import__(name)


def main(argv=None):
    parser = argparse.ArgumentParser(description="libledger89 strict suite")
    parser.add_argument("--tier", default="fast", choices=sorted(TIER_ORDER))
    parser.add_argument("-k", "--filter", default=None, help="substring filter")
    parser.add_argument("--list", action="store_true", help="list cases")
    parser.add_argument("--json", default=None, help="write a JSON report")
    parser.add_argument("--seed", type=int,
                        default=int(os.environ.get("LED89_STRICT_SEED", "89")))
    parser.add_argument("--max-failures", type=int, default=0)
    args = parser.parse_args(argv)

    signal.signal(signal.SIGXFSZ, signal.SIG_IGN)
    os.makedirs(TMP_ROOT, exist_ok=True)
    discover_modules()

    tier_index = TIER_ORDER[args.tier]
    selected = [
        item for item in registry()
        if TIER_ORDER[item.min_tier] <= tier_index
    ]
    if args.filter:
        selected = [item for item in selected if args.filter in item.name]
    selected.sort(key=lambda item: item.name)

    if args.list:
        for item in selected:
            print("%-10s %s" % (item.min_tier, item.name))
        print("%d case(s)" % len(selected))
        return 0

    print("strict tier=%s seed=%d cases=%d" % (args.tier, args.seed, len(selected)))
    failures = 0
    reports = []
    for item in selected:
        error, elapsed = run_case(item, args.tier, TMP_ROOT)
        status = "ok" if error is None else "FAIL"
        print("%-6s %8.3fs %s" % (status, elapsed, item.name))
        reports.append({
            "name": item.name,
            "tier": item.min_tier,
            "seconds": round(elapsed, 4),
            "ok": error is None,
        })
        if error is not None:
            failures += 1
            sys.stderr.write("\n--- %s ---\n%s\n" % (item.name, error))
            if args.max_failures and failures >= args.max_failures:
                print("stopping after %d failure(s)" % failures)
                break
    summary = {
        "tier": args.tier,
        "seed": args.seed,
        "cases": len(selected),
        "failures": failures,
        "seconds": round(sum(item["seconds"] for item in reports), 3),
        "results": reports,
    }
    if args.json:
        os.makedirs(os.path.dirname(os.path.abspath(args.json)), exist_ok=True)
        with open(args.json, "w") as handle:
            json.dump(summary, handle, indent=2, sort_keys=True)
    if failures:
        print("FAILED: %d of %d case(s)" % (failures, len(selected)))
        return 1
    print("PASS: %d case(s) in %.3fs" % (len(selected), summary["seconds"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
