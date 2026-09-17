#!/bin/sh
# Cover for gfx.maxfps (BOB-48): the frame limiter.
#
# Two layers, cheapest first, the same shape as the other Tests/gfx_*_cases.sh
# suites:
#
#   codegen   gfx.maxfps(n) must reach __nexa_gfx_maxfps(n), the pacing must be
#             the last thing present() does, and a program that never asks for
#             a cap must carry none of it -- the runtime is sliced to the gfx
#             builtins a program calls (BOB-25). Transpile only (--source), so
#             this layer runs anywhere NexaC does.
#
#   timing    The limiter changes nothing you can look at: no pixel moves and
#             nothing is returned. The only thing it produces is time, so the
#             test measures time. Tests/gfx_maxfps_test.nxa is built against
#             the fake X server in Tests/gfx_x11_stub -- which, unlike the
#             headless layer elsewhere, is switched *on*, because present()
#             returns before it paces when there is no window -- and it times
#             its own loops. Skipped, loudly, when there is no C++ compiler.
#
# Usage: Tests/gfx_maxfps_cases.sh [path-to-NexaC]     (run from the repo root)

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

# transpile <name> <body> [extra NexaC flags...]
transpile() {
    name=$1
    body=$2
    shift 2
    printf '#include <std/gfx>\nfn main() {\n%s\n}\n' "$body" > "$WORK/$name.nxa"
    if ! "$NEXAC" "$WORK/$name.nxa" "$@" --source "$WORK/$name.cpp" \
            > "$WORK/$name.log" 2>&1; then
        echo "FAIL $name: NexaC could not transpile"
        sed 's/^/  /' "$WORK/$name.log"
        fails=$((fails + 1))
        return 1
    fi
    return 0
}

echo "-- codegen: what a cap emits, and what no cap does not"

CAPPED='    gfx.open("t", 8, 8, 1);
    gfx.maxfps(60);
    gfx.present();'
UNCAPPED='    gfx.open("t", 8, 8, 1);
    gfx.present();'

# emits <label> <grep-ERE> <flags...>  -- present in the capped program, absent
# from the uncapped one. Matching a definition rather than a call means the
# program's own main() cannot satisfy it.
emits() {
    label=$1
    want=$2
    shift 2
    if grep -Eq "$want" "$WORK/capped$SUFFIX.cpp"; then
        if grep -Eq "$want" "$WORK/uncapped$SUFFIX.cpp"; then
            echo "FAIL $label: a program with no cap carried /$want/ anyway"
            fails=$((fails + 1))
            return
        fi
        echo "ok $label"
        return
    fi
    echo "FAIL $label: a program with a cap did not carry /$want/"
    fails=$((fails + 1))
}

run_codegen() {
    SUFFIX=$1
    shift
    transpile "capped$SUFFIX" "$CAPPED" "$@" || return
    transpile "uncapped$SUFFIX" "$UNCAPPED" "$@" || return

    emits "the_call$SUFFIX" '^static void __nexa_gfx_maxfps\(int'
    emits "the_clock$SUFFIX" '^static long long __nexa_gfx_now_ns\('
    emits "the_wait$SUFFIX" '^static void __nexa_gfx_pace\('
    emits "the_state$SUFFIX" '^    long long fps_period;'

    # gfx.maxfps(60) is a statement, not a value: what reaches the runtime is
    # the number the program asked for, nothing else.
    if ! grep -q '__nexa_gfx_maxfps(60);' "$WORK/capped$SUFFIX.cpp"; then
        echo "FAIL dispatch$SUFFIX: gfx.maxfps(60) did not reach __nexa_gfx_maxfps(60)"
        fails=$((fails + 1))
    else
        echo "ok dispatch$SUFFIX"
    fi

    # The wait belongs after the blit and after the poll, so the frame's events
    # are already in when it starts and whatever the user does during it is
    # waiting in the OS queue for the next frame.
    if ! sed -n '/^static void __nexa_gfx_present/,/^}/p' "$WORK/capped$SUFFIX.cpp" |
            grep -q '__nexa_gfx_pace();'; then
        echo "FAIL paces_in_present$SUFFIX: present() does not wait out the frame"
        fails=$((fails + 1))
    elif [ "$(sed -n '/^static void __nexa_gfx_present/,/^}/p' "$WORK/capped$SUFFIX.cpp" |
            grep -c '__nexa_gfx_pace();')" != 1 ]; then
        echo "FAIL paces_in_present$SUFFIX: present() waits more than once"
        fails=$((fails + 1))
    else
        echo "ok paces_in_present$SUFFIX"
    fi
}

run_codegen ""
# The wasm target slices the same runtime and paces on the same deadline, with
# emscripten_sleep in place of nanosleep.
run_codegen "_wasm" --wasm

if ! grep -q 'emscripten_sleep' "$WORK/capped_wasm.cpp"; then
    echo "FAIL wasm_sleeps: the wasm cap has no way to wait"
    fails=$((fails + 1))
else
    echo "ok wasm_sleeps"
fi

echo "-- timing: the cap costs the time it says it does"

pick_cxx() {
    if [ -n "${NEXA_CXX:-}" ]; then
        echo "$NEXA_CXX"
        return
    fi
    for c in clang++ g++ c++; do
        if command -v "$c" > /dev/null 2>&1; then
            echo "$c"
            return
        fi
    done
    echo ""
}

CXX=$(pick_cxx)
if [ -z "$CXX" ]; then
    echo "skip gfx maxfps timing: no C++ compiler (set NEXA_CXX to force one)"
else
    # The stub's display starts switched off, so that every other headless
    # suite sees what a machine with no X server sees. This one needs the
    # opposite: no window, no frames, and nothing to pace. One static
    # constructor, run before the program's main, turns it on.
    cat > "$WORK/display_on.cpp" <<'STUB_ON'
#include <X11/Xlib.h>
namespace {
struct NexaStubDisplayOn {
    NexaStubDisplayOn() { nexa_x11_stub_reset(1); }
} nexa_stub_display_on;
}
STUB_ON
    if ! "$NEXAC" "$HERE/gfx_maxfps_test.nxa" --source "$WORK/timing.cpp" \
            > "$WORK/timing.log" 2>&1; then
        echo "FAIL gfx maxfps timing: NexaC could not transpile the test program"
        sed 's/^/  /' "$WORK/timing.log"
        fails=$((fails + 1))
    elif ! "$CXX" -std=c++17 -O1 -I "$HERE/gfx_x11_stub" \
            "$WORK/timing.cpp" "$HERE/gfx_x11_stub/x11_stub.cpp" \
            "$WORK/display_on.cpp" -o "$WORK/timing" > "$WORK/build.log" 2>&1; then
        echo "FAIL gfx maxfps timing: could not build the test program"
        sed 's/^/  /' "$WORK/build.log"
        fails=$((fails + 1))
    else
        out=$("$WORK/timing" 2>&1)
        printf '%s\n' "$out" | grep '^ok ' | sed 's/^ok /ok timing: /'
        if printf '%s\n' "$out" | grep -q '^FAIL '; then
            printf '%s\n' "$out" | grep '^FAIL ' | sed 's/^FAIL /FAIL timing: /'
            fails=$((fails + 1))
        fi
    fi
fi

if [ $fails -eq 0 ]; then
    echo "gfx_maxfps ok"
    exit 0
fi
echo "gfx_maxfps: $fails failure(s)"
exit 1
