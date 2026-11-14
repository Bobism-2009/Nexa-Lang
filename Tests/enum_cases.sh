#!/bin/sh
# A variant may hold fields. One list holds every kind, and the fields are
# names only inside that case. An enum that never writes fields is unchanged.
#
# Usage: Tests/enum_cases.sh [path-to-NexaC]   (from the repo root)

set -u
NEXAC="${1:-./NexaC}"
case "$NEXAC" in
    /*) ;;
    *) NEXAC="$(pwd)/$NEXAC" ;;
esac
WORK=$(mktemp -d 2>/dev/null || mktemp -d -t nexaenum)
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

run_eq "a plain enum is still a number" \
'#include <std/io>
enum Color { Red; Green; }
fn main(): int {
    let c = Color.Red;
    io.println(c);
    if (c == Color.Green) { return 1; }
    return 0;
}' "0"

run_eq "one list holds every kind" \
'#include <std/io>
enum Event {
    Click { x: int; y: int; };
    Key { code: int; };
    Quit;
}
fn show(e: Event) {
    switch (e) {
        case Event.Click:
            io.println(x);
            io.println(y);
            break;
        case Event.Key:
            io.println(code);
            break;
        case Event.Quit:
            io.println(0);
            break;
    }
}
fn main(): int {
    let xs = [Event.Click { x: 3, y: 4 }, Event.Key { code: 9 }, Event.Quit];
    for (e in xs) { show(e); }
    let a = Event.Click { x: 1, y: 2 };
    let b = Event.Click { x: 1 };
    io.println(a == Event.Click { x: 1, y: 2 });
    io.println(a == b);
    io.println(a == Event.Quit);
    show(b);
    return 0;
}' "$(printf '3\n4\n9\n0\ntrue\nfalse\nfalse\n1\n0')"

printf '%s\n' \
'enum Event {
    Click { x: int; };
    Quit;
}
fn show(e: Event): int {
    switch (e) {
        case Event.Click: return x;
        case Event.Quit: return 0;
    }
    return 0;
}' > "$WORK/lib.nxa"
printf '%s\n' \
'#include <std/io>
#include "lib.nxa"
fn main(): int {
    io.println(lib.show(lib.Event.Click { x: 4 }));
    io.println(lib.show(lib.Event.Quit));
    return 0;
}' > "$WORK/app.nxa"
out=$("$NEXAC" "$WORK/app.nxa" -o "$WORK/app" 2>&1)
if [ $? -ne 0 ]; then
    bad "included enum" "$out"
else
    bin="$WORK/app"
    [ -f "$bin" ] || bin="$WORK/app.exe"
    got=$(printf '%s' "$("$bin" 2>&1)" | tr -d '\r')
    if [ "$got" = "$(printf '4\n0')" ]; then
        ok "an included enum is the file's name"
    else
        bad "included enum" "$got"
    fi
fi

expect_has "a variant with fields needs them written" \
'enum Event { Click { x: int; }; }
fn main(): int {
    let e = Event.Click;
    return 0;
}' "Click has fields"

expect_has "an unknown field is refused" \
'enum Event { Click { x: int; }; }
fn main(): int {
    let e = Event.Click { z: 1 };
    return 0;
}' "Unknown field 'z'"

expect_has "a field is only there in its case" \
'#include <std/io>
enum Event { Click { x: int; }; }
fn main(): int {
    let e = Event.Click { x: 1 };
    io.println(e.x);
    return 0;
}' "x is there in case Event.Click"

if [ $FAIL -ne 0 ]; then
    echo "enum_cases: $FAIL failure(s)"
    exit 1
fi
echo "enum_cases ok"
