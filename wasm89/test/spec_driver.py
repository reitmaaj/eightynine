#!/usr/bin/env python3
"""Drive the wasm89 runtime through the official spec testsuite.

Usage: spec_driver.py <runtime> <file.wast>

Converts the .wast to JSON + .wasm files via `wasm-tools json-from-wast`
(which must be on PATH), then drives one persistent `wasm89 repl` process
through the testsuite commands. The REPL answers with @-prefixed result
lines; bare stdout lines are host prints (spectest) and are ignored.

Skipped: .wat-only malformed modules and modules that fail decode with
"unsupported feature" / "illegal opcode" (SIMD/GC, out of scope).
"""

import json
import os
import select
import struct
import subprocess
import sys
import tempfile
import time

COMMAND_TIMEOUT = float(os.environ.get("W89_COMMAND_TIMEOUT", "90"))


class DriverTimeout(Exception):
    """A single REPL command was not answered within its deadline."""


def value_token(v):
    t = v["type"]
    s = v.get("value")
    if t == "i32":
        return "i32:0x%x" % (int(s, 0) & 0xFFFFFFFF)
    if t == "i64":
        return "i64:0x%x" % (int(s, 0) & 0xFFFFFFFFFFFFFFFF)
    if t == "f32":
        if s.startswith("nan"):
            return "f32:0x7fc00000"
        return "f32:0x%x" % (int(s, 0) & 0xFFFFFFFF)
    if t == "f64":
        if s.startswith("nan"):
            return "f64:0x7ff8000000000000"
        return "f64:0x%x" % (int(s, 0) & 0xFFFFFFFFFFFFFFFF)
    if s == "null":
        ht = {"funcref": "func", "externref": "extern", "exnref": "exn",
              "anyref": "any", "eqref": "eq", "ref": "any",
              "refnull": "none", "nullref": "none"}.get(t, t)
        return "ref.null:" + ht
    if t == "ref.null":
        return "ref.null:" + s
    if t == "ref.func":
        return "ref.func:*" if s is None else "ref.func:" + s
    if t in ("funcref", "ref.func"):
        return "ref.func:*" if s is None else "ref.func:" + s
    if t == "ref.extern":
        return None if s is None else "ref.extern:0x%x" % (
            int(s, 0) & 0xFFFFFFFFFFFFFFFF)
    if t in ("externref",):
        return None if s is None else "ref.extern:0x%x" % (
            int(s, 0) & 0xFFFFFFFFFFFFFFFF)
    return None  # unrecognized value type: skip the command


def args_tokens(args):
    return [value_token(a) for a in args]


def tokens_ok(tokens):
    return tokens is not None and all(t is not None for t in tokens)


class Runtime:
    def __init__(self, cmd, timeout=COMMAND_TIMEOUT):
        self.timeout = timeout
        self.dead = False
        if isinstance(cmd, (list, tuple)):
            argv = list(cmd)
        else:
            argv = [cmd, "repl"]
        self.p = subprocess.Popen(argv, stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, bufsize=0)
        self.buf = b""

    def _next_line(self, deadline):
        """Return the next complete output line as bytes, or None once the
        deadline passes. Reads with os.read into a self-managed buffer instead
        of a Python BufferedReader: a BufferedReader read-ahead can pull the
        @-reply into Python while select() reports an empty pipe, stranding the
        reply behind an intervening host-print (non-@) line."""
        while True:
            nl = self.buf.find(b"\n")
            if nl >= 0:
                raw = self.buf[:nl]
                self.buf = self.buf[nl + 1:]
                return raw
            rem = deadline - time.time()
            if rem <= 0:
                return None
            r, _, _ = select.select([self.p.stdout], [], [], min(rem, 0.5))
            if not r:
                continue
            chunk = os.read(self.p.stdout.fileno(), 65536)
            if not chunk:
                raise RuntimeError("repl closed unexpectedly")
            self.buf += chunk

    def command(self, s, timeout=None):
        if self.dead:
            raise RuntimeError("repl is dead after a stall")
        eff = self.timeout if timeout is None else timeout
        deadline = time.time() + eff
        self.p.stdin.write(s.encode("utf-8") + b"\n")
        self.p.stdin.flush()
        while True:
            raw = self._next_line(deadline)
            if raw is None:
                raise DriverTimeout(
                    "no reply within %.1fs to: %s" % (eff, s))
            line = raw.decode("utf-8", "replace").strip()
            if line.startswith("@"):
                return line

    def close(self):
        if not self.dead:
            try:
                self.p.stdin.write(b"quit\n")
                self.p.stdin.flush()
            except Exception:
                pass
            try:
                self.p.wait(timeout=2)
                return
            except Exception:
                pass
        self.dead = True
        self.p.kill()
        try:
            self.p.wait()
        except Exception:
            pass


