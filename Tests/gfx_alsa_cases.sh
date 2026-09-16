#!/bin/sh
# Cover for the Linux ALSA backend behind gfx.audio() (BOB-41).
#
# The backend reaches ALSA through dlopen and hand-declared function pointers so
# that neither building nor running a Nexa program needs libasound. That is a
# portability decision first, but it also makes the backend testable on a
# machine with no sound card and no ALSA installed -- which is every machine
# this suite has run on so far. Tests/gfx_alsa_stub/alsa_stub.cpp is a shared
# object exporting the eight names the backend looks up, put on LD_LIBRARY_PATH
# as libasound.so.2, and as far as a generated program can tell it is ALSA.
#
# Each case runs Tests/gfx_alsa_test.nxa against one version of that device and
# checks two things: what the public API returned, which the program prints, and
# what the backend asked the device for, which the fake library traces. The
# trace is the half that matters, because the API cannot show whether the stream
# was opened non-blocking on "default", whether it was configured S16_LE mono at
# the rate asked for with a tenth of a second of latency, or whether an underrun
# was recovered rather than swallowed.
#
# The cases that are not about a working device are the ones the philosophy
# rests on: no libasound, a libasound that will not open, one that opens and
# will not take the format, one that is missing a symbol, and one whose buffer
# never empties. Every one of them has to come back as a quiet gfx.audio() == 0
# or a program that finishes anyway -- never a crash and never a hang.
#
# Linux only, and skipped with a word when there is no C++ compiler to build the
# fake library with. The macOS AudioQueue backend has no equivalent here: it
# cannot be faked from outside, because AudioToolbox is linked rather than
# dlopened.
#
# Usage: Tests/gfx_alsa_cases.sh [path-to-NexaC]      (run from the repo root)

set -u

NEXAC="${1:-./NexaC}"
if [ ! -x "$NEXAC" ]; then
    echo "FAIL: NexaC not found or not executable: $NEXAC"
    exit 1
fi
NEXAC=$(cd "$(dirname "$NEXAC")" && pwd)/$(basename "$NEXAC")

SUITE=$(cd "$(dirname "$0")" && pwd)

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT INT TERM

fails=0
skips=0

if [ "$(uname -s)" != "Linux" ]; then
    echo "SKIP gfx_alsa: the ALSA backend is Linux only (this is $(uname -s))"
    echo "gfx_alsa ok"
    exit 0
fi

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
    echo "SKIP gfx_alsa: no C++ compiler on this machine"
    echo "gfx_alsa ok"
    exit 0
fi

# --- build: the program under test, and the device it will find --------------

if ! "$NEXAC" "$SUITE/gfx_alsa_test.nxa" --source "$WORK/prog.cpp" > "$WORK/transpile.log" 2>&1; then
    echo "FAIL build: NexaC could not transpile Tests/gfx_alsa_test.nxa"
    sed 's/^/  /' "$WORK/transpile.log"
    exit 1
fi

# The backend has to have survived the platform slicing as the ALSA one, or the
# rest of this file is testing the wrong code.
if ! grep -q 'libasound.so.2' "$WORK/prog.cpp"; then
    echo "FAIL build: the generated program has no ALSA backend in it"
    fails=$((fails + 1))
fi

# Same route as the other gfx suites: a real gfx link wants the X11 development
# libraries, so build against the fake X11 in Tests/gfx_x11_stub. Sound here
# because this program never opens a window.
# -Wno-unused-function on purpose: gfx.open/close/poll/present are emitted into
# every gfx program whether or not it calls them (see GfxNeed in
# include/GfxRuntime.hpp), so a program that only opens an audio stream carries
# four window helpers nothing reaches. That is the slicing design, not a defect,
# and it is the only warning class this build is excused from -- everything
# else, in the audio backend or anywhere near it, fails the suite below.
if ! "$CXX" -std=c++17 -O1 -Wall -Wextra -Wno-unused-function -I "$SUITE/gfx_x11_stub" \
        "$WORK/prog.cpp" "$SUITE/gfx_x11_stub/x11_stub.cpp" \
        -o "$WORK/prog" > "$WORK/build.log" 2>&1; then
    if grep -qE 'X11|error: .*\.h.* file not found' "$WORK/build.log"; then
        echo "SKIP gfx_alsa: this machine cannot build a gfx program"
        grep -E 'error' "$WORK/build.log" | head -n 3 | sed 's/^/       /'
        echo "gfx_alsa ok"
        exit 0
    fi
    echo "FAIL build: the generated program does not build"
    grep -E 'error:|undefined' "$WORK/build.log" | head -n 5 | sed 's/^/  /'
    exit 1
fi

