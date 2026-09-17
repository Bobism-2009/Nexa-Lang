#!/bin/sh
# Cover for the kernel /dev/snd fallback behind gfx.audio() (BOB-46).
#
# Tests/gfx_alsa_cases.sh covers the first way a Nexa program reaches a speaker
# on Linux: libasound, dlopened, which a fake shared library can stand in for.
# This file covers the second, taken when there is no libasound on the machine
# at all -- the kernel's own PCM interface, driven through ioctls on /dev/snd
# with the uapi structs written out by hand in include/GfxRuntime.hpp.
#
# That hand-writing is the thing worth testing, and it cannot be tested the same
# way. A kernel PCM has no shared library to substitute, so there is no fake
# device to point the program at. What there is instead is two halves:
#
#   abi   -- always runs. Cuts the [nexa:kernel-pcm-*] range out of a generated
#            program, compiles it on its own, and checks every struct size and
#            ioctl number against the kernel's. This is the failure that hides:
#            a struct one field out still builds, still runs, and silently
#            never makes a sound, because the size is part of the ioctl number
#            and the kernel simply refuses a request it does not recognise.
#
#   live  -- runs only on a machine that has a sound card and has no libasound,
#            which is exactly the machine the fallback exists for. There the
#            program has to actually play, and the clock is what proves it: a
#            stream that opened but never started hands three thousand samples
#            back instantly, and a real card cannot.
#
# Linux only, and quiet -- the live half feeds silence.
#
# Usage: Tests/gfx_pcm_cases.sh [path-to-NexaC]      (run from the repo root)

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
    echo "SKIP gfx_pcm: the kernel PCM fallback is Linux only (this is $(uname -s))"
    echo "gfx_pcm ok"
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
    echo "SKIP gfx_pcm: no C++ compiler on this machine"
    echo "gfx_pcm ok"
    exit 0
fi

say_ok() { echo "ok $1"; }
say_fail() {
    echo "FAIL $1"
    fails=$((fails + 1))
}

# --- the generated program ---------------------------------------------------

if ! "$NEXAC" "$SUITE/gfx_pcm_test.nxa" --source "$WORK/prog.cpp" > "$WORK/transpile.log" 2>&1; then
    echo "FAIL build: NexaC could not transpile Tests/gfx_pcm_test.nxa"
    sed 's/^/  /' "$WORK/transpile.log"
    exit 1
fi

if grep -q '/dev/snd/pcmC%dD%dp' "$WORK/prog.cpp"; then
    say_ok "slice: the kernel PCM fallback survived into the generated program"
else
    say_fail "slice: the generated program has no kernel PCM fallback in it"
fi

# libasound stays the first path tried. On a desktop it is the one that shares
# the card with whatever else is making noise, and the fallback is only correct
# because it is reached when there is no such thing to share with.
if grep -q 'if (!__nexa_alsa_load()) {' "$WORK/prog.cpp"; then
    say_ok "order: the kernel path is only reached when libasound will not load"
else
    say_fail "order: the kernel path is no longer guarded by the libasound load"
fi

# The fallback belongs to the sound slice, so a gfx program that never makes a
# noise must not be carrying it.
if ! "$NEXAC" "$SUITE/gfx_shapes_test.nxa" --source "$WORK/quiet.cpp" > "$WORK/quiet.log" 2>&1; then
    echo "FAIL slice: NexaC could not transpile Tests/gfx_shapes_test.nxa"
    sed 's/^/  /' "$WORK/quiet.log"
    fails=$((fails + 1))
elif grep -q '/dev/snd/' "$WORK/quiet.cpp"; then
    say_fail "slice: a gfx program with no sound in it still carries /dev/snd"
else
    say_ok "slice: a gfx program with no sound in it carries no /dev/snd"
fi

# --- abi: the hand-written uapi against the kernel's -------------------------

awk '
    /\[nexa:kernel-pcm-begin\]/ { on = 1; next }
    /\[nexa:kernel-pcm-end\]/   { on = 0; next }
    on                          { print }
' "$WORK/prog.cpp" > "$WORK/uapi.h"

if [ ! -s "$WORK/uapi.h" ]; then
    echo "FAIL abi: the [nexa:kernel-pcm-*] markers no longer bracket the declarations"
    exit 1
fi

