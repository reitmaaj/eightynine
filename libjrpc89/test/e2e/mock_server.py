#!/usr/bin/env python3
"""Test harness: speak JSON-RPC 2.0 to the libjrpc89 CLI over a Unix socket.

For method "fail" it replies with a reserved server error, for "wrongid" with
a mismatched id; otherwise it echoes the params as the result.

Usage: mock_server.py <cli> <method> [params-json]
"""
import json
import sys

import harness


def main():
    if len(sys.argv) < 3:
        sys.exit(2)
    cli = sys.argv[1]
    method = sys.argv[2]
    params = sys.argv[3] if len(sys.argv) >= 4 else None

    proc, server_end = harness.spawn_cli(cli, method, params)
    req = harness.read_frame(server_end)
    req_obj = json.loads(req.decode("utf-8"))
    rid = req_obj.get("id")
    if req_obj.get("method") == "fail":
        resp = {
            "jsonrpc": "2.0",
            "error": {"code": -32001, "message": "boom"},
            "id": rid,
        }
    elif req_obj.get("method") == "wrongid":
        resp = {"jsonrpc": "2.0", "result": 1, "id": rid + 1000}
    else:
        resp = {"jsonrpc": "2.0", "result": req_obj.get("params"), "id": rid}
    harness.send_json(server_end, resp)

    out, err, rc = harness.finish(proc, server_end)
    if out:
        sys.stdout.write(out)
    if err:
        sys.stderr.write(err)
    return rc


if __name__ == "__main__":
    sys.exit(main())
