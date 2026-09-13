#!/bin/sh -eu
# Id-mismatch test: the server replies with an id different from the request;
# the CLI must report a mismatch and exit nonzero instead of printing a result.
cli="$1"

out="$(python3 test/e2e/mock_server.py "$cli" wrongid '{"x":1}' 2>&1 || true)"
case "$out" in
    *mismatch*)
        echo "mismatch: OK"
        ;;
    *)
        echo "mismatch: FAIL: unexpected output: $out" >&2
        exit 1
        ;;
esac