cat > "$WORK/abi.cpp" <<'ABI'
#include <sys/ioctl.h>
#include <cstring>
#include <cstdio>
#include "uapi.h"
int main() {
    std::printf("hw=%zu sw=%zu xferi=%zu\n",
                sizeof(struct __nexa_snd_hw_params),
                sizeof(struct __nexa_snd_sw_params),
                sizeof(struct __nexa_snd_xferi));
    std::printf("hw_params=0x%lx sw_params=0x%lx writei=0x%lx delay=0x%lx\n",
                (unsigned long)NEXA_SND_IOCTL_HW_PARAMS,
                (unsigned long)NEXA_SND_IOCTL_SW_PARAMS,
                (unsigned long)NEXA_SND_IOCTL_WRITEI,
                (unsigned long)NEXA_SND_IOCTL_DELAY);
    std::printf("prepare=0x%lx drop=0x%lx drain=0x%lx resume=0x%lx\n",
                (unsigned long)NEXA_SND_IOCTL_PREPARE,
                (unsigned long)NEXA_SND_IOCTL_DROP,
                (unsigned long)NEXA_SND_IOCTL_DRAIN,
                (unsigned long)NEXA_SND_IOCTL_RESUME);
    std::printf("access=%d format=%d subformat=%d channels=%d rate=%d period=%d periods=%d buffer=%d first=%d\n",
                NEXA_SND_PARAM_ACCESS, NEXA_SND_PARAM_FORMAT, NEXA_SND_PARAM_SUBFORMAT,
                NEXA_SND_PARAM_CHANNELS, NEXA_SND_PARAM_RATE, NEXA_SND_PARAM_PERIOD_SIZE,
                NEXA_SND_PARAM_PERIODS, NEXA_SND_PARAM_BUFFER_SIZE,
                NEXA_SND_PARAM_FIRST_INTERVAL);
    std::printf("rw_interleaved=%d s16_le=%d std=%d\n",
                NEXA_SND_ACCESS_RW_INTERLEAVED, NEXA_SND_FORMAT_S16_LE, NEXA_SND_SUBFORMAT_STD);
    return 0;
}
ABI

if ! "$CXX" -std=c++17 -O1 -Wall -Wextra -I "$WORK" "$WORK/abi.cpp" -o "$WORK/abi" \
        > "$WORK/abi.log" 2>&1; then
    echo "FAIL abi: the hand-written uapi declarations do not compile on their own"
    grep -E 'error' "$WORK/abi.log" | head -n 5 | sed 's/^/  /'
    exit 1
fi
"$WORK/abi" > "$WORK/abi.out"

# The parameter numbers and the enum values are the same everywhere. The sizes
# and the ioctl numbers are not -- a 32-bit kernel has smaller structs, so it
# has different request numbers -- and the values below are the LP64 ones, taken
# from /usr/include/sound/asound.h and from what strace prints for an ALSA
# program. Checking them anywhere else would be checking an arithmetic we have
# not been given the truth for, so that half is skipped there with a word.
abi_says() {
    if grep -qx "$2" "$WORK/abi.out"; then
        say_ok "abi: $1"
    else
        echo "FAIL abi: $1"
        echo "  wanted: $2"
        echo "  got:    $(grep -E "^$(printf '%s' "$2" | cut -d' ' -f1 | cut -d= -f1)" "$WORK/abi.out")"
        fails=$((fails + 1))
    fi
}

abi_says "the parameter numbers are the kernel's SNDRV_PCM_HW_PARAM_*" \
    'access=0 format=1 subformat=2 channels=10 rate=11 period=13 periods=15 buffer=17 first=8'
abi_says "interleaved read/write, S16_LE and the standard subformat" \
    'rw_interleaved=3 s16_le=2 std=0'

if [ "$(getconf LONG_BIT 2>/dev/null || echo 0)" = "64" ]; then
    abi_says "the structs are the size the kernel's are" 'hw=608 sw=136 xferi=24'
    abi_says "hw_params, sw_params, writei and delay are the kernel's ioctls" \
        'hw_params=0xc2604111 sw_params=0xc0884113 writei=0x40184150 delay=0x80084121'
    abi_says "and so are prepare, drop, drain and resume" \
        'prepare=0x4140 drop=0x4143 drain=0x4144 resume=0x4147'
else
    echo "SKIP abi: the struct sizes and ioctl numbers checked here are the"
    echo "     64-bit ones (this is $(getconf LONG_BIT 2>/dev/null)-bit)"
    skips=$((skips + 1))
fi

# --- the whole program, built the way the other gfx suites build one ---------

# -Wno-unused-function for the same reason Tests/gfx_alsa_cases.sh gives: the
# window helpers are emitted into every gfx program whether it opens one or not.
if ! "$CXX" -std=c++17 -O1 -Wall -Wextra -Wno-unused-function -I "$SUITE/gfx_x11_stub" \
        "$WORK/prog.cpp" "$SUITE/gfx_x11_stub/x11_stub.cpp" \
        -o "$WORK/prog" > "$WORK/build.log" 2>&1; then
    if grep -qE 'X11|error: .*\.h.* file not found' "$WORK/build.log"; then
        echo "SKIP gfx_pcm: this machine cannot build a gfx program"
        grep -E 'error' "$WORK/build.log" | head -n 3 | sed 's/^/       /'
        echo "gfx_pcm ok"
        exit 0
    fi
    echo "FAIL build: the generated program does not build"
    grep -E 'error:|undefined' "$WORK/build.log" | head -n 5 | sed 's/^/  /'
    exit 1
fi

diags=$(grep -E '\.cpp:[0-9]+:[0-9]+: (warning|error):' "$WORK/build.log")
if [ -n "$diags" ]; then
    echo "FAIL build: the C++ compiler had something to say about the kernel backend"
    printf '%s\n' "$diags" | head -n 5 | sed 's/^/  /'
    fails=$((fails + 1))
