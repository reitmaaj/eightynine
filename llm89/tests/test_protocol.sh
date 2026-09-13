#!/bin/sh -eu
# test_protocol.sh - start the local test server, run the protocol test binary.
# Usage: test_protocol.sh <path-to-binary>

BIN="$1"
PORT=$(( 20000 + ($$ % 10000) ))
LOG="$(mktemp)"
SERVER="$(dirname "$0")/../scripts/test_server.py"

python3 "$SERVER" "$PORT" >"$LOG" 2>&1 &
SRV=$!
trap 'kill "$SRV" 2>/dev/null || true' EXIT

i=0
until python3 -c "import socket,sys; s=socket.socket(); s.settimeout(0.1); s.connect(('127.0.0.1', $PORT)); s.close()" 2>/dev/null; do
    i=$((i + 1))
    if [ "$i" -gt 50 ]; then
        echo "test server failed to start" >&2
        cat "$LOG" >&2
        exit 1
    fi
    sleep 0.1
done

"$BIN" "$PORT"
