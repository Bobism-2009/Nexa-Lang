#!/bin/sh
# gfx.upload: an image from the program's own pixels, drawn by gfx.blit.
#
# Tests/gfx_upload_test.nxa uploads a []int picture and an RGBA string, draws
# them 1:1 (the copy path), scaled, mirrored and clipped (the sampling path),
# refills a handle in place, and reads every pixel back with gfx.get. Built
# against the fake X server in Tests/gfx_x11_stub with its display switched on,
# since a blit needs a window. Linux only, like the other stub-built layers;
# skipped, loudly, without a C++ compiler.
#
# Usage: Tests/gfx_upload_cases.sh [path-to-NexaC]     (run from the repo root)

set -u

NEXAC="${1:-./NexaC}"
if [ ! -x "$NEXAC" ]; then
    echo "FAIL: NexaC not found or not executable: $NEXAC"
    exit 1
fi
HERE=$(cd "$(dirname "$0")" && pwd)
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT INT TERM
fails=0

CXX="${NEXA_CXX:-}"
if [ -z "$CXX" ]; then
    for c in clang++ g++ c++; do
        if command -v "$c" > /dev/null 2>&1; then CXX=$c; break; fi
    done
fi

if [ -z "$CXX" ]; then
    echo "skip gfx upload: no C++ compiler (set NEXA_CXX to force one)"
elif [ "$(uname -s)" != "Linux" ]; then
    echo "skip gfx upload: it builds against the X11 stand-in, on Linux"
else
    cat > "$WORK/display_on.cpp" <<'STUB_ON'
#include <X11/Xlib.h>
namespace {
struct NexaStubDisplayOn {
    NexaStubDisplayOn() { nexa_x11_stub_reset(1); }
} nexa_stub_display_on;
}
STUB_ON
    if ! "$NEXAC" "$HERE/gfx_upload_test.nxa" --source "$WORK/up.cpp" > "$WORK/t.log" 2>&1; then
        echo "FAIL gfx upload: NexaC could not transpile the test program"
        sed 's/^/  /' "$WORK/t.log"
        fails=$((fails + 1))
    elif ! "$CXX" -std=c++17 -O1 -I "$HERE/gfx_x11_stub" "$WORK/up.cpp" "$HERE/gfx_x11_stub/x11_stub.cpp" \
            "$WORK/display_on.cpp" -o "$WORK/up" > "$WORK/build.log" 2>&1; then
        echo "FAIL gfx upload: could not build the test program"
        sed 's/^/  /' "$WORK/build.log" | tail -8
        fails=$((fails + 1))
    else
        "$WORK/up" > "$WORK/out.txt" 2>&1
        if diff -u --strip-trailing-cr "$HERE/gfx_upload_test.expected" "$WORK/out.txt" > "$WORK/diff.txt"; then
            echo "ok gfx upload: every pixel read back as drawn"
        else
            echo "FAIL gfx upload: pixels moved"
            sed 's/^/  /' "$WORK/diff.txt"
            fails=$((fails + 1))
        fi
    fi
fi

if [ $fails -eq 0 ]; then
    echo "gfx_upload ok"
    exit 0
fi
echo "gfx_upload: $fails failure(s)"
exit 1