# The generated audio code is held to the same bar as the rest of the runtime.
diags=$(grep -E '\.cpp:[0-9]+:[0-9]+: (warning|error):' "$WORK/build.log")
if [ -n "$diags" ]; then
    echo "FAIL build: the C++ compiler had something to say about the ALSA backend"
    printf '%s\n' "$diags" | head -n 5 | sed 's/^/  /'
    fails=$((fails + 1))
else
    echo "ok build: the ALSA backend compiles clean under -Wall -Wextra"
fi

mkdir -p "$WORK/lib" "$WORK/partial" "$WORK/empty"
if ! "$CXX" -std=c++17 -O1 -Wall -Wextra -fPIC -shared \
        "$SUITE/gfx_alsa_stub/alsa_stub.cpp" -o "$WORK/lib/libasound.so.2" \
        > "$WORK/stub.log" 2>&1; then
    echo "SKIP gfx_alsa: cannot build a shared library on this machine"
    sed 's/^/       /' "$WORK/stub.log" | head -n 3
    echo "gfx_alsa ok"
    exit 0
fi
# The same library with one name missing: something calling itself libasound
# that is not one.
if ! "$CXX" -std=c++17 -O1 -fPIC -shared -DNEXA_ALSA_STUB_PARTIAL \
        "$SUITE/gfx_alsa_stub/alsa_stub.cpp" -o "$WORK/partial/libasound.so.2" \
        >> "$WORK/stub.log" 2>&1; then
    echo "FAIL build: could not build the partial fake libasound"
    fails=$((fails + 1))
fi

# --- running one case --------------------------------------------------------

# run_case <label> <lib-dir> [VAR=VALUE ...]
# Runs the program with the given device on the library path and leaves its
# output in $WORK/out and the device's trace in $WORK/trace. A non-zero exit or
# a program that had to be killed is a failure on the spot, so no later check
# has to wonder whether it ran.
run_case() {
    label=$1
    libdir=$2
    shift 2
    rm -f "$WORK/trace"
    if ! env LD_LIBRARY_PATH="$libdir" NEXA_ALSA_STUB_TRACE="$WORK/trace" "$@" \
            "$WORK/prog" > "$WORK/out" 2>&1; then
        echo "FAIL $label: the program exited non-zero"
        sed 's/^/  /' "$WORK/out"
        fails=$((fails + 1))
        return 1
    fi
    # A case where the backend never reaches the device leaves no trace file at
    # all; give the assertions an empty one to read rather than a missing one.
    [ -f "$WORK/trace" ] || : > "$WORK/trace"
    return 0
}

# says <label> <line>
# The program prints one contract value per line, so this is an exact match.
says() {
    if grep -qx "$2" "$WORK/out"; then
        echo "ok $1"
    else
        echo "FAIL $1: the program never printed '$2'"
        sed 's/^/  /' "$WORK/out"
        fails=$((fails + 1))
    fi
}

# asked <label> <grep-ERE>
# An assertion about what the backend asked the device for.
asked() {
    if grep -Eq "$2" "$WORK/trace"; then
        echo "ok $1"
    else
        echo "FAIL $1: no line in the device trace matches /$2/"
        sed 's/^/  /' "$WORK/trace"
        fails=$((fails + 1))
    fi
}

# never_asked <label> <grep-ERE>
never_asked() {
    if grep -Eq "$2" "$WORK/trace"; then
        echo "FAIL $1: the device trace has a line matching /$2/ and should not"
        sed 's/^/  /' "$WORK/trace"
        fails=$((fails + 1))
    else
        echo "ok $1"
    fi
}

# --- a device that works -----------------------------------------------------

echo "-- a working device"

if run_case "working" "$WORK/lib"; then
    says "working: the stream opens" "audio=1"
    says "working: nothing is queued before a sample" "queued0=0"
    says "working: every sample is taken" "fed=3000"
    # The device holds 2048 and 952 are still staged: both halves counted.
    says "working: queued counts the device and the staging buffer" "queued1=3000"
    says "working: a flush moves the partial buffer without losing it" "queued2=3000"
    says "working: the rate already open is not reopened" "again=1"
    says "working: and reopening did not disturb what was queued" "queued3=3000"
    says "working: a closed stream takes no sample" "after=0"
    says "working: and has nothing queued" "queued4=0"
    says "working: the program ran to the end" "done"

    asked "working: opens \"default\", playback, non-blocking" \
        '^open name=default stream=0 mode=1$'
    # S16_LE is 2, RW_INTERLEAVED is 3, and 100000us is the tenth of a second
    # the mixer aims to keep queued.
    asked "working: S16_LE mono at the rate asked for, 100ms deep" \
        '^set_params format=2 access=3 channels=1 rate=22050 resample=1 latency=100000$'
    asked "working: a full buffer goes out whole" '^writei frames=2048 -> 2048$'
    asked "working: the flush submits exactly what was staged" '^writei frames=952 -> 952$'
    asked "working: closing drains first" '^drain$'
    asked "working: and then closes" '^close$'
    never_asked "working: the backend never passed a handle of its own invention" \
        '^BAD-HANDLE$'
    # gfx.audio() at the rate already open must not open a second stream.
    if [ "$(grep -c '^open ' "$WORK/trace")" = "1" ]; then
        echo "ok working: exactly one stream was opened"
    else
        echo "FAIL working: the stream was opened $(grep -c '^open ' "$WORK/trace") times"
        sed 's/^/  /' "$WORK/trace"
        fails=$((fails + 1))
    fi
