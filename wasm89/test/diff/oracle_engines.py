#!/usr/bin/env python3
"""Reference-engine adapters (wasmtime, wasm-interp) for the harness.

Exactness contract (design `0008`, `0015-DIFF-011`):
  * wasmtime prints integer results as signed decimal and finite float
    results as Rust shortest-round-trip decimals (injective for finite
    values), and NaN as "NaN". The harness decodes each back to exact bits
    using the declared result types, so wasmtime is an exact oracle for
    all scalar numerics (NaN compared by the NaN rule).
  * wasm-interp (wabt) prints integer results exactly but floats only to a
    fixed ~6-decimal precision and NaN as "nan"; it is therefore used only
    when a case's results are all-integer, plus for runtime-failure
    agreement."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import common  # noqa: E402
import compare  # noqa: E402


# ---- wasmtime -----------------------------------------------------------

def wasmtime_discover():
    return common.which("wasmtime")


def _wasmtime_args(params, result_types):
    argv = []
    for (t, b) in params:
        if t in ("i32", "i64"):
            argv.append(compare.signed_int_bits_to_decimal(t, b))
        else:
            argv.append(compare.float_bits_to_decimal(t, b))
    return argv


def _classify_wasmtime(rc, out, err):
    text = (err + "\n" + out).lower()
    if rc == 0:
        return "ok"
    if "wasm trap:" in text:
        return "trap"
    if "exhaust" in text:
        return "exhaust"
    if "thrown wasm exception" in text or "uncaught" in text:
        return "trap"
    return "error"


def _parse_wasmtime_results(stdout, result_types):
    """Decode wasmtime decimal result lines into exact bits by type."""
    lines = [ln for ln in stdout.splitlines() if ln.strip()]
    if result_types is None:
        result_types = []
    if len(lines) != len(result_types):
        return None
    values = []
    for vtype, token in zip(result_types, lines):
        token = token.strip()
        if vtype in ("i32", "i64"):
            try:
                bits = int(token, 10) & compare.MASK[vtype]
            except ValueError:
                return None
            values.append((vtype, bits))
        elif vtype in ("f32", "f64"):
            try:
                values.append(compare.decimal_to_bits(vtype, token))
            except (ValueError, OverflowError):
                return None
        else:
            return None
    return values


def wasmtime_invoke(bin_path, module_path, func, params, result_types,
                    timeout, feature_flags=None):
    argv = [bin_path, "run"]
    if feature_flags:
        argv.append("-W")
        argv.append(",".join(feature_flags))
    argv += ["--invoke", func, module_path]
    argv += _wasmtime_args(params, result_types)
    rc, out, err = common.run_argv(argv, timeout)
    status = _classify_wasmtime(rc, out, err)
    if status == "ok":
        values = _parse_wasmtime_results(out, result_types)
        if values is None:
            return ("error", None, "unparseable wasmtime output: %r" % out)
        return (status, values, out)
    return (status, None, (err or out).strip())


# ---- wasm-interp (integer-exact) ----------------------------------------

def wasm_interp_discover():
    return common.which("wasm-interp")


def _wasm_interp_args(params):
    argv = []
    for (t, b) in params:
        if t in ("i32", "i64"):
            argv += ["-a", "%s:%d" % (t, b)]
        else:
            # wasm-interp cannot take float arguments as exact bits; the
            # caller restricts integer-exact comparison to all-int cases.
            argv += ["-a", "%s:0" % t]
    return argv


def _parse_wasm_interp(stdout, result_types):
    for line in stdout.splitlines():
        if "=>" not in line:
            continue
        _, _, rhs = line.partition("=>")
        rhs = rhs.strip()
        if rhs.startswith("error"):
            return "trap", None
        if not rhs:
            return "ok", []
        values = []
        for tok in rhs.split(", "):
            parsed = compare.parse_hex_value(tok)
            if parsed is None:
                return "error", None
            if parsed[0] in ("f32", "f64"):
                return "error", None
            values.append(parsed)
        return "ok", values
    return "error", None


def wasm_interp_invoke(bin_path, module_path, func, params, result_types,
                       timeout, feature_flags=None):
    argv = [bin_path, module_path, "-r", func]
    if feature_flags:
        for flag in feature_flags:
            argv.append("--enable-" + flag)
    argv += _wasm_interp_args(params)
    rc, out, err = common.run_argv(argv, timeout)
    status, values = _parse_wasm_interp(out, result_types)
    if status == "error":
        return ("error", None, (out or err).strip())
    return (status, values, out)