def is_unsupported(msg):
    return ("unsupported" in msg or "illegal opcode" in msg
            or "not implemented" in msg)


def main():
    if len(sys.argv) != 3:
        sys.stderr.write("usage: spec_driver.py <runtime> <file.wast>\n")
        return 2
    runtime = sys.argv[1]
    wast = os.path.abspath(sys.argv[2])
    base = os.path.splitext(os.path.basename(wast))[0]

    with tempfile.TemporaryDirectory() as workdir:
        r = subprocess.run(["wasm-tools", "json-from-wast", wast, "--output",
                            os.path.join(workdir, base + ".json")],
                           capture_output=True, text=True, cwd=workdir)
        if r.returncode != 0:
            sys.stderr.write("json-from-wast failed: %s\n" % r.stderr)
            return 1
        with open(os.path.join(workdir, base + ".json")) as f:
            data = json.load(f)

        rt = Runtime(runtime)
        passed = 0
        failed = 0
        skipped = 0
        last_skipped = False
        last_module_ok = False
        proposal_insts = set()
        skipped_mods = set()
        loaded_mods = set()

        def bad_field(action):
            f = action.get("field", "")
            return any(ord(ch) < 0x20 or ord(ch) == 0x7f for ch in f) \
                or any(ch in f for ch in " \t") or f == ""

        def exec_skip(action=None):
            nonlocal skipped, last_skipped
            if last_skipped:
                skipped += 1
                return True
            if action is not None and bad_field(action):
                skipped += 1
                return True
            return False

        def module_ref(action):
            m = action.get("module", "")
            if m is None or m == "":
                return "last"
            return m

        def module_check(cmd, expect_prefixes):
            """Send a module command and classify the result."""
            nonlocal passed, failed, skipped, last_skipped, last_module_ok
            fn = cmd.get("filename", "")
            if fn.endswith(".wat"):
                if cmd.get("name"):
                    skipped_mods.add(cmd["name"])
                last_skipped = True
                last_module_ok = False
                skipped += 1
                return
            name = cmd.get("name", "")
            if name:
                resp = rt.command("module %s %s"
                                  % (os.path.join(workdir, fn), name))
            else:
                resp = rt.command("module " + os.path.join(workdir, fn))
            msg = resp[1:] if resp.startswith("@") else resp
            if is_unsupported(msg):
                if name:
                    skipped_mods.add(name)
                last_skipped = True
                last_module_ok = False
                skipped += 1
                return
            if resp == "@ok":
                if expect_prefixes is not None:
                    failed += 1
                    sys.stderr.write("FAIL: %s:%d: expected rejection "
                                     "(%s), module loaded ok\n"
                                     % (base, cmd.get("line", 0),
                                        expect_prefixes))
                    return
                last_skipped = False
                last_module_ok = True
                if name:
                    loaded_mods.add(name)
                passed += 1
                return
            last_module_ok = False
            if msg.startswith("error ") or msg.startswith("error:"):
                msg = msg[6:]
            import re
            im = re.search(r'unknown import "([^"]*)"', msg)
            if im and (im.group(1) in proposal_insts
                       or im.group(1) in skipped_mods):
                if name:
                    skipped_mods.add(name)
                last_skipped = True
                skipped += 1
                return
            if expect_prefixes is None:
                failed += 1
                sys.stderr.write("FAIL: %s:%d: module: %s\n" % (
                    base, cmd.get("line", 0), resp))
                return
            if any(e in msg for e in expect_prefixes):
                passed += 1
            else:
                failed += 1
                sys.stderr.write("FAIL: %s:%d: %s\n  expected: %s\n  got: %s\n"
                                 % (base, cmd.get("line", 0),
                                    cmd["filename"],
                                    expect_prefixes, msg))

        def action_command(cmd, action):
            """Send invoke/get and return the response line."""
            a = action
            toks = args_tokens(a.get("args", []))
            ref = module_ref(a)
            if a["type"] == "invoke":
                n = len(toks)
                return rt.command("invoke %s %s %d %s"
                                  % (ref, a["field"], n, " ".join(toks)))
            if a["type"] == "get":
                return rt.command("get %s %s" % (ref, a["field"]))
            raise ValueError(a["type"])

        for cmd in data["commands"]:
            t = cmd["type"]
            line = cmd.get("line", 0)
            try:
                if t == "module":
                    module_check(cmd, None)
                elif t == "module_definition":
                    skipped += 1
                elif t == "module_instance":
                    proposal_insts.add(cmd["instance"])
                    skipped += 1
                elif t == "register":
                    mname = cmd.get("name", "")
                    if mname != "" and (mname in proposal_insts
                                        or mname in skipped_mods
                                        or mname not in loaded_mods):
                        skipped_mods.add(mname)
                        skipped += 1
                    elif mname == "" and not last_module_ok:
                        if cmd.get("as"):
                            skipped_mods.add(cmd["as"])
                        skipped += 1
                    else:
                        rt.command("register %s" % cmd["as"])
                        passed += 1
                elif t == "get":
                    if exec_skip({"field": cmd["field"]}):
                        continue
                    resp = rt.command("get %s %s"
                                      % (module_ref(cmd), cmd["field"]))
                    if resp.startswith("@return"):
                        passed += 1
                    elif is_unsupported(resp):
                        skipped += 1
                    else:
                        failed += 1
                        sys.stderr.write("FAIL: %s:%d: get: %s\n"
                                         % (base, line, resp))
                elif t == "action":
                    if exec_skip(cmd["action"]):
                        continue
                    toks = args_tokens(cmd["action"].get("args", []))
                    if not tokens_ok(toks):
                        skipped += 1
                        continue
                    resp = action_command(cmd, cmd["action"])
                    if resp.startswith("@return"):
                        passed += 1
                    elif is_unsupported(resp):
                        skipped += 1
                    else:
                        failed += 1
                        sys.stderr.write("FAIL: %s:%d: action trapped: %s\n"
                                         % (base, line, resp))
                elif t == "assert_return":
                    if exec_skip(cmd["action"]):
                        continue
                    act = cmd["action"]
                    ref = module_ref(act)
                    if act["type"] == "invoke":
                        toks = args_tokens(act.get("args", []))
                        exps = [value_token(e) for e in cmd["expected"]]
                        if not tokens_ok(toks) or not tokens_ok(exps):
                            skipped += 1
                            continue
                        resp = rt.command(
                            "assert_return %s %s %d %s %d %s"
                            % (ref, act["field"], len(toks), " ".join(toks),
                               len(exps), " ".join(exps)))
                    elif act["type"] == "get":
                        exps = [value_token(e) for e in cmd["expected"]]
                        resp = rt.command(
                            "assert_return_get %s %s %s"
                            % (ref, act["field"], exps[0]))
                    else:
                        raise ValueError(act["type"])
                    if resp == "@pass":
                        passed += 1
                    elif is_unsupported(resp):
                        skipped += 1
                    else:
                        failed += 1
                        sys.stderr.write("FAIL: %s:%d: %s\n"
                                         % (base, line, resp))
                elif t == "assert_trap":
                    if cmd.get("filename") is not None:
                        if last_skipped:
                            skipped += 1
                            continue
                        module_check(cmd, [cmd["text"]])
                        continue
                    act = cmd["action"]
                    if exec_skip(act):
                        continue
                    toks = args_tokens(act.get("args", []))
                    if not tokens_ok(toks):
                        skipped += 1
                        continue
                    resp = rt.command("assert_trap %s %s %d %s %s"
                                      % (module_ref(act), act["field"],
                                         len(toks), " ".join(toks),
                                         cmd["text"]))
                    if resp == "@pass":
                        passed += 1
                    elif is_unsupported(resp):
                        skipped += 1
                    else:
                        failed += 1
                        sys.stderr.write("FAIL: %s:%d: assert_trap: %s\n"
                                         % (base, line, resp))
                elif t == "assert_exhaustion":
                    act = cmd["action"]
                    if exec_skip(act):
                        continue
                    toks = args_tokens(act.get("args", []))
                    if not tokens_ok(toks):
                        skipped += 1
                        continue
                    resp = rt.command("assert_exhaustion %s %s %d %s"
                                      % (module_ref(act), act["field"],
                                         len(toks), " ".join(toks)))
                    if resp == "@pass":
                        passed += 1
                    elif is_unsupported(resp):
                        skipped += 1
                    else:
                        failed += 1
                        sys.stderr.write("FAIL: %s:%d: exhaustion: %s\n"
                                         % (base, line, resp))
                elif t == "assert_invalid":
                    if last_skipped:
                        skipped += 1
                    else:
                        module_check(cmd, [cmd["text"]])
                elif t == "assert_malformed":
                    module_check(cmd, [cmd["text"]])
                elif t == "assert_unlinkable":
                    module_check(cmd, [cmd["text"]])
                elif t == "assert_uninstantiable":
                    if last_skipped:
                        skipped += 1
                        continue
                    fn = cmd.get("filename", "")
                    if fn.endswith(".wat"):
                        skipped += 1
                        continue
                    resp = rt.command("module " + os.path.join(workdir, fn))
                    msg = resp[1:] if resp.startswith("@") else resp
                    if is_unsupported(msg):
                        skipped += 1
                        continue
                    if resp == "@ok":
                        failed += 1
                        sys.stderr.write(
                            "FAIL: %s:%d: expected instantiation trap, got ok\n"
                            % (base, line))
                    elif msg.startswith("error ") or msg.startswith("error:"):
                        if cmd["text"] in msg:
                            passed += 1
                        else:
                            failed += 1
                            sys.stderr.write("FAIL: %s:%d: %s\n"
                                             % (base, line, resp))
                    else:
                        failed += 1
                        sys.stderr.write("FAIL: %s:%d: %s\n"
                                         % (base, line, resp))
                elif t == "assert_exception":
                    act = cmd["action"]
                    if exec_skip(act):
                        continue
                    toks = args_tokens(act.get("args", []))
                    if not tokens_ok(toks):
                        skipped += 1
                        continue
                    resp = rt.command("assert_exception %s %s %d %s"
                                      % (module_ref(act), act["field"],
                                         len(toks), " ".join(toks)))
                    if resp == "@pass":
                        passed += 1
                    elif is_unsupported(resp):
                        skipped += 1
                    else:
                        failed += 1
                        sys.stderr.write("FAIL: %s:%d: assert_exception: %s\n"
                                         % (base, line, resp))
                else:
                    skipped += 1
            except DriverTimeout as dto:
                failed += 1
                rt.dead = True
                rt.close()
                sys.stderr.write("STALL: %s:%d: %s\n" % (base, line, dto))
                break
            except Exception as exc:
                failed += 1
                sys.stderr.write("FAIL: %s:%d: driver error: %s\n"
                                 % (base, line, exc))

        rt.close()
        print("%s: %d passed, %d failed, %d skipped" % (base, passed,
                                                        failed, skipped))
        return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
