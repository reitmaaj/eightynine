#!/usr/bin/env python3
"""Seeded generator of differential cases for the harness.

Two complementary generators, both deterministic functions of an integer
seed (design `.agent/design/0008`, section 3):

  * random numeric-expression modules: type-correct scalar expressions
    (generated bottom-up, so always valid), covering the
    conformance-green numeric/compare/conversion core;
  * fixed template modules for memory/control/calls/traps that the random
    generator must not risk.

Compilation uses `wat2wasm` (on PATH), so every emitted program is checked
against the spec before it reaches the engines.

Public API:
  randrange(seed)                -> python random.Random(seed)
  edge_values(vtype)             -> input edge-case bit pool
  random_values(rng, vtype, n)   -> input values (never exotic-NaN)
  sample_vectors(rng, types, n)  -> [[(type, bits), ...], ...]
  numeric_module(rng, idx, nfuncs, args) -> ModuleSpec
  compile_wat(wat)               -> wasm bytes
  ModuleSpec / FuncSpec
"""

import os
import random
import struct
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import compare  # noqa: E402
import common  # noqa: E402

INT = ("i32", "i64")
FLOAT = ("f32", "f64")
TYPES = INT + FLOAT

# ---- typed instruction tables (all spec core, conformance-green) --------

INT_BINARY = {
    "i32": ["add", "sub", "mul", "div_s", "div_u", "rem_s", "rem_u",
            "and", "or", "xor", "shl", "shr_s", "shr_u", "rotl", "rotr"],
    "i64": ["add", "sub", "mul", "div_s", "div_u", "rem_s", "rem_u",
            "and", "or", "xor", "shl", "shr_s", "shr_u", "rotl", "rotr"],
}
INT_UNARY = {
    "i32": ["clz", "ctz", "popcnt"],
    "i64": ["clz", "ctz", "popcnt"],
}
# copysign is intentionally excluded: when its sign operand is a NaN,
# wasm89's deterministic profile (positive-canonical NaN) and engines that
# preserve the NaN sign bit legitimately differ in the *sign* of the finite
# result, which is not covered by the NaN-result rule.
FLOAT_BINARY = {
    "f32": ["add", "sub", "mul", "div", "min", "max"],
    "f64": ["add", "sub", "mul", "div", "min", "max"],
}
FLOAT_UNARY = {
    "f32": ["abs", "neg", "ceil", "floor", "trunc", "nearest", "sqrt"],
    "f64": ["abs", "neg", "ceil", "floor", "trunc", "nearest", "sqrt"],
}
INT_CMP = {
    "i32": ["eq", "ne", "lt_s", "lt_u", "gt_s", "gt_u", "le_s", "le_u",
            "ge_s", "ge_u"],
    "i64": ["eq", "ne", "lt_s", "lt_u", "gt_s", "gt_u", "le_s", "le_u",
            "ge_s", "ge_u"],
}
FLOAT_CMP = {
    "f32": ["eq", "ne", "lt", "gt", "le", "ge"],
    "f64": ["eq", "ne", "lt", "gt", "le", "ge"],
}
# conversions as (target, source, op) triples; non-saturating truncations
# trap on NaN/out-of-range, which the harness treats as runtime-failure
# agreement.
CONVERT = [
    ("i64", "i32", "i64.extend_i32_s"),
    ("i64", "i32", "i64.extend_i32_u"),
    ("i32", "i64", "i32.wrap_i64"),
    ("i32", "f32", "i32.trunc_f32_s"),
    ("i32", "f32", "i32.trunc_f32_u"),
    ("i32", "f64", "i32.trunc_f64_s"),
    ("i32", "f64", "i32.trunc_f64_u"),
    ("i64", "f32", "i64.trunc_f32_s"),
    ("i64", "f32", "i64.trunc_f32_u"),
    ("i64", "f64", "i64.trunc_f64_s"),
    ("i64", "f64", "i64.trunc_f64_u"),
    ("i32", "f32", "i32.trunc_sat_f32_s"),
    ("i32", "f32", "i32.trunc_sat_f32_u"),
    ("i32", "f64", "i32.trunc_sat_f64_s"),
    ("i32", "f64", "i32.trunc_sat_f64_u"),
    ("i64", "f32", "i64.trunc_sat_f32_s"),
    ("i64", "f32", "i64.trunc_sat_f32_u"),
    ("i64", "f64", "i64.trunc_sat_f64_s"),
    ("i64", "f64", "i64.trunc_sat_f64_u"),
    ("f32", "i32", "f32.convert_i32_s"),
    ("f32", "i32", "f32.convert_i32_u"),
    ("f32", "i64", "f32.convert_i64_s"),
    ("f32", "i64", "f32.convert_i64_u"),
    ("f64", "i32", "f64.convert_i32_s"),
    ("f64", "i32", "f64.convert_i32_u"),
    ("f64", "i64", "f64.convert_i64_s"),
    ("f64", "i64", "f64.convert_i64_u"),
    ("f32", "f64", "f32.demote_f64"),
    ("f64", "f32", "f64.promote_f32"),
    ("i32", "f32", "i32.reinterpret_f32"),
    ("f32", "i32", "f32.reinterpret_i32"),
    ("i64", "f64", "i64.reinterpret_f64"),
    ("f64", "i64", "f64.reinterpret_i64"),
]


