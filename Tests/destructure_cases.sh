#!/bin/sh
# let Point { x, y } = p; binds the fields as locals.
#
# Usage: Tests/destructure_cases.sh [path-to-NexaC]   (from the repo root)

set -u
NEXAC="${1:-./NexaC}"
case "$NEXAC" in
    /*|[A-Za-z]:*) ;;
    *) NEXAC="$(pwd)/$NEXAC" ;;
esac
WORK=$(mktemp -d 2>/dev/null || mktemp -d -t nexadest)
trap 'rm -rf "$WORK"' EXIT
FAIL=0

bad() { echo "FAIL $1: $2"; FAIL=$((FAIL + 1)); }
ok() { echo "ok   $1"; }

run_eq() {
    label=$1
    src=$2
    want=$3
    printf '%s\n' "$src" > "$WORK/p.nxa"
    out=$("$NEXAC" "$WORK/p.nxa" -o "$WORK/p" 2>&1)
    if [ $? -ne 0 ]; then
        bad "$label" "$out"
        return
    fi
    bin="$WORK/p"
    [ -f "$bin" ] || bin="$WORK/p.exe"
    got=$(printf '%s' "$("$bin" 2>&1)" | tr -d '\r')
    if [ "$got" = "$want" ]; then
        ok "$label"
    else
        bad "$label" "$got"
    fi
}

expect_has() {
    label=$1
    src=$2
    want=$3
    printf '%s\n' "$src" > "$WORK/bad.nxa"
    out=$("$NEXAC" "$WORK/bad.nxa" --source "$WORK/bad.cpp" 2>&1)
    if [ $? -eq 0 ]; then
        bad "$label" "NexaC accepted it"
        return
    fi
    case $out in
        *"$want"*) ok "$label" ;;
        *) bad "$label" "$out" ;;
    esac
}

run_eq "fields become locals" \
'#include <std/io>
struct Point { x: int; y: int; }
fn main(): int {
    let p = Point { x: 3, y: 4 };
    let Point { x, y } = p;
    io.println(x);
    io.println(y);
    return 0;
}' "$(printf '3\n4')"

run_eq "a partial bind is fine" \
'#include <std/io>
struct Point { x: int; y: int; }
fn main(): int {
    let p = Point { x: 7, y: 9 };
    let Point { x, } = p;
    io.println(x);
    return 0;
}' "7"

run_eq "the value is evaluated once" \
'#include <std/io>
struct Point { x: int; y: int; }
fn make(): Point {
    io.println("once");
    return Point { x: 1, y: 2 };
}
fn main(): int {
    let Point { x, y } = make();
    io.println(x + y);
    return 0;
}' "$(printf 'once\n3')"

printf '%s\n' \
'struct Point { x: int; y: int; }
fn origin(): Point { return Point { x: 0, y: 0 }; }' > "$WORK/lib.nxa"
printf '%s\n' \
'#include <std/io>
#include "lib.nxa"
fn main(): int {
    let lib.Point { x, y } = lib.origin();
    io.println(x);
    io.println(y);
    return 0;
}' > "$WORK/app.nxa"
out=$("$NEXAC" "$WORK/app.nxa" -o "$WORK/app" 2>&1)
if [ $? -ne 0 ]; then
    bad "included struct destructure" "$out"
else
    bin="$WORK/app"
    [ -f "$bin" ] || bin="$WORK/app.exe"
    got=$(printf '%s' "$("$bin" 2>&1)" | tr -d '\r')
    if [ "$got" = "$(printf '0\n0')" ]; then
        ok "an included struct keeps the type name"
    else
        bad "included struct destructure" "$got"
    fi
fi

expect_has "wrong struct" \
'struct Point { x: int; y: int; }
struct Box { n: int; }
fn main(): int {
    let b = Box { n: 1 };
    let Point { x } = b;
    return 0;
}' "not a Point"

expect_has "unknown field" \
'struct Point { x: int; y: int; }
fn main(): int {
    let p = Point { x: 1, y: 2 };
    let Point { z } = p;
    return 0;
}' "has no field"

expect_has "empty braces" \
'struct Point { x: int; y: int; }
fn main(): int {
    let p = Point { x: 1, y: 2 };
    let Point {} = p;
    return 0;
}' "Expected a field name"

if [ $FAIL -ne 0 ]; then
    echo "destructure_cases: $FAIL failure(s)"
    exit 1
fi
echo "destructure_cases ok"
