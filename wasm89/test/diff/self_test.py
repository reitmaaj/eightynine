#!/usr/bin/env python3
"""Self-tests for the differential harness (`.agent/testing/0015`).

Verifies the comparator can agree, can disagree, applies the NaN rule,
treats trap/exhaustion as interchangeable, decodes wasmtime decimal
results to exact bits, and refuses to run when an engine is missing.

Exit 0 only if every self-test passes; otherwise 1 with a message.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import common  # noqa: E402
import compare  # noqa: E402
import gen_modules as gen  # noqa: E402
import oracle_wasm89 as w89  # noqa: E402
import oracle_engines as oe  # noqa: E402

FIXTURE = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       "fixtures", "f.wasm")

_failures = []


def check(cond, msg):
    if cond:
        print("PASS: " + msg, flush=True)
    else:
        print("FAIL: " + msg, flush=True)
        _failures.append(msg)


def outcome(status, values):
    return (status, values)


def main():
    wbin = w89.discover()
    tbin = oe.wasmtime_discover()
    ibin = oe.wasm_interp_discover()

    # ---- DIFF-001 agreement on an integer case (all three engines) ----
    params = [("i32", 20), ("i32", 22)]
    a = w89.invoke(wbin, FIXTURE, "iadd", params, ["i32"], 30)
    b = oe.wasmtime_invoke(tbin, FIXTURE, "iadd", params, ["i32"], 30)
    c = oe.wasm_interp_invoke(ibin, FIXTURE, "iadd", params, ["i32"], 30)
    check(a[0] == "ok" and a[1] == [("i32", 42)], "wasm89 iadd -> 42")
    check(compare.verdict(a[:2], b[:2])[0] == "PASS",
          "wasm89/wasmtime iadd agree")
    check(compare.verdict(a[:2], c[:2])[0] == "PASS",
          "wasm89/wasm-interp iadd agree")

    # ---- DIFF-011 wasmtime decimal -> exact bits ----
    check(compare.decimal_to_bits("f32", "0.1")[1] == 0x3DCCCCCD,
          "f32 0.1 -> 0x3dcccccd (pack-unpack roundtrip)")
    check(compare.decimal_to_bits("f64", "42")[1] == 0x4045000000000000,
          "f64 42 -> exact bits")
    # inf and nan decode
    inf32 = compare.decimal_to_bits("f32", "inf")[1]
    check(inf32 == 0x7F800000, "f32 inf -> 0x7f800000")
    nan64 = compare.decimal_to_bits("f64", "NaN")[1]
    check(compare.is_nan("f64", nan64), "f64 NaN decodes to a NaN")

    # ---- float strict agreement via wasmtime (interpret excluded) ----
    # fsum(5.0f, 2.5f) = 7.5f == 0x40f00000
    fp = [("f32", 0x40A00000), ("f32", 0x40200000)]
    a = w89.invoke(wbin, FIXTURE, "fsum", fp, ["f32"], 30)
    b = oe.wasmtime_invoke(tbin, FIXTURE, "fsum", fp, ["f32"], 30)
    check(a[0] == "ok" and a[1] == [("f32", 0x40F00000)],
          "wasm89 fsum -> 0x40f00000")
    check(compare.verdict(a[:2], b[:2])[0] == "PASS",
          "wasm89/wasmtime fsum agree on exact bits")

    # ---- finite-bit mismatch is caught (strict float FAIL) ----
    wrong = outcome("ok", [("f32", 0x40F00001)])
    check(compare.verdict(a[:2], wrong)[0] == "FAIL",
          "finite float bit mismatch reported FAIL")

    # ---- DIFF-003 NaN rule ----
    a = w89.invoke(wbin, FIXTURE, "nanout", [], ["f64"], 30)
    b = oe.wasmtime_invoke(tbin, FIXTURE, "nanout", [], ["f64"], 30)
    check(a[0] == "ok" and compare.is_nan("f64", a[1][0][1]),
          "wasm89 nanout -> NaN")
    check(compare.verdict(a[:2], b[:2])[0] == "PASS",
          "NaN results compare equal (payload ignored)")
    # NaN vs finite is a mismatch
    fin = outcome("ok", [("f64", 0x3FF0000000000000)])
    check(compare.verdict(a[:2], fin)[0] == "FAIL",
          "NaN vs finite reported FAIL")

    # ---- DIFF-004 trap agreement ----
    tp = [("i32", 1)]
    a = w89.invoke(wbin, FIXTURE, "divz", tp, ["i32"], 30)
    b = oe.wasmtime_invoke(tbin, FIXTURE, "divz", tp, ["i32"], 30)
    c = oe.wasm_interp_invoke(ibin, FIXTURE, "divz", tp, ["i32"], 30)
    check(a[0] == "trap", "wasm89 divz traps")
    check(compare.verdict(a[:2], b[:2])[0] == "PASS",
          "wasm89/wasmtime both trap -> PASS")
    check(compare.verdict(a[:2], c[:2])[0] == "PASS",
          "wasm89/wasm-interp both trap -> PASS")
    okcase = outcome("ok", [("i32", 0)])
    check(compare.verdict(a[:2], okcase)[0] == "FAIL",
          "trap-vs-ok reported FAIL")

    # ---- DIFF-006 comparator can fail (forced mismatch) ----
    params = [("i32", 3), ("i32", 4)]
    a = w89.invoke(wbin, FIXTURE, "iadd", params, ["i32"], 30)
    tampered = outcome("ok", [(a[1][0][0], (a[1][0][1] + 1)
                               & compare.MASK[a[1][0][0]])])
    check(compare.verdict(a[:2], tampered)[0] == "FAIL",
          "forced value mismatch reported FAIL (comparator is honest)")

    # ---- error status propagates as ERROR, never a silent PASS ----
    check(compare.verdict(outcome("error", None),
                          outcome("ok", [("i32", 0)]))[0] == "ERROR",
          "engine error status reported ERROR")

    # ---- DIFF-007 generator determinism ----
    m1 = gen.numeric_module(gen.randrange(777), 0, 4, 6)
    m2 = gen.numeric_module(gen.randrange(777), 0, 4, 6)
    check(m1.wasm == m2.wasm, "generator is deterministic from a seed")

    # ---- DIFF-005 missing engine raises EngineMissing ----
    missing = False
    try:
        common.which("wasm89-nonexistent-binary-xyz")
    except common.EngineMissing:
        missing = True
    check(missing, "a missing engine raises EngineMissing (never a pass)")

    # ---- oracle cross-check: the two reference engines agree with each
    # ---- other on integer results and traps (engine independence)
    for (x, y) in [(20, 22), (0x7FFFFFFF, 1), (0x80000000, 2)]:
        wt = oe.wasmtime_invoke(tbin, FIXTURE, "iadd",
                                [("i32", x), ("i32", y)], ["i32"], 30)
        ip = oe.wasm_interp_invoke(ibin, FIXTURE, "iadd",
                                   [("i32", x), ("i32", y)], ["i32"], 30)
        v = compare.verdict(wt[:2], ip[:2])[0]
        check(v == "PASS", "wasmtime/wasm-interp agree on iadd(%d,%d)" % (x, y))
    wt = oe.wasmtime_invoke(tbin, FIXTURE, "divz", [("i32", 1)], ["i32"], 30)
    ip = oe.wasm_interp_invoke(ibin, FIXTURE, "divz", [("i32", 1)], ["i32"], 30)
    check(compare.verdict(wt[:2], ip[:2])[0] == "PASS",
          "wasmtime/wasm-interp both trap on divz")

    if _failures:
        print("self-test: %d failure(s)" % len(_failures))
        return 1
    print("self-test: all passed")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except common.EngineMissing as exc:
        sys.stderr.write("ERROR: %s\n" % exc)
        sys.exit(2)
