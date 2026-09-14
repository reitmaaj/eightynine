"""Independent Python codec for the libledger89 v2 on-disk format.

Implements the byte layouts from .agent/design/0001-format-v2.md without
calling the library, so format tests compare two independent implementations.
Also locates every field region inside a real ledger directory for the
corruption sweeps.
"""

import os
import struct

from crclib import crc32c

CURRENT_MAGIC = b"LD89CUR2"
MANIFEST_MAGIC = b"LD89MAN2"
PART_MAGIC = b"LD89PRT2"
BATCH_HEADER_MAGIC = b"B89\x02"
BATCH_FOOTER_MAGIC = b"b89\x02"
MARKER_MAGIC = b"LD89STB2"
MARKER_TRAILER = b"89STABLE"
SEALED_FOOTER_MAGIC = b"LD89SGF2"
FORMAT_VERSION = 2

CURRENT_SIZE = 32
MANIFEST_HEADER_SIZE = 64
SEALED_DESC_SIZE = 24
ACTIVE_DESC_SIZE = 16
MANIFEST_CRC_SIZE = 4
PART_HEADER_SIZE = 64
BATCH_HEADER_SIZE = 32
BATCH_FOOTER_SIZE = 24
RECORD_LENGTH_SIZE = 4
RECORD_CRC_SIZE = 4
MARKER_SIZE = 64
SEALED_FOOTER_SIZE = 64


def get_u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def get_u64(data, offset):
    return struct.unpack_from("<Q", data, offset)[0]


def put_u32(value):
    return struct.pack("<I", value & 0xFFFFFFFF)


def put_u64(value):
    return struct.pack("<Q", value & 0xFFFFFFFFFFFFFFFF)


def encode_current(generation):
    body = CURRENT_MAGIC + put_u64(generation) + put_u32(FORMAT_VERSION)
    return body + put_u32(crc32c(body)) + b"\x00" * 8


def decode_current(data):
    if len(data) != CURRENT_SIZE:
        return None, "size"
    if data[:8] != CURRENT_MAGIC:
        return None, "magic"
    if get_u32(data, 16) != FORMAT_VERSION:
        return None, "version"
    if get_u32(data, 20) != crc32c(data[:20]):
        return None, "crc"
    return {"generation": get_u64(data, 8)}, None


def encode_manifest(generation, uuid, revision, first, sealed, active):
    header = bytearray(MANIFEST_HEADER_SIZE)
    header[0:8] = MANIFEST_MAGIC
    header[8:16] = put_u64(generation)
    header[16:32] = uuid
    header[32:40] = put_u64(revision)
    header[40:48] = put_u64(first)
    header[48:52] = put_u32(len(sealed))
    header[52:56] = put_u32(FORMAT_VERSION)
    header[56:60] = put_u32(crc32c(bytes(header[:56])))
    out = bytearray(header)
    for desc in sealed:
        out += put_u64(desc[0]) + put_u64(desc[1]) + put_u64(desc[2])
    out += put_u64(active[0]) + put_u64(active[1])
    out += put_u32(crc32c(bytes(out)))
    return bytes(out)


def decode_manifest(data):
    if len(data) < MANIFEST_HEADER_SIZE + ACTIVE_DESC_SIZE + MANIFEST_CRC_SIZE:
        return None, "size"
    if data[:8] != MANIFEST_MAGIC:
        return None, "magic"
    if get_u32(data, 52) != FORMAT_VERSION:
        return None, "version"
    if get_u32(data, 56) != crc32c(data[:56]):
        return None, "header_crc"
    count = get_u32(data, 48)
    expect = (
        MANIFEST_HEADER_SIZE + count * SEALED_DESC_SIZE + ACTIVE_DESC_SIZE
        + MANIFEST_CRC_SIZE
    )
    if len(data) != expect:
        return None, "count_size"
    if get_u32(data, len(data) - 4) != crc32c(data[: len(data) - 4]):
        return None, "crc"
    manifest = {
        "generation": get_u64(data, 8),
        "uuid": bytes(data[16:32]),
        "revision": get_u64(data, 32),
        "first": get_u64(data, 40),
        "sealed": [],
    }
    offset = MANIFEST_HEADER_SIZE
    previous = manifest["first"]
    for _ in range(count):
        file_id = get_u64(data, offset)
        start = get_u64(data, offset + 8)
        end = get_u64(data, offset + 16)
        if start != previous or end <= start:
            return None, "descriptor"
        previous = end
        manifest["sealed"].append((file_id, start, end))
        offset += SEALED_DESC_SIZE
    manifest["active"] = (get_u64(data, offset), get_u64(data, offset + 8))
    if manifest["active"][1] != previous:
        return None, "active"
    return manifest, None


