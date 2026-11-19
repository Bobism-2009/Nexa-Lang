#!/bin/sh
# match (expr) { Enum.Variant { fields } => { body } } pattern matching
# on enum variants with field destructuring and exhaustiveness checking.
#
# Usage: Tests/match_cases.sh [path-to-NexaC]   (from the repo root)

set -u
NEXAC="${1:-./NexaC}"
case "$NEXAC" in
    /*) ;;
    *) NEXAC="$(pwd)/$NEXAC" ;;
esac
WORK=$(mktemp -d 2>/dev/null || mktemp -d -t nexamatch)
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
        bad "$label" "expected [$want] got [$got]"
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

source_eq() {
    label=$1
    src_a=$2
    src_b=$3
    printf '%s\n' "$src_a" > "$WORK/a.nxa"
    printf '%s\n' "$src_b" > "$WORK/b.nxa"
    "$NEXAC" "$WORK/a.nxa" --source "$WORK/a.cpp" 2>/dev/null
    "$NEXAC" "$WORK/b.nxa" --source "$WORK/b.cpp" 2>/dev/null
    if diff -q "$WORK/a.cpp" "$WORK/b.cpp" >/dev/null 2>&1; then
        ok "$label"
    else
        bad "$label" "C++ output differs"
    fi
}

# -- non-payload enum match --

run_eq "plain enum match" \
'#include <std/io>
enum Color { Red; Green; Blue; }
fn main() {
    let c: Color = Color.Green;
    match (c) {
        Color.Red => { io.println("red"); }
        Color.Green => { io.println("green"); }
        Color.Blue => { io.println("blue"); }
    }
}' \
"green"

# -- payload enum match with destructuring --

run_eq "payload match Circle" \
'#include <std/io>
enum Shape {
    Circle { radius: float; };
    Rect { w: float; h: float; };
    Point;
}
fn main() {
    let s: Shape = Shape.Circle { radius: 5.0 };
    match (s) {
        Shape.Circle { radius } => {
            io.println("circle r=" + radius);
        }
        Shape.Rect { w, h } => {
            io.println("rect " + w + "x" + h);
        }
        _ => {
            io.println("other");
        }
    }
}' \
"circle r=5"

run_eq "payload match Rect" \
'#include <std/io>
enum Shape {
    Circle { radius: float; };
    Rect { w: float; h: float; };
    Point;
}
fn main() {
    let s: Shape = Shape.Rect { w: 3.0, h: 4.0 };
    match (s) {
        Shape.Circle { radius } => {
            io.println("circle r=" + radius);
        }
        Shape.Rect { w, h } => {
            io.println("rect " + w + "x" + h);
        }
        _ => {
            io.println("other");
        }
    }
}' \
"rect 3x4"

run_eq "payload match wildcard catches Point" \
'#include <std/io>
enum Shape {
    Circle { radius: float; };
    Rect { w: float; h: float; };
    Point;
}
fn main() {
    let s: Shape = Shape.Point;
    match (s) {
        Shape.Circle { radius } => {
            io.println("circle");
        }
        Shape.Rect { w, h } => {
            io.println("rect");
        }
        _ => {
            io.println("point!");
        }
    }
}' \
"point!"

# -- payload match without destructuring --

run_eq "payload match without destructuring" \
'#include <std/io>
enum Shape {
    Circle { radius: float; };
    Rect { w: float; h: float; };
    Point;
}
fn main() {
    let s: Shape = Shape.Circle { radius: 5.0 };
    match (s) {
        Shape.Circle => { io.println("circle"); }
        Shape.Rect => { io.println("rect"); }
        _ => { io.println("other"); }
    }
}' \
"circle"

# -- exhaustiveness errors --

expect_has "missing variant is an error" \
'#include <std/io>
enum Color { Red; Green; Blue; }
fn main() {
    let c: Color = Color.Green;
    match (c) {
        Color.Red => { io.println("red"); }
        Color.Green => { io.println("green"); }
    }
}' \
"missing variant 'Blue'"

expect_has "missing payload variant is an error" \
'#include <std/io>
enum Shape {
    Circle { radius: float; };
    Rect { w: float; h: float; };
    Point;
}
fn main() {
    let s: Shape = Shape.Point;
    match (s) {
        Shape.Circle { radius } => { io.println("circle"); }
    }
}' \
"missing variant"

# -- wildcard makes match exhaustive --

run_eq "wildcard satisfies exhaustiveness" \
'#include <std/io>
enum Color { Red; Green; Blue; }
fn main() {
    let c: Color = Color.Green;
    match (c) {
        Color.Red => { io.println("red"); }
        _ => { io.println("not red"); }
    }
}' \
"not red"

# -- slicing: match-free programs are unchanged --

source_eq "no-match program unchanged" \
'#include <std/io>
fn main() {
    io.println("hello");
}' \
'#include <std/io>
fn main() {
    io.println("hello");
}'

echo ""
if [ "$FAIL" -gt 0 ]; then
    echo "FAILED $FAIL test(s)"
    exit 1
else
    echo "All tests passed"
fi
