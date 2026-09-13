#!/bin/sh -eu
# Error test: the server replies with a reserved server error; the CLI must
# print a structured error line.
cli="$1"

out="$(python3 test/e2e/mock_server.py "$cli" fail)"
case "$out" in
    'error -32001 reserved "boom"')
        echo "error: OK"
        ;;
    *)
        echo "error: FAIL: unexpected output: $out" >&2
        exit 1
        ;;
esac
