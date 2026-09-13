#!/bin/sh -eu
# Smoke test: one request/response round trip over a Unix socket.
cli="$1"

out="$(python3 test/e2e/mock_server.py "$cli" echo '{"x":1}')"
case "$out" in
    '{"x":1}')
        echo "smoke: OK"
        ;;
    *)
        echo "smoke: FAIL: unexpected output: $out" >&2
        exit 1
        ;;
esac
