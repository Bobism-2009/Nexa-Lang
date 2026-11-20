#!/bin/sh
# for (i in a..b) and for (i in a..<b): counted loops over a range of ints or chars.
#
# Usage: Tests/for_range_cases.sh [path-to-NexaC]   (from the repo root)

set -u
NEXAC="${1:-./NexaC}"
case "$NEXAC" in
    /*|[A-Za-z]:*) ;;
    *) NEXAC="$(pwd)/$NEXAC" ;;
esac
WORK=$(mktemp -d 2>/dev/null || mktemp -d -t nexaforrange)
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


run_eq "a..b takes both ends, a..<b stops short of b" \
'#include <std/io>
fn main() {
    for (i in 1..5) { io.print(" ", i); }
    io.println("");
    for (i in 0..<3) { io.print(" ", i); }
    io.println("");
    let xs = [10, 20, 30];
    for (i in 0..<len(xs)) { io.print(" ", xs[i] + i); }
    io.println("");
    let s = 0;
    for (i in -3..2) { s = s + i; }
    io.println(s);
}' " 1 2 3 4 5
 0 1 2
 10 21 32
-3"

run_eq "empty ranges run nothing; the ends are read once, before the name" \
'#include <std/io>
fn main() {
    let n = 4;
    for (i in n..<n) { io.println("never"); }
    for (i in 5..1) { io.println("never"); }
    let i = 3;
    for (i in 0..i) { io.print(" ", i); }
    io.println("| ", i);
    let hi = 2;
    for (k in 0..hi) { hi = 10; io.print(" ", k); }
    io.println("| ", hi);
}' " 0 1 2 3| 3
 0 1 2| 10"

run_eq "break, continue, chars, long and size_t" \
'#include <std/io>
fn main() {
    for (i in 0..100) { if (i % 2 == 0) { continue; } if (i > 7) { break; } io.print(" ", i); }
    io.println("");
    for (c in '"'"'a'"'"'..'"'"'e'"'"') { io.print(c); }
    for (c in '"'"'x'"'"'..'"'"'z'"'"') { io.print("[" + c + "]"); }
    io.println("");
    let big: long = 3000000000;
    let t: long = 0;
    for (k in big..big + 2) { t = t + k; }
    let m: size_t = 3;
    let u: size_t = 0;
    for (k in 0..<m) { u = u + k; }
    io.println(t, " ", u);
}' " 1 3 5 7
abcde[x][y][z]
9000000003 3"

expect_has "a float end" '#include <std/io>
fn main() { let f = 1.5; for (i in 0..f) { } }' "a range loop counts ints or chars, and a float is neither"

expect_has "a char and an int" '#include <std/io>
fn main() { for (i in '"'"'a'"'"'..5) { } }' "a range loop's two ends are both ints or both chars, and these are a char and an int"

expect_has "a size_t from below 0" '#include <std/io>
fn main() { let u: size_t = 4; for (i in -1..<u) { } }' "this range loop counts in size_t, which is never below 0"

prelude_eq "a range loop brings no helpers or headers" \
'#include <std/io>
fn main() { for (i in 1..3) { io.println(i); } }' \
'#include <std/io>
fn main() { for (i, 3) { io.println(i); } }'

echo ""
if [ "$FAIL" -gt 0 ]; then
    echo "FAILED $FAIL test(s)"
    exit 1
else
    echo "All tests passed"
fi
