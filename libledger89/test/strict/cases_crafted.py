"""Crafted structural mismatches (ST17, ST18).

Python-built ledgers with one semantically invalid field each: uuid and
file_id mismatches, revision and end mismatches, non-contiguous
descriptors, and framing lies. These complement the random byte sweeps by
targeting the recovery validator's individual rejection branches.
"""

import os

import formatlib as fmt
from crclib import crc32c
from harness import case, expect, parse_transcript

UUID = bytes(range(16))
UUID2 = bytes(reversed(range(16)))
PART1 = "part.0000000000000001"
PART2 = "part.0000000000000002"
MANIFEST = "MANIFEST.0000000000000002"


def batch_header(first, count, total):
    header = bytearray(32)
    header[0:4] = fmt.BATCH_HEADER_MAGIC
    header[4:8] = fmt.put_u32(count)
    header[8:16] = fmt.put_u64(first)
    header[16:24] = fmt.put_u64(total)
    header[24:28] = fmt.put_u32(crc32c(bytes(header[:24])))
    return bytes(header)


def current(generation, version=2):
    body = fmt.CURRENT_MAGIC + fmt.put_u64(generation) + fmt.put_u32(version)
    return body + fmt.put_u32(crc32c(body)) + b"\x00" * 8


def part_header(uuid, file_id, revision, first, version=2):
    header = bytearray(64)
    header[0:8] = fmt.PART_MAGIC
    header[8:24] = uuid
    header[24:32] = fmt.put_u64(file_id)
    header[32:40] = fmt.put_u64(revision)
    header[40:48] = fmt.put_u64(first)
    header[48:52] = fmt.put_u32(version)
    header[56:60] = fmt.put_u32(crc32c(bytes(header[:56])))
    return bytes(header)


def marker(uuid, file_id, revision, end, version=2):
    out = bytearray(64)
    out[0:8] = fmt.MARKER_MAGIC
    out[8:24] = uuid
    out[24:32] = fmt.put_u64(file_id)
    out[32:40] = fmt.put_u64(revision)
    out[40:48] = fmt.put_u64(end)
    out[48:52] = fmt.put_u32(version)
    out[52:56] = fmt.put_u32(crc32c(bytes(out[:52])))
    out[56:64] = fmt.MARKER_TRAILER
    return bytes(out)


def base_files():
    part1 = (
        fmt.encode_part_header(UUID, 1, 0, 1)
        + fmt.encode_marker(UUID, 1, 0, 1)
        + fmt.encode_batch(1, [b"a", b"b", b"c"])
        + fmt.encode_marker(UUID, 1, 0, 4)
        + fmt.encode_sealed_footer(UUID, 1, 1, 4, 3)
    )
    part2 = (
        fmt.encode_part_header(UUID, 2, 0, 4)
        + fmt.encode_marker(UUID, 2, 0, 4)
        + fmt.encode_batch(4, [b"d", b"e", b"f"])
        + fmt.encode_marker(UUID, 2, 0, 7)
    )
    return {
        "CURRENT": current(2),
        MANIFEST: fmt.encode_manifest(2, UUID, 0, 1, [(1, 1, 4)], (2, 4)),
        PART1: part1,
        PART2: part2,
    }


def splice(data, offset, size, replacement):
    return data[:offset] + replacement + data[offset + size:]


def part1_header(**kwargs):
    fields = {"uuid": UUID, "file_id": 1, "revision": 0, "first": 1}
    fields.update(kwargs)
    return part_header(fields["uuid"], fields["file_id"], fields["revision"],
                       fields["first"], fields.get("version", 2))


def part2_marker(index, **kwargs):
    fields = {"uuid": UUID, "file_id": 2, "revision": 0, "end": 4}
    fields.update(kwargs)
    return marker(fields["uuid"], fields["file_id"], fields["revision"],
                  fields["end"], fields.get("version", 2))


def transform_part1_header(files, **kwargs):
    files[PART1] = splice(files[PART1], 0, 64, part1_header(**kwargs))


def transform_part1_baseline(files, **kwargs):
    fields = {"uuid": UUID, "file_id": 1, "revision": 0, "end": 1}
    fields.update(kwargs)
    replacement = marker(fields["uuid"], fields["file_id"],
                         fields["revision"], fields["end"])
    files[PART1] = splice(files[PART1], 64, 64, replacement)


