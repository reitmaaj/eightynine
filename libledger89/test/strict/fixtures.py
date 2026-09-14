"""Shared deterministic ledger fixtures for the strict suite.

The two-parts and three-parts fixtures are built once and copied per case.
Their layouts are the subject of tables_format.FIELD_ROWS and the
corruption sweeps.
"""

import os
import shutil

from harness import TMP_ROOT, run_script

TWO_PARTS_SCRIPT = "\n".join([
    "open 14 ledger",
    "appendv 61,62,63",
    "sync",
    "rotate",
    "appendv 64,65,66",
    "sync",
    "close",
])

THREE_PARTS_SCRIPT = "\n".join([
    "open 14 ledger",
    "appendv 61,62,63",
    "sync",
    "rotate",
    "appendv 64,65,66",
    "sync",
    "rotate",
    "appendv 67",
    "sync",
    "close",
])

FIXTURE_DIRS = {
    "two_parts": os.path.join(TMP_ROOT, "_fixture-two-parts"),
    "three_parts": os.path.join(TMP_ROOT, "_fixture-three-parts"),
}

FIXTURE_SCRIPTS = {
    "two_parts": TWO_PARTS_SCRIPT,
    "three_parts": THREE_PARTS_SCRIPT,
}

PART1 = "part.0000000000000001"
PART2 = "part.0000000000000002"
PART3 = "part.0000000000000003"

GOLDEN_FILES = [
    ("golden-current.bin", "CURRENT"),
    ("golden-manifest1.bin", "MANIFEST.0000000000000001"),
    ("golden-manifest2.bin", "MANIFEST.0000000000000002"),
    ("golden-part1.bin", "part.0000000000000001"),
    ("golden-part2.bin", "part.0000000000000002"),
]

GOLDEN_RECORDS = {
    1: b"alpha",
    2: b"\x01\x02\x03",
    3: b"",
    4: b"delta",
    5: b"zz",
}


def build_fixture(name):
    directory = FIXTURE_DIRS[name]
    if not os.path.exists(directory):
        os.makedirs(directory, exist_ok=True)
        run_script(directory, FIXTURE_SCRIPTS[name])
    return directory


def copy_fixture(ctx, name):
    template = build_fixture(name)
    destination = ctx.ledger_path
    if os.path.exists(destination):
        shutil.rmtree(destination)
    shutil.copytree(os.path.join(template, "ledger"), destination)
    return destination


def build_two_parts():
    return build_fixture("two_parts")


def copy_two_parts(ctx):
    return copy_fixture(ctx, "two_parts")


def golden_dir():
    root = os.path.dirname(os.path.abspath(__file__))
    return os.path.join(root, "..", "golden")


def copy_golden(ctx):
    source = golden_dir()
    destination = ctx.ledger_path
    if os.path.exists(destination):
        shutil.rmtree(destination)
    os.makedirs(destination)
    for fixture, name in GOLDEN_FILES:
        shutil.copyfile(os.path.join(source, fixture),
                        os.path.join(destination, name))
    return destination
