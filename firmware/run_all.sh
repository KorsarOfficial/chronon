#!/usr/bin/env bash
# Build and run all firmware test cases. Verify expected register state.
set -e
cd "$(dirname "$0")"

EMU=../build/cortex-m.exe
PASS=0; FAIL=0

run_test() {
    local name="$1"; shift
    local expect="$1"; shift
    cd "$name"
    bash build.sh > build.log 2>&1
    cd ..
    local out
    out=$("$EMU" "$name/$name.bin" 2>&1)
    if echo "$out" | grep -q "$expect"; then
        echo "PASS $name  ($(echo "$out" | head -1))"
        PASS=$((PASS+1))
    else
        echo "FAIL $name"
        echo "  expected: $expect"
        echo "  output:"
        echo "$out" | sed 's/^/    /'
        FAIL=$((FAIL+1))
    fi
}

# test1: fib(10) = 55 = 0x37 in R0
run_test test1 "R0=00000037"

# test2: bubble sort + sum=40 + fact(6)=720, R0=(40<<16)|720, R1=arr[0]=1, R2=arr[7]=9
run_test test2 "R0=002802d0 R1=00000001 R2=00000009"

echo ""
echo "===== $PASS passed, $FAIL failed ====="
[ $FAIL -eq 0 ]
