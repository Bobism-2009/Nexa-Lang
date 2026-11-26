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
#   fnvalue    A throwing literal kept in a variable and called through it is
#              NexaC's own errors as well (every call through a function value
#              is tested), and one nothing catches is "Uncaught error", status 1.
#   defer      A throw in a defer is NexaC's own errors and reaches the catch
#              around the call; so is one a break, continue or return from
#              main leaves the block of. With a goto out of the block it keeps
#              C++'s exceptions, and is caught there too.
#   global     A global whose value throws: NexaC's own errors too, "Uncaught
#              error" and status 1, before main.
#   thread     A throw on another thread is NexaC's own errors (one per thread);
#              one nothing on that thread catches is "Uncaught error", status 1.
#   library    A --dll / --shared library catches its own errors; one it does
#              not catch ends the host with "Uncaught error", status 1.
#   cheader    A C standard library header (<math.h>) cannot throw: own errors.
#   fallback   A throw NexaC cannot follow -- here in a program with a C++
#              header, which may throw its own -- keeps C++'s exceptions, and
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

cat > "$WORK/fnvalue.nxa" <<'NXA'
#include <std/io>
fn half(n: int): int {
    if (n % 2 != 0) { throw "odd " + n; }
    return n / 2;
}
fn main() {
    let f = fn (x: int): int { return half(x); };
    try { io.println(f(4)); io.println(f(5)); } catch (e) { io.println("caught ", e); }
    io.println(f(7));
    io.println("never");
}
NXA
"$NEXAC" "$WORK/fnvalue.nxa" --source "$WORK/fnvalue.cpp" >/dev/null 2>&1
if grep -q "__nexa_err_msg" "$WORK/fnvalue.cpp" && ! grep -q "try {" "$WORK/fnvalue.cpp"; then
    ok "fnvalue: a throwing literal kept in a variable is NexaC's own errors"
else
    bad fnvalue "the C++ still uses C++ exceptions"
