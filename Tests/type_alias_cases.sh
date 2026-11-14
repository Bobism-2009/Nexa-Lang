#!/bin/sh
# A short name for a type. Id is still an int; the generated program is the
# same as writing the type out.
#
# Usage: Tests/type_alias_cases.sh [path-to-NexaC]   (from the repo root)

set -u
NEXAC="${1:-./NexaC}"
case "$NEXAC" in
    /*) ;;
    *) NEXAC="$(pwd)/$NEXAC" ;;
esac
WORK=$(mktemp -d 2>/dev/null || mktemp -d -t nexatalias)
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

run_eq "a short name for an int" \
'#include <std/io>
type Id = int;
fn main(): int {
    let a: Id = 4;
    io.println(a);
    return 0;
}' "4"

run_eq "a short name for a map of options" \
'#include <std/io>
enum Option[T] {
    Some { value: T; };
    None;
}
type Bag = map[string]Option[int];
fn main(): int {
    let b: Bag;
    b["a"] = Option.Some { value: 9 };
    switch (b["a"]) {
        case Option.Some: io.println(value); break;
        case Option.None: io.println(0); break;
    }
    return 0;
}' "9"

run_eq "the same C++ as writing the type out" \
'#include <std/io>
type Id = int;
fn main(): int {
    let a: Id = 3;
    io.println(a + 1);
    return 0;
}' "4"

printf '%s\n' \
'#include <std/io>
type Id = int;
fn show(x: Id) { io.println(x); }' > "$WORK/lib.nxa"
printf '%s\n' \
'#include <std/io>
#include "lib.nxa"
fn main(): int {
    lib.show(7);
    let n: lib.Id = 8;
    io.println(n);
    return 0;
}' > "$WORK/app.nxa"
out=$("$NEXAC" "$WORK/app.nxa" -o "$WORK/app" 2>&1)
if [ $? -ne 0 ]; then
    bad "included alias" "$out"
else
    bin="$WORK/app"
    [ -f "$bin" ] || bin="$WORK/app.exe"
    got=$(printf '%s' "$("$bin" 2>&1)" | tr -d '\r')
    if [ "$got" = "$(printf '7\n8')" ]; then
        ok "an included alias is the file's name"
    else
        bad "included alias" "$got"
    fi
fi

# Id and int must emit the same C++
printf '%s\n' \
'type Id = int;
fn main(): int { let a: Id = 1; return a; }' > "$WORK/alias.nxa"
printf '%s\n' \
'fn main(): int { let a: int = 1; return a; }' > "$WORK/plain.nxa"
if ! "$NEXAC" "$WORK/alias.nxa" --source "$WORK/alias.cpp" >/dev/null 2>&1; then
    bad "same as int" "alias program failed"
elif ! "$NEXAC" "$WORK/plain.nxa" --source "$WORK/plain.cpp" >/dev/null 2>&1; then
    bad "same as int" "plain program failed"
elif ! cmp -s "$WORK/alias.cpp" "$WORK/plain.cpp"; then
    bad "same as int" "generated C++ differs from a plain int"
else
    ok "the short name is the type on the right"
fi

expect_has "a name that is already a type" \
'type int = string;
fn main(): int { return 0; }' \
"already a type"

expect_has "a name that names itself" \
'type A = A;
fn main(): int { return 0; }' \
"names itself"

expect_has "an alias does not take types" \
'type Id = int;
fn main(): int {
    let a: Id[int];
    return 0;
}' \
"does not take"

if [ $FAIL -ne 0 ]; then
    echo "type_alias_cases: $FAIL failure(s)"
    exit 1
fi
echo "type_alias_cases ok"
