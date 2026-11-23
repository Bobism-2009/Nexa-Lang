#!/bin/sh
# --object / --assembly / --llvm-ir: the compiler's own output, with no link.
#
#   object_lib     a file with no fn main() is a library's object: its fns are
#                  exported by their C names, and link into a C program and into a
#                  Nexa program (--link).
#   object_main    a file with fn main() keeps it: the object, linked by the C++
#                  compiler, is the program (arguments included).
#   naming         -o with no extension gets .o / .s / .ll; one with an extension
#                  is kept as given.
#   assembly, ir   the .s holds the fns; the .ll defines them (clang only).
#   refused        two outputs at once, or one with --run / --link / --dll.
#
# Usage: Tests/emit_cases.sh [path-to-NexaC]     (run from the repo root)

set -u

NEXAC="${1:-./NexaC}"
if [ ! -x "$NEXAC" ]; then
    echo "FAIL: NexaC not found or not executable: $NEXAC"
    exit 1
fi
NEXAC=$(cd "$(dirname "$NEXAC")" && pwd)/$(basename "$NEXAC")
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT INT TERM
CR=$(printf '\r')
fails=0
ok() { echo "ok $1"; }
bad() { echo "FAIL $1: $2"; fails=$((fails + 1)); }

EXE=
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) EXE=.exe ;; esac

CXX="${NEXA_CXX:-}"
if [ -z "$CXX" ]; then
    for c in clang++ g++ c++; do
        if command -v "$c" > /dev/null 2>&1; then CXX=$c; break; fi
    done
fi
CC=
for c in clang gcc cc; do
    if command -v "$c" > /dev/null 2>&1; then CC=$c; break; fi
done
STATIC=
if [ -n "$EXE" ]; then
    STATIC=-static
    # NexaC builds for MinGW; an MSVC-targeted clang (the official LLVM) on PATH links
    # with lld-link and cannot take its objects. Use a MinGW one, as NexaC does.
    if [ -z "${NEXA_CXX:-}" ] && ! "$CXX" -dumpmachine 2>/dev/null | grep -q mingw; then
        for c in "${LOCALAPPDATA:-}"/Microsoft/WinGet/Packages/*LLVM-MinGW*/llvm-mingw-*/bin/clang++.exe \
                 /c/tools/llvm-mingw/llvm-mingw-*/bin/clang++.exe; do
            if [ -x "$c" ]; then CXX=$c; break; fi
        done
    fi
fi

cd "$WORK" || exit 1
cat > mathlib.nxa <<'NXA'
fn add(a: int, b: int): int {
    return a + b;
}
fn scale(x: float, k: float): float {
    return x * k;
}
NXA
cat > prog.nxa <<'NXA'
#include <std/io>
fn main(args: []string): void {
    io.println("hello from an object, ", len(args), " arg(s)");
}
NXA
cat > app.nxa <<'NXA'
#include <std/io>
extern fn add(a: int, b: int): int;
fn main() {
    io.println("add ", add(40, 2));
}
NXA
cat > use.c <<'C'
#include <stdio.h>
int add(int, int);
double scale(double, double);
int main(void) { printf("from C: %d %g\n", add(2, 3), scale(1.5, 4.0)); return 0; }
C

# object_lib
if "$NEXAC" mathlib.nxa --object > lib.log 2>&1 && [ -f mathlib.o ]; then
    if [ -n "$CC" ] && "$CC" use.c mathlib.o -o use$EXE > cc.log 2>&1; then
        got=$(./use$EXE | tr -d "$CR")
        if [ "$got" = "from C: 5 6" ]; then ok "object_lib (C)"; else bad object_lib "C program printed [$got]"; fi
    else
        echo "skip object_lib (C): no C compiler, or it would not link: $(tail -2 cc.log 2>/dev/null)"
    fi
    if "$NEXAC" app.nxa --link mathlib.o -o app > app.log 2>&1; then
        got=$(./app$EXE | tr -d "$CR")
        if [ "$got" = "add 42" ]; then ok "object_lib (--link)"; else bad object_lib "--link program printed [$got]"; fi
    else
        bad object_lib "--link failed: $(tail -3 app.log)"
    fi
else
    bad object_lib "no mathlib.o: $(tail -3 lib.log)"
fi

# object_main
if "$NEXAC" prog.nxa -c -o progobj > prog.log 2>&1 && [ -f progobj.o ]; then
    if [ -n "$CXX" ] && "$CXX" progobj.o $STATIC -o prog_linked$EXE > link.log 2>&1; then
        got=$(./prog_linked$EXE a b | tr -d "$CR")
        if [ "$got" = "hello from an object, 3 arg(s)" ]; then ok "object_main"; else bad object_main "printed [$got]"; fi
    else
        bad object_main "$CXX would not link it: $(tail -3 link.log)"
    fi
else
    bad object_main "no progobj.o: $(tail -3 prog.log)"
fi

# naming
"$NEXAC" mathlib.nxa --object -o named.obj > /dev/null 2>&1
"$NEXAC" mathlib.nxa -S -o asmout > /dev/null 2>&1
if [ -f named.obj ] && [ -f asmout.s ]; then ok "naming"; else bad naming "named.obj / asmout.s missing: $(ls)"; fi

# assembly
if [ -f asmout.s ] && grep -q 'add' asmout.s && grep -q 'scale' asmout.s; then ok "assembly"
else bad assembly "asmout.s does not hold add and scale"; fi

# ir
if "$NEXAC" mathlib.nxa --llvm-ir -o ir > ir.log 2>&1; then
    if grep -q '^define.*@add' ir.ll && grep -q '^define.*@scale' ir.ll; then ok "ir"; else bad ir "ir.ll defines no add/scale"; fi
elif grep -q 'needs clang' ir.log; then
    echo "skip ir: the C++ compiler is not clang"
else
    bad ir "$(tail -3 ir.log)"
fi

# refused
for a in "--object --assembly" "--object --run" "--object --dll" "-c --link mathlib.o"; do
    if "$NEXAC" mathlib.nxa $a > ref.log 2>&1; then bad refused "'$a' was accepted"
    elif ! grep -q 'Error' ref.log; then bad refused "'$a' failed without saying why"; fi
done
[ $fails -eq 0 ] && ok "refused"

if [ $fails -eq 0 ]; then
    echo "emit ok"
    exit 0
fi
echo "emit: $fails failure(s)"
exit 1