def encode_part_header(uuid, file_id, revision, first):
    header = bytearray(PART_HEADER_SIZE)
    header[0:8] = PART_MAGIC
    header[8:24] = uuid
    header[24:32] = put_u64(file_id)
    header[32:40] = put_u64(revision)
    header[40:48] = put_u64(first)
    header[48:52] = put_u32(FORMAT_VERSION)
    header[56:60] = put_u32(crc32c(bytes(header[:56])))
    return bytes(header)


def decode_part_header(data):
    if len(data) < PART_HEADER_SIZE:
        return None, "size"
    if data[:8] != PART_MAGIC:
        return None, "magic"
    if get_u32(data, 48) != FORMAT_VERSION:
        return None, "version"
    if get_u32(data, 56) != crc32c(data[:56]):
        return None, "crc"
    return {
        "uuid": bytes(data[8:24]),
        "file_id": get_u64(data, 24),
        "revision": get_u64(data, 32),
        "first": get_u64(data, 40),
    }, None


def encode_marker(uuid, file_id, revision, end):
    marker = bytearray(MARKER_SIZE)
    marker[0:8] = MARKER_MAGIC
    marker[8:24] = uuid
    marker[24:32] = put_u64(file_id)
    marker[32:40] = put_u64(revision)
    marker[40:48] = put_u64(end)
    marker[48:52] = put_u32(FORMAT_VERSION)
    marker[52:56] = put_u32(crc32c(bytes(marker[:52])))
    marker[56:64] = MARKER_TRAILER
    return bytes(marker)


def decode_marker(data):
    if len(data) < MARKER_SIZE:
        return None, "size"
    if data[:8] != MARKER_MAGIC:
        return None, "magic"
    if get_u32(data, 48) != FORMAT_VERSION:
        return None, "version"
    if get_u32(data, 52) != crc32c(data[:52]):
        return None, "crc"
    return {
        "uuid": bytes(data[8:24]),
        "file_id": get_u64(data, 24),
        "revision": get_u64(data, 32),
        "end": get_u64(data, 40),
    }, None


def encode_batch(first, payloads):
    records = b""
    for payload in payloads:
        length = put_u32(len(payload))
        record_crc = crc32c(length + payload)
        records += length + payload + put_u32(record_crc)
    count = len(payloads)
    total = BATCH_HEADER_SIZE + len(records) + BATCH_FOOTER_SIZE
    header = bytearray(BATCH_HEADER_SIZE)
    header[0:4] = BATCH_HEADER_MAGIC
    header[4:8] = put_u32(count)
    header[8:16] = put_u64(first)
    header[16:24] = put_u64(total)
    header[24:28] = put_u32(crc32c(bytes(header[:24])))
    footer = bytearray(BATCH_FOOTER_SIZE)
    footer[0:4] = BATCH_FOOTER_MAGIC
    footer[4:8] = put_u32(count)
    footer[8:16] = put_u64(first + count - 1)
    batch = bytes(header) + records + bytes(footer)
    body_crc = crc32c(batch[: BATCH_HEADER_SIZE + len(records) + 16])
    footer[16:20] = put_u32(body_crc)
    return bytes(header) + records + bytes(footer)


def decode_batch(data, offset):
    if offset + BATCH_HEADER_SIZE > len(data):
        return None, "size"
    if data[offset : offset + 4] != BATCH_HEADER_MAGIC:
        return None, "header_magic"
    if get_u32(data, offset + 24) != crc32c(data[offset : offset + 24]):
        return None, "header_crc"
    count = get_u32(data, offset + 4)
    first = get_u64(data, offset + 8)
    total = get_u64(data, offset + 16)
    if count == 0:
        return None, "count"
    if total < BATCH_HEADER_SIZE + BATCH_FOOTER_SIZE:
        return None, "bytes"
    if offset + total > len(data):
        return None, "truncated"
    end = offset + total
    footer_offset = end - BATCH_FOOTER_SIZE
    if data[footer_offset : footer_offset + 4] != BATCH_FOOTER_MAGIC:
        return None, "footer_magic"
    records = []
    position = offset + BATCH_HEADER_SIZE
    for _ in range(count):
        if position + RECORD_LENGTH_SIZE > footer_offset:
            return None, "record_truncated"
        length = get_u32(data, position)
        if position + RECORD_LENGTH_SIZE + length + RECORD_CRC_SIZE > footer_offset:
            return None, "record_truncated"
        payload = bytes(
            data[position + RECORD_LENGTH_SIZE : position + RECORD_LENGTH_SIZE + length]
        )
        record_crc = get_u32(
            data, position + RECORD_LENGTH_SIZE + length
        )
        ok = record_crc == crc32c(
            data[position : position + RECORD_LENGTH_SIZE + length]
        )
        records.append(
            {
                "offset": position,
                "length": length,
                "payload": payload,
                "crc_ok": ok,
            }
        )
        position += RECORD_LENGTH_SIZE + length + RECORD_CRC_SIZE
    footer_count = get_u32(data, footer_offset + 4)
    footer_last = get_u64(data, footer_offset + 8)
    batch_crc = get_u32(data, footer_offset + 16)
    crc_ok = batch_crc == crc32c(data[offset : footer_offset + 16])
    if footer_count != count:
        return None, "footer_count"
    if footer_last != first + count - 1:
        return None, "footer_last"
    return {
        "offset": offset,
        "count": count,
        "first": first,
        "bytes": total,
        "records": records,
        "crc_ok": crc_ok,
        "payload_ok": all(record["crc_ok"] for record in records),
    }, None


