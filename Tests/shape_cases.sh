#!/bin/sh
# A shape names functions. A function that takes one accepts any struct that
# has those functions, and the call is that struct's own.
#
# Usage: Tests/shape_cases.sh [path-to-NexaC]   (from the repo root)

set -u
NEXAC="${1:-./NexaC}"
case "$NEXAC" in
    /*) ;;
    *) NEXAC="$(pwd)/$NEXAC" ;;
esac
WORK=$(mktemp -d 2>/dev/null || mktemp -d -t nexashape)
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

run_eq "two structs share one function" \
'#include <std/io>
shape Drawable {
    fn draw();
    fn width(): int;
}
struct Button {
    w: int;
    fn draw() { io.println("button"); }
    fn width(): int { return self.w; }
}
struct Panel {
    w: int;
    fn draw() { io.println("panel"); }
    fn width(): int { return self.w; }
}
fn paint(d: Drawable) {
    d.draw();
    io.println(d.width());
}
fn main(): int {
    let b = Button { w: 3 };
    let p = Panel { w: 8 };
    paint(b);
    paint(p);
    return 0;
}' "$(printf 'button\n3\npanel\n8')"

run_eq "a shape may be written after the function" \
'#include <std/io>
fn paint(d: Drawable) { d.draw(); }
shape Drawable { fn draw(); }
struct Button {
    fn draw() { io.println("later"); }
}
fn main(): int {
    let b = Button {};
    paint(b);
    return 0;
}' "later"

run_eq "a plain parameter is a copy" \
'#include <std/io>
shape Drawable { fn bump(); }
struct Button {
    w: int;
    fn bump() { self.w = self.w + 1; }
}
fn paint(d: Drawable) { d.bump(); }
fn main(): int {
    let b = Button { w: 1 };
    paint(b);
    io.println(b.w);
    return 0;
}' "1"

run_eq "a pointer parameter is the caller" \
'#include <std/io>
shape Drawable { fn bump(); }
struct Button {
    w: int;
    fn bump() { self.w = self.w + 1; }
}
fn paint(d: *Drawable) { d.bump(); }
fn main(): int {
    let b = Button { w: 1 };
    paint(&b);
    io.println(b.w);
    return 0;
}' "2"

run_eq "an exact struct parameter wins" \
'#include <std/io>
shape Drawable { fn draw(); }
struct Button {
    fn draw() { io.println("method"); }
}
fn paint(d: Drawable) { io.println("shape"); d.draw(); }
fn paint(b: Button) { io.println("exact"); b.draw(); }
fn main(): int {
    let b = Button {};
    paint(b);
    return 0;
}' "$(printf 'exact\nmethod')"

printf '%s\n' \
'#include <std/io>
shape Drawable { fn draw(); }
struct Btn { fn draw() { io.println("lib"); } }
fn paint(d: Drawable) { d.draw(); }' > "$WORK/lib.nxa"
printf '%s\n' \
'#include <std/io>
#include "lib.nxa"
struct Panel { fn draw() { io.println("app"); } }
fn main(): int {
    let b = lib.Btn {};
    lib.paint(b);
    let p = Panel {};
    lib.paint(p);
    return 0;
}' > "$WORK/app.nxa"
out=$("$NEXAC" "$WORK/app.nxa" -o "$WORK/app" 2>&1)
if [ $? -ne 0 ]; then
    bad "included shape" "$out"
else
    bin="$WORK/app"
    [ -f "$bin" ] || bin="$WORK/app.exe"
    got=$(printf '%s' "$("$bin" 2>&1)" | tr -d '\r')
    if [ "$got" = "$(printf 'lib\napp')" ]; then
        ok "an included shape is the file's name"
    else
        bad "included shape" "$got"
    fi
fi

printf '%s\n' \
'#include <std/io>
shape Drawable { fn draw(); }
struct Button { fn draw() { io.println("direct"); } }
fn paint(d: Drawable) { d.draw(); }
fn main(): int {
    let b = Button {};
    paint(b);
    return 0;
}' > "$WORK/src.nxa"
out=$("$NEXAC" "$WORK/src.nxa" --source "$WORK/src.cpp" 2>&1)
if [ $? -ne 0 ]; then
    bad "direct call" "$out"
elif grep -q '\.draw(' "$WORK/src.cpp"; then
    ok "the call is the struct's own function"
else
    bad "direct call" "generated C++ has no .draw("
fi

expect_has "a struct missing a function is refused" \
'#include <std/io>
shape Drawable { fn draw(); fn width(): int; }
struct Label { fn draw() { io.println("label"); } }
fn paint(d: Drawable) { d.draw(); }
fn main(): int {
    let l = Label {};
    paint(l);
    return 0;
}' "Label has no width() for Drawable"

expect_has "a shape is not a field" \
'shape Drawable { fn draw(); }
struct Box { d: Drawable; }
fn main(): int { return 0; }' \
"names functions, so it is not a value you can store"

expect_has "a shape is not a let" \
'shape Drawable { fn draw(); }
fn main(): int {
    let d: Drawable;
    return 0;
}' "names functions, so it is not a value you can store"

expect_has "a method cannot take a shape" \
'shape Drawable { fn draw(); }
struct Box {
    fn go(d: Drawable) { d.draw(); }
}
fn main(): int { return 0; }' \
"a method takes one struct"

if [ $FAIL -ne 0 ]; then
    echo "shape_cases: $FAIL failure(s)"
    exit 1
fi
echo "shape_cases ok"