fi
run fnvalue
if [ "$rc" = 1 ] && [ "$(cat "$WORK/fnvalue.o")" = "$(printf '2
caught odd 5')" ] && [ "$(cat "$WORK/fnvalue.e")" = "Uncaught error: odd 7" ]; then
    ok "fnvalue: caught, then uncaught with status 1"
else
    bad "fnvalue runs" "status $rc, stdout [$(cat "$WORK/fnvalue.o" 2>/dev/null)], stderr [$(cat "$WORK/fnvalue.e" 2>/dev/null)]"
fi

cat > "$WORK/defer.nxa" <<'NXA'
#include <std/io>
fn half(n: int): int {
    if (n % 2 != 0) { throw "odd " + n; }
    return n / 2;
}
fn work(n: int) {
    defer { io.println(half(n)); }
    io.println("work ", n);
}
fn main() {
    try { work(4); work(5); } catch (e) { io.println("caught ", e); }
}
NXA
"$NEXAC" "$WORK/defer.nxa" --source "$WORK/defer.cpp" >/dev/null 2>&1
if grep -q "__nexa_err_msg" "$WORK/defer.cpp" && ! grep -q "try {" "$WORK/defer.cpp"; then
    ok "defer: a throw in a defer is NexaC's own errors"
else
    bad defer "the C++ still uses C++ exceptions"
fi
want=$(printf 'work 4\n2\nwork 5\ncaught odd 5')
run defer
if [ "$rc" = 0 ] && [ "$(cat "$WORK/defer.o")" = "$want" ]; then ok "defer: caught"; else bad "defer runs" "status $rc, printed [$(cat "$WORK/defer.o" 2>/dev/null)]"; fi
# A break out of the defer's block would jump past the test: C++'s exceptions, same output.
cat > "$WORK/deferx.nxa" <<'NXA'
#include <std/io>
fn half(n: int): int {
    if (n % 2 != 0) { throw "odd " + n; }
    return n / 2;
}
fn work(n: int) {
    while (true) {
        defer { io.println(half(n)); }
        io.println("work ", n);
        break;
    }
}
fn main() {
    try { work(4); work(5); } catch (e) { io.println("caught ", e); }
    defer io.println(half(7));
    io.println("main ends");
    return 3;
}
NXA
"$NEXAC" "$WORK/deferx.nxa" --source "$WORK/deferx.cpp" >/dev/null 2>&1
run deferx
if grep -q "__nexa_err_msg" "$WORK/deferx.cpp" && ! grep -q "try {" "$WORK/deferx.cpp" && [ "$rc" = 1 ] &&
   [ "$(tail -2 "$WORK/deferx.o")" = "$(printf 'caught odd 5\nmain ends')" ] && grep -q "Uncaught error: odd 7" "$WORK/deferx.e"; then
    ok "defer: a break or return from main past it is NexaC's own errors, and is caught"
else
    bad "deferx" "status $rc, printed [$(cat "$WORK/deferx.o" 2>/dev/null)]"
fi

cat > "$WORK/defergoto.nxa" <<'NXA'
#include <std/io>
fn half(n: int): int {
    if (n % 2 != 0) { throw "odd " + n; }
    return n / 2;
}
fn work(n: int) {
    if (n > 0) {
        defer { io.println(half(n)); }
        io.println("work ", n);
        goto out;
    }
out:
    io.println("out");
}
fn main() {
    try { work(4); work(5); } catch (e) { io.println("caught ", e); }
}
NXA
"$NEXAC" "$WORK/defergoto.nxa" --source "$WORK/defergoto.cpp" >/dev/null 2>&1
run defergoto
if grep -q "try {" "$WORK/defergoto.cpp" && [ "$rc" = 0 ] && [ "$(tail -2 "$WORK/defergoto.o")" = "$(printf 'work 5\ncaught odd 5')" ]; then
    ok "defer: a goto past it keeps C++ exceptions, and is caught"
else
    bad "defergoto" "status $rc, printed [$(cat "$WORK/defergoto.o" 2>/dev/null)]"
fi

cat > "$WORK/global.nxa" <<'NXA'
#include <std/io>
fn half(n: int): int {
    if (n % 2 != 0) { throw "odd " + n; }
    return n / 2;
}
let A = half(8);
let B = half(A + 1);
fn main() {
    io.println("not here");
}
NXA
"$NEXAC" "$WORK/global.nxa" --source "$WORK/global.cpp" >/dev/null 2>&1
run global
if grep -q "__nexa_err_msg" "$WORK/global.cpp" && [ "$rc" = 1 ] && [ ! -s "$WORK/global.o" ] && grep -q "Uncaught error: odd 5" "$WORK/global.e"; then
    ok "global: a global's throw is NexaC's own errors, uncaught with status 1"
else
    bad "global" "status $rc, printed [$(cat "$WORK/global.o" 2>/dev/null)] [$(cat "$WORK/global.e" 2>/dev/null)]"
fi

cat > "$WORK/thread.nxa" <<'NXA'
#include <std/io>
#include <std/thread>
fn half(n: int): int {
    if (n % 2 != 0) { throw "odd " + n; }
    return n / 2;
}
fn work(n: int) {
    try { io.println(half(n)); } catch (e) { io.println("caught ", e); }
}
fn boom() {
    io.println(half(3));
}
fn main() {
    let t = thread.spawn(work(5));
    thread.join(t);
    let u = thread.spawn(boom);
    thread.join(u);
    io.println("never");
}
NXA
"$NEXAC" "$WORK/thread.nxa" --source "$WORK/thread.cpp" >/dev/null 2>&1
if grep -q "thread_local bool __nexa_err" "$WORK/thread.cpp" && ! grep -q "try {" "$WORK/thread.cpp"; then
    ok "thread: a throw on another thread is NexaC's own errors"
else
    bad thread "the C++ still uses C++ exceptions"
fi
run thread
if [ "$rc" = 1 ] && [ "$(cat "$WORK/thread.o")" = "caught odd 5" ] && [ "$(cat "$WORK/thread.e")" = "Uncaught error: odd 3" ]; then
    ok "thread: caught, then uncaught on a thread with status 1"
else
    bad "thread runs" "status $rc, stdout [$(cat "$WORK/thread.o" 2>/dev/null)], stderr [$(cat "$WORK/thread.e" 2>/dev/null)]"
fi

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) libflag=--dll; libfile="$WORK/errlib.dll" ;;
    Darwin) libflag=--shared; libfile="$WORK/errlib.dylib" ;;
    *) libflag=--shared; libfile="$WORK/errlib.so" ;;
