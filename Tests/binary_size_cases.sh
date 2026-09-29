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
#              an f-string, a Result, io.to_int, io.parse_int / parse_float /
#              to_float, io.eprintln, time.unix / format -- is under 64KB, and prints
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
#   uncaught   Everywhere, and in every kind of build -- the default, the
#              stock runtime (NEXA_NO_SLIM=1), --debug, and a program with
#              inline_cpp: a throw nothing catches keeps what the program
#              printed, says "Uncaught error: <text>" on stderr and exits
#              non-zero. (The stock handler dropped that output when stdout
#              was a file or a pipe.) Windows and Linux: such a program, which
#              keeps the exception runtime, is still under 128KB.
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
#include <std/time>

struct Item {
    name: string;
    qty: int;
}

fn parse(s: string): Result[int] {
    let n = io.to_int(s);
    if (n == 0 && s != "0") { return err("not a number: " + s); }
    return ok(n);
}

fn twice(s: string): Result[int] {
    return ok(parse(s)? * 2);
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
        let t = twice(w);
        if (t.ok()) { sum += t.value(); }
    }
    io.println(f"{len(items)} items, {totals["cup"]:>4} cups, sum {sum}, {2.5 * sum:.2}");
    io.println(totals, " ", words, " ", 1 + 2 + 3);
    // io.parse_int's error text once brought the exception runtime back
    // (libc++'s prebuilt "text" + s): 16KB became 90KB.
    let p = io.parse_int("12x");
    if (!p.ok()) { io.eprintln(p.error(), " ", sum); }
    io.println(io.parse_float(" 2.5 ").value() + io.to_float("0.5"));
    let when: long = 1000000000;
    io.println(time.format_utc("%F %T", when), " ", time.unix() > when, " ", time.month(when));
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

cat > "$WORK/uncaught.nxa" <<'EOF'
#include <std/io>
fn main() {
    let s = "a";
    try {
        s += "b";
        io.println(s);
    } catch (e) {
        io.println(e);
    }
    throw "boom: " + s;
}
EOF
# The same with C++ of its own in the program, which keeps the stock runtime.
cat > "$WORK/uncaught_cpp.nxa" <<'EOF'
#include <std/io>
#include <std/inline>
fn main() {
    let s = "ab";
    inline_cpp! {
        (void)0;
    }
    io.println(s);
    throw "boom: " + s;
}
EOF

# check NAME SOURCE [ENV=VALUE] [flags...]: built that way, an uncaught throw keeps
# the program's output ("ab"), names what was thrown and exits non-zero.
check_uncaught() {
    name=$1; src=$2; shift 2
    envset=""
    case "${1:-}" in *=*) envset=$1; shift ;; esac
    exe="$WORK/$name$EXE"
    if env $envset "$NEXAC" "$src" "$@" -o "$exe" >"$WORK/$name.log" 2>&1; then
        "$exe" >"$WORK/$name.out" 2>"$WORK/$name.err" </dev/null
        code=$?
        out=$(tr -d "$CR" < "$WORK/$name.out")
        if [ "$code" -ne 0 ] && [ "$out" = "ab" ] && grep -q "Uncaught error: boom: ab" "$WORK/$name.err"; then
            ok "$name"
        else
            bad "$name" "exit $code, stdout [$out], stderr [$(head -3 "$WORK/$name.err")]"
        fi
    else
        bad "$name" "build failed: $(tail -3 "$WORK/$name.log")"
    fi
}

check_uncaught uncaught "$WORK/uncaught.nxa"
check_uncaught uncaught_stock "$WORK/uncaught.nxa" NEXA_NO_SLIM=1
check_uncaught uncaught_debug "$WORK/uncaught.nxa" NEXA_NO_SLIM= --debug
check_uncaught uncaught_inline_cpp "$WORK/uncaught_cpp.nxa"
rm -f "$WORK"/*.debug.cpp 2>/dev/null

if [ "$SLIM" = 1 ] && [ -f "$WORK/uncaught$EXE" ]; then
    size=$(wc -c < "$WORK/uncaught$EXE" | tr -d ' ')
    if [ "$size" -lt 131072 ]; then ok "uncaught_size ($size bytes)"; else bad uncaught_size "$size bytes, over 128KB"; fi
fi

echo
echo "$PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
