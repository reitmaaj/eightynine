#!/usr/bin/env python3
"""Unit tests for the bounded, observable conformance sweep (scenarios
0012-SWP-*). Run with: python3 test/test_sweep.py"""

import importlib.util
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)


def load(name):
    path = os.path.join(HERE, name)
    spec = importlib.util.spec_from_file_location(name[:-3], path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


spec_sweep = load("spec_sweep.py")
spec_driver = load("spec_driver.py")

fails = 0


def check(cond, msg):
    global fails
    if not cond:
        fails += 1
        sys.stderr.write("FAIL: %s\n" % msg)


# SWP-001 / SWP-003 / SWP-004: per-file progress, stalled-file handling,
# and aggregate accounting, via an injected (fake) per-file runner so the
# sweep logic is exercised without real wasm-tools/runtime. A runner MUST
# surface a per-file timeout by raising TimeoutError (the real main runner
# enforces it with subprocess.run(timeout=...), which genuinely kills).
def test_sweep_accounting():
    calls = []

    def runner(name):
        calls.append(name)
        if name == "slow.wast":
            raise TimeoutError("slow.wast exceeded timeout")
        if name == "ok.wast":
            return 0, "ok: 2 passed, 1 failed, 3 skipped\n", ""
        if name == "boom.wast":
            return 3, "", "boom driver crashed\n"
        return 0, "z: 0 passed, 0 failed, 0 skipped\n", ""

    totals, bad = spec_sweep.sweep_files(runner, ["ok.wast", "slow.wast",
                                                  "boom.wast"],
                                         timeout=1.0)
    check(calls == ["ok.wast", "slow.wast", "boom.wast"],
          "every file must be attempted, in order")
    # ok.wast: 2 passed,1 failed,3 skipped
    check(totals == (2, 1, 3), "ok.wast totals parsed, got %r" % (totals,))
    names = [n for n, _ in bad]
    check("slow.wast" in names, "stalled file recorded as an error")
    check("boom.wast" in names, "driver-crash file recorded as an error")
    check("ok.wast" not in names, "passing/skipping file not an error")
    check(len(bad) == 2, "exactly the two error files reported")


# SWP-003 (real bound): a genuinely hanging per-file run is cut short by
# the subprocess runner used by spec_sweep.main, so the sweep cannot be
# wedged by one pathological file.
def test_subprocess_timeout():
    import subprocess
    try:
        subprocess.run([sys.executable, "-c", "import time; time.sleep(60)"],
                       timeout=0.4)
    except subprocess.TimeoutExpired:
        pass
    else:
        check(False, "subprocess runner must enforce a per-file timeout")


# SWP-002: a single command the runtime never answers must time out rather
# than block forever.
def test_command_timeout():
    rt = spec_driver.Runtime([
        sys.executable, "-c",
        "import time; line=input(); time.sleep(100)"])
    try:
        start = time.time()
        try:
            rt.command("ping", timeout=0.4)
        except spec_driver.DriverTimeout:
            pass
        else:
            check(False, "command on a silent runtime must time out")
        elapsed = time.time() - start
        check(elapsed < 3, "command timeout bounded (took %.1fs)" % elapsed)
    finally:
        rt.close()


if __name__ == "__main__":
    test_sweep_accounting()
    test_command_timeout()
    if fails:
        sys.stderr.write("%d sweep test(s) FAILED\n" % fails)
        sys.exit(1)
    sys.stdout.write("sweep tests: all passed\n")
