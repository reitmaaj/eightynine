#!/usr/bin/env python3
"""Verify the CLI survives writing to a pipe with no reader.

The write end is passed as the CLI's fd and the read end is closed, so the
write fails. With SIGPIPE ignored the CLI must report a write failure and
exit nonzero instead of dying by signal.

Usage: sigpipe.py <cli>
"""
import os
import subprocess
import sys


def main():
    if len(sys.argv) < 2:
        sys.exit(2)
    cli = sys.argv[1]
    read_end, write_end = os.pipe()
    os.close(read_end)
    proc = subprocess.run(
        [cli, str(write_end), "echo"],
        pass_fds=[write_end],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    os.close(write_end)
    if proc.returncode < 0:
        print(
            "sigpipe: FAIL: killed by signal %d" % -proc.returncode,
            file=sys.stderr,
        )
        return 1
    if proc.returncode == 0:
        print("sigpipe: FAIL: expected nonzero exit", file=sys.stderr)
        return 1
    if proc.stdout.strip():
        print("sigpipe: FAIL: emitted result", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
