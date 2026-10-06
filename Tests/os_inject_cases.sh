#!/bin/sh
# os.inject_dll and os.alloc_console: slice emission and basic behaviour.
#
# Usage: Tests/os_inject_cases.sh [path-to-NexaC]

set -u

NEXAC="${1:-./NexaC}"
case "$NEXAC" in
    /*) ;;
    *) NEXAC="$(pwd)/$NEXAC" ;;
esac
WORK=$(mktemp -d 2>/dev/null || mktemp -d -t nexainject)
trap 'rm -rf "$WORK"' EXIT
FAIL=0

ok() { echo "ok   $1"; }
bad() { FAIL=$((FAIL + 1)); echo "FAIL $1: $2"; }

WINDOWS=0
case "$(uname -s 2>/dev/null)" in
    MINGW*|MSYS*|CYGWIN*) WINDOWS=1 ;;
esac
EXE=""
[ "$WINDOWS" = 1 ] && EXE=".exe"

cat > "$WORK/slice.nxa" <<'EOF'
#include <std/io>
fn main() {
    io.println("hi");
}
EOF
if ! "$NEXAC" "$WORK/slice.nxa" --source "$WORK/slice.cpp" > "$WORK/slice.log" 2>&1; then
    bad "slice transpile" "$(cat "$WORK/slice.log")"
elif grep -q __nexa_os_inject_dll "$WORK/slice.cpp" 2>/dev/null; then
    bad "slice" "inject helper emitted without os.inject_dll"
elif grep -q __nexa_os_alloc_console "$WORK/slice.cpp" 2>/dev/null; then
    bad "slice" "alloc_console helper emitted without os.alloc_console"
else
    ok "programs without inject omit helpers"
fi

cat > "$WORK/use.nxa" <<'EOF'
#include <std/os>
fn main() {
    let a = os.inject_dll(0, "x.dll");
    let b = os.alloc_console();
}
EOF
if ! "$NEXAC" "$WORK/use.nxa" --source "$WORK/use.cpp" > "$WORK/use.log" 2>&1; then
    bad "use transpile" "$(cat "$WORK/use.log")"
elif ! grep -q __nexa_os_inject_dll "$WORK/use.cpp" 2>/dev/null; then
    bad "use" "missing __nexa_os_inject_dll"
elif ! grep -q __nexa_os_inject_dll_path "$WORK/use.cpp" 2>/dev/null; then
    bad "use" "missing relative dll path helper"
elif ! grep -q __nexa_os_alloc_console "$WORK/use.cpp" 2>/dev/null; then
    bad "use" "missing __nexa_os_alloc_console"
else
    ok "inject and alloc_console helpers emitted when used"
fi

cat > "$WORK/simple.nxa" <<'EOF'
#include <std/os>
fn main() {
    let ok = os.inject("notepad.exe", "hook.dll");
}
EOF
if ! "$NEXAC" "$WORK/simple.nxa" --source "$WORK/simple.cpp" > "$WORK/simple.log" 2>&1; then
    bad "simple transpile" "$(cat "$WORK/simple.log")"
elif ! grep -q __nexa_os_inject "$WORK/simple.cpp" 2>/dev/null; then
    bad "simple" "missing __nexa_os_inject wrapper"
else
    ok "os.inject emits one-call wrapper"
fi

if [ "$WINDOWS" = 1 ]; then
    cat > "$WORK/console.nxa" <<'EOF'
#include <std/io>
#include <std/os>
fn main() {
    if (os.alloc_console() != 1) {
        io.println("no console");
        return;
    }
    io.println("console ok");
}
EOF
    if ! "$NEXAC" "$WORK/console.nxa" -o "$WORK/console$EXE" > "$WORK/console.log" 2>&1; then
        bad "console build" "$(cat "$WORK/console.log")"
    else
        out=$("$WORK/console$EXE" 2>&1)
        if [ "$out" = "console ok" ]; then
            ok "alloc_console enables io.println"
        else
            bad "console run" "expected 'console ok', got '$out'"
        fi
    fi
fi

if [ "$FAIL" -eq 0 ]; then
    echo "os_inject_cases: all passed"
    exit 0
fi
echo "os_inject_cases: $FAIL failed"
exit 1
