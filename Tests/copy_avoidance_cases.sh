#!/bin/sh
# Emission cover for the copies NexaC is supposed to avoid.
#
# Two of them:
#   * a slice, string, map, struct, Result or json parameter is copied on every
#     call unless NexaC can prove the copy is unobservable, in which case it
#     emits a `const T&`;
#   * `s += x` on a string appends in place rather than rebuilding the string.
#
# Tests/Lang/param_value_semantics.nxa and Tests/Lang/string_append.nxa pin the
# *behaviour* that must not move; this suite pins the *emission*, because both
# optimisations are invisible from the outside when they work and silent
# regressions when they stop firing.
#
# Each case asserts a substring of the emitted C++ (--source --preserve-names).
#
# Usage: Tests/copy_avoidance_cases.sh [path-to-NexaC]      (run from the repo root)

set -u

NEXAC="${1:-./NexaC}"
if [ ! -x "$NEXAC" ]; then
    echo "FAIL: NexaC not found or not executable: $NEXAC"
    exit 1
fi
NEXAC=$(cd "$(dirname "$NEXAC")" && pwd)/$(basename "$NEXAC")

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT INT TERM

fails=0

# emit <source-text> -> writes generated C++ to $WORK/case.cpp
emit() {
    printf '%s' "$1" > "$WORK/case.nxa"
    "$NEXAC" "$WORK/case.nxa" --source --preserve-names -o "$WORK/case.cpp" >"$WORK/log" 2>&1
}

# expect_sig <label> <wanted-definition-line> <source-text>
expect_sig() {
    label=$1
    want=$2
    src=$3

    if ! emit "$src"; then
        echo "FAIL $label: NexaC exited non-zero"
        sed 's/^/  /' "$WORK/log"
        fails=$((fails + 1))
        return
    fi
    if ! grep -qF "$want" "$WORK/case.cpp"; then
        echo "FAIL $label: expected to find"
        echo "  $want"
        echo "  but the emitted signatures were:"
        grep -E '^(static |[A-Za-z_].* [A-Za-z_]+::)' "$WORK/case.cpp" | sed 's/^/    /'
        fails=$((fails + 1))
        return
    fi
    echo "ok $label"
}

IO='#include <std/io>
'

# --- the reason this exists: a read-only slice parameter costs a pointer ------
expect_sig "read-only slice is a reference" \
    'static int total(const std::vector<int>& xs) {' \
    "${IO}fn total(xs: []int): int {
    let s: int = 0;
    for (x in xs) { s = s + x; }
    return s;
}
fn main() { let xs: []int = [1, 2]; io.println(total(xs)); }
"

expect_sig "read-only string is a reference" \
    'static int width(const std::string& s) {' \
    "${IO}fn width(s: string): int { return len(s); }