def encode_sealed_footer(uuid, file_id, first, end, records, digest=0):
    footer = bytearray(SEALED_FOOTER_SIZE)
    footer[0:8] = SEALED_FOOTER_MAGIC
    footer[8:24] = uuid
    footer[24:32] = put_u64(file_id)
    footer[32:40] = put_u64(first)
    footer[40:48] = put_u64(end)
    footer[48:56] = put_u64(records)
    footer[56:60] = put_u32(digest)
    footer[60:64] = put_u32(crc32c(bytes(footer[:60])))
    return bytes(footer)


def decode_sealed_footer(data, offset):
    if offset + SEALED_FOOTER_SIZE > len(data):
        return None, "size"
    if data[offset : offset + 8] != SEALED_FOOTER_MAGIC:
        return None, "magic"
    if get_u32(data, offset + 60) != crc32c(data[offset : offset + 60]):
        return None, "crc"
    return {
        "uuid": bytes(data[offset + 8 : offset + 24]),
        "file_id": get_u64(data, offset + 24),
        "first": get_u64(data, offset + 32),
        "end": get_u64(data, offset + 40),
        "records": get_u64(data, offset + 48),
        "digest": get_u32(data, offset + 56),
    }, None


def parse_part(data):
    """Walk a part file: header, markers, batches, optional sealed footer."""
    part = {"header": None, "markers": [], "batches": [], "footer": None}
    header, error = decode_part_header(data)
    if header is None:
        part["error"] = "header:" + error
        return part
    part["header"] = header
    offset = PART_HEADER_SIZE
    while offset < len(data):
        chunk = data[offset:]
        if chunk[:4] == BATCH_HEADER_MAGIC:
            batch, error = decode_batch(data, offset)
            if batch is None:
                part["error"] = "batch:" + error
                break
            part["batches"].append(batch)
            offset += batch["bytes"]
            continue
        if chunk[:8] == MARKER_MAGIC:
            marker, error = decode_marker(chunk)
            if marker is None:
                part["error"] = "marker:" + error
                break
            part["markers"].append((offset, marker))
            offset += MARKER_SIZE
            continue
        break
    if len(data) >= SEALED_FOOTER_SIZE:
        footer, error = decode_sealed_footer(data, len(data) - SEALED_FOOTER_SIZE)
        if footer is not None:
            part["footer"] = footer
    return part


def parse_ledger(directory):
    """Parse a ledger directory into its structures."""
    parsed = {"dir": directory, "files": {}, "current": None, "manifest": None}
    names = sorted(os.listdir(directory))
    parsed["names"] = names
    if "CURRENT" in names:
        with open(os.path.join(directory, "CURRENT"), "rb") as handle:
            current, error = decode_current(handle.read())
        parsed["current"] = current
        parsed["current_error"] = error
    if parsed["current"] is not None:
        name = "MANIFEST.%016x" % parsed["current"]["generation"]
        parsed["manifest_name"] = name
        if name in names:
            with open(os.path.join(directory, name), "rb") as handle:
                manifest, error = decode_manifest(handle.read())
            parsed["manifest"] = manifest
            parsed["manifest_error"] = error
    for name in names:
        if name.startswith("part."):
            with open(os.path.join(directory, name), "rb") as handle:
                parsed["files"][name] = parse_part(handle.read())
    return parsed


