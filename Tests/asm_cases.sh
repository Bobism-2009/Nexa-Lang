#!/bin/sh
# inline_asm!: assembly in a Nexa function, with Nexa variables as its operands.
#
# The assembly is the CPU's own, so each layer runs the program written for the
# machine it is on, and skips the others:
#
#   sliced    a program without inline_asm! carries none of its macros.
#   x86       Intel syntax (the default) and AT&T: ints, a long, a label in a loop in a
#             function called twice, a double, a pointer with a clobber; and a function
#             written at file scope, called through extern fn. Linux also builds it
#             with g++ (NEXA_CXX=g++), which reads Intel syntax a different way.
#   arm64     the same in arm64 assembly: natively on an arm64 machine, or built with
#             --target arm64-linux and run under qemu-aarch64 when both are there.
#   slim      Windows: the program keeps NexaC's own entry point (under 8KB).
#
# Usage: Tests/asm_cases.sh [path-to-NexaC]     (run from the repo root)

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

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) OS=windows; EXE=.exe ;;
    Darwin) OS=mac; EXE= ;;
    *) OS=linux; EXE= ;;
esac
ARCH=$(uname -m)
case "$ARCH" in
    x86_64|amd64|i686|i386) ARCH=x86 ;;
    aarch64|arm64) ARCH=arm64 ;;
esac

# run NAME SRC WANT [env...]: build SRC, run it, compare its output with WANT.
build_run() {
    name=$1; src=$2; want=$3; shift 3
    if ! env "$@" "$NEXAC" "$src" -o "$WORK/$name$EXE" > "$WORK/$name.log" 2>&1; then
        bad "$name" "build failed: $(tail -4 "$WORK/$name.log")"
        return 1
    fi
    got=$("$WORK/$name$EXE" 2>&1 | tr -d "$CR")
    if [ "$got" = "$want" ]; then ok "$name"; else bad "$name" "got [$got], want [$want]"; fi
}

WANT='sum 42
twice 24
steps 10 3
float 2.5
third 30'

# sliced
printf '#include <std/io>\nfn main() {\n    io.println("hi");\n}\n' > "$WORK/plain.nxa"
if "$NEXAC" "$WORK/plain.nxa" --source "$WORK/plain.cpp" > "$WORK/plain.log" 2>&1; then
    if grep -q '__NEXA_ASM\|__asm__' "$WORK/plain.cpp"; then bad sliced "a program without inline_asm! carries its macros"
    else ok "sliced"; fi
else
    bad sliced "could not transpile: $(tail -3 "$WORK/plain.log")"
fi

cat > "$WORK/x86.nxa" <<'NXA'
#include <std/io>
#include <std/inline>
fn twice(x: int): int {
    let r = 0;
    inline_asm!(in x, out r) {
        mov {r}, {x}
        add {r}, {r}      // doubled
    }
    return r;
}
fn count_down(n: long): long {
    let steps: long = 0;
    inline_asm!(inout n, inout steps) {
    again:
        dec {n}
        inc {steps}
        cmp {n}, 0
        jg again
    }
    return steps;
}
fn main() {
    let a = 20;
    let b = 22;
    let sum = 0;
    inline_asm!(in a, in b, out sum) {
        mov {sum}, {a}
        add {sum}, {b}
    }
    io.println("sum ", sum);
    io.println("twice ", twice(5) + twice(7));
    io.println("steps ", count_down(10), " ", count_down(3));
    let f = 1.25;
    let g = 0.0;
    inline_asm!(in f, out g) {
        movsd {g}, {f}
        addsd {g}, {g}
    }
    io.println("float ", g);
    let xs = [10, 20, 30, 40];
    let p = &xs[0];
    let third = 0;
    inline_asm!(in p, out third, clobber eax) {
        mov eax, DWORD PTR [{p} + 8]
        mov {third}, eax
    }
    io.println("third ", third);
}
NXA

cat > "$WORK/att.nxa" <<'NXA'
#include <std/io>
#include <std/inline>
fn main() {
    let a = 40;
    let b = 0;
    inline_asm!(att, in a, out b) {
        movl {a}, {b}
        addl $2, {b}
    }
    io.println("att ", b);
}
NXA