esac
cat > "$WORK/errlib.nxa" <<'NXA'
#include <std/io>
fn half(n: int): int {
    if (n % 2 != 0) { throw "odd " + n; }
    return n / 2;
}
fn Safe(n: int): int {
    try { let r = half(n); io.println("safe ", r); return r; } catch (e) { io.println("lib caught ", e); }
    return -1;
}
fn Say(s: string) {
    io.println("say ", s, " ", half(s.len()));
}
fn Both(n: int) {
    try { Say("xyz"); } catch (e) { io.println("both caught ", e); }
    io.println("both ", Safe(n));
}
NXA
# The host loads it by the path the OS knows (C:/... on Windows, not /tmp/...).
libpath=$libfile
command -v cygpath >/dev/null 2>&1 && libpath=$(cygpath -m "$libfile")
cat > "$WORK/errhost.nxa" <<NXA
#include <std/io>
#include <std/dll>
fn main() {
    let h = dll.load("$libpath");
    dll.call(h, "Safe", 8);
    dll.call(h, "Safe", 3);
    dll.call(h, "Both", 6);
    dll.call(h, "Say", "abc");
    io.println("never");
}
NXA
"$NEXAC" "$WORK/errlib.nxa" $libflag --source "$WORK/errlib.cpp" >/dev/null 2>&1
if "$NEXAC" "$WORK/errlib.nxa" $libflag -o "$libfile" >"$WORK/errlib.log" 2>&1 &&
   grep -q "__nexa_err_msg" "$WORK/errlib.cpp" && ! grep -q "try {" "$WORK/errlib.cpp"; then
    ok "library: a library's errors are NexaC's own"
    run errhost
    want=$(printf 'safe 4\nlib caught odd 3\nboth caught odd 3\nsafe 3\nboth 3')
    if [ "$rc" = 1 ] && [ "$(cat "$WORK/errhost.o")" = "$want" ] && [ "$(cat "$WORK/errhost.e")" = "Uncaught error: odd 3" ]; then
        ok "library: caught inside, uncaught at the caller with status 1"
    else
        bad "library runs" "status $rc, stdout [$(cat "$WORK/errhost.o" 2>/dev/null)], stderr [$(cat "$WORK/errhost.e" 2>/dev/null)]"
    fi
else
    bad library "$(head -5 "$WORK/errlib.log")"
fi

cat > "$WORK/cheader.nxa" <<'NXA'
#include <std/io>
#include <math.h>
fn root(x: float): float {
    if (x < 0.0) { throw "negative"; }
    return sqrt(x);
}
fn main() {
    try { io.println(root(16.0)); io.println(root(-1.0)); } catch (e) { io.println("caught ", e); }
}
NXA
"$NEXAC" "$WORK/cheader.nxa" --source "$WORK/cheader.cpp" >/dev/null 2>&1
if grep -q "__nexa_err_msg" "$WORK/cheader.cpp" && ! grep -q "try {" "$WORK/cheader.cpp"; then
    ok "cheader: a C standard library header keeps NexaC's own errors"
else
    bad cheader "the C++ still uses C++ exceptions"
fi
run cheader
if [ "$rc" = 0 ] && [ "$(cat "$WORK/cheader.o")" = "$(printf '4\ncaught negative')" ]; then ok "cheader: caught"; else bad "cheader runs" "status $rc, printed [$(cat "$WORK/cheader.o" 2>/dev/null)]"; fi

cat > "$WORK/fallback.nxa" <<'NXA'
#include <std/io>
#include <algorithm>
fn half(n: int): int {
    if (n % 2 != 0) { throw "odd " + n; }
    return n / 2;
}
fn main() {
    try { io.println(half(5)); } catch (e) { io.println("caught ", e); }
}
NXA
"$NEXAC" "$WORK/fallback.nxa" --source "$WORK/fallback.cpp" >/dev/null 2>&1
if grep -q "try {" "$WORK/fallback.cpp" && ! grep -q "__nexa_err_msg" "$WORK/fallback.cpp"; then
    ok "fallback: a C++ header keeps C++ exceptions"
else
    bad fallback "expected the C++ exception form"
fi
run fallback
if [ "$rc" = 0 ] && [ "$(cat "$WORK/fallback.o")" = "caught odd 5" ]; then ok "fallback: caught"; else bad "fallback runs" "status $rc, printed [$(cat "$WORK/fallback.o" 2>/dev/null)]"; fi

echo
echo "$pass passed, $fail failed"
[ "$fail" = 0 ]
