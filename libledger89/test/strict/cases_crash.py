"""Crash matrix (ST22-ST24).

Every row of tables_fault.CRASH_ROWS kills the process at one syscall and
call index, then reopens and checks: the recovered state is a prefix of the
permitted states, invariants hold, and verify is clean.
"""

import os

from harness import TMP_ROOT, case, expect, parse_transcript
from ledger import OK

import tables_fault as T
from cases_fault import MUTATION, SETUP, fault_env


def check_crash_row(ctx, scenario, op, nth):
    env = fault_env(ctx, op, nth, "kill", "EIO", "crash")
    script = SETUP[scenario] + MUTATION[scenario] + "close\n"
    proc, text = ctx.run_runner(script, env=env, preload=True, check=False)
    expect(proc.returncode in (0, 137),
           "runner exit=%d\n%s" % (proc.returncode, text))
    if proc.returncode == 137:
        expect(os.path.exists(env["LED89_FAULT_MARK"]),
               "kill without mark")
    reads = "\n".join("read_crc %d" % index for index in range(1, 8))
    _proc, text = ctx.run_runner(
        "open 6 ledger\nstate\nverify\n%s\n" % reads, check=False
    )
    rows = parse_transcript(text)
    expect(rows[0].get("rc") == "0",
           "recovery failed: %s\ncrash run:\n%s" % (rows[0], text))
    first = int(rows[1]["first"])
    stable = int(rows[1]["stable"])
    end = int(rows[1]["end"])
    expect(first <= stable <= end,
           "invariant broken: %d %d %d" % (first, stable, end))
    expect(rows[2].get("rc") == "0", "verify failed: %s" % rows[2])
    for offset, row in enumerate(rows[3:], start=1):
        rc = row.get("rc")
        if first <= offset < end:
            expect(rc == "0",
                   "index %d readable range [%d,%d) gave rc=%s"
                   % (offset, first, end, rc))
        elif offset < first:
            expect(rc == "-11",
                   "index %d below first gave rc=%s" % (offset, rc))
        else:
            expect(rc == "-4",
                   "index %d past end gave rc=%s" % (offset, rc))


def register_crash_rows():
    for index, fields in enumerate(T.CRASH_ROWS):
        name, scenario, op, nth = fields
        tier = "fast" if index % 53 == 0 else "full"

        def run(ctx, fields=fields):
            check_crash_row(ctx, *fields[1:])

        run.__name__ = name
        case(name, tier=tier)(run)


register_crash_rows()