def transform_part2_baseline(files, **kwargs):
    files[PART2] = splice(files[PART2], 64, 64, part2_marker(0, **kwargs))


def transform_part2_sync_marker(files, **kwargs):
    files[PART2] = splice(files[PART2], 211, 64, part2_marker(1, **kwargs))


def transform_sealed_footer(files, **kwargs):
    fields = {"uuid": UUID, "file_id": 1, "first": 1, "end": 4, "records": 3,
              "digest": 0}
    fields.update(kwargs)
    replacement = fmt.encode_sealed_footer(
        fields["uuid"], fields["file_id"], fields["first"], fields["end"],
        fields["records"], fields["digest"])
    files[PART1] = splice(files[PART1], 275, 64, replacement)


def transform_manifest(files, **kwargs):
    fields = {"generation": 2, "uuid": UUID, "revision": 0, "first": 1,
              "sealed": [(1, 1, 4)], "active": (2, 4)}
    fields.update(kwargs)
    files[MANIFEST] = fmt.encode_manifest(
        fields["generation"], fields["uuid"], fields["revision"],
        fields["first"], fields["sealed"], fields["active"])


def transform_part2_batch(files, first=4, count=3, total=83):
    header = batch_header(first, count, total)
    files[PART2] = files[PART2][:128] + header + files[PART2][160:]


def transform_part2_footer(files, last=6):
    footer = 128 + 83 - 24
    data = bytearray(files[PART2])
    data[footer + 8: footer + 16] = fmt.put_u64(last)
    files[PART2] = bytes(data)


def transform_part2_record_crc(files):
    data = bytearray(files[PART2])
    data[128 + 32 + 4] ^= 0x01
    files[PART2] = bytes(data)


def transform_part2_sync_trailer(files):
    data = bytearray(files[PART2])
    data[211 + 56] ^= 0x01
    files[PART2] = bytes(data)


def transform_current(files, generation=2, version=2):
    files["CURRENT"] = current(generation, version)


CRAFTED = [
    ("crafted.part1_header_uuid", lambda f: transform_part1_header(f, uuid=UUID2), ("ECORRUPT",)),
    ("crafted.part1_header_file_id", lambda f: transform_part1_header(f, file_id=9), ("ECORRUPT",)),
    ("crafted.part1_header_revision", lambda f: transform_part1_header(f, revision=1), ("OK",)),
    ("crafted.part1_header_first", lambda f: transform_part1_header(f, first=2), ("ECORRUPT",)),
    ("crafted.part1_header_version", lambda f: transform_part1_header(f, version=3), ("ECORRUPT",)),
    ("crafted.part1_baseline_uuid", lambda f: transform_part1_baseline(f, uuid=UUID2), ("ECORRUPT",)),
    ("crafted.part1_baseline_file_id", lambda f: transform_part1_baseline(f, file_id=9), ("ECORRUPT",)),
    ("crafted.part1_baseline_end", lambda f: transform_part1_baseline(f, end=2), ("ECORRUPT",)),
    ("crafted.part2_baseline_uuid", lambda f: transform_part2_baseline(f, uuid=UUID2), ("ECORRUPT",)),
    ("crafted.part2_baseline_file_id", lambda f: transform_part2_baseline(f, file_id=9), ("ECORRUPT",)),
    ("crafted.part2_baseline_end", lambda f: transform_part2_baseline(f, end=5), ("ECORRUPT",)),
    ("crafted.part2_baseline_version", lambda f: transform_part2_baseline(f, version=3), ("ECORRUPT",)),
    ("crafted.part2_sync_uuid", lambda f: transform_part2_sync_marker(f, uuid=UUID2), ("OK", "ECORRUPT")),
    ("crafted.part2_sync_file_id", lambda f: transform_part2_sync_marker(f, file_id=9), ("OK", "ECORRUPT")),
    ("crafted.part2_sync_revision", lambda f: transform_part2_sync_marker(f, revision=1), ("OK", "ECORRUPT")),
    ("crafted.part2_sync_end", lambda f: transform_part2_sync_marker(f, end=8), ("ECORRUPT",)),
    ("crafted.part2_sync_trailer", transform_part2_sync_trailer, ("OK",)),
    ("crafted.sealed_uuid", lambda f: transform_sealed_footer(f, uuid=UUID2), ("ECORRUPT",)),
    ("crafted.sealed_file_id", lambda f: transform_sealed_footer(f, file_id=9), ("ECORRUPT",)),
    ("crafted.sealed_first", lambda f: transform_sealed_footer(f, first=2), ("ECORRUPT",)),
    ("crafted.sealed_end", lambda f: transform_sealed_footer(f, end=5), ("ECORRUPT",)),
    ("crafted.sealed_records", lambda f: transform_sealed_footer(f, records=9), ("ECORRUPT",)),
    ("crafted.manifest_uuid", lambda f: transform_manifest(f, uuid=UUID2), ("ECORRUPT",)),
    ("crafted.manifest_generation", lambda f: transform_manifest(f, generation=3), ("ECORRUPT",)),
    ("crafted.manifest_first", lambda f: transform_manifest(f, first=2, sealed=[(1, 2, 4)], active=(2, 4)), ("ECORRUPT",)),
    ("crafted.manifest_active", lambda f: transform_manifest(f, active=(2, 5)), ("ECORRUPT",)),
    ("crafted.manifest_descriptor_gap", lambda f: transform_manifest(f, sealed=[(1, 1, 3)], active=(2, 4)), ("ECORRUPT",)),
    ("crafted.batch_first", lambda f: transform_part2_batch(f, first=5), ("ECORRUPT",)),
    ("crafted.batch_total_lie", lambda f: transform_part2_batch(f, total=10000), ("ECORRUPT",)),
    ("crafted.batch_count_zero", lambda f: transform_part2_batch(f, count=0, total=56), ("ECORRUPT",)),
    ("crafted.batch_footer_last", lambda f: transform_part2_footer(f, last=9), ("ECORRUPT",)),
    ("crafted.current_generation_missing", lambda f: transform_current(f, generation=9), ("ECORRUPT",)),
    ("crafted.current_version", lambda f: transform_current(f, version=3), ("EFORMAT",)),
]


