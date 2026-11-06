#!/bin/sh
# gfx.pads / pad / pad_pressed / pad_axis: gamepads.
#
# No test machine has a gamepad, so the Linux reader is given a joystick the
# test makes itself. Tests/gfx_pad_test.nxa writes the kernel's joystick events
# into a file and appends to it between polls; this builds the program's C++
# with NEXA_GFX_PAD_PATH pointed at that file instead of /dev/input/js%d, so
# everything from the read() on is the code a real pad reaches: the event
# parsing, the xpad layout, the dead zone, the triggers, the d-pad, and the
# pressed edge between two gfx.poll()s. Built against the X11 stand-in in
# Tests/gfx_x11_stub, as the other gfx layers are; no window is opened.
#
#   sliced   a gfx program that reads no pad carries none of the reader.
#   sliced3d a gfx3d program that reads no pad carries none of it either.
#   shared   a gfx3d program that reads one carries the reader exactly once,
#            and a program using both modules still carries only one copy.
#   hook3d   gfx3d.poll takes the snapshot pad_pressed measures against, and a
#            gfx3d program that reads no pad has no call to it at all.
#   rumble   gfx.rumble and gfx3d.rumble reach XInputSetState, and a program
#            that reads a pad without shaking one carries none of that.
#   reads    the test program, against its expected output.
#
# Linux only; skipped, loudly, without a C++ compiler.
#
# Usage: Tests/gfx_pad_cases.sh [path-to-NexaC]     (run from the repo root)

set -u

NEXAC="${1:-./NexaC}"
if [ ! -x "$NEXAC" ]; then
    echo "FAIL: NexaC not found or not executable: $NEXAC"
    exit 1
fi
NEXAC=$(cd "$(dirname "$NEXAC")" && pwd)/$(basename "$NEXAC")
HERE=$(cd "$(dirname "$0")" && pwd)
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT INT TERM
fails=0

printf '#include <std/gfx>\nfn main() {\n    gfx.open("x", 8, 8, 1);\n    gfx.poll();\n    gfx.close();\n}\n' > "$WORK/nopad.nxa"
if ! "$NEXAC" "$WORK/nopad.nxa" --source "$WORK/nopad.cpp" > "$WORK/t.log" 2>&1; then
    echo "FAIL sliced: NexaC could not transpile a gfx program"
    fails=$((fails + 1))
elif grep -q '__nexa_gfx_pad\|__nexa_pads\|XInputGetState\|NEXA_GFX_PAD_PATH' "$WORK/nopad.cpp"; then
    echo "FAIL sliced: a program that reads no gamepad carries the reader"
    fails=$((fails + 1))
else
    echo "ok sliced: no pad call, no pad code"
fi

# std/gfx3d reads the same pads through the same reader, emitted once ahead of
# both runtimes rather than inside gfx's (gfxPadStackCpp in GfxRuntime.hpp).
# What that must not do is put the reader into a 3D program that never asks for
# a pad, or two copies of it into one that draws with both modules.
printf '#include <std/gfx3d>
fn main(): int {
    gfx3d.open("x", 8, 8);
    gfx3d.poll();
    gfx3d.close();
    return 0;
}
' > "$WORK/nopad3d.nxa"
if ! "$NEXAC" "$WORK/nopad3d.nxa" --source "$WORK/nopad3d.cpp" > "$WORK/t.log" 2>&1; then
    echo "FAIL sliced3d: NexaC could not transpile a gfx3d program"
    fails=$((fails + 1))
elif grep -q '__nexa_gfx_pad\|__nexa_pads\|XInputGetState\|NEXA_GFX_PAD_PATH' "$WORK/nopad3d.cpp"; then
    echo "FAIL sliced3d: a 3D program that reads no gamepad carries the reader"
    fails=$((fails + 1))
else
    echo "ok sliced3d: no pad call, no pad code"
fi

printf '#include <std/io>
#include <std/gfx3d>
fn main(): int {
    io.println(gfx3d.pads());
    io.println(gfx3d.pad(0, "a"));
    io.println(gfx3d.pad_pressed(0, "start"));
    return 0;
}
' > "$WORK/pad3d.nxa"
printf '#include <std/io>
#include <std/gfx>
#include <std/gfx3d>
fn main(): int {
    io.println(gfx.pads());
    io.println(gfx3d.pad(0, "a"));
    return 0;
}
' > "$WORK/padboth.nxa"
for case in pad3d padboth; do
    if ! "$NEXAC" "$WORK/$case.nxa" --source "$WORK/$case.cpp" > "$WORK/t.log" 2>&1; then
        echo "FAIL shared: NexaC could not transpile $case"
        sed 's/^/  /' "$WORK/t.log" | tail -5
        fails=$((fails + 1))
    else
        n=$(grep -c '^static void __nexa_gfx_pad_sample' "$WORK/$case.cpp")
        if [ "$n" = "1" ]; then
            echo "ok shared: $case carries one reader"
        else
            echo "FAIL shared: $case carries $n copies of the reader, want 1"
            fails=$((fails + 1))
        fi
    fi
done

