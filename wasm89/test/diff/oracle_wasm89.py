#!/usr/bin/env python3
"""wasm89 repl adapter for the differential harness.

Spawns one fresh `wasm89 repl` per case so every case starts from an empty
store with identical initial state, mirroring the one-shot reference
engines. The repl already speaks exact hex-bit values (`i32:0x..`,
`f64:0x..`) and `@trap`/`@exhaustion`/`@error` replies."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import common  # noqa: E402
import compare  # noqa: E402

INVOKE_PREFIXES = ("@return", "@trap", "@exhaustion", "@exception", "@error")


def discover(explicit=None):
    """Resolve the wasm89 runtime: an explicit path, $W89_BIN, the repo's
    build/wasm89, or `wasm89` on PATH. Raises EngineMissing if absent."""
    import shutil
    candidates = []
    if explicit:
        candidates.append(explicit)
    env = os.environ.get("W89_BIN")
    if env:
        candidates.append(env)
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.normpath(os.path.join(here, os.pardir, os.pardir))
    candidates.append(os.path.join(repo, "build", "wasm89"))
    for cand in candidates:
        if cand and os.path.isfile(cand) and os.access(cand, os.X_OK):
            return os.path.abspath(cand)
    found = shutil.which("wasm89")
    if found:
        return found
    raise common.EngineMissing(
        "wasm89 not found (set W89_BIN or build with `just build`)")


def engine_missing():
    return common.EngineMissing


def arg_tokens(params):
    return ["%s:0x%x" % (t, b) for (t, b) in params]


def _parse_reply(line, result_types):
    """Parse a single '@'-prefixed invoke reply line."""
    line = line.rstrip("\n")
    if line.startswith("@return"):
        body = line[len("@return"):].strip()
        values = []
        if body:
            for i, tok in enumerate(body.split()):
                vtype, _, rest = tok.partition(":")
                try:
                    bits = int(rest, 16) & compare.MASK[vtype]
                except (ValueError, KeyError):
                    return ("error", None, line)
                values.append((vtype, bits))
        if result_types is not None and len(values) != len(result_types):
            return ("error", None, line)
        return ("ok", values, line)
    if line.startswith("@trap"):
        return ("trap", None, line)
    if line.startswith("@exhaustion"):
        return ("exhaust", None, line)
    if line.startswith("@exception"):
        return ("exhaust", None, line)
    if line.startswith("@error"):
        return ("error", None, line)
    return None


def invoke(bin_path, module_path, func, params, result_types,
           timeout, feature_flags=None):
    """Run one wasm89 case; returns (status, values, raw)."""
    toks = arg_tokens(params)
    cmd = ("module %s\ninvoke last %s %d %s\nquit\n"
           % (module_path, func, len(toks), " ".join(toks)))
    rc, out, err = common.run_stdin([bin_path, "repl"], cmd, timeout)
    if rc != 0 and not out and err:
        return ("error", None, (err or out).strip())
    reply = None
    for line in out.splitlines():
        if line.startswith(INVOKE_PREFIXES):
            parsed = _parse_reply(line, result_types)
            if parsed is not None:
                reply = parsed
    if reply is None:
        return ("error", None, (out or err).strip() or "<no reply>")
    return reply
