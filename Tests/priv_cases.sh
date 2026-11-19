#!/bin/sh
# A priv name stays inside its file. From outside it is an error.
#
# Usage: Tests/priv_cases.sh [path-to-NexaC]   (from the repo root)

set -u
NEXAC="${1:-./NexaC}"
case "$NEXAC" in
    /*|[A-Za-z]:*) ;;
    *) NEXAC="$(pwd)/$NEXAC" ;;
esac
WORK=$(mktemp -d 2>/dev/null || mktemp -d -t nexapriv)
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

run_eq "a private helper stays inside its file" \
'#include <std/io>
priv fn hide(): int { return 4; }
fn show(): int { return hide(); }
fn main(): int {
    io.println(show());
    return 0;
}' "4"

printf '%s\n' \
'#include <std/io>
priv fn hide(): int { return 4; }
fn show(): int { return hide(); }' > "$WORK/lib.nxa"
printf '%s\n' \
'#include <std/io>
#include "lib.nxa"
fn main(): int {
    io.println(lib.show());
    return 0;
}' > "$WORK/app.nxa"
out=$("$NEXAC" "$WORK/app.nxa" -o "$WORK/app" 2>&1)
if [ $? -ne 0 ]; then
    bad "public call through include" "$out"
else
    bin="$WORK/app"
    [ -f "$bin" ] || bin="$WORK/app.exe"
    got=$(printf '%s' "$("$bin" 2>&1)" | tr -d '\r')
    if [ "$got" = "4" ]; then
        ok "the file can call its own private name"
    else
        bad "public call through include" "$got"
    fi
fi

printf '%s\n' \
'priv fn hide(): int { return 4; }
fn show(): int { return hide(); }' > "$WORK/lib2.nxa"
printf '%s\n' \
'#include "lib2.nxa"
fn main(): int {
    return lib2.hide();
}' > "$WORK/badapp.nxa"
out=$("$NEXAC" "$WORK/badapp.nxa" --source "$WORK/bad.cpp" 2>&1)
if [ $? -eq 0 ]; then
    bad "outside cannot call priv" "NexaC accepted it"
else
    case $out in
        *"private to lib2"*) ok "outside cannot call a private function" ;;
        *) bad "outside cannot call priv" "$out" ;;
    esac
fi

printf '%s\n' \
'priv struct Box { n: int; }
fn make(): int { let b = Box { n: 3 }; return b.n; }' > "$WORK/boxlib.nxa"
printf '%s\n' \
'#include "boxlib.nxa"
fn main(): int {
    let b = boxlib.Box { n: 1 };
    return b.n;
}' > "$WORK/boxapp.nxa"
out=$("$NEXAC" "$WORK/boxapp.nxa" --source "$WORK/box.cpp" 2>&1)
if [ $? -eq 0 ]; then
    bad "outside cannot use priv struct" "NexaC accepted it"
else
    case $out in
        *"private to boxlib"*) ok "outside cannot use a private struct" ;;
        *) bad "outside cannot use priv struct" "$out" ;;
    esac
fi

expect_has "main cannot be priv" \
'priv fn main(): int { return 0; }' \
"fn main cannot be priv"

run_eq "a priv field is usable in its file" \
'#include <std/io>
struct Counter {
    priv count: int;
    fn bump() { self.count = self.count + 1; }
    fn get(): int { return self.count; }
}
fn main(): int {
    let c = Counter { count: 3 };
    c.bump();
    io.println(c.get());
    return 0;
}' "4"

printf '%s\n' \
'struct Counter {
    priv count: int = 0;
    fn bump() { self.count = self.count + 1; }
    fn get(): int { return self.count; }
    priv fn reset() { self.count = 0; }
}' > "$WORK/counter.nxa"
printf '%s\n' \
'#include <std/io>
#include "counter.nxa"
fn main(): int {
    let c = counter.Counter {};
    c.bump();
    io.println(c.get());
    return 0;
}' > "$WORK/counter_app.nxa"
out=$("$NEXAC" "$WORK/counter_app.nxa" -o "$WORK/counter_app" 2>&1)
if [ $? -ne 0 ]; then
    bad "public methods through include" "$out"
else
    bin="$WORK/counter_app"
    [ -f "$bin" ] || bin="$WORK/counter_app.exe"
    got=$(printf '%s' "$("$bin" 2>&1)" | tr -d '\r')
    if [ "$got" = "1" ]; then
        ok "public methods see a priv field"
    else
        bad "public methods through include" "$got"
    fi
fi

printf '%s\n' \
'#include "counter.nxa"
fn main(): int {
    let c = counter.Counter {};
    return c.count;
}' > "$WORK/counter_bad.nxa"
out=$("$NEXAC" "$WORK/counter_bad.nxa" --source "$WORK/counter_bad.cpp" 2>&1)
if [ $? -eq 0 ]; then
    bad "outside cannot read priv field" "NexaC accepted it"
else
    case $out in
        *"private on Counter"*) ok "outside cannot read a private field" ;;
        *) bad "outside cannot read priv field" "$out" ;;
    esac
fi

printf '%s\n' \
'#include "counter.nxa"
fn main(): int {
    let c = counter.Counter {};
    c.reset();
    return 0;
}' > "$WORK/counter_meth.nxa"
out=$("$NEXAC" "$WORK/counter_meth.nxa" --source "$WORK/counter_meth.cpp" 2>&1)
if [ $? -eq 0 ]; then
    bad "outside cannot call priv method" "NexaC accepted it"
else
    case $out in
        *"private on Counter"*) ok "outside cannot call a private method" ;;
        *) bad "outside cannot call priv method" "$out" ;;
    esac
fi

printf '%s\n' \
'#include "counter.nxa"
fn main(): int {
    let c = counter.Counter { count: 9 };
    return 0;
}' > "$WORK/counter_lit.nxa"
out=$("$NEXAC" "$WORK/counter_lit.nxa" --source "$WORK/counter_lit.cpp" 2>&1)
if [ $? -eq 0 ]; then
    bad "outside cannot set priv field in literal" "NexaC accepted it"
else
    case $out in
        *"private on Counter"*) ok "outside cannot set a private field in a literal" ;;
        *) bad "outside cannot set priv field in literal" "$out" ;;
    esac
fi

expect_has "public fn cannot take a priv type" \
'priv struct Secret { n: int; }
fn show(s: Secret): int { return s.n; }
fn main(): int { return 0; }' \
"cannot appear in public"

expect_has "public field cannot be a priv type" \
'priv struct Secret { n: int; }
struct Box { s: Secret; }
fn main(): int { return 0; }' \
"cannot appear in public"

run_eq "a priv field may hold a priv type" \
'#include <std/io>
priv struct Secret { n: int; }
struct Box {
    priv s: Secret;
    fn get(): int { return self.s.n; }
}
fn main(): int {
    let b = Box { s: Secret { n: 4 } };
    io.println(b.get());
    return 0;
}' "4"

expect_has "public method cannot return a priv type" \
'priv struct Secret { n: int; }
struct Box {
    priv s: Secret;
    fn get(): Secret { return self.s; }
}
fn main(): int { return 0; }' \
"cannot appear in public"

if [ $FAIL -ne 0 ]; then
    echo "priv_cases: $FAIL failure(s)"
    exit 1
fi
echo "priv_cases ok"