def field_regions(parsed):
    """Return [(filename, region, offset, size)] for every v2 field."""
    regions = []
    directory = parsed["dir"]
    names = parsed["files"]
    if "CURRENT" in names:
        data = read_bytes(directory, "CURRENT")
        regions.append(("CURRENT", "magic", 0, 8))
        regions.append(("CURRENT", "generation", 8, 8))
        regions.append(("CURRENT", "version", 16, 4))
        regions.append(("CURRENT", "crc", 20, 4))
        regions.append(("CURRENT", "reserved", 24, 8))
        del data
    manifest_name = parsed.get("manifest_name")
    if manifest_name and manifest_name in names:
        data = read_bytes(directory, manifest_name)
        regions.append((manifest_name, "magic", 0, 8))
        regions.append((manifest_name, "generation", 8, 8))
        regions.append((manifest_name, "uuid", 16, 16))
        regions.append((manifest_name, "revision", 32, 8))
        regions.append((manifest_name, "first", 40, 8))
        regions.append((manifest_name, "sealed_count", 48, 4))
        regions.append((manifest_name, "version", 52, 4))
        regions.append((manifest_name, "header_crc", 56, 4))
        regions.append((manifest_name, "reserved", 60, 4))
        manifest = parsed.get("manifest")
        offset = MANIFEST_HEADER_SIZE
        if manifest is not None:
            for index in range(len(manifest["sealed"])):
                regions.append(
                    (manifest_name, "sealed[%d]" % index, offset, SEALED_DESC_SIZE)
                )
                offset += SEALED_DESC_SIZE
            regions.append((manifest_name, "active", offset, ACTIVE_DESC_SIZE))
            offset += ACTIVE_DESC_SIZE
        regions.append((manifest_name, "crc", len(data) - 4, 4))
        del data
    for name, part in parsed["files"].items():
        if not name.startswith("part.") or not isinstance(part, dict):
            continue
        data = read_bytes(directory, name)
        regions.append((name, "header.magic", 0, 8))
        regions.append((name, "header.uuid", 8, 16))
        regions.append((name, "header.file_id", 24, 8))
        regions.append((name, "header.revision", 32, 8))
        regions.append((name, "header.first", 40, 8))
        regions.append((name, "header.version", 48, 4))
        regions.append((name, "header.reserved", 52, 4))
        regions.append((name, "header.crc", 56, 4))
        regions.append((name, "header.reserved2", 60, 4))
        for index, (offset, _marker) in enumerate(part["markers"]):
            regions.append((name, "marker[%d].magic" % index, offset, 8))
            regions.append((name, "marker[%d].uuid" % index, offset + 8, 16))
            regions.append((name, "marker[%d].file_id" % index, offset + 24, 8))
            regions.append((name, "marker[%d].revision" % index, offset + 32, 8))
            regions.append((name, "marker[%d].end" % index, offset + 40, 8))
            regions.append((name, "marker[%d].version" % index, offset + 48, 4))
            regions.append((name, "marker[%d].crc" % index, offset + 52, 4))
            regions.append((name, "marker[%d].trailer" % index, offset + 56, 8))
        for index, batch in enumerate(part["batches"]):
            base = batch["offset"]
            regions.append((name, "batch[%d].magic" % index, base, 4))
            regions.append((name, "batch[%d].count" % index, base + 4, 4))
            regions.append((name, "batch[%d].first" % index, base + 8, 8))
            regions.append((name, "batch[%d].bytes" % index, base + 16, 8))
            regions.append((name, "batch[%d].header_crc" % index, base + 24, 4))
            regions.append((name, "batch[%d].reserved" % index, base + 28, 4))
            for record_index, record in enumerate(batch["records"]):
                record_base = record["offset"]
                regions.append(
                    (name, "batch[%d].record[%d].length" % (index, record_index),
                     record_base, 4)
                )
                regions.append(
                    (name, "batch[%d].record[%d].payload" % (index, record_index),
                     record_base + 4, record["length"])
                )
                regions.append(
                    (name, "batch[%d].record[%d].crc" % (index, record_index),
                     record_base + 4 + record["length"], 4)
                )
            footer = base + batch["bytes"] - BATCH_FOOTER_SIZE
            regions.append((name, "batch[%d].footer_magic" % index, footer, 4))
            regions.append((name, "batch[%d].footer_count" % index, footer + 4, 4))
            regions.append((name, "batch[%d].footer_last" % index, footer + 8, 8))
            regions.append((name, "batch[%d].footer_crc" % index, footer + 16, 4))
            regions.append((name, "batch[%d].footer_reserved" % index, footer + 20, 4))
        if part.get("footer") is not None:
            base = len(data) - SEALED_FOOTER_SIZE
            regions.append((name, "sealed.magic", base, 8))
            regions.append((name, "sealed.uuid", base + 8, 16))
            regions.append((name, "sealed.file_id", base + 24, 8))
            regions.append((name, "sealed.first", base + 32, 8))
            regions.append((name, "sealed.end", base + 40, 8))
            regions.append((name, "sealed.records", base + 48, 8))
            regions.append((name, "sealed.digest", base + 56, 4))
            regions.append((name, "sealed.crc", base + 60, 4))
        del data
    return regions


def read_bytes(directory, name):
    with open(os.path.join(directory, name), "rb") as handle:
        return handle.read()


def read_records(parsed):
    """Return {index: payload} for all framed records in all parts."""
    records = {}
    for name, part in parsed["files"].items():
        if not name.startswith("part.") or not isinstance(part, dict):
            continue
        for batch in part["batches"]:
            for position, record in enumerate(batch["records"]):
                records[batch["first"] + position] = record["payload"]
    return records
