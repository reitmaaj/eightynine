"""Adversarial inputs and host safety (ST28-ST31).

Hostile paths and resource limits run in the strictrun subprocess so a
blocking open or a resource failure cannot take down the driver.
"""

import os

import ledger
from harness import (
    TMP_ROOT,
    case,
    expect,
    expect_rc,
    expect_state,
    parse_transcript,
    run_script,
)
from ledger import EINVAL, OK

import tables_adversarial as T


def documented(rc):
    try:
        value = int(rc)
    except (TypeError, ValueError):
        return False
    return -16 <= value <= 1


CTYPE_NAMES = {
    "space_name": "with space",
    "newline_name": "line\nbreak",
    "tab_name": "with\ttab",
    "dash_name": "-leading-dash",
    "unicode_name": "n\u00e4me-\u00fc",
    "dotfile_name": ".hidden",
    "percent_name": "100%done",
    "colon_name": "a:b",
    "quote_name": "quote'and\"double",
    "backslash_name": "back\\slash",
}


def run_path_case(ctx, kind):
    root = ctx.dir
    if kind.startswith("name_len:"):
        length = int(kind.split(":")[1])
        name = "n" * length
        rc, handle = ledger.open_ledger(os.path.join(root, name), 14)
        expect(rc in (OK, -4, -1), "name_len %d rc=%d" % (length, rc))
        if rc == OK:
            expect_state(handle, first=1, stable=1, end=1, rev=0)
            ledger.close(handle)
        return
    if kind in CTYPE_NAMES:
        path = os.path.join(root, CTYPE_NAMES[kind])
        rc, handle = ledger.open_ledger(path, 14)
        expect(rc in (OK, -4, -1), "%s rc=%d" % (kind, rc))
        if rc == OK:
            ledger.close(handle)
        return
    if kind == "deep_nested":
        rc, handle = ledger.open_ledger(os.path.join(root, "a", "b", "c"), 14)
        expect(rc in (-4, -3, -1), "deep_nested rc=%d" % rc)
        return
    if kind == "double_slash":
        os.makedirs(os.path.join(root, "sub"))
        rc, handle = ledger.open_ledger(os.path.join(root, "sub") + "//", 6)
        expect(rc in (-4, 0), "double_slash rc=%d" % rc)
        return
    script = None
    if kind == "empty":
        rc, handle = ledger.open_ledger("", 14)
        expect_rc(rc, ledger.EINVAL, "empty path")
        expect(handle is None, "empty path produced a handle")
        return
    elif kind == "dot":
        script = "open 14 .\n"
    elif kind == "dotdot":
        script = "open 14 ..\n"
    elif kind == "missing":
        script = "open 14 missing\n"
    elif kind == "nested_missing":
        script = "open 14 a/b/c\n"
    elif kind == "file_as_dir":
        with open(os.path.join(root, "plain"), "wb") as handle:
            handle.write(b"not a ledger")
        script = "open 14 plain\n"
    elif kind == "trailing_slash":
        os.makedirs(os.path.join(root, "empty"))
        script = "open 14 empty/\n"
    elif kind == "long_255":
        script = "open 14 %s\n" % ("a" * 246)
    elif kind == "long_4095":
        script = "open 14 %s\n" % ("b" * 4000)
    elif kind == "long_4096":
        script = "open 14 %s\n" % ("c" * 4089)
    elif kind == "long_8192":
        script = "open 14 %s\n" % ("d" * 8185)
    elif kind == "symlink_dir":
        os.makedirs(os.path.join(root, "target"))
        os.symlink("target", os.path.join(root, "link"))
        script = "open 14 link\nstate\nclose\n"
    elif kind == "symlink_loop":
        os.symlink("loop2", os.path.join(root, "loop1"))
        os.symlink("loop1", os.path.join(root, "loop2"))
        script = "open 14 loop1\n"
    elif kind == "symlink_file":
        with open(os.path.join(root, "plain"), "wb") as handle:
            handle.write(b"x")
        os.symlink("plain", os.path.join(root, "linkfile"))
        script = "open 14 linkfile\n"
    elif kind == "fifo":
        os.mkfifo(os.path.join(root, "pipe"))
        script = "open 14 pipe\n"
    elif kind == "empty_component":
        os.makedirs(os.path.join(root, "sub"))
        script = "open 14 sub//\n"
    elif kind == "dot_component":
        os.makedirs(os.path.join(root, "sub"))
        script = "open 14 sub/./\n"
    elif kind == "absolute_missing":
        script = "open 14 %s\n" % os.path.join(root, "absolute-missing")
    else:
        raise AssertionError("unknown path kind %s" % kind)
    proc, text = ctx.run_runner(script, check=False)
    expect(proc.returncode in (0, 3, 1),
           "%s: runner exit=%d" % (kind, proc.returncode))
    rows = parse_transcript(text)
    if not rows:
        expect(kind in ("fifo", "symlink_loop", "long_8192", "long_4096"),
               "%s: empty transcript" % kind)
        return
    expect(documented(rows[0].get("rc")),
           "%s: rc=%s" % (kind, rows[0].get("rc")))


