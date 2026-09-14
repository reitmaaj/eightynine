"""ST02/ST03: runner protocol and fault-shim self-tests.

Proves the out-of-process runner transcript matches the ctypes driver and
that the shim fails exactly the configured call.
"""

import os

import ledger
from harness import (
    TMP_ROOT,
    case,
    expect,
    expect_rc,
    parse_transcript,
)
from ledger import ECORRUPT, EIO, ENOENT, OK

OPEN_RDWR_CREATE = ledger.OPEN_RDWR | ledger.OPEN_CREATE


def _aux(ctx, suffix):
    path = os.path.join(TMP_ROOT, "%s-%d.%s" % (ctx.name, os.getpid(), suffix))
    if os.path.exists(path):
        os.unlink(path)
    return path


def _fault_env(ctx, ops, nth=1, err="EIO", mode="fail", entropy=False):
    env = {
        "LED89_FAULT_OPS": ops,
        "LED89_FAULT_NTH": str(nth),
        "LED89_FAULT_ERR": err,
        "LED89_FAULT_MODE": mode,
        "LED89_FAULT_ROOT": ctx.dir,
        "LED89_FAULT_COUNT": _aux(ctx, "count"),
        "LED89_FAULT_MARK": _aux(ctx, "mark"),
    }
    if entropy:
        env["LED89_FAULT_ENTROPY"] = "1"
    return env


@case("shim.runner_smoke", tags=("shim",))
def shim_runner_smoke(ctx):
    script = "\n".join([
        "open 14 ledger",
        "append 6162",
        "append 6364",
        "sync",
        "state",
        "read 1 8",
        "read_crc 2",
        "iter_init 1",
        "iter_next 8",
        "verify",
        "close",
    ])
    proc, text = ctx.run_runner(script)
    rows = parse_transcript(text)
    expect(rows[0]["rc"] == "0", "open rc %s" % rows[0]["rc"])
    expect(rows[1]["rc"] == "0" and rows[1]["idx"] == "1", "append 1")
    expect(rows[2]["rc"] == "0" and rows[2]["idx"] == "2", "append 2")
    expect(rows[3]["rc"] == "0" and rows[3]["stable"] == "3", "sync")
    expect(rows[4]["first"] == "1" and rows[4]["end"] == "3", "state")
    expect(rows[5]["data"] == "6162", "read payload")
    expect(rows[6]["size"] == "2", "read_crc size")
    expect(rows[7]["rc"] == "0", "iter_init")
    expect(rows[8]["idx"] == "1" and rows[8]["data"] == "6162", "iter_next")
    expect(rows[9]["rc"] == "0", "verify")


@case("shim.fail_pwrite", tags=("shim",))
def shim_fail_pwrite(ctx):
    ctx.run_runner("open 14 ledger\nappend 6162\nsync\nclose\n")
    env = _fault_env(ctx, "pwrite", nth=1, mode="fail")
    script = "\n".join(["open 6 ledger", "append 6364", "close"])
    proc, text = ctx.run_runner(script, env=env, preload=True, check=False)
    rows = parse_transcript(text)
    expect(rows[1]["rc"] == str(EIO), "append rc=%s" % rows[1]["rc"])
    expect(proc.returncode == 3, "exit=%d" % proc.returncode)
    with open(env["LED89_FAULT_COUNT"]) as handle:
        fired = handle.read()
    expect("pwrite" in fired, "fired=%r" % fired)

    proc, text = ctx.run_runner("open 6 ledger\nstate\nread 1 8\nclose\n")
    rows = parse_transcript(text)
    expect(rows[0]["rc"] == "0", "reopen after failed append")
    expect(rows[1]["end"] == "2", "failed append became visible: %s" % rows[1])
    expect(rows[2]["data"] == "6162", "record 1 damaged: %s" % rows[2])


@case("shim.eintr_transparent", tags=("shim",))
def shim_eintr_transparent(ctx):
    env = _fault_env(ctx, "pwrite", nth=1, mode="eintr")
    script = "\n".join(["open 14 ledger", "append 6162", "sync", "state", "close"])
    proc, text = ctx.run_runner(script, env=env, preload=True, check=False)
    rows = parse_transcript(text)
    expect(rows[1]["rc"] == "0", "append under EINTR rc=%s" % rows[1]["rc"])
    expect(rows[2]["rc"] == "0", "sync under EINTR")
    expect(rows[3]["end"] == "2", "state end=%s" % rows[3]["end"])
    with open(env["LED89_FAULT_COUNT"]) as handle:
        expect("pwrite" in handle.read(), "eintr did not fire")


@case("shim.kill_marks_and_recovers", tags=("shim",))
def shim_kill_marks_and_recovers(ctx):
    ctx.run_runner("open 14 ledger\nappend 6162\nsync\nclose\n")
    env = _fault_env(ctx, "pwrite", nth=1, mode="kill")
    script = "\n".join(["open 6 ledger", "append 6364", "sync", "close"])
    proc, _text = ctx.run_runner(script, env=env, preload=True, check=False)
    expect(proc.returncode == 137, "kill exit=%d" % proc.returncode)
    expect(os.path.exists(env["LED89_FAULT_MARK"]), "mark missing")
    proc, text = ctx.run_runner("open 6 ledger\nstate\nverify\nread 1 8\nclose\n")
    rows = parse_transcript(text)
    expect(rows[0]["rc"] == "0", "reopen after kill")
    expect(rows[1]["end"] == "2", "state after kill: %s" % rows[1])
    expect(rows[2]["rc"] == "0", "verify after kill")
    expect(rows[3]["data"] == "6162", "record 1 after kill: %s" % rows[3])


@case("shim.entropy_failure", tags=("shim",))
def shim_entropy_failure(ctx):
    env = _fault_env(ctx, "read", nth=1, mode="fail", entropy=True)
    proc, text = ctx.run_runner("open 14 ledger\nclose\n", env=env, preload=True, check=False)
    rows = parse_transcript(text)
    expect(rows[0]["rc"] != "0", "create should fail: %s" % rows[0])
    expect(proc.returncode == 3, "exit=%d" % proc.returncode)


@case("shim.clean_path_untouched", tags=("shim",))
def shim_clean_path_untouched(ctx):
    """Faults must not fire for paths outside the configured root."""
    outside = ctx.subdir("outside")
    env = _fault_env(ctx, "pwrite", nth=0, mode="fail")
    env["LED89_FAULT_ROOT"] = outside
    script = "\n".join(["open 14 ledger", "append 6162", "sync", "close"])
    proc, text = ctx.run_runner(script, env=env, preload=True, check=False)
    rows = parse_transcript(text)
    expect(rows[1]["rc"] == "0", "outside root fired: %s" % rows[1])
    expect(proc.returncode == 0, "exit=%d" % proc.returncode)
