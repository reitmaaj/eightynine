"""On-disk format tests (ST12-ST14).

The field table is checked against an independent Python decoder, and a
Python-built ledger is opened by the library to prove the encoders agree in
both directions.
"""

import os

import formatlib as fmt
import ledger
from fixtures import (
    GOLDEN_RECORDS,
    PART1,
    PART2,
    copy_golden,
    copy_two_parts,
)
from harness import case, expect, expect_rc
from ledger import ECORRUPT, OK, append, open_ledger, read, verify

import tables_format as T


def _read(path):
    with open(path, "rb") as handle:
        return handle.read()


def _resolve(parsed, file_kind, region):
    directory = parsed["dir"]
    if file_kind == "CURRENT":
        data = _read(os.path.join(directory, "CURRENT"))
        current = parsed["current"]
        return {
            "magic": data[:8],
            "generation": current["generation"],
            "version": fmt.get_u32(data, 16),
            "reserved": fmt.get_u64(data, 24),
        }[region]
    if file_kind == "MANIFEST":
        name = parsed["manifest_name"]
        data = _read(os.path.join(directory, name))
        manifest = parsed["manifest"]
        mapping = {
            "magic": data[:8],
            "generation": manifest["generation"],
            "uuid": manifest["uuid"],
            "revision": manifest["revision"],
            "first": manifest["first"],
            "sealed_count": len(manifest["sealed"]),
            "version": fmt.get_u32(data, 52),
            "reserved": fmt.get_u32(data, 60),
        }
        if region.startswith("sealed["):
            _, _, rest = region.partition("].")
            index = int(region[len("sealed["): region.index("]")])
            field = {"file_id": 0, "first": 1, "end": 2}[rest]
            return manifest["sealed"][index][field]
        if region.startswith("active."):
            field = region.split(".")[1]
            return {"file_id": manifest["active"][0],
                    "first": manifest["active"][1]}[field]
        return mapping[region]
    data = _read(os.path.join(directory, file_kind))
    part = parsed["files"][file_kind]
    if region.startswith("header."):
        field = region.split(".")[1]
        mapping = {
            "magic": data[:8],
            "uuid": part["header"]["uuid"],
            "file_id": part["header"]["file_id"],
            "revision": part["header"]["revision"],
            "first": part["header"]["first"],
            "version": fmt.get_u32(data, 48),
            "reserved": fmt.get_u32(data, 52),
            "reserved2": fmt.get_u32(data, 60),
        }
        return mapping[field]
    if region.startswith("marker["):
        index = int(region[len("marker["): region.index("]")])
        offset, marker = part["markers"][index]
        field = region.split(".")[1]
        if field == "magic":
            return data[offset: offset + 8]
        if field == "trailer":
            return data[offset + 56: offset + 64]
        if field == "version":
            return fmt.get_u32(data, offset + 48)
        return marker[field]
    if region.startswith("batch["):
        index = int(region[len("batch["): region.index("]")])
        batch = part["batches"][index]
        if region.endswith(".first"):
            return batch["first"]
        if region.endswith(".count"):
            return batch["count"]
        if region.endswith(".bytes"):
            return batch["bytes"]
        if ".record[" in region:
            start = region.index("record[") + len("record[")
            record_index = int(region[start: region.index("]", start)])
            return batch["records"][record_index]["length"]
        footer = batch["offset"] + batch["bytes"] - fmt.BATCH_FOOTER_SIZE
        field = region.split(".")[-1]
        mapping = {
            "footer_magic": data[footer: footer + 4],
            "footer_count": fmt.get_u32(data, footer + 4),
            "footer_last": fmt.get_u64(data, footer + 8),
        }
        return mapping[field]
    if region.startswith("sealed."):
        field = region.split(".")[1]
        footer = len(data) - fmt.SEALED_FOOTER_SIZE
        if field == "magic":
            return data[footer: footer + 8]
        return part["footer"][field]
    raise AssertionError("unresolved region %s" % region)


