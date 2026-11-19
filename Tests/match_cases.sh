#!/bin/sh
# match (expr) { Enum.Variant { fields } => { body } } pattern matching
# on enum variants with field destructuring and exhaustiveness checking.
#
# Usage: Tests/match_cases.sh [path-to-NexaC]   (from the repo root)

set -u
NEXAC="${1:-./NexaC}"
case "$NEXAC" in
    /*|[A-Za-z]:*) ;;
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

# The C++ ahead of main() -- headers, runtime helpers, types -- is the same for
# both programs: what the first one uses brings no code of its own.
prelude_eq() {
    label=$1
    printf '%s\n' "$2" > "$WORK/a.nxa"
    printf '%s\n' "$3" > "$WORK/b.nxa"
    if ! "$NEXAC" "$WORK/a.nxa" --source "$WORK/a.cpp" >/dev/null 2>&1 ||
       ! "$NEXAC" "$WORK/b.nxa" --source "$WORK/b.cpp" >/dev/null 2>&1; then
        bad "$label" "NexaC refused a program"
        return
    fi
    sed '/^int main/,$d' "$WORK/a.cpp" > "$WORK/a.pre"
    sed '/^int main/,$d' "$WORK/b.cpp" > "$WORK/b.pre"
    if [ -s "$WORK/a.pre" ] && cmp -s "$WORK/a.pre" "$WORK/b.pre"; then
        ok "$label"
    else
        bad "$label" "the C++ before main() differs"
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

# -- match as a value --

SHAPE='enum Shape {
    Circle { radius: float; };
    Rect { w: float; h: float; };
    Point;
}'

run_eq "value: payload arms take fields apart" "#include <std/io>
$SHAPE
fn area(s: Shape): float {
    return match (s) {
        Shape.Circle { radius } => radius * radius * 3.0,
        Shape.Rect { w, h } => w * h,
        Shape.Point => 0,
    };
}
fn main() {
    io.println(area(Shape.Circle { radius: 2.0 }), \" \", area(Shape.Rect { w: 2.0, h: 5.0 }), \" \", area(Shape.Point));
}" "12 10 0"

run_eq "value: plain enum, strings and _" '#include <std/io>
enum Color { Red; Green; Blue; }
fn name(c: Color): string {
    return match (c) { Color.Red => "red", Color.Green => "green", _ => "other" };
}
fn main() {
    let c: Color = Color.Blue;
    let k = match (c) { Color.Red => 1, Color.Green => 2, Color.Blue => 3 };
    io.println(name(Color.Red) + " " + name(c) + " " + k);
}' "red other 3"

run_eq "value: in a call, a concat, a condition and a slice" '#include <std/io>
enum Op { Add { n: int; }; Neg; }
fn twice(x: int): int { return x * 2; }
fn main() {
    let ops = [Op.Add { n: 3 }, Op.Neg];
    let acc = 1;
    for (o in ops) { acc = match (o) { Op.Add { n } => acc + n, Op.Neg => -acc }; }
    let xs = [match (ops[0]) { Op.Add { n } => n, _ => 0 }, 5];
    if (match (ops[1]) { Op.Neg => true, _ => false }) {
        io.println(acc, " ", xs, " ", twice(match (ops[0]) { Op.Add { n } => n, _ => 0 }),
                   " op: " + match (ops[1]) { Op.Neg => "neg", _ => "add" });
    }
}' "-4 [3, 5] 6 op: neg"

run_eq "value: the scrutinee is read once" '#include <std/io>
enum Op { Add { n: int; }; Neg; }
let calls = 0;
fn next(): Op { calls += 1; return Op.Add { n: 7 }; }
fn main() {
    let v = match (next()) { Op.Add { n } => n + 1, Op.Neg => 0 };
    io.println(v, " ", calls);
}' "8 1"

run_eq "value: nested, and in a global let" '#include <std/io>
enum Color { Red; Blue; }
let g = match (Color.Blue) { Color.Red => 1, _ => 2 };
fn main() {
    let c = Color.Red;
    let d = Color.Blue;
    io.println(g + match (c) { Color.Red => match (d) { Color.Blue => 10, _ => 20 }, _ => 30 });
}' "12"

run_eq "an arm reads the scrutinee as its variant" '#include <std/io>
enum Op { Add { n: int; }; Neg; }
fn main() {
    let o = Op.Add { n: 4 };
    io.println(match (o) { Op.Add => o.n + 1, _ => 0 });
    match (o) {
        Op.Add => { io.println("n=", o.n); }
        _ => { }
    }
}' "5
n=4"

expect_has "value: a { block } arm is refused" "#include <std/io>
$SHAPE
fn main() {
    let s = Shape.Point;
    let a = match (s) { Shape.Point => { 1 }, _ => 2 };
}" "not a { block }"

expect_has "value: arms of different types" "#include <std/io>
$SHAPE
fn main() {
    let s = Shape.Point;
    let a = match (s) { Shape.Point => 1, _ => \"x\" };
}" "the Shape.Point arm gives int, and the _ arm gives string"

expect_has "value: a missing variant is an error" "#include <std/io>
$SHAPE
fn main() {
    let s = Shape.Point;
    let a = match (s) { Shape.Point => 1, Shape.Rect => 2 };
}" "missing variant 'Circle'"

expect_has "an unknown field is refused" "#include <std/io>
$SHAPE
fn main() {
    let s = Shape.Point;
    match (s) { Shape.Circle { nope } => { } _ => { } }
}" "Shape.Circle has no field 'nope' (its fields: radius)"

expect_has "an arm naming another enum is refused" "#include <std/io>
$SHAPE
enum Color { Red; Blue; }
fn main() {
    let s = Shape.Point;
    let a = match (s) { Color.Red => 1, _ => 2 };
}" "the arm Color.Red names Color, but this match looks at a Shape"

expect_has "two arms for one variant are refused" "#include <std/io>
$SHAPE
fn main() {
    let s = Shape.Point;
    let a = match (s) { Shape.Point => 1, Shape.Point => 2, _ => 3 };
}" "two arms for Shape.Point"

expect_has "match on a non-enum is refused" '#include <std/io>
fn main() {
    let a = match (5) { _ => 1 };
}' "match looks at an enum's variants, and this is an int"

# -- slicing: match is code in place, nothing ahead of main --

prelude_eq "match brings no helpers, headers or types" \
'#include <std/io>
enum Shape { Circle { r: int; }; Dot; }
fn main() {
    let s = Shape.Circle { r: 2 };
    let a = match (s) { Shape.Circle { r } => r, _ => 0 };
    match (s) {
        Shape.Dot => { io.println("dot"); }
        _ => { io.println(a); }
    }
}' \
'#include <std/io>
enum Shape { Circle { r: int; }; Dot; }
fn main() {
    let s = Shape.Circle { r: 2 };
    let a = 0;
    if (a == 0) { io.println(a); }
}'

echo ""
if [ "$FAIL" -gt 0 ]; then
    echo "FAILED $FAIL test(s)"
    exit 1
else
    echo "All tests passed"
fi
