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

expect_has "match on a float is refused" '#include <std/io>
fn main() {
    let f = 1.5;
    let a = match (f) { _ => 1 };
}' "match looks at an enum, a Result, or an int, char, string or bool, and this is a float"

# -- Result: Ok { v } / Err { e } --

run_eq "match on a Result, statement and value" \
'#include <std/io>
fn half(n: int): Result[int] {
    if (n % 2 != 0) { return err("odd " + n); }
    return ok(n / 2);
}
fn main() {
    for (i in [4, 7]) {
        match (half(i)) {
            Ok { v } => { io.println("half " + v); }
            Err { e } => { io.println("error: " + e); }
        }
    }
    io.println(match (io.parse_int("12")) { Ok { n } => n + 1, Err => 0 });
    let r = half(3);
    io.println(match (r) { Ok => "fine", Err { e } => e });
}' "half 2
error: odd 7
13
odd 3"

expect_has "a Result match needs Ok and Err" '#include <std/io>
fn half(n: int): Result[int] { return ok(n); }
fn main() {
    let a = match (half(2)) { Ok { v } => v };
}' "match is not exhaustive: missing Err"

expect_has "Ok of a Result[void] binds nothing" '#include <std/io>
fn check(): Result[void] { return ok(); }
fn main() {
    match (check()) { Ok { v } => { } Err => { } }
}' "Ok of a Result[void] holds no value"

expect_has "Ok on an enum is refused" "#include <std/io>
$SHAPE
fn main() {
    let s = Shape.Point;
    let a = match (s) { Ok => 1, _ => 2 };
}" "the arm Ok is for a Result, but this match looks at a Shape"

# -- values: int, char, string, bool --

run_eq "match on ints, chars, strings and bools" \
'#include <std/io>
fn name(n: int): string {
    return match (n) { 0 => "zero", 1 | 2 | 3 => "few", -1 => "minus one", _ => "many" };
}
fn main() {
    io.println(name(0) + " " + name(2) + " " + name(-1) + " " + name(9));
    let c = '"'"'e'"'"';
    match (c) {
        '"'"'a'"'"' | '"'"'e'"'"' => { io.println("vowel"); }
        _ => { io.println("other"); }
    }
    let cmd = "stop";
    match (cmd) {
        "go" => { io.println("going"); }
        "stop" => { io.println("stopping"); }
        _ => { io.println("?"); }
    }
    io.println(match ("go") { "go" => 1, _ => 0 });
    let on = false;
    io.println(match (on) { true => "on", false => "off" });
}' "zero few minus one many
vowel
stopping
1
off"

expect_has "an int match needs _" '#include <std/io>
fn main() {
    let n = 2;
    match (n) { 1 => { } 2 => { } }
}' "it needs a _ arm"

expect_has "a value of the wrong type is refused" '#include <std/io>
fn main() {
    let n = 2;
    match (n) { "two" => { } _ => { } }
}' "the arm \"two\" is a string, but this match looks at an int"

expect_has "a float pattern is refused" '#include <std/io>
fn main() {
    let n = 2;
    match (n) { 1.5 => { } _ => { } }
}' "a float cannot be a match pattern"

expect_has "an arm after _ is refused" '#include <std/io>
fn main() {
    let n = 2;
    match (n) { _ => { } 1 => { } }
}' "the arm 1 comes after _"

# -- guards --

run_eq "guards on variants, values and _" \
'#include <std/io>
enum Shape { Circle { r: int; }; Box { w: int; h: int; }; Dot; }
fn size(s: Shape): string {
    return match (s) {
        Shape.Circle { r } if r > 10 => "big circle",
        Shape.Circle => "circle",
        Shape.Box { w, h } if w == h => "square",
        Shape.Box => "box",
        Shape.Dot => "dot",
    };
}
fn main() {
    io.println(size(Shape.Circle { r: 20 }) + " " + size(Shape.Circle { r: 2 }) + " " +
        size(Shape.Box { w: 3, h: 3 }) + " " + size(Shape.Box { w: 3, h: 4 }) + " " + size(Shape.Dot));
    let k = 7;
    match (k) {
        1 if k > 0 => { io.println("one"); }
        _ if k > 5 => { io.println("over five"); }
        _ => { io.println("five or less"); }
    }
}' "big circle circle square box dot
over five"