@case("format.field_rows", tags=("format",))
def field_rows(ctx):
    copy_two_parts(ctx)
    parsed = fmt.parse_ledger(ctx.ledger_path)
    expect(parsed["current"] is not None, "CURRENT did not decode")
    expect(parsed["manifest"] is not None, "manifest did not decode")
    for fields in T.FIELD_ROWS:
        name, file_kind, region, expected = fields
        actual = _resolve(parsed, file_kind, region)
        if isinstance(expected, str):
            expected = expected.encode("latin-1")
        expect(actual == expected,
               "%s: %s = %r, expected %r" % (name, region, actual, expected))


def _write_python_ledger(directory, uuid, payloads, flip=None):
    os.makedirs(directory, exist_ok=True)
    with open(os.path.join(directory, "CURRENT"), "wb") as handle:
        handle.write(fmt.encode_current(1))
    with open(os.path.join(directory, "MANIFEST.0000000000000001"), "wb") as handle:
        handle.write(fmt.encode_manifest(1, uuid, 0, 1, [], (7, 1)))
    part = bytearray()
    part += fmt.encode_part_header(uuid, 7, 0, 1)
    part += fmt.encode_marker(uuid, 7, 0, 1)
    part += fmt.encode_batch(1, payloads)
    part += fmt.encode_marker(uuid, 7, 0, 1 + len(payloads))
    if flip is not None:
        part[flip] ^= 0x01
    with open(os.path.join(directory, "part.0000000000000007"), "wb") as handle:
        handle.write(bytes(part))


@case("format.encoder_roundtrip", tags=("format",))
def encoder_roundtrip(ctx):
    uuid = bytes(range(16))
    directory = ctx.ledger_path
    _write_python_ledger(directory, uuid, [b"one", b"two"])
    rc, handle = open_ledger(directory, ledger.OPEN_RDWR)
    expect_rc(rc, OK, "open python ledger")
    rc, state = ledger.get_state(handle)
    expect_rc(rc, OK, "get_state")
    expect(state.first.value() == 1, "first=%r" % state.first)
    expect(state.stable_end.value() == 3, "stable=%r" % state.stable_end)
    expect(state.end.value() == 3, "end=%r" % state.end)
    for index, payload in enumerate([b"one", b"two"], start=1):
        rc, size, data = read(handle, index, 8)
        expect_rc(rc, OK, "read(%d)" % index)
        expect(data == payload, "payload %r" % data)
    expect_rc(verify(handle), OK, "verify")
    ledger.close(handle)


@case("format.encoder_payload_corruption", tags=("format",))
def encoder_payload_corruption(ctx):
    uuid = bytes(range(16))
    directory = ctx.ledger_path
    payload_offset = fmt.PART_HEADER_SIZE + fmt.MARKER_SIZE + fmt.BATCH_HEADER_SIZE + 4
    _write_python_ledger(directory, uuid, [b"one", b"two"], flip=payload_offset)
    rc, handle = open_ledger(directory, ledger.OPEN_RDWR)
    expect_rc(rc, OK, "open corrupted python ledger")
    rc, size, data = read(handle, 1, 8)
    expect_rc(rc, ECORRUPT, "read corrupted payload")
    expect_rc(verify(handle), ECORRUPT, "verify corrupted payload")
    ledger.close(handle)


@case("format.golden_records", tags=("format",))
def golden_records(ctx):
    copy_golden(ctx)
    parsed = fmt.parse_ledger(ctx.ledger_path)
    records = fmt.read_records(parsed)
    for index, payload in GOLDEN_RECORDS.items():
        expect(records.get(index) == payload,
               "golden record %d = %r" % (index, records.get(index)))
    rc, handle = open_ledger(ctx.ledger_path, ledger.OPEN_RDWR)
    expect_rc(rc, OK, "open golden")
    for index, payload in GOLDEN_RECORDS.items():
        rc, size, data = read(handle, index, max(len(payload), 1))
        expect_rc(rc, OK, "golden read %d" % index)
        expect(data == payload, "golden payload %d" % index)
    expect_rc(verify(handle), OK, "golden verify")
    ledger.close(handle)
