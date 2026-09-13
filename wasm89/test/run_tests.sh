#!/bin/sh -eu
cc="$1"
flags="$2"
./build/test_version
./build/test_leb
./build/test_numeric
./build/test_float
./build/test_conv
./build/test_decode
./build/test_validate
./build/test_eval
./build/test_calls
./build/test_exn
./build/test_instantiate
./test/smoke_test.sh
./test/repl_smoke.sh
./test/c89_probe.sh "$cc" "$flags"
./test/perf_on2.sh
python3 test/test_sweep.py
