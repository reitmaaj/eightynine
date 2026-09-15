#!/usr/bin/env python3
"""Parameterized e2e suite of JSON-RPC 2.0 exchanges against the libjrpc89 CLI.

Covers at least 100 passing exchanges (valid responses, exit 0, result echoed)
and at least 100 failing exchanges (valid server errors reported as a
structured error, or malformed/framing/mismatch/request failures rejected with
a nonzero exit). Each case runs the CLI over a real Unix socketpair.

Usage: suite.py <cli>
Exit 0 only if every case passes and the >=100/100 thresholds are met.
"""

import json
import sys

import harness


def reserved(code):
    return -32768 <= code <= -32000


# ---- server play functions: play(sock, req, arg) -----------------------


def play_json(sock, _req, obj):
    harness.send_json(sock, obj)


def play_raw(sock, _req, text):
    harness.send_bytes(sock, (text + "\n").encode("utf-8"))


def play_error(sock, req, arg):
    if not req:
        sock.close()
        return
    code, message, has_data = arg
    error_obj = {"code": code, "message": message}
    if has_data:
        error_obj["data"] = {"d": 1}
    r = json.loads(req)
    harness.send_json(
        sock,
        {"jsonrpc": "2.0", "error": error_obj, "id": r.get("id")},
    )


def play_echo_result(sock, req, result):
    if not req:
        sock.close()
        return
    r = json.loads(req)
    harness.send_json(
        sock, {"jsonrpc": "2.0", "result": result, "id": r.get("id")}
    )


def play_echo_params(sock, req, _result):
    if not req:
        sock.close()
        return
    r = json.loads(req)
    harness.send_json(
        sock, {"jsonrpc": "2.0", "result": r.get("params"), "id": r.get("id")}
    )


def play_wrongid(sock, req, arg):
    if not req:
        sock.close()
        return
    wrong = arg
    r = json.loads(req)
    rid = r.get("id")
    if isinstance(rid, int):
        rid = rid + 1000 if wrong == "int" else "x"
    harness.send_json(sock, {"jsonrpc": "2.0", "result": 1, "id": rid})


def play_truncated(sock, _req, _arg):
    harness.send_bytes(sock, b"abc")


def play_oversized(sock, _req, n):
    harness.send_bytes(sock, (b"a" * n) + b"\n")


def play_eof(sock, _req, _arg):
    sock.close()


# ---- result checks: check(out, err, rc, arg) ---------------------------


def check_result(out, _err, rc, expected):
    if rc != 0:
        return "rc=%d expected 0" % rc
    try:
        got = json.loads(out)
    except ValueError:
        return "stdout not json: %r" % out
    if got != expected:
        return "stdout %r != %r" % (got, expected)
    return None


def check_error(out, _err, rc, arg):
    code, message, _has_data = arg
    if rc != 0:
        return "rc=%d expected 0" % rc
    cls = "reserved" if reserved(code) else "application"
    expected = 'error %d %s "%s"' % (code, cls, message)
    if out.strip() != expected:
        return "stdout %r != %r" % (out, expected)
    return None


def check_failure(out, _err, rc, _arg):
    if rc == 0:
        return "rc=0 expected nonzero"
    if out.strip():
        return "emitted result on failure: %r" % out
    return None


def run_case(cli, method, params, play, play_arg, check):
    proc, sock = harness.spawn_cli(cli, method, params)
    req = harness.read_frame(sock)
    play(sock, req, play_arg)
    out, err, rc = harness.finish(proc, sock)
    return check(out, err, rc, play_arg)


# ---- scenario tables -----------------------------------------------------


def build_passing():
    cases = []
    results = [None, True, False]
    results += [0, 1, -1, 7, -7, 42, 1000, -1000,
                2 ** 30, 2 ** 52, 2 ** 53 - 1, -(2 ** 53 - 1)]
    results += [1.5, -0.5, 123.456, 0.0, -1.25]
    results += ["", "a", "z", "hello world", "héllo ☃",
                "line1\nline2\ttab", '"quoted"', "x" * 4000]
    results += [[], [1], [1, 2, 3], ["a", "b"], ["x", "y", "z"],
                [1, "x", True, None, {"k": 1}], [[1, 2], [3, [4, 5]]],
                list(range(10)), list(range(50))]
    results += [{"k": 1}, {"a": {"b": {"c": 1}}},
                {"a": [1, 2], "b": {"c": [3, 4]}},
                {"héllo": "☃", "a": None}, [{"a": 1}, {"b": 2}],
                [[], [], {}], {("n%d" % i): i for i in range(10)}]
    results += [[i] for i in range(20)]
    results += ["value%d" % i for i in range(15)]
    results += [i * 10 for i in range(1, 15)]
    for i, res in enumerate(results):
        cases.append(("pass:result:%d" % i, "echo", None,
                      play_echo_result, res, check_result))

    params_list = [
        "[]", "[1,2,3]", "[1,\"x\",null]", "[1,2,3,4,5]", "[[],[]]",
        "[true,false,null]",
        "{}", '{"a":1}', '{"a":{"b":[1,2]}}', '[{"a":1},{"b":2}]',
        '{"a":[1,2],"b":3}', '{"k":[1,2,3]}',
    ]
    for i, p in enumerate(params_list):
        expected = json.loads(p)
        cases.append(("pass:params:%d" % i, "echo", p,
                      play_echo_params, None,
                      lambda o, e, r, a, v=expected: check_result(o, e, r, v)))
    cases.append(("pass:noparams", "echo", None, play_echo_params, None,
                  lambda o, e, r, a: check_result(o, e, r, None)))

    sizes = [1, 8, 16, 64, 255, 1024, 4096, 8000, 8155]
    for n in sizes:
        payload = "a" * n
        cases.append(("pass:size:%d" % n, "echo", None,
                      play_echo_result, payload,
                      lambda o, e, r, v=payload: check_result(o, e, r, v)))

    methods = ["m", "add", "do_something", "proc.alpha", "héllo",
               "m" * 400, "foo.bar.baz", "g()"]
    for i, m in enumerate(methods):
        cases.append(("pass:method:%d" % i, m, None, play_echo_result, "ok",
                      lambda o, e, r, a: check_result(o, e, r, "ok")))
    cases.append(("pass:emptymethod", "", None, play_echo_result, "ok",
                  lambda o, e, r, a: check_result(o, e, r, "ok")))
    return cases


