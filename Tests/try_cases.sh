#!/bin/sh
# expr? in main: an error there ends the program -- "error: <message>" on
# stderr, after whatever it printed first, and exit status 1. Tests/Lang can't
# say that (run_tests.sh wants exit status 0), so it is said here.
#
# Usage: Tests/try_cases.sh [path-to-NexaC]     (run from the repo root)

set -u

NEXAC="${1:-./NexaC}"
if [ ! -x "$NEXAC" ]; then
    echo "FAIL: NexaC not found or not executable: $NEXAC"
    exit 1
fi

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT INT TERM
fails=0

cat > "$WORK/main_err.nxa" <<'EOF'
#include <std/io>
fn half(n: int): Result[int] {
    if (n % 2 != 0) { return err(f"{n} is odd"); }
    return ok(n / 2);
}
fn main() {
    io.println("got ", half(10)?);
    io.println("got ", half(7)?);
    io.println("never printed");
}
EOF

if "$NEXAC" "$WORK/main_err.nxa" -o "$WORK/main_err" > "$WORK/build.log" 2>&1; then
    bin="$WORK/main_err"
    [ -x "$bin" ] || bin="$WORK/main_err.exe"
    "$bin" > "$WORK/out.txt" 2> "$WORK/err.txt"
    rc=$?
    out=$(tr -d '\r' < "$WORK/out.txt")
    err=$(tr -d '\r' < "$WORK/err.txt")
    if [ "$rc" = 1 ]; then echo "ok main_exit_status"; else echo "FAIL main_exit_status: $rc, want 1"; fails=$((fails + 1)); fi
    if [ "$out" = "got 5" ]; then echo "ok main_stdout"; else echo "FAIL main_stdout: [$out]"; fails=$((fails + 1)); fi
    if [ "$err" = "error: 7 is odd" ]; then echo "ok main_stderr"; else echo "FAIL main_stderr: [$err]"; fails=$((fails + 1)); fi
else
    echo "FAIL build: NexaC could not build the program"
    sed 's/^/  /' "$WORK/build.log" | tail -5
    fails=$((fails + 1))
fi

if [ $fails -eq 0 ]; then
    echo "try ok"
    exit 0
fi
echo "try: $fails failure(s)"
exit 1