def conversions_from(target):
    """Yield (source_type, op) pairs that produce `target`."""
    for t, s, op in CONVERT:
        if t == target and s != target:
            yield s, op


# ---- constants / rng ----------------------------------------------------

def randrange(seed):
    return random.Random(seed)


def signed(bits, width):
    if width == 32:
        return bits if bits < 0x80000000 else bits - 0x100000000
    return bits if bits < 0x8000000000000000 else bits - 0x10000000000000000


def wat_const(vtype, bits):
    if vtype == "i32":
        return "i32.const %d" % signed(bits, 32)
    if vtype == "i64":
        return "i64.const %d" % signed(bits, 64)
    return "%s.const %s" % (vtype, wat_float(vtype, bits))


def wat_float(vtype, bits):
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


def edge_values(vtype):
    """Input edge-case bit pool for a value type (no exotic NaN payloads)."""
    if vtype == "i32":
        return [0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x40000000]
    if vtype == "i64":
        return [0, 1, 0x7FFFFFFFFFFFFFFF, 0x8000000000000000,
                0xFFFFFFFFFFFFFFFF, 0x4000000000000000]
    if vtype == "f32":
        return [0x00000000, 0x80000000, 0x3F800000, 0xBF800000,
                0x7F7FFFFF, 0xFF7FFFFF, 0x00800000, 0x00000001,
                0x7F800000, 0xFF800000, 0x3F000000, 0x40490FDB]
    return [0x0000000000000000, 0x8000000000000000,
            0x3FF0000000000000, 0xBFF0000000000000,
            0x7FEFFFFFFFFFFFFF, 0xFFEFFFFFFFFFFFFF,
            0x0010000000000000, 0x0000000000000001,
            0x7FF0000000000000, 0xFFF0000000000000,
            0x3FE0000000000000, 0x400921FB54442D18]


def _finite_random_bits(vtype):
    """Random finite-or-infinite (never NaN) float bit pattern."""
    if vtype == "f32":
        sign = random.randrange(2) << 31
        exp = random.randrange(256)
        mant = 0 if exp == 255 else random.randrange(1 << 23)
        return sign | (exp << 23) | mant
    sign = random.randrange(2) << 63
    exp = random.randrange(2048)
    mant = 0 if exp == 2047 else random.randrange(1 << 52)
    return sign | (exp << 52) | mant


