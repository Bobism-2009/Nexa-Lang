#!/bin/sh
# try / catch / throw as NexaC's own errors, and where it keeps C++'s exceptions.
#
#   own        A program whose errors NexaC can follow is emitted without C++
#              exceptions (no `try {` and no std::runtime_error in --source), and
#              still catches what it throws.
#   uncaught   An error nothing catches, there: stdout keeps what was printed,
#              stderr says "Uncaught error: <text>", exit status 1. Also from a
#              main written with a type, whose body is a closure.
#   lambda     A function literal that throws, handed straight to xs.map: NexaC
#              writes the loop that calls it, so it is NexaC's own errors too.
#   fallback   A throw NexaC cannot follow -- here a function literal kept in a
#              variable and called through it -- keeps C++'s exceptions, and
#              catches the same.
# The cases in Tests/Lang/try_noexc_*.nxa check the behaviour in detail.
#
# Usage: Tests/throw_cases.sh [path-to-NexaC]     (run from the repo root)

set -u

NEXAC="${1:-./NexaC}"
case "$NEXAC" in
    /*|[A-Za-z]:*) ;;
    *) NEXAC="$(pwd)/$NEXAC" ;;
esac
WORK=$(mktemp -d 2>/dev/null || mktemp -d -t nexathrow)
trap 'rm -rf "$WORK"' EXIT
pass=0
fail=0
ok() { echo "ok   $1"; pass=$((pass + 1)); }
bad() { echo "FAIL $1: $2"; fail=$((fail + 1)); }
run() {  # run NAME: build $WORK/NAME.nxa, run it, leave .out .err and the status in $rc
    if ! "$NEXAC" "$WORK/$1.nxa" -o "$WORK/$1" >"$WORK/$1.log" 2>&1; then
        rc=build
        return
    fi
    bin="$WORK/$1"
    [ -f "$bin.exe" ] && bin="$bin.exe"
    "$bin" >"$WORK/$1.out" 2>"$WORK/$1.err" </dev/null
    rc=$?
    tr -d '\r' < "$WORK/$1.out" > "$WORK/$1.o"
    tr -d '\r' < "$WORK/$1.err" > "$WORK/$1.e"
}

cat > "$WORK/own.nxa" <<'NXA'
#include <std/io>
fn half(n: int): int {
    if (n % 2 != 0) { throw "odd: " + n; }
    return n / 2;
}
fn main() {
    try { io.println(half(8)); io.println(half(3)); } catch (e) { io.println("caught ", e); }
}
NXA
"$NEXAC" "$WORK/own.nxa" --source "$WORK/own.cpp" >/dev/null 2>&1
if grep -q "__nexa_err_msg" "$WORK/own.cpp" && ! grep -q "try {" "$WORK/own.cpp" && ! grep -q "runtime_error" "$WORK/own.cpp"; then
    ok "own: no C++ exceptions in the C++"
else
    bad own "the C++ still uses C++ exceptions"
fi
run own
if [ "$rc" = 0 ] && [ "$(cat "$WORK/own.o")" = "$(printf '4\ncaught odd: 3')" ]; then ok "own: caught"; else bad "own runs" "status $rc, printed [$(cat "$WORK/own.o" 2>/dev/null)]"; fi

cat > "$WORK/uncaught.nxa" <<'NXA'
#include <std/io>
fn deep(n: int): int {
    if (n == 0) { throw "bottom"; }
    return deep(n - 1) + 1;
}
fn main() {
    io.println("before");
    io.println(deep(2));
    io.println("never");
}
NXA
run uncaught
if [ "$rc" = 1 ] && [ "$(cat "$WORK/uncaught.o")" = "before" ] && [ "$(cat "$WORK/uncaught.e")" = "Uncaught error: bottom" ]; then
    ok "uncaught: output kept, message, status 1"
else
    bad uncaught "status $rc, stdout [$(cat "$WORK/uncaught.o" 2>/dev/null)], stderr [$(cat "$WORK/uncaught.e" 2>/dev/null)]"
fi

cat > "$WORK/typed.nxa" <<'NXA'
#include <std/io>
fn get(n: int): int {
    if (n > 1) { throw "get " + n; }
    return n;
}
fn main(): int {
    try { io.println(get(2)); } catch (e) { io.println("caught ", e); }
    io.println(get(1));
    io.println(get(5));
    return 0;
}
NXA
run typed
if [ "$rc" = 1 ] && [ "$(cat "$WORK/typed.o")" = "$(printf 'caught get 2\n1')" ] && [ "$(cat "$WORK/typed.e")" = "Uncaught error: get 5" ]; then
    ok "uncaught in a typed main"
else
    bad "typed main" "status $rc, stdout [$(cat "$WORK/typed.o" 2>/dev/null)], stderr [$(cat "$WORK/typed.e" 2>/dev/null)]"
fi

cat > "$WORK/lambda.nxa" <<'NXA'
#include <std/io>
fn half(n: int): int {
    if (n % 2 != 0) { throw "odd " + n; }
    return n / 2;
}
fn main() {
    let xs = [2, 4, 5];
    try { let ys = xs.map(fn (x) { return half(x); }); io.println(len(ys)); } catch (e) { io.println("caught ", e); }
}
NXA
"$NEXAC" "$WORK/lambda.nxa" --source "$WORK/lambda.cpp" >/dev/null 2>&1
if grep -q "__nexa_err_msg" "$WORK/lambda.cpp" && ! grep -q "try {" "$WORK/lambda.cpp"; then
    ok "lambda: a throwing literal given to xs.map is NexaC's own errors"
else
    bad lambda "the C++ still uses C++ exceptions"
fi
run lambda
if [ "$rc" = 0 ] && [ "$(cat "$WORK/lambda.o")" = "caught odd 5" ]; then ok "lambda: caught"; else bad "lambda runs" "status $rc, printed [$(cat "$WORK/lambda.o" 2>/dev/null)]"; fi

cat > "$WORK/fallback.nxa" <<'NXA'
#include <std/io>
fn half(n: int): int {
    if (n % 2 != 0) { throw "odd " + n; }
    return n / 2;
}
fn main() {
    let f = fn (x: int): int { return half(x); };
    try { io.println(f(4)); io.println(f(5)); } catch (e) { io.println("caught ", e); }
}
NXA
"$NEXAC" "$WORK/fallback.nxa" --source "$WORK/fallback.cpp" >/dev/null 2>&1
if grep -q "try {" "$WORK/fallback.cpp" && ! grep -q "__nexa_err_msg" "$WORK/fallback.cpp"; then
    ok "fallback: a throwing literal called through a variable keeps C++ exceptions"
else
    bad fallback "expected the C++ exception form"
fi
run fallback
if [ "$rc" = 0 ] && [ "$(cat "$WORK/fallback.o")" = "$(printf '2
caught odd 5')" ]; then ok "fallback: caught"; else bad "fallback runs" "status $rc, printed [$(cat "$WORK/fallback.o" 2>/dev/null)]"; fi

echo
echo "$pass passed, $fail failed"
[ "$fail" = 0 ]
