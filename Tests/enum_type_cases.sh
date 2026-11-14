#!/bin/sh
# An enum names a type. Option of an int and Option of a string are two enums.
#
# Usage: Tests/enum_type_cases.sh [path-to-NexaC]   (from the repo root)

set -u
NEXAC="${1:-./NexaC}"
case "$NEXAC" in
    /*) ;;
    *) NEXAC="$(pwd)/$NEXAC" ;;
esac
WORK=$(mktemp -d 2>/dev/null || mktemp -d -t nexaenumtype)
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

run_eq "one enum, an int and a string" \
'#include <std/io>
enum Option[T] {
    Some { value: T; };
    None;
}
fn show(o: Option[int]) {
    switch (o) {
        case Option.Some: io.println(value); break;
        case Option.None: io.println(0); break;
    }
}
fn main(): int {
    show(Option.Some { value: 4 });
    show(Option.None);
    let s = Option.Some { value: "pen" };
    switch (s) {
        case Option.Some: io.println(value); break;
        case Option.None: io.println("no"); break;
    }
    let t: Option[string] = Option.None;
    switch (t) {
        case Option.Some: io.println(value); break;
        case Option.None: io.println("empty"); break;
    }
    return 0;
}' "$(printf '4\n0\npen\nempty')"

run_eq "the type is written on the value" \
'#include <std/io>
enum Option[T] {
    Some { value: T; };
    None;
}
fn main(): int {
    let a = Option[int].None;
    let b = Option[string].Some { value: "cup" };
    switch (a) {
        case Option.None: io.println(1); break;
        case Option.Some: io.println(value); break;
    }
    switch (b) {
        case Option.Some: io.println(value); break;
        case Option.None: io.println(0); break;
    }
    return 0;
}' "$(printf '1\ncup')"

printf '%s\n' \
'#include <std/io>
enum Option[T] {
    Some { value: T; };
    None;
}
fn show[T](o: Option[T]): int {
    switch (o) {
        case Option.Some: return 1;
        case Option.None: return 0;
    }
    return 0;
}' > "$WORK/lib.nxa"
printf '%s\n' \
'#include <std/io>
#include "lib.nxa"
fn main(): int {
    io.println(lib.show(lib.Option.Some { value: 4 }));
    io.println(lib.show(lib.Option[string].None));
    return 0;
}' > "$WORK/app.nxa"
out=$("$NEXAC" "$WORK/app.nxa" -o "$WORK/app" 2>&1)
if [ $? -ne 0 ]; then
    bad "included enum" "$out"
else
    bin="$WORK/app"
    [ -f "$bin" ] || bin="$WORK/app.exe"
    got=$(printf '%s' "$("$bin" 2>&1)" | tr -d '\r')
    if [ "$got" = "$(printf '1\n0')" ]; then
        ok "an included enum keeps the type name"
    else
        bad "included enum" "$got"
    fi
fi

expect_has "a field left out has nothing to take" \
'enum Option[T] { Some { value: T; }; None; }
fn main(): int {
    let o = Option.Some {};
    return 0;
}' "T has no value to take its type from"

expect_has "the enum name alone is not a value" \
'enum Option[T] { Some { value: T; }; None; }
fn main(): int {
    let o: Option;
    return 0;
}' "Option names a type"

expect_has "None needs a type" \
'enum Option[T] { Some { value: T; }; None; }
fn main(): int {
    let o = Option.None;
    return 0;
}' "write it"

if [ $FAIL -ne 0 ]; then
    echo "enum_type_cases: $FAIL failure(s)"
    exit 1
fi
echo "enum_type_cases ok"
