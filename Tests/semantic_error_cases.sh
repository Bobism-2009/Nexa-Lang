#!/bin/sh
# Error-path cover for the transpiler's semantic checks (BOB-8).
#
# These cases cannot live in a .nxa test, because the point of each one is that
# NexaC refuses to compile it. Each case asserts three things:
#   1. NexaC exits non-zero,
#   2. it prints a "[Nexa] Error:" naming the source file and the right line,
#   3. no C++ compiler diagnostic leaks through to the user.
#
# The legal counterparts — shadowing, nested loops, enum variants, implicit
# numeric conversions — live in Tests/semantic_checks_test.nxa.
#
# Usage: Tests/semantic_error_cases.sh [path-to-NexaC]    (run from the repo root)

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

# expect_error <label> <expected-substring> <source-text>
expect_error() {
    label=$1
    want=$2
    src=$3

    printf '%s' "$src" > "$WORK/case.nxa"
    out=$("$NEXAC" "$WORK/case.nxa" --source "$WORK/case.cpp" 2>&1)
    status=$?

    if [ $status -eq 0 ]; then
        echo "FAIL $label: NexaC exited 0, expected a compile error"
        fails=$((fails + 1))
        return
    fi
    case $out in
        *"$want"*) ;;
        *)
            echo "FAIL $label: expected \"$want\""
            echo "  got: $out"
            fails=$((fails + 1))
            return
            ;;
    esac
    # The whole point of diagnosing at the Nexa level is that clang never speaks.
    case $out in
        *"error:"*)
            echo "FAIL $label: a raw C++ compiler error leaked through"
            echo "  got: $out"
            fails=$((fails + 1))
            return
            ;;
    esac
    case $out in
        *case.nxa*) ;;
        *)
            echo "FAIL $label: diagnostic does not name the source file"
            echo "  got: $out"
            fails=$((fails + 1))
            return
            ;;
    esac
    echo "ok $label"
}

# expect_ok <label> <source-text>
expect_ok() {
    label=$1
    src=$2

    printf '%s' "$src" > "$WORK/case.nxa"
    out=$("$NEXAC" "$WORK/case.nxa" --source "$WORK/case.cpp" 2>&1)
    if [ $? -ne 0 ]; then
        echo "FAIL $label: expected this to transpile, but NexaC refused"
        echo "  got: $out"
        fails=$((fails + 1))
        return
    fi
    echo "ok $label"
}

# --- undefined names: used to be emitted verbatim and left to clang ---------
expect_error "undefined in call" \
    "Undefined variable 'nope' at line 3" \
    '#include <std/io>
fn main() {
    io.println(nope);
}
'

expect_error "undefined in expression" \
    "Undefined variable 'missing' at line 4" \
    '#include <std/io>
fn main() {
    let a = 1;
    let b = a + missing;
    io.println(b);
}
'

expect_error "undefined assignment target" \
    "Undefined variable 'never' at line 3" \
    '#include <std/io>
fn main() {
    never = 5;
    io.println(1);
}
'

expect_error "out of scope after its block" \
    "Undefined variable 'inner' at line 7" \
    '#include <std/io>
fn main() {
    if (true) {
        let inner = 1;
        io.println(inner);
    }
    io.println(inner);
}
'

expect_error "loop variable after the loop" \
    "Undefined variable 'i' at line 6" \
    '#include <std/io>
fn main() {
    for (i, 3) {
        io.println(i);
    }
    io.println(i);
}
'

# --- redeclaration: used to reach clang as a C++ redefinition ---------------
expect_error "duplicate let" \
    "Redeclaration of '"'x'"' in the same scope at line 4" \
    '#include <std/io>
fn main() {
    let x = 1;
    let x = 2;
    io.println(x);
}
'

expect_error "duplicate global let" \
    "Redeclaration of '"'g'"' in the same scope at line 3" \
    '#include <std/io>
let g = 1;
let g = 2;
fn main() {
    io.println(g);
}
'

# Shadowing in a nested block is legal and must stay legal (regression guard
# for the block scoping fixed in e89f28f).
expect_ok "nested shadow still compiles" \
    '#include <std/io>
fn main() {
    let x = 1;
    if (true) {
        let x = 2;
        io.println(x);
    }
    io.println(x);
}
'

expect_ok "shadowing a parameter still compiles" \
    '#include <std/io>
fn f(x: int): int {
    let x = 9;
    return x;
}
fn main() {
    io.println(f(1));
}
'