fi

# --- devices that do not work ------------------------------------------------

echo "-- no device, and devices that refuse"

# No fake libasound on the path. This is the case every machine without ALSA is
# in, and the one gfx.audio() returned 0 for before this backend existed.
#
# The one case in this file that is not hermetic: dlopen falls back to the
# system library path, so a machine with ALSA actually installed opens a real
# device here instead of finding nothing. That is the backend working, not
# failing, so say so and move on rather than asserting against the test
# machine's sound card.
if run_case "nolib" "$WORK/empty"; then
    if grep -qx "audio=1" "$WORK/out"; then
        echo "SKIP nolib: this machine has a real libasound, so there is no"
        echo "     'no libasound' case to run here"
        skips=$((skips + 1))
    else
        says "nolib: no libasound is a quiet 0" "audio=0"
        says "nolib: and no sample is taken" "fed=0"
        says "nolib: and nothing is queued" "queued1=0"
        says "nolib: the program ran to the end anyway" "done"
    fi
fi

# A libasound missing one of the names the backend needs: refuse it whole rather
# than call half of it.
if run_case "partial" "$WORK/partial"; then
    says "partial: a libasound missing a symbol is a 0" "audio=0"
    says "partial: and no sample is taken" "fed=0"
    says "partial: the program ran to the end anyway" "done"
    never_asked "partial: and the stream was never opened" '^open '
fi

# libasound is there, the device is not.
if run_case "noopen" "$WORK/lib" NEXA_ALSA_STUB_FAIL_OPEN=1; then
    says "noopen: a device that will not open is a 0" "audio=0"
    says "noopen: and no sample is taken" "fed=0"
    says "noopen: the program ran to the end anyway" "done"
    asked "noopen: the backend did try" '^open failed$'
    never_asked "noopen: and stopped there" '^set_params '
fi

# The device opens and then refuses S16 mono. The handle it handed back has to
# be closed, not leaked.
if run_case "noparams" "$WORK/lib" NEXA_ALSA_STUB_FAIL_PARAMS=1; then
    says "noparams: a device that refuses the format is a 0" "audio=0"
    says "noparams: and no sample is taken" "fed=0"
    says "noparams: the program ran to the end anyway" "done"
    asked "noparams: the open that failed to configure is closed again" '^close$'
    never_asked "noparams: and nothing was written to it" '^writei '
fi

# --- a device that misbehaves ------------------------------------------------

echo "-- a device that underruns, and one that never empties"

# The first write underruns. snd_pcm_recover is what turns that back into a
# working stream, and the samples after it must still arrive.
if run_case "xrun" "$WORK/lib" NEXA_ALSA_STUB_XRUN=1; then
    says "xrun: an underrun does not close the stream" "audio=1"
    says "xrun: and does not stop the program feeding it" "fed=3000"
    says "xrun: the program ran to the end" "done"
    asked "xrun: the underrun happened" 'underrun$'
    asked "xrun: and was recovered rather than swallowed" '^recover err=-32 silent=1$'
    asked "xrun: and the stream took samples afterwards" '^writei frames=[0-9]+ -> [0-9]+$'
fi

# A device whose buffer never empties. The backend must wait a bounded time and
# then drop, because a game that is late with its samples must not find the next
# gfx.sample() blocked behind a backlog that is never going to clear.
if run_case "full" "$WORK/lib" NEXA_ALSA_STUB_CAP=0; then
    says "full: a device that is full is still an open stream" "audio=1"
    says "full: gfx.sample still stages what it is given" "fed=3000"
    says "full: the program finished rather than blocking on it" "done"
    asked "full: the device said it was full" '^writei frames=2048 -> full$'
    asked "full: the backend waited on it rather than spinning" '^wait timeout=2$'
    asked "full: and gave up" '^drain$'
fi

if [ $fails -eq 0 ]; then
    if [ $skips -gt 0 ]; then
        echo "gfx_alsa ok ($skips skipped)"
    else
        echo "gfx_alsa ok"
    fi
    exit 0
fi
echo "gfx_alsa: $fails failure(s)"
exit 1