def random_values(rng, vtype, n):
    """`n` input values mixing the edge pool and type-correct random bits
    (floats are finite-or-infinite, never exotic-NaN)."""
    pool = list(edge_values(vtype))
    rng.shuffle(pool)
    if vtype in FLOAT:
        pool.append(compare.CANONICAL_NAN[vtype])
    while len(pool) < n:
        if vtype == "i32":
            bits = rng.getrandbits(32)
        elif vtype == "i64":
            bits = rng.getrandbits(64)
        else:
            bits = _finite_random_bits(vtype)
            if compare.is_nan(vtype, bits):
                continue
        pool.append(bits)
    return pool[:n]


def sample_vectors(rng, types, count):
    """`count` argument vectors over `types` (seeded, reproducible)."""
    vectors = []
    for i in range(count):
        vec = []
        for t in types:
            pool = random_values(rng, t, max(8, count))
            vec.append((t, pool[i % len(pool)]))
        if rng.random() < 0.5:
            vec = [(t, random_values(rng, t, 1)[0]) for t in types]
        vectors.append(vec)
    return vectors


# ---- expression generation ----------------------------------------------

def _expr_leaf(rng, vtype, params):
    if params and rng.random() < 0.5:
        idx = [i for i, t in enumerate(params) if t == vtype]
        if idx:
            return "local.get %d" % rng.choice(idx)
    return wat_const(vtype, rng.choice(edge_values(vtype)))


def gen_expression(rng, vtype, params, depth):
    """Emit WAT instructions producing one value of `vtype`."""
    if depth <= 0 or rng.random() < 0.35:
        return _expr_leaf(rng, vtype, params)

    # a value-producing typed select / if node (keeps everything type-correct)
    if rng.random() < 0.22:
        return gen_if_or_select(rng, vtype, params, depth)

    # a conversion into vtype from some source type
    convs = list(conversions_from(vtype))
    if rng.random() < 0.5 and convs:
        source, op = rng.choice(convs)
        operand = gen_expression(rng, source, params, depth - 1)
        return "%s %s" % (operand, op)

    if vtype in INT:
        if rng.random() < 0.5:
            op = "%s.%s" % (vtype, rng.choice(INT_BINARY[vtype]))
            return "%s %s %s" % (
                gen_expression(rng, vtype, params, depth - 1),
                gen_expression(rng, vtype, params, depth - 1), op)
        op = "%s.%s" % (vtype, rng.choice(INT_UNARY[vtype]))
        return "%s %s" % (gen_expression(rng, vtype, params, depth - 1), op)

    # float
    if rng.random() < 0.6:
        op = "%s.%s" % (vtype, rng.choice(FLOAT_BINARY[vtype]))
        return "%s %s %s" % (gen_expression(rng, vtype, params, depth - 1),
                             gen_expression(rng, vtype, params, depth - 1), op)
    op = "%s.%s" % (vtype, rng.choice(FLOAT_UNARY[vtype]))
    return "%s %s" % (gen_expression(rng, vtype, params, depth - 1), op)


def gen_if_or_select(rng, vtype, params, depth):
    """A value of `vtype` from a typed select or an if/else."""
    if rng.random() < 0.4:
        # untyped select (numeric operands only): <a> <b> <cond> select
        a = gen_expression(rng, vtype, params, depth - 1)
        b = gen_expression(rng, vtype, params, depth - 1)
        cond = gen_expression(rng, "i32", params, depth - 1)
        return "%s %s %s select" % (a, b, cond)
    # value-returning if/else (flat form)
    cond = gen_expression(rng, "i32", params, depth - 1)
    then_expr = gen_expression(rng, vtype, params, depth - 1)
    else_expr = gen_expression(rng, vtype, params, depth - 1)
    return "%s if (result %s) %s else %s end" \
        % (cond, vtype, then_expr, else_expr)


