"""Corruption sweeps (ST15-ST18).

Each row of tables_format.CORRUPT_ROWS and TRUNCATE_FILE_ROWS mutates one
byte (or truncates one file) of a fixture and checks the classification.
Rows are tiered: every 149th runs fast, every 5th runs in the full tier, and
the long tier runs all of them.
"""

import os
import shutil

import formatlib as fmt
from fixtures import copy_fixture
from harness import TMP_ROOT, case, expect, parse_transcript
from ledger import OK

import tables_format as T

_TEMPLATES = {}


def template(name):
    if name not in _TEMPLATES:
        import harness

        scratch = harness.CaseContext("corrupt-template-" + name, "fast",
                                      TMP_ROOT)
        os.makedirs(scratch.dir, exist_ok=True)
        copy_fixture(scratch, name)
        _TEMPLATES[name] = fmt.parse_ledger(scratch.ledger_path)
        shutil.rmtree(scratch.dir, ignore_errors=True)
    return _TEMPLATES[name]


def file_name(fixture, file_kind, explicit):
    if explicit:
        return explicit
    if file_kind == "MANIFEST":
        return template(fixture)["manifest_name"]
    return file_kind


def target_index(fixture, explicit, region):
    start = region.index("record[") + len("record[")
    record_index = int(region[start: region.index("]", start)])
    batch_index = int(region[len("batch["): region.index("]")])
    parsed = template(fixture)
    part = parsed["files"][explicit]
    batch = part["batches"][batch_index]
    return batch["first"] + record_index


def mutate(path, offset, spec):
    with open(path, "rb") as handle:
        data = bytearray(handle.read())
    op, _, arg = spec.partition(":")
    value = int(arg, 16)
    original = data[offset]
    if op == "set":
        data[offset] = value
    elif op == "xor":
        data[offset] = original ^ value
    elif op == "add":
        data[offset] = (original + value) & 0xFF
    elif op == "sub":
        data[offset] = (original - value) & 0xFF
    elif op == "swap":
        data[offset] = ((original << 4) | (original >> 4)) & 0xFF
    else:
        raise AssertionError("unknown mutation %s" % spec)
    changed = data[offset] != original
    with open(path, "wb") as handle:
        handle.write(bytes(data))
    return changed


def codes(allowed):
    import ledger

    by_name = {name: code for code, name in ledger.RESULT_NAMES.items()}
    return {str(by_name[name]) for name in allowed}


def check_row(ctx, fixture, file_kind, explicit, region, offset, spec, check,
              allowed):
    copy_fixture(ctx, fixture)
    path = os.path.join(ctx.ledger_path, file_name(fixture, file_kind, explicit))
    if not mutate(path, offset, spec):
        return
    if check == "open":
        _proc, text = ctx.run_runner("open 6 ledger\nstate\nclose\n",
                                     check=False)
        rows = parse_transcript(text)
        expect(len(rows) >= 1, "no transcript")
        expect(rows[0].get("rc") in codes(allowed),
               "open rc=%s allowed=%s region=%s spec=%s"
               % (rows[0].get("rc"), allowed, region, spec))
        if rows[0].get("rc") == "0":
            first = int(rows[1]["first"])
            stable = int(rows[1]["stable"])
            end = int(rows[1]["end"])
            expect(first <= stable <= end,
                   "invariant broken: %d %d %d" % (first, stable, end))
    elif check == "read":
        index = target_index(fixture, explicit, region)
        _proc, text = ctx.run_runner(
            "open 6 ledger\nread_crc %d\nclose\n" % index, check=False
        )
        rows = parse_transcript(text)
        expect(rows[0].get("rc") == "0",
               "open failed before read: %s" % rows[0])
        expect(rows[1].get("rc") == str(-8),
               "read rc=%s region=%s spec=%s" % (rows[1].get("rc"), region, spec))
    elif check == "verify":
        _proc, text = ctx.run_runner("open 6 ledger\nverify\nclose\n",
                                     check=False)
        rows = parse_transcript(text)
        expect(rows[0].get("rc") == "0",
               "open failed before verify: %s" % rows[0])
        expect(rows[1].get("rc") == str(-8),
               "verify rc=%s region=%s spec=%s" % (rows[1].get("rc"), region, spec))
    else:
        raise AssertionError("unknown check %s" % check)