expect_has "a guarded arm covers nothing" "#include <std/io>
$SHAPE
fn main() {
    let s = Shape.Point;
    let a = match (s) { Shape.Point if true => 1, Shape.Circle => 2, Shape.Rect => 3 };
}" "(an arm with an if guard does not count"

expect_has "a guard is a condition" '#include <std/io>
fn half(n: int): Result[int] { return ok(n); }
fn main() {
    let n = 2;
    match (n) { _ if half(n) => { } _ => { } }
}' "an arm's if guard is a condition, and this is a Result[int]"

# -- break and continue in an arm are the loop's --

run_eq "break in an arm leaves the loop" \
'#include <std/io>
fn main() {
    let i = 0;
    while (true) {
        match (i) {
            3 => { break; }
            _ => { i = i + 1; }
        }
    }
    io.println("stopped at " + i);
}' "stopped at 3"

# -- Option[T], built in: Some(x), None, and Some { v } / None arms --

run_eq "built-in Option: Some, None and their arms" \
'#include <std/io>
struct Item { name: string; qty: int; }
fn find(xs: []int, want: int): Option[int] {
    for (i, len(xs)) {
        if (xs[i] == want) { return Some(i); }
    }
    return None;
}
fn lookup(items: []Item, name: string): Option[Item] {
    for (it in items) {
        if (it.name == name) { return Some(it); }
    }
    return None;
}
fn show(o: Option[string]): string {
    return match (o) { Some { s } => "got " + s, None => "nothing" };
}
fn main() {
    let xs = [4, 8, 15, 16];
    match (find(xs, 15)) {
        Some { i } => { io.println("15 is at " + i); }
        None => { io.println("no 15"); }
    }
    io.println(match (find(xs, 99)) { Some { i } => i, None => -1 });
    let items = [Item { name: "pen", qty: 3 }, Item { name: "cup", qty: 12 }];
    match (lookup(items, "cup")) {
        Some { it } if it.qty > 10 => { io.println("lots of " + it.name); }
        Some { it } => { io.println(it.qty); }
        None => { io.println("none"); }
    }
    io.println(show(Some("hi")) + ", " + show(None));
    let a: Option[float] = None;
    io.println(match (a) { Some { v } => v, None => 0.5 });
    let c = Option.Some { value: 7 };
    io.println(match (c) { Option.Some { value } => value, Option.None => 0 });
}' "15 is at 2
-1
lots of cup
got hi, nothing
0.5
7"

run_eq "a program that declares its own Option keeps it" \
'#include <std/io>
enum Option[T] { Some { value: T; }; Empty; }
fn main() {
    let a = Option.Some { value: 4 };
    io.println(match (a) { Some { v } => v, Option.Empty => 0 });
}' "4"

run_eq "a fn Some, a variable None and a variant None are left alone" \
'#include <std/io>
enum Mode { None; Fast; }
fn Some(x: int): int { return x + 100; }
fn main() {
    let None = 5;
    let m = Mode.None;
    io.println(Some(1) + None);
    io.println(match (m) { Mode.None => "none", Mode.Fast => "fast" });
}' "106
none"

expect_has "None needs its type" '#include <std/io>
fn main() {
    let x = None;
}' "'x' is an Option, and nothing says of what; write its type, as in let x: Option[int] = ..."

expect_has "an Option match needs None" '#include <std/io>
fn f(): Option[int] { return Some(1); }
fn main() {
    match (f()) { Some { v } => { } }
}' "match is not exhaustive: missing variant 'None'"

expect_has "Some holds one value" '#include <std/io>
fn f(): Option[int] { return Some(1); }
fn main() {
    match (f()) { Some { a, b } => { } None => { } }
}' "Some holds one value: write Some { name }"

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
