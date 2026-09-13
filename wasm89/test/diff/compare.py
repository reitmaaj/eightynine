#!/usr/bin/env python3
"""Shared value model and cross-engine verdict for the differential harness.

A case outcome is normalized to `status` and a list of `(vtype, bits)`.

Statuses (canonical): `ok`, `rt` (runtime trap or exhaustion), `err`
(missing engine, module/config problem, or unparseable oracle output).

Values are `(vtype, bits)` where bits is the raw 32/64-bit pattern for the
scalar numeric type vtype in {i32,i64,f32,f64}. A float NaN result is
carried with its raw bits but compared only as "is NaN" (see values_equal).

Verdict rules (design `.agent/design/0008`):
  ok vs ok          -> PASS iff value vectors are equal
  rt  vs rt         -> PASS (trap/exhaustion are interchangeable)
  ok  vs rt (either) -> FAIL
  err (either side)  -> ERROR
"""

import struct

VTYPES = ("i32", "i64", "f32", "f64")

MASK = {
    "i32": 0xFFFFFFFF,
    "i64": 0xFFFFFFFFFFFFFFFF,
    "f32": 0xFFFFFFFF,
    "f64": 0xFFFFFFFFFFFFFFFF,
}

WIDTH = {"i32": 32, "i64": 64, "f32": 32, "f64": 64}

# canonical quiet positive NaN (payload 0) used only as a placeholder for
# an engine that reports NaN without a payload.
CANONICAL_NAN = {"f32": 0x7FC00000, "f64": 0x7FF8000000000000}


def is_nan(vtype, bits):
    """True when a float bit pattern is any NaN (any payload, any sign)."""
    if vtype == "f32":
        exp = (bits >> 23) & 0xFF
        mant = bits & 0x7FFFFF
        return exp == 0xFF and mant != 0
    if vtype == "f64":
        exp = (bits >> 52) & 0x7FF
        mant = bits & 0xFFFFFFFFFFFFF
        return exp == 0x7FF and mant != 0
    return False


def same_type_width(t):
    return t in ("f32", "f64")


def values_equal(a, b):
    """True iff two scalar values are equal under the exact-bits + NaN rule.

    a, b are (vtype, bits). Numerics compare by raw bits; floats compare by
    raw bits unless either is NaN, in which case they are equal iff both are
    NaN (payload and sign ignored)."""
    if a[0] != b[0]:
        return False
    vtype, abit, bbit = a[0], a[1], b[1]
    if same_type_width(vtype):
        an = is_nan(vtype, abit)
        bn = is_nan(vtype, bbit)
        if an or bn:
            return an == bn
    return abit == bbit


def results_equal(a, b):
    """True iff two ordered value vectors are equal (types, counts, values)."""
    if a is None or b is None:
        return a is b
    if len(a) != len(b):
        return False
    for i, av in enumerate(a):
        if not values_equal(av, b[i]):
            return False
    return True


def normalize_status(status):
    """Map raw oracle statuses to the canonical {ok, rt, err} set."""
    if status in ("ok",):
        return "ok"
    if status in ("trap", "exhaust", "rt"):
        return "rt"
    return "err"


def verdict(w89, oracle):
    """Compare a wasm89 outcome with an oracle outcome.

    w89/oracle are (status, values). Returns (verdict, note) with verdict in
    {'PASS','FAIL','ERROR'} and note describing any mismatch."""
    sa = normalize_status(w89[0])
    sb = normalize_status(oracle[0])
    if sa == "err" or sb == "err":
        return ("ERROR", "status error: wasm89=%s oracle=%s"
                % (w89[0], oracle[0]))
    if sa != sb:
        return ("FAIL", "status mismatch: wasm89=%s oracle=%s"
                % (w89[0], oracle[0]))
    if sa == "rt":
        return ("PASS", "both runtime-failure")
    if results_equal(w89[1], oracle[1]):
        return ("PASS", "ok")
    return ("FAIL", "result mismatch: wasm89=%r oracle=%r"
            % (fmt_results(w89[1]), fmt_results(oracle[1])))


def fmt_results(values):
    if values is None:
        return "<none>"
    return " ".join("%s:%s" % (t, bits_str(t, b)) for (t, b) in values)


def bits_str(vtype, bits):
    if vtype in ("f32", "f64"):
        if is_nan(vtype, bits):
            return "nan"
        try:
            if vtype == "f32":
                return repr(struct.unpack("<f", struct.pack("<I", bits))[0])
            return repr(struct.unpack("<d", struct.pack("<Q", bits))[0])
        except struct.error:
            return "0x%x" % bits
    return str(bits)


def parse_hex_value(token):
    """Parse a 'type:0xbits' token (wasm89 repl / wasm-interp int output).

    Returns (vtype, bits) or None if malformed."""
    if ":" not in token:
        return None
    vtype, _, rest = token.partition(":")
    if vtype not in VTYPES:
        return None
    if rest.startswith("0x") or rest.startswith("0X"):
        try:
            bits = int(rest, 16)
        except ValueError:
            return None
    else:
        try:
            bits = int(rest, 10)
        except ValueError:
            return None
    return (vtype, bits & MASK[vtype])


def float_bits_to_decimal(vtype, bits):
    """Convert float bits to a shortest-decimal string for use as a wasmtime
    argument. For f32 the decimal is chosen to round-trip to that exact f32;
    wasmtime re-parses it to the same f32."""
    if vtype == "f32":
        value = struct.unpack("<f", struct.pack("<I", bits))[0]
    else:
        value = struct.unpack("<d", struct.pack("<Q", bits))[0]
    if value != value:
        return "nan"
    if value == float("inf"):
        return "inf"
    if value == float("-inf"):
        return "-inf"
    return repr(value)


def decimal_to_bits(vtype, token):
    """Decode a wasmtime shortest-decimal result token to exact float bits.

    Returns (vtype, bits) using the canonical quiet NaN placeholder when the
    token denotes a NaN (its payload is not observable through wasmtime)."""
    low = token.lower()
    if "nan" in low:
        return (vtype, CANONICAL_NAN[vtype])
    if low in ("inf", "+inf", "infinity", "+infinity"):
        return (vtype, MASK[vtype] & (0x7F800000 if vtype == "f32"
                                      else 0x7FF0000000000000))
    if low in ("-inf", "-infinity"):
        return (vtype, MASK[vtype] & (0xFF800000 if vtype == "f32"
                                      else 0xFFF0000000000000))
    value = float(token)
    if vtype == "f32":
        return (vtype, struct.unpack("<I", struct.pack("<f", value))[0])
    return (vtype, struct.unpack("<Q", struct.pack("<d", value))[0])


def signed_int_bits_to_decimal(vtype, bits):
    """Encode integer bits as the signed decimal wasmtime expects as input."""
    w = WIDTH[vtype]
    if vtype == "i32":
        signed = bits if bits < 0x80000000 else bits - 0x100000000
    else:
        signed = bits if bits < 0x8000000000000000 else bits - 0x10000000000000000
    return str(signed)