# --- initializer type mismatches -------------------------------------------
expect_error "string into int" \
    "Type mismatch: '"'x'"' is declared '"'int'"' but the initializer is a string at line 3" \
    '#include <std/io>
fn main() {
    let x: int = "hello";
    io.println(x);
}
'

expect_error "int into string" \
    "Type mismatch" \
    '#include <std/io>
fn main() {
    let s: string = 42;
    io.println(s);
}
'

expect_error "concatenation into float" \
    "Type mismatch" \
    '#include <std/io>
fn main() {
    let f: float = "a" + "b";
    io.println(f);
}
'

expect_error "array into int" \
    "Type mismatch" \
    '#include <std/io>
fn main() {
    let n: int = [1, 2, 3];
    io.println(n);
}
'

expect_error "string variable into int" \
    "Type mismatch" \
    '#include <std/io>
fn main() {
    let s = "text";
    let n: int = s;
    io.println(n);
}
'

# Numeric conversions are implicit in Nexa and must not be diagnosed.
expect_ok "implicit numeric conversions still compile" \
    '#include <std/io>
fn main() {
    let a: float = 3;
    let b: char = 65;
    let c: long = 7;
    let d: int = 2;
    io.println((int)a + (int)b + (int)c + d);
}
'

# --- unknown struct fields --------------------------------------------------
expect_error "unknown field assignment" \
    "Struct '"'P'"' has no field '"'zz'"' at line 7" \
    '#include <std/io>
struct P {
    x: int;
}
fn main() {
    let p = P{x: 1};
    p.zz = 5;
    io.println(p.x);
}
'

expect_error "unknown field read" \
    "Struct '"'P'"' has no field '"'y'"'" \
    '#include <std/io>
struct P {
    x: int;
}
fn main() {
    let p = P{x: 1};
    let n: int = p.y;
    io.println(n);
}
'

expect_error "unknown field through a pointer" \
    "Struct '"'P'"' has no field '"'nope'"'" \
    '#include <std/io>
struct P {
    x: int;
}
fn main() {
    let p = P{x: 1};
    let q: *P = &p;
    q->nope = 2;
    io.println(p.x);
}
'

# --- break / continue outside a loop ---------------------------------------
expect_error "break in main" \
    "'"'break'"' outside of a loop or switch at line 3" \
    '#include <std/io>
fn main() {
    break;
}
'

expect_error "continue in main" \
    "'"'continue'"' outside of a loop at line 3" \
    '#include <std/io>
fn main() {
    continue;
}
'

expect_error "break in a function body" \
    "'"'break'"' outside of a loop or switch" \
    '#include <std/io>
fn f() {
    break;
}
fn main() {
    f();
    io.println(1);
}
'

expect_error "break inside an if, still no loop" \
    "'"'break'"' outside of a loop or switch" \
    '#include <std/io>
fn main() {
    if (true) {
        break;
    }
}
'

# A lambda is its own function: an enclosing loop does not reach into it.
expect_error "break in a lambda inside a loop" \
    "'"'break'"' outside of a loop or switch" \
    '#include <std/io>
fn main() {
    for (i, 3) {
        let f = fn(): int { break; };
        io.println(f());
    }
}
'

expect_error "continue inside a switch with no loop" \
    "'"'continue'"' outside of a loop" \
    '#include <std/io>
fn main() {
    switch (1) {
        case 1:
            continue;
    }
}
'

# --- the diagnostic must name the file the mistake is in, not the entry file --
printf '%s' 'fn helper(): int {
    io.println(ghost);
    return 1;
}
' > "$WORK/helper.nxa"
printf '%s' '#include <std/io>
#include "helper.nxa"
fn main() {
    io.println(helper());
}
' > "$WORK/entry.nxa"
out=$("$NEXAC" "$WORK/entry.nxa" --source "$WORK/entry.cpp" 2>&1)
if [ $? -eq 0 ]; then
    echo "FAIL included file: NexaC exited 0, expected a compile error"
    fails=$((fails + 1))
else
    case $out in
        *helper.nxa*"Undefined variable 'ghost' at line 2"*)
            echo "ok included file" ;;
        *)
            echo "FAIL included file: expected helper.nxa to be named at line 2"
            echo "  got: $out"
            fails=$((fails + 1)) ;;
    esac
fi

if [ $fails -ne 0 ]; then
    echo "semantic_error_cases: $fails failure(s)"
    exit 1
fi
echo "semantic_error_cases ok"
