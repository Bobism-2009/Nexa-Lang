#!/bin/sh
# A struct names a type. Slot of an int and Slot of a string are two structs.
#
# Usage: Tests/struct_type_cases.sh [path-to-NexaC]   (from the repo root)

set -u
NEXAC="${1:-./NexaC}"
case "$NEXAC" in
    /*) ;;
    *) NEXAC="$(pwd)/$NEXAC" ;;
esac
WORK=$(mktemp -d 2>/dev/null || mktemp -d -t nexastructtype)
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

run_eq "one struct, an int and a string" \
'#include <std/io>
struct Slot[T] {
    value: T;
    n: int;
    fn get(): T { return self.value; }
}
fn put[T](s: *Slot[T], v: T) {
    s->value = v;
}
fn main(): int {
    let a = Slot { value: 4 };
    let b = Slot { value: "pen", n: 2 };
    io.println(a.get());
    io.println(a.n);
    io.println(b.get());
    io.println(b.n);
    put(&a, 9);
    io.println(a.value);
    return 0;
}' "$(printf '4\n0\npen\n2\n9')"

run_eq "two names, each from its field" \
'#include <std/io>
struct Pair[A, B] { a: A; b: B; }
fn main(): int {
    let p = Pair { a: 3, b: "cup" };
    io.println(p.a);
    io.println(p.b);
    return 0;
}' "$(printf '3\ncup')"

run_eq "the return type names the struct" \
'#include <std/io>
struct Slot[T] { value: T; }
fn make[T](v: T): Slot[T] {
    return Slot { value: v };
}
fn main(): int {
    io.println(make(6).value);
    io.println(make("ink").value);
    return 0;
}' "$(printf '6\nink')"

printf '%s\n' \
'#include <std/io>
struct Slot[T] { value: T; }
fn show[T](s: Slot[T]): T { return s.value; }' > "$WORK/lib.nxa"
printf '%s\n' \
'#include <std/io>
#include "lib.nxa"
fn main(): int {
    io.println(lib.show(lib.Slot { value: 4 }));
    io.println(lib.show(lib.Slot { value: "pen" }));
    return 0;
}' > "$WORK/app.nxa"
out=$("$NEXAC" "$WORK/app.nxa" -o "$WORK/app" 2>&1)
if [ $? -ne 0 ]; then
    bad "included struct" "$out"
else
    bin="$WORK/app"
    [ -f "$bin" ] || bin="$WORK/app.exe"
    got=$(printf '%s' "$("$bin" 2>&1)" | tr -d '\r')
    if [ "$got" = "$(printf '4\npen')" ]; then
        ok "an included struct keeps the type name"
    else
        bad "included struct" "$got"
    fi
fi

expect_has "a field left out has nothing to take" \
'struct Slot[T] { value: T; ok: bool; }
fn main(): int {
    let s = Slot { ok: true };
    return 0;
}' "T has no value to take its type from"

expect_has "two fields that disagree" \
'struct Same[T] { a: T; b: T; }
fn main(): int {
    let s = Same { a: 1, b: "x" };
    return 0;
}' "T is int and string"

expect_has "the struct name alone is not a value" \
'struct Slot[T] { value: T; }
fn main(): int {
    let s: Slot;
    return 0;
}' "Slot names a type"

expect_has "the wrong number of types" \
'struct Pair[A, B] { a: A; b: B; }
fn main(): int {
    let p: Pair[int];
    return 0;
}' "Pair names 2 types"

if [ $FAIL -ne 0 ]; then
    echo "struct_type_cases: $FAIL failure(s)"
    exit 1
fi
echo "struct_type_cases ok"