# The first argument: rcx on Windows, rdi elsewhere (System V).
ARG=rdi
[ "$OS" = windows ] && ARG=rcx
cat > "$WORK/fs.nxa" <<NXA
#include <std/io>
#include <std/inline>
inline_asm! {
    .globl add_one
add_one:
    lea eax, [$ARG + 1]
    ret
}
extern fn add_one(x: int): int;
fn main() {
    io.println("add_one ", add_one(41));
}
NXA

cat > "$WORK/arm64.nxa" <<'NXA'
#include <std/io>
#include <std/inline>
fn twice(x: int): int {
    let r = 0;
    inline_asm!(in x, out r) {
        add {r}, {x}, {x}
    }
    return r;
}
fn count_down(n: long): long {
    let steps: long = 0;
    inline_asm!(inout n, inout steps) {
    again:
        sub {n}, {n}, #1
        add {steps}, {steps}, #1
        cmp {n}, #0
        b.gt again
    }
    return steps;
}
fn main() {
    let a = 20;
    let b = 22;
    let sum = 0;
    inline_asm!(in a, in b, out sum) {
        add {sum}, {a}, {b}
    }
    io.println("sum ", sum);
    io.println("twice ", twice(5) + twice(7));
    io.println("steps ", count_down(10), " ", count_down(3));
    let f = 1.25;
    let g = 0.0;
    inline_asm!(in f, out g) {
        fadd {g}, {f}, {f}
    }
    io.println("float ", g);
    let xs = [10, 20, 30, 40];
    let p = &xs[0];
    let third = 0;
    inline_asm!(in p, out third, clobber x9) {
        ldr w9, [{p}, #8]
        mov {third}, w9
    }
    io.println("third ", third);
}
NXA

if [ "$ARCH" = x86 ]; then
    build_run x86_intel "$WORK/x86.nxa" "$WANT"
    build_run x86_att "$WORK/att.nxa" "att 42"
    if [ "$OS" != mac ]; then
        build_run x86_file_scope "$WORK/fs.nxa" "add_one 42"
    else
        echo "skip x86_file_scope: Mach-O names the symbol _add_one"
    fi
    if [ "$OS" = linux ] && [ -z "${NEXA_CXX:-}" ] && command -v g++ > /dev/null 2>&1; then
        build_run x86_intel_gcc "$WORK/x86.nxa" "$WANT" NEXA_CXX=g++
        build_run x86_file_scope_gcc "$WORK/fs.nxa" "add_one 42" NEXA_CXX=g++
    else
        echo "skip x86 g++: not Linux, NEXA_CXX set, or no g++"
    fi
    if [ "$OS" = windows ] && [ -f "$WORK/x86_intel$EXE" ]; then
        size=$(wc -c < "$WORK/x86_intel$EXE" | tr -d ' ')
        if [ "$size" -lt 8192 ]; then ok "slim ($size bytes)"; else bad slim "$size bytes: lost the slim entry?"; fi
    fi
else
    echo "skip x86: this machine is $ARCH"
fi

if [ "$ARCH" = arm64 ]; then
    build_run arm64 "$WORK/arm64.nxa" "$WANT"
elif command -v qemu-aarch64 > /dev/null 2>&1; then
    if "$NEXAC" "$WORK/arm64.nxa" --target arm64-linux -o "$WORK/arm64x" > "$WORK/arm64x.log" 2>&1; then
        got=$(qemu-aarch64 "$WORK/arm64x" 2>&1 | tr -d "$CR")
        if [ "$got" = "$WANT" ]; then ok "arm64 (qemu)"; else bad arm64 "got [$got]"; fi
    else
        echo "skip arm64: no arm64-linux target installed ($(tail -1 "$WORK/arm64x.log"))"
    fi
else
    echo "skip arm64: not an arm64 machine and no qemu-aarch64"
fi

if [ $fails -eq 0 ]; then
    echo "asm ok"
    exit 0
fi
echo "asm: $fails failure(s)"
exit 1
