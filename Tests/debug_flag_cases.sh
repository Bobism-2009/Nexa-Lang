#!/bin/sh
# Cover for --debug / -g (BOB-13): debuggable binaries instead of the stripped release build.
#
# These cases cannot live in a .nxa test, because what they assert is not what the
# program prints but what the *binary* contains (debug sections, an unmangled symbol)
# and which flag combinations NexaC refuses.
#
# Requires a C++ toolchain (NexaC shells out to clang++/g++). The ELF inspections are
# skipped automatically when neither readelf nor objdump is installed, and on non-ELF
# hosts (macOS/Windows), so the flag-level cases still run everywhere.
#
# Usage: Tests/debug_flag_cases.sh [path-to-NexaC]     (run from the repo root)

set -u

NEXAC="${1:-./NexaC}"
if [ ! -x "$NEXAC" ]; then
    echo "FAIL: NexaC not found or not executable: $NEXAC"
    exit 1
fi
NEXAC=$(cd "$(dirname "$NEXAC")" && pwd)/$(basename "$NEXAC")

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT INT TERM

fails=0
pass() { echo "ok $1"; }
fail() {
    echo "FAIL $1: $2"
    [ $# -ge 3 ] && echo "  got: $3"
    fails=$((fails + 1))
}

# A program with a function whose name a debugger must be able to break on.
cat > "$WORK/prog.nxa" <<'NXA'
#include <std/io>

fn compute_total(a: int, b: int): int {
    let sum = a + b;
    return sum * 2;
}

fn main() {
    io.println("total=", compute_total(3, 4));
}
NXA

# ELF inspection is optional: pick whichever tool exists.
elf_has_debug_info() {
    if command -v readelf >/dev/null 2>&1; then
        readelf -S "$1" 2>/dev/null | grep -q '\.debug_info'
    elif command -v objdump >/dev/null 2>&1; then
        objdump -h "$1" 2>/dev/null | grep -q '\.debug_info'
    else
        return 2
    fi
}

elf_has_symbol() {
    if command -v readelf >/dev/null 2>&1; then
        readelf -sW "$1" 2>/dev/null | grep -q "$2"
    elif command -v objdump >/dev/null 2>&1; then
        objdump -t "$1" 2>/dev/null | grep -q "$2"
    else
        return 2
    fi
}

# --- 1. --debug produces debug info; the default build does not --------------
out=$("$NEXAC" "$WORK/prog.nxa" --debug -o "$WORK/dbg" 2>&1)
if [ $? -ne 0 ]; then
    fail "debug build" "NexaC exited non-zero" "$out"
elif [ ! -f "$WORK/dbg" ]; then
    fail "debug build" "no binary at $WORK/dbg" "$out"
else
    pass "debug build succeeds"

    elf_has_debug_info "$WORK/dbg"
    case $? in
        0) pass "debug binary carries .debug_info" ;;
        2) echo "skip .debug_info check (no readelf/objdump)" ;;
        *) fail "debug binary" "no .debug_info section: --debug did not reach the compiler, or stripping still ran" ;;
    esac

    # --preserve-names is implied, so the Nexa name survives into the symbol table
    # (C++-mangled, which is exactly what `break compute_total` resolves through).
    elf_has_symbol "$WORK/dbg" "compute_total"
    case $? in
        0) pass "debug binary keeps the Nexa function name" ;;
        2) echo "skip symbol check (no readelf/objdump)" ;;
        *) fail "debug binary" "compute_total is absent: --debug did not imply --preserve-names, or the symbol table was stripped" ;;
    esac

    # The binary's line info names this file; deleting it would leave a debugger
    # with line numbers and nothing to show.
    if [ -f "$WORK/dbg.debug.cpp" ]; then
        pass "debug build keeps the generated C++"
    else
        fail "debug build" "generated C++ was deleted (expected $WORK/dbg.debug.cpp)"
    fi

    got=$("$WORK/dbg" 2>&1)
    if [ "$got" = "total=14" ]; then
        pass "debug binary runs correctly"
    else
        fail "debug binary" "wrong output" "$got"
    fi
fi

out=$("$NEXAC" "$WORK/prog.nxa" -o "$WORK/rel" 2>&1)
if [ $? -ne 0 ]; then
    fail "release build" "NexaC exited non-zero" "$out"
else
    elf_has_debug_info "$WORK/rel"
    case $? in
        0) fail "release build" "default build carries .debug_info; it must stay stripped" ;;
        2) echo "skip release .debug_info check (no readelf/objdump)" ;;
        *) pass "default build stays stripped" ;;
    esac
fi

# --- 2. -g is an alias for --debug -------------------------------------------
out=$("$NEXAC" "$WORK/prog.nxa" -g -o "$WORK/galias" 2>&1)
if [ $? -ne 0 ]; then
    fail "-g alias" "NexaC exited non-zero" "$out"
else
    elf_has_debug_info "$WORK/galias"
    case $? in
        0) pass "-g is an alias for --debug" ;;
        2) echo "skip -g alias check (no readelf/objdump)" ;;
        *) fail "-g alias" "-g produced a binary without .debug_info" ;;
    esac
fi

# --- 3. --debug --run runs the program and keeps the binary -------------------
# Regression: the run target used to be a bare relative name, which no shell
# resolves from the current directory ("sh: 1: prog: not found").
out=$("$NEXAC" "$WORK/prog.nxa" --debug --run -o "$WORK/runme" 2>&1)
case $out in
    *total=14*) pass "--debug --run runs the program" ;;
    *) fail "--debug --run" "program output missing" "$out" ;;
esac
if [ -f "$WORK/runme" ]; then
    pass "--debug --run keeps the binary"
else
    fail "--debug --run" "binary was deleted; a debug build exists to be debugged afterwards"
fi

# --- 4. contradictory / unsupported combinations are refused -----------------
# expect_reject <label> <expected-substring> <args...>
expect_reject() {
    label=$1
    want=$2
    shift 2
    out=$("$NEXAC" "$WORK/prog.nxa" "$@" -o "$WORK/nope" 2>&1)
    if [ $? -eq 0 ]; then
        fail "$label" "NexaC exited 0, expected a refusal" "$out"
        return
    fi
    case $out in
        *"$want"*) pass "$label" ;;
        *) fail "$label" "expected \"$want\"" "$out" ;;
    esac
}

expect_reject "--debug --small is rejected" "--debug and --small are contradictory" --debug --small
expect_reject "--debug --wasm is rejected" "--debug is not supported with --wasm" --debug --wasm

if [ $fails -eq 0 ]; then
    echo "All --debug cases passed."
    exit 0
fi
echo "$fails --debug case(s) failed."
exit 1