def gen_boolean(rng, params, depth):
    """An i32-valued expression, possibly a comparison/eqz of another type."""
    if depth <= 0 or rng.random() < 0.5:
        return gen_expression(rng, "i32", params, 1)
    base = rng.choice(TYPES)
    if base in INT:
        if rng.random() < 0.5:
            op = "%s.%s" % (base, rng.choice(INT_CMP[base]))
            return "%s %s %s" % (gen_expression(rng, base, params, depth - 1),
                                 gen_expression(rng, base, params, depth - 1),
                                 op)
        op = "%s.eqz" % base
        return "%s %s" % (gen_expression(rng, base, params, depth - 1), op)
    op = "%s.%s" % (base, rng.choice(FLOAT_CMP[base]))
    return "%s %s %s" % (gen_expression(rng, base, params, depth - 1),
                         gen_expression(rng, base, params, depth - 1), op)


def function_body(rng, params, results):
    parts = []
    for i, rt in enumerate(results):
        if len(results) == 1 and rt == "i32":
            parts.append(gen_boolean(rng, params, 3))
        else:
            parts.append(gen_expression(rng, rt, params, 3))
    return " ".join(parts)


def build_module_wat(rng, funcs):
    lines = ["(module"]
    for name, params, results in funcs:
        lines.append("  (func (export \"%s\")" % name)
        if params:
            lines.append("    (param %s)" % " ".join(params))
        if results:
            lines.append("    (result %s)" % " ".join(results))
        lines.append("    %s" % function_body(rng, params, results))
        lines.append("  )")
    lines.append(")")
    return "\n".join(lines)


# ---- wat2wasm / module specs --------------------------------------------

def compile_wat(wat, timeout=60):
    """Compile WAT text to wasm bytes via wat2wasm (which enforces typing)."""
    wat2wasm = common.which("wat2wasm")
    fd, wat_path = tempfile.mkstemp(suffix=".wat")
    try:
        with os.fdopen(fd, "w") as f:
            f.write(wat)
        out_path = wat_path + ".wasm"
        try:
            rc, out, err = common.run_argv(
                [wat2wasm, wat_path, "-o", out_path], timeout)
            if rc != 0:
                raise ValueError("wat2wasm rejected module: %s"
                                 % ((err or out).strip()))
            with open(out_path, "rb") as f:
                return f.read()
        finally:
            try:
                os.unlink(out_path)
            except OSError:
                pass
    finally:
        try:
            os.unlink(wat_path)
        except OSError:
            pass


class ModuleSpec(object):
    """A concrete wasm module plus entrypoints and argument vectors."""

    def __init__(self, source, wasm, funcs, flags=()):
        self.source = source
        self.wasm = wasm
        self.funcs = funcs
        self.flags = list(flags)


class FuncSpec(object):
    def __init__(self, name, param_types, result_types, arg_vectors):
        self.name = name
        self.param_types = param_types
        self.result_types = result_types
        self.arg_vectors = arg_vectors


def numeric_module(rng, idx, nfuncs, args_per_func=16):
    """A module of `nfuncs` random numeric expressions (deterministic)."""
    rng2 = random.Random(rng.randrange(1 << 31))
    funcs = []
    specs = []
    for i in range(nfuncs):
        param_types = [rng2.choice(TYPES)
                       for _ in range(rng2.randint(0, 3))]
        nres = 1 if rng2.random() < 0.75 else 2
        result_types = [rng2.choice(TYPES) for _ in range(nres)]
        name = "f%d" % i
        funcs.append((name, param_types, result_types))
        vectors = sample_vectors(rng2, param_types, args_per_func)
        specs.append(FuncSpec(name, param_types, result_types, vectors))
    wat = build_module_wat(rng2, funcs)
    wasm = compile_wat(wat)
    return ModuleSpec("num-%d" % idx, wasm, specs)


