#!/bin/sh
# What a program costs to ship, and the behaviour the size work must keep.
#
# A Nexa program that cannot throw is built without the C++ exception runtime.
# On Windows the static libc++ (and on Linux the static libstdc++) used to pull
# that runtime in anyway -- with the
# C++ demangler and mingw's own printf/strtod -- through operator new, the
# prebuilt std::string members, libc++'s abort message and std::to_string:
# ~200KB on every program that used a string, a slice or a map, against 14KB
# for one that did not. NexaC now closes each of those ways in (see
# kNexaSlimRuntime in NexaC.cpp), and Result.value() / io.to_int no longer need
# exceptions at all.
#
#   size       Windows and Linux: an everyday program -- strings, a slice, a map,
#              an f-string, a Result, io.to_int -- is under 64KB, and prints
#              exactly what the same program built without the slimming
#              (NEXA_NO_SLIM=1) prints.
#
#   literal    Windows and Linux: the std::to_string rewrite leaves a string that
#              happens to spell it alone.
#
#   value      Everywhere: .value() on an error, with no try/catch around it,
#              keeps what the program printed before it, says what the error
#              was on stderr, and exits non-zero.
#
#   catch      Everywhere: with a try/catch in the program, .value() on an
#              error is still an exception the catch receives.
#
#   sh Tests/binary_size_cases.sh           (from the repo root)
#
# Usage: Tests/binary_size_cases.sh [path-to-NexaC]

set -u

NEXAC="${1:-./NexaC}"
case "$NEXAC" in
    /*) ;;
    *) NEXAC="$(pwd)/$NEXAC" ;;
esac
WORK=$(mktemp -d 2>/dev/null || mktemp -d -t nexasize)
trap 'rm -rf "$WORK"' EXIT
FAIL=0
PASS=0
CR=$(printf '\r')

ok() { PASS=$((PASS + 1)); echo "ok   $1"; }
bad() { FAIL=$((FAIL + 1)); echo "FAIL $1: $2"; }

WINDOWS=0
SLIM=0
case "$(uname -s 2>/dev/null)" in
    MINGW*|MSYS*|CYGWIN*) WINDOWS=1; SLIM=1 ;;
    Linux) SLIM=1 ;;
esac
EXE=""
[ "$WINDOWS" = 1 ] && EXE=".exe"

cat > "$WORK/every.nxa" <<'EOF'
#include <std/io>

struct Item {
    name: string;
    qty: int;
}

fn parse(s: string): Result[int] {
    let n = io.to_int(s);
    if (n == 0 && s != "0") { return err("not a number: " + s); }
    return ok(n);
}

fn main() {
    let items = [Item { name: "pen", qty: 3 }, Item { name: "cup", qty: 12 }];
    let totals: map[string]int;
    for (it in items) { totals[it.name] += it.qty; }
    let words = "7,x,-4".split(",");
    let sum = 0;
    for (w in words) {
        let r = parse(w);
        if (r.ok()) { sum += r.value(); } else { io.println(r.error()); }
    }
    io.println(f"{len(items)} items, {totals["cup"]:>4} cups, sum {sum}, {2.5 * sum:.2}");
    io.println(totals, " ", words, " ", 1 + 2 + 3);
}
EOF

if [ "$SLIM" = 1 ]; then
    if "$NEXAC" "$WORK/every.nxa" -o "$WORK/every$EXE" >"$WORK/b1.log" 2>&1 &&
       NEXA_NO_SLIM=1 "$NEXAC" "$WORK/every.nxa" -o "$WORK/stock$EXE" >"$WORK/b2.log" 2>&1; then
        size=$(wc -c < "$WORK/every$EXE" | tr -d ' ')
        if [ "$size" -lt 65536 ]; then ok "size ($size bytes)"; else bad size "$size bytes, over 64KB"; fi
        a=$("$WORK/every$EXE" </dev/null | tr -d "$CR")
        b=$("$WORK/stock$EXE" </dev/null | tr -d "$CR")
        if [ "$a" = "$b" ] && [ -n "$a" ]; then ok "size_same_output"; else bad size_same_output "slim printed [$a], stock printed [$b]"; fi
    else
        bad size "build failed: $(tail -3 "$WORK/b1.log" "$WORK/b2.log" 2>/dev/null)"
    fi

    cat > "$WORK/lit.nxa" <<'EOF'
#include <std/io>
fn main() {
    let n = 5;
    io.println("std::to_string(" + n + ")");
}
EOF
    if "$NEXAC" "$WORK/lit.nxa" -o "$WORK/lit$EXE" >"$WORK/b3.log" 2>&1; then
        out=$("$WORK/lit$EXE" | tr -d "$CR")
        if [ "$out" = "std::to_string(5)" ]; then ok literal; else bad literal "printed [$out]"; fi
    else
        bad literal "build failed: $(tail -3 "$WORK/b3.log")"
    fi
else
    echo "skip size, literal (Windows and Linux only)"
fi

cat > "$WORK/value.nxa" <<'EOF'
#include <std/io>
fn load(): Result[string] { return err("no such file"); }
fn main() {
    io.println("before");
    let s = load().value();
    io.println("after " + s);
}
EOF
if "$NEXAC" "$WORK/value.nxa" -o "$WORK/value$EXE" >"$WORK/b4.log" 2>&1; then
    "$WORK/value$EXE" >"$WORK/v.out" 2>"$WORK/v.err" </dev/null
    code=$?
    out=$(tr -d "$CR" < "$WORK/v.out")
    if [ "$code" -ne 0 ] && [ "$out" = "before" ] && grep -q "no such file" "$WORK/v.err"; then
        ok value
    else
        bad value "exit $code, stdout [$out], stderr [$(cat "$WORK/v.err")]"
    fi
else
    bad value "build failed: $(tail -3 "$WORK/b4.log")"
fi

cat > "$WORK/catch.nxa" <<'EOF'
#include <std/io>
fn load(): Result[string] { return err("no such file"); }
fn main() {
    try {
        let s = load().value();
        io.println("after " + s);
    } catch (e) {
        io.println("caught: " + e);
    }
}
EOF
if "$NEXAC" "$WORK/catch.nxa" -o "$WORK/catch$EXE" >"$WORK/b5.log" 2>&1; then
    out=$("$WORK/catch$EXE" </dev/null 2>&1 | tr -d "$CR")
    if [ "$out" = "caught: no such file" ]; then ok catch; else bad catch "printed [$out]"; fi
else
    bad catch "build failed: $(tail -3 "$WORK/b5.log")"
fi

echo
echo "$PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
