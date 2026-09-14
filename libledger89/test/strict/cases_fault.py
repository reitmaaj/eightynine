"""Fault matrix (ST19-ST21).

Every row of tables_fault.FAULT_ROWS arms the shim at one syscall and call
index, runs a scenario, and checks: no crash, only documented result codes,
and a clean recovery with intact invariants. EINTR rows must succeed
transparently.
"""

import os

from harness import TMP_ROOT, case, expect, parse_transcript
from ledger import OK

import tables_fault as T

SETUP = {
    "append": "open 14 ledger\nappendv 61,62,63\nsync\n",
    "appendv": "open 14 ledger\nappendv 61,62,63\nsync\n",
    "appendv_at": "open 14 ledger\nappendv 61,62,63\nsync\n",
    "sync": "open 14 ledger\nappendv 61,62,63\nsync\n",
    "truncate": "open 14 ledger\nappendv 61,62,63\nsync\n",
    "truncate_mid": "open 14 ledger\nappendv 61,62,63\nsync\n",
    "prune": ("open 14 ledger\nappendv 61,62,63\nsync\nrotate\n"
              "appendv 64,65,66\nsync\n"),
    "rotate": "open 14 ledger\nappendv 61,62,63\nsync\n",
    "rotate_empty": ("open 14 ledger\nappendv 61,62,63\nsync\nrotate\n"),
    "recover": "open 14 ledger\nappendv 61,62,63\nsync\nclose\n",
    "read": "open 14 ledger\nappendv 61,62,63\nsync\n",
    "verify": "open 14 ledger\nappendv 61,62,63\nsync\n",
}

MUTATION = {
    "append": "append 78\n",
    "appendv": "appendv 64,65,66\n",
    "appendv_at": "appendv_at 0 4 64,65\n",
    "sync": "sync\n",
    "truncate": "truncate 2\n",
    "truncate_mid": "truncate 1\n",
    "prune": "prune 4\n",
    "rotate": "rotate\n",
    "rotate_empty": "rotate\n",
    "recover": "",
    "read": "read_crc 1\n",
    "verify": "verify\n",
}

DOCUMENTED = {str(code) for code in range(-16, 2)}


def fault_env(ctx, op, nth, mode, err, tag):
    count = os.path.join(TMP_ROOT, "%s-%s-%d-%s.count" % (ctx.name, op, nth, tag))
    mark = os.path.join(TMP_ROOT, "%s-%s-%d-%s.mark" % (ctx.name, op, nth, tag))
    for path in (count, mark):
        if os.path.exists(path):
            os.unlink(path)
    return {
        "LED89_FAULT_OPS": op,
        "LED89_FAULT_NTH": str(nth),
        "LED89_FAULT_ERR": err,
        "LED89_FAULT_MODE": mode,
        "LED89_FAULT_ROOT": ctx.dir,
        "LED89_FAULT_COUNT": count,
        "LED89_FAULT_MARK": mark,
    }


def check_fault_row(ctx, scenario, op, nth, mode, err, expect_kind):
    env = fault_env(ctx, op, nth, mode, err, "run")
    script = SETUP[scenario] + MUTATION[scenario] + "close\n"
    proc, text = ctx.run_runner(script, env=env, preload=True, check=False)
    expect(proc.returncode in (0, 3),
           "runner died: exit=%d\n%s" % (proc.returncode, text))
    rows = parse_transcript(text)
    for row in rows:
        if "rc" in row:
            expect(row["rc"] in DOCUMENTED,
                   "undocumented rc=%s in %s" % (row["rc"], text))
    if mode == "eintr" and op != "close":
        expect(all(row.get("rc") == "0" for row in rows if "rc" in row),
               "EINTR run reported an error\n%s" % text)
        return
    if mode == "eintr" and proc.returncode != 3:
        return
    if proc.returncode != 3:
        return
    _proc, text = ctx.run_runner(
        "open 6 ledger\nstate\nverify\nclose\n", check=False
    )
    rows = parse_transcript(text)
    expect(rows[0].get("rc") == "0",
           "recovery failed after fault: %s\nfirst run:\n%s" % (rows[0], text))
    first = int(rows[1]["first"])
    stable = int(rows[1]["stable"])
    end = int(rows[1]["end"])
    expect(first <= stable <= end,
           "invariant broken after fault: %d %d %d" % (first, stable, end))
    expect(rows[2].get("rc") == "0", "verify failed after fault: %s" % rows[2])


def register_fault_rows():
    for index, fields in enumerate(T.FAULT_ROWS):
        name, scenario, op, nth, mode, err, expect_kind = fields
        if index % 113 == 0:
            tier = "fast"
        elif nth <= 4:
            tier = "full"
        else:
            tier = "long"

        def run(ctx, fields=fields):
            check_fault_row(ctx, *fields[1:])

        run.__name__ = name
        case(name, tier=tier)(run)


register_fault_rows()


def high_nth(ctx, scenario, ops):
    for op in ops:
        for nth in range(13, 25):
            check_fault_row(ctx, scenario, op, nth, "fail", "EIO", "error")


@case("fault.sync_high_nth", tags=("fault",))
def sync_high_nth(ctx):
    high_nth(ctx, "sync", ("pwrite", "fsync", "fdatasync", "rename"))


@case("fault.truncate_high_nth", tags=("fault",))
def truncate_high_nth(ctx):
    high_nth(ctx, "truncate", ("pwrite", "fsync", "rename", "unlink"))


@case("fault.rotate_high_nth", tags=("fault",))
def rotate_high_nth(ctx):
    high_nth(ctx, "rotate", ("pwrite", "fsync", "rename", "mkdir"))


@case("fault.recover_high_nth", tags=("fault",))
def recover_high_nth(ctx):
    high_nth(ctx, "recover", ("pread", "open", "opendir", "readdir"))