# fixed template modules: memory/control/calls/traps covered by hand-vetted
# fixtures whose entrypoints mask/clamp their inputs so every curated or
# random argument vector stays valid and fast.
_TEMPLATE_FUNCS = {
    "f.wasm": [
        ("iadd", ["i32", "i32"], ["i32"]),
        ("imul", ["i64", "i64"], ["i64"]),
        ("fdiv", ["f64", "f64"], ["f64"]),
        ("sel", ["i32", "i32", "i32"], ["i32"]),
        ("lt", ["i32", "i32"], ["i32"]),
        ("multi", ["i32"], ["i32", "i32", "i32"]),
        ("divz", ["i32"], ["i32"]),
        ("nanout", [], ["f64"]),
        ("fsum", ["f32", "f32"], ["f32"]),
    ],
    "tpl.wasm": [
        ("memrt", ["i32", "i32"], ["i32"]),
        ("lsum", ["i32"], ["i32"]),
        ("dispatch", ["i32", "i32", "i32"], ["i32"]),
        ("fac", ["i32"], ["i32"]),
    ],
    "m.wasm": [
        ("cs", ["f64", "f64"], ["f64"]),
        ("csneg", ["f64"], ["f64"]),
        ("sqrts", ["f64"], ["f64"]),
        ("trunci", ["f64"], ["i32"]),
        ("storeload", ["i32"], ["i32"]),
        ("oob", [], ["i32"]),
        ("boom", [], ["i32"]),
        ("tcount", ["i32"], ["i32"]),
    ],
    "g.wasm": [
        ("lcl", ["i32", "i32"], ["i32"]),
        ("gglob", ["i32"], ["i32"]),
        ("gglob64", ["i32"], ["i64"]),
    ],
    "buf.wasm": [
        ("fillv", ["i32", "i32"], ["i32"]),
        ("copyv", ["i32", "i32"], ["i32"]),
    ],
    "ex.wasm": [
        ("boom", ["i32"], []),
        ("maybe", ["i32"], ["i32"]),
    ],
    "ct.wasm": [
        ("m", ["i32"], ["i32"]),
    ],
    "mem64.wasm": [
        ("rt", ["i64", "i32"], ["i32"]),
    ],
    "mm.wasm": [
        ("m1rt", ["i32", "i32"], ["i32"]),
        ("m0rt", ["i32", "i32"], ["i32"]),
    ],
}

# feature flags required to run a template fixture (passed to wasm-interp
# via --enable-<flag> and to wasmtime via -W <flag>).
_TEMPLATE_FLAGS = {
    "ex.wasm": ["exceptions"],
    "ct.wasm": ["exceptions"],
    "mem64.wasm": ["memory64"],
    "mm.wasm": ["multi-memory"],
}


def _fixture_dir():
    return os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "fixtures")


def template_specs(rng, args_per_func=8):
    """ModuleSpecs for the committed template fixtures (memory/control/
    calls/traps), with seeded argument vectors."""
    rng2 = random.Random(rng.randrange(1 << 31))
    specs = []
    for fname in ("f.wasm", "tpl.wasm", "m.wasm", "g.wasm", "buf.wasm",
                  "ex.wasm", "ct.wasm", "mem64.wasm", "mm.wasm"):
        with open(os.path.join(_fixture_dir(), fname), "rb") as f:
            wasm = f.read()
        funcs = []
        for name, param_types, result_types in _TEMPLATE_FUNCS[fname]:
            vectors = sample_vectors(rng2, param_types, args_per_func)
            funcs.append(FuncSpec(name, param_types, result_types, vectors))
        specs.append(ModuleSpec("tpl-" + fname, wasm, funcs,
                                _TEMPLATE_FLAGS.get(fname, ())))
    return specs


def generate(rng, nmods, nfuncs, args_per_func, n_template_args=8):
    """Full corpus for a sweep: random numeric modules plus templates."""
    specs = []
    for i in range(nmods):
        specs.append(numeric_module(rng, i, nfuncs, args_per_func))
    specs.extend(template_specs(rng, n_template_args))
    return specs