else
    say_ok "build: the kernel PCM fallback compiles clean under -Wall -Wextra"
fi

# --- live: only on the machine this fallback is for --------------------------

# libasound present means the program never reaches the kernel path, so there is
# nothing here to run -- Tests/gfx_alsa_cases.sh is the suite for that machine.
has_libasound=$(python3 - <<'PY' 2>/dev/null || echo unknown
import ctypes
try:
    ctypes.CDLL("libasound.so.2")
    print("yes")
except OSError:
    try:
        ctypes.CDLL("libasound.so")
        print("yes")
    except OSError:
        print("no")
PY
)

has_pcm=no
for n in /dev/snd/pcmC*D*p; do
    [ -c "$n" ] && has_pcm=yes && break
done

if [ "$has_libasound" != "no" ]; then
    echo "SKIP live: this machine has libasound, so gfx.audio() never reaches"
    echo "     the kernel path (that machine is Tests/gfx_alsa_cases.sh's)"
    skips=$((skips + 1))
elif [ "$has_pcm" != "yes" ]; then
    echo "SKIP live: no /dev/snd playback node on this machine, so there is no"
    echo "     card for the fallback to open"
    skips=$((skips + 1))
else
    echo "-- a real card, reached without libasound"

    # A device node that is not a PCM at all. The fallback has to give up on it
    # and say so the way it says everything else: a quiet 0 and a program that
    # finishes. /dev/null opens, so this gets past the open and fails where it
    # matters, on the first ioctl.
    if NEXA_PCM_DEVICE=/dev/null timeout 60 "$WORK/prog" > "$WORK/out" 2>&1; then
        if grep -qx "audio=0" "$WORK/out" && grep -qx "fed=0" "$WORK/out" &&
                grep -qx "done" "$WORK/out"; then
            say_ok "notapcm: a node that is not a PCM is a quiet 0"
        else
            echo "FAIL notapcm: a node that is not a PCM did not come back as a quiet 0"
            sed 's/^/  /' "$WORK/out"
            fails=$((fails + 1))
        fi
    else
        echo "FAIL notapcm: the program exited non-zero"
        sed 's/^/  /' "$WORK/out"
        fails=$((fails + 1))
    fi

    # A path with nothing behind it: the open itself fails.
    if NEXA_PCM_DEVICE="$WORK/no-such-device" timeout 60 "$WORK/prog" > "$WORK/out" 2>&1 &&
            grep -qx "audio=0" "$WORK/out" && grep -qx "done" "$WORK/out"; then
        say_ok "nodevice: a device that is not there is a quiet 0"
    else
        echo "FAIL nodevice: a missing device did not come back as a quiet 0"
        sed 's/^/  /' "$WORK/out"
        fails=$((fails + 1))
    fi

    # And the card itself.
    if ! timeout 60 "$WORK/prog" > "$WORK/out" 2>&1; then
        echo "FAIL live: the program exited non-zero"
        sed 's/^/  /' "$WORK/out"
        fails=$((fails + 1))
    else
        for want in "audio=1" "fed=3000" "drained=0" "after=0" "queued_after=0" "done"; do
            if grep -qx "$want" "$WORK/out"; then
                say_ok "live: $want"
            else
                echo "FAIL live: the program never printed '$want'"
                sed 's/^/  /' "$WORK/out"
                fails=$((fails + 1))
            fi
        done

        # The card took samples and had not played them all yet, which only a
        # prepared, running stream reports.
        held=$(sed -n 's/^held=//p' "$WORK/out")
        if [ -n "$held" ] && [ "$held" -gt 0 ] && [ "$held" -le 3000 ]; then
            say_ok "live: the card was holding $held of the 3000 samples"
        else
            echo "FAIL live: held=$held, which no running stream reports"
            sed 's/^/  /' "$WORK/out"
            fails=$((fails + 1))
        fi

        # Three thousand samples cannot come back faster than a card can play
        # them: 31ms at 96000, 62ms at 48000, 68ms at 44100. A stream that never
        # started gives them back in no time at all, which is the whole point of
        # measuring. The ceiling is only there to catch a stream that stalled.
        elapsed=$(sed -n 's/^elapsed_ms=//p' "$WORK/out")
        verdict=$(awk -v e="$elapsed" 'BEGIN { print (e + 0 >= 25 && e + 0 <= 4000) ? "ok" : "no" }')
        if [ "$verdict" = "ok" ]; then
            say_ok "live: the samples drained in ${elapsed}ms, the speed of a real card"
        else
            echo "FAIL live: 3000 samples drained in ${elapsed}ms, which is not playback"
            sed 's/^/  /' "$WORK/out"
            fails=$((fails + 1))
        fi
    fi
fi

if [ $fails -eq 0 ]; then
    if [ $skips -gt 0 ]; then
        echo "gfx_pcm ok ($skips skipped)"
    else
        echo "gfx_pcm ok"
    fi
    exit 0
fi
echo "gfx_pcm: $fails failure(s)"
exit 1