def register_corruption_rows():
    for index, fields in enumerate(T.CORRUPT_ROWS):
        name = fields[0]
        if index % 149 == 0:
            tier = "fast"
        elif index % 5 == 0:
            tier = "full"
        else:
            tier = "long"

        def run(ctx, fields=fields):
            check_row(ctx, *fields[1:])

        run.__name__ = name
        case(name, tier=tier)(run)


def check_truncation(ctx, fixture, file_kind, explicit, length, allowed):
    copy_fixture(ctx, fixture)
    path = os.path.join(ctx.ledger_path, file_name(fixture, file_kind, explicit))
    size = os.path.getsize(path)
    if length >= size:
        return
    with open(path, "rb") as handle:
        data = handle.read()
    with open(path, "wb") as handle:
        handle.write(data[:length])
    _proc, text = ctx.run_runner("open 6 ledger\nstate\nclose\n", check=False)
    rows = parse_transcript(text)
    expect(len(rows) >= 1, "no transcript")
    expect(rows[0].get("rc") in codes(allowed),
           "open rc=%s allowed=%s fixture=%s file=%s length=%d"
           % (rows[0].get("rc"), allowed, fixture, file_kind, length))
    if rows[0].get("rc") == "0":
        first = int(rows[1]["first"])
        stable = int(rows[1]["stable"])
        end = int(rows[1]["end"])
        expect(first <= stable <= end, "invariant broken after truncation")


def register_truncation_rows():
    for index, fields in enumerate(T.TRUNCATE_FILE_ROWS):
        name = fields[0]
        tier = "fast" if index % 29 == 0 else "full"

        def run(ctx, fields=fields):
            check_truncation(ctx, *fields[1:])

        run.__name__ = name
        case(name, tier=tier)(run)


@case("corrupt.truncate_to_zero_current", tags=("corrupt",))
def truncate_to_zero_current(ctx):
    copy_fixture(ctx, "two_parts")
    path = os.path.join(ctx.ledger_path, "CURRENT")
    with open(path, "wb"):
        pass
    _proc, text = ctx.run_runner("open 6 ledger\nclose\n", check=False)
    rows = parse_transcript(text)
    expect(rows[0].get("rc") in ("-8", "-9", "-4"),
           "rc=%s" % rows[0].get("rc"))


@case("corrupt.orphan_manifest_ignored", tags=("corrupt",))
def orphan_manifest_ignored(ctx):
    copy_fixture(ctx, "two_parts")
    path = os.path.join(ctx.ledger_path, "MANIFEST.00000000000000ff")
    with open(path, "wb") as handle:
        handle.write(b"garbage")
    _proc, text = ctx.run_runner("open 6 ledger\nstate\nclose\n")
    rows = parse_transcript(text)
    expect(rows[0].get("rc") == "0", "orphan broke open: %s" % rows[0])
    expect(rows[1]["end"] == "7", "state=%s" % rows[1])


@case("corrupt.orphan_part_ignored", tags=("corrupt",))
def orphan_part_ignored(ctx):
    copy_fixture(ctx, "two_parts")
    path = os.path.join(ctx.ledger_path, "part.00000000000000ff")
    with open(path, "wb") as handle:
        handle.write(b"garbage")
    _proc, text = ctx.run_runner("open 6 ledger\nstate\nclose\n")
    rows = parse_transcript(text)
    expect(rows[0].get("rc") == "0", "orphan broke open: %s" % rows[0])
    expect(rows[1]["end"] == "7", "state=%s" % rows[1])


register_corruption_rows()
register_truncation_rows()