fn main() { io.println(width(\"hi\")); }
"

expect_sig "read-only struct is a reference" \
    'static int sum(const Point& p) {' \
    "${IO}struct Point { x: int; y: int; }
fn sum(p: Point): int { return p.x + p.y; }
fn main() { let p: Point; io.println(sum(p)); }
"

expect_sig "read-only map is a reference" \
    'static int entries(const std::map<std::string, int>& m) {' \
    "${IO}fn entries(m: map[string]int): int { return len(m.keys()); }
fn main() { let m: map[string]int; io.println(entries(m)); }
"

expect_sig "a struct method's parameter is a reference too" \
    'int Point::span(const Point& other) {' \
    "${IO}struct Point {
    x: int;
    fn span(other: Point): int { return self.x + other.x; }
}
fn main() { let a: Point; let b: Point; io.println(a.span(b)); }
"

# --- and the cases where the copy is load-bearing -----------------------------
expect_sig "a mutated parameter stays a copy" \
    'static int drop(std::vector<int> xs) {' \
    "${IO}fn drop(xs: []int): int { xs.pop(); return len(xs); }
fn main() { let xs: []int = [1, 2]; io.println(drop(xs)); }
"

expect_sig "an appended string parameter stays a copy" \
    'static std::string shout(std::string s) {' \
    "${IO}fn shout(s: string): string { s += \"!\"; return s; }
fn main() { io.println(shout(\"hi\")); }
"

expect_sig "writing a global keeps every parameter a copy" \
    'static int count(std::vector<int> list) {' \
    "${IO}let seen: []int;
fn count(list: []int): int { seen.push(1); return len(list); }
fn main() { io.println(count(seen)); }
"

expect_sig "writing a global one call deeper still counts" \
    'static int count(std::vector<int> list) {' \
    "${IO}let hits = 0;
fn note(): void { hits = hits + 1; }
fn count(list: []int): int { note(); return len(list); }
fn main() { io.println(count([1])); }
"

expect_sig "a method that writes self keeps its parameter a copy" \
    'int Bag::absorb(Bag other) {' \
    "${IO}struct Bag {
    items: []int;
    fn absorb(other: Bag): int { self.items.push(7); return len(other.items); }
}
fn main() { let b: Bag; io.println(b.absorb(b)); }
"

expect_sig "a store through a pointer keeps every parameter a copy" \
    'static int poke(Point* p, Point q) {' \
    "${IO}struct Point { x: int; }
fn poke(p: *Point, q: Point): int { p->x = 5; return q.x; }
fn main() { let a: Point; io.println(poke(&a, a)); }
"

# std::map::operator[] is non-const and inserts on a miss, so indexing a map
# parameter is a write: it must keep the copy it silently mutates today.
expect_sig "indexing a map parameter keeps the copy" \
    'static int look(std::map<std::string, int> m) {' \
    "${IO}fn look(m: map[string]int): int { return m[\"k\"]; }
fn main() { let m: map[string]int; io.println(look(m)); }
"

expect_sig "indexing a map inside a struct parameter keeps the copy" \
    'static int look(Counts c) {' \
    "${IO}struct Counts { byName: map[string]int; }
fn look(c: Counts): int { return c.byName[\"k\"]; }
fn main() { let c: Counts; io.println(look(c)); }
"

# A user method is emitted as a non-const member function, so calling one on a
# parameter needs the copy (and would not compile against a const reference).
expect_sig "calling a method on a struct parameter keeps the copy" \
    'static int report(Point p) {' \
    "${IO}struct Point {
    x: int;
    fn get(): int { return self.x; }
}
fn report(p: Point): int { return p.get(); }
fn main() { let p: Point; io.println(report(p)); }
"

expect_sig "taking a parameter's address keeps the copy" \
    'static int borrow(Point p) {' \
    "${IO}struct Point { x: int; }
fn peek(q: *Point): int { return q->x; }
fn borrow(p: Point): int { return peek(&p); }
fn main() { let p: Point; io.println(borrow(p)); }
"

# inline_cpp! can name and write anything; the analysis stops looking.
expect_sig "inline_cpp disables the optimisation program-wide" \
    'static int total(std::vector<int> xs) {' \
    "${IO}#include <std/inline>
fn total(xs: []int): int { return len(xs); }
fn main() {
    inline_cpp! { int unused = 0; (void)unused; }
    io.println(total([1, 2]));
}
"

# Taking a global's address makes any pointer in the program a possible alias.
expect_sig "taking a global's address disables the optimisation" \
    'static int total(std::vector<int> xs) {' \
    "${IO}let anchor = 0;
fn total(xs: []int): int { return len(xs); }
fn main() {
    let p: *int = &anchor;
    let v: int = *p;
    io.println(total([1, 2]), v);
}
"

# Another thread can write while the callee reads.
expect_sig "a thread disables the optimisation" \
    'static int total(std::vector<int> xs) {' \
    "${IO}#include <std/thread>
fn total(xs: []int): int { return len(xs); }
fn work(): void { io.println(\"tick\"); }
fn main() {
    let t = thread.spawn(work);
    thread.join(t);
    io.println(total([1, 2]));
}
"

# Scalars were already cheap; a reference to an int is a pessimisation.
expect_sig "scalars are still passed by value" \
    'static int add(int a, int b) {' \
    "${IO}fn add(a: int, b: int): int { return a + b; }
fn main() { io.println(add(1, 2)); }
"

# --- string append builds in place instead of rebuilding ---------------------
expect_sig "string += appends in place" \
    's += std::to_string(i);' \
    "${IO}fn main() {
    let s: string = \"\";
    for (i, 3) { s += i; }
    io.println(s);
}
"

expect_sig "a string field += still appends in place" \
    'p.label += "x";' \
    "${IO}struct Tag { label: string; }
fn main() {
    let p: Tag;
    p.label += \"x\";
    io.println(p.label);
}
"

if [ $fails -eq 0 ]; then
    echo "copy_avoidance ok"
    exit 0
fi
echo "copy_avoidance: $fails failure(s)"
exit 1
