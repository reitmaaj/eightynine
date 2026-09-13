#!/usr/bin/env python3
"""Bounded, seeded differential sweep for wasm89.

Usage: sweep.py [--seed N] [--modules N] [--funcs N] [--args N]
                [--template-args N] [--timeout SEC] [--wasm89 PATH]

Generates a corpus (random numeric modules plus fixed memory/control/call
templates), runs every case under wasm89 and the reference engines, and
requires agreement under the exact-bits + NaN rule. Per-case and
per-module deadlines bound the run; progress is printed per module; every
case is accounted for.

Exit 0 iff there are no mismatches and no errors (self-test is separate;
run test/diff/self_test.py first via the Justfile recipes).
"""

import argparse
import os
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import common  # noqa: E402
import compare  # noqa: E402
import gen_modules as gen  # noqa: E402
import oracle_engines as oe  # noqa: E402
import oracle_wasm89 as w89  # noqa: E402


def run_case(binaries, source, path, fs, vec, timeout, flags=()):
    """Run one case across engines; returns a list of verdict reports as
    (verdict, module_source, func_name, vec, reason)."""
    wbin, tbin, ibin = binaries
    reports = []
    w89out = w89.invoke(wbin, path, fs.name, vec, fs.result_types, timeout)
    if w89out[0] == "error":
        return [("ERROR", source, fs.name, vec,
                 "wasm89: " + str(w89out[2]))]
    wt = oe.wasmtime_invoke(tbin, path, fs.name, vec, fs.result_types,
                            timeout, feature_flags=flags or None)
    if wt[0] == "error":
        reports.append(("ERROR", source, fs.name, vec,
                        "wasmtime: " + str(wt[2])))
    else:
        v, note = compare.verdict(w89out[:2], wt[:2])
        reports.append((v, source, fs.name, vec,
                        "wasmtime: " + note))
    all_int = all(t in ("i32", "i64")
                  for t in fs.param_types + fs.result_types)
    if all_int:
        ip = oe.wasm_interp_invoke(ibin, path, fs.name, vec, fs.result_types,
                                   timeout, feature_flags=list(flags))
        if ip[0] != "error":
            v, note = compare.verdict(w89out[:2], ip[:2])
            reports.append((v, source, fs.name, vec,
                            "wasm-interp: " + note))
    return reports


def fmt_vec(vec):
    return "(" + compare.fmt_results(vec) + ")"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--modules", type=int, default=40,
                    help="random numeric modules")
    ap.add_argument("--funcs", type=int, default=6, help="funcs per module")
    ap.add_argument("--args", type=int, default=12, help="arg vectors/func")
    ap.add_argument("--template-args", type=int, default=8)
    ap.add_argument("--timeout", type=float,
                    default=float(os.environ.get("W89_DIFF_TIMEOUT", "20")))
    ap.add_argument("--wasm89", default=os.environ.get("W89_BIN"))
    ap.add_argument("--quiet", action="store_true",
                    help="suppress per-module progress lines")
    args = ap.parse_args()

    wbin = w89.discover(args.wasm89)
    tbin = oe.wasmtime_discover()
    ibin = oe.wasm_interp_discover()
    binaries = (wbin, tbin, ibin)

    rng = gen.randrange(args.seed)
    corpus = gen.generate(rng, args.modules, args.funcs, args.args,
                          args.template_args)

    passed = failed = errors = 0
    failures = []
    t0 = time.time()
    with tempfile.TemporaryDirectory() as tmp:
        for i, mod in enumerate(corpus):
            if not args.quiet:
                print("module %s" % mod.source, flush=True)
            path = os.path.join(tmp, "m%d.wasm" % i)
            with open(path, "wb") as f:
                f.write(mod.wasm)
            deadline = time.time() + max(10.0, args.timeout * 2)
            for fs in mod.funcs:
                for vec in fs.arg_vectors:
                    if time.time() > deadline:
                        errors += 1
                        failures.append(("ERROR", mod.source, fs.name, vec,
                                         "per-module deadline"))
                        continue
                    for report in run_case(binaries, mod.source, path, fs,
                                           vec, args.timeout, mod.flags):
                        verdict = report[0]
                        if verdict == "PASS":
                            passed += 1
                        elif verdict == "FAIL":
                            failed += 1
                            failures.append(report)
                        else:
                            errors += 1
                            failures.append(report)
    elapsed = time.time() - t0
    print("seed %d: %d passed, %d failed, %d errors (%.1fs)"
          % (args.seed, passed, failed, errors, elapsed))
    for (verdict, source, name, vec, reason) in failures:
        print("%s module=%s func=%s args=%s %s"
              % (verdict, source, name, fmt_vec(vec), reason))
    return 1 if (failed or errors) else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except common.EngineMissing as exc:
        sys.stderr.write("ERROR: %s\n" % exc)
        sys.exit(2)
