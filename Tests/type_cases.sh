#!/bin/sh
# A function names a type. Each list, map or Result it is given is its own
# function, and the call is direct.
#
# Usage: Tests/type_cases.sh [path-to-NexaC]   (from the repo root)

set -u
NEXAC="${1:-./NexaC}"
case "$NEXAC" in
    /*) ;;
    *) NEXAC="$(pwd)/$NEXAC" ;;
esac
WORK=$(mktemp -d 2>/dev/null || mktemp -d -t nexatype)
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

run_eq "one function, a list of ints and a list of strings" \
'#include <std/io>
struct Item { n: int; }
fn first[T](xs: []T): T { return xs[0]; }
fn value[T](r: Result[T]): T { return r.value(); }
fn get[V](m: map[string]V, k: string): V { return m[k]; }
fn pair[K, V](k: K, v: V): V { return v; }
fn thru[T](p: *T): T { return *p; }
fn main(): int {
    io.println(first([4, 5]));
    io.println(first(["pen", "cup"]));
    io.println(value(ok(3)));
    io.println(value(ok("hi")));
    let m: map[string]int;
    m["a"] = 9;
    io.println(get(m, "a"));
    let items = [Item { n: 7 }];
    io.println(first(items).n);
    io.println(pair("a", 3));
    let n = 4;
    io.println(thru(&n));
    return 0;
}' "$(printf '4\npen\n3\nhi\n9\n7\n3\n4')"

run_eq "a plain list of ints still wins" \
'#include <std/io>
fn first[T](xs: []T): T { return xs[0]; }
fn first(xs: []int): int { return xs[0] + 1; }
fn main(): int {
    io.println(first([1]));
    io.println(first(["pen"]));
    return 0;
}' "$(printf '2\npen')"

printf '%s\n' \
'#include <std/io>
fn first[T](xs: []T): T { return xs[0]; }' > "$WORK/lib.nxa"
printf '%s\n' \
'#include <std/io>
#include "lib.nxa"
fn main(): int {
    io.println(lib.first([4]));
    io.println(lib.first(["pen"]));
    return 0;
}' > "$WORK/app.nxa"
out=$("$NEXAC" "$WORK/app.nxa" -o "$WORK/app" 2>&1)
if [ $? -ne 0 ]; then
    bad "included function" "$out"
else
    bin="$WORK/app"
    [ -f "$bin" ] || bin="$WORK/app.exe"
    got=$(printf '%s' "$("$bin" 2>&1)" | tr -d '\r')
    if [ "$got" = "$(printf '4\npen')" ]; then
        ok "an included function keeps the type name"
    else
        bad "included function" "$got"
    fi
fi

printf '%s\n' \
'#include <std/io>
fn first[T](xs: []T): T { return xs[0]; }
fn main(): int {
    io.println(first([4]));
    io.println(first(["pen"]));
    return 0;
}' > "$WORK/src.nxa"
out=$("$NEXAC" "$WORK/src.nxa" --source "$WORK/src.cpp" 2>&1)
if [ $? -ne 0 ]; then
    bad "direct call" "$out"
elif grep -q 'typevar:' "$WORK/src.cpp"; then
    bad "direct call" "generated C++ still names the type"
else
    ok "each list is its own function"
fi

expect_has "two arguments that disagree" \
'#include <std/io>
fn same[T](a: []T, b: []T): int { return a[0]; }
fn main(): int {
    same([1], ["a"]);
    return 0;
}' "T is int and string"

expect_has "a type with nothing to take it from" \
'fn make[T](): int { return 0; }
fn main(): int {
    make();
    return 0;
}' "T has no value to take its type from"

expect_has "a method cannot name a type" \
'struct Box {
    fn id[T](x: T): T { return x; }
}
fn main(): int { return 0; }' \
"a method cannot name a type"

expect_has "a type parameter has no default" \
'fn id[T](x: T = 0): T { return x; }
fn main(): int { return 0; }' \
"a type parameter has no default"

expect_has "a builtin is already a type" \
'fn id[int](x: int): int { return x; }
fn main(): int { return 0; }' \
"'int' is already a type"

expect_has "a function cannot name a type and a shape" \
'shape Drawable { fn draw(); }
fn paint[T](d: Drawable, x: T) { d.draw(); }
fn main(): int { return 0; }' \
"names a type and a shape"

if [ $FAIL -ne 0 ]; then
    echo "type_cases: $FAIL failure(s)"
    exit 1
fi
echo "type_cases ok"