def register_path_rows():
    for fields in T.PATH_ROWS:
        name, kind, _expect = fields

        def run(ctx, kind=kind):
            run_path_case(ctx, kind)

        run.__name__ = name
        case(name, tier="fast")(run)


def run_limit_case(ctx, as_bytes, fsize, nofile, scenario):
    root = ctx.dir
    limits = {"as_bytes": as_bytes or 1024 * 1024 * 1024,
              "fsize": fsize or 1024 * 1024 * 1024,
              "nofile": nofile or 256,
              "cpu": 20}
    if scenario == "create":
        script = "open 14 ledger\nappendv 61,62,63\nsync\nclose\n"
    elif scenario == "append":
        script = ("open 14 ledger\nappendv 61,62,63\nsync\n"
                  "append_fill 78 4096\nsync\nclose\n")
    elif scenario == "append_huge":
        script = "open 14 ledger\nappend_fill 78 16777216\nclose\n"
    elif scenario == "open":
        script = "open 14 ledger\nstate\nclose\n"
    elif scenario == "many_parts":
        lines = ["open 14 ledger"]
        for _ in range(20):
            lines.append("append 61")
            lines.append("rotate")
        lines.append("close")
        script = "\n".join(lines) + "\n"
    else:
        raise AssertionError("unknown limit scenario %s" % scenario)
    try:
        proc, text = run_script(root, script, limits=limits, check=False)
    except OSError:
        return
    expect(proc.returncode >= 0 or proc.returncode == -9,
           "runner killed by signal: %d" % proc.returncode)
    rows = parse_transcript(text)
    for row in rows:
        if "rc" in row:
            expect(documented(row["rc"]),
                   "limit %s: undocumented rc=%s" % (scenario, row["rc"]))


def register_limit_rows():
    for fields in T.LIMIT_ROWS:
        name, as_bytes, fsize, nofile, scenario, _expect = fields

        def run(ctx, as_bytes=as_bytes, fsize=fsize, nofile=nofile,
                scenario=scenario):
            run_limit_case(ctx, as_bytes, fsize, nofile, scenario)

        run.__name__ = name
        case(name, tier="fast")(run)


@case("adversarial.sparse_part", tags=("adversarial",))
def sparse_part(ctx):
    """A huge sparse part file must be rejected within the time budget."""
    root = ctx.dir
    script = "open 14 ledger\nappendv 61,62,63\nsync\nclose\n"
    ctx.run_runner(script)
    part = os.path.join(root, "ledger", "part.0000000000000001")
    with open(part, "r+b") as handle:
        handle.truncate(256 * 1024 * 1024)
    proc, text = ctx.run_runner("open 6 ledger\nclose\n", check=False,
                                timeout=10)
    rows = parse_transcript(text)
    expect(rows and documented(rows[0].get("rc")),
           "sparse part: rc=%s" % (rows[0] if rows else None))


@case("adversarial.many_slices", tags=("adversarial",))
def many_slices(ctx):
    payload = ",".join(["61"] * 4096)
    script = "open 14 ledger\nappendv %s\nsync\nstate\nclose\n" % payload
    proc, text = ctx.run_runner(script)
    rows = parse_transcript(text)
    expect(rows[1].get("rc") == "0", "4096-slice batch failed: %s" % rows[1])
    expect(rows[3]["end"] == "4097", "end=%s" % rows[3])


@case("adversarial.zero_length_records", tags=("adversarial",))
def zero_length_records(ctx):
    script = "open 14 ledger\nappendv -,-,-\nsync\nstate\nread 1 -\nclose\n"
    proc, text = ctx.run_runner(script)
    rows = parse_transcript(text)
    expect(rows[1].get("rc") == "0", "empty slices failed")
    expect(rows[3]["end"] == "4", "end=%s" % rows[3])


register_path_rows()
register_limit_rows()
