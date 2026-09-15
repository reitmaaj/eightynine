#!/bin/sh -eu
# SIGPIPE policy: the CLI must not die when the peer is gone.
cli="$1"

python3 test/e2e/sigpipe.py "$cli"
echo "sigpipe: OK"