def build_failing():
    cases = []
    codes = [-32700, -32600, -32601, -32602, -32603,
             -32000, -32099, -32050, -1, 0, 1, 1234, 2 ** 30]
    messages = ["boom", "", "unicode ☃", "x" * 300]
    for code in codes:
        for has_data in (False, True):
            for message in messages:
                arg = (code, message, has_data)
                cases.append(("fail:error:%d:%d:%s" % (code, has_data, len(message)),
                              "echo", None, play_error, arg, check_error))

    raw_bad = [
        "[]",
        "\"x\"",
        "123",
        "null",
        '{"jsonrpc":"1.0","result":1,"id":1}',
        '{"jsonrpc":"2","result":1,"id":1}',
        '{"jsonrpc":2.0,"result":1,"id":1}',
        '{"result":1,"id":1}',
        '{"jsonrpc":"2.0","result":1,"error":{"code":1},"id":1}',
        '{"jsonrpc":"2.0","id":1}',
        '{"jsonrpc":"2.0","result":1}',
        '{"jsonrpc":"2.0","error":123,"id":1}',
        '{"jsonrpc":"2.0","error":"x","id":1}',
        '{"jsonrpc":"2.0","error":[],"id":1}',
        '{"jsonrpc":"2.0","error":null,"id":1}',
        "not json at all",
        '{"jsonrpc":"2.0","result":1,"id":1} trailing',
        '{"jsonrpc":"2.0","result":1,"id":1',
        '{"jsonrpc":"2.0","error":{"message":"x"},"id":1}',
        '{"jsonrpc":"2.0","error":{"code":"x","message":"y"},"id":1}',
        '{"jsonrpc":"2.0","error":{"code":1},"id":1}',
        '{"jsonrpc":"2.0","error":{"code":1,"message":5},"id":1}',
    ]
    for i, text in enumerate(raw_bad):
        cases.append(("fail:malformed:%d" % i, "echo", None,
                      play_raw, text, check_failure))

    cases.append(("fail:truncated", "echo", None, play_truncated, None,
                  check_failure))
    cases.append(("fail:oversized:frame", "echo", None, play_oversized, 8192,
                  check_failure))
    cases.append(("fail:oversized:big", "echo", None, play_oversized, 16384,
                  check_failure))
    cases.append(("fail:eof", "echo", None, play_eof, None, check_failure))
    cases.append(("fail:invalidparams", "echo", "not json", play_echo_params,
                  None, check_failure))
    scalar_params = ["null", "true", "false", "0", "1", "-1", "42", "1.5",
                     "1e3", "-0.25", "123", "-7",
                     '"x"', '""', '"héllo ☃"', '"a b c"', '"x y z"']
    for i, p in enumerate(scalar_params):
        cases.append(("fail:scalarparams:%d" % i, "echo", p, play_echo_params,
                      None, check_failure))
    cases.append(("fail:wrongid:int", "echo", None, play_wrongid, "int",
                  check_failure))
    cases.append(("fail:wrongid:type", "echo", None, play_wrongid, "string",
                  check_failure))
    return cases


def run_table(cli, table):
    results = []
    for label, method, params, play, play_arg, check in table:
        try:
            errmsg = run_case(cli, method, params, play, play_arg, check)
        except Exception as exc:  # noqa: BLE001 - report and continue
            errmsg = "exception: %r" % exc
        results.append((label, errmsg))
    return results


def main():
    if len(sys.argv) < 2:
        sys.exit(2)
    cli = sys.argv[1]
    passing = run_table(cli, build_passing())
    failing = run_table(cli, build_failing())
    pass_fail = [label for label, msg in passing if msg is not None]
    fail_fail = [label for label, msg in failing if msg is not None]
    print("e2e-suite: %d passing, %d failing" % (len(passing), len(failing)))
    print("e2e-suite: pass-mismatches=%d fail-mismatches=%d"
          % (len(pass_fail), len(fail_fail)))
    for label, msg in passing + failing:
        if msg is not None:
            print("  FAIL %s: %s" % (label, msg))
    ok = True
    if pass_fail:
        ok = False
    if fail_fail:
        ok = False
    if len(passing) < 100:
        print("e2e-suite: fewer than 100 passing cases (%d)" % len(passing))
        ok = False
    if len(failing) < 100:
        print("e2e-suite: fewer than 100 failing cases (%d)" % len(failing))
        ok = False
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