# The snapshot gfx3d.poll takes. Absent from a 3D program that reads no pad --
# not a call to an empty stub, which is what would make a pad-free program's C++
# differ from what it was before gfx3d could read one at all.
if grep -q '__nexa_gfx_pad_frame();' "$WORK/pad3d.cpp"; then
    echo "ok hook3d: gfx3d.poll takes the pad snapshot"
else
    echo "FAIL hook3d: gfx3d.poll does not sample the pads"
    fails=$((fails + 1))
fi
if grep -q '__nexa_gfx_pad_frame' "$WORK/nopad3d.cpp"; then
    echo "FAIL hook3d: a pad-free gfx3d program still names the snapshot"
    fails=$((fails + 1))
else
    echo "ok hook3d: no pad, no snapshot and no stub"
fi

# gfx.rumble / gfx3d.rumble. Sliced apart from reading, because most programs
# that read a pad never shake one: the reader must come out with no
# XInputSetState and no slot lookup in it.
printf '#include <std/io>
#include <std/gfx>
fn main(): int {
    io.println(gfx.pad(0, "a"));
    io.println(gfx.rumble(0, 1.0));
    io.println(gfx.rumble(0, 0.0, 0.5));
    return 0;
}
' > "$WORK/rum.nxa"
printf '#include <std/io>
#include <std/gfx3d>
fn main(): int {
    io.println(gfx3d.rumble(0, 0.5));
    return 0;
}
' > "$WORK/rum3d.nxa"
for case in rum rum3d; do
    if ! "$NEXAC" "$WORK/$case.nxa" --source "$WORK/$case.cpp" > "$WORK/t.log" 2>&1; then
        echo "FAIL rumble: NexaC could not transpile $case"
        sed 's/^/  /' "$WORK/t.log" | tail -5
        fails=$((fails + 1))
    elif ! grep -q 'XInputSetState' "$WORK/$case.cpp"; then
        echo "FAIL rumble: $case does not reach XInputSetState"
        fails=$((fails + 1))
    elif ! grep -q '__nexa_gfx_rumble' "$WORK/$case.cpp"; then
        echo "FAIL rumble: $case has no rumble call"
        fails=$((fails + 1))
    else
        echo "ok rumble: $case drives the motors"
    fi
done

# One strength shakes both motors; two drive them apart. Checked in the emitted
# call rather than by running it, so this holds on a machine with no pad.
if grep -q '__nexa_gfx_rumble(0, 1.0, 1.0)' "$WORK/rum.cpp" &&
   grep -q '__nexa_gfx_rumble(0, 0.0, 0.5)' "$WORK/rum.cpp"; then
    echo "ok rumble: one strength drives both motors, two drive them apart"
else
    echo "FAIL rumble: the two forms do not emit the motor pair expected"
    grep -n '__nexa_gfx_rumble' "$WORK/rum.cpp" | sed 's/^/  /'
    fails=$((fails + 1))
fi

for case in nopad pad3d padboth; do
    if grep -q 'XInputSetState\|__nexa_gfx_pad_slot' "$WORK/$case.cpp"; then
        echo "FAIL rumble: $case reads a pad but still carries the rumble code"
        fails=$((fails + 1))
    else
        echo "ok rumble: $case reads without carrying rumble"
    fi
done

CXX="${NEXA_CXX:-}"
if [ -z "$CXX" ]; then
    for c in clang++ g++ c++; do
        if command -v "$c" > /dev/null 2>&1; then CXX=$c; break; fi
    done
fi

if [ -z "$CXX" ]; then
    echo "skip reads: no C++ compiler (set NEXA_CXX to force one)"
elif [ "$(uname -s)" != "Linux" ]; then
    echo "skip reads: the joystick device is Linux's"
elif ! "$NEXAC" "$HERE/gfx_pad_test.nxa" --source "$WORK/pad.cpp" > "$WORK/t.log" 2>&1; then
    echo "FAIL reads: NexaC could not transpile the test program"
    sed 's/^/  /' "$WORK/t.log"
    fails=$((fails + 1))
elif ! "$CXX" -std=c++17 -O1 '-DNEXA_GFX_PAD_PATH="_nexa_pad/js%d"' -I "$HERE/gfx_x11_stub" "$WORK/pad.cpp" \
        "$HERE/gfx_x11_stub/x11_stub.cpp" -o "$WORK/pad" > "$WORK/build.log" 2>&1; then
    echo "FAIL reads: could not build the test program"
    sed 's/^/  /' "$WORK/build.log" | tail -8
    fails=$((fails + 1))
else
    (cd "$WORK" && ./pad > out.txt 2>&1)
    if diff -u --strip-trailing-cr "$HERE/gfx_pad_test.expected" "$WORK/out.txt" > "$WORK/diff.txt"; then
        echo "ok reads: buttons, sticks, triggers, d-pad and the pressed edge"
    else
        echo "FAIL reads: the pad read differently"
        sed 's/^/  /' "$WORK/diff.txt"
        fails=$((fails + 1))
    fi
fi

if [ $fails -eq 0 ]; then
    echo "gfx_pad ok"
    exit 0
fi
echo "gfx_pad: $fails failure(s)"
exit 1