def write_files(ctx, files):
    directory = ctx.ledger_path
    os.makedirs(directory, exist_ok=True)
    for name, data in files.items():
        with open(os.path.join(directory, name), "wb") as handle:
            handle.write(data)


def open_rc(ctx):
    _proc, text = ctx.run_runner("open 6 ledger\nclose\n", check=False)
    rows = parse_transcript(text)
    return rows[0].get("rc") if rows else None


@case("crafted.base_ledger_opens", tags=("crafted",))
def base_ledger_opens(ctx):
    write_files(ctx, base_files())
    expect(open_rc(ctx) == "0", "python-built base ledger did not open")


def register_crafted():
    for name, transform, allowed in CRAFTED:
        def run(ctx, transform=transform, allowed=allowed):
            files = base_files()
            transform(files)
            write_files(ctx, files)
            rc = open_rc(ctx)
            expected = set()
            for label in allowed:
                expected.add(str({"OK": 0, "ECORRUPT": -8,
                                  "EFORMAT": -9}[label]))
            expect(rc in expected,
                   "%s: rc=%s allowed=%s" % (ctx.name, rc, allowed))

        run.__name__ = name
        case(name, tier="fast")(run)


@case("crafted.record_crc_read", tags=("crafted",))
def record_crc_read(ctx):
    files = base_files()
    transform_part2_record_crc(files)
    write_files(ctx, files)
    _proc, text = ctx.run_runner("open 6 ledger\nread_crc 4\nverify\nclose\n",
                                 check=False)
    rows = parse_transcript(text)
    expect(rows[0].get("rc") == "0", "open after record crc flip")
    expect(rows[1].get("rc") == "-8", "read corrupted record")
    expect(rows[2].get("rc") == "-8", "verify corrupted record")


@case("crafted.sealed_digest_verify", tags=("crafted",))
def sealed_digest_verify(ctx):
    files = base_files()
    transform_sealed_footer(files, digest=0x12345678)
    write_files(ctx, files)
    _proc, text = ctx.run_runner("open 6 ledger\nverify\nclose\n", check=False)
    rows = parse_transcript(text)
    expect(rows[0].get("rc") == "0", "open after digest change")
    expect(rows[1].get("rc") == "-8", "verify after digest change")


register_crafted()
