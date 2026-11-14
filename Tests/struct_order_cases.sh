#!/bin/sh
# A struct may name one written later. A value field is laid out first; a
# pointer only needs the other struct named. A cycle of value fields is an error.
#
# Usage: Tests/struct_order_cases.sh [path-to-NexaC]   (from the repo root)

set -u
NEXAC="${1:-./NexaC}"
case "$NEXAC" in
    /*) ;;
    *) NEXAC="$(pwd)/$NEXAC" ;;
esac
WORK=$(mktemp -d 2>/dev/null || mktemp -d -t nexastruct)
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

run_eq "a value field may name a struct written later" \
'#include <std/io>
struct Box { item: Item; }
struct Item { n: int; }
fn main(): int {
    let item = Item { n: 4 };
    let box = Box { item: item };
    io.println(box.item.n);
    return 0;
}' "4"

run_eq "a pointer may name a struct written later" \
'#include <std/io>
struct Box { item: *Item; }
struct Item { n: int; }
fn main(): int {
    let item = Item { n: 7 };
    let box = Box { item: &item };
    io.println(box.item.n);
    return 0;
}' "7"

run_eq "two structs may point at each other" \
'#include <std/io>
struct A { n: int; b: *B; }
struct B { n: int; a: *A; }
fn main(): int {
    let b = B { n: 2 };
    let a = A { n: 1, b: &b };
    io.println(a.b.n);
    return 0;
}' "2"

run_eq "a list may hold a struct written later" \
'#include <std/io>
struct Bag { items: []Item; }
struct Item { n: int; }
fn main(): int {
    let bag: Bag;
    bag.items.push(Item { n: 3 });
    io.println(bag.items[0].n);
    return 0;
}' "3"

run_eq "a struct may point at itself" \
'#include <std/io>
struct Node { n: int; next: *Node; }
fn main(): int {
    let a = Node { n: 1 };
    let b = Node { n: 2, next: &a };
    io.println(b.next.n);
    return 0;
}' "1"

run_eq "a later struct may use an enum written between them" \
'#include <std/io>
struct Box { item: Item; }
enum Color { Red; Green; }
struct Item { n: int; c: Color; }
fn main(): int {
    let item = Item { n: 1, c: Color.Red };
    let box = Box { item: item };
    if (box.item.c == Color.Red) { io.println(box.item.n); }
    return 0;
}' "1"

run_eq "a method may return a struct written later" \
'#include <std/io>
struct A {
    fn get(): B {
        let b: B;
        b.n = 9;
        return b;
    }
}
struct B { n: int; }
fn main(): int {
    let a: A;
    io.println(a.get().n);
    return 0;
}' "9"

printf '%s\n' 'struct Box { item: Item; }
struct Item { n: int; }
fn make(): Box {
    let item = Item { n: 6 };
    let box = Box { item: item };
    return box;
}' > "$WORK/lib.nxa"
printf '%s\n' '#include <std/io>
#include "lib.nxa"
fn main(): int {
    io.println(lib.make().item.n);
    return 0;
}' > "$WORK/app.nxa"
out=$("$NEXAC" "$WORK/app.nxa" -o "$WORK/app" 2>&1)
if [ $? -ne 0 ]; then
    bad "included file order" "$out"
else
    bin="$WORK/app"
    [ -f "$bin" ] || bin="$WORK/app.exe"
    got=$(printf '%s' "$("$bin" 2>&1)" | tr -d '\r')
    if [ "$got" = "6" ]; then
        ok "an included file may write its structs in either order"
    else
        bad "included file order" "$got"
    fi
fi

expect_has() {
    label=$1
    src=$2
    want=$3
    printf '%s\n' "$src" > "$WORK/bad.nxa"
    out=$("$NEXAC" "$WORK/bad.nxa" --source "$WORK/bad.cpp" 2>&1)
    if [ $? -eq 0 ]; then
        bad "$label" "NexaC accepted a cycle of value fields"
        return
    fi
    case $out in
        *"$want"*) ok "$label" ;;
        *) bad "$label" "$out" ;;
    esac
}

expect_has "a cycle of value fields is refused" \
'struct Box { item: Item; }
struct Item { box: Box; }
fn main(): int { return 0; }' \
"contains Item by value"

expect_has "a struct cannot contain a list of itself" \
'struct Node { kids: []Node; }
fn main(): int { return 0; }' \
"contains Node by value"

if [ $FAIL -ne 0 ]; then
    echo "struct_order_cases: $FAIL failure(s)"
    exit 1
fi
echo "struct_order_cases ok"
