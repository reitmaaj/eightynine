#!/bin/sh -eu
# e2e: CLI terminal-state handling (trap, budget, helper, usage).
bpf="$1"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# A program that traps: r1 = 5000; LDX B r0 = mem[5000] (beyond 4096-byte mem).
printf '\xb7\x01\x00\x00\x88\x13\x00\x00\x71\x10\x00\x00\x00\x00\x00\x00\x95\x00\x00\x00\x00\x00\x00\x00' \
    >"$tmp/trap.bpf"
if "$bpf" run "$tmp/trap.bpf" >/dev/null 2>&1; then
    echo "FAIL: trap program accepted" >&2
    exit 1
fi

# An infinite self-jump must be rejected by the budget.
printf '\x05\x00\x00\x00\xff\xff\xff\xff' >"$tmp/loop.bpf"
if "$bpf" run "$tmp/loop.bpf" >/dev/null 2>&1; then
    echo "FAIL: infinite loop accepted" >&2
    exit 1
fi

# A helper call is unsupported (no host).
printf '\x85\x00\x00\x00\x07\x00\x00\x00\x95\x00\x00\x00\x00\x00\x00\x00' \
    >"$tmp/helper.bpf"
if "$bpf" run "$tmp/helper.bpf" >/dev/null 2>&1; then
    echo "FAIL: helper call accepted" >&2
    exit 1
fi

# Invalid register (r11) must be rejected by validation.
printf '\xb7\x00\x00\x00\x2a\x00\x00\x00\xb7\x0b\x00\x00\x01\x00\x00\x00' \
    >"$tmp/reg.bpf"
if "$bpf" run "$tmp/reg.bpf" >/dev/null 2>&1; then
    echo "FAIL: invalid register accepted" >&2
    exit 1
fi

# Missing subcommand prints usage and exits nonzero.
if "$bpf" >/dev/null 2>&1; then
    echo "FAIL: no-arg accepted" >&2
    exit 1
fi
if "$bpf" bogus file >/dev/null 2>&1; then
    echo "FAIL: unknown subcommand accepted" >&2
    exit 1
fi

echo "PASS: bpf terminal-state handling"
