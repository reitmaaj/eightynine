#!/usr/bin/env python3
"""Aggregate the conformance driver across every top-level testsuite file.

Usage: spec_sweep.py <runtime> [<dir>]

Runs test/spec_driver.py on each *.wast in <dir> (default
vendor/testsuite-main) and aggregates the per-command totals. Files whose
commands do not all pass (or skip) are listed individually.
"""

import os
import re
import signal
import subprocess
import sys

here = os.path.dirname(os.path.abspath(__file__))
driver = os.path.join(here, "spec_driver.py")
default_dir = os.path.join(here, os.pardir, "vendor", "testsuite-main")
DEFAULT_TIMEOUT = float(os.environ.get("W89_SWEEP_TIMEOUT", "600"))


def sweep_files(runner, files, timeout=DEFAULT_TIMEOUT):
    """Run a per-file `runner` over `files`, printing progress per file.

    `runner(name)` MUST return (returncode, stdout, stderr) and SHOULD
    raise TimeoutError if the file exceeds `timeout`. A file that raises
    TimeoutError or returns nonzero is recorded in `bad` and the sweep
    continues. Returns (tp, tf, ts) totals and the `bad` error list.
    """
    tp = tf = ts = 0
    bad = []
    for name in files:
        print(name, flush=True)
        try:
            rc, out, err = runner(name)
        except TimeoutError:
            bad.append((name, "file exceeded %.0fs timeout" % timeout))
            sys.stderr.write("TIMEOUT %s: file exceeded %.0fs\n"
                             % (name, timeout))
            continue
        if rc != 0:
            bad.append((name, (err or out).strip()))
            sys.stderr.write(err or "")
            continue
        m = re.search(r": (\d+) passed, (\d+) failed, (\d+) skipped$",
                      out.strip())
        if not m:
            bad.append((name, "unparseable driver output"))
            continue
        p, f, s = (int(g) for g in m.groups())
        tp += p
        tf += f
        ts += s
    print("%d files: %d passed, %d failed, %d skipped" %
          (len(files), tp, tf, ts))
    for name, err in bad:
        print("FAIL %s: %s" % (name, err.replace("\n", " | ")))
    return (tp, tf, ts), bad


def main():
    args = sys.argv[1:]
    if len(args) < 1:
        sys.stderr.write("usage: spec_sweep.py <runtime> [<dir>] "
                         "[--filter <name>...]\n")
        return 2
    runtime = args[0]
    filt = []
    if "--filter" in args:
        i = args.index("--filter")
        filt = args[i + 1:]
        args = args[:i]
    if len(args) > 2:
        sys.stderr.write("usage: spec_sweep.py <runtime> [<dir>] "
                         "[--filter <name>...]\n")
        return 2
    testsuite = os.path.abspath(args[1]) if len(args) == 2 \
        else os.path.abspath(default_dir)
    files = sorted(f for f in os.listdir(testsuite) if f.endswith(".wast"))
    if filt:
        files = [f for f in files
                 if os.path.splitext(f)[0] in set(filt)]
    timeout = DEFAULT_TIMEOUT

    def runner(name):
        try:
            r = subprocess.run([sys.executable, driver, runtime,
                                os.path.join(testsuite, name)],
                               capture_output=True, text=True,
                               timeout=timeout, start_new_session=True)
        except subprocess.TimeoutExpired as e:
            # Kill the driver and any runtime it spawned (new session).
            pid = getattr(e, "pid", None)
            if pid is not None:
                try:
                    os.killpg(pid, signal.SIGKILL)
                except OSError:
                    pass
            raise TimeoutError(name)
        return r.returncode, r.stdout, r.stderr

    (tp, tf, ts), bad = sweep_files(runner, files, timeout=timeout)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
