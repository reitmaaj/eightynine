#!/bin/sh -eu
# PERF-001/002: non-tail recursion to depth 5000 must complete within an
# O(N) wall-clock bound. Current O(N^2) build takes ~30s and FAILS this;
# the iterative machine must make it fast. Guarded by `timeout` so a
# regression fails fast instead of hanging the suite.
# S2.3: this gate exercises the iterative driver, now the sole evaluation
# path (S2.6); the legacy recursive stepper is gone.

wasm89=./build/wasm89
rec=test/fixtures/rec.wasm
limit_ms=3000

start=$(date +%s%N 2>/dev/null || date +%s)
out=$(timeout 60 "$wasm89" repl <<EOF
module $rec
invoke last down 1 i32:0x1388
quit
EOF
)
rc=$?
if [ "$rc" -ne 0 ]; then
    printf 'FAIL: down(5000) did not complete (exit %s)\n' "$rc"
    exit 1
fi
end=$(date +%s%N 2>/dev/null || date +%s)
if [ "$end" -ge "$start" ] && [ "${#start}" -gt 10 ]; then
    elapsed_ms=$(( (end - start) / 1000000 ))
else
    elapsed_ms=$(( (end - start) * 1000 ))
fi

# The invocation must have returned normally (no exhaustion, no error).
if ! printf '%s\n' "$out" | grep -qx '@return'; then
    printf 'FAIL: down(5000) did not return normally:\n%s\n' "$out"
    exit 1
fi
if [ "$elapsed_ms" -gt "$limit_ms" ]; then
    printf 'FAIL: down(5000) took %s ms (limit %s ms) - non-tail recursion is not O(N)\n' \
        "$elapsed_ms" "$limit_ms"
    exit 1
fi
printf 'PASS: down(5000) completed in %s ms\n' "$elapsed_ms"
